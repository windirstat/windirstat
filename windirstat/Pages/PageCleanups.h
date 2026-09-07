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
    bool HasCurrentUdc() const noexcept { return m_current >= 0 && std::cmp_less(m_current, m_udc.size()); }
    void MoveCurrentUdc(int offset);

    std::vector<USERDEFINEDCLEANUP> m_udc;
    int m_current = -1; // currently selected user defined cleanup
    bool m_updating = false;

    // Dialog data
    CListBox m_customCleanupList;
    CComboBox m_ctlRefreshPolicy;
    CEdit m_ctlTitle;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnLbnSelchangeList();
    void OnBnClickedEnabled();
    void OnEnChangeTitle();
    void OnBnClickedAdd();
    void OnBnClickedRemove();
    void OnBnClickedUp();
    void OnBnClickedDown();
    void OnBnClickedHelpbutton();
};

inline std::span<const RouteEntry> CPageCleanups::Routes()
{
    static constexpr std::array entries
    {
        Route::Control<&OnLbnSelchangeList>(LBN_SELCHANGE, IDC_LIST),
        Route::Control<&OnBnClickedEnabled>(BN_CLICKED, IDC_ENABLED),
        Route::Control<&OnEnChangeTitle>(EN_CHANGE, IDC_TITLE),
        Route::Control<&OnSomethingChanged>(BN_CLICKED, IDC_WORKSFORDRIVES),
        Route::Control<&OnSomethingChanged>(BN_CLICKED, IDC_WORKSFORDIRECTORIES),
        Route::Control<&OnSomethingChanged>(BN_CLICKED, IDC_WORKSFORFILES),
        Route::Control<&OnSomethingChanged>(BN_CLICKED, IDC_WORKSFORUNCPATHS),
        Route::Control<&OnSomethingChanged>(EN_CHANGE, IDC_COMMANDLINE),
        Route::Control<&OnSomethingChanged>(BN_CLICKED, IDC_RECURSEINTOSUBDIRECTORIES),
        Route::Control<&OnSomethingChanged>(BN_CLICKED, IDC_ASKFORCONFIRMATION),
        Route::Control<&OnSomethingChanged>(BN_CLICKED, IDC_SHOWCONSOLEWINDOW),
        Route::Control<&OnSomethingChanged>(BN_CLICKED, IDC_WAITFORCOMPLETION),
        Route::Control<&OnSomethingChanged>(CBN_SELENDOK, IDC_REFRESHPOLICY),
        Route::Control<&OnBnClickedAdd>(BN_CLICKED, IDC_ADD_CLEANUP),
        Route::Control<&OnBnClickedRemove>(BN_CLICKED, IDC_REMOVE_CLEANUP),
        Route::Control<&OnBnClickedUp>(BN_CLICKED, IDC_UP),
        Route::Control<&OnBnClickedDown>(BN_CLICKED, IDC_DOWN),
        Route::Control<&OnBnClickedHelpbutton>(BN_CLICKED, IDC_HELPBUTTON),
    };
    return entries;
}
