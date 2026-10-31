// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "PageAdvanced.h"

CPageAdvanced::CPageAdvanced() : MessageTarget(IDD, L"IDS_PAGE_ADVANCED_TITLE")
{
}

void CPageAdvanced::InitializePage()
{
    // Populate fixed choices in their persisted selection order.
    for (int threads = 1; threads <= MAX_SCANNING_THREADS; ++threads)
        GetDlgItem(IDC_COMBO_THREADS).SendMessage(CB_ADDSTRING, 0, std::to_wstring(threads).c_str());
    for (const auto algorithm : { L"MD5", L"SHA1", L"SHA256", L"SHA384", L"SHA512", L"xxHash" })
        GetDlgItem(IDC_HASH_ALGORITHM).SendMessage(CB_ADDSTRING, 0, algorithm);

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
