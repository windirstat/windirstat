// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "PagePrompts.h"

CPagePrompts::CPagePrompts() : MessageTarget(IDD) {}

void CPagePrompts::InitializePage()
{
    LoadBinds(CheckboxBindings);

    struct PromptControl
    {
        int controlId;
        std::wstring_view operationId;
        std::wstring_view detail;
    };
    static constexpr std::array promptControls =
    {
        PromptControl{ IDC_DELETION_WARNING,         IDS_MENU_DELETE,            {} },
        PromptControl{ IDC_DELETION_BIN_WARNING,     IDS_MENU_DELETE_BIN,        {} },
        PromptControl{ IDC_PROMPT_EMPTY_BIN,         IDS_MENU_EMPTY_BIN,         {} },
        PromptControl{ IDC_PROMPT_CREATE_HARDLINK,   IDS_MENU_CREATE_HARDLINK,   {} },
        PromptControl{ IDC_PROMPT_REMOVE_MOTW,       IDS_MENU_REMOVE_MOTW,       {} },
        PromptControl{ IDC_PROMPT_DISABLE_HIBERNATE, IDS_MENU_DISABLE_HIBERNATE, {} },
        PromptControl{ IDC_PROMPT_REMOVE_SHADOW,     IDS_MENU_REMOVE_SHADOW,     {} },
        PromptControl{ IDC_PROMPT_DISM_NORMAL,       IDS_MENU_DISM,               L"/StartComponentCleanup" },
        PromptControl{ IDC_PROMPT_DISM_RESET,        IDS_MENU_DISM,               L"/StartComponentCleanup /ResetBase" },
        PromptControl{ IDC_PROMPT_SET_DATES,         IDS_MENU_SET_DATES,         {} },
        PromptControl{ IDC_PROMPT_REMOVE_EMPTY,      IDS_MENU_REMOVE_EMPTY,      {} },
    };

    for (const auto& [controlId, operationId, detail] : promptControls)
    {
        SetText(controlId, Localization::Format(IDS_PAGE_PROMPTS_OPERATION_CONFIRMATIONs,
            GetLocalizedMenuText(operationId, detail)));
    }
}

std::optional<CPropertyPage::ValidationError> CPagePrompts::PrepareSettings()
{
    StageBinds(ReadBinds(CheckboxBindings).value());
    return {};
}
