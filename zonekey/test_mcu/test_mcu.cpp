// test_mcu.cpp : 定义应用程序的入口点。
//

#include "stdafx.h"
#include <stdio.h>
#include "test_mcu.h"
#include <zonekey/xmpp_uac.h>
#include <string>
#include <sstream>
#include <mediastreamer2/mediastream.h>
#include <WindowsX.h>
#include <zonekey/video-inf.h>
#include <mediastreamer2/zk.yuv_sink.h>
#include <mediastreamer2/zk.h264.source.h>
#include <map>
#include <ortp/ortp.h>
#include <mediastreamer2/msrtp.h>
#include <vector>
#include <assert.h>
#include <opencv/highgui.h>

// 是否启动本地 audio stream
#define LOCAL_AUDIO_STREAM 1

static void cmd_get_sources(HWND hwnd);
static void add_audio_stream(HWND hwnd);
typedef std::map<std::string, std::string> KVS;
static HWND _mainwnd = 0;
static const char *get_mcu_jid();
static void log(const char *fmt, ...);
static CvCapture *_cam = 0;
static bool _connected = false;

struct LocalAudioStream
{
	AudioStream *as_;
	MSSndCard *capt_, *play_;
	int sid;
};

static LocalAudioStream _las;

KVS util_parse_options(const char *options)
{
	KVS kvs;
	std::vector<char *> strs;
	if (!options) return kvs;

	// 使用 strtok() 分割
	char *tmp = strdup(options);
	char *p = strtok(tmp, "&");
	while (p) {
		strs.push_back(p);
		p = strtok(0, "&");
	}

	for (std::vector<char*>::iterator it = strs.begin(); it != strs.end(); ++it) {
		char *key = strtok(*it, "=");
		assert(key);

		char *value = strtok(0, "\1");	// FIXME:
		if (!value)
			value = "";

		kvs[key] = value;
	}

	free(tmp);
	return kvs;
}

bool chk_params(const KVS &kvs, char info[1024], const char *key, ...)
{
	bool ok = true;
	va_list list;
	va_start(list, key);
	strcpy(info, "");

	while (key) {
		KVS::const_iterator itf = kvs.find(key);
		if (itf == kvs.end()) {
			ok = false;
			snprintf(info, 1024, "'%s' not found", key);
			break;
		}

		key = va_arg(list, const char*);
	}
	va_end(list);

	return ok;	// 都存在
}

#define MAX_LOADSTRING 100

// 全局变量:
HINSTANCE hInst;								// 当前实例
static uac_token_t *_uac = 0;
TCHAR szTitle[MAX_LOADSTRING];					// 标题栏文本
TCHAR szWindowClass[MAX_LOADSTRING];			// 主窗口类名
static bool _stream_added = false;

static void delete_audio_stream()
{
	char options[128];
	snprintf(options, sizeof(options), "streamid=%d", _las.sid);
	zk_xmpp_uac_send_cmd(_uac, get_mcu_jid(), "test.dc.del_stream", options, 0, 0);

	audio_stream_stop(_las.as_);
}

static void connect_notify(uac_token_t *const token, int is_connected, void *userdata)
{
	if (!is_connected) {
		MessageBox(0, "无法连接到 xmpp_server", "错误", MB_OK);
	}
	else {
		_connected = true;
	}
}

// 此代码模块中包含的函数的前向声明:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

#include <cc++/thread.h>
extern "C" {
#	include <x264.h>
#	include <libavcodec/avcodec.h>
#	include <libswscale/swscale.h>
}
#include <mediastreamer2/msrtp.h>

class CamThread : public ost::Thread
{
	bool quit_;
	SwsContext *sws_;
	x264_t *encoder_;
	AVPicture pic_;
	int64_t pts_;
	unsigned char *frame_buf_;
	int buf_size_;
	int data_size_;
	RtpSession *rtp_sess_;	// 用于发送
	OrtpEvQueue *evq_;	// 用于处理 rtcp
	MSTicker *ticker_;
	MSFilter *filter_sender_, *filter_rtp_;
	std::string server_ip_;
	int server_rtp_port_, server_rtcp_port_;
	int source_id_;
	ZonekeyH264SourceWriterParam sender_params_;

#define FPS 10
#define WIDTH 320
#define HEIGHT 240
#define KBPS 50

	void run()
	{
		assert(_cam);

		// 申请一路 Source
		while (!_connected && !quit_) sleep(10);
		sleep(500);
		if (quit_) 
			return;

		char host_name[64], desc[128];
		gethostname(host_name, sizeof(host_name));
		char *user_name = getenv("USERNAME");
		snprintf(desc, sizeof(desc), "payload=100&desc=VIDEO: %s,user=%s", host_name, user_name);
		zk_xmpp_uac_send_cmd(_uac, get_mcu_jid(), "test.fc.add_source", desc, this, response);

		init(_cam);

		int frame_duration = 1000 / FPS;
		pts_ = 0;

		frame_buf_ = (unsigned char *)malloc(0x100000);
		buf_size_ = 0x100000;
		data_size_ = 0;

		rtp_sess_ = rtp_session_new(RTP_SESSION_SENDONLY);
		rtp_session_set_payload_type(rtp_sess_, 100);	// h264
		evq_ = ortp_ev_queue_new();
		rtp_session_register_event_queue(rtp_sess_, evq_);

		char *username = getenv("USERNAME");
		char hostname[64];
		gethostname(hostname, sizeof(hostname));
		rtp_session_set_source_description(rtp_sess_, username, hostname, 0, 0, 0, 0, 0);

		assert(encoder_);

		ticker_ = ms_ticker_new();
		filter_sender_ = ms_filter_new_from_name("ZonekeyH264Source");
		filter_rtp_ = ms_filter_new(MS_RTP_SEND_ID);

		ms_filter_call_method(filter_rtp_, MS_RTP_SEND_SET_SESSION, rtp_sess_);
		ms_filter_call_method(filter_sender_, ZONEKEY_METHOD_H264_SOURCE_GET_WRITER_PARAM, &sender_params_);

		while (!quit_ && source_id_ == -1) {
			sleep(100);
		}

		if (source_id_ >= 0 && !quit_) {
			// 此时得到了 Source 的描述了
			rtp_session_set_remote_addr_and_port(rtp_sess_, server_ip_.c_str(), server_rtp_port_, server_rtcp_port_);

			JBParameters jb;
			jb.adaptive = 1;
			jb.max_packets = 300;
			jb.max_size = -1;
			jb.min_size = jb.nom_size = 400;
			rtp_session_set_jitter_buffer_params(rtp_sess_, &jb);

			// 连接 filters
			ms_filter_link(filter_sender_, 0, filter_rtp_, 0);

			ms_ticker_attach(ticker_, filter_sender_);
		}

		// 根据帧率，从 _cam 获取数据，转换为 YUV，使用 h264 压缩，发布到 mcu 上，作为一路 Source
		while (!quit_) {
			if (source_id_ >= 0) {
				AVPicture *pic = next_pic(_cam);
				unsigned char *data;
				int len = encode_frame(pic);
				if (len > 0) {
					if (source_id_ >= 0) {
						// TODO: send ...
						assert(sender_params_.write);
						sender_params_.write(sender_params_.ctx, frame_buf_, len, GetTickCount()/1000.0);
					}
				}
			}
			sleep(frame_duration);

			// 处理 rtcp
			process_rtcp(rtp_sess_, evq_);
		}

		ms_ticker_detach(ticker_, filter_sender_);
		ms_ticker_destroy(ticker_);
		rtp_session_destroy(rtp_sess_);
		ortp_ev_queue_destroy(evq_);

		if (source_id_ != -1) {
			char options[128];
			snprintf(options, sizeof(options), "sid=%d", source_id_);
			zk_xmpp_uac_send_cmd(_uac, get_mcu_jid(), "test.fc.del_source", options, 0, 0);

			sleep(1000);	// 等待发送到 mcu
		}
	}

	void process_rtcp(RtpSession *rs, OrtpEvQueue *q)
	{
		OrtpEvent *ev = ortp_ev_queue_get(q);
		while (ev) {
			OrtpEventType type = ortp_event_get_type(ev);
			if (type == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
				// 收到，一般是 RR
				mblk_t *m = ortp_event_get_data(ev)->packet;
				do {
					if (rtcp_is_RR(m)) {
						const rtcp_common_header *header = rtcp_get_common_header(m);
						for (int i = 0; i < header->rc; i++) {
						}
					}
				} while (rtcp_next_packet(m));
			}
			ortp_event_destroy(ev);
			ev = ortp_ev_queue_get(q);
		}
	}

	/** 压缩 pic 数据，返回 nals 字节流 */
	int encode_frame(AVPicture *pic)
	{
		x264_picture_t pic_in, pic_out;
		x264_picture_init(&pic_in);
		x264_picture_init(&pic_out);
		
		pic_in.img.i_csp = X264_CSP_I420;
		pic_in.img.i_plane = 3;
		for (int i = 0; i < 3; i++) {
			pic_in.img.i_stride[i] = pic->linesize[i];
		}
		pic_in.img.plane[0] = pic->data[0];
		pic_in.img.plane[1] = pic->data[2];
		pic_in.img.plane[2] = pic->data[1];

		x264_nal_t *nals;
		int nal_cnt;

		pic_in.i_pts = pts_++;
		pic_in.i_type = X264_TYPE_AUTO;

		data_size_ = 0;

		int rc = x264_encoder_encode(encoder_, &nals, &nal_cnt, &pic_in, &pic_out);
		if (rc >= 0 && nal_cnt > 0) {
			for (int i = 0; i < nal_cnt; i++) {
				save_nal(nals[i]);
			}
		}

		return data_size_;
	}

	/** 将 nals 保存到 frame_buf_ 中
	 */
	int save_nal(x264_nal_t nal)
	{
		if (buf_size_ - data_size_ < nal.i_payload) {
			buf_size_ += nal.i_payload;
			frame_buf_ = (unsigned char *)realloc(frame_buf_, buf_size_);
		}

		memcpy(frame_buf_ + data_size_, nal.p_payload, nal.i_payload);
		data_size_ += nal.i_payload;
		return nal.i_payload;
	}

	/** 从 cam 取出一帧数据，并且转换为 WIDTH x HEIGHT
	 */
	AVPicture *next_pic(CvCapture *cam)
	{
		assert(sws_);
		IplImage *img = cvQueryFrame(cam);

		unsigned char *data[4];
		data[0] = (unsigned char*)img->imageData;
		data[1] = data[2] = data[3] = 0;
		int stride[4];
		stride[0] = img->widthStep;
		stride[1] = stride[2] = stride[3] = 0;

		sws_scale(sws_, data, stride, 0, img->height, pic_.data, pic_.linesize);

		return &pic_;
	}

	/** 获取第一帧图像，初始化 sws_，创建 h264 encoder ....
	 */
	int init(CvCapture *cap)
	{
		IplImage *img = cvQueryFrame(cap);
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

		return 0;
	}

	static void response(zk_xmpp_uac_msg_token *rsp_token, const char *result, const char *option)
	{
		CamThread *This = (CamThread*)rsp_token->userdata;
		if (!strcmp("test.fc.add_source", rsp_token->cmd)) {
			if (!strcmp("ok", result)) {
				KVS params = util_parse_options(option);
				char info[1024];
				assert(chk_params(params, info, "sid", "server_ip", "server_rtp_port", "server_rtcp_port", 0));
				
				This->set_stream_desc(atoi(params["sid"].c_str()), params["server_ip"].c_str(), 
					atoi(params["server_rtp_port"].c_str()), atoi(params["server_rtcp_port"].c_str()));
			}
		}
	}

public:
	CamThread()
	{
		source_id_ = -1;
		quit_ = false;
		start();

	}

	~CamThread()
	{
		quit_ = true;
		join();
	}

	void set_stream_desc(int source_id, const char *server_ip, int server_rtp_port, int server_rtcp_port)
	{
		server_ip_ = server_ip;
		server_rtp_port_ = server_rtp_port;
		server_rtcp_port_ = server_rtcp_port;
		source_id_ = source_id;
	}
};

static CamThread *_cam_thread = 0;

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	zk_xmpp_uac_init();
	ortp_init();
	ms_init();

	_las.as_ = 0;

	zonekey_yuv_sink_register();
	zonekey_h264_source_register();

	rtp_profile_set_payload(&av_profile, 100, &payload_type_h264);
	rtp_profile_set_payload(&av_profile, 110, &payload_type_speex_wb);

	// 如果有摄像头，启动一路 h264 流
	CvCapture *cap = cvCaptureFromCAM(0);
	if (cap) {
		// 将启动一路 h264 source ....
		int hr = MessageBox(0, "你的摄像头将作为一路 Source，可能被大家点播，确认么?", "警告", MB_OKCANCEL);
		if (hr == IDOK)
			_cam = cap;
		else {
			cvReleaseCapture(&cap);
		}
	}

	if (_cam) {
		_cam_thread = new CamThread;
	}

	const char *domain = "app.zonekey.com.cn";
	char *p = getenv("xmpp_domain");
	if (p)
		domain = p;

	// 使用 normaluser 登录
	char jid[128];
	snprintf(jid, sizeof(jid), "normaluser@%s", domain);
	cb_xmpp_uac cbs = { 0, 0, 0, 0, connect_notify };
	_uac = zk_xmpp_uac_log_in(jid, "ddkk1212", &cbs, 0);

	MSG msg;
	HACCEL hAccelTable;

	// 初始化全局字符串
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_TEST_MCU, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// 执行应用程序初始化:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TEST_MCU));

	// 主消息循环:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (_cam_thread)
		delete _cam_thread;

	if (_las.as_) {
		delete_audio_stream();

		// 需要等一会儿，等到 mcu 收到
		Sleep(1000);
	}

	cvReleaseCapture(&cap);

	return (int) msg.wParam;
}

// 返回 mcu jid
static const char *get_mcu_jid()
{
	static std::string _jid;

	if (_jid.empty()) {
		const char *domain = "app.zonekey.com.cn";
		char *p = getenv("xmpp_domain");
		if (p)
			domain = p;

		std::stringstream ss;
		ss << "mse_s_000000000000_mcu_0" << "@" << domain;
		_jid = ss.str();
	}

	return _jid.c_str();
}

#define WM_XMPP_NOTIFY (WM_USER + 1000)

struct XMPP_Notify
{
	const char *cmd;
	const char *cmd_options;

	const char *result;	// 
	const char *result_options;
};

// 处理收到的 xmpp response，必定不在窗口线程中执行，因此必须 SendMessage(WM_XMPP_NOTIFY) 到主窗口线程中
static void cb_response(zk_xmpp_uac_msg_token *rsp_token, const char *result, const char *option)
{
	HWND hwnd = (HWND)rsp_token->userdata;

	XMPP_Notify notify;
	notify.cmd = rsp_token->cmd;
	notify.cmd_options = rsp_token->option;
	notify.result = result;
	notify.result_options = option;

	SendMessage(hwnd, WM_XMPP_NOTIFY, 0, (LPARAM)&notify);
}

// 在窗口线程中调用，获取 mcu 上的 sources 列表
static void cmd_get_sources(HWND hwnd)
{
	zk_xmpp_uac_send_cmd(_uac, get_mcu_jid(), "test.fc.list_sources", 0, hwnd, cb_response);
	zk_xmpp_uac_send_cmd(_uac, get_mcu_jid(), "test.dc.list_streams", 0, hwnd, cb_response);
}

static void add_audio_stream(HWND hwnd)
{
#if LOCAL_AUDIO_STREAM
	char desc[128];
	char host[64], *user = getenv("USERNAME");
	gethostname(host, sizeof(host));
	snprintf(desc, sizeof(desc), "payload=110&desc=AUDIO: %s, %s", host, user);
	zk_xmpp_uac_send_cmd(_uac, get_mcu_jid(), "test.dc.add_stream", desc, hwnd, cb_response);
#endif // 
}

static void add_audio_stream_done(const char *options)
{
	char info[1024];
	KVS param = util_parse_options(options);
	assert(chk_params(param, info, "streamid", "server_ip", "server_rtp_port", "server_rtcp_port", 0));

	int streamid = atoi(param["streamid"].c_str());
	std::string server_ip = param["server_ip"];
	int rtp_port = atoi(param["server_rtp_port"].c_str());
	int rtcp_port = atoi(param["server_rtcp_port"].c_str());

	// 在主窗口线程中调用
	log("%s:\r\n"
		"\tstreamid=%d\r\n"
		"\tserver_ip=%s\r\n"
		"\tserver_rtp_port=%d\r\n"
		"\tserver_rtcp_port=%d\r\n",
		__FUNCTION__, streamid, server_ip.c_str(), rtp_port, rtcp_port);

	_las.sid = streamid;

	// audio stream ....

	MSSndCardManager *manager=ms_snd_card_manager_get();
	_las.capt_ = ms_snd_card_manager_get_default_capture_card(manager);
	_las.play_ =  ms_snd_card_manager_get_default_playback_card(manager);

	if (!_las.capt_ || !_las.play_) {
		MessageBox(0, "没有找到能用的 mic 和 player", "错误", MB_OK);
		exit(-1);
	}

	_las.as_ = audio_stream_new(0, 0, false);
	
	//audio_stream_enable_automatic_gain_control(_las.as_, 1);
	//audio_stream_enable_noise_gate(_las.as_, 1);

	audio_stream_start_full(_las.as_, &av_profile, server_ip.c_str(), rtp_port, server_ip.c_str(), rtcp_port, 110, 200, 0, 0, 
		_las.play_, _las.capt_, 1);

}

//
//  函数: MyRegisterClass()
//
//  目的: 注册窗口类。
//
//  注释:
//
//    仅当希望
//    此代码与添加到 Windows 95 中的“RegisterClassEx”
//    函数之前的 Win32 系统兼容时，才需要此函数及其用法。调用此函数十分重要，
//    这样应用程序就可以获得关联的
//    “格式正确的”小图标。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TEST_MCU));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_TEST_MCU);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目的: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // 将实例句柄存储在全局变量中

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

static HWND _log = 0;

static void log(const char *fmt, ...)
{
	va_list mark;
	char buf[1024];

	va_start(mark, fmt);
	vsnprintf(buf, sizeof(buf), fmt, mark);
	va_end(mark);

	// 在主窗口线程中
	int len = Edit_GetTextLength(_log);

	if (len > 32000)
		Edit_SetSel(_log, 0, len);
	else
		Edit_SetSel(_log, len, len);
	Edit_ReplaceSel(_log, buf);
}

const char *util_get_myip()
{
	static std::string _ip;
	if (_ip.empty()) {
		char hostname[512];
		gethostname(hostname, sizeof(hostname));

		struct hostent *host = gethostbyname(hostname);
		if (host) {
			if (host->h_addrtype == AF_INET) {
				in_addr addr;
				
				for (int i = 0; host->h_addr_list[i]; i++) {
					addr.s_addr = *(ULONG*)host->h_addr_list[i];
					_ip = inet_ntoa(addr);
					if (strstr(_ip.c_str(), "192.168.56."))	// virtual box 的ip地址，需要跳过
						continue;
				}
			}
		}
	}

	return _ip.c_str();
}

struct VOD
{
	VideoCtx video_render;
	MSTicker *ticker;
	MSFilter *rtp_recver, *decoder, *yuv_sinker;
	RtpSession *rtp;
	void *hwnd;

	std::string server_ip;
	int server_rtp_port, server_rtcp_port;
	bool ok_rtp, ok_rtcp;	// 是否收到来自服务器的 rtp 数据了
	OrtpEvQueue *evq;	// 用于接受 rtcp

};

static void yuv_push(void *ctx, int width, int height, unsigned char *data[4], int stride[4])
{
	VOD *vod = (VOD*)ctx;
	if (!vod->video_render) {
		rv_open(&vod->video_render, vod->hwnd, width, height);
	}

	if (vod->video_render) {
		rv_writepic(vod->video_render, PIX_FMT_YUV420P, data, stride);
	}

	// 已经收到 rtp 包
	vod->ok_rtp = true;
}

static void send_somthing(int sock, const char *ip, int port)
{
	// 发送几个 ascii
	sockaddr_in to;
	to.sin_family = AF_INET;
	to.sin_port = htons(port);
	to.sin_addr.s_addr = inet_addr(ip);

	sendto(sock, "abcdefghijklmnopqrstuvwxyz", 26, 0, (sockaddr*)&to, sizeof(to));
}

// XXX: 需要首先利用 rtp socket 发送一些数据，让 mcu 的 publisher 能够得到自己的真实通讯地址，因为有可能在 NAT 后面
static void active_rtp_session(RtpSession *rtp, const char *ip, int rtp_port, int rtcp_port)
{
	/** FIXME：应该持续发送，直到收到了 server 的数据 ....
	 */
	int sock = rtp_session_get_rtp_socket(rtp);
	send_somthing(sock, ip, rtp_port);

	sock = rtp_session_get_rtcp_socket(rtp);
	send_somthing(sock, ip, rtcp_port);
}

static VOD *init_vod(HWND hwnd, const char *server_ip, int rtp_port, int rtcp_port)
{
	/* 构造 h264 接收，显示
	 */

	VOD *vod = new VOD;
	vod->video_render = 0;
	vod->hwnd = hwnd;

	vod->ticker = ms_ticker_new();

	vod->rtp = rtp_session_new(RTP_SESSION_RECVONLY);
	rtp_session_set_local_addr(vod->rtp, util_get_myip(), 0, 0);
	rtp_session_set_payload_type(vod->rtp, 100);
	rtp_session_set_remote_addr_and_port(vod->rtp, server_ip, rtp_port, rtcp_port);

	vod->ok_rtp = vod->ok_rtcp = false;
	vod->server_ip = server_ip;
	vod->server_rtp_port = rtp_port;
	vod->server_rtcp_port = rtcp_port;

	vod->evq = ortp_ev_queue_new();
	rtp_session_register_event_queue(vod->rtp, vod->evq);

	JBParameters jbp;
	jbp.adaptive = 1;
	jbp.max_packets = 500;
	jbp.max_size = -1;
	jbp.min_size = jbp.nom_size = 200;
	rtp_session_set_jitter_buffer_params(vod->rtp, &jbp);

	vod->rtp_recver = ms_filter_new(MS_RTP_RECV_ID);
	ms_filter_call_method(vod->rtp_recver, MS_RTP_RECV_SET_SESSION, vod->rtp);

	vod->decoder = ms_filter_new(MS_H264_DEC_ID);

	ZonekeyYUVSinkCallbackParam cp;
	cp.ctx = vod;
	cp.push = yuv_push;
	vod->yuv_sinker = ms_filter_new_from_name("ZonekeyYUVSink");
	ms_filter_call_method(vod->yuv_sinker, ZONEKEY_METHOD_YUV_SINK_SET_CALLBACK_PARAM, &cp);

	ms_filter_link(vod->rtp_recver, 0, vod->decoder, 0);
	ms_filter_link(vod->decoder, 0, vod->yuv_sinker, 0);

	ms_ticker_attach(vod->ticker, vod->rtp_recver);

	// 激活 socket
	active_rtp_session(vod->rtp, server_ip, rtp_port, rtcp_port);

	return vod;
}

// 处理收到的 rtcp
void process_rtcp(RtpSession *rtp, const char *pre, mblk_t *m)
{
	log("[%s]: %s: \r\n", pre, __FUNCTION__);
	if (rtcp_is_SR(m)) {
		log("\t <SR>\r\n");
		do {
			const report_block_t *rb = rtcp_SR_get_report_block(m, 0);
			if (rb) {
				float rt = rtp_session_get_round_trip_propagation(rtp);
				unsigned int jitter = report_block_get_interarrival_jitter(rb);
				float lost = 100.0 * report_block_get_fraction_lost(rb)/256.0;

				log("\ttssrc=%u: rtt=%f, jitter=%u, lost=%f\r\n",
					rb->ssrc, rt, jitter, lost);
			}
		} while (rtcp_next_packet(m));

	}
	else if (rtcp_is_RR(m)) {
	}
	else if (rtcp_is_SDES(m)) {
	}
}


// 在 timer 中周期调用，用于检查收到的 rtcp 包
void process_vod(VOD *vod)
{
	OrtpEvent *ev = ortp_ev_queue_get(vod->evq);
	while (ev) {
		// 收到 rtcp 包了，
		vod->ok_rtcp = true;

		OrtpEventType type = ortp_event_get_type(ev);
		if (type == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
			process_rtcp(vod->rtp, "vod", ortp_event_get_data(ev)->packet);
		}

		ortp_event_destroy(ev);
		ev = ortp_ev_queue_get(vod->evq);
	}

	if (!vod->ok_rtp || !vod->ok_rtcp) {
		active_rtp_session(vod->rtp, vod->server_ip.c_str(), vod->server_rtp_port, vod->server_rtcp_port);
	}
}

LONG WINAPI vod_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	XMPP_Notify *notify;
	static int sid = -1, sink_id = -1;
	CREATESTRUCT *cs;
	static VOD *_vod = 0;
	const int timer_id = 10010;

	switch (msg) {
	case WM_CREATE:
		cs = (CREATESTRUCT*)lparam;
		sid = (int)cs->lpCreateParams;
		return 0;

	case WM_XMPP_NOTIFY:
		notify = (XMPP_Notify*)lparam;
		log("WM_XMPP_NOTIFY: cmd=%s, result=%s, options=%s\r\n",
			notify->cmd, notify->result, notify->result_options);

		if (!strcmp(notify->cmd, "test.fc.add_sink")) {
			KVS param = util_parse_options(notify->result_options);
			char info[1024];
			assert(chk_params(param, info, "sinkid", "server_rtp_port", "server_rtcp_port", "server_ip", 0));

			sink_id = atoi(param["sinkid"].c_str());
			int rtp_port = atoi(param["server_rtp_port"].c_str());
			int rtcp_port = atoi(param["server_rtcp_port"].c_str());
			const char *ip = param["server_ip"].c_str();

			log("\t en: sinkid=%d, rtp_port=%d, rtcp_port=%d, sip=%s\r\n", sink_id, rtp_port, rtcp_port, ip);

			// TODO: 创建接收器，....
			_vod = init_vod(hwnd, ip, rtp_port, rtcp_port);

			SetTimer(hwnd, timer_id, 300, 0);
		}

		return 0;

	case WM_TIMER:
		if (wparam == timer_id) {
			process_vod(_vod);
		}
		break;

	case WM_CLOSE:
		// 删除 sink
		{
			char options[128];
			snprintf(options, sizeof(options), "sinkid=%d", sink_id);
			zk_xmpp_uac_send_cmd(_uac, get_mcu_jid(), "test.fc.del_sink", options, 0, 0); // 不管response了
			DestroyWindow(hwnd);
			return 0;
		}
		break;

	case WM_DESTROY:
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

LONG WINAPI aod_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	XMPP_Notify *notify;
	static int sid = -1, sink_id = -1;
	CREATESTRUCT *cs;

	switch (msg) {
	case WM_CREATE:
		cs = (CREATESTRUCT*)lparam;
		sid = (int)cs->lpCreateParams;
		return 0;

	case WM_XMPP_NOTIFY:
		notify = (XMPP_Notify*)lparam;
		log("WM_XMPP_NOTIFY: cmd=%s, result=%s, options=%s\r\n",
			notify->cmd, notify->result, notify->result_options);

		if (!strcmp(notify->cmd, "test.fc.add_sink")) {
			KVS param = util_parse_options(notify->result_options);
			char info[1024];
			assert(chk_params(param, info, "sinkid", "server_rtp_port", "server_rtcp_port", "server_ip", 0));

			sink_id = atoi(param["sinkid"].c_str());
			int rtp_port = atoi(param["server_rtp_port"].c_str());
			int rtcp_port = atoi(param["server_rtcp_port"].c_str());
			const char *ip = param["server_ip"].c_str();

			log("\t en: sinkid=%d, rtp_port=%d, rtcp_port=%d, sip=%s\r\n", sink_id, rtp_port, rtcp_port, ip);

			// TODO: 创建接收器，....
			//init_vod(hwnd, ip, rtp_port, rtcp_port);
		}

		return 0;

	case WM_CLOSE:
		// 删除 sink
		{
			char options[128];
			snprintf(options, sizeof(options), "sinkid=%d", sink_id);
			zk_xmpp_uac_send_cmd(_uac, get_mcu_jid(), "test.fc.del_sink", options, 0, 0); // 不管response了
			DestroyWindow(hwnd);
			return 0;
		}
		break;

	case WM_DESTROY:
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

/** 创建新窗口，点播 id 对应的视频
 */
static void vod(HWND hwnd, int id, const char *title, bool stream = false)
{
	WNDCLASSEX wcex;
	char classname[128];

	snprintf(classname, sizeof(classname), "vod show");
	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= vod_proc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInst;
	wcex.hIcon			= 0;
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= 0;
	wcex.lpszClassName	= classname;
	wcex.hIconSm		= 0;

	RegisterClassEx(&wcex);

	HWND wnd = CreateWindow(classname, title, WS_VISIBLE | WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 960, 540, 0, 0, hInst, (void*)id);
	ShowWindow(wnd, SW_SHOW);

	// 给 mcu 发送点播命令
	char options[128];

	if (stream) {
		snprintf(options, sizeof(options), "streamid=%d", id);
		zk_xmpp_uac_send_cmd(_uac, get_mcu_jid(), "test.dc.add_sink", options, wnd, cb_response);
	}
	else {
		snprintf(options, sizeof(options), "sid=%d", id);
		zk_xmpp_uac_send_cmd(_uac, get_mcu_jid(), "test.fc.add_sink", options, wnd, cb_response);
	}

	log("add sink for %d\r\n", id);
}

/** 创建新窗口，对应音频
 */
static void aod(HWND hwnd, int id, const char *title)
{
	WNDCLASSEX wcex;
	char classname[128];

	snprintf(classname, sizeof(classname), "aod show");
	wcex.cbSize = sizeof(WNDCLASSEX);
	
	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= aod_proc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInst;
	wcex.hIcon			= 0;
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= 0;
	wcex.lpszClassName	= classname;
	wcex.hIconSm		= 0;

	RegisterClassEx(&wcex);

	HWND wnd = CreateWindow(classname, title, WS_VISIBLE | WS_SYSMENU | WS_CAPTION, CW_USEDEFAULT, CW_USEDEFAULT, 400, 100, 0, 0, hInst, (void*)id);
	ShowWindow(wnd, SW_SHOW);

	char options[128];
	snprintf(options, sizeof(options), "sid=%d", id);
	zk_xmpp_uac_send_cmd(_uac, get_mcu_jid(), "test.fc.add_sink", options, wnd, cb_response);
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的: 处理主窗口的消息。
//
//  WM_COMMAND	- 处理应用程序菜单
//  WM_PAINT	- 绘制主窗口
//  WM_DESTROY	- 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;
	static HWND _list = 0, _list_dc = 0;
	static int _timerid = 1000;
	static int _interleaved = 5000;
	XMPP_Notify *notify = 0;
	NMHDR *nm = 0;
	static int _cnt = 30;
	const int _id_list = 1100;	// 用于处理 WM_COMMAND
	const int _id_list_dc = 1101;

	switch (message)
	{
	case WM_CREATE:
		/** 创建一个 listBox，启动一个1秒的 Timer，从 mcu 获取当前的 sources 列表
		 */
		_list = CreateWindow(_T("LISTBOX"), _T(""), WS_BORDER | WS_CHILD | WS_VISIBLE | LBS_SORT | LBS_NOTIFY, 0, 0, 0, 0, hWnd, (HMENU)_id_list, hInst, 0);
		_list_dc = CreateWindow(_T("LISTBOX"), "", WS_BORDER | WS_CHILD | WS_VISIBLE | LBS_SORT | LBS_NOTIFY, 0, 0, 0, 0, hWnd, (HMENU)_id_list_dc, hInst, 0);
		_log = CreateWindow(_T("EDIT"), _T(""), WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_AUTOHSCROLL | ES_MULTILINE | ES_READONLY, 0, 0, 0, 0, hWnd, 0, hInst, 0);
		SetTimer(hWnd, _timerid, _interleaved, 0);
		_mainwnd = hWnd;
		return 0;

	case WM_TIMER:
		if (wParam == _timerid) {
			if (_connected) {
				if (!_stream_added) {
					_stream_added = true;
					add_audio_stream(hWnd);
				}

				// 获取 sources 列表，更新 _list
				cmd_get_sources(hWnd);
			}
			//log("to get source\r\n");

			//KillTimer(hWnd, _timerid);
		}
		return 0;

	case WM_XMPP_NOTIFY:
		// 此处收到了 xmpp response，可以直接进行处理了 :)
		notify = (XMPP_Notify*)lParam;

		if (!strcmp(notify->cmd, "test.fc.list_sources")) {
			// notify->result_options 中为 sources 列表，更新到 _list 中
			// 格式为： 
			//			<id>  <desc>\n
			//			<id>  <desc>\n
			//				....

			if (notify->result_options) {
				ListBox_ResetContent(_list);

				char *options = strdup(notify->result_options);
				char *item = strtok(options, "\n");
				while (item) {
					// item 必须为 <id> <desc> 类型
					int id;
					char desc[128];
					if (sscanf(item, "%d %127[^\n]", &id, desc) == 2) {
						ListBox_AddString(_list, item);
					}

					item = strtok(0, "\n");
				}

				free(options);
			}
		}
		else if (!strcmp(notify->cmd, "test.dc.list_streams")) {
			// 更新导播模式列表
			if (notify->result_options) {
				// 清空
				ListBox_ResetContent(_list_dc);

				char *options = strdup(notify->result_options);
				char *item = strtok(options, "\n");
				while (item) {
					char desc[128];
					int sid;
					if (sscanf(item, "%d %127[^\n]", &sid, desc) == 2) {
						ListBox_AddString(_list_dc, item);
					}

					item = strtok(0, "\n");
				}

				free(options);
			}
		}
		else if (!strcmp(notify->cmd, "test.dc.add_stream")) {
			log("WM_XMPP_NOTIFY: cmd=%s, result=%s, options=%s\r\n",
				notify->cmd, notify->result, notify->result_options);

			// 添加
			if (!strcmp("err", notify->result)) {
				MessageBox(hWnd, notify->result_options, "创建 Stream 失败", MB_OK);
				exit(-1);
			}

			if (strstr(notify->cmd_options, "110")) {
				// 启动双向语音....
				add_audio_stream_done(notify->result_options);
			}
			else if (strstr(notify->cmd_options, "100")) {
				// 启动双向视频 ...
				// TODO: ..
			}
		}

		return 0;

	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// 分析菜单选择:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;

		case _id_list:
			if (wmEvent == LBN_DBLCLK) {
				// 双击啊
				int index = SendMessage(_list, LB_GETCURSEL, 0, 0);
				char txt[128];
				SendMessage(_list, LB_GETTEXT, index, (LPARAM)txt);
				
				int id = -1;
				if (sscanf(txt, "%d ", &id) == 1) {
					log("get id=%d\r\n", id);

					// 启动新窗口，点播 id 对应的源
					if (strstr(txt, "speex"))
						aod(hWnd, id, txt);
					else 
						vod(hWnd, id, txt);
				}
			}
			break;

			/*
		case _id_list_dc:
			if (wmEvent == LBN_DBLCLK) {
				// 导播模式双击
				int index = SendMessage(_list_dc, LB_GETCURSEL, 0, 0);
				char txt[128];
				SendMessage(_list_dc, LB_GETTEXT, index, (LPARAM)txt);

				int id = -1;
				if (sscanf(txt, "%d ", &id) == 1) {
					log("DIRECTOR: get id=%d\r\n", id);

					if (strstr(txt, "speex")) {
						// aod(hWnd, id, txt);
					}
					else {
						vod(hWnd, id, txt);
					}
				}
			}
			break;
			*/

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: 在此添加任意绘图代码...
		EndPaint(hWnd, &ps);
		break;

	case WM_SIZE:
		if (IsWindow(_list)) {
			RECT rect;
			GetClientRect(hWnd, &rect);
			int height = rect.bottom - rect.top;
			MoveWindow(_list, 0, 0, (rect.right - rect.left)/2, height / 3, TRUE);
			MoveWindow(_list_dc, (rect.right-rect.left)/2, 0, (rect.right-rect.left)/2, height / 3, TRUE);
			MoveWindow(_log, 0, height / 3, rect.right - rect.left, height / 3 * 2, TRUE);	// log 占用 2/3 的空间
		}
		return 0;

	case WM_DESTROY:
		if (hWnd == _mainwnd) {
			PostQuitMessage(0);
		}
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
