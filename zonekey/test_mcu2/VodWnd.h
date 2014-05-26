#pragma once
#include <zonekey/video-inf.h>
#include <zonekey/xmpp_uac.h>
#include <mediastreamer2/mediastream.h>
#include <mediastreamer2/zk.yuv_sink.h>
#include <string>

// VodWnd 对话框

class VodWnd : public CDialog
{
	DECLARE_DYNAMIC(VodWnd)

public:
	VodWnd(int sid, const char *desc, const char *mcu_jid, uac_token_t *uac, CWnd* pParent = NULL);   // 标准构造函数
	virtual ~VodWnd();

// 对话框数据
	enum { IDD = IDD_VODWND };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

private:
	int sid_;
	std::string desc_, mcu_jid_;
	uac_token_t *uac_;
	std::string server_ip_;
	int server_rtp_port_, server_rtcp_port_;
	int sinkid_;

	static void cb_response(zk_xmpp_uac_msg_token *rsp_token, const char *result, const char *option);
	static void cb_yuv(void *ctx, int width, int height, unsigned char *data[4], int stride[4]);
	void vod(const char *ip, int rtp_port, int rtcp_port);
	void stop_vod();
	void draw_yuv(int width, int height, unsigned char *data[4], int stride[4]);
	void active_rtp_sock();
	void active_rtcp_sock();
	void process_rtcp();

	MSTicker *ticker_;
	MSFilter *filter_rtp_, *filter_decoder_, *filter_sink_;
	RtpSession *rtp_;
	OrtpEvQueue *evq_;
	VideoCtx render_;
	bool first_rtp_, first_rtcp_;

protected:
	afx_msg LRESULT OnXmppNotify(WPARAM wParam, LPARAM lParam);

public:
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	virtual BOOL OnInitDialog();
	afx_msg void OnClose();
};
