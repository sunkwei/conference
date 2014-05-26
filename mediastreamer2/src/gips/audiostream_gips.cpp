#include <stdio.h>
#include <stdlib.h>
#include <WinSock2.h>
#include <../../gips/interface/GIPSVEBase.h>
#include <../../gips/interface/GIPSVEVQE.h>
#include <../../gips/interface/GIPSVEVolumeControl.h>
#include <../../gips/interface/GIPSVEVideoSync.h>
#include <../../gips/detours/src/detours.h>
#include "mediastreamer2/audiostream_gips.h"
#include <assert.h>
#include <cc++/thread.h>
#include <list>
#include <../../gips/interface/GIPSVECodec.h>
#if EXTERNAL_MEDIA
#	pragma comment(lib, "../../gips/lib/GIPSVoiceEngineLib_MT_externalmedia.lib")
#	include <../../gips/interface/GIPSVEExternalMedia.h>
#else
#	pragma comment(lib, "../../gips/lib/GIPSVoiceEngineLib_MT.lib")
#endif // external media

#pragma comment(lib, "avrt.lib")

/** 替换 GetSystemTime api
 */
void (__stdcall * Real_GetSystemTime)(LPSYSTEMTIME a0) = GetSystemTime;

static void __stdcall Mine_GetSystemTime(LPSYSTEMTIME a0)
{
    Real_GetSystemTime(a0);
	a0->wYear = 2010;
	a0->wMonth = 4;		// :)
}

namespace {
	class Init
	{
	public:
		Init()
		{
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)Real_GetSystemTime, Mine_GetSystemTime);
			DetourTransactionCommit();
		}
	};
};

static Init _init;

typedef struct zk_audio_data{
	short *buf;
	int len;
}zk_audio_data;
typedef std::list<zk_audio_data *> zk_audio_list;

namespace {
#if EXTERNAL_MEDIA
	class LocalGIPSVEMediaProcess: public GIPSVEMediaProcess
	{	
		bool is_record_;

		zk_audio_list *local_audio_;
		zk_audio_list *remote_audio_;
		ost::Mutex *mutex_;
		
		as_gips_on_audio_data cb_;
		void *user_data_;

	public:
		LocalGIPSVEMediaProcess(void)
		{
			is_record_ = false;
		}

		void start_record(zk_audio_list *local, zk_audio_list *remote, ost::Mutex *mutex,
			as_gips_on_audio_data cb, void *user_data)
		{
			local_audio_ = local;
			remote_audio_ = remote;
			mutex_ = mutex;
			cb_ = cb;
			user_data_ = user_data;
			is_record_ = true;
		}

		void stop_record(void)
		{
			mutex_->enter();
			is_record_ = false;
			while (!local_audio_->empty())
			{
				zk_audio_data *local_data = local_audio_->front();
				free(local_data->buf);
				delete local_data;
				local_audio_->pop_front();
			}
			mutex_->leave();
		}

		void Process(int channelNumber, short* audioLeft10ms, short* audioRight10ms, 
			int length, int samplingFreq, bool isStereo)
		{
			assert(!isStereo);
			if (is_record_)
			{
				zk_audio_data *audio_data = new zk_audio_data;
				audio_data->buf = (short *)malloc(sizeof(short) * length);
				audio_data->len = length;
				memcpy(audio_data->buf, audioLeft10ms, sizeof(short) * length);
				mutex_->enter();
				local_audio_->push_back(audio_data);
				if (remote_audio_->size()>=2 && local_audio_->size()>=2)
				{
					zk_audio_data *local_1 = local_audio_->front();
					local_audio_->pop_front();
					zk_audio_data *local_2 = local_audio_->front();
					local_audio_->pop_front();
					zk_audio_data *remote_1 = remote_audio_->front();
					remote_audio_->pop_front();
					zk_audio_data *remote_2 = remote_audio_->front();
					remote_audio_->pop_front();

					short *buf = (short *)malloc(sizeof(short) * (local_1->len + local_2->len));
					for (int i = 0; i < local_1->len; i++)
					{
						short sum = local_1->buf[i] + remote_1->buf[i];
						if (local_1->buf[i] < 0 && remote_1->buf[i] < 0 && sum >= 0)
							sum = -32767;
						if (local_1->buf[i] > 0 && remote_1->buf[i] > 0 && sum <= 0)
							sum = 37267;
						buf[i] = sum;
					}
					
					for (int i = 0; i < local_2->len; i++)
					{
						short sum = local_2->buf[i] + remote_2->buf[i];
						if (local_2->buf[i] < 0 && remote_2->buf[i] < 0 && sum >= 0)
							sum = -32767;
						if (local_2->buf[i] > 0 && remote_2->buf[i] > 0 && sum <= 0)
							sum = 37267;
						buf[i + local_1->len] = sum;
					}

					cb_(buf, sizeof(short), local_1->len + local_2->len, user_data_);
					
					free(buf);
					free(local_1->buf);
					free(local_2->buf);
					free(remote_1->buf);
					free(remote_2->buf);
					delete local_1;
					delete local_2;
					delete remote_1;
					delete remote_2;
				}

				mutex_->leave();
			}
		}
	};

	class RemoteGIPSVEMediaProcess: public GIPSVEMediaProcess
	{	
		bool is_record_;

		zk_audio_list *remote_audio_;
		ost::Mutex *mutex_;

	public:
		RemoteGIPSVEMediaProcess(void)
		{
			is_record_ = false;
		}

		void start_record(zk_audio_list *remote, ost::Mutex *mutex)
		{
			remote_audio_ = remote;
			mutex_ = mutex;
			is_record_ = true;
		}

		void stop_record(void)
		{
			mutex_->enter();
			is_record_ = false;
			while (!remote_audio_->empty())
			{
				zk_audio_data *remote_data = remote_audio_->front();
				free(remote_data->buf);
				delete remote_data;
				remote_audio_->pop_front();
			}
			mutex_->leave();

		}

		void Process(int channelNumber, short* audioLeft10ms, short* audioRight10ms, 
			int length, int samplingFreq, bool isStereo)
		{
			assert(!isStereo);
			if (is_record_)
			{
				zk_audio_data *audio_data = new zk_audio_data;
				audio_data->buf = (short *)malloc(sizeof(short) * length);
				audio_data->len = length;
				memcpy(audio_data->buf, audioLeft10ms, sizeof(short) * length);
				mutex_->enter();
				remote_audio_->push_back(audio_data);
				mutex_->leave();
			}
		}
	};
#endif // External Media support

	void log_init()
	{
		FILE *fp = fopen("ec.log", "w");
		if (fp) {
			fprintf(fp, "================ begin ==================\n");
			fclose(fp);
		}

		FILE *fpv = fopen("ec.vol.log", "w");
		if (fpv) {
			fprintf(fpv, "..... begin .....\n");
			fclose(fpv);
		}
	}

	void log(const char *fmt, ...)
	{
		static ost::Mutex _cs;
		ost::MutexLock al(_cs);

		va_list mark;
		char buf[2048];

		va_start(mark, fmt);
		vsnprintf(buf, sizeof(buf), fmt, mark);
		va_end(mark);

		FILE *fp = fopen("ec.log", "at");
		if (fp) {
			fprintf(fp, buf);
			fclose(fp);
		}
	}

	void log_vol(int iv[10], int ov[10])
	{
		FILE *fp = fopen("ec.vol.log", "at");
		if (fp) {
			fprintf(fp, "mic: %d %d %d %d %d %d %d %d %d %d\n",
				iv[0], iv[1], iv[2], iv[3], iv[4], iv[5], iv[6], iv[7], iv[8], iv[9]);
			fprintf(fp, "spk: %d %d %d %d %d %d %d %d %d %d\n",
				ov[0], ov[1], ov[2], ov[3], ov[4], ov[5], ov[6], ov[7], ov[8], ov[9]);
		
			fprintf(fp, "\n");
			fclose(fp);
		}
	}

	class MyLog : public GIPSVoiceEngineObserver
	{
	public:
		void CallbackOnError(const int errCode, const int channel)
		{
			log("ERROR: errcode=%d, channel=%d\n", errCode, channel);
		}

		void CallbackOnTrace(const GIPS::TraceLevel level, const char* message, const int length)
		{
			log("TRACE: level=%d, %s\n", level, message);
		}
	};

	class Ctx : ost::Thread
	{
		int local_rtp_port_, local_rtcp_port_;
		GIPSVoiceEngine *engine_;
		GIPSVEBase *vebase_;
		GIPSVEVQE *vevqe_;
		GIPSVEVolumeControl *vol_;
#if EXTERNAL_MEDIA
		GIPSVEExternalMedia *external_media_;
#endif // 
		GIPSVECodec *vecodec_;
		GIPSVEVideoSync *vesync_;

		int chan_no_;
#if EXTERNAL_MEDIA
		LocalGIPSVEMediaProcess local_audio_process;
		RemoteGIPSVEMediaProcess remote_audio_process;
#endif
		zk_audio_list local_audio_list;
		zk_audio_list remote_audio_list;
		ost::Mutex mutex;
		MyLog log_;
		bool quit_;

	public:
		Ctx(int rtp_port, int rtcp_port) : local_rtp_port_(rtp_port), local_rtcp_port_(rtcp_port)
		{
			char *p;
			log_init();

			engine_ = GIPSVoiceEngine::Create();

			vebase_ = GIPSVEBase::GIPSVE_GetInterface(engine_);
			vevqe_ = GIPSVEVQE::GetInterface(engine_);
			vol_ = GIPSVEVolumeControl::GetInterface(engine_);
			vesync_ = GIPSVEVideoSync::GetInterface(engine_);
			vecodec_ = GIPSVECodec::GetInterface(engine_);
#if EXTERNAL_MEDIA
			external_media_ = GIPSVEExternalMedia::GetInterface(engine_);
#endif

			int rc = vebase_->GIPSVE_Init(5, 15, 2010);
			if (rc < 0) {
				fprintf(stderr, "%s: GIPSVE_Init error\n", __FUNCTION__);
				::exit(-1);
			}

			// 通知
			p = getenv("zonekey_audio_gips_trace");
			if (p && !strcmp(p, "enable")) {
				vebase_->GIPSVE_SetObserver(log_);
			}

			// 回声消除模块
			int ec_mode, aes_mode, aes_attn;
			bool enabled;
			rc = vevqe_->GIPSVE_GetECStatus(enabled, ((GIPS_ECmodes&)ec_mode), ((GIPS_AESmodes&)aes_mode), aes_attn);
			log("GetECStatus: rc=%d, enabled=%d, ec_mode=%d, aes_mode=%d, aes_attn=%d\n", rc, enabled, ec_mode, aes_mode, aes_attn);

			ec_mode = EC_DEFAULT;
			p = getenv("zonekey_audio_gips_ec_mode");
			if (p) ec_mode = atoi(p);
			if (ec_mode < 0 || ec_mode > EC_NEC_IAD)
				ec_mode = EC_DEFAULT;

			log("SetECStatus: ecmode=%d\n",	ec_mode);

			// 总是启用 AEC
			rc = vevqe_->GIPSVE_SetECStatus(true, GIPS_ECmodes(ec_mode));
			log("\tret %d, err=%d\n", rc, vebase_->GIPSVE_LastError());

			// 噪声模块
			int ns_mode;
			bool enable_ns;
			rc = vevqe_->GIPSVE_GetNSStatus(enable_ns, ((GIPS_NSmodes&)ns_mode));
			log("GetNSStatus: rc=%d, enabled=%d, ns_mode=%d\n", rc, enable_ns, ns_mode);

			ns_mode = NS_DEFAULT;
			enable_ns = true;

			p = getenv("zonekey_audio_gips_ns_mode");
			if (p)
				ns_mode = atoi(p);
			if (ns_mode < 0 || ns_mode > NS_VERY_HIGH_SUPPRESSION)
				ns_mode = NS_DEFAULT;

			// enable ns
			p = getenv("zonekey_audio_gips_ns");
			if (p && !strcmp(p, "disable"))
				enable_ns = false;
			log("SetNSStatus: enable=%d, mode=%d\n", enable_ns, ns_mode);
			rc = vevqe_->GIPSVE_SetNSStatus(enable_ns, (GIPS_NSmodes)ns_mode);
			log("\tret %d, err=%d\n", rc, vebase_->GIPSVE_LastError());

			// 自动增益模块
			bool enable_agc;
			int agc_mode;
			rc = vevqe_->GIPSVE_GetAGCStatus(enable_agc, (GIPS_AGCmodes&)agc_mode);
			log("GetAGCStatus: rc=%d, enabled=%d, agc_mode=%d\n", rc, enable_agc, agc_mode);

			enable_agc = false;
			agc_mode = AGC_DEFAULT;
			p = getenv("zonekey_audio_gips_agc");
			if (p && !strcmp(p, "enable"))
				enable_agc = true;

			p = getenv("zonekey_audio_gips_agc_mode");
			if (p)
				agc_mode = atoi(p);
			if (agc_mode < 0 || agc_mode > AGC_STANDALONE_DIGITAL)
				agc_mode = AGC_DEFAULT;

			log("SetAGCStatus: enable=%d, agc_mode=%d\n", enable_agc, agc_mode);
			rc = vevqe_->GIPSVE_SetAGCStatus(enable_agc, (GIPS_AGCmodes)agc_mode);
			log("\tret %d, err=%d\n", rc, vebase_->GIPSVE_LastError());

			//quit_ = false;
			//ost::Thread::start();
		}

		~Ctx()
		{
			//quit_ = true;
			//join();

			vecodec_->Release();
			vesync_->Release();
			vol_->Release();
			vevqe_->Release();
			vebase_->GIPSVE_Release();
			GIPSVoiceEngine::Delete(engine_);
			log("======= end =============\n");
		}

		void run()
		{
			int iv[10], ov[10];
			int n = 0;
			while (!quit_) {
				// 每隔xxx，检查 mic 音量输入和输出
				sleep(100);
				if (vol_) {
					unsigned int mic=0, speaker=0;
					vol_->GIPSVE_GetSpeechInputLevel(mic);
					vol_->GIPSVE_GetSpeechOutputLevel(chan_no_, speaker);

					iv[n] = mic, ov[n] = speaker;
					n++;
					if (n == 10) {
						log_vol(iv, ov);
						n = 0;
					}
				}
			}
		}

		int start(int pt, const char *remote_ip, int remote_rtp_port, int remote_rtcp_port, int jitter_comp)
		{
			int rc;
			char *p;

			/// 启动通道 ...
			chan_no_ = vebase_->GIPSVE_CreateChannel();
			if (chan_no_ < 0) {
				fprintf(stderr, "%s: base->GIPSVE_CreateChannel() ERR, ret %d\n", __FUNCTION__, chan_no_);
				::exit(-1);
			}

			// jitter buffer
			int delay_ms = 320;
			p = getenv("zonekey_audio_gips_delay");
			if (p)
				delay_ms = atoi(p);
			if (delay_ms < 0 || delay_ms > 1000)
				delay_ms = 320;
			log("SetMinimumPlayoutDelay: delay ms =%d\n", delay_ms);
			rc = vesync_->GIPSVE_SetMinimumPlayoutDelay(chan_no_, delay_ms);
			log("ret %d, err=%d\n", rc, vebase_->GIPSVE_LastError());

			// 禁用 VAD
			rc = vecodec_->GIPSVE_SetVADStatus(chan_no_, false);
			log("\tSetVADStatus: mode=%d, rc=%d\n", false, rc);

#if 0
			// 缺省使用pcm
			p = getenv("zonekey_audio_gips_ilbc");
			if (p && !strcmp(p, "enable")) {
				log("using ilbc codec\n");
				GIPS_CodecInst cinst;
				// 支持的编码;
				for (int i = 0; i < vecodec_->GIPSVE_NumOfCodecs(); i++) {
					vecodec_->GIPSVE_GetCodec(i, cinst);
					if (!stricmp(cinst.plname, "iLBC")) {
						rc = vecodec_->GIPSVE_SetSendCodec(chan_no_, cinst);
						rc  = vecodec_->GIPSVE_SetRecPayloadType(chan_no_, cinst);
						fprintf(stdout, "enable iLBC codec, ret %d\n", rc);
						break;
					}
					printf("codec #%d: name=%s\n", i, cinst.plname);
					printf("\t type=%d\n", cinst.pltype);
					printf("\t channel=%d\n", cinst.channels);
					printf("\t freq=%d\n", cinst.plfreq);
					printf("\t acsize=%d\n", cinst.pacsize);
					printf("\t rate=%d\n", cinst.rate);
				}
			}
			else {
				log("using PCM codec\n");
			}
#endif
			// 这个版本仅仅作简单的发送，只有rtp;
			//external_media_->GIPSVE_SetExternalMediaProcessing(PLAYBACK_PER_CHANNEL, chan_no_, true, remote_audio_process);
			//external_media_->GIPSVE_SetExternalMediaProcessing(RECORDING_PER_CHANNEL, chan_no_, true, local_audio_process);
			rc = vebase_->GIPSVE_SetLocalReceiver(chan_no_, local_rtp_port_);
			log("\tSetLocalReceiver: rc=%d, ch=%d, rtp_port=%d\n", rc, chan_no_, local_rtp_port_);
			rc = vebase_->GIPSVE_StartListen(chan_no_);
			log("\tStartListen: rc=%d\n", rc);
			rc = vebase_->GIPSVE_SetSendDestination(chan_no_, remote_rtp_port, remote_ip);
			log("\tSetSendDestination: rc=%d, remote rtp port=%d, remote_ip=%s\n", rc, remote_rtp_port, remote_ip);
			rc = vebase_->GIPSVE_StartSend(chan_no_);
			log("\tStartSend: rc=%d\n", rc);
			rc = vebase_->GIPSVE_StartPlayout(chan_no_);
			log("\tStartPlayout: rc=%d\n", rc);

			return 0;
		}

		int stop()
		{
			log("%s: stop all\n", __FUNCTION__);
			vebase_->GIPSVE_StopSend(chan_no_);
			vebase_->GIPSVE_StopListen(chan_no_);
			vebase_->GIPSVE_StopPlayout(chan_no_);
			vebase_->GIPSVE_DeleteChannel(chan_no_);

			return 0;
		}

#if EXTERNAL_MEDIA
		int record_start(as_gips_on_audio_data cb, void *userdata)
		{
			local_audio_process.start_record(&local_audio_list, &remote_audio_list, &mutex, cb, userdata);
			remote_audio_process.start_record(&remote_audio_list, &mutex);
			return 0;
		}

		int record_stop()
		{
			remote_audio_process.stop_record();
			local_audio_process.stop_record();
			return 0;
		}
#endif // EXTERNAL MEDIA
	};
};

AudioStreamGips *audio_stream_gips_new(int rtp_port, int rtcp_port)
{
	return (AudioStreamGips*)new Ctx(rtp_port, rtcp_port);
}

int audio_stream_gips_start(AudioStreamGips *as, int pt, const char *remote_ip, int remote_rtp, int remote_rtcp, int jitter_comp)
{
	return ((Ctx*)as)->start(pt, remote_ip, remote_rtp, remote_rtcp, jitter_comp);
}

int audio_stream_gips_stop(AudioStreamGips *as)
{
	return ((Ctx*)as)->stop();
}

#if EXTERNAL_MEDIA
int audio_stream_record_start(AudioStreamGips *as, as_gips_on_audio_data cb, void *userdata)
{
	return ((Ctx*)as)->record_start(cb, userdata);
}

int audio_stream_record_stop(AudioStreamGips *as)
{
	return ((Ctx *)as)->record_stop();
}
#endif // External media support

void audio_stream_gips_destroy(AudioStreamGips *as)
{
	delete (Ctx*)as;
}
