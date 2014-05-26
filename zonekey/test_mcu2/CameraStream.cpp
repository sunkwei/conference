#include "StdAfx.h"
#include "CameraStream.h"
#include "util.h"

#define WIDTH 640
#define HEIGHT 480
#define FPS 15
#define KBPS 300

CameraStream::CameraStream(void)
{
	switch_ = false;
	rtp_ = 0;
	sws_ = 0;
	cap_ = 0;
	encoder_ = 0;
	buf_size_ = 0x1000000;
	frame_buf_ = (unsigned char*)malloc(buf_size_);
	data_size_ = 0;
}

CameraStream::~CameraStream(void)
{
	free(frame_buf_);
}

double CameraStream::fps() const
{
	return FPS*1.0;
}

int CameraStream::open()
{
	CvCapture *cap = cvCaptureFromCAM(0);
	if (!cap) 
		return -1;

	cap_ = cap;

	return 0;
}

int CameraStream::close()
{
	if (rtp_) {
		ms_ticker_detach(ticker_sender_, filter_h264_sender_);
		ms_ticker_detach(ticker_recver_, filter_rtp_recver_);

		ms_ticker_destroy(ticker_sender_);
		ms_ticker_destroy(ticker_recver_);
		
		ms_filter_destroy(filter_h264_sender_);
		ms_filter_destroy(filter_rtp_sender_);
		ms_filter_destroy(filter_rtp_recver_);
		ms_filter_destroy(filter_decoder_);
		ms_filter_destroy(filter_yuv_sink_);

		rtp_session_destroy(rtp_);
		rtp_ = 0;
	}

	if (cap_) {
		cvReleaseCapture(&cap_);
		cap_ = 0;
	}

	if (sws_) {
		avpicture_free(&pic_);
		sws_freeContext(sws_);
		sws_ = 0;
	}

	if (encoder_) {
		x264_encoder_close(encoder_);
		encoder_ = 0;
	}

	return 0;
}

void CameraStream::init()
{
	/** 获取第一帧图像，初始化 sws_，创建 h264 encoder ....
	 */
	IplImage *img = cvQueryFrame(cap_);
	// FIXME: 未必是 rgb24 吧？？？
	sws_ = sws_getContext(img->width, img->height, PIX_FMT_RGB24, WIDTH, HEIGHT, PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);

	x264_param_t param;
	x264_param_default_preset(&param, "veryfast", "zerolatency");
	param.i_threads = 0;
	param.i_width = WIDTH;
	param.i_height = HEIGHT;
	param.i_keyint_max = FPS * 2;
	param.i_fps_den = 1;
	param.i_fps_num = FPS;
	param.i_slice_max_size = 1300;
	param.b_repeat_headers = 1;
	param.b_annexb = 1;
	param.rc.i_rc_method = X264_RC_ABR;
	param.rc.i_bitrate = KBPS;
	param.rc.i_vbv_max_bitrate = KBPS*1.1;
		
	encoder_ = x264_encoder_open(&param);

	avpicture_alloc(&pic_, PIX_FMT_YUV420P, WIDTH, HEIGHT);

	rtp_ = rtp_session_new(RTP_SESSION_SENDRECV);
	rtp_session_set_payload_type(rtp_, 100);
	rtp_session_set_remote_addr_and_port(rtp_, server_ip_.c_str(), server_rtp_port_, server_rtcp_port_);
	rtp_session_set_local_addr(rtp_, util_get_myip(), 0, 0);
	JBParameters jb;
	jb.adaptive = 1;
	jb.max_packets = 500;
	jb.max_size = -1;
	jb.min_size = jb.nom_size = 300;
	rtp_session_set_jitter_buffer_params(rtp_, &jb);

	filter_rtp_sender_ = ms_filter_new(MS_RTP_SEND_ID);
	ms_filter_call_method(filter_rtp_sender_, MS_RTP_SEND_SET_SESSION, rtp_);

	filter_h264_sender_ = ms_filter_new_from_name("ZonekeyH264Source");
	ms_filter_call_method(filter_h264_sender_, ZONEKEY_METHOD_H264_SOURCE_GET_WRITER_PARAM, &sender_params_);

	filter_rtp_recver_ = ms_filter_new(MS_RTP_RECV_ID);
	ms_filter_call_method(filter_rtp_recver_, MS_RTP_RECV_SET_SESSION, rtp_);

	filter_decoder_ = ms_filter_new(MS_H264_DEC_ID);

	filter_yuv_sink_ = ms_filter_new_from_name("ZonekeyYUVSink");
	// TODO: 显示 ...

	ms_filter_link(filter_rtp_recver_, 0, filter_decoder_, 0);
	ms_filter_link(filter_decoder_, 0, filter_yuv_sink_, 0);

	ticker_recver_ = ms_ticker_new();
	ms_ticker_attach(ticker_recver_, filter_rtp_recver_);

	ms_filter_link(filter_h264_sender_, 0, filter_rtp_sender_, 0);

	ticker_sender_ = ms_ticker_new();
	ms_ticker_attach(ticker_sender_, filter_h264_sender_);
}

void CameraStream::run_once()
{
	if (switch_) {
		switch_ = false;
		init();
	}

	if (rtp_) {
		AVPicture *pic = next_pic(cap_);
		if (pic) {
			int bytes = encode_frame(pic);
			if (bytes > 0) {
				sender_params_.write(sender_params_.ctx, frame_buf_, bytes, GetTickCount()/1000.0);
			}
		}
	}
}
