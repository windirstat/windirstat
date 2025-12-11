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
    DDX_Check(pDX, IDC_EXCLUDE_VOLUME_MOUNT_POINTS, m_ExcludeVolumeMountPoints);
    DDX_Check(pDX, IDC_EXCLUDE_JUNCTIONS, m_ExcludeJunctions);
    DDX_Check(pDX, IDC_EXCLUDE_SYMLINKS_DIRECTORY, m_ExcludeSymbolicLinksDirectory);
    DDX_Check(pDX, IDC_SKIP_CLOUD_LINKS, m_SkipDupeDetectionCloudLinks);
    DDX_Check(pDX, IDC_EXCLUDE_HIDDEN_DIRECTORY, m_SkipHiddenDirectory);
    DDX_Check(pDX, IDC_EXCLUDE_PROTECTED_DIRECTORY, m_SkipProtectedDirectory);
    DDX_Check(pDX, IDC_BACKUP_RESTORE, m_UseBackupRestore);
    DDX_Check(pDX, IDC_EXCLUDE_SYMLINKS_FILE, m_ExcludeSymbolicLinksFile);
    DDX_Check(pDX, IDC_EXCLUDE_HIDDEN_FILE, m_SkipHiddenFile);
    DDX_Check(pDX, IDC_EXCLUDE_PROTECTED_FILE, m_SkipProtectedFile);
    DDX_Text(pDX, IDC_LARGEST_FILE_COUNT, m_LargestFileCount);
    DDX_Text(pDX, IDC_FOLDER_HISTORY_COUNT, m_FolderHistoryCount);
    DDX_CBIndex(pDX, IDC_COMBO_THREADS, m_ScanningThreads);
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

    m_ExcludeVolumeMountPoints = COptions::ExcludeVolumeMountPoints;
    m_ExcludeJunctions = COptions::ExcludeJunctions;
    m_ExcludeSymbolicLinksDirectory = COptions::ExcludeSymbolicLinksDirectory;
    m_SkipDupeDetectionCloudLinks = COptions::SkipDupeDetectionCloudLinks;
    m_SkipHiddenDirectory = COptions::ExcludeHiddenDirectory;
    m_SkipProtectedDirectory = COptions::ExcludeProtectedDirectory;
    m_ExcludeSymbolicLinksFile = COptions::ExcludeSymbolicLinksFile;
    m_SkipHiddenFile = COptions::ExcludeHiddenFile;
    m_SkipProtectedFile = COptions::ExcludeProtectedFile;
    m_UseBackupRestore = COptions::UseBackupRestore;
    m_ScanningThreads = COptions::ScanningThreads - 1;
    m_LargestFileCount = std::to_wstring(COptions::LargeFileCount.Obj()).c_str();
    m_FolderHistoryCount = std::to_wstring(COptions::FolderHistoryCount.Obj()).c_str();

    UpdateData(FALSE);
    return TRUE;
}

void CPageAdvanced::OnOK()
{
    UpdateData();

    const bool refreshReparsepoints =
        COptions::ExcludeJunctions && COptions::ExcludeJunctions != static_cast<bool>(m_ExcludeJunctions) ||
        COptions::ExcludeSymbolicLinksDirectory && COptions::ExcludeSymbolicLinksDirectory != static_cast<bool>(m_ExcludeSymbolicLinksDirectory) ||
        COptions::ExcludeVolumeMountPoints && COptions::ExcludeVolumeMountPoints != static_cast<bool>(m_ExcludeVolumeMountPoints) ||
        COptions::ExcludeSymbolicLinksFile && COptions::ExcludeSymbolicLinksFile != static_cast<bool>(m_ExcludeSymbolicLinksFile);
    const bool refreshAll = COptions::ExcludeHiddenDirectory != static_cast<bool>(m_SkipHiddenDirectory) ||
        COptions::ExcludeProtectedDirectory != static_cast<bool>(m_SkipProtectedDirectory) ||
        COptions::ExcludeHiddenFile != static_cast<bool>(m_SkipHiddenFile) ||
        COptions::ExcludeProtectedFile != static_cast<bool>(m_SkipProtectedFile);

    COptions::ExcludeJunctions = (FALSE != m_ExcludeJunctions);
    COptions::ExcludeSymbolicLinksDirectory = (FALSE != m_ExcludeSymbolicLinksDirectory);
    COptions::ExcludeVolumeMountPoints = (FALSE != m_ExcludeVolumeMountPoints);
    COptions::SkipDupeDetectionCloudLinks = (FALSE != m_SkipDupeDetectionCloudLinks);
    COptions::ExcludeHiddenDirectory = (FALSE != m_SkipHiddenDirectory);
    COptions::ExcludeProtectedDirectory = (FALSE != m_SkipProtectedDirectory);
    COptions::ExcludeSymbolicLinksFile = (FALSE != m_ExcludeSymbolicLinksFile);
    COptions::ExcludeHiddenFile = (FALSE != m_SkipHiddenFile);
    COptions::ExcludeProtectedFile = (FALSE != m_SkipProtectedFile);
    COptions::UseBackupRestore = (FALSE != m_UseBackupRestore);
    COptions::ScanningThreads = m_ScanningThreads + 1;
    COptions::LargeFileCount = std::stoi(m_LargestFileCount.GetString());
    COptions::FolderHistoryCount = std::stoi(m_FolderHistoryCount.GetString());

    if (refreshAll)
    {
        CDirStatDoc::GetDocument()->OnOpenDocument(
            CDirStatDoc::GetDocument()->GetPathName().GetString());
    }
    else if (refreshReparsepoints)
    {
        CDirStatDoc::GetDocument()->RefreshReparsePointItems();
    }

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

    m_LargestFileCount = std::to_wstring(std::clamp(_wtoi(m_LargestFileCount),
        COptions::LargeFileCount.Min(), COptions::LargeFileCount.Max())).c_str();

    UpdateData(FALSE);
}

void CPageAdvanced::OnEnChangeFolderHistoryCount()
{
    // This function limits the value in the folder history count
    UpdateData(TRUE);

    m_FolderHistoryCount = std::to_wstring(std::clamp(_wtoi(m_FolderHistoryCount),
        COptions::FolderHistoryCount.Min(), COptions::FolderHistoryCount.Max())).c_str();

    UpdateData(FALSE);
}
