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

IMPLEMENT_DYNAMIC(CPageFiltering, CMFCPropertyPage)

CPageFiltering::CPageFiltering() : CMFCPropertyPage(IDD) {}

COptionsPropertySheet* CPageFiltering::GetSheet() const
{
    return DYNAMIC_DOWNCAST(COptionsPropertySheet, GetParent());
}

void CPageFiltering::DoDataExchange(CDataExchange* pDX)
{
    CMFCPropertyPage::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_FILTERING_EXCLUDE_DIRS, m_filteringExcludeDirs);
    DDX_Text(pDX, IDC_FILTERING_EXCLUDE_FILES, m_filteringExcludeFiles);
    DDX_Text(pDX, IDC_FILTERING_INCLUDE_DIRS, m_filteringIncludeDirs);
    DDX_Text(pDX, IDC_FILTERING_INCLUDE_FILES, m_filteringIncludeFiles);
    DDX_Text(pDX, IDC_FILTERING_SIZE_MIN, m_filteringSizeMinimum);
    DDX_Check(pDX, IDC_FILTERING_USE_REGEX, m_filteringUseRegex);
    DDX_Text(pDX, IDC_FILTERING_MAX_AGE_DAYS, m_filteringMaxAgeDays);
    DDX_Control(pDX, IDC_FILTERING_MIN_UNITS, m_ctlFilteringSizeUnits);
    DDX_Control(pDX, IDC_FILTERING_EXCLUDE_FILES, m_ctrlFilteringExcludeFiles);
    DDX_Control(pDX, IDC_FILTERING_EXCLUDE_DIRS, m_ctrlFilteringExcludeDirs);
    DDX_Control(pDX, IDC_FILTERING_INCLUDE_FILES, m_ctrlFilteringIncludeFiles);
    DDX_Control(pDX, IDC_FILTERING_INCLUDE_DIRS, m_ctrlFilteringIncludeDirs);
    DDX_CBIndex(pDX, IDC_FILTERING_MIN_UNITS, m_filteringSizeUnits);
}

BEGIN_MESSAGE_MAP(CPageFiltering, CMFCPropertyPage)
    ON_EN_CHANGE(IDC_FILTERING_EXCLUDE_DIRS, OnSettingChanged)
    ON_EN_CHANGE(IDC_FILTERING_EXCLUDE_FILES, OnSettingChanged)
    ON_EN_CHANGE(IDC_FILTERING_INCLUDE_DIRS, OnSettingChanged)
    ON_EN_CHANGE(IDC_FILTERING_INCLUDE_FILES, OnSettingChanged)
    ON_BN_CLICKED(IDC_FILTERING_USE_REGEX, OnSettingChanged)
    ON_EN_CHANGE(IDC_FILTERING_SIZE_MIN, OnSettingChanged)
    ON_EN_CHANGE(IDC_FILTERING_MIN_UNITS, OnSettingChanged)
    ON_CBN_SELENDOK(IDC_FILTERING_MIN_UNITS, OnSettingChanged)
    ON_EN_CHANGE(IDC_FILTERING_MAX_AGE_DAYS, OnSettingChanged)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

HBRUSH CPageFiltering::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CMFCPropertyPage::OnCtlColor(pDC, pWnd, nCtlColor);
}

BOOL CPageFiltering::OnInitDialog()
{
    CMFCPropertyPage::OnInitDialog();

    Localization::UpdateDialogs(*this);

    m_filteringSizeMinimum = COptions::FilteringSizeMinimum;
    m_filteringSizeUnits = COptions::FilteringSizeUnits;
    m_filteringUseRegex = COptions::FilteringUseRegex;
    m_filteringMaxAgeDays = COptions::FilteringMaxAgeDays;
    m_filteringExcludeDirs = COptions::FilteringExcludeDirs.Obj().c_str();
    m_filteringExcludeFiles = COptions::FilteringExcludeFiles.Obj().c_str();
    m_filteringIncludeDirs = COptions::FilteringIncludeDirs.Obj().c_str();
    m_filteringIncludeFiles = COptions::FilteringIncludeFiles.Obj().c_str();

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

    // Apply dark mode to this property page AFTER controls are initialized
    if (DarkMode::IsDarkModeActive())
    {
        DarkMode::AdjustControls(GetSafeHwnd());
        DarkMode::AdjustControls(m_ctrlFilteringExcludeDirs.GetSafeHwnd());
        DarkMode::AdjustControls(m_ctrlFilteringExcludeFiles.GetSafeHwnd());
        DarkMode::AdjustControls(m_ctrlFilteringIncludeDirs.GetSafeHwnd());
        DarkMode::AdjustControls(m_ctrlFilteringIncludeFiles.GetSafeHwnd());
        m_ctrlFilteringExcludeDirs.Invalidate();
        m_ctrlFilteringExcludeFiles.Invalidate();
        m_ctrlFilteringIncludeDirs.Invalidate();
        m_ctrlFilteringIncludeFiles.Invalidate();
    }

    return TRUE;
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

    COptions::FilteringSizeMinimum = m_filteringSizeMinimum;
    COptions::FilteringSizeUnits = m_filteringSizeUnits;
    COptions::FilteringUseRegex = (FALSE != m_filteringUseRegex);
    COptions::FilteringMaxAgeDays = m_filteringMaxAgeDays;
    COptions::FilteringExcludeFiles.Obj() = m_filteringExcludeFiles;
    COptions::FilteringExcludeDirs.Obj() = m_filteringExcludeDirs;
    COptions::FilteringIncludeFiles.Obj() = m_filteringIncludeFiles;
    COptions::FilteringIncludeDirs.Obj() = m_filteringIncludeDirs;
    CFiltering::CompileFilters();

    CMFCPropertyPage::OnOK();
}

void CPageFiltering::OnSettingChanged()
{
    UpdateData();
    SetModified();
    SetToolTips();
}

BOOL CPageFiltering::PreTranslateMessage(MSG* pMsg)
{
    m_toolTip.RelayEvent(pMsg);

    return CMFCPropertyPage::PreTranslateMessage(pMsg);
}
