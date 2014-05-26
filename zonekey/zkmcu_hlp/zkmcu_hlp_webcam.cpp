#include <stdio.h>
#include <stdlib.h>
#include <opencv/highgui.h>
#include <cc++/thread.h>
#include <deque>
extern "C" {
#	include <libavcodec/avcodec.h>
#	include <libswscale/swscale.h>
#	include <x264.h>
}
#include "zkmcu_hlp_webcam.h"

namespace {
struct Frame
{
	double stamp;
	void *data;
	int data_len;
};

class Ctx : public ost::Thread
{
	CvCapture *capture_;
	bool quit_;
	std::deque<Frame> frames_;
	ost::Mutex cs_;
	void *buf_;
	int buf_size_, data_size_;
	int width_, height_, kbits_;
	double fps_;

public:
	Ctx(int width, int height, double fps, int kbits)
		: width_(width), height_(height), fps_(fps), kbits_(kbits)
	{
#define INIT_BUF_SIZE (64*1024)
		buf_ = malloc(INIT_BUF_SIZE);
		buf_size_ = INIT_BUF_SIZE;
		data_size_ = 0;

		capture_ = cvCaptureFromCAM(0);
		if (capture_) {
			quit_ = false;
			start();
		}
	}

	~Ctx()
	{
		if (capture_) {
			quit_ = true;
			join();

			cvReleaseCapture(&capture_);
		}
		free(buf_);
	}

	int get_h264(const void **data, double *pts)
	{
		cs_.enter();
		if (frames_.empty()) {
			cs_.leave();
			ost::Thread::sleep(40);
			return 0;
		}

		Frame frame = frames_.front();
		frames_.pop_front();

		// 将数据复制到 buf_ 中
		if (buf_size_ < frame.data_len) {
			buf_size_ = frame.data_len;
			buf_ = realloc(buf_, buf_size_);
		}
		memcpy(buf_, frame.data, frame.data_len);
		*pts = frame.stamp;
		*data = buf_;
		data_size_ = frame.data_len;

		free(frame.data);
		cs_.leave();

		return data_size_;
	}

	double now()
	{
		struct timeval tv;
		ost::gettimeofday(&tv, 0);
		return tv.tv_sec + tv.tv_usec / 1000000.0;
	}

	void run()
	{
		// 从 capture_ 中取出图像，sws 转换，h264 压缩，放到 frames_ 队列中
		IplImage *img = cvQueryFrame(capture_);
		while (!img) img = cvQueryFrame(capture_);

		AVPicture pic;
		avpicture_alloc(&pic, PIX_FMT_YUV420P, width_, height_);
		SwsContext *sws = sws_getContext(img->width, img->height, PIX_FMT_BGR24, width_, height_, PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);

		x264_param_t param;
		x264_param_default_preset(&param, "veryfast", "zerolatency");

		param.i_threads = 0;

		param.i_width = width_;
		param.i_height = height_;

		param.i_keyint_max = (int)fps_;	// 
		param.i_fps_den = param.i_timebase_den = 1;
		param.i_fps_num = param.i_timebase_num = (int)fps_;

		param.b_repeat_headers = 1;	// 需要重复 sps/pps...
		param.b_annexb = 1;			// 便于直接发送

		// 码率控制
		param.rc.i_rc_method = X264_RC_ABR;
		param.rc.i_bitrate = kbits_;
		param.rc.i_vbv_max_bitrate = (int)(kbits_ * 1.1);
		param.rc.i_vbv_buffer_size = (int)(kbits_ / 2.0);	// 希望在 500 ms 之内的调整
		param.rc.i_qp_max = 52;
		param.rc.i_qp_step = 6;

		x264_t *encoder = x264_encoder_open(&param);

		int64_t pts = 0;

		while (!quit_) {
			unsigned char *src[4] = { (unsigned char*)img->imageData, 0, 0, 0 };
			int src_stride[4] = { img->widthStep, 0, 0, 0 };
			sws_scale(sws, src, src_stride, 0, img->height, pic.data, pic.linesize);

			x264_picture_t ipic, opic;
			x264_nal_t *nals = 0;
			int nals_num = 0;

			x264_picture_init(&ipic);
			x264_picture_init(&opic);

			ipic.i_type = X264_TYPE_AUTO;
			ipic.i_pts = pts;
			ipic.img.i_csp = X264_CSP_I420;
			ipic.img.i_plane = 3;
			for (int i = 0; i < 3; i++) {
				ipic.img.plane[i] = pic.data[i];
				ipic.img.i_stride[i] = pic.linesize[i];
			}

			if (x264_encoder_encode(encoder, &nals, &nals_num, &ipic, &opic) >= 0) {
				// 将收到的
				save_frame(nals, nals_num, now());

				pts++;
			}

			ost::Thread::sleep((int)(1000 / fps_));	// 等待

			img = cvQueryFrame(capture_);	// 下一帧
		}

		x264_encoder_close(encoder);
		sws_freeContext(sws);
		avpicture_free(&pic);
	}

	void save_frame(x264_nal_t *nals, int num, double pts)
	{
		int bufsize = 4096, datasize = 0;
		char *buf = (char*)malloc(bufsize);

		// 直接将 nals 串行话，保存到 frames_ 队列中
		for (int i = 0; i < num; i++) {
			if (bufsize - datasize < nals[i].i_payload) {
				bufsize = (datasize + nals[i].i_payload + 4095) / 4096 * 4096;
				buf = (char*)realloc(buf, bufsize);
			}

			memcpy(buf+datasize, nals[i].p_payload, nals[i].i_payload);
			datasize += nals[i].i_payload;
		}

		Frame frame;
		frame.data = buf;
		frame.data_len = datasize;
		frame.stamp = pts;

		ost::MutexLock al(cs_);
		frames_.push_back(frame);
	}
};
};

int zkmcu_hlp_webcam_exist()
{
	CvCapture *cap = cvCaptureFromCAM(0);
	if (cap) {
		cvReleaseCapture(&cap);
		return 1;
	}
	return 0;
}

zkmcu_hlp_webcam_t *zkmcu_hlp_webcam_open(int width, int height, double fps, int kbits)
{
	return (zkmcu_hlp_webcam_t*)new Ctx(width, height, fps, kbits);
}

void zkmcu_hlp_webcam_close(zkmcu_hlp_webcam_t *ctx)
{
	delete (Ctx*)ctx;
}

int zkmcu_hlp_webcam_get_h264(zkmcu_hlp_webcam_t *ctx, const void **data, double *pts)
{
	return ((Ctx*)ctx)->get_h264(data, pts);
}
