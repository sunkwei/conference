/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#define UNICODE
#define WIN_EC

#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#	include <dmo.h>
#	include <objbase.h>
#	include <mediaobj.h>
#	include <uuids.h>
#	include <propidl.h>	
#	include <wmcodecdsp.h>
#	include <audioclient.h>
#	include <MMDeviceApi.h>
#	include <AudioEngineEndPoint.h>
#	include <DeviceTopology.h>
#	include <propkey.h>
#	include <strsafe.h>
#	include <conio.h>
#	include <dsound.h>
#	include <process.h>
// uuid.lib amstrmid.lib wmcodecdspuuid.lib Msdmo.lib dmoguids.lib
#	pragma comment(lib, "amstrmid.lib")
#	pragma comment(lib, "wmcodecdspuuid.lib")
#	pragma comment(lib, "msdmo.lib")
#	pragma comment(lib, "dmoguids.lib")
static bool support_win_ec();
#include <mmsystem.h>
#ifdef _MSC_VER
#include <mmreg.h>
#endif
#include <msacm.h>

#if defined(_WIN32_WCE)
//#define DISABLE_SPEEX
//#define WCE_OPTICON_WORKAROUND 1000
#endif

void PVInit(PROPVARIANT *pv)
{
	memset(pv, 0, sizeof(PROPVARIANT));
}

#define WINSND_NBUFS 10
#define WINSND_OUT_DELAY 0.020
#define WINSND_OUT_NBUFS 40
#define WINSND_NSAMPLES 320
#define WINSND_MINIMUMBUFFER 5

static MSFilter *ms_winsnd_read_new(MSSndCard *card);
static MSFilter *ms_winsnd_write_new(MSSndCard *card);

typedef struct WinSndCard{
	int in_devid;
	int out_devid;
} WinSndCard;

static bool _enable_log = false;
static void _ec_log_init()
{
	if (_enable_log) {
		FILE *fp = fopen("zk.ec.log", "w");
		if (fp) fclose(fp);
	}
}
static void _ec_log(const char *fmt, ...)
{
	if (_enable_log) {
		char buf[1024];
		va_list ap;

		FILE *fp = fopen("zk.ec.log", "at");
		if (fp) {
			va_start(ap, fmt);
			vsnprintf(buf, sizeof(buf), fmt, ap);
			va_end(ap);

			fprintf(fp, buf);
			fclose(fp);
		}
	}
}

struct ZKAudioDuplexMediaBuffer : public IMediaBuffer 
{
public:
	ZKAudioDuplexMediaBuffer(ULONG ulSize) 
		: m_pData(new BYTE[ulSize]), m_ulSize(ulSize), m_ulData(0), m_cRef(1) 
	{}
	~ZKAudioDuplexMediaBuffer()
	{
		delete []m_pData;
	}
	STDMETHODIMP_(ULONG) AddRef() {
		return InterlockedIncrement((long*)&m_cRef);
	}
	STDMETHODIMP_(ULONG) Release() {
		long l = InterlockedDecrement((long*)&m_cRef);
		if (l == 0)
			delete this;
		return l;
	}
	STDMETHODIMP QueryInterface(REFIID riid, void **ppv) {
		if (riid == IID_IUnknown) {
			AddRef();
			*ppv = (IUnknown*)this;
			return NOERROR;
		}
		else if (riid == IID_IMediaBuffer) {
			AddRef();
			*ppv = (IMediaBuffer*)this;
			return NOERROR;
		}
		else
			return E_NOINTERFACE;
	}
	STDMETHODIMP SetLength(DWORD ulLength) 
	{
		m_ulData = ulLength; 
		return NOERROR;
	}
	STDMETHODIMP GetMaxLength(DWORD *pcbMaxLength) 
	{
		*pcbMaxLength = m_ulSize; 
		return NOERROR;
	}

	STDMETHODIMP GetBufferAndLength(BYTE **ppBuffer, DWORD *pcbLength) 
	{
		if (ppBuffer) *ppBuffer = m_pData;
		if (pcbLength) *pcbLength = m_ulData;
		return NOERROR;
	}
	void reset()
	{
		m_ulData = 0;
	}

	BYTE *m_pData;
	ULONG m_ulSize;
	ULONG m_ulData;
	ULONG m_cRef;
};

static void winsndcard_set_level(MSSndCard *card, MSSndCardMixerElem e, int percent){
#if 0
    MMRESULT mr = MMSYSERR_NOERROR;
    DWORD dwVolume = 0xFFFF;
    dwVolume = ((0xFFFF) * percent) / 100;

	switch(e){
		case MS_SND_CARD_MASTER:
            /*mr = waveOutSetVolume(d->waveoutdev, dwVolume); */
	        if (mr != MMSYSERR_NOERROR)
	        {
                ms_warning("Failed to set master volume. (waveOutSetVolume:0x%i)", mr);
                return;
	        }
        break;
        case MS_SND_CARD_CAPTURE:
		break;
		case MS_SND_CARD_PLAYBACK:
		break;
        default:
			ms_warning("winsnd_card_set_level: unsupported command.");
	}
#endif
}

static int winsndcard_get_level(MSSndCard *card, MSSndCardMixerElem e){
	switch(e){
		case MS_SND_CARD_MASTER:
            /*mr=waveOutGetVolume(d->waveoutdev, &dwVolume);*/
            /* Transform to 0 to 100 scale*/
            /*dwVolume = (dwVolume *100) / (0xFFFF);*/
            return 60;
        break;
        case MS_SND_CARD_CAPTURE:
		break;
		case MS_SND_CARD_PLAYBACK:
		break;
		default:
			ms_warning("winsnd_card_get_level: unsupported command.");
			return -1;
	}
	return -1;
}

static void winsndcard_set_source(MSSndCard *card, MSSndCardCapture source){

	switch(source){
		case MS_SND_CARD_MIC:
		break;
		case MS_SND_CARD_LINE:
		break;
	}	
}

static void winsndcard_init(MSSndCard *card){
	WinSndCard *c=(WinSndCard *)ms_new(WinSndCard,1);
	card->data=c;
}

static void winsndcard_uninit(MSSndCard *card){
	ms_free(card->data);
}

static void winsndcard_detect(MSSndCardManager *m);
static  MSSndCard *winsndcard_dup(MSSndCard *obj);

extern "C" MSSndCardDesc winsnd_card_desc={
	"WINSND",
	winsndcard_detect,
	winsndcard_init,
	winsndcard_set_level,
	winsndcard_get_level,
	winsndcard_set_source,
	NULL,
	NULL,
	ms_winsnd_read_new,
	ms_winsnd_write_new,
	winsndcard_uninit,
	winsndcard_dup
};

static  MSSndCard *winsndcard_dup(MSSndCard *obj){
	MSSndCard *card=ms_snd_card_new(&winsnd_card_desc);
	card->name=ms_strdup(obj->name);
	card->data=ms_new(WinSndCard,1);
	memcpy(card->data,obj->data,sizeof(WinSndCard));
	return card;
}

static MSSndCard *winsndcard_new(const char *name, int in_dev, int out_dev, unsigned cap){
	MSSndCard *card=ms_snd_card_new(&winsnd_card_desc);
	WinSndCard *d=(WinSndCard*)card->data;
	card->name=ms_strdup(name);
	d->in_devid=in_dev;
	d->out_devid=out_dev;
	char *p = getenv("zonekey_debug");
	if (p && !strcmp(p, "enable")) {
		_enable_log = true;
		_ec_log_init();
	}

	if (support_win_ec()) {
		_ec_log("ok, support win voice capture effect\n");
		cap |= MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER;
	}
	else {
		_ec_log("en, the os NOT support voice capture effect!\n");
	}

	card->capabilities=cap;
	return card;
}

static void add_or_update_card(MSSndCardManager *m, const char *name, int indev, int outdev, unsigned int capability){
	MSSndCard *card;
	const MSList *elem=ms_snd_card_manager_get_list(m);
	for(;elem!=NULL;elem=elem->next){
		card=(MSSndCard*)elem->data;
		if (strcmp(card->name,name)==0){
			/*update already entered card */
			WinSndCard *d=(WinSndCard*)card->data;
			card->capabilities|=capability;
			if (indev!=-1) 
				d->in_devid=indev;
			if (outdev!=-1)
				d->out_devid=outdev;
				
			return;
		}
	}
	/* add this new card:*/
	ms_snd_card_manager_add_card(m,winsndcard_new(name,indev,outdev,capability));
}

static void winsndcard_detect(MSSndCardManager *m){
	MMRESULT mr = NOERROR;
	unsigned int nOutDevices = waveOutGetNumDevs ();
	unsigned int nInDevices = waveInGetNumDevs ();
	unsigned int item;
	char card[256]={0};
	
	if (nOutDevices>nInDevices)
		nInDevices = nOutDevices;

	for (item = 0; item < nInDevices; item++){
		WAVEINCAPS incaps;
		WAVEOUTCAPS outcaps;
		mr = waveInGetDevCaps (item, &incaps, sizeof (WAVEINCAPS));
		if (mr == MMSYSERR_NOERROR)
		{	
#if defined(_WIN32_WCE)
			snprintf(card, sizeof(card), "Input card %i", item);
#else
			WideCharToMultiByte(CP_UTF8,0,incaps.szPname,-1
				,card,sizeof(card)-1,NULL,NULL);
#endif
			add_or_update_card(m,card,item,-1,MS_SND_CARD_CAP_CAPTURE);
		}
		mr = waveOutGetDevCaps (item, &outcaps, sizeof (WAVEOUTCAPS));
		if (mr == MMSYSERR_NOERROR)
		{
#if defined(_WIN32_WCE)
			snprintf(card, sizeof(card), "Output card %i", item);
#else
			WideCharToMultiByte(CP_UTF8,0,outcaps.szPname,-1
				,card,sizeof(card)-1,NULL,NULL);
#endif
			add_or_update_card(m,card,-1,item,MS_SND_CARD_CAP_PLAYBACK);
		}
	}
}


typedef struct WinSnd{
	int dev_id;
	HWAVEIN indev;
	HWAVEOUT outdev;
	WAVEFORMATEX wfx;
	WAVEHDR hdrs_read[WINSND_NBUFS];
	WAVEHDR hdrs_write[WINSND_OUT_NBUFS];
	queue_t rq;
	ms_mutex_t mutex;
	unsigned int bytes_read;
	unsigned int nbufs_playing;
	bool_t running;
	int outcurbuf;
	int nsamples;
	queue_t wq;
	int32_t stat_input;
	int32_t stat_output;
	int32_t stat_notplayed;

	int32_t stat_minimumbuffer;
	int ready;
	int workaround; /* workaround for opticon audio device */
	bool_t overrun;

	// 支持 win7 AEC mo
	bool supported_;	// 是否支持 voice capture dmo
	IMediaObject *mo_;
	IPropertyStore *ps_;
	ZKAudioDuplexMediaBuffer *media_buf_;
	DMO_OUTPUT_DATA_BUFFER dmo_data_buffer_;
	HANDLE th_;	// 读工作线程
	bool quit_;	// 用于结束工作线程
}WinSnd;

static void winsnd_apply_settings(WinSnd *d){
	d->wfx.nBlockAlign=d->wfx.nChannels*d->wfx.wBitsPerSample/8;
	d->wfx.nAvgBytesPerSec=d->wfx.nSamplesPerSec*d->wfx.nBlockAlign;
}

static bool support_win_ec()
{
	char *p = getenv("zonekey_ocx_audio_speex_dsp");
	if (p && !strcmp(p, "enable"))
		return false;

	DWORD ver = GetVersion();
	DWORD major = (DWORD)(LOBYTE(LOWORD(ver)));
	DWORD minor = (DWORD)(HIBYTE(LOWORD(ver)));

	CoInitialize(0);

	if (major >= 6) {
		// VISTA，WIN7 ....
		IUnknown *p;
		HRESULT hr = CoCreateInstance(CLSID_CWMAudioAEC, NULL, CLSCTX_INPROC_SERVER, IID_IMediaObject, (void**)&p);
		if (hr == S_OK) {
			p->Release();
			return true;
		}
	}
	return false;
}

static int _open_win_ec(WinSnd *d, int bsize)
{
	if (d->supported_) {
		_ec_log("%s:: bsize=%d\n", __FUNCTION__, bsize);

		// VISTA，WIN7 ....
		HRESULT hr = CoCreateInstance(CLSID_CWMAudioAEC, NULL, CLSCTX_INPROC_SERVER, IID_IMediaObject, (void**)&d->mo_);
		if (FAILED(hr)) {
			fprintf(stderr, "[zkMediaAudio]: %s: CoCreateInstance for CLSID_CWMAudioAEC err!. code=%08x\n", __FUNCTION__, hr);
			_ec_log("ERR: CoCreateInstance for CLSID_CWMAudioAEC, hr=%08x\n", hr);
			return -1;
		}
		_ec_log("\tCreateInstance for CLSID_CWMAudioAEC OK\n");
		hr = d->mo_->QueryInterface(IID_IPropertyStore, (void**)&d->ps_);
		if (FAILED(hr)) {
			fprintf(stderr, "[zkMediaAudio]: %s: QueryInterface for IID_IPropertyStore err, code=%08x\n", __FUNCTION__, hr);
			d->mo_->Release();
			d->mo_ = 0;
			return -2;
		}

		d->media_buf_ = new ZKAudioDuplexMediaBuffer(bsize);
		d->dmo_data_buffer_.pBuffer = d->media_buf_;

		// Source mode to Filter mode

		char *p;
		// system mode
		int iSystemMode = 0;	// AEC and map
		p = getenv("zonekey_ocx_audio_dsp_mode");
		if (p) iSystemMode = atoi(p);
		PROPVARIANT pvSysMode;
		PVInit(&pvSysMode);
		pvSysMode.vt = VT_I4;
		pvSysMode.lVal = (LONG)(iSystemMode);
		d->ps_->SetValue(MFPKEY_WMAAECMA_SYSTEM_MODE, pvSysMode);
		pvSysMode.lVal = -1;
		d->ps_->GetValue(MFPKEY_WMAAECMA_SYSTEM_MODE, &pvSysMode);
		_ec_log("\tSYSTEM_MODE(zonekey_ocx_audio_dsp_mode): try %d, get %d\n", iSystemMode, pvSysMode.lVal);
		PropVariantClear(&pvSysMode);
		
        // Turn on feature modes
        PROPVARIANT pvFeatrModeOn;
        PVInit(&pvFeatrModeOn);
        pvFeatrModeOn.vt = VT_BOOL;
        pvFeatrModeOn.boolVal = -1;
		p = getenv("zonekey_ocx_audio_feature");
		if (p && !strcmp(p, "disable")) {
			_ec_log("\t Disable winec feature!\n");
			pvFeatrModeOn.boolVal = 0;
		}
        d->ps_->SetValue(MFPKEY_WMAAECMA_FEATURE_MODE, pvFeatrModeOn);
        PropVariantClear(&pvFeatrModeOn);

		PROPVARIANT pvFrameSize;
		// Turn on/off noise suppression
		PROPVARIANT pvNoiseSup;
		PVInit(&pvNoiseSup);
		pvNoiseSup.vt = VT_BOOL;
		pvNoiseSup.boolVal = -1;
		p = getenv("zonekey_ocx_audio_enable_ns");
		if (p) pvNoiseSup.boolVal = atoi(p);
		d->ps_->SetValue(MFPKEY_WMAAECMA_FEATR_NS, pvNoiseSup);
		_ec_log("\tFEATR_NS(zonekey_ocx_audio_enable_ns): try %d, ", (short)pvNoiseSup.boolVal);
		d->ps_->GetValue(MFPKEY_WMAAECMA_FEATR_NS, &pvNoiseSup);
		_ec_log("get %d\n", pvNoiseSup.boolVal);
		PropVariantClear(&pvNoiseSup);

		// noise filling
		PROPVARIANT pvNoiseFilling;
		PVInit(&pvNoiseFilling);
		pvNoiseFilling.boolVal = -1;
		p = getenv("zonekey_ocx_audio_noise_filling");
		if (p) pvNoiseFilling.boolVal = atoi(p);
		d->ps_->SetValue(MFPKEY_WMAAECMA_FEATR_NOISE_FILL, pvNoiseFilling);
		_ec_log("\tFEATR_NF(zonekey_ocx_audio_noise_filling): try %d, ", pvNoiseFilling.boolVal);
		d->ps_->GetValue(MFPKEY_WMAAECMA_FEATR_NOISE_FILL, &pvNoiseFilling);
		_ec_log("get %d\n", pvNoiseFilling.boolVal);
		PropVariantClear(&pvNoiseFilling);

		// mic gain bounder: 禁用mic 自动增益调整
		PROPVARIANT pvMicGain;
		PVInit(&pvMicGain);
		pvMicGain.boolVal = -1;
		p = getenv("zonekey_ocx_audio_mic_gain");
		if (p) pvMicGain.boolVal = atoi(p);
		hr = d->ps_->SetValue(MFPKEY_WMAAECMA_MIC_GAIN_BOUNDER, pvMicGain);
		_ec_log("\tFEATR_MIC_GAIN:(zonekey_ocx_audio_mic_gain): try %d, ", pvMicGain.boolVal);
		d->ps_->GetValue(MFPKEY_WMAAECMA_MIC_GAIN_BOUNDER, &pvMicGain);
		_ec_log("get %d, hr=%08x\n", pvMicGain.boolVal, hr);
		PropVariantClear(&pvMicGain);

		// Turn on/off AGC
		PROPVARIANT pvAGC;
		PVInit(&pvAGC);
		pvAGC.vt = VT_BOOL;
		pvAGC.boolVal = 0;
		p = getenv("zonekey_ocx_audio_enable_agc");
		if (p) pvAGC.boolVal = atoi(p);
		d->ps_->SetValue(MFPKEY_WMAAECMA_FEATR_AGC, pvAGC);
		_ec_log("\tFEATR_AGC(zonekey_ocx_audio_enable_agc): try %d, ", (short)pvAGC.boolVal);
		d->ps_->GetValue(MFPKEY_WMAAECMA_FEATR_AGC, &pvAGC);
		_ec_log("get %d\n", pvAGC.boolVal);
		PropVariantClear(&pvAGC);

		// AEC echo tail, 128, 256*, 512, 1024
		PROPVARIANT pvTailLen;
		PVInit(&pvTailLen);
		pvTailLen.vt = VT_I4;
		pvTailLen.lVal = 256;
		p = getenv("zonekey_ocx_audio_ec_tail_len");
		if (p) pvTailLen.lVal = atoi(p);
		d->ps_->SetValue(MFPKEY_WMAAECMA_FEATR_ECHO_LENGTH, pvTailLen);
		_ec_log("\tFEATR_ECHO_LENGTH(zonekey_ocx_audio_ec_tail_len): try %d, ", pvTailLen.lVal);
		pvTailLen.lVal = 0;
		d->ps_->GetValue(MFPKEY_WMAAECMA_FEATR_ECHO_LENGTH, &pvTailLen);
		_ec_log("get %d\n", pvTailLen.lVal);
		PropVariantClear(&pvTailLen);

		// AEC frame size, 80, 128, 160, 240, 256, 320*
		PVInit(&pvFrameSize);
		pvFrameSize.vt = VT_I4;
		pvFrameSize.lVal = 320;	// 
		p = getenv("zonekey_ocx_audio_ec_frame_size");
		if (p) pvFrameSize.lVal = atoi(p);
		d->ps_->SetValue(MFPKEY_WMAAECMA_FEATR_FRAME_SIZE, pvFrameSize);
		_ec_log("\tFEATR_FRAME_SIZE(zonekey_ocx_audio_ec_frame_size): try %d, ", pvFrameSize.lVal);
		d->ps_->GetValue(MFPKEY_WMAAECMA_FEATR_FRAME_SIZE, &pvFrameSize);
		_ec_log("get %d\n", pvFrameSize.lVal);
		PropVariantClear(&pvFrameSize);

		// AES: 0, 1, 2
		PROPVARIANT pvAES;
		PVInit(&pvAES);
		pvAES.vt = VT_I4;
		pvAES.lVal = 0;
		p = getenv("zonekey_ocx_audio_aes");
		if (p) pvAES.lVal = atoi(p);
		d->ps_->SetValue(MFPKEY_WMAAECMA_FEATR_AES, pvAES);
		_ec_log("\tFEATR_AES(zonekey_ocx_audio_aes): try %d, ", pvAES.lVal);
		d->ps_->GetValue(MFPKEY_WMAAECMA_FEATR_AES, &pvAES);
		_ec_log("get %d\n", pvAES.lVal);
		PropVariantClear(&pvAES);

		// Turn on/off center clip
		PROPVARIANT pvCntrClip;
		PVInit(&pvCntrClip);
		pvCntrClip.vt = VT_BOOL;
		pvCntrClip.boolVal = -1;
		p = getenv("zonekey_ocx_audio_center_clip");
		if (p) pvCntrClip.boolVal = atoi(p);
		d->ps_->SetValue(MFPKEY_WMAAECMA_FEATR_CENTER_CLIP, pvCntrClip);
		_ec_log("\tFEATR_CENTER_CLIP(zonekey_ocx_audio_center_clip): try %d, ", (short)pvCntrClip.boolVal);
		d->ps_->GetValue(MFPKEY_WMAAECMA_FEATR_CENTER_CLIP, &pvCntrClip);
		_ec_log("get %d\n", pvCntrClip.boolVal);
		PropVariantClear(&pvCntrClip);

		// 保存统计信息
		PROPVARIANT pvStat;
		PVInit(&pvStat);
		pvStat.vt = VT_BOOL;
		pvStat.boolVal = -1;	// 缺省开启
		p = getenv("zonekey_ocx_audio_stat_storage");
		if (p) pvStat.boolVal = atoi(p);
		d->ps_->SetValue(MFPKEY_WMAAECMA_RETRIEVE_TS_STATS, pvStat);
		_ec_log("\tRETRIEVE_TS_STATS(zonekey_ocx_audio_stat_storage): try %d, ", (short)pvStat.boolVal);
		d->ps_->GetValue(MFPKEY_WMAAECMA_RETRIEVE_TS_STATS, &pvStat);
		_ec_log("get %d\n", pvStat.boolVal);
		PropVariantClear(&pvStat);

		// 输出格式：pcm format
		DMO_MEDIA_TYPE mt;
		hr = MoInitMediaType(&mt, sizeof(WAVEFORMATEX));
		mt.majortype = MEDIATYPE_Audio;
		mt.subtype = MEDIASUBTYPE_PCM;
		mt.lSampleSize = 0;
		mt.bFixedSizeSamples = TRUE;
		mt.bTemporalCompression = FALSE;
		mt.formattype = FORMAT_WaveFormatEx;
		memcpy(mt.pbFormat, &d->wfx, sizeof(WAVEFORMATEX));

		hr = d->mo_->SetOutputType(0, &mt, 0);
		MoFreeMediaType(&mt);
		if (FAILED(hr)) {
			fprintf(stderr, "[zkMediaAudio] %s: SetOutputType err, hr=%08x\n", __FUNCTION__, hr);
			d->ps_->Release();
			d->ps_ = 0;
			d->mo_->Release();
			d->mo_ = 0;
			return -3;
		}

		// Allocate streaming resources. This step is optional. If it is not called here, it
		// will be called when first time ProcessInput() is called. However, if you want to 
		// get the actual frame size being used, it should be called explicitly here.
		hr = d->mo_->AllocateStreamingResources();
		
		// Get actually frame size being used in the DMO. (optional, do as you need)

		int iFrameSize;
		PVInit(&pvFrameSize);
		d->ps_->GetValue(MFPKEY_WMAAECMA_FEATR_FRAME_SIZE, &pvFrameSize);
		iFrameSize = pvFrameSize.lVal;
		_ec_log("\tGET FEATR_FRAME_SIZE: get %d\n", iFrameSize);
		PropVariantClear(&pvFrameSize);

		// all done

		return 0;
	}
	else {
		d->mo_ = 0;
		d->ps_ = 0;
		d->media_buf_ = 0;

		return -1;
	}
}

static void _close_win_ec(WinSnd *d)
{
	if (d->mo_) {
		_ec_log("%s: END!\n", __FUNCTION__);
		d->mo_->FreeStreamingResources();
		d->mo_->Release();
	}
	if (d->ps_) d->ps_->Release();
	if (d->media_buf_) d->media_buf_->Release();
	d->dmo_data_buffer_.pBuffer = 0;

	d->mo_ = 0;
	d->ps_ = 0;
	d->media_buf_ = 0;
}

static int _try_win_ec(WinSnd *d, void *data, int len)
{
	if (d->mo_) {
		DWORD state;
		d->dmo_data_buffer_.dwStatus = 0;
		d->media_buf_->reset();
		//memcpy(d->media_buf_->m_pData, data, len);
		//d->media_buf_->SetLength(len);
		HRESULT hr = d->mo_->ProcessOutput(0, 1, &d->dmo_data_buffer_, &state);
		if (hr != S_OK) {
			fprintf(stderr, "[zkMediaAudio] %s: IMediaObject::ProcessOutput err, code=%08x\n", __FUNCTION__, hr);
			_ec_log("%s: ProcessOutput ERR: hr=%08x\n", __FUNCTION__, hr);
			return -1;
		}

		ULONG bytes;
		hr = d->media_buf_->GetBufferAndLength(0, &bytes);
		if (bytes <= len) {
			memcpy(data, d->media_buf_->m_pData, bytes);
		}

		if (bytes != len) {
			_ec_log("   ERR: ??? bytes=%d, len=%d\n", bytes, len);
		}
	}

	return 0;
}

#ifndef _TRUE_TIME
static uint64_t winsnd_get_cur_time( void *data){
	WinSnd *d=(WinSnd*)data;
	uint64_t curtime=((uint64_t)d->bytes_read*1000)/(uint64_t)d->wfx.nAvgBytesPerSec;
	/* ms_debug("winsnd_get_cur_time: bytes_read=%u return %lu\n",d->bytes_read,(unsigned long)curtime); */
	return curtime;
}
#endif

static void winsnd_init(MSFilter *f){
	WinSnd *d=(WinSnd *)ms_new0(WinSnd,1);
	d->wfx.wFormatTag = WAVE_FORMAT_PCM;
	d->wfx.cbSize = 0;
	d->wfx.nAvgBytesPerSec = 16000;
	d->wfx.nBlockAlign = 2;
	d->wfx.nChannels = 1;
	d->wfx.nSamplesPerSec = 8000;
	d->wfx.wBitsPerSample = 16;
	qinit(&d->rq);
	qinit(&d->wq);
	d->ready=0;
	d->workaround=0;
	ms_mutex_init(&d->mutex,NULL);
	f->data=d;

	d->stat_input=0;
	d->stat_output=0;
	d->stat_notplayed=0;
	d->stat_minimumbuffer=WINSND_MINIMUMBUFFER;

	d->supported_ = support_win_ec();
	d->mo_ = 0;
	d->ps_ = 0;
	d->media_buf_ = 0;
	memset(&d->dmo_data_buffer_, 0, sizeof(d->dmo_data_buffer_));
	d->th_ = 0;
}

static void winsnd_uninit(MSFilter *f){
	WinSnd *d=(WinSnd*)f->data;
	flushq(&d->rq,0);
	flushq(&d->wq,0);
	d->ready=0;
	d->workaround=0;
	ms_mutex_destroy(&d->mutex);
	ms_free(f->data);
}

static void add_input_buffer(WinSnd *d, WAVEHDR *hdr, int buflen){
	mblk_t *m=allocb(buflen,0);
	MMRESULT mr;
	memset(hdr,0,sizeof(*hdr));
	if (buflen==0) ms_error("add_input_buffer: buflen=0 !");
	hdr->lpData=(LPSTR)m->b_wptr;
	hdr->dwBufferLength=buflen;
	hdr->dwFlags = 0;
	hdr->dwUser = (DWORD)m;
	mr = waveInPrepareHeader (d->indev,hdr,sizeof(*hdr));
	if (mr != MMSYSERR_NOERROR){
		ms_error("waveInPrepareHeader() error");
		return ;
	}
	mr=waveInAddBuffer(d->indev,hdr,sizeof(*hdr));
	if (mr != MMSYSERR_NOERROR){
		ms_error("waveInAddBuffer() error");
		return ;
	}
}

static void CALLBACK 
read_callback (HWAVEIN waveindev, UINT uMsg, DWORD dwInstance, DWORD dwParam1,
                DWORD dwParam2)
{
	WAVEHDR *wHdr=(WAVEHDR *) dwParam1;
	MSFilter *f=(MSFilter *)dwInstance;
	WinSnd *d=(WinSnd*)f->data;
	mblk_t *m;
	int bsize;
	switch (uMsg){
		case WIM_OPEN:
			ms_debug("read_callback : WIM_OPEN");
		break;
		case WIM_CLOSE:
			ms_debug("read_callback : WIM_CLOSE");
		break;
		case WIM_DATA:
			bsize=wHdr->dwBytesRecorded;

			/* ms_warning("read_callback : WIM_DATA (%p,%i)",wHdr,bsize); */
			m=(mblk_t*)wHdr->dwUser;
			m->b_wptr+=bsize;
			wHdr->dwUser=0;
			ms_mutex_lock(&d->mutex);
			putq(&d->rq,m);
			ms_mutex_unlock(&d->mutex);
			d->bytes_read+=wHdr->dwBufferLength;
			d->stat_input++;
			d->stat_input++;
#ifdef WIN32_TIMERS
			if (f->ticker->TimeEvent!=NULL)
				SetEvent(f->ticker->TimeEvent);
#endif
		break;
	}
}

// 工作线程，
static unsigned int __stdcall _proc_voice_capture(void *p)
{
	HRESULT hr;
	DWORD state, processed;
	WinSnd *d = (WinSnd*)p;

	CoInitialize(0);

	int bsize = WINSND_NSAMPLES * d->wfx.nAvgBytesPerSec / 8000;
	_open_win_ec(d, bsize);

	while (!d->quit_) {
		// 从 voice capture dmo 中读取数据，保存到 d->rq 中
		do {
			d->dmo_data_buffer_.dwStatus = 0;
			d->media_buf_->reset();
			hr = d->mo_->ProcessOutput(0, 1, &d->dmo_data_buffer_, &state);
			if (hr == S_FALSE)
				processed = 0;
			else if (hr == S_OK) {
				BYTE *data;
				processed = 0;
				hr = d->dmo_data_buffer_.pBuffer->GetBufferAndLength(&data, &processed);
				if (hr == S_OK && processed) {
					// 将数据写入 d->rq
					mblk_t *m = allocb(processed, 0);
					memcpy(m->b_wptr, data, processed);
					m->b_wptr += processed;

					ms_mutex_lock(&d->mutex);
					putq(&d->rq,m);
					ms_mutex_unlock(&d->mutex);
					d->bytes_read+=processed;
					d->stat_input++;
				}
			}
		} while (d->dmo_data_buffer_.dwStatus & DMO_OUTPUT_DATA_BUFFERF_INCOMPLETE);

		Sleep(10);
	}

	_close_win_ec(d);

	return 0;
}

static void winsnd_read_preprocess(MSFilter *f){
	WinSnd *d=(WinSnd*)f->data;
	MMRESULT mr;
	int i;
	int bsize;
	DWORD dwFlag;

	d->stat_input=0;
	d->stat_output=0;
	d->stat_notplayed=0;
	d->stat_minimumbuffer=WINSND_MINIMUMBUFFER;

	winsnd_apply_settings(d);

	if (d->supported_) {
		// 如果支持，则启用工作线程，从 voice capture dmo 中读数据，而无需启动 waveInxxx
		d->quit_ = false;
		d->th_ = (HANDLE)_beginthreadex(0, 0, _proc_voice_capture, d, 0, 0);
	}
	else {
		/* Init Microphone device */
		dwFlag = CALLBACK_FUNCTION;
		if (d->dev_id != WAVE_MAPPER)
			dwFlag = WAVE_MAPPED | CALLBACK_FUNCTION;
		mr = waveInOpen (&d->indev, d->dev_id, &d->wfx,
					(DWORD) read_callback, (DWORD)f, dwFlag);
		if (mr != MMSYSERR_NOERROR)
		{
			ms_error("Failed to prepare windows sound device. (waveInOpen:0x%i)", mr);
			mr = waveInOpen (&d->indev, WAVE_MAPPER, &d->wfx,
						(DWORD) read_callback, (DWORD)f, CALLBACK_FUNCTION);
			if (mr != MMSYSERR_NOERROR)
			{
				d->indev=NULL;
				ms_error("Failed to prepare windows sound device. (waveInOpen:0x%i)", mr);
				return ;
			}
		}

		bsize=WINSND_NSAMPLES*d->wfx.nAvgBytesPerSec/8000;
		ms_debug("Using input buffers of %i bytes",bsize);

		for(i=0;i<WINSND_NBUFS;++i){
			WAVEHDR *hdr=&d->hdrs_read[i];
			add_input_buffer(d,hdr,bsize);
		}
		d->running=TRUE;
		mr=waveInStart(d->indev);
		if (mr != MMSYSERR_NOERROR){
			ms_error("waveInStart() error");
			return ;
		}
#ifndef _TRUE_TIME
		ms_ticker_set_time_func(f->ticker,winsnd_get_cur_time,d);
#endif
	}
}

static void winsnd_read_postprocess(MSFilter *f){
	WinSnd *d=(WinSnd*)f->data;
	MMRESULT mr;
	int i;
#ifndef _TRUE_TIME
	ms_ticker_set_time_func(f->ticker,NULL,NULL);
#endif
	d->running=FALSE;

	if (d->supported_) {
		d->quit_ = true;
		WaitForSingleObject(d->th_, -1);
	}
	else {
		mr=waveInStop(d->indev);

		if (mr != MMSYSERR_NOERROR){
			ms_error("waveInStop() error");
			return ;
		}
		mr=waveInReset(d->indev);
		if (mr != MMSYSERR_NOERROR){
			ms_error("waveInReset() error");
			return ;
		}
		for(i=0;i<WINSND_NBUFS;++i){
			WAVEHDR *hdr=&d->hdrs_read[i];
			if (hdr->dwFlags & WHDR_PREPARED)
			{
				mr = waveInUnprepareHeader(d->indev,hdr,sizeof (*hdr));
				if (mr != MMSYSERR_NOERROR){
					ms_error("waveInUnPrepareHeader() error");
				}
			}
		}
		mr = waveInClose(d->indev);
		if (mr != MMSYSERR_NOERROR){
			ms_error("waveInClose() error");
			return ;
		}
	}

	ms_message("Shutting down sound device (playing: %i) (input-output: %i) (notplayed: %i)", d->nbufs_playing, d->stat_input - d->stat_output, d->stat_notplayed);
	flushq(&d->rq,0);
}

static void winsnd_read_process(MSFilter *f){
	WinSnd *d=(WinSnd*)f->data;
	mblk_t *m;
	int i;
	ms_mutex_lock(&d->mutex);
	while((m=getq(&d->rq))!=NULL){
		ms_queue_put(f->outputs[0],m);
	}
	ms_mutex_unlock(&d->mutex);

	if (!d->supported_) {
		for(i=0;i<WINSND_NBUFS;++i){
			WAVEHDR *hdr=&d->hdrs_read[i];
			if (hdr->dwUser==0) {
				MMRESULT mr;
				mr=waveInUnprepareHeader(d->indev,hdr,sizeof(*hdr));
				if (mr!=MMSYSERR_NOERROR)
					ms_warning("winsnd_read_process: Fail to unprepare header!");
				add_input_buffer(d,hdr,hdr->dwBufferLength);
			}
		}
	}
}

static void CALLBACK
write_callback(HWAVEOUT outdev, UINT uMsg, DWORD dwInstance,
                 DWORD dwParam1, DWORD dwParam2)
{
	WAVEHDR *hdr=(WAVEHDR *) dwParam1;
	WinSnd *d=(WinSnd*)dwInstance;
	
	switch (uMsg){
		case WOM_OPEN:
			break;
		case WOM_CLOSE:
		case WOM_DONE:
			if (hdr){
				d->nbufs_playing--;
			}
			if (d->stat_output==0)
			{
				d->stat_input=1; /* reset */
				d->stat_notplayed=0;
			}
			d->stat_output++;
		break;
	}
}

static void winsnd_write_preprocess(MSFilter *f){
	WinSnd *d=(WinSnd*)f->data;
	MMRESULT mr;
	DWORD dwFlag;
	int i;

	d->stat_input=0;
	d->stat_output=0;
	d->stat_notplayed=0;
	d->stat_minimumbuffer=WINSND_MINIMUMBUFFER;

	winsnd_apply_settings(d);
	/* Init Microphone device */
	dwFlag = CALLBACK_FUNCTION;
	if (d->dev_id != WAVE_MAPPER)
		dwFlag = WAVE_MAPPED | CALLBACK_FUNCTION;
	mr = waveOutOpen (&d->outdev, d->dev_id, &d->wfx,
	            (DWORD) write_callback, (DWORD)d, dwFlag);
	if (mr != MMSYSERR_NOERROR)
	{
		ms_error("Failed to open windows sound device %i. (waveOutOpen:0x%i)",d->dev_id, mr);
		mr = waveOutOpen (&d->outdev, WAVE_MAPPER, &d->wfx,
					(DWORD) write_callback, (DWORD)d, CALLBACK_FUNCTION);
		if (mr != MMSYSERR_NOERROR)
		{
			ms_error("Failed to open windows sound device %i. (waveOutOpen:0x%i)",d->dev_id, mr);
			d->outdev=NULL;
			return ;
		}
	}
	for(i=0;i<WINSND_OUT_NBUFS;++i){
		WAVEHDR *hdr=&d->hdrs_write[i];
		hdr->dwFlags=0;
		hdr->dwUser=0;
	}
	d->outcurbuf=0;
	d->overrun=FALSE;
	d->nsamples=0;
}

static void winsnd_write_postprocess(MSFilter *f){
	WinSnd *d=(WinSnd*)f->data;
	MMRESULT mr;
	int i;
	if (d->outdev==NULL) return;
	mr=waveOutReset(d->outdev);
	if (mr != MMSYSERR_NOERROR){
		ms_error("waveOutReset() error");
		return ;
	}
	for(i=0;i<WINSND_OUT_NBUFS;++i){
		WAVEHDR *hdr=&d->hdrs_write[i];
		mblk_t *old;
		if (hdr->dwFlags & WHDR_DONE){
			mr=waveOutUnprepareHeader(d->outdev,hdr,sizeof(*hdr));
			if (mr != MMSYSERR_NOERROR){
				ms_error("waveOutUnprepareHeader error");
			}
			old=(mblk_t*)hdr->dwUser;
			if (old) freemsg(old);
			hdr->dwUser=0;
		}
	}
	mr=waveOutClose(d->outdev);
	if (mr != MMSYSERR_NOERROR){
		ms_error("waveOutClose() error");
		return ;
	}
	d->ready=0;
	d->workaround=0;
}

static void playout_buf(WinSnd *d, WAVEHDR *hdr, mblk_t *m){
	MMRESULT mr;
	hdr->dwUser=(DWORD)m;
	hdr->lpData=(LPSTR)m->b_rptr;
	hdr->dwBufferLength=msgdsize(m);
	hdr->dwFlags = 0;
	mr = waveOutPrepareHeader(d->outdev,hdr,sizeof(*hdr));
	if (mr != MMSYSERR_NOERROR){
		ms_error("waveOutPrepareHeader() error");
		d->stat_notplayed++;
	}
	mr=waveOutWrite(d->outdev,hdr,sizeof(*hdr));
	if (mr != MMSYSERR_NOERROR){
		ms_error("waveOutWrite() error");
		d->stat_notplayed++;
	}else {
		d->nbufs_playing++;
	}
}

static void winsnd_write_process(MSFilter *f){
	WinSnd *d=(WinSnd*)f->data;
	mblk_t *m;
	MMRESULT mr;
	mblk_t *old;
	if (d->outdev==NULL) {
		ms_queue_flush(f->inputs[0]);
		return;
	}
	if (d->overrun){
		ms_warning("nbufs_playing=%i",d->nbufs_playing);
		if (d->nbufs_playing>0){
			ms_queue_flush(f->inputs[0]);
			return;
		}
		else d->overrun=FALSE;
	}
	while(1){
		int outcurbuf=d->outcurbuf % WINSND_OUT_NBUFS;
		WAVEHDR *hdr=&d->hdrs_write[outcurbuf];
		old=(mblk_t*)hdr->dwUser;
		if (d->nsamples==0){
			int tmpsize=WINSND_OUT_DELAY*d->wfx.nAvgBytesPerSec;
			mblk_t *tmp=allocb(tmpsize,0);
			memset(tmp->b_wptr,0,tmpsize);
			tmp->b_wptr+=tmpsize;
			playout_buf(d,hdr,tmp);
			d->outcurbuf++;
			d->nsamples+=WINSND_OUT_DELAY*d->wfx.nSamplesPerSec;
			continue;
		}
		m=ms_queue_get(f->inputs[0]);
		if (!m) break;
		d->nsamples+=msgdsize(m)/d->wfx.nBlockAlign;
		/*if the output buffer has finished to play, unprepare it*/
		if (hdr->dwFlags & WHDR_DONE){
			mr=waveOutUnprepareHeader(d->outdev,hdr,sizeof(*hdr));
			if (mr != MMSYSERR_NOERROR){
				ms_error("waveOutUnprepareHeader error");
			}
			freemsg(old);
			old=NULL;
			hdr->dwFlags=0;
			hdr->dwUser=0;
		}
		if (old==NULL){
			/* a free wavheader */
			playout_buf(d,hdr,m);
		}else{
			/* no more free wavheader, overrun !*/
			ms_warning("WINSND overrun, restarting");
			d->overrun=TRUE;
			d->nsamples=0;
			waveOutReset(d->outdev);
			break;
		}
		d->outcurbuf++;
	}
}

static int set_rate(MSFilter *f, void *arg){
	WinSnd *d=(WinSnd*)f->data;
	d->wfx.nSamplesPerSec=*((int*)arg);
	return 0;
}

static int get_rate(MSFilter *f, void *arg){
	WinSnd *d=(WinSnd*)f->data;
	*((int*)arg)=d->wfx.nSamplesPerSec;
	return 0;
}

static int set_nchannels(MSFilter *f, void *arg){
	WinSnd *d=(WinSnd*)f->data;
	d->wfx.nChannels=*((int*)arg);
	return 0;
}

static int winsnd_get_stat_input(MSFilter *f, void *arg){
	WinSnd *d=(WinSnd*)f->data;
	return d->stat_input;
}

static int winsnd_get_stat_ouptut(MSFilter *f, void *arg){
	WinSnd *d=(WinSnd*)f->data;

	return d->stat_output;
}

static int winsnd_get_stat_discarded(MSFilter *f, void *arg){
	WinSnd *d=(WinSnd*)f->data;

	return d->stat_notplayed;
}

static MSFilterMethod winsnd_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	, set_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE	, get_rate	},
	{	MS_FILTER_SET_NCHANNELS		, set_nchannels	},
	{	MS_FILTER_GET_STAT_INPUT, winsnd_get_stat_input },
	{	MS_FILTER_GET_STAT_OUTPUT, winsnd_get_stat_ouptut },
	{	MS_FILTER_GET_STAT_DISCARDED, winsnd_get_stat_discarded },
	{	0				, NULL		}
};

MSFilterDesc winsnd_read_desc={
	MS_WINSND_READ_ID,
	"MSWinSndRead",
	"Sound capture filter for Windows Sound drivers",
	MS_FILTER_OTHER,
	NULL,
    0,
	1,
	winsnd_init,
    winsnd_read_preprocess,
	winsnd_read_process,
	winsnd_read_postprocess,
    winsnd_uninit,
	winsnd_methods
};


MSFilterDesc winsnd_write_desc={
	MS_WINSND_WRITE_ID,
	"MSWinSndWrite",
	"Sound playback filter for Windows Sound drivers",
	MS_FILTER_OTHER,
	NULL,
    1,
	0,
	winsnd_init,
    winsnd_write_preprocess,
	winsnd_write_process,
	winsnd_write_postprocess,
	winsnd_uninit,
    winsnd_methods
};

MSFilter *ms_winsnd_read_new(MSSndCard *card){
	MSFilter *f=ms_filter_new_from_desc(&winsnd_read_desc);
	WinSndCard *wc=(WinSndCard*)card->data;
	WinSnd *d=(WinSnd*)f->data;
	d->dev_id=wc->in_devid;
	return f;
}


MSFilter *ms_winsnd_write_new(MSSndCard *card){
	MSFilter *f=ms_filter_new_from_desc(&winsnd_write_desc);
	WinSndCard *wc=(WinSndCard*)card->data;
	WinSnd *d=(WinSnd*)f->data;
	d->dev_id=wc->out_devid;
	return f;
}

MS_FILTER_DESC_EXPORT(winsnd_read_desc)
MS_FILTER_DESC_EXPORT(winsnd_write_desc)
