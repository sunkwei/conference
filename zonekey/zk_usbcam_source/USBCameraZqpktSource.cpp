#include "USBCameraZqpktSource.h"
extern "C" {
#  include <libavcodec/avcodec.h>
#  include <libswscale/swscale.h>
#  include <x264.h>
}
#include <opencv/highgui.h>
#include <zonekey/zqsender.h>
#include <zonekey/zq_atom_pkt.h>
#include <zonekey/zq_atom_types.h>

USBCameraZqpktSource::USBCameraZqpktSource(int cam, int port)
	: cam_id_(cam)
	, tcp_port_(port)
{
	fps_ = 25.0;
	kbitrate_ = 500;
	width_ = 960;
	height_ = 540;
}

USBCameraZqpktSource::~USBCameraZqpktSource(void)
{
}

int USBCameraZqpktSource::Start()
{
	quit_ = false;
	last_err_ = -1;
	// 启动工作线程，并等待打开 usb camera 返回
	evt_open_.reset();
	start();
	evt_open_.wait();
	return last_err_;
}

void USBCameraZqpktSource::Stop()
{
	quit_ = true;
	evt_close_.signal();
	join();	// 等待工作线程结束
}

static double now()
{
	struct timeval tv;
	ost::gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec / 1000000.0;
}

typedef void (*PFN_scale)(SwsContext *sws, IplImage *img, AVPicture tarpic);

/// 从 RGB24 转换到 YUV420P
static void scale_from_rgb24(SwsContext *sws, IplImage *img, AVPicture tar)
{
	AVPicture pic;
	pic.data[0] = (unsigned char*)img->imageData;
	pic.data[1] = pic.data[2] = pic.data[3] = 0;
	pic.linesize[0] = img->widthStep;
	pic.linesize[1] = pic.linesize[2] = pic.linesize[3];

	sws_scale(sws, pic.data, pic.linesize, 0, img->height, tar.data, tar.linesize);
}

// 根据 img 的类型，创建 swscale
static SwsContext *getScaler(IplImage *img, int w, int h, AVPicture *pic, PFN_scale *proc)
{
	SwsContext *sws = 0;

	// TODO: 应该检查 IplImage 的每种格式
	if (!strcmp(img->colorModel, "RGB")) {
		if (img->nChannels == 3 && img->depth == 8) {
			// RGB24
			sws = sws_getContext(img->width, img->height, PIX_FMT_BGR24, w, h, PIX_FMT_YUV420P, SWS_FAST_BILINEAR, 0, 0, 0);
			avpicture_free(pic);
			avpicture_alloc(pic, PIX_FMT_YUV420P, w, h);
			*proc = scale_from_rgb24;
		}
	}
	return sws;
}

// 压缩为 x264 nals
static void encode(x264_t *enc, AVPicture *pic, int64_t *pts, x264_nal_t **nals, int *nal_num)
{
	x264_picture_t ipic, opic;
	x264_picture_init(&ipic);
	x264_picture_init(&opic);

	ipic.i_type = X264_TYPE_AUTO;
	ipic.i_pts = *pts;
	ipic.img.i_csp = X264_CSP_I420;
	ipic.img.i_plane = 3;
	for (int i = 0; i < 3; i++) {
		ipic.img.plane[i] = pic->data[i];
		ipic.img.i_stride[i] = pic->linesize[i];
	}

	if (x264_encoder_encode(enc, nals, nal_num, &ipic, &opic) >= 0) {
		*pts++;
	}
}

// x264
static x264_t *getEncoder(int width, int height, double fps, int kbitrate)
{
	x264_param_t param;
	x264_param_default_preset(&param, "ultrafast", "zerolatency");

	param.i_threads = 0;

	param.i_width = width;
	param.i_height = height;

	param.i_keyint_max = (int)fps;	// 
	param.i_fps_den = param.i_timebase_den = 1;
	param.i_fps_num = param.i_timebase_num = (int)fps;

	param.b_repeat_headers = 1;	// 需要重复 sps/pps...
	param.b_annexb = 1;			// 便于直接发送

	// 码率控制
	param.rc.i_rc_method = X264_RC_ABR;
	param.rc.i_bitrate = kbitrate;
	param.rc.i_vbv_max_bitrate = (int)(kbitrate * 1.1);
	param.rc.i_vbv_buffer_size = (int)(kbitrate / 2.0);	// 希望在 500 ms 之内的调整
	param.rc.i_qp_max = 52;
	param.rc.i_qp_step = 6;

	x264_t *encoder = x264_encoder_open(&param);
	return encoder;
}

static int send(ZqSenderTcpCtx *sender, x264_nal_t *nals, int num, double stamp, int width, int height)
{
	// 是否为关键帧
	// FIXME: 简单的判断第一个是否为 sps
	bool key = false;
	if (nals[0].p_payload[4] == 0x67)
		key = true;

	if (key) // 发送 sync
		zqsnd_tcp_send(sender, CONST_ATOM_SYNC, 16);

	// DATA 长度
	size_t datalen = 0;
	for (int i = 0; i < num; i++) {
		datalen += nals[i].i_payload;
	}

	// 发送头信息
	zq_atom_header vid;
	vid.type.type_i = ZQ_ATOMS_TYPE_VIDEO;
	vid.size = sizeof(vid) + sizeof(zq_atom_header) + sizeof(zq_atom_video_header_data) + sizeof(zq_atom_header) + datalen;
	zqsnd_tcp_send(sender, &vid, sizeof(vid));

	zq_atom_header vh;
	vh.type.type_i = ZQ_ATOM_TYPE_VIDEO_HEADER;
	vh.size = sizeof(vh) + sizeof(zq_atom_video_header_data);
	zqsnd_tcp_send(sender, &vh, sizeof(vh));

	zq_atom_video_header_data vhd;
	vhd.stream_codec_type = 0x1b;
	vhd.id = 0;
	vhd.frame_type = key ? 'I' : 'P';
	vhd.width = width;
	vhd.height = height;
	vhd.pts = (unsigned int)stamp * 45000.0;
	vhd.dts = vhd.pts;
	zqsnd_tcp_send(sender, &vhd, sizeof(vhd));

	zq_atom_header dh;
	dh.type.type_i = ZQ_ATOM_TYPE_DATA;
	dh.size = sizeof(dh) + datalen;
	zqsnd_tcp_send(sender, &dh, sizeof(dh));

	for (int i = 0; i < num; i++) {
		// 发送 body 信息
		zqsnd_tcp_send(sender, nals[i].p_payload, nals[i].i_payload);
	}

	return 0;
}

void USBCameraZqpktSource::run()
{
	// 尝试打开 cam_id_ 指定的 usb cam，如果成功，设置 last_err_ = 0，并且设置 evt_open_
	CvCapture *cap = cvCaptureFromCAM(cam_id_);
	if (!cap) {
		// 一般是 cam_id_ 错误
		last_err_ = -1;
		evt_open_.signal();
		return;
	}

	// 获取一帧图像，决定入参
	IplImage *img = cvQueryFrame(cap);
	while (!img)
		img = cvQueryFrame(cap);

	AVPicture pic;
	avpicture_alloc(&pic, PIX_FMT_YUV420P, 16, 16);

	PFN_scale scale = 0;
	SwsContext *sws = getScaler(img, width_, height_, &pic, &scale);
	x264_t *x264 = getEncoder(width_, height_, fps_, kbitrate_);

	if (!sws || !x264) {
		last_err_ = -2;
		evt_open_.signal();
		return;
	}

	// 启动 zqsender
	ZqSenderTcpCtx  *sender = 0;
	if (zqsnd_open_tcp_server(&sender, tcp_port_, 0) < 0) {
		last_err_ = -3;
		evt_open_.signal();
		return;
	}

	// last_err_ = 0;
	last_err_ = 0;
	evt_open_.signal(); // 正式开始

	// 根据参数计算每帧之间的等待时间
	int frame_duration = (int)(1000.0 / fps_), delta = 0;	// delta用于修正cpu耗时，下次等待时间为 wait = frame_duration - delta;

	// 打开摄像头成功，根据配置参数，创建对应的encoder，scaler .....
	int64_t pts = 0;
	double tl = now();
	while (!quit_) {
		if (evt_close_.wait(frame_duration - delta))
			continue; // Stop() 被调用了

		// 采集
		img = cvQueryFrame(cap);
		double stamp = now();

		// 准备处理一帧数据 ...
		scale(sws, img, pic);	// pic 中已经为拉伸后的数据，直接使用 x264 压缩即可

		// 压缩 ...
		x264_nal_t *nals = 0;
		int nal_num = 0;
		encode(x264, &pic, &pts, &nals, &nal_num);

		// 检查是否有输出 
		if (nal_num > 0) {
			// 发送到 zqsender
			send(sender, nals, nal_num, stamp, img->width, img->height);
		}

		// 通过修正 delta，是帧率尽可能的稳定
		double tn = now();
		if (tn - tl > frame_duration / 1000.0) {
			// 说明等待时间超过了一帧的间隔，cpu 太慢了？需要增加 delta
			delta++;
		}
		else {
			// 
			delta --;
		}
		tl = tn;

		if (delta > frame_duration)
			delta = frame_duration;

		fprintf(stderr, "frame duration=%d, delta=%d\n", frame_duration, delta);
	}

	zqsnd_close_tcp_server(sender);
	avpicture_free(&pic);
	sws_freeContext(sws);
	x264_encoder_close(x264);
	cvReleaseCapture(&cap);
}
