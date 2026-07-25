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

IMPLEMENT_DYNAMIC(CPageFiltering, COptionsPage)

CPageFiltering::CPageFiltering(const bool refreshOnFilteringChange) :
    COptionsPage(IDD),
    m_refreshOnFilteringChange(refreshOnFilteringChange)
{
    BindText(IDC_FILTERING_SIZE_MIN, COptions::FilteringSizeMinimum, m_filteringSizeMinimum);
    BindCombo(IDC_FILTERING_MIN_UNITS, COptions::FilteringSizeUnits, m_filteringSizeUnits);
    BindCheck(IDC_FILTERING_USE_REGEX, COptions::FilteringUseRegex, m_filteringUseRegex);
    BindText(IDC_FILTERING_MAX_AGE_DAYS, COptions::FilteringMaxAgeDays, m_filteringMaxAgeDays);
    BindText(IDC_FILTERING_EXCLUDE_DIRS, COptions::FilteringExcludeDirs, m_filteringExcludeDirs);
    BindText(IDC_FILTERING_EXCLUDE_FILES, COptions::FilteringExcludeFiles, m_filteringExcludeFiles);
    BindText(IDC_FILTERING_INCLUDE_DIRS, COptions::FilteringIncludeDirs, m_filteringIncludeDirs);
    BindText(IDC_FILTERING_INCLUDE_FILES, COptions::FilteringIncludeFiles, m_filteringIncludeFiles);
}

void CPageFiltering::DoDataExchange(CDataExchange* pDX)
{
    COptionsPage::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_FILTERING_MIN_UNITS, m_ctlFilteringSizeUnits);
    DDX_Control(pDX, IDC_FILTERING_EXCLUDE_FILES, m_ctrlFilteringExcludeFiles);
    DDX_Control(pDX, IDC_FILTERING_EXCLUDE_DIRS, m_ctrlFilteringExcludeDirs);
    DDX_Control(pDX, IDC_FILTERING_INCLUDE_FILES, m_ctrlFilteringIncludeFiles);
    DDX_Control(pDX, IDC_FILTERING_INCLUDE_DIRS, m_ctrlFilteringIncludeDirs);
}

BEGIN_MESSAGE_MAP(CPageFiltering, COptionsPage)
    ON_EN_CHANGE(IDC_FILTERING_EXCLUDE_DIRS, OnSettingChanged)
    ON_EN_CHANGE(IDC_FILTERING_EXCLUDE_FILES, OnSettingChanged)
    ON_EN_CHANGE(IDC_FILTERING_INCLUDE_DIRS, OnSettingChanged)
    ON_EN_CHANGE(IDC_FILTERING_INCLUDE_FILES, OnSettingChanged)
    ON_BN_CLICKED(IDC_FILTERING_USE_REGEX, OnSettingChanged)
    ON_EN_CHANGE(IDC_FILTERING_SIZE_MIN, OnSettingChanged)
    ON_EN_CHANGE(IDC_FILTERING_MIN_UNITS, OnSettingChanged)
    ON_CBN_SELENDOK(IDC_FILTERING_MIN_UNITS, OnSettingChanged)
    ON_EN_CHANGE(IDC_FILTERING_MAX_AGE_DAYS, OnSettingChanged)
END_MESSAGE_MAP()

void CPageFiltering::InitializePage()
{
    m_ctlFilteringSizeUnits.AddString(GetSpec_Bytes().c_str());
    m_ctlFilteringSizeUnits.AddString(GetSpec_KiB().c_str());
    m_ctlFilteringSizeUnits.AddString(GetSpec_MiB().c_str());
    m_ctlFilteringSizeUnits.AddString(GetSpec_GiB().c_str());
    m_ctlFilteringSizeUnits.AddString(GetSpec_TiB().c_str());

    // Initialize the tooltip control
    m_toolTip.Create(this);
    SetToolTips();
    m_toolTip.SetMaxTipWidth(200);
    m_toolTip.Activate(TRUE);

    UpdateData(FALSE);
}

void CPageFiltering::AdjustControls()
{
    // Apply dark mode to this property page AFTER controls are initialized
    if (DarkMode::IsDarkModeActive())
    {
        COptionsPage::AdjustControls();
        DarkMode::AdjustControls(m_ctrlFilteringExcludeDirs.GetSafeHwnd());
        DarkMode::AdjustControls(m_ctrlFilteringExcludeFiles.GetSafeHwnd());
        DarkMode::AdjustControls(m_ctrlFilteringIncludeDirs.GetSafeHwnd());
        DarkMode::AdjustControls(m_ctrlFilteringIncludeFiles.GetSafeHwnd());
        m_ctrlFilteringExcludeDirs.Invalidate();
        m_ctrlFilteringExcludeFiles.Invalidate();
        m_ctrlFilteringIncludeDirs.Invalidate();
        m_ctrlFilteringIncludeFiles.Invalidate();
    }
}

void CPageFiltering::SetToolTips()
{
    const std::wstring tip = Localization::Lookup(IDS_PAGE_FILTERING_TOOLTIP_PREFIX) + L"\n\n";
    if (m_filteringUseRegex)
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
    UpdateData();

    const bool refreshAll = COptions::FilteringSizeMinimum != m_filteringSizeMinimum ||
        COptions::FilteringSizeUnits != m_filteringSizeUnits ||
        COptions::FilteringUseRegex != (FALSE != m_filteringUseRegex) ||
        COptions::FilteringMaxAgeDays != m_filteringMaxAgeDays ||
        COptions::FilteringExcludeFiles.Obj() != m_filteringExcludeFiles.GetString() ||
        COptions::FilteringExcludeDirs.Obj() != m_filteringExcludeDirs.GetString() ||
        COptions::FilteringIncludeFiles.Obj() != m_filteringIncludeFiles.GetString() ||
        COptions::FilteringIncludeDirs.Obj() != m_filteringIncludeDirs.GetString();

    ApplyOptionBindings();
    CFiltering::CompileFilters();

    if (m_refreshOnFilteringChange && refreshAll)
    {
        CWinDirStatModel::Get()->StartScan(
            CWinDirStatModel::Get()->GetScanPathSpec());
    }

    CMFCPropertyPage::OnOK();
}

void CPageFiltering::OnSettingChanged()
{
    if (!IsInitialized())
        return;

    UpdateData();
    SetModified();
    SetToolTips();
}

BOOL CPageFiltering::PreTranslateMessage(MSG* pMsg)
{
    m_toolTip.RelayEvent(pMsg);

    return COptionsPage::PreTranslateMessage(pMsg);
}
