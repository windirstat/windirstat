// pagecleanups.h	- Declaration of CPageCleanups
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
#include "afxwin.h"

//
// CPageCleanups. "Settings" property page "User defined cleanups".
//
class CPageCleanups : public CPropertyPage
{
	DECLARE_DYNAMIC(CPageCleanups)
	enum { IDD = IDD_PAGE_CLEANUPS };

public:
	CPageCleanups();
	virtual ~CPageCleanups();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK();

	void CurrentUdcToDialog();
	void DialogToCurrentUdc();
	void OnSomethingChanged();
	void UpdateControlStatus();
	void CheckEmptyTitle();

	USERDEFINEDCLEANUP m_udc[USERDEFINEDCLEANUPCOUNT];
	int m_current;	// currently selected user defined cleanup

	// Dialog data
	CListBox m_list;
	BOOL m_enabled;
	CString m_title;
	BOOL m_worksForDrives;
	BOOL m_worksForDirectories;
	BOOL m_worksForFilesFolder;
	BOOL m_worksForFiles;
	BOOL m_worksForUncPaths;
	CString m_commandLine;
	BOOL m_recurseIntoSubdirectories;
	BOOL m_askForConfirmation;
	BOOL m_showConsoleWindow;
	BOOL m_waitForCompletion;
	int m_refreshPolicy;
	CComboBox m_ctlRefreshPolicy;

	CEdit m_ctlTitle;
	CButton m_ctlWorksForDrives;
	CButton m_ctlWorksForDirectories;
	CButton m_ctlWorksForFilesFolder;
	CButton m_ctlWorksForFiles;
	CButton m_ctlWorksForUncPaths;
	CEdit m_ctlCommandLine;
	CButton m_ctlRecurseIntoSubdirectories;
	CButton m_ctlAskForConfirmation;
	CButton m_ctlShowConsoleWindow;
	CButton m_ctlWaitForCompletion;
	CStatic m_ctlHintSp;
	CStatic m_ctlHintSn;
	CButton m_ctlUp;
	CButton m_ctlDown;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnLbnSelchangeList();
	afx_msg void OnBnClickedEnabled();
	afx_msg void OnEnChangeTitle();
	afx_msg void OnBnClickedWorksfordrives();
	afx_msg void OnBnClickedWorksfordirectories();
	afx_msg void OnBnClickedWorksforfilesfolder();
	afx_msg void OnBnClickedWorksforfiles();
	afx_msg void OnBnClickedWorksforuncpaths();
	afx_msg void OnEnChangeCommandline();
	afx_msg void OnBnClickedRecurseintosubdirectories();
	afx_msg void OnBnClickedAskforconfirmation();
	afx_msg void OnBnClickedShowconsolewindow();
	afx_msg void OnBnClickedWaitforcompletion();
	afx_msg void OnCbnSelendokRefreshpolicy();
	afx_msg void OnBnClickedUp();
	afx_msg void OnBnClickedDown();
	afx_msg void OnBnClickedHelpbutton();
};

// $Log$
// Revision 1.6  2004/11/12 22:14:16  bseifert
// Eliminated CLR_NONE. Minor corrections.
//
// Revision 1.5  2004/11/05 16:53:06  assarbad
// Added Date and History tag where appropriate.
//
