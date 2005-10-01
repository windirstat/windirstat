#pragma once
#include "afxwin.h"
#include "../common/version.h"
#include "StaticHyperlink.h"

void StartUpdateDialog();

class CUpdateThread: public CWinThread
{
	DECLARE_DYNCREATE(CUpdateThread);
protected:
	virtual BOOL InitInstance();
};

// CCheckForUpdateDlg dialog

class CCheckForUpdateDlg : public CDialog
{
	DECLARE_DYNAMIC(CCheckForUpdateDlg)

public:
	CCheckForUpdateDlg(CWnd* pParent = NULL);   // standard constructor
	virtual ~CCheckForUpdateDlg();

// Dialog Data
	enum { IDD = IDD_CHECKFORUPDATE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CStatic DescriptiveText;
	CButton CloseButton;
	CStaticHyperlink UpdateUrl;
	afx_msg void OnBnClickedOk();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	void UpdateFromUrl(CString server, CString uri, INTERNET_PORT port);
};
