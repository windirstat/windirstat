// PageAdvanced.cpp - Implementation of CPageAdvanced
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "stdafx.h"
#include "MainFrame.h"
#include "PageAdvanced.h"
#include "DirStatDoc.h"
#include "Options.h"
#include "Localization.h"
#include "WinDirStat.h"

IMPLEMENT_DYNAMIC(CPageAdvanced, CPropertyPageEx)

CPageAdvanced::CPageAdvanced() : CPropertyPageEx(IDD) {}

CPageAdvanced::~CPageAdvanced() = default;

COptionsPropertySheet* CPageAdvanced::GetSheet() const
{
    return DYNAMIC_DOWNCAST(COptionsPropertySheet, GetParent());
}

void CPageAdvanced::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPageEx::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_EXCLUDE_VOLUME_MOUNT_POINTS, m_ExcludeVolumeMountPoints);
    DDX_Check(pDX, IDC_EXCLUDE_JUNCTIONS, m_ExcludeJunctions);
    DDX_Check(pDX, IDC_EXCLUDE_SYMLINKS_DIRECTORY, m_ExcludeSymbolicLinksDirectory);
    DDX_Check(pDX, IDC_PAGE_ADVANCED_SKIP_CLOUD_LINKS, m_SkipDupeDetectionCloudLinks);
    DDX_Check(pDX, IDC_EXCLUDE_HIDDEN_DIRECTORY, m_SkipHiddenDirectory);
    DDX_Check(pDX, IDC_EXCLUDE_PROTECTED_DIRECTORY, m_SkipProtectedDirectory);
    DDX_Check(pDX, IDC_BACKUP_RESTORE, m_UseBackupRestore);
    DDX_Check(pDX, IDC_EXCLUDE_SYMLINKS_FILE, m_ExcludeSymbolicLinksFile);
    DDX_Check(pDX, IDC_EXCLUDE_HIDDEN_FILE, m_SkipHiddenFile);
    DDX_Check(pDX, IDC_EXCLUDE_PROTECTED_FILE, m_SkipProtectedFile);
    DDX_Text(pDX, IDC_LARGEST_FILE_COUNT, m_LargestFileCount);
    DDX_CBIndex(pDX, IDC_COMBO_THREADS, m_ScanningThreads);
}

BEGIN_MESSAGE_MAP(CPageAdvanced, CPropertyPageEx)
    ON_BN_CLICKED(IDC_BACKUP_RESTORE, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_HIDDEN_DIRECTORY, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_PROTECTED_DIRECTORY, OnSettingChanged)
    ON_CBN_SELENDOK(IDC_COMBO_THREADS, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_VOLUME_MOUNT_POINTS, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_JUNCTIONS, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_SYMLINKS_DIRECTORY, OnSettingChanged)
    ON_BN_CLICKED(IDC_PAGE_ADVANCED_SKIP_CLOUD_LINKS, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_SYMLINKS_FILE, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_HIDDEN_FILE, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_PROTECTED_FILE, OnSettingChanged)
    ON_BN_CLICKED(IDC_RESET_PREFERENCES, &CPageAdvanced::OnBnClickedResetPreferences)
END_MESSAGE_MAP()

BOOL CPageAdvanced::OnInitDialog()
{
    CPropertyPageEx::OnInitDialog();

    Localization::UpdateDialogs(*this);

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

    if (refreshAll)
    {
        CDirStatDoc::GetDocument()->RefreshItem(CDirStatDoc::GetDocument()->GetRootItem());
    }
    else if (refreshReparsepoints)
    {
        CDirStatDoc::GetDocument()->RefreshReparsePointItems();
    }

    CPropertyPageEx::OnOK();
}

void CPageAdvanced::OnSettingChanged()
{
    SetModified();
}

void CPageAdvanced::OnBnClickedResetPreferences()
{
    CDirStatApp::Get()->RestartApplication(true);
}
