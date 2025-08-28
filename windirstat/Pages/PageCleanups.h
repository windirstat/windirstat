// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
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

#include "WinDirStat.h"

//
// CPageCleanups. "Settings" property page "Cleanups".
//
class CPageCleanups final : public CPropertyPageEx
{
    DECLARE_DYNAMIC(CPageCleanups)

    enum : std::uint8_t { IDD = IDD_PAGE_CLEANUPS };

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

    std::unique_ptr<USERDEFINEDCLEANUP[]> m_Udc = std::make_unique<USERDEFINEDCLEANUP[]>(USERDEFINEDCLEANUPCOUNT);
    int m_Current = -1; // currently selected user defined cleanup

    // Dialog data
    CListBox m_List;
    BOOL m_Enabled = FALSE;
    CStringW m_Title;
    BOOL m_WorksForDrives = FALSE;
    BOOL m_WorksForDirectories = FALSE;
    BOOL m_WorksForFiles = FALSE;
    BOOL m_WorksForUncPaths = FALSE;
    CStringW m_CommandLine;
    BOOL m_RecurseIntoSubdirectories = FALSE;
    BOOL m_AskForConfirmation = FALSE;
    BOOL m_ShowConsoleWindow = FALSE;
    BOOL m_WaitForCompletion = FALSE;
    int m_RefreshPolicy = 0;
    CComboBox m_CtlRefreshPolicy;

    CEdit m_CtlTitle;
    CButton m_CtlWorksForDrives;
    CButton m_CtlWorksForDirectories;
    CButton m_CtlWorksForFiles;
    CButton m_CtlWorksForUncPaths;
    CEdit m_CtlCommandLine;
    CButton m_CtlRecurseIntoSubdirectories;
    CButton m_CtlAskForConfirmation;
    CButton m_CtlShowConsoleWindow;
    CButton m_CtlWaitForCompletion;
    CStatic m_CtlHintSp;
    CStatic m_CtlHintSn;
    CButton m_CtlUp;
    CButton m_CtlDown;

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
