// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "PageShared.h"
#include "ColorButton.h"

//
// CPagePermissions. "Settings" property page "Permissions".
//
class CPagePermissions final : public MessageTarget<CPagePermissions, CSettingsPage>
{
public:
    enum : std::uint8_t { IDD = IDD_PAGE_PERMISSIONS };

    CPagePermissions();
    ~CPagePermissions() override = default;

protected:
    void InitializePage() override;
    std::optional<ValidationError> PrepareSettings() override;

    CComboBox m_levelCombo[PERMSRULECOUNT];
    CColorButton m_colorButton[PERMSRULECOUNT];

public:
    static std::span<const RouteEntry> Routes();

};

inline std::span<const RouteEntry> CPagePermissions::Routes()
{
    static constexpr std::array entries
    {
        Route::Notify<&OnSettingChanged>(COLBN_CHANGED, IDC_COLORBUTTON0, IDC_COLORBUTTON4),
        Route::Control<&OnSettingChanged>(EN_CHANGE, IDC_PERMS_ACCOUNT0, IDC_PERMS_ACCOUNT4),
        Route::Control<&OnSettingChanged>(CBN_SELCHANGE, IDC_PERMS_LEVEL0, IDC_PERMS_LEVEL4),
        Route::Control<&OnSettingChanged>(EN_CHANGE, IDC_PERMS_EXCLUDE),
    };
    return entries;
}
