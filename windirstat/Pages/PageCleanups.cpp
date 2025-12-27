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
#include "PageCleanups.h"

IMPLEMENT_DYNAMIC(CPageCleanups, CMFCPropertyPage)

CPageCleanups::CPageCleanups() : CMFCPropertyPage(IDD) {}

CPageCleanups::~CPageCleanups() = default;

void CPageCleanups::DoDataExchange(CDataExchange* pDX)
{
    CMFCPropertyPage::DoDataExchange(pDX);
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

BEGIN_MESSAGE_MAP(CPageCleanups, CMFCPropertyPage)
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
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

HBRUSH CPageCleanups::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CMFCPropertyPage::OnCtlColor(pDC, pWnd, nCtlColor);
}

BOOL CPageCleanups::OnInitDialog()
{
    CMFCPropertyPage::OnInitDialog();

    Localization::UpdateDialogs(*this);

    // Apply dark mode to this property page
    DarkMode::AdjustControls(GetSafeHwnd());

    // Combobox data correspond to enum REFRESHPOLICY:
    m_ctlRefreshPolicy.AddString(Localization::Lookup(IDS_POLICY_NOREFRESH).c_str());
    m_ctlRefreshPolicy.AddString(Localization::Lookup(IDS_POLICY_REFRESH_ENTRY).c_str());
    m_ctlRefreshPolicy.AddString(Localization::Lookup(IDS_POLICY_REFRESH_PARENT).c_str());

    for (const auto i : std::views::iota(size_t{0}, COptions::UserDefinedCleanups.size()))
    {
        m_udc[i] = COptions::UserDefinedCleanups[i];
        m_list.AddString(m_udc[i].Title.Obj().c_str());
    }

    m_list.SetCurSel(0);
    OnLbnSelchangeList();

    return TRUE; // return TRUE unless you set the focus to a control
}

void CPageCleanups::OnOK()
{
    CheckEmptyTitle();

    for (const auto i : std::views::iota(size_t{0}, COptions::UserDefinedCleanups.size()))
    {
        COptions::UserDefinedCleanups[i].AskForConfirmation = m_udc[i].AskForConfirmation.Obj();
        COptions::UserDefinedCleanups[i].CommandLine = m_udc[i].CommandLine.Obj();
        COptions::UserDefinedCleanups[i].Enabled = m_udc[i].Enabled.Obj();
        COptions::UserDefinedCleanups[i].RecurseIntoSubdirectories = m_udc[i].RecurseIntoSubdirectories.Obj();
        COptions::UserDefinedCleanups[i].RefreshPolicy = m_udc[i].RefreshPolicy.Obj();
        COptions::UserDefinedCleanups[i].ShowConsoleWindow = m_udc[i].ShowConsoleWindow.Obj();
        COptions::UserDefinedCleanups[i].Title = m_udc[i].Title.Obj();
        COptions::UserDefinedCleanups[i].VirginTitle = m_udc[i].VirginTitle.Obj();
        COptions::UserDefinedCleanups[i].WaitForCompletion = m_udc[i].WaitForCompletion.Obj();
        COptions::UserDefinedCleanups[i].WorksForDirectories = m_udc[i].WorksForDirectories.Obj();
        COptions::UserDefinedCleanups[i].WorksForDrives = m_udc[i].WorksForDrives.Obj();
        COptions::UserDefinedCleanups[i].WorksForFiles = m_udc[i].WorksForFiles.Obj();
        COptions::UserDefinedCleanups[i].WorksForUncPaths = m_udc[i].WorksForUncPaths.Obj();
    }

    CMFCPropertyPage::OnOK();
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
        m_title = Localization::Format(IDS_USER_DEFINED_CLEANUPd, m_current).c_str();
        UpdateData(FALSE);

        m_list.DeleteString(m_current);
        m_list.InsertString(m_current, m_title);

        DialogToCurrentUdc();
        m_udc[m_current].VirginTitle = true;
    }
}

void CPageCleanups::CurrentUdcToDialog()
{
    m_askForConfirmation        = m_udc[m_current].AskForConfirmation;
    m_commandLine               = m_udc[m_current].CommandLine.Obj().c_str();
    m_enabled                   = m_udc[m_current].Enabled;
    m_recurseIntoSubdirectories = m_udc[m_current].RecurseIntoSubdirectories;
    m_refreshPolicy             = m_udc[m_current].RefreshPolicy;
    m_showConsoleWindow         = m_udc[m_current].ShowConsoleWindow;
    m_title                     = m_udc[m_current].Title.Obj().c_str();
    m_waitForCompletion         = m_udc[m_current].WaitForCompletion;
    m_worksForDirectories       = m_udc[m_current].WorksForDirectories;
    m_worksForDrives            = m_udc[m_current].WorksForDrives;
    m_worksForFiles             = m_udc[m_current].WorksForFiles;
    m_worksForUncPaths          = m_udc[m_current].WorksForUncPaths;

    UpdateControlStatus();
    UpdateData(FALSE);
}

void CPageCleanups::DialogToCurrentUdc()
{
    UpdateData();

    m_udc[m_current].Enabled                   = FALSE != m_enabled;
    m_udc[m_current].Title.Obj()              = m_title;
    m_udc[m_current].WorksForDrives            = FALSE != m_worksForDrives;
    m_udc[m_current].WorksForDirectories       = FALSE != m_worksForDirectories;
    m_udc[m_current].WorksForFiles             = FALSE != m_worksForFiles;
    m_udc[m_current].WorksForUncPaths          = FALSE != m_worksForUncPaths;
    m_udc[m_current].CommandLine.Obj()       = m_commandLine;
    m_udc[m_current].RecurseIntoSubdirectories = FALSE != m_recurseIntoSubdirectories;
    m_udc[m_current].AskForConfirmation        = FALSE != m_askForConfirmation;
    m_udc[m_current].ShowConsoleWindow         = FALSE != m_showConsoleWindow;
    m_udc[m_current].WaitForCompletion         = FALSE != m_waitForCompletion;
    m_udc[m_current].RefreshPolicy             = m_refreshPolicy;
}

void CPageCleanups::OnSomethingChanged()
{
    UpdateData();
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
    UpdateData(FALSE);
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
    m_udc[m_current].VirginTitle = false;
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
    CDirStatApp::LaunchHelp();
}
