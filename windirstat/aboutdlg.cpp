// aboutdlg.cpp		- Implementation of the StartAboutDialog() function
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003 Bernhard Seifert
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Author: bseifert@users.sourceforge.net, bseifert@daccord.net

#include "stdafx.h"
#include "windirstat.h"
#include ".\aboutdlg.h"

namespace
{
	enum
	{
		RE_CONTROL = 4711	// Id of the RichEdit Control
	};

	// Tabs
	enum
	{
		TAB_ABOUT,
		TAB_THANKSTO,
		TAB_LICENSE
	};

	// Retrieve the GPL text from our resources
	CString GetTextResource(UINT id)
	{
		CString s;

		HGLOBAL hresource = NULL;
		try
		{
			HRSRC hrsrc= FindResource(NULL, MAKEINTRESOURCE(id), _T("TEXT"));
			if (hrsrc == NULL)
				MdThrowLastWinerror();

			DWORD dwSize= SizeofResource(AfxGetInstanceHandle(), hrsrc);
			if (dwSize == 0)
				MdThrowLastWinerror();

			hresource= LoadResource(NULL, hrsrc);
			const BYTE *pData= (const BYTE *)LockResource(hresource);

			CComBSTR bstr(dwSize, (LPCSTR)pData);

			s= bstr;
		}
		catch (CException *pe)
		{
			pe->ReportError();
			pe->Delete();
		}

		if (hresource != NULL)
			FreeResource(hresource);

		return s;
	}
}

/////////////////////////////////////////////////////////////////////////////

void StartAboutDialog()
{
	AfxBeginThread(RUNTIME_CLASS(CAboutThread), NULL);
}


/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNCREATE(CAboutThread, CWinThread);

BOOL CAboutThread::InitInstance()
{
	CWinThread::InitInstance();

	CAboutDlg dlg;
	dlg.DoModal();
	return false;
}

/////////////////////////////////////////////////////////////////////////////

void CAboutDlg::CMyTabControl::Initialize()
{
	ModifyStyle(0, WS_CLIPCHILDREN);

	InsertItem(TAB_ABOUT, LoadString(IDS_ABOUT_ABOUT));
	InsertItem(TAB_THANKSTO, LoadString(IDS_ABOUT_THANKSTO));
	InsertItem(TAB_LICENSE, LoadString(IDS_ABOUT_LICENSEAGREEMENT));

	CRect rc;
	GetClientRect(rc);

	CRect rcItem;
	GetItemRect(0, rcItem);

	rc.top= rcItem.bottom;

	VERIFY(m_text.CreateEx(0, WS_CHILD|WS_VISIBLE|WS_BORDER | ES_CENTER|ES_MULTILINE|ES_READONLY, rc, this, RE_CONTROL));
	SetPageText(TAB_ABOUT);
}

void CAboutDlg::CMyTabControl::SetPageText(int tab)
{
	CString text;
	DWORD newStyle= ES_CENTER;

	switch (tab)
	{
	case TAB_ABOUT:
		text.FormatMessage(IDS_ABOUT_ABOUTTEXTss, GetAuthorEmail(), GetWinDirStatHomepage());
		break;
	case TAB_THANKSTO:
		text.LoadString(IDS_ABOUT_THANKSTOTEXT);
		break;
	case TAB_LICENSE:
		text= GetTextResource(IDR_LICENSE);
		newStyle= ES_LEFT;
		break;
	default:
		ASSERT(0);
	}
	CRect rc;
	m_text.GetWindowRect(rc);
	ScreenToClient(rc);

	DWORD style= m_text.GetStyle();
	style&= ~ES_CENTER;
	style|= newStyle;
	style|= WS_VSCROLL;

	DWORD exstyle= m_text.GetExStyle();

	m_text.DestroyWindow();
	m_text.CreateEx(exstyle, style, rc, this, RE_CONTROL);

	m_text.SetAutoURLDetect();
	m_text.SetEventMask(ENM_LINK);
	m_text.SetFont(GetFont());

	m_text.SetWindowText(text);
	m_text.HideCaret();
}

BEGIN_MESSAGE_MAP(CAboutDlg::CMyTabControl, CTabCtrl)
	ON_NOTIFY(EN_LINK, RE_CONTROL, OnEnLinkText)
	ON_WM_SIZE()
END_MESSAGE_MAP()

void CAboutDlg::CMyTabControl::OnEnLinkText(NMHDR *pNMHDR, LRESULT *pResult)
{
	ENLINK *el= reinterpret_cast<ENLINK *>(pNMHDR);
	*pResult = 0;

	if (el->msg == WM_LBUTTONDOWN)
	{
		CString link;
		m_text.GetTextRange(el->chrg.cpMin, el->chrg.cpMax, link);

		ShellExecute(*this, NULL, link, NULL, _T(""), SW_SHOWNORMAL);
	}
}

void CAboutDlg::CMyTabControl::OnSize(UINT nType, int cx, int cy)
{
	CTabCtrl::OnSize(nType, cx, cy);

	if (IsWindow(m_text.m_hWnd))
	{
		CRect rc;
		GetClientRect(rc);

		CRect rcItem;
		GetItemRect(0, rcItem);

		rc.top= rcItem.bottom;

		m_text.MoveWindow(rc);
	}
}


////////////////////////////////////////////////////////////////////////////

CAboutDlg::CAboutDlg() 
	: CDialog(CAboutDlg::IDD)
	, m_layout(this, _T("aboutdlg"))
{
}

CString CAboutDlg::GetAppVersion()
{
	CString s;
	s.Format(_T("WinDirStat devel"));
	return s;
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CAPTION, m_caption);
	DDX_Control(pDX, IDC_TAB, m_tab);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB, OnTcnSelchangeTab)
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CAboutDlg::OnInitDialog()
{
	CDialog::OnInitDialog();
	
	m_layout.AddControl(IDC_CAPTION,		0.5, 0, 0, 0);
	m_layout.AddControl(IDC_TAB,			0,   0, 1, 1);
	m_layout.AddControl(IDOK,				0.5, 1, 0, 0);

	m_layout.OnInitDialog(true);

	m_tab.Initialize();
	m_caption.SetWindowText(GetAppVersion());

	return true;
}

void CAboutDlg::OnTcnSelchangeTab(NMHDR * /* pNMHDR */, LRESULT *pResult)
{
	*pResult = 0;
	m_tab.SetPageText(m_tab.GetCurSel());
}

void CAboutDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);
	m_layout.OnSize();
}


void CAboutDlg::OnGetMinMaxInfo(MINMAXINFO* mmi)
{
	m_layout.OnGetMinMaxInfo(mmi);
	CDialog::OnGetMinMaxInfo(mmi);

}

void CAboutDlg::OnDestroy()
{
	m_layout.OnDestroy();
	CDialog::OnDestroy();
}
