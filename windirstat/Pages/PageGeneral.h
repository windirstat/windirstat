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
// CPageGeneral. "Settings" property page "General".
//
class CPageGeneral final : public MessageTarget<CPageGeneral, CSettingsPage>
{
public:
    enum : std::uint8_t { IDD = IDD_PAGE_GENERAL };

    CPageGeneral();
    ~CPageGeneral() override = default;

protected:
    void InitializePage() override;
    std::optional<ValidationError> PrepareSettings() override;

    static constexpr std::array CheckboxBindings
    {
        &COptions::AutoElevate,
        &COptions::AutomaticallyResizeColumns,
        &COptions::ListFullRowSelection,
        &COptions::ListGrid,
        &COptions::ListStripes,
        &COptions::UseSizeSuffixes,
        &COptions::UseWindowsLocaleSetting,
        &COptions::ShowTimeSeconds,
    };

    CComboBox m_combo;

    // Helper methods for context menu registry operations
    static bool IsContextMenuRegistered(HKEY root);
    static bool SetContextMenuRegistration(bool enable);

public:
    static std::span<const RouteEntry> Routes();

};

inline std::span<const RouteEntry> CPageGeneral::Routes()
{
    static constexpr std::array entries
    {
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_AUTO_ELEVATE),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_COLUMN_AUTOSIZE),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_CONTEXT_MENU),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_FULL_ROW_SELECTION),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_PORTABLE_MODE),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_SHOW_GRID),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_SHOW_STRIPES, IDC_SHOW_TIME_SECONDS),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_SIZE_SUFFIXES),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_USE_WINDOWS_LOCALE),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_DARK_MODE_DISABLED, IDC_DARK_MODE_USE_WINDOWS),
        Route::Control<&OnSettingChanged>(CBN_SELENDOK, IDC_COMBO),
    };
    return entries;
}
