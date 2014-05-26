// VodWnd.cpp : 实现文件
//

#include "stdafx.h"
#include "test_mcu2.h"
#include "VodWnd.h"
#include "afxdialogex.h"
#include "util.h"

#define WM_XMPP_Notify (WM_USER+1002)
// VodWnd 对话框

IMPLEMENT_DYNAMIC(VodWnd, CDialog)

VodWnd::VodWnd(int sid, const char *desc, const char *mcu_jid, uac_token_t *uac, CWnd* pParent /*=NULL*/)
	: CDialog(VodWnd::IDD, pParent)
{
	sid_ = sid;
	desc_ = desc;
	uac_ = uac;
	mcu_jid_ = mcu_jid;

	ticker_ = 0;
	filter_rtp_ = filter_decoder_ = filter_sink_ = 0;
	rtp_ = 0;
	evq_ = 0;
	first_rtp_ = first_rtcp_ = true;
	render_ = 0;
}

VodWnd::~VodWnd()
{
	// FIXME: 需要保证发出的 xmpp cmd 收到 response 才行 ...
	
}

namespace {
struct XmppData
{
	std::string cmd, cmd_options;
	std::string result, result_options;
};
}

void VodWnd::cb_response(zk_xmpp_uac_msg_token *rsp_token, const char *result, const char *option)
{
	VodWnd *This = (VodWnd*)rsp_token->userdata;
	XmppData *data = new XmppData;
	data->cmd = rsp_token->cmd;
	if (rsp_token->option) data->cmd_options = rsp_token->option;
	data->result = result;
	data->result_options = option;

	::PostMessage(This->m_hWnd, WM_XMPP_Notify, 0, (LPARAM)data);
}

void VodWnd::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(VodWnd, CDialog)
	ON_MESSAGE(WM_XMPP_Notify, &VodWnd::OnXmppNotify)
	ON_WM_TIMER()
	ON_WM_CLOSE()
END_MESSAGE_MAP()

// VodWnd 消息处理程序
afx_msg LRESULT VodWnd::OnXmppNotify(WPARAM wParam, LPARAM lParam)
{
	XmppData *data = (XmppData*)lParam;
	if (!strcmp(data->cmd.c_str(), "test.fc.add_sink")) {
		if (!strcmp(data->result.c_str(), "ok")) {
			KVS param = util_parse_options(data->result_options.c_str());
			char info[128];
			assert(chk_params(param, info, "sinkid", "server_ip", "server_rtcp_port", "server_rtp_port", 0));
			
			sinkid_ = atoi(param["sinkid"].c_str());

			snprintf(info, sizeof(info), "VOD: id=%d, server_ip=%s, rtp_port=%d, rtcp_port=%d", sid_, param["server_ip"].c_str(),
				atoi(param["server_rtp_port"].c_str()), atoi(param["server_rtcp_port"].c_str()));
			SetWindowText(info);
			vod(param["server_ip"].c_str(), atoi(param["server_rtp_port"].c_str()), atoi(param["server_rtcp_port"].c_str()));
		}
	}
	delete data;
	return 0;
}

void VodWnd::stop_vod()
{
	if (ticker_) {
		char opt[128];
		snprintf(opt, sizeof(opt), "sinkid=%d", sinkid_);
		zk_xmpp_uac_send_cmd(uac_, mcu_jid_.c_str(), "test.fc.del_sink", opt, 0, 0);

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
	}
}

void VodWnd::vod(const char *ip, int rtp_port, int rtcp_port)
{
	server_ip_ = ip;
	server_rtp_port_ = rtp_port;
	server_rtcp_port_ = rtcp_port;

	rtp_ = rtp_session_new(RTP_SESSION_RECVONLY);
	rtp_session_set_payload_type(rtp_, 100);
	rtp_session_set_local_addr(rtp_, util_get_myip(), 0, 0);
	rtp_session_set_remote_addr_and_port(rtp_, ip, rtp_port, rtcp_port);

	JBParameters jb;
	jb.adaptive = 1;
	jb.max_packets = 3000;
	jb.max_size = -1;
	jb.min_size = jb.nom_size = 300;
	rtp_session_set_jitter_buffer_params(rtp_, &jb);

	rtp_session_enable_jitter_buffer(rtp_, 0);

	evq_ = ortp_ev_queue_new();
	rtp_session_register_event_queue(rtp_, evq_);

	ticker_ = ms_ticker_new();

	filter_rtp_ = ms_filter_new(MS_RTP_RECV_ID);
	ms_filter_call_method(filter_rtp_, MS_RTP_RECV_SET_SESSION, rtp_);

	filter_decoder_ = ms_filter_new(MS_H264_DEC_ID);

	ZonekeyYUVSinkCallbackParam cbp;
	cbp.ctx = this;
	cbp.push = cb_yuv;
	filter_sink_ = ms_filter_new_from_name("ZonekeyYUVSink");
	ms_filter_call_method(filter_sink_, ZONEKEY_METHOD_YUV_SINK_SET_CALLBACK_PARAM, &cbp);

	ms_filter_link(filter_rtp_, 0, filter_decoder_, 0);
	ms_filter_link(filter_decoder_, 0, filter_sink_, 0);

	ms_ticker_attach(ticker_, filter_rtp_);
}

void VodWnd::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	// 处理 rtcp 消息
	if (nIDEvent == 110) {
		if (first_rtp_) {
			active_rtp_sock();
		}

		if (first_rtcp_) {
			active_rtcp_sock();
		}

		process_rtcp();
	}

	CDialog::OnTimer(nIDEvent);
}

void VodWnd::cb_yuv(void *ctx, int width, int height, unsigned char *data[4], int stride[4])
{
	((VodWnd*)ctx)->draw_yuv(width, height, data, stride);
}

void VodWnd::draw_yuv(int width, int height, unsigned char *data[4], int stride[4])
{
	/** WARNING: 此函数在非窗口线程中调用！！！！
	 */

	if (!render_) {
		rv_open(&render_, m_hWnd, width, height);
		first_rtp_ = false;
	}

	if (render_) {
		rv_writepic(render_, PIX_FMT_YUV420P, data, stride);
	}
}

static void send_something(int fd, const char *ip, int port)
{
	sockaddr_in to;
	to.sin_family = AF_INET;
	to.sin_port = htons(port);
	to.sin_addr.s_addr = inet_addr(ip);

	sendto(fd, "abcd", 4, 0, (sockaddr*)&to, sizeof(to));
}

void VodWnd::active_rtcp_sock()
{
	if (!rtp_) return;

	int sock = rtp_session_get_rtcp_socket(rtp_);
	send_something(sock, server_ip_.c_str(), server_rtcp_port_);
}

void VodWnd::active_rtp_sock()
{
	if (!rtp_) return;

	int sock = rtp_session_get_rtp_socket(rtp_);
	send_something(sock, server_ip_.c_str(), server_rtp_port_);
}

void VodWnd::process_rtcp()
{
	if (evq_) {
		OrtpEvent *ev = ortp_ev_queue_get(evq_);
		while (ev) {
			// TODO: ..
			OrtpEventType type = ortp_event_get_type(ev);
			if (type == ORTP_EVENT_RTCP_PACKET_RECEIVED) {
				first_rtcp_ = false;
			}

			ortp_event_destroy(ev);
			ev = ortp_ev_queue_get(evq_);
		}
	}
}

BOOL VodWnd::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化
	SetTimer(110, 300, 0);
	// 发送点播请求
	char options[128];
	snprintf(options, sizeof(options), "sid=%d", sid_);
	zk_xmpp_uac_send_cmd(uac_, mcu_jid_.c_str(), "test.fc.add_sink", options, this, cb_response);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}


void VodWnd::OnClose()
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	stop_vod();
	CloseWindow();
	//CDialog::OnClose();
}
