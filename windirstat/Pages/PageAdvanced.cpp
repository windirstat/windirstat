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

IMPLEMENT_DYNAMIC(CPageAdvanced, COptionsPage)

CPageAdvanced::CPageAdvanced() : COptionsPage(IDD)
{
    BindCheck(IDC_EXCLUDE_VOLUME_MOUNT_POINTS, COptions::ExcludeVolumeMountPoints, m_excludeVolumeMountPoints);
    BindCheck(IDC_EXCLUDE_JUNCTIONS, COptions::ExcludeJunctions, m_excludeJunctions);
    BindCheck(IDC_EXCLUDE_SYMLINKS_DIRECTORY, COptions::ExcludeSymbolicLinksDirectory, m_excludeSymbolicLinksDirectory);
    BindCheck(IDC_SKIP_CLOUD_LINKS, COptions::SkipDupeDetectionCloudLinks, m_skipDupeDetectionCloudLinks);
    BindCheck(IDC_EXCLUDE_HIDDEN_DIRECTORY, COptions::ExcludeHiddenDirectory, m_skipHiddenDirectory);
    BindCheck(IDC_EXCLUDE_PROTECTED_DIRECTORY, COptions::ExcludeProtectedDirectory, m_skipProtectedDirectory);
    BindCheck(IDC_BACKUP_RESTORE, COptions::UseBackupRestore, m_useBackupRestore);
    BindCheck(IDC_EXCLUDE_SYMLINKS_FILE, COptions::ExcludeSymbolicLinksFile, m_excludeSymbolicLinksFile);
    BindCheck(IDC_EXCLUDE_HIDDEN_FILE, COptions::ExcludeHiddenFile, m_skipHiddenFile);
    BindCheck(IDC_EXCLUDE_PROTECTED_FILE, COptions::ExcludeProtectedFile, m_skipProtectedFile);
    BindCheck(IDC_PROCESS_HARDLINKS, COptions::ProcessHardlinks, m_processHardlinks);
    BindCombo(IDC_HASH_ALGORITHM, COptions::FileHashAlgorithm, m_fileHashAlgorithm);
    BindCombo(IDC_PROCESS_PRIORITY, COptions::ProcessPriority, m_processPriority);
}

void CPageAdvanced::DoDataExchange(CDataExchange* pDX)
{
    COptionsPage::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_LARGEST_FILE_COUNT, m_largestFileCount);
    DDX_Text(pDX, IDC_FOLDER_HISTORY_COUNT, m_folderHistoryCount);
    DDX_CBIndex(pDX, IDC_COMBO_THREADS, m_scanningThreads);
}

BEGIN_MESSAGE_MAP(CPageAdvanced, COptionsPage)
    ON_BN_CLICKED(IDC_BACKUP_RESTORE, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_HIDDEN_DIRECTORY, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_PROTECTED_DIRECTORY, OnSettingChanged)
    ON_CBN_SELENDOK(IDC_COMBO_THREADS, OnSettingChanged)
    ON_CBN_SELENDOK(IDC_HASH_ALGORITHM, OnSettingChanged)
    ON_CBN_SELENDOK(IDC_PROCESS_PRIORITY, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_VOLUME_MOUNT_POINTS, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_JUNCTIONS, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_SYMLINKS_DIRECTORY, OnSettingChanged)
    ON_BN_CLICKED(IDC_SKIP_CLOUD_LINKS, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_SYMLINKS_FILE, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_HIDDEN_FILE, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_PROTECTED_FILE, OnSettingChanged)
    ON_BN_CLICKED(IDC_PROCESS_HARDLINKS, OnSettingChanged)
    ON_BN_CLICKED(IDC_RESET_PREFERENCES, &CPageAdvanced::OnBnClickedResetPreferences)
    ON_EN_CHANGE(IDC_LARGEST_FILE_COUNT, &CPageAdvanced::OnEnChangeLargestFileCount)
    ON_EN_CHANGE(IDC_FOLDER_HISTORY_COUNT, &CPageAdvanced::OnEnChangeFolderHistoryCount)
END_MESSAGE_MAP()

void CPageAdvanced::InitializePage()
{
    m_scanningThreads = COptions::ScanningThreads - 1;
    m_largestFileCount = std::to_wstring(COptions::LargeFileCount.Obj()).c_str();
    m_folderHistoryCount = std::to_wstring(COptions::FolderHistoryCount.Obj()).c_str();

    if (auto* priorityCombo = static_cast<CComboBox*>(GetDlgItem(IDC_PROCESS_PRIORITY)); priorityCombo != nullptr)
    {
        for (const auto& priority : SplitString(Localization::Lookup(IDS_PRIORITY_LEVELS), L','))
            priorityCombo->AddString(priority.c_str());
    }

    UpdateData(FALSE);
}

void CPageAdvanced::OnOK()
{
    UpdateData();

    const bool refreshReparsepoints =
        COptions::ExcludeJunctions && COptions::ExcludeJunctions != static_cast<bool>(m_excludeJunctions) ||
        COptions::ExcludeSymbolicLinksDirectory && COptions::ExcludeSymbolicLinksDirectory != static_cast<bool>(m_excludeSymbolicLinksDirectory) ||
        COptions::ExcludeVolumeMountPoints && COptions::ExcludeVolumeMountPoints != static_cast<bool>(m_excludeVolumeMountPoints) ||
        COptions::ExcludeSymbolicLinksFile && COptions::ExcludeSymbolicLinksFile != static_cast<bool>(m_excludeSymbolicLinksFile);
    const bool refreshAll = COptions::ExcludeHiddenDirectory != static_cast<bool>(m_skipHiddenDirectory) ||
        COptions::ExcludeProtectedDirectory != static_cast<bool>(m_skipProtectedDirectory) ||
        COptions::ExcludeHiddenFile != static_cast<bool>(m_skipHiddenFile) ||
        COptions::ExcludeProtectedFile != static_cast<bool>(m_skipProtectedFile) ||
        COptions::ProcessHardlinks != static_cast<bool>(m_processHardlinks) ||
        (COptions::ScanForDuplicates && COptions::FileHashAlgorithm != m_fileHashAlgorithm);

    ApplyOptionBindings();
    COptions::ScanningThreads = m_scanningThreads + 1;
    SetProcessPriority(COptions::ProcessPriority);
    COptions::LargeFileCount = std::stoi(m_largestFileCount.GetString());
    COptions::FolderHistoryCount = std::stoi(m_folderHistoryCount.GetString());

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
    CMFCPropertyPage::OnOK();
}

void CPageAdvanced::OnBnClickedResetPreferences()
{
    CDirStatApp::Get()->RestartApplication(true);
}

void CPageAdvanced::OnEnChangeLargestFileCount()
{
    if (!IsInitialized())
        return;

    // This function limits the number of files in the largest files list
    UpdateData(TRUE);

    m_largestFileCount = m_largestFileCount.IsEmpty() ? L"0" :
        std::to_wstring(std::clamp(std::stoi(m_largestFileCount.GetString()),
        COptions::LargeFileCount.Min(), COptions::LargeFileCount.Max())).c_str();

    UpdateData(FALSE);
}

void CPageAdvanced::OnEnChangeFolderHistoryCount()
{
    if (!IsInitialized())
        return;

    // This function limits the value in the folder history count
    UpdateData(TRUE);

    m_folderHistoryCount = m_folderHistoryCount.IsEmpty() ? L"0" :
        std::to_wstring(std::clamp(std::stoi(m_folderHistoryCount.GetString()),
        COptions::FolderHistoryCount.Min(), COptions::FolderHistoryCount.Max())).c_str();

    UpdateData(FALSE);
}
