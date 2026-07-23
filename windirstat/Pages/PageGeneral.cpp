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

IMPLEMENT_DYNAMIC(CPageGeneral, COptionsPage)

CPageGeneral::CPageGeneral() : COptionsPage(IDD)
{
    BindCheck(IDC_AUTO_ELEVATE, COptions::AutoElevate, m_automaticallyElevateOnStartup);
    BindCheck(IDC_COLUMN_AUTOSIZE, COptions::AutomaticallyResizeColumns, m_automaticallyResizeColumns);
    BindCheck(IDC_FULL_ROW_SELECTION, COptions::ListFullRowSelection, m_listFullRowSelection);
    BindCheck(IDC_SHOW_GRID, COptions::ListGrid, m_listGrid);
    BindCheck(IDC_SHOW_STRIPES, COptions::ListStripes, m_listStripes);
    BindCheck(IDC_SIZE_SUFFIXES, COptions::UseSizeSuffixes, m_sizeSuffixesFormat);
    BindCheck(IDC_USE_WINDOWS_LOCALE, COptions::UseWindowsLocaleSetting, m_useWindowsLocale);
    BindRadio(IDC_DARK_MODE_DISABLED, COptions::DarkMode, m_darkModeRadio);
}

void CPageGeneral::DoDataExchange(CDataExchange* pDX)
{
    COptionsPage::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_CONTEXT_MENU, m_contextMenuIntegration);
    DDX_Check(pDX, IDC_PORTABLE_MODE, m_portableMode);
    DDX_Control(pDX, IDC_COMBO, m_combo);
}

BEGIN_MESSAGE_MAP(CPageGeneral, COptionsPage)
    ON_BN_CLICKED(IDC_AUTO_ELEVATE, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_COLUMN_AUTOSIZE, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_CONTEXT_MENU, OnBnClickedSetModified)
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
END_MESSAGE_MAP()

bool CPageGeneral::IsContextMenuRegistered(HKEY root)
{
    return CRegKey().Open(root, std::format(LR"(Software\Classes\Drive\shell\{})",
        wds::strWinDirStat).c_str(), KEY_READ) == ERROR_SUCCESS;
}

bool CPageGeneral::SetContextMenuRegistration(bool enable)
{
    // Elevated instances manage the system-level entry; otherwise use a per-user entry
    const HKEY root = IsElevationActive() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

    for (const std::wstring& rootSubKey : { L"Drive", L"Directory" })
    {
        const std::wstring baseKey = std::format(LR"(Software\Classes\{}\shell\{})",
            rootSubKey, wds::strWinDirStat);

        if (!enable)
        {
            // Remove the context menu entries, including any per-user entry
            // so the menu item does not linger after an elevated removal
            RegDeleteTree(root, baseKey.c_str());
            RegDeleteTree(HKEY_CURRENT_USER, baseKey.c_str());
            continue;
        }

        // Create/open the base key
        CRegKey key;
        const std::wstring exePath = GetAppFileName();
        if (key.Create(root, baseKey.c_str()) != ERROR_SUCCESS ||
            key.SetStringValue(nullptr, wds::strWinDirStat) != ERROR_SUCCESS ||
            key.SetStringValue(L"Icon", exePath.c_str()) != ERROR_SUCCESS)
        {
            SetContextMenuRegistration(false);
            return false;
        }

        // Create/open the command key
        const std::wstring cmdKey = baseKey + L"\\command";
        const std::wstring cmdVal = std::format(LR"("{}" "%1")", exePath);
        if (key.Create(root, cmdKey.c_str()) != ERROR_SUCCESS ||
            key.SetStringValue(nullptr, cmdVal.c_str()) != ERROR_SUCCESS)
        {
            SetContextMenuRegistration(false);
            return false;
        }
    }

    return true;
}

void CPageGeneral::InitializePage()
{
    m_portableMode = CDirStatApp::InPortableMode();

    // Query checkbox status and then gray out if a system-level entry
    // exists that cannot be changed without elevation
    m_contextMenuIntegration = IsContextMenuRegistered(HKEY_LOCAL_MACHINE) ||
        IsContextMenuRegistered(HKEY_CURRENT_USER) ? TRUE : FALSE;
    if (CWnd* pWnd = GetDlgItem(IDC_CONTEXT_MENU); pWnd != nullptr &&
        !IsElevationActive() && IsContextMenuRegistered(HKEY_LOCAL_MACHINE))
    {
        pWnd->EnableWindow(FALSE);
    }

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
}

void CPageGeneral::OnOK()
{
    UpdateData();

    const bool windowsLocaleChanged = static_cast<bool>(m_useWindowsLocale) != COptions::UseWindowsLocaleSetting;
    const bool listChanged = static_cast<bool>(m_listGrid) != COptions::ListGrid ||
        static_cast<bool>(m_listStripes) != COptions::ListStripes ||
        static_cast<bool>(m_listFullRowSelection) != COptions::ListFullRowSelection ||
        static_cast<bool>(m_sizeSuffixesFormat) != COptions::UseSizeSuffixes;

    ApplyOptionBindings();

    if (!CDirStatApp::Get()->SetPortableMode(m_portableMode))
    {
        DisplayError(L"Could not toggle WinDirStat portable mode. Check your permissions.");
    }

    // Update context menu registration; non-elevated instances may only
    // manage the per-user entry when no system-level entry exists
    const bool shouldBeRegistered = (m_contextMenuIntegration != FALSE);
    const bool systemRegistered = IsContextMenuRegistered(HKEY_LOCAL_MACHINE);
    const bool isRegistered = systemRegistered || IsContextMenuRegistered(HKEY_CURRENT_USER);
    if (isRegistered != shouldBeRegistered && (IsElevationActive() || !systemRegistered))
    {
        SetContextMenuRegistration(shouldBeRegistered);
    }

    // force general user interface update if anything changes
    if (const CWinDirStatModel* model = CWinDirStatModel::Get(); listChanged && model != nullptr)
    {
        // Iterate over all drive items and update their display names/free space item sizes
        if (const CItem* root = model->GetRootItem(); root != nullptr)
        {
            for (CItem* item : root->GetDriveItems())
            {
                item->UpdateFreeSpaceItem();
            }
        }

        CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_LIST_STYLE);
    }
    if (windowsLocaleChanged)
    {
        CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_NONE);
    }

    const LANGID id = static_cast<LANGID>(m_combo.GetItemData(m_combo.GetCurSel()));
    COptions::LanguageId = static_cast<int>(id);

    CMFCPropertyPage::OnOK();
}

void CPageGeneral::OnBnClickedSetModified()
{
    if (!IsInitialized())
        return;

    UpdateData(TRUE);

    // Assess for restart required
    const LANGID id = static_cast<LANGID>(m_combo.GetItemData(m_combo.GetCurSel()));
    const bool languageChanged = id != static_cast<LANGID>(COptions::LanguageId);
    const bool darkModeChanged = m_darkModeRadio != COptions::DarkMode;
    GetSheet()->SetRestartRequired(darkModeChanged || languageChanged);

    SetModified();
}
