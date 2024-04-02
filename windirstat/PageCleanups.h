// PageCleanups.h - Declaration of CPageCleanups
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
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

#pragma once
#include "afxwin.h"

//
// CPageCleanups. "Settings" property page "User defined cleanups".
//
class CPageCleanups final : public CPropertyPage
{
    DECLARE_DYNAMIC(CPageCleanups)

    enum { IDD = IDD_PAGE_CLEANUPS };

    CPageCleanups();
    ~CPageCleanups() override;

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    void CurrentUdcToDialog();
    void DialogToCurrentUdc();
    void OnSomethingChanged();
    void UpdateControlStatus();
    void CheckEmptyTitle();

    USERDEFINEDCLEANUP m_udc[USERDEFINEDCLEANUPCOUNT]{}; // FIXME: should probably go to the heap
    int m_current = -1; // currently selected user defined cleanup

    // Dialog data
    CListBox m_list;
    BOOL m_enabled = FALSE;
    CStringW m_title;
    BOOL m_worksForDrives = FALSE;
    BOOL m_worksForDirectories = FALSE;
    BOOL m_worksForFiles = FALSE;
    BOOL m_worksForUncPaths = FALSE;
    CStringW m_commandLine;
    BOOL m_recurseIntoSubdirectories = FALSE;
    BOOL m_askForConfirmation = FALSE;
    BOOL m_showConsoleWindow = FALSE;
    BOOL m_waitForCompletion = FALSE;
    int m_refreshPolicy = 0;
    CComboBox m_ctlRefreshPolicy;

    CEdit m_ctlTitle;
    CButton m_ctlWorksForDrives;
    CButton m_ctlWorksForDirectories;
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
    afx_msg void OnBnClickedModified();
    afx_msg void OnBnClickedRecurseintosubdirectories();
    afx_msg void OnBnClickedUp();
    afx_msg void OnBnClickedDown();
    afx_msg void OnBnClickedHelpbutton();
};
