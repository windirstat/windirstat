// PageCleanups.cpp - Implementation of CPageCleanups
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
#include "Options.h"
#include "PageCleanups.h"

#include "Localization.h"

IMPLEMENT_DYNAMIC(CPageCleanups, CPropertyPage)

CPageCleanups::CPageCleanups() : CPropertyPage(CPageCleanups::IDD) {}

CPageCleanups::~CPageCleanups() = default;

void CPageCleanups::DoDataExchange(CDataExchange* pDX)
{
    CPropertyPage::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_LIST, m_list);
    DDX_Check(pDX, IDC_ENABLED, m_enabled);
    DDX_Text(pDX, IDC_TITLE, m_title);
    DDX_Check(pDX, IDC_WORKSFORDRIVES, m_worksForDrives);
    DDX_Check(pDX, IDC_WORKSFORDIRECTORIES, m_worksForDirectories);
    DDX_Check(pDX, IDC_WORKSFORFILES, m_worksForFiles);
    DDX_Check(pDX, IDC_WORKSFORUNCPATHS, m_worksForUncPaths);
    DDX_Text(pDX, IDC_COMMANDLINE, m_commandLine);
    DDX_Check(pDX, IDC_RECURSEINTOSUBDIRECTORIES, m_recurseIntoSubdirectories);
    DDX_Check(pDX, IDC_ASKFORCONFIRMATION, m_askForConfirmation);
    DDX_Check(pDX, IDC_SHOWCONSOLEWINDOW, m_showConsoleWindow);
    DDX_Check(pDX, IDC_WAITFORCOMPLETION, m_waitForCompletion);
    DDX_Control(pDX, IDC_REFRESHPOLICY, m_ctlRefreshPolicy);
    DDX_CBIndex(pDX, IDC_REFRESHPOLICY, m_refreshPolicy);

    DDX_Control(pDX, IDC_TITLE, m_ctlTitle);
    DDX_Control(pDX, IDC_WORKSFORDRIVES, m_ctlWorksForDrives);
    DDX_Control(pDX, IDC_WORKSFORDIRECTORIES, m_ctlWorksForDirectories);
    DDX_Control(pDX, IDC_WORKSFORFILES, m_ctlWorksForFiles);
    DDX_Control(pDX, IDC_WORKSFORUNCPATHS, m_ctlWorksForUncPaths);
    DDX_Control(pDX, IDC_COMMANDLINE, m_ctlCommandLine);
    DDX_Control(pDX, IDC_RECURSEINTOSUBDIRECTORIES, m_ctlRecurseIntoSubdirectories);
    DDX_Control(pDX, IDC_ASKFORCONFIRMATION, m_ctlAskForConfirmation);
    DDX_Control(pDX, IDC_SHOWCONSOLEWINDOW, m_ctlShowConsoleWindow);
    DDX_Control(pDX, IDC_WAITFORCOMPLETION, m_ctlWaitForCompletion);
    DDX_Control(pDX, IDC_HINTSP, m_ctlHintSp);
    DDX_Control(pDX, IDC_HINTSN, m_ctlHintSn);
    DDX_Control(pDX, IDC_UP, m_ctlUp);
    DDX_Control(pDX, IDC_DOWN, m_ctlDown);
}

BEGIN_MESSAGE_MAP(CPageCleanups, CPropertyPage)
    ON_LBN_SELCHANGE(IDC_LIST, OnLbnSelchangeList)
    ON_BN_CLICKED(IDC_ENABLED, OnBnClickedEnabled)
    ON_EN_CHANGE(IDC_TITLE, OnEnChangeTitle)
    ON_BN_CLICKED(IDC_WORKSFORDRIVES, OnBnClickedWorksfordrives)
    ON_BN_CLICKED(IDC_WORKSFORDIRECTORIES, OnBnClickedWorksfordirectories)
    ON_BN_CLICKED(IDC_WORKSFORFILES, OnBnClickedModified)
    ON_BN_CLICKED(IDC_WORKSFORUNCPATHS, OnBnClickedModified)
    ON_EN_CHANGE(IDC_COMMANDLINE, OnBnClickedModified)
    ON_BN_CLICKED(IDC_RECURSEINTOSUBDIRECTORIES, OnBnClickedRecurseintosubdirectories)
    ON_BN_CLICKED(IDC_ASKFORCONFIRMATION, OnBnClickedModified)
    ON_BN_CLICKED(IDC_SHOWCONSOLEWINDOW, OnBnClickedModified)
    ON_BN_CLICKED(IDC_WAITFORCOMPLETION, OnBnClickedModified)
    ON_CBN_SELENDOK(IDC_REFRESHPOLICY, OnBnClickedModified)
    ON_BN_CLICKED(IDC_UP, OnBnClickedUp)
    ON_BN_CLICKED(IDC_DOWN, OnBnClickedDown)
    ON_BN_CLICKED(IDC_HELPBUTTON, OnBnClickedHelpbutton)
END_MESSAGE_MAP()

BOOL CPageCleanups::OnInitDialog()
{
    CPropertyPage::OnInitDialog();

    Localization::UpdateDialogs(*this);

    // Combobox data correspond to enum REFRESHPOLICY:
    m_ctlRefreshPolicy.AddString(Localization::Lookup(IDS_POLICY_NOREFRESH));
    m_ctlRefreshPolicy.AddString(Localization::Lookup(IDS_POLICY_REFRESHTHISENTRY));
    m_ctlRefreshPolicy.AddString(Localization::Lookup(IDS_POLICY_REFRESHPARENT));

    for (size_t i = 0; i < COptions::UserDefinedCleanups.size(); i++)
    {
        m_udc[i] = COptions::UserDefinedCleanups[i];
        m_list.AddString(m_udc[i].title.Obj().c_str());
    }

    m_list.SetCurSel(0);
    OnLbnSelchangeList();

    return TRUE; // return TRUE unless you set the focus to a control
}

void CPageCleanups::OnOK()
{
    CheckEmptyTitle();

    for (size_t i = 0; i < COptions::UserDefinedCleanups.size(); i++)
    {
        COptions::UserDefinedCleanups[i].askForConfirmation = m_udc[i].askForConfirmation.Obj();
        COptions::UserDefinedCleanups[i].commandLine = m_udc[i].commandLine.Obj();
        COptions::UserDefinedCleanups[i].enabled = m_udc[i].enabled.Obj();
        COptions::UserDefinedCleanups[i].recurseIntoSubdirectories = m_udc[i].recurseIntoSubdirectories.Obj();
        COptions::UserDefinedCleanups[i].refreshPolicy = m_udc[i].refreshPolicy.Obj();
        COptions::UserDefinedCleanups[i].showConsoleWindow = m_udc[i].showConsoleWindow.Obj();
        COptions::UserDefinedCleanups[i].title = m_udc[i].title.Obj();
        COptions::UserDefinedCleanups[i].virginTitle = m_udc[i].virginTitle.Obj();
        COptions::UserDefinedCleanups[i].waitForCompletion = m_udc[i].waitForCompletion.Obj();
        COptions::UserDefinedCleanups[i].worksForDirectories = m_udc[i].worksForDirectories.Obj();
        COptions::UserDefinedCleanups[i].worksForDrives = m_udc[i].worksForDrives.Obj();
        COptions::UserDefinedCleanups[i].worksForFiles = m_udc[i].worksForFiles.Obj();
        COptions::UserDefinedCleanups[i].worksForUncPaths = m_udc[i].worksForUncPaths.Obj();
    }

    CPropertyPage::OnOK();
}

void CPageCleanups::OnLbnSelchangeList()
{
    CheckEmptyTitle();

    m_current = m_list.GetCurSel();
    if (m_current < 0 || m_current >= USERDEFINEDCLEANUPCOUNT)
    {
        m_current = USERDEFINEDCLEANUPCOUNT - 1;
        m_list.SetCurSel(m_current);
    }
    CurrentUdcToDialog();
}

void CPageCleanups::CheckEmptyTitle()
{
    if (m_current == -1)
    {
        return;
    }

    UpdateData();
    if (m_title.IsEmpty())
    {
        m_title.FormatMessage(Localization::Lookup(IDS_USERDEFINEDCLEANUPd), m_current);
        UpdateData(false);

        m_list.DeleteString(m_current);
        m_list.InsertString(m_current, m_title);

        DialogToCurrentUdc();
        m_udc[m_current].virginTitle = true;
    }
}

void CPageCleanups::CurrentUdcToDialog()
{
    m_askForConfirmation        = m_udc[m_current].askForConfirmation;
    m_commandLine               = m_udc[m_current].commandLine.Obj().c_str();
    m_enabled                   = m_udc[m_current].enabled;
    m_recurseIntoSubdirectories = m_udc[m_current].recurseIntoSubdirectories;
    m_refreshPolicy             = m_udc[m_current].refreshPolicy;
    m_showConsoleWindow         = m_udc[m_current].showConsoleWindow;
    m_title                     = m_udc[m_current].title.Obj().c_str();
    m_waitForCompletion         = m_udc[m_current].waitForCompletion;
    m_worksForDirectories       = m_udc[m_current].worksForDirectories;
    m_worksForDrives            = m_udc[m_current].worksForDrives;
    m_worksForFiles             = m_udc[m_current].worksForFiles;
    m_worksForUncPaths          = m_udc[m_current].worksForUncPaths;

    UpdateControlStatus();
    UpdateData(false);
}

void CPageCleanups::DialogToCurrentUdc()
{
    UpdateData();

    m_udc[m_current].enabled                   = FALSE != m_enabled;
    m_udc[m_current].title                     = std::wstring(m_title.GetString());
    m_udc[m_current].worksForDrives            = FALSE != m_worksForDrives;
    m_udc[m_current].worksForDirectories       = FALSE != m_worksForDirectories;
    m_udc[m_current].worksForFiles             = FALSE != m_worksForFiles;
    m_udc[m_current].worksForUncPaths          = FALSE != m_worksForUncPaths;
    m_udc[m_current].commandLine               = std::wstring(m_commandLine.GetString());
    m_udc[m_current].recurseIntoSubdirectories = FALSE != m_recurseIntoSubdirectories;
    m_udc[m_current].askForConfirmation        = FALSE != m_askForConfirmation;
    m_udc[m_current].showConsoleWindow         = FALSE != m_showConsoleWindow;
    m_udc[m_current].waitForCompletion         = FALSE != m_waitForCompletion;
    m_udc[m_current].refreshPolicy             = m_refreshPolicy;
}

void CPageCleanups::OnSomethingChanged()
{
    UpdateData();
    if (!m_worksForDrives && !m_worksForDirectories)
    {
        m_recurseIntoSubdirectories = false;
    }
    if (!m_worksForDrives && !m_worksForDirectories)
    {
        m_recurseIntoSubdirectories = false;
    }
    if (!m_waitForCompletion)
    {
        m_refreshPolicy = RP_NO_REFRESH;
    }
    if (m_recurseIntoSubdirectories)
    {
        m_waitForCompletion = true;
    }
    UpdateData(false);
    DialogToCurrentUdc();
    SetModified();
}

void CPageCleanups::UpdateControlStatus()
{
    m_ctlTitle.EnableWindow(m_enabled);
    m_ctlWorksForDrives.EnableWindow(m_enabled);
    m_ctlWorksForDirectories.EnableWindow(m_enabled);
    m_ctlWorksForFiles.EnableWindow(m_enabled);
    m_ctlWorksForUncPaths.EnableWindow(m_enabled);
    m_ctlCommandLine.EnableWindow(m_enabled);
    m_ctlRecurseIntoSubdirectories.EnableWindow(m_enabled && (m_worksForDrives || m_worksForDirectories));
    m_ctlAskForConfirmation.EnableWindow(m_enabled);
    m_ctlShowConsoleWindow.EnableWindow(m_enabled);
    m_ctlWaitForCompletion.EnableWindow(m_enabled && !m_recurseIntoSubdirectories);
    m_ctlRefreshPolicy.EnableWindow(m_enabled);

    m_ctlHintSp.ShowWindow(m_recurseIntoSubdirectories ? SW_SHOW : SW_HIDE);
    m_ctlHintSn.ShowWindow(m_recurseIntoSubdirectories ? SW_SHOW : SW_HIDE);

    m_ctlUp.EnableWindow(m_current > 0);
    m_ctlDown.EnableWindow(m_current < USERDEFINEDCLEANUPCOUNT - 1);
}

void CPageCleanups::OnBnClickedEnabled()
{
    OnSomethingChanged();
    UpdateControlStatus();
    if (m_enabled)
    {
        m_ctlTitle.SetFocus();
        m_ctlTitle.SetSel(0, -1, true);
    }
    else
    {
        m_list.SetFocus();
    }
}

void CPageCleanups::OnEnChangeTitle()
{
    OnSomethingChanged();
    m_udc[m_current].virginTitle = false;
    m_list.DeleteString(m_current);
    m_list.InsertString(m_current, m_title);
    m_list.SetCurSel(m_current);
}

void CPageCleanups::OnBnClickedWorksfordrives()
{
    OnSomethingChanged();
    UpdateControlStatus();
}

void CPageCleanups::OnBnClickedWorksfordirectories()
{
    OnSomethingChanged();
    UpdateControlStatus();
}

void CPageCleanups::OnBnClickedModified()
{
    OnSomethingChanged();
}

void CPageCleanups::OnBnClickedRecurseintosubdirectories()
{
    OnSomethingChanged();
    UpdateControlStatus();
}

void CPageCleanups::OnBnClickedUp()
{
    ASSERT(m_current > 0);

    UpdateData();

    const USERDEFINEDCLEANUP h = m_udc[m_current - 1];
    m_udc[m_current - 1] = m_udc[m_current];
    m_udc[m_current] = h;

    m_list.DeleteString(m_current);
    m_list.InsertString(m_current - 1, m_title);

    m_current--;
    m_list.SetCurSel(m_current);

    SetModified();
    UpdateControlStatus();
}

void CPageCleanups::OnBnClickedDown()
{
    ASSERT(m_current < USERDEFINEDCLEANUPCOUNT - 1);

    UpdateData();

    const USERDEFINEDCLEANUP h = m_udc[m_current + 1];
    m_udc[m_current + 1] = m_udc[m_current];
    m_udc[m_current] = h;

    m_list.DeleteString(m_current);
    m_list.InsertString(m_current + 1, m_title);

    m_current++;
    m_list.SetCurSel(m_current);

    SetModified();
    UpdateControlStatus();
}

void CPageCleanups::OnBnClickedHelpbutton()
{
    GetWDSApp()->LaunchHelp();
}
