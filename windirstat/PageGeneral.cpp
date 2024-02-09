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
#include "MainFrame.h" // COptionsPropertySheet
#include "PageGeneral.h"

#include "DirStatDoc.h"
#include "Options.h"
#include "GlobalHelpers.h"
#include "Localization.h"

IMPLEMENT_DYNAMIC(CPageGeneral, CPropertyPage)

CPageGeneral::CPageGeneral()
    : CPropertyPage(CPageGeneral::IDD)
      , m_useWdsLocale(0)
      , m_humanFormat(0)
      , m_portableMode(0)
      , m_listGrid(0)
      , m_listStripes(0)
      , m_listFullRowSelection(0)
      , m_originalLanguage(0)
{
}

CPageGeneral::~CPageGeneral() = default;

COptionsPropertySheet* CPageGeneral::GetSheet() const
{
    auto sheet = DYNAMIC_DOWNCAST(COptionsPropertySheet, GetParent());
    ASSERT(sheet != NULL);
    return sheet;
}

void CPageGeneral::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPage::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_HUMANFORMAT, m_humanFormat);
    DDX_Check(pDX, IDC_USEWDSLOCALE, m_useWdsLocale);
    DDX_Control(pDX, IDC_COMBO, m_combo);
    DDX_Check(pDX, IDC_SHOWGRID, m_listGrid);
    DDX_Check(pDX, IDC_SHOWSTRIPES, m_listStripes);
    DDX_Check(pDX, IDC_FULLROWSELECTION, m_listFullRowSelection);
    DDX_Check(pDX, IDC_PORTABLE_MODE, m_portableMode);
}

BEGIN_MESSAGE_MAP(CPageGeneral, CPropertyPage)
    ON_BN_CLICKED(IDC_HUMANFORMAT, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_USEWDSLOCALE, OnBnClickedSetModified)
    ON_CBN_SELENDOK(IDC_COMBO, OnCbnSelendokCombo)
    ON_BN_CLICKED(IDC_SHOWGRID, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_SHOWSTRIPES, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_FULLROWSELECTION, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_PORTABLE_MODE, OnBnClickedSetModified)
END_MESSAGE_MAP()

BOOL CPageGeneral::OnInitDialog()
{
    CPropertyPage::OnInitDialog();

    Localization::UpdateDialogs(*this);

    m_humanFormat = COptions::HumanFormat;
    m_listGrid = COptions::ListGrid;
    m_listStripes = COptions::ListStripes;
    m_listFullRowSelection = COptions::ListFullRowSelection;
    m_useWdsLocale= COptions::UseWdsLocale;
    m_portableMode = GetWDSApp()->InPortableMode();

    const auto languages = Localization::GetLanguageList();
    for (const auto & language : languages)
    {
        int i = m_combo.AddString(GetLocaleLanguage(language));
        m_combo.SetItemData(i, language);
        if (language == COptions::LanguageId)
        {
            m_originalLanguage = i;
            m_combo.SetCurSel(i);
        }
    }

    UpdateData(false);
    return TRUE;
}

void CPageGeneral::OnOK()
{
    UpdateData();

    const bool wds_changed = static_cast<bool>(m_useWdsLocale) != COptions::UseWdsLocale;
    const bool lg_changed = static_cast<bool>(m_listGrid) != COptions::ListGrid ||
        static_cast<bool>(m_listStripes) != COptions::ListStripes ||
        static_cast<bool>(m_listFullRowSelection) != COptions::ListFullRowSelection;

    COptions::HumanFormat = (FALSE != m_humanFormat);
    COptions::UseWdsLocale = (FALSE != m_useWdsLocale);
    COptions::ListGrid = (FALSE != m_listGrid);
    COptions::ListStripes = (FALSE != m_listStripes);
    COptions::ListFullRowSelection = (FALSE != m_listFullRowSelection);
    if (!GetWDSApp()->SetPortableMode(m_portableMode))
    {
        AfxMessageBox(L"Could not toggle WinDirStat portable mode. Check your permissions.", MB_OK | MB_ICONERROR);
    }

    if (lg_changed)
    {
        GetDocument()->UpdateAllViews(nullptr, HINT_LISTSTYLECHANGED);
    }
    if (wds_changed)
    {
        GetDocument()->UpdateAllViews(nullptr, HINT_NULL);
    }

    const LANGID id = static_cast<LANGID>(m_combo.GetItemData(m_combo.GetCurSel()));
    COptions::LanguageId = static_cast<int>(id);

    CPropertyPage::OnOK();
}

void CPageGeneral::OnBnClickedSetModified()
{
    SetModified();
}

void CPageGeneral::OnCbnSelendokCombo()
{
    const int i = m_combo.GetCurSel();
    GetSheet()->SetLanguageChanged(i != m_originalLanguage);
    SetModified();
}
