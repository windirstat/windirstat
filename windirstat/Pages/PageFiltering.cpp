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
    m_ctrlFilteringExcludeFiles.SubclassDlgItem(IDC_FILTERING_EXCLUDE_FILES, this);
    m_ctrlFilteringExcludeDirs.SubclassDlgItem(IDC_FILTERING_EXCLUDE_DIRS, this);
    m_ctrlFilteringIncludeFiles.SubclassDlgItem(IDC_FILTERING_INCLUDE_FILES, this);
    m_ctrlFilteringIncludeDirs.SubclassDlgItem(IDC_FILTERING_INCLUDE_DIRS, this);

    m_ctlFilteringSizeUnits.AddString(GetSpec_Bytes().c_str());
    m_ctlFilteringSizeUnits.AddString(GetSpec_KiB().c_str());
    m_ctlFilteringSizeUnits.AddString(GetSpec_MiB().c_str());
    m_ctlFilteringSizeUnits.AddString(GetSpec_GiB().c_str());
    m_ctlFilteringSizeUnits.AddString(GetSpec_TiB().c_str());

    SetText(IDC_FILTERING_SIZE_MIN, std::to_wstring(COptions::FilteringSizeMinimum));
    SetComboSelection(IDC_FILTERING_MIN_UNITS, COptions::FilteringSizeUnits);
    SetChecked(IDC_FILTERING_USE_REGEX, COptions::FilteringUseRegex);
    SetText(IDC_FILTERING_MAX_AGE_DAYS, std::to_wstring(COptions::FilteringMaxAgeDays));
    SetText(IDC_FILTERING_EXCLUDE_DIRS, COptions::FilteringExcludeDirs.Obj());
    SetText(IDC_FILTERING_EXCLUDE_FILES, COptions::FilteringExcludeFiles.Obj());
    SetText(IDC_FILTERING_INCLUDE_DIRS, COptions::FilteringIncludeDirs.Obj());
    SetText(IDC_FILTERING_INCLUDE_FILES, COptions::FilteringIncludeFiles.Obj());

    // Initialize the tooltip control
    m_toolTip.Create(this);
    SetToolTips();
    m_toolTip.SetMaxTipWidth(200);
    m_toolTip.Activate();
}

void CPageFiltering::AdjustControls()
{
    // Apply dark mode to this property page AFTER controls are initialized
    if (DarkMode::IsDarkModeActive())
    {
        CSettingsPage::AdjustControls();
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
    const std::wstring tip = Localization::Lookup(IDS_PAGE_FILTERING_TOOLTIP_PREFIX) + L"\n\n";
    if (IsChecked(IDC_FILTERING_USE_REGEX))
    {
        m_toolTip.AddTool(&m_ctrlFilteringExcludeDirs, (tip + Localization::LookupNeutral(IDS_FILTER_EXAMPLE_DIRS_REGEX)).c_str());
        m_toolTip.AddTool(&m_ctrlFilteringExcludeFiles, (tip + Localization::LookupNeutral(IDS_FILTER_EXAMPLE_FILES_REGEX)).c_str());
        m_toolTip.AddTool(&m_ctrlFilteringIncludeDirs, (tip + Localization::LookupNeutral(IDS_FILTER_EXAMPLE_DIRS_REGEX)).c_str());
        m_toolTip.AddTool(&m_ctrlFilteringIncludeFiles, (tip + Localization::LookupNeutral(IDS_FILTER_EXAMPLE_FILES_REGEX)).c_str());
    }
    else
    {
        m_toolTip.AddTool(&m_ctrlFilteringExcludeDirs, (tip + Localization::LookupNeutral(IDS_FILTER_EXAMPLE_DIRS)).c_str());
        m_toolTip.AddTool(&m_ctrlFilteringExcludeFiles, (tip + Localization::LookupNeutral(IDS_FILTER_EXAMPLE_FILES)).c_str());
        m_toolTip.AddTool(&m_ctrlFilteringIncludeDirs, (tip + Localization::LookupNeutral(IDS_FILTER_EXAMPLE_DIRS)).c_str());
        m_toolTip.AddTool(&m_ctrlFilteringIncludeFiles, (tip + Localization::LookupNeutral(IDS_FILTER_EXAMPLE_FILES)).c_str());
    }
}

void CPageFiltering::OnOK()
{
    const int filteringSizeMinimum = std::stoi(GetText(IDC_FILTERING_SIZE_MIN));
    const int filteringSizeUnits = ComboSelection(IDC_FILTERING_MIN_UNITS);
    const bool filteringUseRegex = IsChecked(IDC_FILTERING_USE_REGEX);
    const int filteringMaxAgeDays = std::stoi(GetText(IDC_FILTERING_MAX_AGE_DAYS));
    const std::wstring filteringExcludeFiles = GetText(IDC_FILTERING_EXCLUDE_FILES);
    const std::wstring filteringExcludeDirs = GetText(IDC_FILTERING_EXCLUDE_DIRS);
    const std::wstring filteringIncludeFiles = GetText(IDC_FILTERING_INCLUDE_FILES);
    const std::wstring filteringIncludeDirs = GetText(IDC_FILTERING_INCLUDE_DIRS);

    const bool refreshAll = COptions::FilteringSizeMinimum != filteringSizeMinimum ||
        COptions::FilteringSizeUnits != filteringSizeUnits ||
        COptions::FilteringUseRegex != filteringUseRegex ||
        COptions::FilteringMaxAgeDays != filteringMaxAgeDays ||
        COptions::FilteringExcludeFiles.Obj() != filteringExcludeFiles ||
        COptions::FilteringExcludeDirs.Obj() != filteringExcludeDirs ||
        COptions::FilteringIncludeFiles.Obj() != filteringIncludeFiles ||
        COptions::FilteringIncludeDirs.Obj() != filteringIncludeDirs;

    COptions::FilteringSizeMinimum = filteringSizeMinimum;
    COptions::FilteringSizeUnits = filteringSizeUnits;
    COptions::FilteringUseRegex = filteringUseRegex;
    COptions::FilteringMaxAgeDays = filteringMaxAgeDays;
    COptions::FilteringExcludeFiles.Obj() = filteringExcludeFiles;
    COptions::FilteringExcludeDirs.Obj() = filteringExcludeDirs;
    COptions::FilteringIncludeFiles.Obj() = filteringIncludeFiles;
    COptions::FilteringIncludeDirs.Obj() = filteringIncludeDirs;
    CFiltering::CompileFilters();

    if (m_refreshOnFilteringChange && refreshAll)
    {
        CWinDirStatModel::Get()->StartScan(
            CWinDirStatModel::Get()->GetScanPathSpec());
    }
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
