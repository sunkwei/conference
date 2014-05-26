#pragma once


// LogDialog 对话框

class LogDialog : public CDialog
{
	DECLARE_DYNAMIC(LogDialog)

public:
	LogDialog(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~LogDialog();

// 对话框数据
	enum { IDD = IDD_LOGDIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CRichEditCtrl m_log;

protected:
	afx_msg LRESULT OnLog(WPARAM wParam, LPARAM lParam);
public:
	virtual BOOL OnInitDialog();
};

void _log(const char *fmt, ...);
