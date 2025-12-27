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
#include "PageGeneral.h"

IMPLEMENT_DYNAMIC(CPageGeneral, CMFCPropertyPage)

CPageGeneral::CPageGeneral() : CMFCPropertyPage(IDD) {}

CPageGeneral::~CPageGeneral() = default;

COptionsPropertySheet* CPageGeneral::GetSheet() const
{
    const auto sheet = DYNAMIC_DOWNCAST(COptionsPropertySheet, GetParent());
    ASSERT(sheet != nullptr);
    return sheet;
}

void CPageGeneral::DoDataExchange(CDataExchange* pDX)
{
    CMFCPropertyPage::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_AUTO_ELEVATE, m_automaticallyElevateOnStartup);
    DDX_Check(pDX, IDC_COLUMN_AUTOSIZE, m_automaticallyResizeColumns);
    DDX_Check(pDX, IDC_FULL_ROW_SELECTION, m_listFullRowSelection);
    DDX_Check(pDX, IDC_PORTABLE_MODE, m_portableMode);
    DDX_Check(pDX, IDC_SHOW_GRID, m_listGrid);
    DDX_Check(pDX, IDC_SHOW_STRIPES, m_listStripes);
    DDX_Check(pDX, IDC_SIZE_SUFFIXES, m_sizeSuffixesFormat);
    DDX_Check(pDX, IDC_USE_WINDOWS_LOCALE, m_useWindowsLocale);
    DDX_Control(pDX, IDC_COMBO, m_combo);
    DDX_Radio(pDX, IDC_DARK_MODE_DISABLED, m_darkModeRadio);
}

BEGIN_MESSAGE_MAP(CPageGeneral, CMFCPropertyPage)
    ON_BN_CLICKED(IDC_AUTO_ELEVATE, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_COLUMN_AUTOSIZE, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_FULL_ROW_SELECTION, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_PORTABLE_MODE, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_SHOW_GRID, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_SHOW_STRIPES, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_SIZE_SUFFIXES, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_USE_WINDOWS_LOCALE, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_DARK_MODE_DISABLED, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_DARK_MODE_ENABLED, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_DARK_MODE_USE_WINDOWS, OnBnClickedSetModified)
    ON_CBN_SELENDOK(IDC_COMBO, OnBnClickedSetModified)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

HBRUSH CPageGeneral::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CMFCPropertyPage::OnCtlColor(pDC, pWnd, nCtlColor);
}

BOOL CPageGeneral::OnInitDialog()
{
    CMFCPropertyPage::OnInitDialog();

    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(GetSafeHwnd());

    m_automaticallyElevateOnStartup = COptions::AutoElevate;
    m_automaticallyResizeColumns = COptions::AutomaticallyResizeColumns;
    m_sizeSuffixesFormat = COptions::UseSizeSuffixes;
    m_listGrid = COptions::ListGrid;
    m_listStripes = COptions::ListStripes;
    m_listFullRowSelection = COptions::ListFullRowSelection;
    m_useWindowsLocale = COptions::UseWindowsLocaleSetting;
    m_portableMode = CDirStatApp::InPortableMode();
    m_darkModeRadio = COptions::DarkMode;

    for (const auto& language : Localization::GetLanguageList())
    {
        const int i = m_combo.AddString(GetLocaleLanguage(language).c_str());
        m_combo.SetItemData(i, language);
        if (language == COptions::LanguageId)
        {
            m_combo.SetCurSel(i);
        }
    }

    UpdateData(FALSE);
    return TRUE;
}

void CPageGeneral::OnOK()
{
    UpdateData();

    const bool windowsLocaleChanged = static_cast<bool>(m_useWindowsLocale) != COptions::UseWindowsLocaleSetting;
    const bool listChanged = static_cast<bool>(m_listGrid) != COptions::ListGrid ||
        static_cast<bool>(m_listStripes) != COptions::ListStripes ||
        static_cast<bool>(m_listFullRowSelection) != COptions::ListFullRowSelection ||
        static_cast<bool>(m_sizeSuffixesFormat) != COptions::UseSizeSuffixes;

    COptions::AutoElevate = (FALSE != m_automaticallyElevateOnStartup);
    COptions::AutomaticallyResizeColumns = (FALSE != m_automaticallyResizeColumns);
    COptions::UseSizeSuffixes = (FALSE != m_sizeSuffixesFormat);
    COptions::UseWindowsLocaleSetting = (FALSE != m_useWindowsLocale);
    COptions::ListGrid = (FALSE != m_listGrid);
    COptions::ListStripes = (FALSE != m_listStripes);
    COptions::ListFullRowSelection = (FALSE != m_listFullRowSelection);
    COptions::DarkMode = m_darkModeRadio;

    if (!CDirStatApp::Get()->SetPortableMode(m_portableMode))
    {
        DisplayError(L"Could not toggle WinDirStat portable mode. Check your permissions.");
    }

    // force general user interface update if anything changes
    if (const CDirStatDoc* doc = CDirStatDoc::Get(); listChanged && doc != nullptr)
    {
        // Iterate over all drive items and update their display names/free space item sizes
        if (CItem* root = doc->GetRootItem(); root != nullptr)
        {
            for (CItem* item : root->GetDriveItems())
            {
                item->UpdateFreeSpaceItem();
            }
        }

        CDirStatDoc::Get()->UpdateAllViews(nullptr, HINT_LISTSTYLECHANGED);
    }
    if (windowsLocaleChanged)
    {
        CDirStatDoc::Get()->UpdateAllViews(nullptr, HINT_NULL);
    }

    const LANGID id = static_cast<LANGID>(m_combo.GetItemData(m_combo.GetCurSel()));
    COptions::LanguageId = static_cast<int>(id);

    CMFCPropertyPage::OnOK();
}

void CPageGeneral::OnBnClickedSetModified()
{
    UpdateData(TRUE);

    // Assess for restart required
    const LANGID id = static_cast<LANGID>(m_combo.GetItemData(m_combo.GetCurSel()));
    const bool languagedChanged = id != static_cast<LANGID>(COptions::LanguageId);
    const bool darkModeChanged = m_darkModeRadio != COptions::DarkMode;
    GetSheet()->SetRestartRequired(darkModeChanged || languagedChanged);

    SetModified();
}
