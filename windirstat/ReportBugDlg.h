// reportbugdlg.h	- Declaratino of CReportBugDlg
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2004 Bernhard Seifert
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
// Last modified: $Date$

#pragma once

#include "layout.h"


class CReportBugDlg : public CDialog
{
	DECLARE_DYNAMIC(CReportBugDlg)
	enum { IDD = IDD_REPORTBUG };

public:
	CReportBugDlg(CWnd* pParent = NULL);
	virtual ~CReportBugDlg();

	// [out]
	CString m_recipient;
	CString m_subject;
	CString m_body;

protected:
	virtual BOOL OnInitDialog();
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual void OnOK();
	void OnSeverityClick();
	CString GetSeverityString();

	CString m_from;
	CString m_to;
	CString m_application;
	CString m_platform;
	CString m_hint;
	int m_severity;
	CString m_inAWord;
	CString m_text;

	CLayout m_layout;

	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedCritical();
	afx_msg void OnBnClickedGrave();
	afx_msg void OnBnClickedNormal();
	afx_msg void OnBnClickedWish();
	afx_msg void OnBnClickedFeedback();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	afx_msg void OnDestroy();
};

// $Log$
// Revision 1.4  2004/11/05 16:53:06  assarbad
// Added Date and History tag where appropriate.
//
