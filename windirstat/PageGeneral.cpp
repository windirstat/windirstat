// PageGeneral.cpp - Implementation of CPageGeneral
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
#include "WinDirStat.h"
#include "MainFrame.h"
#include "PageGeneral.h"
#include "DirStatDoc.h"
#include "Options.h"
#include "GlobalHelpers.h"
#include "Localization.h"

IMPLEMENT_DYNAMIC(CPageGeneral, CPropertyPage)

CPageGeneral::CPageGeneral() : CPropertyPage(IDD) {}

CPageGeneral::~CPageGeneral() = default;

COptionsPropertySheet* CPageGeneral::GetSheet() const
{
    const auto sheet = DYNAMIC_DOWNCAST(COptionsPropertySheet, GetParent());
    ASSERT(sheet != nullptr);
    return sheet;
}

void CPageGeneral::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPage::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_SIZE_SUFFIXES, m_SizeSuffixesFormat);
    DDX_Check(pDX, IDC_USE_WDS_LOCALE, m_UseFallbackLocale);
    DDX_Control(pDX, IDC_COMBO, m_Combo);
    DDX_Check(pDX, IDC_SHOW_GRID, m_ListGrid);
    DDX_Check(pDX, IDC_SHOW_STRIPES, m_ListStripes);
    DDX_Check(pDX, IDC_DELETION_WARNING, m_ShowDeletionWarning);
    DDX_Check(pDX, IDC_FULL_ROW_SELECTION, m_ListFullRowSelection);
    DDX_Check(pDX, IDC_PORTABLE_MODE, m_PortableMode);
}

BEGIN_MESSAGE_MAP(CPageGeneral, CPropertyPage)
    ON_BN_CLICKED(IDC_SIZE_SUFFIXES, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_USE_WDS_LOCALE, OnBnClickedSetModified)
    ON_CBN_SELENDOK(IDC_COMBO, OnCbnSelendokCombo)
    ON_BN_CLICKED(IDC_SHOW_GRID, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_SHOW_STRIPES, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_DELETION_WARNING, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_FULL_ROW_SELECTION, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_PORTABLE_MODE, OnBnClickedSetModified)
END_MESSAGE_MAP()

BOOL CPageGeneral::OnInitDialog()
{
    CPropertyPage::OnInitDialog();

    Localization::UpdateDialogs(*this);

    m_SizeSuffixesFormat = COptions::UseSizeSuffixes;
    m_ListGrid = COptions::ListGrid;
    m_ListStripes = COptions::ListStripes;
    m_ShowDeletionWarning = COptions::ShowDeleteWarning;
    m_ListFullRowSelection = COptions::ListFullRowSelection;
    m_UseFallbackLocale = COptions::UseWindowsLocaleSetting;
    m_PortableMode = CDirStatApp::InPortableMode();

    for (const auto & language : Localization::GetLanguageList())
    {
        const int i = m_Combo.AddString(GetLocaleLanguage(language).c_str());
        m_Combo.SetItemData(i, language);
        if (language == COptions::LanguageId)
        {
            m_Combo.SetCurSel(i);
        }
    }

    UpdateData(FALSE);
    return TRUE;
}

void CPageGeneral::OnOK()
{
    UpdateData();

    const bool fallbackChanged = static_cast<bool>(m_UseFallbackLocale) != COptions::UseWindowsLocaleSetting;
    const bool gridChanged = static_cast<bool>(m_ListGrid) != COptions::ListGrid ||
        static_cast<bool>(m_ListStripes) != COptions::ListStripes ||
        static_cast<bool>(m_ListFullRowSelection) != COptions::ListFullRowSelection;

    COptions::UseSizeSuffixes = (FALSE != m_SizeSuffixesFormat);
    COptions::UseWindowsLocaleSetting = (FALSE != m_UseFallbackLocale);
    COptions::ListGrid = (FALSE != m_ListGrid);
    COptions::ListStripes = (FALSE != m_ListStripes);
    COptions::ShowDeleteWarning = (FALSE != m_ShowDeletionWarning);
    COptions::ListFullRowSelection = (FALSE != m_ListFullRowSelection);
    if (!CDirStatApp::Get()->SetPortableMode(m_PortableMode))
    {
        AfxMessageBox(L"Could not toggle WinDirStat portable mode. Check your permissions.", MB_OK | MB_ICONERROR);
    }

    if (gridChanged)
    {
        CDirStatDoc::GetDocument()->UpdateAllViews(nullptr, HINT_LISTSTYLECHANGED);
    }
    if (fallbackChanged)
    {
        CDirStatDoc::GetDocument()->UpdateAllViews(nullptr, HINT_NULL);
    }

    const LANGID id = static_cast<LANGID>(m_Combo.GetItemData(m_Combo.GetCurSel()));
    COptions::LanguageId = static_cast<int>(id);

    CPropertyPage::OnOK();
}

void CPageGeneral::OnBnClickedSetModified()
{
    SetModified();
}

void CPageGeneral::OnCbnSelendokCombo()
{
    const LANGID langid = static_cast<LANGID>(m_Combo.GetItemData(m_Combo.GetCurSel()));
    GetSheet()->SetLanguageChanged(langid != COptions::LanguageId);
    SetModified();
}
