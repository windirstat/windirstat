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
#include "WinDirStat.h"
#include "MainFrame.h" // COptionsPropertySheet
#include "PageAdvanced.h"

#include "DirStatDoc.h"
#include "Options.h"
#include "Localization.h"

IMPLEMENT_DYNAMIC(CPageAdvanced, CPropertyPage)

CPageAdvanced::CPageAdvanced() : CPropertyPage(CPageAdvanced::IDD) {}

CPageAdvanced::~CPageAdvanced() = default;

COptionsPropertySheet* CPageAdvanced::GetSheet() const
{
    return DYNAMIC_DOWNCAST(COptionsPropertySheet, GetParent());
}

void CPageAdvanced::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPage::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_EXCLUDE_VOLUME_MOUNT_POINTS, m_excludeVolumeMountPoints);
    DDX_Check(pDX, IDC_EXCLUDE_JUNCTIONS, m_excludeJunctions);
    DDX_Check(pDX, IDC_EXCLUDE_SYMLINKS, m_excludeSymbolicLinks);
    DDX_Check(pDX, IDC_PAGE_ADVANCED_SKIP_CLOUD_LINKS, m_skipDupeDetectionCloudLinks);
    DDX_Check(pDX, IDC_SKIPHIDDEN, m_skipHidden);
    DDX_Check(pDX, IDC_SKIPPROTECTED, m_skipProtected);
    DDX_Check(pDX, IDC_BACKUP_RESTORE, m_useBackupRestore);
    DDX_CBIndex(pDX, IDC_COMBO_THREADS, m_scanningThreads);
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

    m_excludeVolumeMountPoints = COptions::ExcludeVolumeMountPoints;
    m_excludeJunctions = COptions::ExcludeJunctions;
    m_excludeSymbolicLinks = COptions::ExcludeSymbolicLinks;
    m_skipDupeDetectionCloudLinks = COptions::SkipDuplicationDetectionCloudLinks;
    m_skipHidden = COptions::SkipHidden;
    m_skipProtected = COptions::SkipProtected;
    m_useBackupRestore = COptions::UseBackupRestore;
    m_scanningThreads = COptions::ScanningThreads - 1;

    UpdateData(false);
    return TRUE;
}

void CPageAdvanced::OnOK()
{
    UpdateData();

    const bool refresh_reprasepoints =
        COptions::ExcludeJunctions && COptions::ExcludeJunctions != static_cast<bool>(m_excludeJunctions) ||
        COptions::ExcludeSymbolicLinks && COptions::ExcludeSymbolicLinks != static_cast<bool>(m_excludeSymbolicLinks) ||
        COptions::ExcludeVolumeMountPoints && COptions::ExcludeVolumeMountPoints != static_cast<bool>(m_excludeVolumeMountPoints);
    const bool refresh_all = COptions::SkipHidden != static_cast<bool>(m_skipHidden) ||
        COptions::SkipProtected != static_cast<bool>(m_skipProtected);

    COptions::ExcludeJunctions = (FALSE != m_excludeJunctions);
    COptions::ExcludeSymbolicLinks = (FALSE != m_excludeSymbolicLinks);
    COptions::ExcludeVolumeMountPoints = (FALSE != m_excludeVolumeMountPoints);
    COptions::SkipDuplicationDetectionCloudLinks = (FALSE != m_skipDupeDetectionCloudLinks);
    COptions::SkipHidden = (FALSE != m_skipHidden);
    COptions::SkipProtected = (FALSE != m_skipProtected);
    COptions::UseBackupRestore = (FALSE != m_useBackupRestore);
    COptions::ScanningThreads = m_scanningThreads + 1;

    if (refresh_all)
    {
        GetDocument()->RefreshItem(GetDocument()->GetRootItem());
    }
    else if (refresh_reprasepoints)
    {
        GetDocument()->RefreshReparsePointItems();
    }

    CPropertyPage::OnOK();
}

void CPageAdvanced::OnSettingChanged()
{
    SetModified();
}
