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
    DDX_Control(pDX, IDC_LIST, m_List);
    DDX_Check(pDX, IDC_ENABLED, m_Enabled);
    DDX_Text(pDX, IDC_TITLE, m_Title);
    DDX_Check(pDX, IDC_WORKSFORDRIVES, m_WorksForDrives);
    DDX_Check(pDX, IDC_WORKSFORDIRECTORIES, m_WorksForDirectories);
    DDX_Check(pDX, IDC_WORKSFORFILES, m_WorksForFiles);
    DDX_Check(pDX, IDC_WORKSFORUNCPATHS, m_WorksForUncPaths);
    DDX_Text(pDX, IDC_COMMANDLINE, m_CommandLine);
    DDX_Check(pDX, IDC_RECURSEINTOSUBDIRECTORIES, m_RecurseIntoSubdirectories);
    DDX_Check(pDX, IDC_ASKFORCONFIRMATION, m_AskForConfirmation);
    DDX_Check(pDX, IDC_SHOWCONSOLEWINDOW, m_ShowConsoleWindow);
    DDX_Check(pDX, IDC_WAITFORCOMPLETION, m_WaitForCompletion);
    DDX_Control(pDX, IDC_REFRESHPOLICY, m_CtlRefreshPolicy);
    DDX_CBIndex(pDX, IDC_REFRESHPOLICY, m_RefreshPolicy);

    DDX_Control(pDX, IDC_TITLE, m_CtlTitle);
    DDX_Control(pDX, IDC_WORKSFORDRIVES, m_CtlWorksForDrives);
    DDX_Control(pDX, IDC_WORKSFORDIRECTORIES, m_CtlWorksForDirectories);
    DDX_Control(pDX, IDC_WORKSFORFILES, m_CtlWorksForFiles);
    DDX_Control(pDX, IDC_WORKSFORUNCPATHS, m_CtlWorksForUncPaths);
    DDX_Control(pDX, IDC_COMMANDLINE, m_CtlCommandLine);
    DDX_Control(pDX, IDC_RECURSEINTOSUBDIRECTORIES, m_CtlRecurseIntoSubdirectories);
    DDX_Control(pDX, IDC_ASKFORCONFIRMATION, m_CtlAskForConfirmation);
    DDX_Control(pDX, IDC_SHOWCONSOLEWINDOW, m_CtlShowConsoleWindow);
    DDX_Control(pDX, IDC_WAITFORCOMPLETION, m_CtlWaitForCompletion);
    DDX_Control(pDX, IDC_HINTSP, m_CtlHintSp);
    DDX_Control(pDX, IDC_HINTSN, m_CtlHintSn);
    DDX_Control(pDX, IDC_UP, m_CtlUp);
    DDX_Control(pDX, IDC_DOWN, m_CtlDown);
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
    m_CtlRefreshPolicy.AddString(Localization::Lookup(IDS_POLICY_NOREFRESH).c_str());
    m_CtlRefreshPolicy.AddString(Localization::Lookup(IDS_POLICY_REFRESHTHISENTRY).c_str());
    m_CtlRefreshPolicy.AddString(Localization::Lookup(IDS_POLICY_REFRESHPARENT).c_str());

    for (size_t i = 0; i < COptions::UserDefinedCleanups.size(); i++)
    {
        m_Udc[i] = COptions::UserDefinedCleanups[i];
        m_List.AddString(m_Udc[i].title.Obj().c_str());
    }

    m_List.SetCurSel(0);
    OnLbnSelchangeList();

    return TRUE; // return TRUE unless you set the focus to a control
}

void CPageCleanups::OnOK()
{
    CheckEmptyTitle();

    for (size_t i = 0; i < COptions::UserDefinedCleanups.size(); i++)
    {
        COptions::UserDefinedCleanups[i].askForConfirmation = m_Udc[i].askForConfirmation.Obj();
        COptions::UserDefinedCleanups[i].commandLine = m_Udc[i].commandLine.Obj();
        COptions::UserDefinedCleanups[i].enabled = m_Udc[i].enabled.Obj();
        COptions::UserDefinedCleanups[i].recurseIntoSubdirectories = m_Udc[i].recurseIntoSubdirectories.Obj();
        COptions::UserDefinedCleanups[i].refreshPolicy = m_Udc[i].refreshPolicy.Obj();
        COptions::UserDefinedCleanups[i].showConsoleWindow = m_Udc[i].showConsoleWindow.Obj();
        COptions::UserDefinedCleanups[i].title = m_Udc[i].title.Obj();
        COptions::UserDefinedCleanups[i].virginTitle = m_Udc[i].virginTitle.Obj();
        COptions::UserDefinedCleanups[i].waitForCompletion = m_Udc[i].waitForCompletion.Obj();
        COptions::UserDefinedCleanups[i].worksForDirectories = m_Udc[i].worksForDirectories.Obj();
        COptions::UserDefinedCleanups[i].worksForDrives = m_Udc[i].worksForDrives.Obj();
        COptions::UserDefinedCleanups[i].worksForFiles = m_Udc[i].worksForFiles.Obj();
        COptions::UserDefinedCleanups[i].worksForUncPaths = m_Udc[i].worksForUncPaths.Obj();
    }

    CPropertyPage::OnOK();
}

void CPageCleanups::OnLbnSelchangeList()
{
    CheckEmptyTitle();

    m_Current = m_List.GetCurSel();
    if (m_Current < 0 || m_Current >= USERDEFINEDCLEANUPCOUNT)
    {
        m_Current = USERDEFINEDCLEANUPCOUNT - 1;
        m_List.SetCurSel(m_Current);
    }
    CurrentUdcToDialog();
}

void CPageCleanups::CheckEmptyTitle()
{
    if (m_Current == -1)
    {
        return;
    }

    UpdateData();
    if (m_Title.IsEmpty())
    {
        m_Title.FormatMessage(Localization::Lookup(IDS_USERDEFINEDCLEANUPd).c_str(), m_Current);
        UpdateData(false);

        m_List.DeleteString(m_Current);
        m_List.InsertString(m_Current, m_Title);

        DialogToCurrentUdc();
        m_Udc[m_Current].virginTitle = true;
    }
}

void CPageCleanups::CurrentUdcToDialog()
{
    m_AskForConfirmation        = m_Udc[m_Current].askForConfirmation;
    m_CommandLine               = m_Udc[m_Current].commandLine.Obj().c_str();
    m_Enabled                   = m_Udc[m_Current].enabled;
    m_RecurseIntoSubdirectories = m_Udc[m_Current].recurseIntoSubdirectories;
    m_RefreshPolicy             = m_Udc[m_Current].refreshPolicy;
    m_ShowConsoleWindow         = m_Udc[m_Current].showConsoleWindow;
    m_Title                     = m_Udc[m_Current].title.Obj().c_str();
    m_WaitForCompletion         = m_Udc[m_Current].waitForCompletion;
    m_WorksForDirectories       = m_Udc[m_Current].worksForDirectories;
    m_WorksForDrives            = m_Udc[m_Current].worksForDrives;
    m_WorksForFiles             = m_Udc[m_Current].worksForFiles;
    m_WorksForUncPaths          = m_Udc[m_Current].worksForUncPaths;

    UpdateControlStatus();
    UpdateData(false);
}

void CPageCleanups::DialogToCurrentUdc()
{
    UpdateData();

    m_Udc[m_Current].enabled                   = FALSE != m_Enabled;
    m_Udc[m_Current].title.Obj()              = m_Title;
    m_Udc[m_Current].worksForDrives            = FALSE != m_WorksForDrives;
    m_Udc[m_Current].worksForDirectories       = FALSE != m_WorksForDirectories;
    m_Udc[m_Current].worksForFiles             = FALSE != m_WorksForFiles;
    m_Udc[m_Current].worksForUncPaths          = FALSE != m_WorksForUncPaths;
    m_Udc[m_Current].commandLine.Obj()       = m_CommandLine;
    m_Udc[m_Current].recurseIntoSubdirectories = FALSE != m_RecurseIntoSubdirectories;
    m_Udc[m_Current].askForConfirmation        = FALSE != m_AskForConfirmation;
    m_Udc[m_Current].showConsoleWindow         = FALSE != m_ShowConsoleWindow;
    m_Udc[m_Current].waitForCompletion         = FALSE != m_WaitForCompletion;
    m_Udc[m_Current].refreshPolicy             = m_RefreshPolicy;
}

void CPageCleanups::OnSomethingChanged()
{
    UpdateData();
    if (!m_WorksForDrives && !m_WorksForDirectories)
    {
        m_RecurseIntoSubdirectories = false;
    }
    if (!m_WorksForDrives && !m_WorksForDirectories)
    {
        m_RecurseIntoSubdirectories = false;
    }
    if (!m_WaitForCompletion)
    {
        m_RefreshPolicy = RP_NO_REFRESH;
    }
    if (m_RecurseIntoSubdirectories)
    {
        m_WaitForCompletion = true;
    }
    UpdateData(false);
    DialogToCurrentUdc();
    SetModified();
}

void CPageCleanups::UpdateControlStatus()
{
    m_CtlTitle.EnableWindow(m_Enabled);
    m_CtlWorksForDrives.EnableWindow(m_Enabled);
    m_CtlWorksForDirectories.EnableWindow(m_Enabled);
    m_CtlWorksForFiles.EnableWindow(m_Enabled);
    m_CtlWorksForUncPaths.EnableWindow(m_Enabled);
    m_CtlCommandLine.EnableWindow(m_Enabled);
    m_CtlRecurseIntoSubdirectories.EnableWindow(m_Enabled && (m_WorksForDrives || m_WorksForDirectories));
    m_CtlAskForConfirmation.EnableWindow(m_Enabled);
    m_CtlShowConsoleWindow.EnableWindow(m_Enabled);
    m_CtlWaitForCompletion.EnableWindow(m_Enabled && !m_RecurseIntoSubdirectories);
    m_CtlRefreshPolicy.EnableWindow(m_Enabled);

    m_CtlHintSp.ShowWindow(m_RecurseIntoSubdirectories ? SW_SHOW : SW_HIDE);
    m_CtlHintSn.ShowWindow(m_RecurseIntoSubdirectories ? SW_SHOW : SW_HIDE);

    m_CtlUp.EnableWindow(m_Current > 0);
    m_CtlDown.EnableWindow(m_Current < USERDEFINEDCLEANUPCOUNT - 1);
}

void CPageCleanups::OnBnClickedEnabled()
{
    OnSomethingChanged();
    UpdateControlStatus();
    if (m_Enabled)
    {
        m_CtlTitle.SetFocus();
        m_CtlTitle.SetSel(0, -1, true);
    }
    else
    {
        m_List.SetFocus();
    }
}

void CPageCleanups::OnEnChangeTitle()
{
    OnSomethingChanged();
    m_Udc[m_Current].virginTitle = false;
    m_List.DeleteString(m_Current);
    m_List.InsertString(m_Current, m_Title);
    m_List.SetCurSel(m_Current);
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
    ASSERT(m_Current > 0);

    UpdateData();

    const USERDEFINEDCLEANUP h = m_Udc[m_Current - 1];
    m_Udc[m_Current - 1] = m_Udc[m_Current];
    m_Udc[m_Current] = h;

    m_List.DeleteString(m_Current);
    m_List.InsertString(m_Current - 1, m_Title);

    m_Current--;
    m_List.SetCurSel(m_Current);

    SetModified();
    UpdateControlStatus();
}

void CPageCleanups::OnBnClickedDown()
{
    ASSERT(m_Current < USERDEFINEDCLEANUPCOUNT - 1);

    UpdateData();

    const USERDEFINEDCLEANUP h = m_Udc[m_Current + 1];
    m_Udc[m_Current + 1] = m_Udc[m_Current];
    m_Udc[m_Current] = h;

    m_List.DeleteString(m_Current);
    m_List.InsertString(m_Current + 1, m_Title);

    m_Current++;
    m_List.SetCurSel(m_Current);

    SetModified();
    UpdateControlStatus();
}

void CPageCleanups::OnBnClickedHelpbutton()
{
    CDirStatApp::LaunchHelp();
}
