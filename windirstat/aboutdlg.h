// aboutdlg.h - Declaration of StartAboutDialog(), CAboutThread and CAboutDlg
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2006 Oliver Schneider (assarbad.net)
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
//
// $Header$

#pragma once

#include "layout.h"
#include "../common/wds_constants.h"


void StartAboutDialog();


class CAboutThread: public CWinThread
{
	DECLARE_DYNCREATE(CAboutThread);
protected:
	virtual BOOL InitInstance();
};


class CAboutDlg : public CDialog
{
	enum { IDD = IDD_ABOUTBOX };

	class CMyTabControl: public CTabCtrl
	{
	public:
		void Initialize();
		void SetPageText(int tab);

	protected:
		CRichEditCtrl m_text;
	
		DECLARE_MESSAGE_MAP()
		afx_msg void OnEnLinkText(NMHDR *pNMHDR, LRESULT *pResult);
		afx_msg void OnEnMsgFilter(NMHDR *pNMHDR, LRESULT *pResult);
		afx_msg void OnSize(UINT nType, int cx, int cy);
	};

public:
	CAboutDlg();
	static CString GetAppVersion();

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);

	CStatic m_caption;
	CMyTabControl m_tab;
	CLayout m_layout;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnTcnSelchangeTab(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	afx_msg void OnDestroy();
};


// $Log$
// Revision 1.8  2006/07/04 22:49:20  assarbad
// - Replaced CVS keyword "Date" by "Header" in the file headers
//
// Revision 1.7  2006/07/04 20:45:22  assarbad
// - See changelog for the changes of todays previous check-ins as well as this one!
//
// Revision 1.6  2004/11/23 06:37:04  bseifert
// Fixed bug in AboutDlg: Esc-key made controls disappear.
//
// Revision 1.5  2004/11/13 08:17:07  bseifert
// Remove blanks in Unicode Configuration names.
//
// Revision 1.4  2004/11/12 22:14:16  bseifert
// Eliminated CLR_NONE. Minor corrections.
//
// Revision 1.3  2004/11/05 16:53:07  assarbad
// Added Date and History tag where appropriate.
//
