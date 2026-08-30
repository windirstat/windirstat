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
// CPageFiltering. "Settings" property page "Filtering".
//
class CPageFiltering final : public MessageTarget<CPageFiltering, CSettingsPage>
{
public:
    enum : std::uint8_t { IDD = IDD_PAGE_FILTERING };

    explicit CPageFiltering(bool refreshOnFilteringChange = true);
    ~CPageFiltering() override = default;

protected:
    void InitializePage() override;
    void AdjustControls() override;
    void OnOK() override;
    void SetToolTips();
    CComboBox m_ctlFilteringSizeUnits;
    CComboBox m_ctlFilteringSizeComparison;
    CComboBox m_ctlFilteringMaxAgeComparison;
    CEdit m_ctrlFilteringExcludeFiles;
    CEdit m_ctrlFilteringExcludeDirs;
    CEdit m_ctrlFilteringIncludeFiles;
    CEdit m_ctrlFilteringIncludeDirs;
    CToolTipCtrl m_toolTip;
    bool m_refreshOnFilteringChange = true;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnSettingChanged();
    bool PreprocessMessage(MSG* pMsg) override;
};

inline std::span<const RouteEntry> CPageFiltering::Routes()
{
    static constexpr std::array entries
    {
        Route::Control<&OnSettingChanged>(EN_CHANGE, IDC_FILTERING_EXCLUDE_DIRS),
        Route::Control<&OnSettingChanged>(EN_CHANGE, IDC_FILTERING_EXCLUDE_FILES),
        Route::Control<&OnSettingChanged>(EN_CHANGE, IDC_FILTERING_INCLUDE_DIRS),
        Route::Control<&OnSettingChanged>(EN_CHANGE, IDC_FILTERING_INCLUDE_FILES),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_FILTERING_USE_REGEX),
        Route::Control<&OnSettingChanged>(EN_CHANGE, IDC_FILTERING_SIZE_MIN),
        Route::Control<&OnSettingChanged>(EN_CHANGE, IDC_FILTERING_MIN_UNITS),
        Route::Control<&OnSettingChanged>(CBN_SELENDOK, IDC_FILTERING_MIN_UNITS),
        Route::Control<&OnSettingChanged>(CBN_SELENDOK, IDC_FILTERING_SIZE_COMPARISON),
        Route::Control<&OnSettingChanged>(EN_CHANGE, IDC_FILTERING_MAX_AGE_DAYS),
        Route::Control<&OnSettingChanged>(CBN_SELENDOK, IDC_FILTERING_MAX_AGE_COMPARISON),
    };
    return entries;
}
