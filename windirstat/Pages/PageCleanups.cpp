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

CPageCleanups::CPageCleanups() : MessageTarget(IDD) {}

void CPageCleanups::InitializePage()
{
    m_customCleanupList.SubclassDlgItem(IDC_LIST, this);
    m_ctlRefreshPolicy.SubclassDlgItem(IDC_REFRESHPOLICY, this);
    m_ctlTitle.SubclassDlgItem(IDC_TITLE, this);
    m_ctlWorksForDrives.SubclassDlgItem(IDC_WORKSFORDRIVES, this);
    m_ctlWorksForDirectories.SubclassDlgItem(IDC_WORKSFORDIRECTORIES, this);
    m_ctlWorksForFiles.SubclassDlgItem(IDC_WORKSFORFILES, this);
    m_ctlWorksForUncPaths.SubclassDlgItem(IDC_WORKSFORUNCPATHS, this);
    m_ctlCommandLine.SubclassDlgItem(IDC_COMMANDLINE, this);
    m_ctlRecurseIntoSubdirectories.SubclassDlgItem(IDC_RECURSEINTOSUBDIRECTORIES, this);
    m_ctlAskForConfirmation.SubclassDlgItem(IDC_ASKFORCONFIRMATION, this);
    m_ctlShowConsoleWindow.SubclassDlgItem(IDC_SHOWCONSOLEWINDOW, this);
    m_ctlWaitForCompletion.SubclassDlgItem(IDC_WAITFORCOMPLETION, this);
    m_ctlHintSp.SubclassDlgItem(IDC_HINTSP, this);
    m_ctlHintSn.SubclassDlgItem(IDC_HINTSN, this);
    m_ctlUp.SubclassDlgItem(IDC_UP, this);
    m_ctlDown.SubclassDlgItem(IDC_DOWN, this);

    // Combobox data correspond to enum REFRESHPOLICY:
    m_ctlRefreshPolicy.AddString(Localization::Lookup(IDS_POLICY_NOREFRESH).c_str());
    m_ctlRefreshPolicy.AddString(Localization::Lookup(IDS_POLICY_REFRESH_ENTRY).c_str());
    m_ctlRefreshPolicy.AddString(Localization::Lookup(IDS_POLICY_REFRESH_PARENT).c_str());

    for (const auto i : std::views::iota(size_t{0}, COptions::UserDefinedCleanups.size()))
    {
        m_udc[i] = COptions::UserDefinedCleanups[i];
        m_customCleanupList.AddString(m_udc[i].Title.Obj().c_str());
    }

    m_customCleanupList.SetCurSel(0);
    OnLbnSelchangeList();
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
}

void CPageCleanups::OnLbnSelchangeList()
{
    CheckEmptyTitle();

    m_current = m_customCleanupList.GetCurSel();
    if (m_current < 0 || m_current >= USERDEFINEDCLEANUPCOUNT)
    {
        m_current = USERDEFINEDCLEANUPCOUNT - 1;
        m_customCleanupList.SetCurSel(m_current);
    }
    CurrentUdcToDialog();
}

void CPageCleanups::CheckEmptyTitle()
{
    if (m_current == -1)
    {
        return;
    }

    if (GetText(IDC_TITLE).empty())
    {
        const std::wstring title = Localization::Format(IDS_USER_DEFINED_CLEANUPd, m_current);
        SetText(IDC_TITLE, title);

        m_customCleanupList.DeleteString(m_current);
        m_customCleanupList.InsertString(m_current, title.c_str());

        DialogToCurrentUdc();
        m_udc[m_current].VirginTitle = true;
    }
}

void CPageCleanups::CurrentUdcToDialog()
{
    SetChecked(IDC_ASKFORCONFIRMATION, m_udc[m_current].AskForConfirmation);
    SetText(IDC_COMMANDLINE, m_udc[m_current].CommandLine.Obj());
    SetChecked(IDC_ENABLED, m_udc[m_current].Enabled);
    SetChecked(IDC_RECURSEINTOSUBDIRECTORIES, m_udc[m_current].RecurseIntoSubdirectories);
    SetComboSelection(IDC_REFRESHPOLICY, m_udc[m_current].RefreshPolicy);
    SetChecked(IDC_SHOWCONSOLEWINDOW, m_udc[m_current].ShowConsoleWindow);
    SetText(IDC_TITLE, m_udc[m_current].Title.Obj());
    SetChecked(IDC_WAITFORCOMPLETION, m_udc[m_current].WaitForCompletion);
    SetChecked(IDC_WORKSFORDIRECTORIES, m_udc[m_current].WorksForDirectories);
    SetChecked(IDC_WORKSFORDRIVES, m_udc[m_current].WorksForDrives);
    SetChecked(IDC_WORKSFORFILES, m_udc[m_current].WorksForFiles);
    SetChecked(IDC_WORKSFORUNCPATHS, m_udc[m_current].WorksForUncPaths);

    UpdateControlStatus();
}

void CPageCleanups::DialogToCurrentUdc()
{
    m_udc[m_current].Enabled                   = IsChecked(IDC_ENABLED);
    m_udc[m_current].Title.Obj()               = GetText(IDC_TITLE);
    m_udc[m_current].WorksForDrives            = IsChecked(IDC_WORKSFORDRIVES);
    m_udc[m_current].WorksForDirectories       = IsChecked(IDC_WORKSFORDIRECTORIES);
    m_udc[m_current].WorksForFiles             = IsChecked(IDC_WORKSFORFILES);
    m_udc[m_current].WorksForUncPaths          = IsChecked(IDC_WORKSFORUNCPATHS);
    m_udc[m_current].CommandLine.Obj()         = GetText(IDC_COMMANDLINE);
    m_udc[m_current].RecurseIntoSubdirectories = IsChecked(IDC_RECURSEINTOSUBDIRECTORIES);
    m_udc[m_current].AskForConfirmation        = IsChecked(IDC_ASKFORCONFIRMATION);
    m_udc[m_current].ShowConsoleWindow         = IsChecked(IDC_SHOWCONSOLEWINDOW);
    m_udc[m_current].WaitForCompletion         = IsChecked(IDC_WAITFORCOMPLETION);
    m_udc[m_current].RefreshPolicy             = ComboSelection(IDC_REFRESHPOLICY);
}

void CPageCleanups::OnSomethingChanged()
{
    const bool worksForDrives = IsChecked(IDC_WORKSFORDRIVES);
    const bool worksForDirectories = IsChecked(IDC_WORKSFORDIRECTORIES);
    bool recurseIntoSubdirectories = IsChecked(IDC_RECURSEINTOSUBDIRECTORIES);
    const bool waitForCompletion = IsChecked(IDC_WAITFORCOMPLETION);

    if (!worksForDrives && !worksForDirectories)
    {
        recurseIntoSubdirectories = false;
        SetChecked(IDC_RECURSEINTOSUBDIRECTORIES, false);
    }
    if (!waitForCompletion)
    {
        SetComboSelection(IDC_REFRESHPOLICY, RP_NO_REFRESH);
    }
    if (recurseIntoSubdirectories)
    {
        SetChecked(IDC_WAITFORCOMPLETION, true);
    }

    DialogToCurrentUdc();
    SetModified();
}

void CPageCleanups::UpdateControlStatus()
{
    const bool enabled = IsChecked(IDC_ENABLED);
    const bool worksForDrives = IsChecked(IDC_WORKSFORDRIVES);
    const bool worksForDirectories = IsChecked(IDC_WORKSFORDIRECTORIES);
    const bool recurseIntoSubdirectories = IsChecked(IDC_RECURSEINTOSUBDIRECTORIES);

    m_ctlTitle.EnableWindow(enabled);
    m_ctlWorksForDrives.EnableWindow(enabled);
    m_ctlWorksForDirectories.EnableWindow(enabled);
    m_ctlWorksForFiles.EnableWindow(enabled);
    m_ctlWorksForUncPaths.EnableWindow(enabled);
    m_ctlCommandLine.EnableWindow(enabled);
    m_ctlRecurseIntoSubdirectories.EnableWindow(enabled && (worksForDrives || worksForDirectories));
    m_ctlAskForConfirmation.EnableWindow(enabled);
    m_ctlShowConsoleWindow.EnableWindow(enabled);
    m_ctlWaitForCompletion.EnableWindow(enabled && !recurseIntoSubdirectories);
    m_ctlRefreshPolicy.EnableWindow(enabled);

    m_ctlHintSp.ShowWindow(recurseIntoSubdirectories ? SW_SHOW : SW_HIDE);
    m_ctlHintSn.ShowWindow(recurseIntoSubdirectories ? SW_SHOW : SW_HIDE);

    m_ctlUp.EnableWindow(m_current > 0);
    m_ctlDown.EnableWindow(m_current < USERDEFINEDCLEANUPCOUNT - 1);
}

void CPageCleanups::OnBnClickedEnabled()
{
    OnSomethingChanged();
    UpdateControlStatus();
    if (IsChecked(IDC_ENABLED))
    {
        m_ctlTitle.SetFocus();
        m_ctlTitle.SetSel(0, -1);
    }
    else
    {
        m_customCleanupList.SetFocus();
    }
}

void CPageCleanups::OnEnChangeTitle()
{
    OnSomethingChanged();
    const std::wstring title = GetText(IDC_TITLE);
    m_udc[m_current].VirginTitle = false;
    m_customCleanupList.DeleteString(m_current);
    m_customCleanupList.InsertString(m_current, title.c_str());
    m_customCleanupList.SetCurSel(m_current);
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
    assert(m_current > 0);

    DialogToCurrentUdc();

    const USERDEFINEDCLEANUP h = m_udc[m_current - 1];
    m_udc[m_current - 1] = m_udc[m_current];
    m_udc[m_current] = h;

    m_customCleanupList.DeleteString(m_current);
    m_customCleanupList.InsertString(m_current - 1, m_udc[m_current - 1].Title.Obj().c_str());

    m_current--;
    m_customCleanupList.SetCurSel(m_current);

    SetModified();
    UpdateControlStatus();
}

void CPageCleanups::OnBnClickedDown()
{
    assert(m_current < USERDEFINEDCLEANUPCOUNT - 1);

    DialogToCurrentUdc();

    const USERDEFINEDCLEANUP h = m_udc[m_current + 1];
    m_udc[m_current + 1] = m_udc[m_current];
    m_udc[m_current] = h;

    m_customCleanupList.DeleteString(m_current);
    m_customCleanupList.InsertString(m_current + 1, m_udc[m_current + 1].Title.Obj().c_str());

    m_current++;
    m_customCleanupList.SetCurSel(m_current);

    SetModified();
    UpdateControlStatus();
}

void CPageCleanups::OnBnClickedHelpbutton()
{
    CDirStatApp::LaunchHelp();
}
