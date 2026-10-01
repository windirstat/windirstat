// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

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
    std::optional<ValidationError> PrepareSettings() override;
    void BeforeApply() override;
    void SetToolTips();

    static constexpr std::array FilterBindings
    {
        &COptions::FilteringExcludeFiles,
        &COptions::FilteringExcludeDirs,
        &COptions::FilteringIncludeFiles,
        &COptions::FilteringIncludeDirs,
    };

    CComboBox m_ctlFilteringSizeUnits;
    CComboBox m_ctlFilteringSizeComparison;
    CComboBox m_ctlFilteringMaxAgeComparison;
    CEdit m_ctrlFilteringExcludeFiles;
    CEdit m_ctrlFilteringExcludeDirs;
    CEdit m_ctrlFilteringIncludeFiles;
    CEdit m_ctrlFilteringIncludeDirs;
    CToolTipCtrl m_toolTip;
    bool m_refreshOnFilteringChange = true;
    bool m_filtersChanged = false;

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
        Route::Control<&OnSettingChanged>(EN_CHANGE, IDC_FILTERING_EXCLUDE_DIRS, IDC_FILTERING_INCLUDE_FILES),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_FILTERING_USE_REGEX),
        Route::Control<&OnSettingChanged>(EN_CHANGE, IDC_FILTERING_SIZE_MIN),
        Route::Control<&OnSettingChanged>(EN_CHANGE, IDC_FILTERING_MAX_AGE_DAYS, IDC_FILTERING_MIN_UNITS),
        Route::Control<&OnSettingChanged>(CBN_SELENDOK, IDC_FILTERING_MIN_UNITS, IDC_FILTERING_SIZE_COMPARISON),
        Route::Control<&OnSettingChanged>(CBN_SELENDOK, IDC_FILTERING_MAX_AGE_COMPARISON),
    };
    return entries;
}
