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
