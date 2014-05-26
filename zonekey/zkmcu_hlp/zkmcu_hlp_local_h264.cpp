#define OLD_VIDEORENDER 1
#define ZKPLAYER 1

#include <stdio.h>
#include <stdlib.h>
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/zk.h264.source.h>
#include <mediastreamer2/zk.yuv_sink.h>
#include "zkmcu_hlp_local_h264.h"
#if ZKPLAYER
#	include <zonekey/zkH264Player.h>
#endif
extern "C" {
#	include <libswscale/swscale.h>
#	include <libavcodec/avcodec.h>
}

#include <zonekey/video-inf.h>

namespace 
{
	struct CTX
	{
		CTX(void *wnd)
		{
			show_ = 0;
			hwnd_ = (HWND)wnd;
			last_width_ = 16;
			last_height_ = 16;
			sws_ = sws_getContext(last_width_, last_height_, PIX_FMT_YUV420P, last_width_, last_height_, PIX_FMT_RGB32, SWS_FAST_BILINEAR, 0, 0, 0);
			avpicture_alloc(&pic_, PIX_FMT_RGB32, last_width_, last_height_);

			f_src = ms_filter_new_from_name("ZonekeyH264Source");
			ms_filter_call_method(f_src, ZONEKEY_METHOD_H264_SOURCE_GET_WRITER_PARAM, &writer_param);

			f_dec = ms_filter_new(MS_H264_DEC_ID);

			f_yuv = ms_filter_new_from_name("ZonekeyYUVSink");
			ZonekeyYUVSinkCallbackParam cbs;
			cbs.ctx = this;
			cbs.push = cb_yuv;
			ms_filter_call_method(f_yuv, ZONEKEY_METHOD_YUV_SINK_SET_CALLBACK_PARAM, &cbs);

			ms_filter_link(f_src, 0, f_dec, 0);
			ms_filter_link(f_dec, 0, f_yuv, 0);

			ticker = ms_ticker_new();
			ms_ticker_attach(ticker, f_src);
		}

		~CTX()
		{
			ms_ticker_detach(ticker, f_src);
			ms_filter_destroy(f_src);
			ms_filter_destroy(f_dec);
			ms_filter_destroy(f_yuv);

			sws_freeContext(sws_);
			avpicture_free(&pic_);

			if (show_) {
				rv_close(show_);
				show_ = 0;
			}
		}

		int set(const void *data, int len, double stamp)
		{
			return writer_param.write(writer_param.ctx, data, len, stamp);
		}

		static void cb_yuv(void *ctx, int width, int height, unsigned char *data[4], int stride[4])
		{
			((CTX*)ctx)->on_yuv(width, height, data, stride);
		}

		void on_yuv(int width, int height, unsigned char *data[4], int stride[4])
		{
			// 此时收到完整图像，
			if (!show_) {
				rv_open(&show_, hwnd_, width, height);
				if (!show_) {
					MessageBoxA(0, "无法初始化显示模块，检查ocx", "严重错误", MB_OK);
					::exit(-1);
				}
			}

			PostMessage(hwnd_, 0x7789, 1, 0);	// 用于通知显示窗口，视频能够显示了

			draw_yuv(width, height, data, stride);
		}

		void draw_yuv(int width, int height, unsigned char *data[4], int stride[4])
		{
			if (width > 0 && height > 0) {
				if (width != last_width_ || height != last_height_) {
					avpicture_free(&pic_);
					sws_freeContext(sws_);

					avpicture_alloc(&pic_, PIX_FMT_RGB32, width, height);
					sws_ = sws_getContext(width, height, PIX_FMT_YUV420P, width, height, PIX_FMT_RGB32, SWS_FAST_BILINEAR, 0, 0, 0);

					last_width_ = width;
					last_height_ = height;
				}

				//sws_scale(sws_, data, stride, 0, height, pic_.data, pic_.linesize);

//				RECT rect;
//				GetWindowRect(hwnd_, &rect);
//				ResizeWindow(show_, 0, 0, rect.right - rect.left, rect.bottom - rect.top);
//				PutData(show_, pic_.data[0], 0, width, height);
				rv_writepic(show_, PIX_FMT_YUV420P, data, stride);
			}
		}

		HWND hwnd_;
		AVPicture pic_;
		int last_width_, last_height_;
		SwsContext *sws_;
		VideoCtx show_;
		MSTicker *ticker;
		MSFilter *f_src, *f_dec, *f_yuv;
		ZonekeyH264SourceWriterParam writer_param;
	};
};

zkmcu_hlp_local_h264_t *zkmcu_hlp_local_h264_open(void *wnd)
{
	return (zkmcu_hlp_local_h264_t*)new CTX(wnd);
}

void zkmcu_hlp_local_h264_close(zkmcu_hlp_local_h264_t *c)
{
	delete (CTX*)c;
}

int zkmcu_hlp_local_h264_set(zkmcu_hlp_local_h264_t *obj, const void *data, int len, double stamp)
{
	return ((CTX*)obj)->set(data, len, stamp);
}
