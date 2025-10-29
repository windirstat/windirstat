﻿// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
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

IMPLEMENT_DYNAMIC(CPageGeneral, CPropertyPageEx)

CPageGeneral::CPageGeneral() : CPropertyPageEx(IDD) {}

CPageGeneral::~CPageGeneral() = default;

COptionsPropertySheet* CPageGeneral::GetSheet() const
{
    const auto sheet = DYNAMIC_DOWNCAST(COptionsPropertySheet, GetParent());
    ASSERT(sheet != nullptr);
    return sheet;
}

void CPageGeneral::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPageEx::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_COLUMN_AUTOSIZE, m_AutomaticallyResizeColumns);
    DDX_Check(pDX, IDC_DELETION_WARNING, m_ShowDeletionWarning);
    DDX_Check(pDX, IDC_FULL_ROW_SELECTION, m_ListFullRowSelection);
    DDX_Check(pDX, IDC_PORTABLE_MODE, m_PortableMode);
    DDX_Check(pDX, IDC_SHOW_GRID, m_ListGrid);
    DDX_Check(pDX, IDC_SHOW_STRIPES, m_ListStripes);
    DDX_Check(pDX, IDC_SIZE_SUFFIXES, m_SizeSuffixesFormat);
    DDX_Check(pDX, IDC_USE_WINDOWS_LOCALE, m_UseWindowsLocale);
    DDX_Control(pDX, IDC_COMBO, m_Combo);
}

BEGIN_MESSAGE_MAP(CPageGeneral, CPropertyPageEx)
    ON_BN_CLICKED(IDC_COLUMN_AUTOSIZE, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_DELETION_WARNING, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_FULL_ROW_SELECTION, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_PORTABLE_MODE, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_SHOW_GRID, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_SHOW_STRIPES, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_SIZE_SUFFIXES, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_USE_WINDOWS_LOCALE, OnBnClickedSetModified)
    ON_CBN_SELENDOK(IDC_COMBO, OnCbnSelendokCombo)
END_MESSAGE_MAP()

BOOL CPageGeneral::OnInitDialog()
{
    CPropertyPageEx::OnInitDialog();

    Localization::UpdateDialogs(*this);

    m_AutomaticallyResizeColumns = COptions::AutomaticallyResizeColumns;
    m_SizeSuffixesFormat = COptions::UseSizeSuffixes;
    m_ListGrid = COptions::ListGrid;
    m_ListStripes = COptions::ListStripes;
    m_ShowDeletionWarning = COptions::ShowDeleteWarning;
    m_ListFullRowSelection = COptions::ListFullRowSelection;
    m_UseWindowsLocale = COptions::UseWindowsLocaleSetting;
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

    const bool windowsLocaleChanged = static_cast<bool>(m_UseWindowsLocale) != COptions::UseWindowsLocaleSetting;
    const bool listChanged = static_cast<bool>(m_ListGrid) != COptions::ListGrid ||
        static_cast<bool>(m_ListStripes) != COptions::ListStripes ||
        static_cast<bool>(m_ListFullRowSelection) != COptions::ListFullRowSelection ||
        static_cast<bool>(m_SizeSuffixesFormat) != COptions::UseSizeSuffixes;

    COptions::AutomaticallyResizeColumns = (FALSE != m_AutomaticallyResizeColumns);
    COptions::UseSizeSuffixes = (FALSE != m_SizeSuffixesFormat);
    COptions::UseWindowsLocaleSetting = (FALSE != m_UseWindowsLocale);
    COptions::ListGrid = (FALSE != m_ListGrid);
    COptions::ListStripes = (FALSE != m_ListStripes);
    COptions::ShowDeleteWarning = (FALSE != m_ShowDeletionWarning);
    COptions::ListFullRowSelection = (FALSE != m_ListFullRowSelection);

    if (!CDirStatApp::Get()->SetPortableMode(m_PortableMode))
    {
        DisplayError(L"Could not toggle WinDirStat portable mode. Check your permissions.");
    }

    // force general user interface update if anything changes
    if (listChanged)
    {
        const CDirStatDoc* pDoc = CDirStatDoc::GetDocument();
        if (pDoc != nullptr)
        {
            // Iterate over all drive items and update their display names/free space item sizes
            for (CItem* pItem : pDoc->GetDriveItems())
            {
                pItem->UpdateFreeSpaceItem();
            }
        }
        CDirStatDoc::GetDocument()->UpdateAllViews(nullptr, HINT_LISTSTYLECHANGED);
    }
    if (windowsLocaleChanged)
    {
        CDirStatDoc::GetDocument()->UpdateAllViews(nullptr, HINT_NULL);
    }

    const LANGID id = static_cast<LANGID>(m_Combo.GetItemData(m_Combo.GetCurSel()));
    COptions::LanguageId = static_cast<int>(id);

    CPropertyPageEx::OnOK();
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
