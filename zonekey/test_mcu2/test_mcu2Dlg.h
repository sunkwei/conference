
// test_mcu2Dlg.h : 头文件
//

#pragma once

#include <string>
#include <zonekey/video-inf.h>
#include "CameraStream.h"

struct XmppData
{
	std::string cmd, cmd_options;
	std::string result, result_options;
};

// Ctest_mcu2Dlg 对话框
class Ctest_mcu2Dlg : public CDialogEx
{
// 构造
public:
	Ctest_mcu2Dlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_TEST_MCU2_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnTimer(UINT_PTR nIDEvent);

private:
	uac_token_t *uac_;
	CEvent evt_;
	bool connected_;
	static void cb_connect(uac_token_t *const token, int is_connected, void *userdata);
	static void cb_response(zk_xmpp_uac_msg_token *rsp_token, const char *result, const char *option);
	void uac_get_stream_list_begin();
	void uac_get_stream_list_end(XmppData *data);
	void uac_get_source_list_begin();
	void uac_get_source_list_end(XmppData *data);

	void re_xmpp_wait(uac_token_t *uac);
	
	// vod for dc video
	MSTicker *ticker_;
	MSFilter *filter_rtp_, *filter_decoder_, *filter_sink_;
	RtpSession *rtp_;
	OrtpEvQueue *evq_;
	VideoCtx render_;
	bool first_rtp_, first_rtcp_;
	int sinkid_;
	std::string server_ip_;
	int server_rtp_port_, server_rtcp_port_;

	void vod_init();
	void vod();
	void vod_stop();
	static void cb_yuv(void *c, int w, int h, unsigned char *d[4], int l[4]);
	void cb_yuv(int w, int h, unsigned char *d[4], int l[4]);

	// audio stream
	AudioStream *audio_stream_;	// 
	int audio_stream_id_;
	std::string audio_stream_ip_;
	int audio_stream_rtp_port_, audio_stream_rtcp_port_;
	MSSndCard *snd_capt_, *snd_play_;

	void del_audio_stream();

	void add_stream_start();
	void add_stream_done(XmppData *data);

	// webcam stream
	CameraStream cam_;
	int cam_id_;
	std::string cam_server_ip_;
	int cam_server_rtp_port_, cam_server_rtcp_port_;


	std::string xmpp_domain_;

protected:
	afx_msg LRESULT OnXmppNotify(WPARAM wParam, LPARAM lParam);
public:
	CListBox m_list_streams;
	CListBox m_list_sources;
	afx_msg void OnDblclkListSources();
	CStatic m_render_wnd;
	afx_msg void OnClose();
	CStatic m_lab_running;
//	afx_msg void OnBnClickedButton1();
};
