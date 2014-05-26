#include <stdio.h>
#include <stdlib.h>
#include <mediastreamer2/mediastream.h>
#include <zonekey/video-inf.h>
#include <mediastreamer2/zk.h264.source.h>
#include <mediastreamer2/zk.yuv_sink.h>
#include <zonekey/zqpsource.h>
#include <ortp/rtp.h>
#include <mediastreamer2/msrtp.h>
#include <Windows.h>
#include <tchar.h>
#include "../zkmcu_hlp/zkmcu_hlp_webcam.h"

#define SIMPLE 1

static HWND create_window();

// 构造接收链，返回 sink filter
static MSFilter *build_recver_chain()
{
	return 0;
}

// 构造 source --> decoder --> sink
static MSFilter *build_simple_chain(ZonekeyYUVSinkCallbackParam *yuvcb)
{
	MSFilter *f_source = ms_filter_new_from_name("ZonekeyH264Source");
	MSFilter *f_decoder = ms_filter_new(MS_H264_DEC_ID);
	MSFilter *f_sink = ms_filter_new_from_name("ZonekeyYUVSink");
	
	ms_filter_call_method(f_sink, ZONEKEY_METHOD_YUV_SINK_SET_CALLBACK_PARAM, yuvcb);

	ms_filter_link(f_source, 0, f_decoder, 0);
	ms_filter_link(f_decoder, 0, f_sink, 0);

	return f_source;
}

// 构造发送 source --> rtp session
static MSFilter *build_sender_chain(const char *ip, int port)
{
	MSFilter *f_source = ms_filter_new_from_name("ZonekeyH264Source");
	MSFilter *f_rtp = ms_filter_new(MS_RTP_SEND_ID);

	fprintf(stdout, "sendto %s:%d\n", ip, port);
	RtpSession *rtp = rtp_session_new(RTP_SESSION_SENDONLY);
	rtp_session_set_rtp_socket_send_buffer_size(rtp, 3*1024*1024);
	rtp_session_set_remote_addr_and_port(rtp, ip, port, port+1);
	rtp_session_set_payload_type(rtp, 100);
	//rtp_session_enable_jitter_buffer(rtp, 0);	// 禁用的

	JBParameters jb;
	jb.adaptive = 1;
	jb.max_packets = -1;
	jb.max_size = -1;
	jb.min_size = jb.nom_size = 300;
	rtp_session_set_jitter_buffer_params(rtp, &jb);

	ms_filter_call_method(f_rtp, MS_RTP_SEND_SET_SESSION, rtp);

	ms_filter_link(f_source, 0, f_rtp, 0);

	return f_source;
}

static void cb_yuv(void *ctx, int width, int height, unsigned char *data[4], int stride[4])
{
	static VideoCtx _render;

	if (!_render && width  > 0 && height > 0) {
		rv_open(&_render, create_window(), width, height);
	}
	if (_render) {
		rv_writepic(_render, PIX_FMT_YUV420P, data, stride);
	}
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s <url>\n", argv[0]);
		exit(-1);
	}

	bool webcam = false;
	const char *url = argv[1];
	if (!strcmp("webcam://localhost", url)) {
		if (!zkmcu_hlp_webcam_exist()) {
			fprintf(stderr, "NOT found local webcam!\n");
			return -1;
		}

		webcam = true;
	}

	ms_init();
	ortp_init();
	zonekey_h264_source_register();
	zonekey_yuv_sink_register();

	ZonekeyYUVSinkCallbackParam yuvcb;
	yuvcb.ctx = 0;
	yuvcb.push = cb_yuv;

#if SIMPLE 
	//MSFilter *f_source = build_simple_chain(&yuvcb);
	MSFilter *f_source = build_sender_chain("172.16.1.104", 50000);
	MSTicker *ticker = ms_ticker_new();
	
	ms_ticker_attach(ticker, f_source);
#else
	/**    source --> rtp sender ====> rtp recver --> decoder --> sink
	 */
	MSFilter *f_source = build_sender_chain();
	MSFilter *f_sink = build_recver_chain();

	MSTicker *ticker_send = ms_ticker_new(), *ticker_recv = ms_ticker_new();
	ms_ticker_attach(ticker_send, f_source), ms_ticker_attach(ticker_recv, f_sink);

#endif
	ZonekeyH264SourceWriterParam source_param;
	ms_filter_call_method(f_source, ZONEKEY_METHOD_H264_SOURCE_GET_WRITER_PARAM, &source_param);

	if (webcam) {
		zkmcu_hlp_webcam_t *cam = zkmcu_hlp_webcam_open(1024, 768, 5.0, 100);
		for (;; ) {
			const void *data;
			int len;
			double stamp;

			len = zkmcu_hlp_webcam_get_h264(cam, &data, &stamp);
			if (len > 0) {
				source_param.write(source_param.ctx, data, len, stamp);
			}
		}
	}
	else {
		void *zqp = 0;
		if (zqpsrc_open(&zqp, url) < 0) {
			fprintf(stderr, "can't open url='%s'\n", url);
			exit(-2);
		}

		zq_pkt *pkt = zqpsrc_getpkt(zqp);
		while (pkt) {
			if (pkt->type == 1) {
				// 视频
				source_param.write(source_param.ctx, pkt->ptr, pkt->len, pkt->pts/45000.0);
			}

			zqpsrc_freepkt(zqp, pkt);
			pkt = zqpsrc_getpkt(zqp);
		}
	}
	return 0;
}

LONG WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg) {
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

ATOM MyRegisterClass(TCHAR *classname)
{
	WNDCLASS wc;
	wc.style			= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc	= WndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 0;
	wc.hInstance		= 0;
	wc.lpszMenuName	= 0;
	wc.lpszClassName	= classname;
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);

	return RegisterClass(&wc);
}

static HWND _hwnd;
LONG WINAPI thread_proc(void *x)
{
	TCHAR szWindowClass[] = _T("test source and sink");
	MyRegisterClass(szWindowClass);
	_hwnd = CreateWindow(szWindowClass, _T("video render"), WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 960, 540,
		0, 0, 0, 0);
	
	ShowWindow(_hwnd, SW_SHOW);

	SetEvent((HANDLE)x);

	MSG msg;
	while (GetMessage(&msg, 0, 0, 0)) {
		if (msg.message == WM_CLOSE)
			break;
		DispatchMessage(&msg);
	}

	return 0;
}

static HWND create_window()
{
	HANDLE evt = CreateEvent(0, 0, 0, 0);
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)thread_proc, evt, 0, 0);
	WaitForSingleObject(evt, -1);

	return _hwnd;
}
