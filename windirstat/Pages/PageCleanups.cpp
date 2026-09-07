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

    // Combobox data correspond to enum REFRESHPOLICY:
    m_ctlRefreshPolicy.AddString(Localization::Lookup(IDS_POLICY_NOREFRESH).c_str());
    m_ctlRefreshPolicy.AddString(Localization::Lookup(IDS_POLICY_REFRESH_ENTRY).c_str());
    m_ctlRefreshPolicy.AddString(Localization::Lookup(IDS_POLICY_REFRESH_PARENT).c_str());

    m_udc = COptions::UserDefinedCleanups;
    for (const USERDEFINEDCLEANUP& udc : m_udc) m_customCleanupList.AddString(udc.Title.Obj().c_str());

    if (!m_udc.empty()) m_customCleanupList.SetCurSel(0);
    OnLbnSelchangeList();
}

void CPageCleanups::OnOK()
{
    CheckEmptyTitle();
    DialogToCurrentUdc();
    COptions::SetUserDefinedCleanups(m_udc);
}

void CPageCleanups::OnLbnSelchangeList()
{
    CheckEmptyTitle();

    m_current = m_customCleanupList.GetCurSel();
    if (!HasCurrentUdc()) m_current = -1;
    CurrentUdcToDialog();
}

void CPageCleanups::CheckEmptyTitle()
{
    if (!HasCurrentUdc() || !GetText(IDC_TITLE).empty()) return;

    const int selection = m_customCleanupList.GetCurSel();
    const std::wstring title = Localization::Format(IDS_USER_DEFINED_CLEANUPd, m_current);
    DialogToCurrentUdc();
    m_udc[m_current].Title = title;
    m_udc[m_current].VirginTitle = true;

    const ScopedValue updating(m_updating, true);
    SetText(IDC_TITLE, title);
    m_customCleanupList.DeleteString(m_current);
    m_customCleanupList.InsertString(m_current, title.c_str());
    m_customCleanupList.SetCurSel(selection);
}

void CPageCleanups::CurrentUdcToDialog()
{
    const ScopedValue updating(m_updating, true);
    if (!HasCurrentUdc())
    {
        for (const UINT id : { IDC_ASKFORCONFIRMATION, IDC_ENABLED, IDC_RECURSEINTOSUBDIRECTORIES,
            IDC_SHOWCONSOLEWINDOW, IDC_WAITFORCOMPLETION, IDC_WORKSFORDIRECTORIES, IDC_WORKSFORDRIVES,
            IDC_WORKSFORFILES, IDC_WORKSFORUNCPATHS }) SetChecked(id, false);
        for (const UINT id : { IDC_COMMANDLINE, IDC_TITLE }) SetText(id, L"");
        SetComboSelection(IDC_REFRESHPOLICY, -1);
        UpdateControlStatus();
        return;
    }

    auto& udc = m_udc[m_current];
    SetChecked(IDC_ASKFORCONFIRMATION, udc.AskForConfirmation);
    SetText(IDC_COMMANDLINE, udc.CommandLine.Obj());
    SetChecked(IDC_ENABLED, udc.Enabled);
    SetChecked(IDC_RECURSEINTOSUBDIRECTORIES, udc.RecurseIntoSubdirectories);
    SetComboSelection(IDC_REFRESHPOLICY, udc.RefreshPolicy);
    SetChecked(IDC_SHOWCONSOLEWINDOW, udc.ShowConsoleWindow);
    SetText(IDC_TITLE, udc.Title.Obj());
    SetChecked(IDC_WAITFORCOMPLETION, udc.WaitForCompletion);
    SetChecked(IDC_WORKSFORDIRECTORIES, udc.WorksForDirectories);
    SetChecked(IDC_WORKSFORDRIVES, udc.WorksForDrives);
    SetChecked(IDC_WORKSFORFILES, udc.WorksForFiles);
    SetChecked(IDC_WORKSFORUNCPATHS, udc.WorksForUncPaths);

    UpdateControlStatus();
}

void CPageCleanups::DialogToCurrentUdc()
{
    if (!HasCurrentUdc()) return;

    auto& udc = m_udc[m_current];
    udc.Enabled                   = IsChecked(IDC_ENABLED);
    udc.Title.Obj()               = GetText(IDC_TITLE);
    udc.WorksForDrives            = IsChecked(IDC_WORKSFORDRIVES);
    udc.WorksForDirectories       = IsChecked(IDC_WORKSFORDIRECTORIES);
    udc.WorksForFiles             = IsChecked(IDC_WORKSFORFILES);
    udc.WorksForUncPaths          = IsChecked(IDC_WORKSFORUNCPATHS);
    udc.CommandLine.Obj()         = GetText(IDC_COMMANDLINE);
    udc.RecurseIntoSubdirectories = IsChecked(IDC_RECURSEINTOSUBDIRECTORIES);
    udc.AskForConfirmation        = IsChecked(IDC_ASKFORCONFIRMATION);
    udc.ShowConsoleWindow         = IsChecked(IDC_SHOWCONSOLEWINDOW);
    udc.WaitForCompletion         = IsChecked(IDC_WAITFORCOMPLETION);
    udc.RefreshPolicy             = ComboSelection(IDC_REFRESHPOLICY);
}

void CPageCleanups::OnSomethingChanged()
{
    if (m_updating || !HasCurrentUdc()) return;

    if (!IsChecked(IDC_WORKSFORDRIVES) && !IsChecked(IDC_WORKSFORDIRECTORIES))
        SetChecked(IDC_RECURSEINTOSUBDIRECTORIES, false);
    if (!IsChecked(IDC_WAITFORCOMPLETION)) SetComboSelection(IDC_REFRESHPOLICY, RP_NO_REFRESH);
    if (IsChecked(IDC_RECURSEINTOSUBDIRECTORIES)) SetChecked(IDC_WAITFORCOMPLETION, true);

    DialogToCurrentUdc();
    SetModified();
    UpdateControlStatus();
}

void CPageCleanups::UpdateControlStatus()
{
    const bool hasCurrent = HasCurrentUdc();
    const bool enabled = hasCurrent && IsChecked(IDC_ENABLED);
    const bool recurseIntoSubdirectories = IsChecked(IDC_RECURSEINTOSUBDIRECTORIES);

    for (const UINT id : { IDC_REMOVE_CLEANUP, IDC_ENABLED }) GetDlgItem(id)->EnableWindow(hasCurrent);
    for (const UINT id : { IDC_TITLE, IDC_WORKSFORDRIVES, IDC_WORKSFORDIRECTORIES, IDC_WORKSFORFILES,
        IDC_WORKSFORUNCPATHS, IDC_COMMANDLINE, IDC_ASKFORCONFIRMATION, IDC_SHOWCONSOLEWINDOW,
        IDC_REFRESHPOLICY }) GetDlgItem(id)->EnableWindow(enabled);
    GetDlgItem(IDC_RECURSEINTOSUBDIRECTORIES)->EnableWindow(enabled &&
        (IsChecked(IDC_WORKSFORDRIVES) || IsChecked(IDC_WORKSFORDIRECTORIES)));
    GetDlgItem(IDC_WAITFORCOMPLETION)->EnableWindow(enabled && !recurseIntoSubdirectories);

    const int showHints = hasCurrent && recurseIntoSubdirectories ? SW_SHOW : SW_HIDE;
    for (const UINT id : { IDC_HINTSP, IDC_HINTSN }) GetDlgItem(id)->ShowWindow(showHints);

    GetDlgItem(IDC_UP)->EnableWindow(hasCurrent && m_current > 0);
    GetDlgItem(IDC_DOWN)->EnableWindow(hasCurrent && std::cmp_less(m_current + 1, m_udc.size()));
}

void CPageCleanups::OnBnClickedEnabled()
{
    OnSomethingChanged();
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
    if (m_updating || !HasCurrentUdc()) return;

    OnSomethingChanged();
    m_udc[m_current].VirginTitle = false;
    m_customCleanupList.DeleteString(m_current);
    m_customCleanupList.InsertString(m_current, m_udc[m_current].Title.Obj().c_str());
    m_customCleanupList.SetCurSel(m_current);
}

void CPageCleanups::OnBnClickedAdd()
{
    CheckEmptyTitle();

    auto& udc = m_udc.emplace_back();
    m_current = static_cast<int>(m_udc.size()) - 1;
    udc.Title = Localization::Format(IDS_USER_DEFINED_CLEANUPd, m_current);
    m_customCleanupList.AddString(udc.Title.Obj().c_str());
    m_customCleanupList.SetCurSel(m_current);

    CurrentUdcToDialog();
    SetModified();
}

void CPageCleanups::OnBnClickedRemove()
{
    if (!HasCurrentUdc()) return;

    m_udc.erase(m_udc.begin() + m_current);
    m_customCleanupList.DeleteString(m_current);
    if (std::cmp_equal(m_current, m_udc.size())) --m_current;
    m_customCleanupList.SetCurSel(m_current);

    CurrentUdcToDialog();
    SetModified();
}

void CPageCleanups::MoveCurrentUdc(const int offset)
{
    const int destination = m_current + offset;
    if (!HasCurrentUdc() || destination < 0 || !std::cmp_less(destination, m_udc.size())) return;

    DialogToCurrentUdc();
    std::swap(m_udc[m_current], m_udc[destination]);
    m_customCleanupList.DeleteString(m_current);
    m_customCleanupList.InsertString(destination, m_udc[destination].Title.Obj().c_str());
    m_current = destination;
    m_customCleanupList.SetCurSel(m_current);

    SetModified();
    UpdateControlStatus();
}

void CPageCleanups::OnBnClickedUp()
{
    MoveCurrentUdc(-1);
}

void CPageCleanups::OnBnClickedDown()
{
    MoveCurrentUdc(1);
}

void CPageCleanups::OnBnClickedHelpbutton()
{
    CDirStatApp::LaunchHelp();
}
