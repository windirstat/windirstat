// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

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
    std::optional<ValidationError> PrepareSettings() override;

    void CurrentUdcToDialog();
    void DialogToCurrentUdc();
    void OnSomethingChanged();
    void UpdateControlStatus();
    void CheckEmptyTitle();
    bool HasCurrentUdc() const noexcept { return m_current >= 0 && std::cmp_less(m_current, m_udc.size()); }
    void MoveCurrentUdc(int offset);

    static constexpr std::array CheckboxBindings
    {
        &USERDEFINEDCLEANUP::AskForConfirmation,
        &USERDEFINEDCLEANUP::Enabled,
        &USERDEFINEDCLEANUP::RecurseIntoSubdirectories,
        &USERDEFINEDCLEANUP::ShowConsoleWindow,
        &USERDEFINEDCLEANUP::WaitForCompletion,
        &USERDEFINEDCLEANUP::WorksForDirectories,
        &USERDEFINEDCLEANUP::WorksForDrives,
        &USERDEFINEDCLEANUP::WorksForFiles,
        &USERDEFINEDCLEANUP::WorksForUncPaths,
    };

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
        Route::Control<&OnSomethingChanged>(BN_CLICKED, IDC_WAITFORCOMPLETION, IDC_WORKSFORUNCPATHS),
        Route::Control<&OnSomethingChanged>(EN_CHANGE, IDC_COMMANDLINE),
        Route::Control<&OnSomethingChanged>(BN_CLICKED, IDC_RECURSEINTOSUBDIRECTORIES),
        Route::Control<&OnSomethingChanged>(BN_CLICKED, IDC_ASKFORCONFIRMATION),
        Route::Control<&OnSomethingChanged>(BN_CLICKED, IDC_SHOWCONSOLEWINDOW),
        Route::Control<&OnSomethingChanged>(CBN_SELENDOK, IDC_REFRESHPOLICY),
        Route::Control<&OnBnClickedAdd>(BN_CLICKED, IDC_ADD_CLEANUP),
        Route::Control<&OnBnClickedRemove>(BN_CLICKED, IDC_REMOVE_CLEANUP),
        Route::Control<&OnBnClickedUp>(BN_CLICKED, IDC_UP),
        Route::Control<&OnBnClickedDown>(BN_CLICKED, IDC_DOWN),
        Route::Control<&OnBnClickedHelpbutton>(BN_CLICKED, IDC_HELPBUTTON),
    };
    return entries;
}
