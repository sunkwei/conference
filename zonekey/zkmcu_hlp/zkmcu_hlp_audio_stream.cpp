#include <mediastreamer2/mediastream.h>
#include "zkmcu_hlp_audio_stream.h"
#include <Windows.h>

#define USING_GIPS 1
#define RECORDING 1

#if USING_GIPS
#	include <mediastreamer2/audiostream_gips.h>
#endif // 

namespace 
{
	class CTX
	{
#if USING_GIPS
		AudioStreamGips *audio_stream_;
#else
		MSSndCard *snd_capt_, *snd_play_;
		AudioStream *audio_stream_;
#endif
		void log(const char *fmt, ...)
		{
			va_list mark;
			char buf[1024];

			va_start(mark, fmt);
			vsnprintf(buf, sizeof(buf), fmt, mark);
			va_end(mark);

			FILE *fp = fopen("hlp.log", "at");
			if (fp) {
				fprintf(fp, buf);
				fclose(fp);
			}
		}

	public:
		CTX(const char *ip, int rtp, int rtcp)
		{
#if USING_GIPS
			log("hlp: %s: ip=%d, rtp=%d, rtcp=%d\n", __FUNCTION__, ip, rtp, rtcp);
			audio_stream_ = audio_stream_gips_new(40000, -1);
			audio_stream_gips_start(audio_stream_, 0, ip, rtp, rtcp, 40);
#else
			MSSndCardManager *manager=ms_snd_card_manager_get();
			snd_capt_ = ms_snd_card_manager_get_default_capture_card(manager);
			snd_play_ =  ms_snd_card_manager_get_default_playback_card(manager);

			if (!snd_capt_ || !snd_play_) {
				MessageBoxA(0, "Ã»ÓÐÕÒµ½ºÏÊÊµÄÉù¿¨", "ÖÂÃü´íÎó", MB_OK);
				::exit(-1);
			}

			fp_ = fopen("audio_stream.pcm", "wb");
			cb_data.on_data = on_data;
			cb_data.userdata = this;

			audio_stream_ = audio_stream_new(40000, 40001, false);
			audio_stream_set_echo_canceller_params(audio_stream_, 250, 0, 0);
			audio_stream_enable_automatic_gain_control(audio_stream_, 0);
			audio_stream_enable_noise_gate(audio_stream_, 0);
//			audio_stream_start_full_record(audio_stream_, &av_profile, ip, rtp, 
//				ip, rtcp, 110, 20, 0, 0, snd_play_, snd_capt_, 1, &cb_data);
			audio_stream_start_full(audio_stream_, &av_profile, ip, rtp, 
				ip, rtcp, 110, 20, 0, 0, snd_play_, snd_capt_, 1);

#endif
		}

		zkrecorder_cb_data cb_data;
		FILE *fp_;
		static void on_data(unsigned char *data, int len, void *opaque)
		{
			CTX *self = (CTX*)opaque;
			fwrite(data, 1, len, self->fp_);
		}

		~CTX()
		{
#if USING_GIPS
			audio_stream_gips_stop(audio_stream_);
			audio_stream_gips_destroy(audio_stream_);
#else
			audio_stream_stop(audio_stream_);
			fclose(fp_);
#endif
		}
	};
};

zkmcu_hlp_audio_stream_t *zkmcu_hlp_audio_stream_open(int port, const char *server, int rtp_port, int rtcp_port)
{
	return (zkmcu_hlp_audio_stream_t*)new CTX(server, rtp_port, rtcp_port);
}

void zkmcu_hlp_audio_stream_close(zkmcu_hlp_audio_stream_t *ins)
{
	delete ((CTX*)ins);
}
