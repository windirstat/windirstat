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
            m_priorityCombo.AddString(priority.c_str());

    SetChecked(IDC_EXCLUDE_VOLUME_MOUNT_POINTS, COptions::ExcludeVolumeMountPoints);
    SetChecked(IDC_EXCLUDE_JUNCTIONS, COptions::ExcludeJunctions);
    SetChecked(IDC_EXCLUDE_SYMLINKS_DIRECTORY, COptions::ExcludeSymbolicLinksDirectory);
    SetChecked(IDC_SKIP_CLOUD_LINKS, COptions::SkipDupeDetectionCloudLinks);
    SetChecked(IDC_EXCLUDE_HIDDEN_DIRECTORY, COptions::ExcludeHiddenDirectory);
    SetChecked(IDC_EXCLUDE_PROTECTED_DIRECTORY, COptions::ExcludeProtectedDirectory);
    SetChecked(IDC_BACKUP_RESTORE, COptions::UseBackupRestore);
    SetChecked(IDC_EXCLUDE_SYMLINKS_FILE, COptions::ExcludeSymbolicLinksFile);
    SetChecked(IDC_EXCLUDE_HIDDEN_FILE, COptions::ExcludeHiddenFile);
    SetChecked(IDC_EXCLUDE_PROTECTED_FILE, COptions::ExcludeProtectedFile);
    SetChecked(IDC_PROCESS_HARDLINKS, COptions::ProcessHardlinks);

    SetComboSelection(IDC_HASH_ALGORITHM, COptions::FileHashAlgorithm);
    SetComboSelection(IDC_PROCESS_PRIORITY, COptions::ProcessPriority);
    SetComboSelection(IDC_COMBO_THREADS, COptions::ScanningThreads - 1);

    SetText(IDC_LARGEST_FILE_COUNT, std::to_wstring(COptions::LargeFileCount.Obj()));
    SetText(IDC_FOLDER_HISTORY_COUNT, std::to_wstring(COptions::FolderHistoryCount.Obj()));
}

void CPageAdvanced::OnOK()
{
    const bool excludeJunctions = IsChecked(IDC_EXCLUDE_JUNCTIONS);
    const bool excludeSymbolicLinksDirectory = IsChecked(IDC_EXCLUDE_SYMLINKS_DIRECTORY);
    const bool excludeVolumeMountPoints = IsChecked(IDC_EXCLUDE_VOLUME_MOUNT_POINTS);
    const bool excludeSymbolicLinksFile = IsChecked(IDC_EXCLUDE_SYMLINKS_FILE);
    const bool skipHiddenDirectory = IsChecked(IDC_EXCLUDE_HIDDEN_DIRECTORY);
    const bool skipProtectedDirectory = IsChecked(IDC_EXCLUDE_PROTECTED_DIRECTORY);
    const bool skipHiddenFile = IsChecked(IDC_EXCLUDE_HIDDEN_FILE);
    const bool skipProtectedFile = IsChecked(IDC_EXCLUDE_PROTECTED_FILE);
    const bool processHardlinks = IsChecked(IDC_PROCESS_HARDLINKS);
    const int fileHashAlgorithm = ComboSelection(IDC_HASH_ALGORITHM);

    const bool refreshReparsepoints =
        COptions::ExcludeJunctions != excludeJunctions ||
        COptions::ExcludeSymbolicLinksDirectory != excludeSymbolicLinksDirectory ||
        COptions::ExcludeVolumeMountPoints != excludeVolumeMountPoints ||
        COptions::ExcludeSymbolicLinksFile != excludeSymbolicLinksFile;
    const bool refreshAll = COptions::ExcludeHiddenDirectory != skipHiddenDirectory ||
        COptions::ExcludeProtectedDirectory != skipProtectedDirectory ||
        COptions::ExcludeHiddenFile != skipHiddenFile ||
        COptions::ExcludeProtectedFile != skipProtectedFile ||
        COptions::ProcessHardlinks != processHardlinks ||
        (COptions::ScanForDuplicates && COptions::FileHashAlgorithm != fileHashAlgorithm);

    COptions::ExcludeVolumeMountPoints = excludeVolumeMountPoints;
    COptions::ExcludeJunctions = excludeJunctions;
    COptions::ExcludeSymbolicLinksDirectory = excludeSymbolicLinksDirectory;
    COptions::SkipDupeDetectionCloudLinks = IsChecked(IDC_SKIP_CLOUD_LINKS);
    COptions::ExcludeHiddenDirectory = skipHiddenDirectory;
    COptions::ExcludeProtectedDirectory = skipProtectedDirectory;
    COptions::UseBackupRestore = IsChecked(IDC_BACKUP_RESTORE);
    COptions::ExcludeSymbolicLinksFile = excludeSymbolicLinksFile;
    COptions::ExcludeHiddenFile = skipHiddenFile;
    COptions::ExcludeProtectedFile = skipProtectedFile;
    COptions::ProcessHardlinks = processHardlinks;
    COptions::FileHashAlgorithm = fileHashAlgorithm;
    COptions::ProcessPriority = ComboSelection(IDC_PROCESS_PRIORITY);

    COptions::ScanningThreads = ComboSelection(IDC_COMBO_THREADS) + 1;
    SetProcessPriority(COptions::ProcessPriority);
    COptions::LargeFileCount = std::stoi(GetText(IDC_LARGEST_FILE_COUNT));
    COptions::FolderHistoryCount = std::stoi(GetText(IDC_FOLDER_HISTORY_COUNT));

    // Trim the folder history if needed
    COptions::SelectDrivesFolder.Obj().resize(std::min(static_cast<size_t>(COptions::FolderHistoryCount),
        COptions::SelectDrivesFolder.Obj().size()));

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
}

void CPageAdvanced::OnBnClickedResetPreferences()
{
    CDirStatApp::Get()->RestartApplication(true);
}

void CPageAdvanced::OnEnChangeLargestFileCount()
{
    if (!IsInitialized())
        return;

    std::wstring count = GetText(IDC_LARGEST_FILE_COUNT);
    count = count.empty() ? L"0" :
        std::to_wstring(std::clamp(std::stoi(count),
        COptions::LargeFileCount.Min(), COptions::LargeFileCount.Max()));

    SetText(IDC_LARGEST_FILE_COUNT, count);
}

void CPageAdvanced::OnEnChangeFolderHistoryCount()
{
    if (!IsInitialized())
        return;

    std::wstring count = GetText(IDC_FOLDER_HISTORY_COUNT);
    count = count.empty() ? L"0" :
        std::to_wstring(std::clamp(std::stoi(count),
        COptions::FolderHistoryCount.Min(), COptions::FolderHistoryCount.Max()));

    SetText(IDC_FOLDER_HISTORY_COUNT, count);
}
