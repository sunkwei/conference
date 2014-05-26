// LogDialog.cpp : 实现文件
//

#include "stdafx.h"
#include "test_mcu2.h"
#include "LogDialog.h"
#include "afxdialogex.h"

// LogDialog 对话框
static HWND _log_wnd = 0;

IMPLEMENT_DYNAMIC(LogDialog, CDialog)

LogDialog::LogDialog(CWnd* pParent /*=NULL*/)
	: CDialog(LogDialog::IDD, pParent)
{

}

LogDialog::~LogDialog()
{
}

void LogDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LOG, m_log);
}

#define WM_Log (WM_USER + 113)

BEGIN_MESSAGE_MAP(LogDialog, CDialog)
	ON_MESSAGE(WM_Log, &LogDialog::OnLog)
END_MESSAGE_MAP()


// LogDialog 消息处理程序


afx_msg LRESULT LogDialog::OnLog(WPARAM wParam, LPARAM lParam)
{
	std::string *info = (std::string*)lParam;

	// 显示到 m_log 中
	long len = m_log.GetTextLength();
	m_log.SetSel(len, len);
	m_log.ReplaceSel(info->c_str());

	return 0;
}

void _log(const char *fmt, ...)
{
	if (IsWindow(_log_wnd)) {
		va_list ap;
		char buf[1024];
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);

		std::string *info = new std::string(buf);

		PostMessage(_log_wnd, WM_Log, 0, (LPARAM)info);
	}
}

BOOL LogDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化
	_log_wnd = m_hWnd;

	ShowWindow(SW_SHOW);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}
