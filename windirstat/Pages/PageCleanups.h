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
#include "PageShared.h"

//
// CPageCleanups. "Settings" property page "Cleanups".
//
class CPageCleanups final : public MessageTarget<CPageCleanups, CSettingsPage>
{
public:
    enum : std::uint8_t { IDD = IDD_PAGE_CLEANUPS };

    CPageCleanups();
    ~CPageCleanups() override = default;

protected:
    void InitializePage() override;
    void OnOK() override;

    void CurrentUdcToDialog();
    void DialogToCurrentUdc();
    void OnSomethingChanged();
    void UpdateControlStatus();
    void CheckEmptyTitle();

    std::unique_ptr<USERDEFINEDCLEANUP[]> m_udc = std::make_unique<USERDEFINEDCLEANUP[]>(USERDEFINEDCLEANUPCOUNT);
    int m_current = -1; // currently selected user defined cleanup

    // Dialog data
    CListBox m_customCleanupList;
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

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnLbnSelchangeList();
    void OnBnClickedEnabled();
    void OnEnChangeTitle();
    void OnBnClickedWorksfordrives();
    void OnBnClickedWorksfordirectories();
    void OnBnClickedModified();
    void OnBnClickedRecurseintosubdirectories();
    void OnBnClickedUp();
    void OnBnClickedDown();
    void OnBnClickedHelpbutton();
};

inline std::span<const RouteEntry> CPageCleanups::Routes()
{
    using ThisClass = CPageCleanups;
    static constexpr std::array entries
    {
        Route::Control<&ThisClass::OnLbnSelchangeList>(LBN_SELCHANGE, IDC_LIST),
        Route::Control<&ThisClass::OnBnClickedEnabled>(BN_CLICKED, IDC_ENABLED),
        Route::Control<&ThisClass::OnEnChangeTitle>(EN_CHANGE, IDC_TITLE),
        Route::Control<&ThisClass::OnBnClickedWorksfordrives>(BN_CLICKED, IDC_WORKSFORDRIVES),
        Route::Control<&ThisClass::OnBnClickedWorksfordirectories>(BN_CLICKED, IDC_WORKSFORDIRECTORIES),
        Route::Control<&ThisClass::OnBnClickedModified>(BN_CLICKED, IDC_WORKSFORFILES),
        Route::Control<&ThisClass::OnBnClickedModified>(BN_CLICKED, IDC_WORKSFORUNCPATHS),
        Route::Control<&ThisClass::OnBnClickedModified>(EN_CHANGE, IDC_COMMANDLINE),
        Route::Control<&ThisClass::OnBnClickedRecurseintosubdirectories>(BN_CLICKED, IDC_RECURSEINTOSUBDIRECTORIES),
        Route::Control<&ThisClass::OnBnClickedModified>(BN_CLICKED, IDC_ASKFORCONFIRMATION),
        Route::Control<&ThisClass::OnBnClickedModified>(BN_CLICKED, IDC_SHOWCONSOLEWINDOW),
        Route::Control<&ThisClass::OnBnClickedModified>(BN_CLICKED, IDC_WAITFORCOMPLETION),
        Route::Control<&ThisClass::OnBnClickedModified>(CBN_SELENDOK, IDC_REFRESHPOLICY),
        Route::Control<&ThisClass::OnBnClickedUp>(BN_CLICKED, IDC_UP),
        Route::Control<&ThisClass::OnBnClickedDown>(BN_CLICKED, IDC_DOWN),
        Route::Control<&ThisClass::OnBnClickedHelpbutton>(BN_CLICKED, IDC_HELPBUTTON),
    };
    return entries;
}
