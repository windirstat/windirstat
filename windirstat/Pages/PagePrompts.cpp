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
#include "PagePrompts.h"

CPagePrompts::CPagePrompts() : MessageTarget(IDD) {}

std::span<const CSettingsPage::CheckboxSettingBinding> CPagePrompts::CheckboxSettings()
{
    static constexpr std::array bindings
    {
        CheckboxSettingBinding{ IDC_SHOW_MICROSOFT_PROGRESS, COptions::ShowMicrosoftProgress },
        CheckboxSettingBinding{ IDC_ELEVATION_PROMPT, COptions::ShowElevationPrompt },
        CheckboxSettingBinding{ IDC_CLOUD_LINKS_WARNING, COptions::ShowDupeDetectionCloudLinksWarning },
        CheckboxSettingBinding{ IDC_DELETION_WARNING, COptions::ShowDeletePermanentlyWarning },
        CheckboxSettingBinding{ IDC_DELETION_BIN_WARNING, COptions::ShowDeleteToRecycleBinWarning },
        CheckboxSettingBinding{ IDC_PROMPT_EMPTY_BIN, COptions::ShowEmptyRecycleBinPrompt },
        CheckboxSettingBinding{ IDC_PROMPT_CREATE_HARDLINK, COptions::ShowCreateHardlinkPrompt },
        CheckboxSettingBinding{ IDC_PROMPT_REMOVE_MOTW, COptions::ShowRemoveMotwPrompt },
        CheckboxSettingBinding{ IDC_PROMPT_DISABLE_HIBERNATE, COptions::ShowDisableHibernatePrompt },
        CheckboxSettingBinding{ IDC_PROMPT_REMOVE_SHADOW, COptions::ShowRemoveShadowCopiesPrompt },
        CheckboxSettingBinding{ IDC_PROMPT_DISM_NORMAL, COptions::ShowDismCleanupPrompt },
        CheckboxSettingBinding{ IDC_PROMPT_DISM_RESET, COptions::ShowDismResetPrompt },
        CheckboxSettingBinding{ IDC_PROMPT_SET_DATES, COptions::ShowSetDatesPrompt },
        CheckboxSettingBinding{ IDC_PROMPT_REMOVE_EMPTY, COptions::ShowRemoveEmptyFoldersPrompt },
    };
    return bindings;
}

void CPagePrompts::InitializePage()
{
    LoadCheckboxSettings(CheckboxSettings());

    struct PromptControl
    {
        int controlId;
        std::wstring_view operationId;
        std::wstring_view detail;
    };
    static constexpr PromptControl promptControls[] =
    {
        { IDC_DELETION_WARNING,         IDS_MENU_DELETE,            {} },
        { IDC_DELETION_BIN_WARNING,     IDS_MENU_DELETE_BIN,        {} },
        { IDC_PROMPT_EMPTY_BIN,         IDS_MENU_EMPTY_BIN,         {} },
        { IDC_PROMPT_CREATE_HARDLINK,   IDS_MENU_CREATE_HARDLINK,   {} },
        { IDC_PROMPT_REMOVE_MOTW,       IDS_MENU_REMOVE_MOTW,       {} },
        { IDC_PROMPT_DISABLE_HIBERNATE, IDS_MENU_DISABLE_HIBERNATE, {} },
        { IDC_PROMPT_REMOVE_SHADOW,     IDS_MENU_REMOVE_SHADOW,     {} },
        { IDC_PROMPT_DISM_NORMAL,       IDS_MENU_DISM,               L"/StartComponentCleanup" },
        { IDC_PROMPT_DISM_RESET,        IDS_MENU_DISM,               L"/StartComponentCleanup /ResetBase" },
        { IDC_PROMPT_SET_DATES,         IDS_MENU_SET_DATES,         {} },
        { IDC_PROMPT_REMOVE_EMPTY,      IDS_MENU_REMOVE_EMPTY,      {} },
    };

    for (const auto& [controlId, operationId, detail] : promptControls)
    {
        SetText(controlId, Localization::Format(IDS_PAGE_PROMPTS_OPERATION_CONFIRMATIONs,
            GetLocalizedMenuText(operationId, detail)));
    }
}

void CPagePrompts::OnOK()
{
    SaveCheckboxSettings(CheckboxSettings());
}
