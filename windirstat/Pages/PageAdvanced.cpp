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

IMPLEMENT_DYNAMIC(CPageAdvanced, CMFCPropertyPage)

CPageAdvanced::CPageAdvanced() : CMFCPropertyPage(IDD) {}

CPageAdvanced::~CPageAdvanced() = default;

COptionsPropertySheet* CPageAdvanced::GetSheet() const
{
    return DYNAMIC_DOWNCAST(COptionsPropertySheet, GetParent());
}

void CPageAdvanced::DoDataExchange(CDataExchange* pDX)
{
    CMFCPropertyPage::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_EXCLUDE_VOLUME_MOUNT_POINTS, m_excludeVolumeMountPoints);
    DDX_Check(pDX, IDC_EXCLUDE_JUNCTIONS, m_excludeJunctions);
    DDX_Check(pDX, IDC_EXCLUDE_SYMLINKS_DIRECTORY, m_excludeSymbolicLinksDirectory);
    DDX_Check(pDX, IDC_SKIP_CLOUD_LINKS, m_skipDupeDetectionCloudLinks);
    DDX_Check(pDX, IDC_EXCLUDE_HIDDEN_DIRECTORY, m_skipHiddenDirectory);
    DDX_Check(pDX, IDC_EXCLUDE_PROTECTED_DIRECTORY, m_skipProtectedDirectory);
    DDX_Check(pDX, IDC_BACKUP_RESTORE, m_useBackupRestore);
    DDX_Check(pDX, IDC_EXCLUDE_SYMLINKS_FILE, m_excludeSymbolicLinksFile);
    DDX_Check(pDX, IDC_EXCLUDE_HIDDEN_FILE, m_skipHiddenFile);
    DDX_Check(pDX, IDC_EXCLUDE_PROTECTED_FILE, m_skipProtectedFile);
    DDX_Check(pDX, IDC_PROCESS_HARDLINKS, m_processHardlinks);
    DDX_Text(pDX, IDC_LARGEST_FILE_COUNT, m_largestFileCount);
    DDX_Text(pDX, IDC_FOLDER_HISTORY_COUNT, m_folderHistoryCount);
    DDX_CBIndex(pDX, IDC_COMBO_THREADS, m_scanningThreads);
}

BEGIN_MESSAGE_MAP(CPageAdvanced, CMFCPropertyPage)
    ON_BN_CLICKED(IDC_BACKUP_RESTORE, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_HIDDEN_DIRECTORY, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_PROTECTED_DIRECTORY, OnSettingChanged)
    ON_CBN_SELENDOK(IDC_COMBO_THREADS, OnSettingChanged)
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
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

HBRUSH CPageAdvanced::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CMFCPropertyPage::OnCtlColor(pDC, pWnd, nCtlColor);
}

BOOL CPageAdvanced::OnInitDialog()
{
    CMFCPropertyPage::OnInitDialog();

    Localization::UpdateDialogs(*this);

    // Apply dark mode to this property page
    DarkMode::AdjustControls(GetSafeHwnd());

    m_excludeVolumeMountPoints = COptions::ExcludeVolumeMountPoints;
    m_excludeJunctions = COptions::ExcludeJunctions;
    m_excludeSymbolicLinksDirectory = COptions::ExcludeSymbolicLinksDirectory;
    m_skipDupeDetectionCloudLinks = COptions::SkipDupeDetectionCloudLinks;
    m_skipHiddenDirectory = COptions::ExcludeHiddenDirectory;
    m_skipProtectedDirectory = COptions::ExcludeProtectedDirectory;
    m_excludeSymbolicLinksFile = COptions::ExcludeSymbolicLinksFile;
    m_skipHiddenFile = COptions::ExcludeHiddenFile;
    m_skipProtectedFile = COptions::ExcludeProtectedFile;
    m_useBackupRestore = COptions::UseBackupRestore;
    m_processHardlinks = COptions::ProcessHardlinks;
    m_scanningThreads = COptions::ScanningThreads - 1;
    m_largestFileCount = std::to_wstring(COptions::LargeFileCount.Obj()).c_str();
    m_folderHistoryCount = std::to_wstring(COptions::FolderHistoryCount.Obj()).c_str();

    UpdateData(FALSE);
    return TRUE;
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
        COptions::ProcessHardlinks != static_cast<bool>(m_processHardlinks);

    COptions::ExcludeJunctions = (FALSE != m_excludeJunctions);
    COptions::ExcludeSymbolicLinksDirectory = (FALSE != m_excludeSymbolicLinksDirectory);
    COptions::ExcludeVolumeMountPoints = (FALSE != m_excludeVolumeMountPoints);
    COptions::SkipDupeDetectionCloudLinks = (FALSE != m_skipDupeDetectionCloudLinks);
    COptions::ExcludeHiddenDirectory = (FALSE != m_skipHiddenDirectory);
    COptions::ExcludeProtectedDirectory = (FALSE != m_skipProtectedDirectory);
    COptions::ExcludeSymbolicLinksFile = (FALSE != m_excludeSymbolicLinksFile);
    COptions::ExcludeHiddenFile = (FALSE != m_skipHiddenFile);
    COptions::ExcludeProtectedFile = (FALSE != m_skipProtectedFile);
    COptions::UseBackupRestore = (FALSE != m_useBackupRestore);
    COptions::ProcessHardlinks = (FALSE != m_processHardlinks);
    COptions::ScanningThreads = m_scanningThreads + 1;
    COptions::LargeFileCount = std::stoi(m_largestFileCount.GetString());
    COptions::FolderHistoryCount = std::stoi(m_folderHistoryCount.GetString());

    // Trim the folder history if needed
    COptions::SelectDrivesFolder.Obj().resize(min(static_cast<size_t>(COptions::FolderHistoryCount),
        COptions::SelectDrivesFolder.Obj().size()));

    if (refreshAll)
    {
        CDirStatDoc::Get()->OnOpenDocument(
            CDirStatDoc::Get()->GetPathName().GetString());
    }
    else if (refreshReparsepoints)
    {
        CDirStatDoc::Get()->RefreshReparsePointItems();
    }

    CDirStatDoc::Get()->UpdateAllViews(nullptr, HINT_LISTSTYLECHANGED);
    CMFCPropertyPage::OnOK();
}

void CPageAdvanced::OnSettingChanged()
{
    SetModified();
}

void CPageAdvanced::OnBnClickedResetPreferences()
{
    CDirStatApp::Get()->RestartApplication(true);
}

void CPageAdvanced::OnEnChangeLargestFileCount()
{
    // This function limits the number of files in the largest files list
    UpdateData(TRUE);

    m_largestFileCount = std::to_wstring(std::clamp(std::stoi(m_largestFileCount.GetString()),
        COptions::LargeFileCount.Min(), COptions::LargeFileCount.Max())).c_str();

    UpdateData(FALSE);
}

void CPageAdvanced::OnEnChangeFolderHistoryCount()
{
    // This function limits the value in the folder history count
    UpdateData(TRUE);

    m_folderHistoryCount = std::to_wstring(std::clamp(std::stoi(m_folderHistoryCount.GetString()),
        COptions::FolderHistoryCount.Min(), COptions::FolderHistoryCount.Max())).c_str();

    UpdateData(FALSE);
}
