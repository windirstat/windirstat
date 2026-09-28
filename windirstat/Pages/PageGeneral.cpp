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

void CPageGeneral::InitializePage()
{
    m_combo.SubclassDlgItem(IDC_COMBO, this);

    LoadBinds(CheckboxBindings);
    const int darkMode = std::clamp<int>(COptions::DarkMode, std::to_underlying(DM_DISABLED), std::to_underlying(DM_USE_WINDOWS));
    SetCheckedRadioButton(IDC_DARK_MODE_DISABLED, IDC_DARK_MODE_USE_WINDOWS, IDC_DARK_MODE_DISABLED + darkMode);

    SetChecked(IDC_PORTABLE_MODE, CDirStatApp::InPortableMode());

    // Query checkbox status and then gray out if a system-level entry
    // exists that cannot be changed without elevation
    const bool contextMenuIntegration = IsContextMenuRegistered(HKEY_LOCAL_MACHINE) ||
        IsContextMenuRegistered(HKEY_CURRENT_USER);
    SetChecked(IDC_CONTEXT_MENU, contextMenuIntegration);

    if (auto pWnd = GetDlgItem(IDC_CONTEXT_MENU); pWnd != nullptr &&
        !IsElevationActive() && IsContextMenuRegistered(HKEY_LOCAL_MACHINE))
    {
        pWnd.EnableWindow(false);
    }

    for (const auto& language : Localization::GetLanguageList())
    {
        const int i = m_combo.AddString(GetLocaleLanguage(language));
        m_combo.SetItemData(i, language);
        if (language == COptions::LanguageId)
        {
            m_combo.SetCurSel(i);
        }
    }

}

std::optional<CPropertyPage::ValidationError> CPageGeneral::PrepareSettings()
{
    int languageIndex = 0;
    if (auto error = ReadSelection(IDC_COMBO, languageIndex, 0, m_combo.GetCount() - 1)) return error;
    const int language = static_cast<int>(m_combo.GetItemData(languageIndex));
    const int checkedRadio = GetCheckedRadioButton(IDC_DARK_MODE_DISABLED, IDC_DARK_MODE_USE_WINDOWS);
    const int darkMode = checkedRadio == 0
        ? std::clamp<int>(COptions::DarkMode, std::to_underlying(DM_DISABLED), std::to_underlying(DM_USE_WINDOWS))
        : checkedRadio - IDC_DARK_MODE_DISABLED;
    // Assess whether a restart is required
    const auto sheet = GetParent<CSettingsSheet>();
    assert(sheet != nullptr);
    sheet->SetRestartRequired(darkMode, language);
    const auto checkboxes = ReadBinds(CheckboxBindings).value();
    const bool portableMode = IsChecked(IDC_PORTABLE_MODE);
    const bool contextMenuIntegration = IsChecked(IDC_CONTEXT_MENU);

    const bool formattingChanged = BindsChanged(checkboxes,
        { &COptions::UseWindowsLocaleSetting, &COptions::ShowTimeSeconds });
    const bool listChanged = BindsChanged(checkboxes, { &COptions::ListGrid,
        &COptions::ListStripes, &COptions::ListFullRowSelection, &COptions::UseSizeSuffixes });

    StageBinds(checkboxes);
    Stage(COptions::DarkMode, darkMode);

    Stage(COptions::LanguageId, language);
    StageEffect([portableMode, contextMenuIntegration, listChanged, formattingChanged]
    {
        if (const DWORD error = CDirStatApp::Get()->SetPortableMode(portableMode); error != ERROR_SUCCESS)
        {
            DisplayError(TranslateError(error));
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
        if (formattingChanged)
        {
            CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_NONE);
        }
    });
    return {};
}
