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
#include "Options.h"
#include "GlobalHelpers.h"

IMPLEMENT_DYNAMIC(CPageAdvanced, CPropertyPage)

CPageAdvanced::CPageAdvanced()
    : CPropertyPage(CPageAdvanced::IDD)
    , m_followMountPoints(FALSE)
    , m_followJunctionPoints(FALSE)
    , m_skipHidden(FALSE)
    , m_useBackupRestore(FALSE)
    , m_scanningThreads(0)
{
}

CPageAdvanced::~CPageAdvanced() = default;

COptionsPropertySheet* CPageAdvanced::GetSheet() const
{
    return DYNAMIC_DOWNCAST(COptionsPropertySheet, GetParent());
}

void CPageAdvanced::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPage::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_FOLLOWMOUNTPOINTS, m_followMountPoints);
    DDX_Check(pDX, IDC_FOLLOWJUNCTIONS, m_followJunctionPoints);
    DDX_Control(pDX, IDC_FOLLOWMOUNTPOINTS, m_ctlFollowMountPoints);
    DDX_Control(pDX, IDC_FOLLOWJUNCTIONS, m_ctlFollowJunctionPoints);
    DDX_Check(pDX, IDC_SKIPHIDDEN, m_skipHidden);
    DDX_Check(pDX, IDC_BACKUP_RESTORE, m_useBackupRestore);
    DDX_CBIndex(pDX, IDC_COMBO_THREADS, m_scanningThreads);
}

BEGIN_MESSAGE_MAP(CPageAdvanced, CPropertyPage)
    ON_BN_CLICKED(IDC_FOLLOWMOUNTPOINTS, OnBnClickedFollowmountpoints)
    ON_BN_CLICKED(IDC_FOLLOWJUNCTIONS, OnBnClickedFollowjunctionpoints)
    ON_CBN_SELENDOK(IDC_COMBO_THREADS, OnCbnSelThreadsCombo)
    ON_BN_CLICKED(IDC_SKIPHIDDEN, OnBnClickedSkipHidden)
END_MESSAGE_MAP()

BOOL CPageAdvanced::OnInitDialog()
{
    CPropertyPage::OnInitDialog();

    m_followMountPoints = GetOptions()->IsFollowMountPoints();
    m_followJunctionPoints = GetOptions()->IsFollowJunctionPoints();
    m_skipHidden = GetOptions()->IsSkipHidden();
    m_useBackupRestore = GetOptions()->IsUseBackupRestore();
    m_scanningThreads = GetOptions()->GetScanningThreads() - 1;

    UpdateData(false);
    return TRUE;
}

void CPageAdvanced::OnOK()
{
    UpdateData();
    GetOptions()->SetFollowMountPoints(FALSE != m_followMountPoints);
    GetOptions()->SetFollowJunctionPoints(FALSE != m_followJunctionPoints);
    GetOptions()->SetSkipHidden(FALSE != m_skipHidden);
    GetOptions()->SetUseBackupRestore(FALSE != m_useBackupRestore);
    GetOptions()->SetScanningThreads(m_scanningThreads + 1);

    CPropertyPage::OnOK();
}

void CPageAdvanced::OnBnClickedFollowmountpoints()
{
    SetModified();
}

void CPageAdvanced::OnBnClickedFollowjunctionpoints()
{
    SetModified();
}

void CPageAdvanced::OnCbnSelThreadsCombo()
{
    SetModified();
}

void CPageAdvanced::OnBnClickedSkipHidden()
{
    SetModified();
}
