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
// CPagePrompts. "Settings" property page "Prompts".
//
class CPagePrompts final : public MessageTarget<CPagePrompts, CSettingsPage>
{
public:
    enum : std::uint8_t { IDD = IDD_PAGE_PROMPTS };

    CPagePrompts();
    ~CPagePrompts() override = default;

protected:
    void InitializePage() override;
    void OnOK() override;
    static std::span<const CheckboxSettingBinding> CheckboxSettings();

public:
    static std::span<const RouteEntry> Routes();

};

inline std::span<const RouteEntry> CPagePrompts::Routes()
{
    static constexpr std::array entries
    {
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_DELETION_WARNING),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_DELETION_BIN_WARNING),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_ELEVATION_PROMPT),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_CLOUD_LINKS_WARNING),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_SHOW_MICROSOFT_PROGRESS),
        Route::Control<&OnSettingRangeChanged>(BN_CLICKED, IDC_PROMPT_EMPTY_BIN, IDC_PROMPT_REMOVE_EMPTY),
    };
    return entries;
}
