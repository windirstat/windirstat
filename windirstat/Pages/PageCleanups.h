// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

#include "pch.h"

//
// CPageCleanups. "Settings" property page "Cleanups".
//
class CPageCleanups final : public CMFCPropertyPage
{
    DECLARE_DYNAMIC(CPageCleanups)

    enum : std::uint8_t { IDD = IDD_PAGE_CLEANUPS };

    CPageCleanups();
    ~CPageCleanups() override = default;

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    void CurrentUdcToDialog();
    void DialogToCurrentUdc();
    void OnSomethingChanged();
    void UpdateControlStatus();
    void CheckEmptyTitle();

    std::unique_ptr<USERDEFINEDCLEANUP[]> m_udc = std::make_unique<USERDEFINEDCLEANUP[]>(USERDEFINEDCLEANUPCOUNT);
    int m_current = -1; // currently selected user defined cleanup

    // Dialog data
    CListBox m_driveList;
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
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
