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

static constexpr std::array DarkModeRadioIds
{
    IDC_DARK_MODE_DISABLED,
    IDC_DARK_MODE_ENABLED,
    IDC_DARK_MODE_USE_WINDOWS,
};

CPageGeneral::CPageGeneral() : MessageTarget(IDD)
{
}

bool CPageGeneral::IsContextMenuRegistered(const HKEY root)
{
    return CRegKey().Open(root, std::format(LR"(Software\Classes\Drive\shell\{})",
        wds::strWinDirStat).c_str(), KEY_READ) == ERROR_SUCCESS;
}

bool CPageGeneral::SetContextMenuRegistration(const bool enable)
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

int CPageGeneral::GetSelectedDarkMode() const
{
    const int checkedRadio = CheckedRadioButton(IDC_DARK_MODE_DISABLED, IDC_DARK_MODE_ENABLED);
    const auto selected = std::ranges::find(DarkModeRadioIds, checkedRadio);
    assert(selected != DarkModeRadioIds.end());
    return selected == DarkModeRadioIds.end()
        ? std::clamp<int>(COptions::DarkMode, DM_DISABLED, DM_USE_WINDOWS)
        : static_cast<int>(selected - DarkModeRadioIds.begin());
}

void CPageGeneral::InitializePage()
{
    m_combo.SubclassDlgItem(IDC_COMBO, this);

    SetChecked(IDC_AUTO_ELEVATE, COptions::AutoElevate);
    SetChecked(IDC_COLUMN_AUTOSIZE, COptions::AutomaticallyResizeColumns);
    SetChecked(IDC_FULL_ROW_SELECTION, COptions::ListFullRowSelection);
    SetChecked(IDC_SHOW_GRID, COptions::ListGrid);
    SetChecked(IDC_SHOW_STRIPES, COptions::ListStripes);
    SetChecked(IDC_SIZE_SUFFIXES, COptions::UseSizeSuffixes);
    SetChecked(IDC_USE_WINDOWS_LOCALE, COptions::UseWindowsLocaleSetting);
    const int darkMode = std::clamp<int>(COptions::DarkMode, DM_DISABLED, DM_USE_WINDOWS);
    SetCheckedRadioButton(IDC_DARK_MODE_DISABLED, IDC_DARK_MODE_ENABLED, DarkModeRadioIds[darkMode]);

    SetChecked(IDC_PORTABLE_MODE, CDirStatApp::InPortableMode());

    // Query checkbox status and then gray out if a system-level entry
    // exists that cannot be changed without elevation
    const bool contextMenuIntegration = IsContextMenuRegistered(HKEY_LOCAL_MACHINE) ||
        IsContextMenuRegistered(HKEY_CURRENT_USER);
    SetChecked(IDC_CONTEXT_MENU, contextMenuIntegration);

    if (CWnd* pWnd = GetDlgItem(IDC_CONTEXT_MENU); pWnd != nullptr &&
        !IsElevationActive() && IsContextMenuRegistered(HKEY_LOCAL_MACHINE))
    {
        pWnd->EnableWindow(false);
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

}

void CPageGeneral::OnOK()
{
    const bool useWindowsLocale = IsChecked(IDC_USE_WINDOWS_LOCALE);
    const bool listGrid = IsChecked(IDC_SHOW_GRID);
    const bool listStripes = IsChecked(IDC_SHOW_STRIPES);
    const bool listFullRowSelection = IsChecked(IDC_FULL_ROW_SELECTION);
    const bool sizeSuffixesFormat = IsChecked(IDC_SIZE_SUFFIXES);
    const bool portableMode = IsChecked(IDC_PORTABLE_MODE);
    const bool contextMenuIntegration = IsChecked(IDC_CONTEXT_MENU);

    const bool windowsLocaleChanged = useWindowsLocale != COptions::UseWindowsLocaleSetting;
    const bool listChanged = listGrid != COptions::ListGrid ||
        listStripes != COptions::ListStripes ||
        listFullRowSelection != COptions::ListFullRowSelection ||
        sizeSuffixesFormat != COptions::UseSizeSuffixes;

    COptions::AutoElevate = IsChecked(IDC_AUTO_ELEVATE);
    COptions::AutomaticallyResizeColumns = IsChecked(IDC_COLUMN_AUTOSIZE);
    COptions::ListFullRowSelection = listFullRowSelection;
    COptions::ListGrid = listGrid;
    COptions::ListStripes = listStripes;
    COptions::UseSizeSuffixes = sizeSuffixesFormat;
    COptions::UseWindowsLocaleSetting = useWindowsLocale;
    COptions::DarkMode = GetSelectedDarkMode();

    if (!CDirStatApp::Get()->SetPortableMode(portableMode))
    {
        DisplayError(L"Could not toggle WinDirStat portable mode. Check your permissions.");
    }

    // Update context menu registration; non-elevated instances may only
    // manage the per-user entry when no system-level entry exists
    const bool shouldBeRegistered = contextMenuIntegration;
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
}

void CPageGeneral::OnBnClickedSetModified()
{
    if (!IsInitialized())
        return;

    // Assess for restart required
    const LANGID id = static_cast<LANGID>(m_combo.GetItemData(m_combo.GetCurSel()));
    const bool languageChanged = id != static_cast<LANGID>(COptions::LanguageId);
    const bool darkModeChanged = GetSelectedDarkMode() != COptions::DarkMode;
    GetSheet()->SetRestartRequired(darkModeChanged || languageChanged);

    SetModified();
}
