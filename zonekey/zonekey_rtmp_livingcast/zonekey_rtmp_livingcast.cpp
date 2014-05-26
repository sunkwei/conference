#include <stdio.h>
#include <stdlib.h>
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/zk.rtmp.livingcast.h>
#include <zonekey/rtmp_inte.h>
#include <mediastreamer2/rfc3984.h>
#include <string.h>

namespace 
{
	class CTX
	{
		void *rtmp_;
		char *url_;
		bool running_;
		Rfc3984Context *rfc3984_;
		MSQueue queue_;
		unsigned int st_video_last_, st_audio_last_;	// 最新的时间戳
		double st_video_, st_audio_;	// 当前时间戳

	public:
		CTX()
		{
			ms_queue_init(&queue_);
			Init_Rtmp(&rtmp_);
			url_ = 0;
			running_ = false;
			rfc3984_ = 0;

			st_audio_ = -1.0;
			st_video_ = -1.0;
		}

		~CTX()
		{
			UnInit_Rtmp(rtmp_);
			if (url_)
				free(url_);
		}

		void set_url(const char *url)
		{
			if (url)
				url_ = strdup(url);
		}

		bool open()
		{
			if (!url_ || !rtmp_) return false;
			if (Open_Rtmp(rtmp_, url_) == 1) running_ = true;

			if (running_)
				rfc3984_ = rfc3984_new();


			return running_;
		}

		void close()
		{
			if (running_) {
				running_ = false;
				Close_Rtmp(rtmp_);
				rfc3984_destroy(rfc3984_);
				rfc3984_ = 0;
			}
		}

		bool saveH264(mblk_t *m)
		{
			if (!running_) return false;

			unsigned int delta = 0;
			unsigned int st = mblk_get_timestamp_info(m);
			if (st_video_ > 0.0)
				delta = (st - st_video_last_) / 90;	// 转换为毫秒
			else
				st_video_ = 0.0;
			st_video_last_ = st;

			st_video_ += delta;	// 当前视频时间戳的毫秒....
			
			unsigned char *obuf = 0;
			int olen = 0;
			rfc3984_unpack(rfc3984_, m, &queue_);
			while (mblk_t *om = ms_queue_get(&queue_)) {
				/** om 为 rfc3984 输出的 slices，需要加上 startcode
				 */
				int dlen = om->b_wptr - om->b_rptr;
				if (olen == 0) {
					obuf = (unsigned char*)malloc(dlen + 4);	// 00 00 00 01
					obuf[0] = obuf[1] = obuf[2] = 0;
					obuf[3] = 1;
					memcpy(&obuf[4], om->b_rptr, dlen);
					olen = dlen+4;
				}
				else {
					obuf = (unsigned char*)realloc(obuf, olen+dlen + 3);	// 00 00 01
					obuf[olen] = obuf[olen+1] = 0;
					obuf[olen+2] = 1;
					memcpy(&obuf[olen+3], om->b_rptr, dlen);
					olen += dlen + 3;
				}

				freemsg(om);	// 不再需要了
			}

			if (olen > 0) {
				int key = 0;
				// TODO: 根据 00 00 00 01 xx 判断是否为关键帧
				if (olen > 5) {
					if (obuf[4] == 0x67)
						key = -1;
				}
				SendVideoPacket_Rtmp(rtmp_, obuf, olen, key, st_video_);
			}

			if (obuf) free(obuf);

			return true;
		}

		bool SaveAAC(mblk_t *m)
		{
			if (!running_) return false;

			unsigned int delta = 0;
			unsigned int st = mblk_get_timestamp_info(m);
			if (st_audio_ > 0.0)
				delta = (st - st_audio_last_) / 32;	// 转换为毫秒 .... FIXME: 这里应该根据 ADTS 解析一下，得到采样率，先直接使用 32000Hz 吧

			SendAudioPacket_Rtmp(rtmp_, m->b_rptr, m->b_wptr - m->b_rptr, st_audio_);

			return true;
		}
	};
};

static void _init(MSFilter *f)
{
	f->data = new CTX;
}

static void _uninit(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;
	delete ctx;
	f->data = 0;
}

static void _preprocess(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;
	ctx->open();
}

static void _postprocess(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;
	ctx->close();
}

static void _process(MSFilter *f)
{
	CTX *ctx = (CTX*)f->data;

	/** 从 input pin 0 获取 AAC(with ADTS) 数据
		从 input pin 1 获取 H264 (rtp frag) 数据
	 */
	if (f->inputs[0]) {
		while (mblk_t *m = ms_queue_get(f->inputs[0]))
			ctx->SaveAAC(m);
	}
	if (f->inputs[1]) {
		while (mblk_t *m = ms_queue_get(f->inputs[1]))
			ctx->saveH264(m);
	}
}

static int _set_url(MSFilter *f, void *args)
{
	CTX *ctx = (CTX*)f->data;
	ctx->set_url((const char*)args);
	return 0;
}

static MSFilterMethod _methods[] = 
{
	{ ZONEKEY_METHOD_RTMP_LIVINGCAST_SET_URL, _set_url },
	{ 0, 0 },
};

static MSFilterDesc _desc = 
{
	MS_FILTER_PLUGIN_ID,
	"ZonekeyRTMPLivingcast",
	"zonekey rtmp livingcast filter, support h264+aac",
	MS_FILTER_OTHER,
	0,
	2,	// 0: aac, 1: h264
	0,	
	_init,
	_preprocess,
	_process,
	_postprocess,
	_uninit,
	_methods,
};

void zonekey_rtmp_livingcast_register()
{
	ms_filter_register(&_desc);
}
