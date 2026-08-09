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
    void OnOK() override;

    CComboBox m_levelCombo[PERMSRULECOUNT];
    CColorButton m_colorButton[PERMSRULECOUNT];

public:
    static std::span<const RouteEntry> Routes();

};

inline std::span<const RouteEntry> CPagePermissions::Routes()
{
    using ThisClass = CPagePermissions;
    static constexpr std::array entries
    {
        Route::Notify<&ThisClass::OnSettingNotifyChanged>(COLBN_CHANGED, IDC_COLORBUTTON0, IDC_COLORBUTTON4),
        Route::Control<&ThisClass::OnSettingRangeChanged>(EN_CHANGE, IDC_PERMS_ACCOUNT0, IDC_PERMS_ACCOUNT4),
        Route::Control<&ThisClass::OnSettingRangeChanged>(CBN_SELCHANGE, IDC_PERMS_LEVEL0, IDC_PERMS_LEVEL4),
        Route::Control<&ThisClass::OnSettingChanged>(EN_CHANGE, IDC_PERMS_EXCLUDE),
    };
    return entries;
}
