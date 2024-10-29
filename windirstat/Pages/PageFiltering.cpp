// PageFiltering.cpp - Implementation of CPageFiltering
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "stdafx.h"
#include "MainFrame.h"
#include "PageFiltering.h"
#include "GlobalHelpers.h"
#include "Options.h"
#include "Localization.h"
#include "WinDirStat.h"

IMPLEMENT_DYNAMIC(CPageFiltering, CPropertyPageEx)

CPageFiltering::CPageFiltering() : CPropertyPageEx(IDD) {}

CPageFiltering::~CPageFiltering() = default;

COptionsPropertySheet* CPageFiltering::GetSheet() const
{
    return DYNAMIC_DOWNCAST(COptionsPropertySheet, GetParent());
}

void CPageFiltering::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPageEx::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_FILTERING_EXCLUDE_DIRS, m_FilteringExcludeDirs);
    DDX_Text(pDX, IDC_FILTERING_EXCLUDE_FILES, m_FilteringExcludeFiles);
    DDX_Text(pDX, IDC_FILTERING_SIZE_MIN, m_FilteringSizeMinimum);
    DDX_Check(pDX, IDC_FILTERING_USE_REGEX, m_FilteringUseRegex);
    DDX_Control(pDX, IDC_FILTERING_MIN_UNITS, m_CtlFilteringSizeUnits);
    DDX_Control(pDX, IDC_FILTERING_EXCLUDE_FILES, m_CtrlFilteringExcludeFiles);
    DDX_Control(pDX, IDC_FILTERING_EXCLUDE_DIRS, m_CtrlFilteringExcludeDirs);
    DDX_CBIndex(pDX, IDC_FILTERING_MIN_UNITS, m_FilteringSizeUnits);
}

BEGIN_MESSAGE_MAP(CPageFiltering, CPropertyPageEx)
    ON_EN_CHANGE(IDC_FILTERING_EXCLUDE_DIRS, OnSettingChanged)
    ON_EN_CHANGE(IDC_FILTERING_EXCLUDE_FILES, OnSettingChanged)
    ON_BN_CLICKED(IDC_FILTERING_USE_REGEX, OnSettingChanged)
    ON_EN_CHANGE(IDC_FILTERING_SIZE_MIN, OnSettingChanged)
    ON_EN_CHANGE(IDC_FILTERING_MIN_UNITS, OnSettingChanged)
    ON_CBN_SELENDOK(IDC_FILTERING_MIN_UNITS, OnSettingChanged)
END_MESSAGE_MAP()

BOOL CPageFiltering::OnInitDialog()
{
    CPropertyPageEx::OnInitDialog();

    Localization::UpdateDialogs(*this);

    m_FilteringSizeMinimum = COptions::FilteringSizeMinimum;
    m_FilteringSizeUnits = COptions::FilteringSizeUnits;
    m_FilteringUseRegex = COptions::FilteringUseRegex;
    m_FilteringSizeUnits = COptions::FilteringSizeUnits;
    m_FilteringExcludeDirs = COptions::FilteringExcludeDirs.Obj().c_str();
    m_FilteringExcludeFiles = COptions::FilteringExcludeFiles.Obj().c_str();

    m_CtlFilteringSizeUnits.AddString(GetSpec_Bytes().c_str());
    m_CtlFilteringSizeUnits.AddString(GetSpec_KB().c_str());
    m_CtlFilteringSizeUnits.AddString(GetSpec_MB().c_str());
    m_CtlFilteringSizeUnits.AddString(GetSpec_GB().c_str());
    m_CtlFilteringSizeUnits.AddString(GetSpec_TB().c_str());

    // Initialize the tooltip control
    m_ToolTip.Create(this);
    SetToolTips();
    m_ToolTip.SetMaxTipWidth(200);
    m_ToolTip.Activate(TRUE);

    UpdateData(FALSE);
    return TRUE;
}

void CPageFiltering::SetToolTips()
{
    const std::wstring tip = Localization::Lookup(IDS_PAGE_FILTERING_TOOLTIP_PREFIX) + L"\n\n";
    if (m_FilteringUseRegex)
    {
        m_ToolTip.AddTool(&m_CtrlFilteringExcludeDirs, (tip + Localization::LookupNeutral(IDS_FILTER_EXAMPLE_DIRS_REGEX)).c_str());
        m_ToolTip.AddTool(&m_CtrlFilteringExcludeFiles, (tip + Localization::LookupNeutral(IDS_FILTER_EXAMPLE_FILES_REGEX)).c_str());
    }
    else
    {
        m_ToolTip.AddTool(&m_CtrlFilteringExcludeDirs, (tip + Localization::LookupNeutral(IDS_FILTER_EXAMPLE_DIRS)).c_str());
        m_ToolTip.AddTool(&m_CtrlFilteringExcludeFiles, (tip + Localization::LookupNeutral(IDS_FILTER_EXAMPLE_FILES)).c_str());
    }
}

void CPageFiltering::OnOK()
{
    UpdateData();

    COptions::FilteringSizeMinimum = m_FilteringSizeMinimum;
    COptions::FilteringSizeUnits = m_FilteringSizeUnits;
    COptions::FilteringUseRegex = (FALSE != m_FilteringUseRegex);
    COptions::FilteringExcludeFiles.Obj() = m_FilteringExcludeFiles;
    COptions::FilteringExcludeDirs.Obj() = m_FilteringExcludeDirs;
    COptions::CompileFilters();

    CPropertyPageEx::OnOK();
}

void CPageFiltering::OnSettingChanged()
{
    UpdateData();
    SetModified();
    SetToolTips();
}

BOOL CPageFiltering::PreTranslateMessage(MSG* pMsg)
{
    m_ToolTip.RelayEvent(pMsg);

    return CPropertyPageEx::PreTranslateMessage(pMsg);
}

