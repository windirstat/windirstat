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

#include "pch.h"
#include "Filtering.h"
#include "PageFiltering.h"

CPageFiltering::CPageFiltering(const bool refreshOnFilteringChange) :
    MessageTarget(IDD),
    m_refreshOnFilteringChange(refreshOnFilteringChange)
{
}

void CPageFiltering::InitializePage()
{
    m_ctlFilteringSizeUnits.SubclassDlgItem(IDC_FILTERING_MIN_UNITS, this);
    m_ctlFilteringSizeComparison.SubclassDlgItem(IDC_FILTERING_SIZE_COMPARISON, this);
    m_ctlFilteringMaxAgeComparison.SubclassDlgItem(IDC_FILTERING_MAX_AGE_COMPARISON, this);
    m_ctrlFilteringExcludeFiles.SubclassDlgItem(IDC_FILTERING_EXCLUDE_FILES, this);
    m_ctrlFilteringExcludeDirs.SubclassDlgItem(IDC_FILTERING_EXCLUDE_DIRS, this);
    m_ctrlFilteringIncludeFiles.SubclassDlgItem(IDC_FILTERING_INCLUDE_FILES, this);
    m_ctrlFilteringIncludeDirs.SubclassDlgItem(IDC_FILTERING_INCLUDE_DIRS, this);

    m_ctlFilteringSizeUnits.AddString(GetSpec_Bytes());
    m_ctlFilteringSizeUnits.AddString(GetSpec_KiB());
    m_ctlFilteringSizeUnits.AddString(GetSpec_MiB());
    m_ctlFilteringSizeUnits.AddString(GetSpec_GiB());
    m_ctlFilteringSizeUnits.AddString(GetSpec_TiB());
    m_ctlFilteringSizeComparison.AddString(L"<");
    m_ctlFilteringSizeComparison.AddString(L">");
    m_ctlFilteringMaxAgeComparison.AddString(L"<");
    m_ctlFilteringMaxAgeComparison.AddString(L">");

    SetText(IDC_FILTERING_SIZE_MIN, std::to_wstring(COptions::FilteringSizeMinimum));
    SetComboSelection(IDC_FILTERING_MIN_UNITS, COptions::FilteringSizeUnits);
    SetComboSelection(IDC_FILTERING_SIZE_COMPARISON, COptions::FilteringSizeComparison);
    SetChecked(IDC_FILTERING_USE_REGEX, COptions::FilteringUseRegex);
    SetText(IDC_FILTERING_MAX_AGE_DAYS, std::to_wstring(COptions::FilteringMaxAgeDays));
    SetComboSelection(IDC_FILTERING_MAX_AGE_COMPARISON, COptions::FilteringMaxAgeComparison);
    LoadBinds(FilterBindings);

    // Initialize the tooltip control
    m_toolTip.Create(this);
    SetToolTips();
    m_toolTip.SetMaxTipWidth(ScaleForDpi(200));
    m_toolTip.Activate();
}

void CPageFiltering::AdjustControls()
{
    // Apply dark mode to this property page AFTER controls are initialized
    if (DarkMode::IsDarkModeActive())
    {
        CSettingsPage::AdjustControls();
        DarkMode::AdjustControls(m_ctlFilteringSizeComparison.Handle());
        DarkMode::AdjustControls(m_ctlFilteringMaxAgeComparison.Handle());
        DarkMode::AdjustControls(m_ctrlFilteringExcludeDirs.Handle());
        DarkMode::AdjustControls(m_ctrlFilteringExcludeFiles.Handle());
        DarkMode::AdjustControls(m_ctrlFilteringIncludeDirs.Handle());
        DarkMode::AdjustControls(m_ctrlFilteringIncludeFiles.Handle());
        m_ctrlFilteringExcludeDirs.Invalidate();
        m_ctrlFilteringExcludeFiles.Invalidate();
        m_ctrlFilteringIncludeDirs.Invalidate();
        m_ctrlFilteringIncludeFiles.Invalidate();
    }
}

void CPageFiltering::SetToolTips()
{
    const auto setToolTip = [this](const WindowRef control, const std::wstring& text)
    {
        if (IsInitialized()) m_toolTip.UpdateTipText(control, text);
        else m_toolTip.AddTool(control, text);
    };
    const bool useRegex = IsChecked(IDC_FILTERING_USE_REGEX);
    const std::wstring tip = Localization::Lookup(IDS_PAGE_FILTERING_TOOLTIP_PREFIX) + L"\n\n";
    const std::wstring dirs = tip + Localization::LookupNeutral(useRegex
        ? IDS_FILTER_EXAMPLE_DIRS_REGEX : IDS_FILTER_EXAMPLE_DIRS);
    const std::wstring files = tip + Localization::LookupNeutral(useRegex
        ? IDS_FILTER_EXAMPLE_FILES_REGEX : IDS_FILTER_EXAMPLE_FILES);
    setToolTip(&m_ctrlFilteringExcludeDirs, dirs);
    setToolTip(&m_ctrlFilteringExcludeFiles, files);
    setToolTip(&m_ctrlFilteringIncludeDirs, dirs);
    setToolTip(&m_ctrlFilteringIncludeFiles, files);
}

std::optional<CPropertyPage::ValidationError> CPageFiltering::PrepareSettings()
{
    m_filtersChanged = false;
    int filteringSizeMinimum = 0, filteringSizeUnits = 0, filteringSizeComparison = 0;
    int filteringMaxAgeDays = 0, filteringMaxAgeComparison = 0;
    if (auto error = ReadSelection(IDC_FILTERING_MIN_UNITS, filteringSizeUnits, 0, 4)) return error;
    if (auto error = ReadSelection(IDC_FILTERING_SIZE_COMPARISON, filteringSizeComparison, 0, 1)) return error;
    if (auto error = ReadSelection(IDC_FILTERING_MAX_AGE_COMPARISON, filteringMaxAgeComparison, 0, 1)) return error;
    const int maximumSize = static_cast<int>(std::min<ULONGLONG>(INT_MAX,
        ULLONG_MAX >> (10 * filteringSizeUnits)));
    const int maximumAge = static_cast<int>(std::bit_cast<ULONGLONG>(CurrentSystemFileTime()) / 864'000'000'000ULL);
    if (auto error = ReadInteger(IDC_FILTERING_SIZE_MIN, filteringSizeMinimum, 0, maximumSize)) return error;
    if (auto error = ReadInteger(IDC_FILTERING_MAX_AGE_DAYS, filteringMaxAgeDays, 0, maximumAge)) return error;
    const bool filteringUseRegex = IsChecked(IDC_FILTERING_USE_REGEX);
    const auto patterns = ReadBinds(FilterBindings).value();

    bool patternsChanged = false;
    for (const auto& pattern : patterns)
    {
        const bool pathFilter = &pattern.setting == &COptions::FilteringExcludeDirs ||
            &pattern.setting == &COptions::FilteringIncludeDirs;
        if (auto invalid = CFiltering::ValidateFilters(pattern.value, filteringUseRegex, pathFilter))
            return ValidationError{ ::GetDlgItem(Handle(), pattern.setting.Bind().controlId),
                Localization::Lookup(IDS_PAGE_FILTERING_INVALID_FILTER) + L" " + *invalid };
        patternsChanged |= pattern.Changed();
    }

    const bool refreshAll = COptions::FilteringSizeMinimum != filteringSizeMinimum ||
        COptions::FilteringSizeUnits != filteringSizeUnits ||
        COptions::FilteringUseRegex != filteringUseRegex ||
        COptions::FilteringMaxAgeDays != filteringMaxAgeDays ||
        COptions::FilteringSizeComparison != filteringSizeComparison ||
        COptions::FilteringMaxAgeComparison != filteringMaxAgeComparison || patternsChanged;

    if (!refreshAll) return {};
    m_filtersChanged = true;

    Stage(COptions::FilteringSizeMinimum, filteringSizeMinimum);
    Stage(COptions::FilteringSizeUnits, filteringSizeUnits);
    Stage(COptions::FilteringSizeComparison, filteringSizeComparison);
    Stage(COptions::FilteringUseRegex, filteringUseRegex);
    Stage(COptions::FilteringMaxAgeDays, filteringMaxAgeDays);
    Stage(COptions::FilteringMaxAgeComparison, filteringMaxAgeComparison);
    StageBinds(patterns);
    StageEffect([refresh = m_refreshOnFilteringChange]
    {
        CFiltering::CompileFilters();

        if (refresh)
        {
            CWinDirStatModel::Get()->StartScan(
                CWinDirStatModel::Get()->GetScanPathSpec());
        }
    });
    return {};
}

void CPageFiltering::BeforeApply()
{
    if (m_filtersChanged) CWinDirStatModel::Get()->StopScanningEngine();
}

void CPageFiltering::OnSettingChanged()
{
    if (!IsInitialized())
        return;

    SetModified();
    SetToolTips();
}

bool CPageFiltering::PreprocessMessage(MSG* pMsg)
{
    m_toolTip.RelayEvent(pMsg);

    return CSettingsPage::PreprocessMessage(pMsg);
}
