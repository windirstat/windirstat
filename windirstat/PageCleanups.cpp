// PageCleanups.cpp	- Implementation of CPageCleanups
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003 Bernhard Seifert
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
// Author: bseifert@users.sourceforge.net, bseifert@daccord.net

#include "stdafx.h"
#include "windirstat.h"
#include ".\pagecleanups.h"


IMPLEMENT_DYNAMIC(CPageCleanups, CPropertyPage)

CPageCleanups::CPageCleanups()
	: CPropertyPage(CPageCleanups::IDD)
	, m_enabled(FALSE)
	, m_title(_T(""))
	, m_worksForDrives(FALSE)
	, m_worksForDirectories(FALSE)
	, m_worksForFilesFolder(FALSE)
	, m_worksForFiles(FALSE)
	, m_recurseIntoSubdirectories(FALSE)
	, m_askForConfirmation(FALSE)
	, m_showConsoleWindow(FALSE)
	, m_waitForCompletion(FALSE)
	, m_refreshPolicy(0)
	, m_commandLine(_T(""))
	, m_current(-1)
{
}

CPageCleanups::~CPageCleanups()
{
}

void CPageCleanups::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST, m_list);
	DDX_Check(pDX, IDC_ENABLED, m_enabled);
	DDX_Text(pDX, IDC_TITLE, m_title);
	DDX_Check(pDX, IDC_WORKSFORDRIVES, m_worksForDrives);
	DDX_Check(pDX, IDC_WORKSFORDIRECTORIES, m_worksForDirectories);
	DDX_Check(pDX, IDC_WORKSFORFILESFOLDER, m_worksForFilesFolder);
	DDX_Check(pDX, IDC_WORKSFORFILES, m_worksForFiles);
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
	DDX_Control(pDX, IDC_WORKSFORFILESFOLDER, m_ctlWorksForFilesFolder);
	DDX_Control(pDX, IDC_WORKSFORFILES, m_ctlWorksForFiles);
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
	ON_BN_CLICKED(IDC_WORKSFORFILESFOLDER, OnBnClickedWorksforfilesfolder)
	ON_BN_CLICKED(IDC_WORKSFORFILES, OnBnClickedWorksforfiles)
	ON_EN_CHANGE(IDC_COMMANDLINE, OnEnChangeCommandline)
	ON_BN_CLICKED(IDC_RECURSEINTOSUBDIRECTORIES, OnBnClickedRecurseintosubdirectories)
	ON_BN_CLICKED(IDC_ASKFORCONFIRMATION, OnBnClickedAskforconfirmation)
	ON_BN_CLICKED(IDC_SHOWCONSOLEWINDOW, OnBnClickedShowconsolewindow)
	ON_BN_CLICKED(IDC_WAITFORCOMPLETION, OnBnClickedWaitforcompletion)
	ON_CBN_SELENDOK(IDC_REFRESHPOLICY, OnCbnSelendokRefreshpolicy)
	ON_BN_CLICKED(IDC_UP, OnBnClickedUp)
	ON_BN_CLICKED(IDC_DOWN, OnBnClickedDown)
	ON_BN_CLICKED(IDC_HELPBUTTON, OnBnClickedHelpbutton)
END_MESSAGE_MAP()



BOOL CPageCleanups::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	// Combobox data correspond to enum REFRESHPOLICY:
	m_ctlRefreshPolicy.AddString(LoadString(IDS_POLICY_NOREFRESH));
	m_ctlRefreshPolicy.AddString(LoadString(IDS_POLICY_REFRESHTHISENTRY));
	m_ctlRefreshPolicy.AddString(LoadString(IDS_POLICY_REFRESHPARENT));
	// We don't do this: m_ctlRefreshPolicy.AddString("Assume entry has been deleted");
	// That conflicts with "works for <Files> Pseudoentries".

	GetOptions()->GetUserDefinedCleanups(m_udc);

	for (int i=0; i < USERDEFINEDCLEANUPCOUNT; i++)
		m_list.AddString(m_udc[i].title);

	m_list.SetCurSel(0);
	OnLbnSelchangeList();

	return TRUE;  // return TRUE unless you set the focus to a control
}

void CPageCleanups::OnOK()
{
	CheckEmptyTitle();

	GetOptions()->SetUserDefinedCleanups(m_udc);

	CPropertyPage::OnOK();
}

void CPageCleanups::OnLbnSelchangeList()
{
	CheckEmptyTitle();

	m_current= m_list.GetCurSel();
	CurrentUdcToDialog();
}

void CPageCleanups::CheckEmptyTitle()
{
	if (m_current == -1)
		return;

	UpdateData();
	if (m_title.IsEmpty())
	{
		m_title.FormatMessage(IDS_USERDEFINEDCLEANUPd, m_current);
		UpdateData(false);

		m_list.DeleteString(m_current);
		m_list.InsertString(m_current, m_title);

		DialogToCurrentUdc();
	}
}

void CPageCleanups::CurrentUdcToDialog()
{
	m_enabled= m_udc[m_current].enabled;
	m_title= m_udc[m_current].title;
	m_worksForDrives= m_udc[m_current].worksForDrives;
	m_worksForDirectories= m_udc[m_current].worksForDirectories;
	m_worksForFilesFolder= m_udc[m_current].worksForFilesFolder;
	m_worksForFiles= m_udc[m_current].worksForFiles;
	m_commandLine= m_udc[m_current].commandLine;
	m_recurseIntoSubdirectories= m_udc[m_current].recurseIntoSubdirectories;
	m_askForConfirmation= m_udc[m_current].askForConfirmation;
	m_showConsoleWindow= m_udc[m_current].showConsoleWindow;
	m_waitForCompletion= m_udc[m_current].waitForCompletion;
	m_refreshPolicy= m_udc[m_current].refreshPolicy;

	UpdateControlStatus();
	UpdateData(false);
	
}

void CPageCleanups::DialogToCurrentUdc()
{
	UpdateData();

	m_udc[m_current].enabled= m_enabled;
	m_udc[m_current].title= m_title;
	m_udc[m_current].worksForDrives= m_worksForDrives;
	m_udc[m_current].worksForDirectories= m_worksForDirectories;
	m_udc[m_current].worksForFilesFolder= m_worksForFilesFolder;
	m_udc[m_current].worksForFiles= m_worksForFiles;
	m_udc[m_current].commandLine= m_commandLine;
	m_udc[m_current].recurseIntoSubdirectories= m_recurseIntoSubdirectories;
	m_udc[m_current].askForConfirmation= m_askForConfirmation;
	m_udc[m_current].showConsoleWindow= m_showConsoleWindow;
	m_udc[m_current].waitForCompletion= m_waitForCompletion;
	m_udc[m_current].refreshPolicy= (REFRESHPOLICY)m_refreshPolicy;
}

void CPageCleanups::OnSomethingChanged()
{
	UpdateData();
	if (!m_worksForDrives && !m_worksForDirectories)
		m_recurseIntoSubdirectories= false;
	if (!m_worksForDrives && !m_worksForDirectories)
		m_recurseIntoSubdirectories= false;
	if (!m_waitForCompletion)
		m_refreshPolicy= RP_NO_REFRESH;
	if (m_recurseIntoSubdirectories)
		m_waitForCompletion= true;
	UpdateData(false);
	DialogToCurrentUdc();
	SetModified();
}

void CPageCleanups::UpdateControlStatus()
{
	m_ctlTitle.EnableWindow(m_enabled);
	m_ctlWorksForDrives.EnableWindow(m_enabled);
	m_ctlWorksForDirectories.EnableWindow(m_enabled);
	m_ctlWorksForFilesFolder.EnableWindow(m_enabled);
	m_ctlWorksForFiles.EnableWindow(m_enabled);
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

void CPageCleanups::OnBnClickedWorksforfilesfolder()
{
	OnSomethingChanged();
}

void CPageCleanups::OnBnClickedWorksforfiles()
{
	OnSomethingChanged();
}

void CPageCleanups::OnEnChangeCommandline()
{
	OnSomethingChanged();
}

void CPageCleanups::OnBnClickedRecurseintosubdirectories()
{
	OnSomethingChanged();
	UpdateControlStatus();
}

void CPageCleanups::OnBnClickedAskforconfirmation()
{
	OnSomethingChanged();
}

void CPageCleanups::OnBnClickedShowconsolewindow()
{
	OnSomethingChanged();
}

void CPageCleanups::OnBnClickedWaitforcompletion()
{
	OnSomethingChanged();
}

void CPageCleanups::OnCbnSelendokRefreshpolicy()
{
	OnSomethingChanged();
}


void CPageCleanups::OnBnClickedUp()
{
	ASSERT(m_current > 0);
	
	UpdateData();

	USERDEFINEDCLEANUP h= m_udc[m_current - 1];
	m_udc[m_current - 1]= m_udc[m_current];
	m_udc[m_current]= h;

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

	USERDEFINEDCLEANUP h= m_udc[m_current + 1];
	m_udc[m_current + 1]= m_udc[m_current];
	m_udc[m_current]= h;

	m_list.DeleteString(m_current);
	m_list.InsertString(m_current + 1, m_title);

	m_current++;
	m_list.SetCurSel(m_current);

	SetModified();
	UpdateControlStatus();
}

void CPageCleanups::OnBnClickedHelpbutton()
{
	GetApp()->DoContextHelp(IDH_UserDefinedCleanups);
}
