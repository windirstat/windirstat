// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "PageShared.h"

//
// CPagePrompts. "Settings" property page "Prompts".
//
class CPagePrompts final : public MessageTarget<CPagePrompts, CSettingsPage>
{
public:
    enum : std::uint8_t { IDD = IDD_PAGE_PROMPTS };

    CPagePrompts();
    ~CPagePrompts() override = default;

protected:
    void InitializePage() override;
    std::optional<ValidationError> PrepareSettings() override;

    static constexpr std::array CheckboxBindings
    {
        &COptions::ShowMicrosoftProgress,
        &COptions::ShowElevationPrompt,
        &COptions::ShowDupeDetectionCloudLinksWarning,
        &COptions::ShowDeletePermanentlyWarning,
        &COptions::ShowDeleteToRecycleBinWarning,
        &COptions::ShowEmptyRecycleBinPrompt,
        &COptions::ShowCreateHardlinkPrompt,
        &COptions::ShowRemoveMotwPrompt,
        &COptions::ShowDisableHibernatePrompt,
        &COptions::ShowRemoveShadowCopiesPrompt,
        &COptions::ShowDismCleanupPrompt,
        &COptions::ShowDismResetPrompt,
        &COptions::ShowSetDatesPrompt,
        &COptions::ShowRemoveEmptyFoldersPrompt,
    };

public:
    static std::span<const RouteEntry> Routes();

};

inline std::span<const RouteEntry> CPagePrompts::Routes()
{
    static constexpr std::array entries
    {
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_DELETION_BIN_WARNING, IDC_DELETION_WARNING),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_ELEVATION_PROMPT),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_CLOUD_LINKS_WARNING),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_SHOW_MICROSOFT_PROGRESS),
        Route::Control<&OnSettingChanged>(BN_CLICKED, IDC_PROMPT_EMPTY_BIN, IDC_PROMPT_REMOVE_EMPTY),
    };
    return entries;
}
