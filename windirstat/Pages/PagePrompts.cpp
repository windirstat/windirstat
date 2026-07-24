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

IMPLEMENT_DYNAMIC(CPagePrompts, COptionsPage)

CPagePrompts::CPagePrompts() : COptionsPage(IDD)
{
    BindCheck(IDC_SHOW_MICROSOFT_PROGRESS, COptions::ShowMicrosoftProgress, m_showMicrosoftProgress);
    BindCheck(IDC_ELEVATION_PROMPT, COptions::ShowElevationPrompt, m_showElevationPrompt);
    BindCheck(IDC_CLOUD_LINKS_WARNING, COptions::ShowDupeDetectionCloudLinksWarning,
        m_showDupeDetectionCloudLinksWarning);
    BindCheck(IDC_DELETION_WARNING, COptions::ShowDeletePermanentlyWarning, m_showDeletePermanentlyWarning);
    BindCheck(IDC_DELETION_BIN_WARNING, COptions::ShowDeleteToRecycleBinWarning, m_showDeleteToRecycleBinWarning);
    BindCheck(IDC_PROMPT_EMPTY_BIN, COptions::ShowEmptyRecycleBinPrompt, m_showEmptyRecycleBinPrompt);
    BindCheck(IDC_PROMPT_CREATE_HARDLINK, COptions::ShowCreateHardlinkPrompt, m_showCreateHardlinkPrompt);
    BindCheck(IDC_PROMPT_REMOVE_MOTW, COptions::ShowRemoveMotwPrompt, m_showRemoveMotwPrompt);
    BindCheck(IDC_PROMPT_DISABLE_HIBERNATE, COptions::ShowDisableHibernatePrompt, m_showDisableHibernatePrompt);
    BindCheck(IDC_PROMPT_REMOVE_SHADOW, COptions::ShowRemoveShadowCopiesPrompt, m_showRemoveShadowCopiesPrompt);
    BindCheck(IDC_PROMPT_DISM_NORMAL, COptions::ShowDismCleanupPrompt, m_showDismCleanupPrompt);
    BindCheck(IDC_PROMPT_DISM_RESET, COptions::ShowDismResetPrompt, m_showDismResetPrompt);
    BindCheck(IDC_PROMPT_SET_DATES, COptions::ShowSetDatesPrompt, m_showSetDatesPrompt);
    BindCheck(IDC_PROMPT_REMOVE_EMPTY, COptions::ShowRemoveEmptyFoldersPrompt, m_showRemoveEmptyFoldersPrompt);
}

BEGIN_MESSAGE_MAP(CPagePrompts, COptionsPage)
    ON_BN_CLICKED(IDC_DELETION_WARNING, OnSettingChanged)
    ON_BN_CLICKED(IDC_DELETION_BIN_WARNING, OnSettingChanged)
    ON_BN_CLICKED(IDC_ELEVATION_PROMPT, OnSettingChanged)
    ON_BN_CLICKED(IDC_CLOUD_LINKS_WARNING, OnSettingChanged)
    ON_BN_CLICKED(IDC_SHOW_MICROSOFT_PROGRESS, OnSettingChanged)
    ON_CONTROL_RANGE(BN_CLICKED, IDC_PROMPT_EMPTY_BIN, IDC_PROMPT_REMOVE_EMPTY, OnSettingRangeChanged)
END_MESSAGE_MAP()

void CPagePrompts::InitializePage()
{
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
        SetDlgItemText(controlId,
            Localization::Format(IDS_PAGE_PROMPTS_OPERATION_CONFIRMATIONs,
                GetLocalizedMenuText(operationId, detail)).c_str());
    }
    UpdateData(FALSE);
}

void CPagePrompts::OnOK()
{
    UpdateData();

    ApplyOptionBindings();
    CMFCPropertyPage::OnOK();
}
