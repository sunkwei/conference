
// test_mcu2Dlg.cpp : 实现文件
//

#include "stdafx.h"
#include "test_mcu2.h"
#include "test_mcu2Dlg.h"
#include "afxdialogex.h"
#include <opencv/highgui.h>
#include "VodWnd.h"
#include "util.h"

extern "C" {
#	include <x264.h>
#	include <libavcodec/avcodec.h>
#	include <libswscale/swscale.h>
}

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

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
		ss << "mse_s_000000000000_mcu_1" << "@" << domain;
		_jid = ss.str();
	}

	return _jid.c_str();
}

// Ctest_mcu2Dlg 对话框
void Ctest_mcu2Dlg::vod_init()
{
	rtp_ = 0;
	ticker_ = 0;
	filter_sink_ = filter_rtp_ = filter_decoder_ = 0;
	render_ = 0;
	evq_ = 0;
	first_rtp_ = first_rtcp_ = true;

	char opt[128];
	snprintf(opt, sizeof(opt), "sid=-1");
	zk_xmpp_uac_send_cmd(uac_, get_mcu_jid(), "test.dc.add_sink", opt, this, cb_response);
}

void Ctest_mcu2Dlg::vod()
{
	rtp_ = rtp_session_new(RTP_SESSION_RECVONLY);
	rtp_session_set_payload_type(rtp_, 100);	// h264
	rtp_session_set_local_addr(rtp_, util_get_myip(), 0, 0);
	rtp_session_set_remote_addr_and_port(rtp_, server_ip_.c_str(), server_rtp_port_, server_rtcp_port_);
	JBParameters jb;
	jb.adaptive = 1;
	jb.max_packets = 500;
	jb.max_size = -1;
	jb.min_size = jb.nom_size = 200;
	rtp_session_set_jitter_buffer_params(rtp_, &jb);
	evq_ = ortp_ev_queue_new();
	rtp_session_register_event_queue(rtp_, evq_);
	char host[64];
	strcpy(host, "SINK-");
	gethostname(host+5, sizeof(host)-5);
	rtp_session_set_source_description(rtp_, host, getenv("USERNAME"), 0, 0, 0, 0, 0);
	
	ticker_ = ms_ticker_new();

	filter_rtp_ = ms_filter_new(MS_RTP_RECV_ID);
	ms_filter_call_method(filter_rtp_, MS_RTP_RECV_SET_SESSION, rtp_);

	filter_decoder_ = ms_filter_new(MS_H264_DEC_ID);

	filter_sink_ = ms_filter_new_from_name("ZonekeyYUVSink");
	ZonekeyYUVSinkCallbackParam yp;
	yp.ctx = this;
	yp.push = cb_yuv;
	ms_filter_call_method(filter_sink_, ZONEKEY_METHOD_YUV_SINK_SET_CALLBACK_PARAM, &yp);

	ms_filter_link(filter_rtp_, 0, filter_decoder_, 0);
	ms_filter_link(filter_decoder_, 0, filter_sink_, 0);

	ms_ticker_attach(ticker_, filter_rtp_);
}

void Ctest_mcu2Dlg::vod_stop()
{
	if (ticker_) {
		char opt[128];
		snprintf(opt, sizeof(opt), "sinkid=%d", sinkid_);
		zk_xmpp_uac_send_cmd(uac_, get_mcu_jid(), "test.dc.del_sink", opt, 0, 0);

		ms_ticker_detach(ticker_, filter_rtp_);

		ms_filter_destroy(filter_rtp_); filter_rtp_ = 0;
		ms_filter_destroy(filter_decoder_); filter_decoder_ = 0;
		ms_filter_destroy(filter_sink_); filter_sink_ = 0;

		rtp_session_destroy(rtp_); rtp_ = 0;
		ortp_ev_queue_destroy(evq_); evq_ = 0;

		ms_ticker_destroy(ticker_); ticker_ = 0;

		if (render_) {
			rv_close(render_);
			render_ = 0;
		}

		Sleep(1000);	// FIXME: 多等一会，让 test.dc.del_sink 发送出去
	}
}

void Ctest_mcu2Dlg::cb_yuv(void *c, int w, int h, unsigned char *d[4], int l[4])
{
	((Ctest_mcu2Dlg*)c)->cb_yuv(w, h, d, l);
}

void Ctest_mcu2Dlg::cb_yuv(int w, int h, unsigned char *d[4], int l[4])
{
	/** WARNING: 不在窗口线程中调用！！！
	 */
	if (!render_) {
		HWND wnd = m_render_wnd.m_hWnd;
		rv_open(&render_, wnd, w, h);
		first_rtp_ = false;
	}

	if (render_) {
		rv_writepic(render_, PIX_FMT_YUV420P, d, l);
	}
}

Ctest_mcu2Dlg::Ctest_mcu2Dlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(Ctest_mcu2Dlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	audio_stream_ = 0;
	cam_id_ = -1;
}

void Ctest_mcu2Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_SOURCES, m_list_sources);
	DDX_Control(pDX, IDC_LIST_STREAMS, m_list_streams);
	DDX_Control(pDX, IDC_VIDEO, m_render_wnd);
	DDX_Control(pDX, IDC_STATIC_RUNNING, m_lab_running);
}

#define WM_XMPP_Notify (WM_USER + 1001)

BEGIN_MESSAGE_MAP(Ctest_mcu2Dlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_MESSAGE(WM_XMPP_Notify, &Ctest_mcu2Dlg::OnXmppNotify)
	ON_LBN_DBLCLK(IDC_LIST_SOURCES, &Ctest_mcu2Dlg::OnDblclkListSources)
	ON_WM_CLOSE()
//	ON_BN_CLICKED(IDC_BUTTON1, &Ctest_mcu2Dlg::OnBnClickedButton1)
END_MESSAGE_MAP()

#include "LogDialog.h"

// Ctest_mcu2Dlg 消息处理程序

BOOL Ctest_mcu2Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	ShowWindow(SW_SHOW);

	// 登录 xmpp
	const char *domain = "app.zonekey.com.cn";
	char *p = getenv("xmpp_domain");
	if (p) domain = p;

	xmpp_domain_ = domain;

	// 使用 normaluser 登录
	char jid[128];
	snprintf(jid, sizeof(jid), "normaluser@%s", domain);

	cb_xmpp_uac cbs = { 0, 0, 0, 0, cb_connect };
	uac_ = zk_xmpp_uac_log_in(jid, "ddkk1212", &cbs, this);
	::WaitForSingleObject(evt_.m_hObject, -1);

	if (!connected_) {
		MessageBox("无法连接到 xmpp server，请检查 xmpp_domain 环境变量的设置", "ERROR");
		::exit(-1);
	}

	static LogDialog *_logdlg = 0;

	// TODO: 在此添加控件通知处理程序代码
	// FIXME: 这里一次发送了两个 wait_event，呵呵

	re_xmpp_wait(uac_);
	re_xmpp_wait(uac_);

	if (cam_.open() < 0) {
		cam_id_ = -1;
	}
	else {
		if (::MessageBox(m_hWnd, "是否将你的摄像头作为会议的一个视频源？", "提示", MB_OKCANCEL) == IDOK) {
			char opt[256], host[64];
			gethostname(host, sizeof(host));
			snprintf(opt, sizeof(opt), "payload=100&desc=h264 camera from %s:%s, ip=%s", host, getenv("USERNAME"), util_get_myip());
			zk_xmpp_uac_send_cmd(uac_, get_mcu_jid(), "test.dc.add_stream", opt, this, cb_response);
		}
		else {
			cam_.close();
			cam_id_ = -1;
		}
	}

	if (!_logdlg) {
		_logdlg = new LogDialog;
		_logdlg->Create(IDD_LOGDIALOG);
	}

	vod_init();
	add_stream_start();	// 启动音频 stream

	SetWindowText(jid);

	SetTimer(1001, 5000, 0);	// 用于更新 source/stream 列表
	SetTimer(1002, 300, 0);		// 用于检查 rtcp
	SetTimer(1003, (uint32_t)(1000 / cam_.fps()), 0);		// 发送 x264 stream，来自摄像头

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void Ctest_mcu2Dlg::re_xmpp_wait(uac_token_t *uac)
{
	// 发送一个 wait_event 命令，等待通知
	assert(uac);
	zk_xmpp_uac_send_cmd(uac, get_mcu_jid(), "test.dc.wait_event", 0, this, cb_response);
}

void Ctest_mcu2Dlg::cb_connect(uac_token_t *const token, int is_connected, void *userdata)
{
	Ctest_mcu2Dlg *This = (Ctest_mcu2Dlg*)userdata;
	if (is_connected)
		This->connected_ = true;
	else
		This->connected_ = false;
	This->evt_.SetEvent();
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void Ctest_mcu2Dlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR Ctest_mcu2Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

static void send_something(int fd, const char *ip, int port)
{
	sockaddr_in to;
	to.sin_family = AF_INET;
	to.sin_port = htons(port);
	to.sin_addr.s_addr = inet_addr(ip);

	sendto(fd, "abcd", 4, 0, (sockaddr*)&to, sizeof(to));
}

void Ctest_mcu2Dlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == 1001) {
		// 刷新 streams/sources
		uac_get_source_list_begin();
		uac_get_stream_list_begin();
		zk_xmpp_uac_send_cmd(uac_, get_mcu_jid(), "sys_info", 0, this, cb_response);
		return;
	}
	else if (nIDEvent == 1002) {
		if (rtp_ && evq_) {
			if (first_rtp_) {
				send_something(rtp_session_get_rtp_socket(rtp_), server_ip_.c_str(), server_rtp_port_);
			}
			if (first_rtcp_) {
				send_something(rtp_session_get_rtcp_socket(rtp_), server_ip_.c_str(), server_rtcp_port_);
			}

			OrtpEvent *ev = ortp_ev_queue_get(evq_);
			while (ev) {
				OrtpEventType type = ortp_event_get_type(ev);
				if (type == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
					first_rtcp_ = false;
				}

				// TODO:
				if (type == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
					/*
					mblk_t *m = ortp_event_get_data(ev)->packet;
					if (rtcp_is_SR(m)) {
						_log("[RTCP] SR: mixer video session: \r\n");
						const rtcp_common_header *header = rtcp_get_common_header(m);
						for (int i = 0;i < header->rc; i++) {
							const report_block_t *rb = rtcp_SR_get_report_block(m, i);
							assert(rb);
							float rt = rtp_session_get_round_trip_propagation(rtp_);
							unsigned int jitter = report_block_get_interarrival_jitter(rb);
							float lost = 100.0 * report_block_get_fraction_lost(rb)/256.0;

							_log("\t\tssrc=%u: rtt=%f, jitter=%u, lost=%f\r\n",
									rb->ssrc, rt, jitter, lost);
						}

					}
					
					if (rtcp_is_RR(m)) {
					}
					*/

					const rtp_stats_t *stat = rtp_session_get_stats(rtp_);
					_log("[video mixer stat]: \r\n"
						"\trecv bytes: %I64d\r\n"
						"\trecv packets: %I64d\r\n"
						"\ttot lost: %I64d\r\n"
						"\ttot discarded: %I64d\r\n\r\n",
						stat->recv, stat->packet_recv, stat->cum_packet_loss, stat->discarded);
				}
				else if (type == ORTP_EVENT_RTCP_PACKET_EMITTED) {
					_log("[local] Jitter buffer size: %f ms\r\n",rtp_session_get_jitter_stats(rtp_)->jitter_buffer_size_ms);
				}

				ortp_event_destroy(ev);
				ev = ortp_ev_queue_get(evq_);
			}
		}
		return;
	}
	else if (nIDEvent == 1003) {
		static char *_chs[] = { "-", "\\", "|", "/" };
		static int _nch = 0;
		m_lab_running.SetWindowTextA(_chs[(++_nch) % 4]);
		if (cam_id_ >= 0)
			cam_.run_once();
		return;
	}

	CDialogEx::OnTimer(nIDEvent);
}

void Ctest_mcu2Dlg::uac_get_source_list_begin()
{
	zk_xmpp_uac_send_cmd(uac_, get_mcu_jid(), "test.fc.list_sources", 0, this, cb_response);
}

void Ctest_mcu2Dlg::uac_get_stream_list_begin()
{
	zk_xmpp_uac_send_cmd(uac_, get_mcu_jid(), "test.dc.list_streams", 0, this, cb_response);
}

void Ctest_mcu2Dlg::uac_get_source_list_end(XmppData *data)
{
	if (!strcmp("ok", data->result.c_str())) {
		/* 列表为： 使用 \n 分割，每行为 <id> <desc> 。。
		 */
		m_list_sources.ResetContent();
		char *options = strdup(data->result_options.c_str());
		char *item = strtok(options, "\n");
		while (item) {
			char desc[128];
			int id;
			if (sscanf(options, "%d %127[^\n]", &id, desc) == 2) {
				m_list_sources.AddString(item);
			}

			item = strtok(0, "\n");
		}
		free(options);
	}
}

void Ctest_mcu2Dlg::uac_get_stream_list_end(XmppData *data)
{
	if (!strcmp("ok", data->result.c_str())) {
		m_list_streams.ResetContent();
		char *options = strdup(data->result_options.c_str());
		char *item = strtok(options, "\n");
		while (item) {
			char desc[128];
			int id;
			if (sscanf(options, "%d %127[^\n]", &id, desc) == 2) {
				m_list_streams.AddString(item);
			}

			item = strtok(0, "\n");
		}
		free(options);
	}
}

void Ctest_mcu2Dlg::cb_response(zk_xmpp_uac_msg_token *rsp_token, const char *result, const char *option)
{
	// 必须放到窗口线程中进行
	Ctest_mcu2Dlg *This = (Ctest_mcu2Dlg*)rsp_token->userdata;
	XmppData *data = new XmppData;
	data->cmd = rsp_token->cmd;
	if (rsp_token->option)
		data->cmd_options = rsp_token->option;
	data->result = result;
	if (option)
		data->result_options = option;
	::PostMessage(This->m_hWnd, WM_XMPP_Notify, 0, (LPARAM)data);
}

afx_msg LRESULT Ctest_mcu2Dlg::OnXmppNotify(WPARAM wParam, LPARAM lParam)
{
	XmppData *data = (XmppData*)lParam;
	if (!strcmp(data->cmd.c_str(), "test.fc.list_sources")) {
		uac_get_source_list_end(data);
	}
	if (!strcmp(data->cmd.c_str(), "test.dc.list_streams")) {
		uac_get_stream_list_end(data);
	}
	if (!strcmp(data->cmd.c_str(), "sys_info")) {
		// 更新服务器 cpu, mem, send/recv
		if (!strcmp("ok", data->result.c_str())) {
			KVS opt = util_parse_options(data->result_options.c_str());
			std::stringstream ss;
			ss << xmpp_domain_;
			ss << ": cpu=" << opt["cpu"] << ", mem=" << opt["mem"] << ", rcv=" << opt["net_recv"] << ", snd=" << opt["net_send"];
			std::string t = ss.str();
			SetWindowText(t.c_str());
		}
	}
	if (!strcmp(data->cmd.c_str(), "test.dc.add_sink")) {
		if (!strcmp("ok", data->result.c_str())) {
			KVS param = util_parse_options(data->result_options.c_str());
			char info[128];
			assert(chk_params(param, info, "sinkid", "server_ip", "server_rtcp_port", "server_rtp_port", 0));
			sinkid_ = atoi(param["sinkid"].c_str());
			server_ip_ = param["server_ip"];
			server_rtp_port_ = atoi(param["server_rtp_port"].c_str());
			server_rtcp_port_ = atoi(param["server_rtcp_port"].c_str());

			vod();
		}
	}
	if (!strcmp(data->cmd.c_str(), "test.dc.add_stream")) {
		add_stream_done(data);
	}
	if (!strcmp(data->cmd.c_str(), "test.dc.wait_event")) {
		// 继续
		re_xmpp_wait(uac_);

		/** 收到 mcu 上发生 addSource/addStream/delSource/delStream 事件
		 */
		// 开始调用 test.dc.get_params(v.complete_state) 
		char info[1024];
		KVS opts = util_parse_options(data->result_options.c_str());
		assert(chk_params(opts, info, "who", "cmd", "cmd_options", "result", "result_options", 0));

		std::string who = opts["who"];
		std::string cmd = opts["cmd"];
		std::string cmd_options = opts["cmd_options"];
		std::string result = opts["result"];
		std::string result_options = opts["result_options"];

		// 仅仅 h264 才获取视频属性
		if (result == "ok" && strstr(cmd_options.c_str(), "payload=100")) {
			zk_xmpp_uac_send_cmd(uac_, get_mcu_jid(), "test.dc.get_params", "name=v.complete_state", this, cb_response);
		}
	}

	if (!strcmp(data->cmd.c_str(), "test.dc.get_params")) {
		if (!data->cmd_options.empty() && strstr(data->cmd_options.c_str(), "v.complete_state")) {
			// 收到完整 streams state
			if (data->result == "ok") {
				KVS result = util_parse_options(data->result_options.c_str());
				std::string state = result["state"];
				char *str = strdup(state.c_str());
				char *p = strtok(str, "\n");
				while (p) {
					int sid, cid, x, y, width, height, alpha;
					if (7 == sscanf(p, "%d %d %d %d %d %d %d", &sid, &cid, &x, &y, &width, &height, &alpha)) {
					}

					p = strtok(0, "\n");
				}
				free(str);
			}
		}
	}

	delete data;

	return 0;
}

void Ctest_mcu2Dlg::OnDblclkListSources()
{
	// 点播这一路
	int id = m_list_sources.GetCurSel();
	CString item;
	m_list_sources.GetText(id, item);
	const char *p = (LPCTSTR)item;
	char desc[128];
	if (sscanf(p, "%d %127[^\n]", &id, desc) == 2) {
		if (strstr(desc, "h264")) {
			VodWnd *vod = new VodWnd(id, desc, get_mcu_jid(), uac_);
			vod->Create(IDD_VODWND, 0);
			vod->ShowWindow(SW_SHOW);
		}
	}
}

void Ctest_mcu2Dlg::OnClose()
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	del_audio_stream();
	cam_.close();
	if (cam_id_ >= 0) {
		char opt[128];
		sprintf(opt, "streamid=%d", cam_id_);
		zk_xmpp_uac_send_cmd(uac_, get_mcu_jid(), "test.dc.del_stream", opt, 0, 0);
	}
	vod_stop();

	CDialogEx::OnClose();
}

void Ctest_mcu2Dlg::add_stream_start()
{
	char options[256];
	char host[64];
	gethostname(host, sizeof(host));
	snprintf(options, sizeof(options), "payload=110&desc=speex from %s:%s", host, getenv("USERNAME"));

	if (uac_) {
		zk_xmpp_uac_send_cmd(uac_, get_mcu_jid(), "test.dc.add_stream", options, this, cb_response);
	}
}

void Ctest_mcu2Dlg::del_audio_stream()
{
	if (audio_stream_) {
		if (uac_) {
			char options[128];
			snprintf(options, sizeof(options), "streamid=%d", audio_stream_id_);
			zk_xmpp_uac_send_cmd(uac_, get_mcu_jid(), "test.dc.del_stream", options, 0, 0);
		}
		audio_stream_stop(audio_stream_);
		audio_stream_ = 0;
	}
}

void Ctest_mcu2Dlg::add_stream_done(XmppData *data)
{
	KVS param = util_parse_options(data->result_options.c_str());
	char info[128];

	if (strcmp("ok", data->result.c_str())) {
		MessageBox("创建 stream 失败!");
	}
	else {
		if (strstr(data->cmd_options.c_str(), "speex")) {
			// 启动音频 stream
			assert(chk_params(param, info, "streamid", "server_ip", "server_rtp_port", "server_rtcp_port", 0));
			audio_stream_id_ = atoi(param["streamid"].c_str());
			audio_stream_ip_ = param["server_ip"].c_str();
			audio_stream_rtp_port_ = atoi(param["server_rtp_port"].c_str());
			audio_stream_rtcp_port_ = atoi(param["server_rtcp_port"].c_str());

			MSSndCardManager *manager=ms_snd_card_manager_get();
			snd_capt_ = ms_snd_card_manager_get_default_capture_card(manager);
			snd_play_ =  ms_snd_card_manager_get_default_playback_card(manager);

			if (!snd_capt_ || !snd_play_) {
				MessageBox("没有找到合适的声卡");
				::exit(-1);
			}

			audio_stream_ = audio_stream_new(0, 0, false);
			audio_stream_set_echo_canceller_params(audio_stream_, 150, 0, 0);
			audio_stream_enable_automatic_gain_control(audio_stream_, 1);
			audio_stream_enable_noise_gate(audio_stream_, 1);
			audio_stream_start_full(audio_stream_, &av_profile, audio_stream_ip_.c_str(), audio_stream_rtp_port_, 
				audio_stream_ip_.c_str(), audio_stream_rtcp_port_, 110, 120, 0, 0, snd_play_, snd_capt_, true);

		}
		else {
			// 启动视频 stream
			// 启动 webcam 
			assert(chk_params(param, info, "streamid", "server_ip", "server_rtp_port", "server_rtcp_port", 0));
			cam_id_ = atoi(param["streamid"].c_str());
			cam_server_ip_ = param["server_ip"];
			cam_server_rtp_port_ = atoi(param["server_rtp_port"].c_str());
			cam_server_rtcp_port_ = atoi(param["server_rtcp_port"].c_str());
			cam_.set_server_info(cam_server_ip_.c_str(), cam_server_rtp_port_, cam_server_rtcp_port_);
		}
	}
}
