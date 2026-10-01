// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "PageAdvanced.h"

CPageAdvanced::CPageAdvanced() : MessageTarget(IDD)
{
}

void CPageAdvanced::InitializePage()
{
    if (m_priorityCombo.SubclassDlgItem(IDC_PROCESS_PRIORITY, this))
        for (const auto& priority : SplitString(Localization::Lookup(IDS_PRIORITY_LEVELS), L','))
            m_priorityCombo.AddString(priority);

    LoadBinds(CheckboxBindings);

    LoadBinds(NumberBindings);
}

std::optional<CPropertyPage::ValidationError> CPageAdvanced::PrepareSettings()
{
    auto numbers = ReadBinds(NumberBindings);
    if (!numbers) return numbers.error();
    const auto& [largeFileCount, folderHistoryCount, priority, threads, fileHashAlgorithm] = *numbers;
    const auto checkboxes = ReadBinds(CheckboxBindings).value();

    const bool refreshReparsepoints = BindsChanged(checkboxes, { &COptions::ExcludeJunctions,
        &COptions::ExcludeSymbolicLinksDirectory, &COptions::ExcludeVolumeMountPoints, &COptions::ExcludeSymbolicLinksFile });
    const bool refreshAll = BindsChanged(checkboxes, { &COptions::ExcludeHiddenDirectory,
        &COptions::ExcludeProtectedDirectory, &COptions::ExcludeDropboxIgnored,
        &COptions::ExcludeHiddenFile, &COptions::ExcludeProtectedFile, &COptions::ProcessHardlinks }) ||
        (COptions::ScanForDuplicates && (fileHashAlgorithm.Changed() ||
            BindsChanged(checkboxes, { &COptions::SampleLargeFiles })));

    StageBinds(checkboxes);
    StageBinds(*numbers);

    // Trim the folder history if needed
    auto history = COptions::SelectDrivesFolder.Obj();
    history.resize(std::min(static_cast<size_t>(folderHistoryCount.value), history.size()));
    Stage(COptions::SelectDrivesFolder, std::move(history));
    StageEffect([priority = priority.value, refreshAll, refreshReparsepoints]
    {
        SetProcessPriority(priority);

        if (refreshAll)
        {
            CWinDirStatModel::Get()->StartScan(
                CWinDirStatModel::Get()->GetScanPathSpec());
        }
        else if (refreshReparsepoints)
        {
            CWinDirStatModel::Get()->RefreshReparsePointItems();
        }

        CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_LIST_STYLE);
    });
    return {};
}

void CPageAdvanced::OnBnClickedResetPreferences()
{
    CDirStatApp::Get()->RestartApplication(true);
}
