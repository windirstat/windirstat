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

IMPLEMENT_DYNAMIC(CPageAdvanced, CPropertyPage)

CPageAdvanced::CPageAdvanced() : CPropertyPage(IDD) {}

CPageAdvanced::~CPageAdvanced() = default;

COptionsPropertySheet* CPageAdvanced::GetSheet() const
{
    return DYNAMIC_DOWNCAST(COptionsPropertySheet, GetParent());
}

void CPageAdvanced::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPage::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_EXCLUDE_VOLUME_MOUNT_POINTS, m_ExcludeVolumeMountPoints);
    DDX_Check(pDX, IDC_EXCLUDE_JUNCTIONS, m_ExcludeJunctions);
    DDX_Check(pDX, IDC_EXCLUDE_SYMLINKS, m_ExcludeSymbolicLinks);
    DDX_Check(pDX, IDC_PAGE_ADVANCED_SKIP_CLOUD_LINKS, m_SkipDupeDetectionCloudLinks);
    DDX_Check(pDX, IDC_SKIPHIDDEN, m_SkipHidden);
    DDX_Check(pDX, IDC_SKIPPROTECTED, m_SkipProtected);
    DDX_Check(pDX, IDC_BACKUP_RESTORE, m_UseBackupRestore);
    DDX_CBIndex(pDX, IDC_COMBO_THREADS, m_ScanningThreads);
}

BEGIN_MESSAGE_MAP(CPageAdvanced, CPropertyPage)
    ON_BN_CLICKED(IDC_BACKUP_RESTORE, OnSettingChanged)
    ON_BN_CLICKED(IDC_SKIPHIDDEN, OnSettingChanged)
    ON_BN_CLICKED(IDC_SKIPPROTECTED, OnSettingChanged)
    ON_CBN_SELENDOK(IDC_COMBO_THREADS, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_VOLUME_MOUNT_POINTS, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_JUNCTIONS, OnSettingChanged)
    ON_BN_CLICKED(IDC_EXCLUDE_SYMLINKS, OnSettingChanged)
    ON_BN_CLICKED(IDC_PAGE_ADVANCED_SKIP_CLOUD_LINKS, OnSettingChanged)
END_MESSAGE_MAP()

BOOL CPageAdvanced::OnInitDialog()
{
    CPropertyPage::OnInitDialog();

    Localization::UpdateDialogs(*this);

    m_ExcludeVolumeMountPoints = COptions::ExcludeVolumeMountPoints;
    m_ExcludeJunctions = COptions::ExcludeJunctions;
    m_ExcludeSymbolicLinks = COptions::ExcludeSymbolicLinks;
    m_SkipDupeDetectionCloudLinks = COptions::SkipDupeDetectionCloudLinks;
    m_SkipHidden = COptions::SkipHidden;
    m_SkipProtected = COptions::SkipProtected;
    m_UseBackupRestore = COptions::UseBackupRestore;
    m_ScanningThreads = COptions::ScanningThreads - 1;

    UpdateData(FALSE);
    return TRUE;
}

void CPageAdvanced::OnOK()
{
    UpdateData();

    const bool refreshReparsepoints =
        COptions::ExcludeJunctions && COptions::ExcludeJunctions != static_cast<bool>(m_ExcludeJunctions) ||
        COptions::ExcludeSymbolicLinks && COptions::ExcludeSymbolicLinks != static_cast<bool>(m_ExcludeSymbolicLinks) ||
        COptions::ExcludeVolumeMountPoints && COptions::ExcludeVolumeMountPoints != static_cast<bool>(m_ExcludeVolumeMountPoints);
    const bool refreshAll = COptions::SkipHidden != static_cast<bool>(m_SkipHidden) ||
        COptions::SkipProtected != static_cast<bool>(m_SkipProtected);

    COptions::ExcludeJunctions = (FALSE != m_ExcludeJunctions);
    COptions::ExcludeSymbolicLinks = (FALSE != m_ExcludeSymbolicLinks);
    COptions::ExcludeVolumeMountPoints = (FALSE != m_ExcludeVolumeMountPoints);
    COptions::SkipDupeDetectionCloudLinks = (FALSE != m_SkipDupeDetectionCloudLinks);
    COptions::SkipHidden = (FALSE != m_SkipHidden);
    COptions::SkipProtected = (FALSE != m_SkipProtected);
    COptions::UseBackupRestore = (FALSE != m_UseBackupRestore);
    COptions::ScanningThreads = m_ScanningThreads + 1;

    if (refreshAll)
    {
        CDirStatDoc::GetDocument()->RefreshItem(CDirStatDoc::GetDocument()->GetRootItem());
    }
    else if (refreshReparsepoints)
    {
        CDirStatDoc::GetDocument()->RefreshReparsePointItems();
    }

    CPropertyPage::OnOK();
}

void CPageAdvanced::OnSettingChanged()
{
    SetModified();
}
