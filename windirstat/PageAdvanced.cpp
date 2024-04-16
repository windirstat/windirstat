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
    DDX_Check(pDX, IDC_FOLLOWMOUNTPOINTS, m_followMountPoints);
    DDX_Check(pDX, IDC_FOLLOWJUNCTIONS, m_followJunctions);
    DDX_Control(pDX, IDC_FOLLOWMOUNTPOINTS, m_ctlFollowMountPoints);
    DDX_Control(pDX, IDC_FOLLOWJUNCTIONS, m_ctlFollowJunctions);
    DDX_Check(pDX, IDC_SKIPHIDDEN, m_skipHidden);
    DDX_Check(pDX, IDC_SKIPPROTECTED, m_skipProtected);
    DDX_Check(pDX, IDC_BACKUP_RESTORE, m_useBackupRestore);
    DDX_Check(pDX, IDC_UNCOMPRESSED_FILE_SIZES, m_showUncompressedFileSizes);
    DDX_CBIndex(pDX, IDC_COMBO_THREADS, m_scanningThreads);
}

BEGIN_MESSAGE_MAP(CPageAdvanced, CPropertyPage)
    ON_BN_CLICKED(IDC_FOLLOWMOUNTPOINTS, OnSettingChanged)
    ON_BN_CLICKED(IDC_FOLLOWJUNCTIONS, OnSettingChanged)
    ON_BN_CLICKED(IDC_BACKUP_RESTORE, OnSettingChanged)
    ON_BN_CLICKED(IDC_UNCOMPRESSED_FILE_SIZES, OnSettingChanged)
    ON_CBN_SELENDOK(IDC_COMBO_THREADS, OnSettingChanged)
    ON_BN_CLICKED(IDC_SKIPHIDDEN, OnSettingChanged)
    ON_BN_CLICKED(IDC_SKIPPROTECTED, OnSettingChanged)
END_MESSAGE_MAP()

BOOL CPageAdvanced::OnInitDialog()
{
    CPropertyPage::OnInitDialog();

    Localization::UpdateDialogs(*this);

    m_followMountPoints = COptions::FollowMountPoints;
    m_followJunctions = COptions::FollowJunctions;
    m_skipHidden = COptions::SkipHidden;
    m_skipProtected = COptions::SkipProtected;
    m_useBackupRestore = COptions::UseBackupRestore;
    m_showUncompressedFileSizes = COptions::ShowUncompressedFileSizes;
    m_scanningThreads = COptions::ScanningThreads - 1;

    UpdateData(false);
    return TRUE;
}

void CPageAdvanced::OnOK()
{
    UpdateData();

    const bool refresh_reprasepoints =
        m_followMountPoints && COptions::FollowMountPoints != static_cast<bool>(m_followMountPoints) ||
        m_followJunctions && COptions::FollowJunctions != static_cast<bool>(m_followJunctions);
    const bool refresh_all = COptions::ShowUncompressedFileSizes != static_cast<bool>(m_showUncompressedFileSizes);

    COptions::FollowMountPoints = (FALSE != m_followMountPoints);
    COptions::FollowJunctions = (FALSE != m_followJunctions);
    COptions::SkipHidden = (FALSE != m_skipHidden);
    COptions::SkipProtected = (FALSE != m_skipProtected);
    COptions::UseBackupRestore = (FALSE != m_useBackupRestore);
    COptions::ShowUncompressedFileSizes = (FALSE != m_showUncompressedFileSizes);
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
