// CheckForUpdateDlg.h
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
// Last modified: $Date$

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
