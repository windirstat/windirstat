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
#include "SearchDlg.h"
#include "FileSearchControl.h"
#include "FileTabbedView.h"

// SearchDlg dialog

SearchDlg::SearchDlg(CWnd* pParent /*=nullptr*/)
    : MessageTarget(IDD_SEARCH, COptions::SearchWindowRect.Ptr(), pParent)
{
}

// SearchDlg message handlers

bool SearchDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(Handle());

    ModifyStyle(0, WS_CLIPCHILDREN);

    m_layout.AddControl(IDOK, 1, 0, 0, 0);
    m_layout.AddControl(IDCANCEL, 1, 0, 0, 0);
    m_layout.AddControl(IDC_SEARCH_TERM, 0, 0, 1, 0);
    m_layout.AddControl(IDC_SEARCH_WHOLE_PHRASE, 0, 0, 0, 0);
    m_layout.AddControl(IDC_SEARCH_REGEX, 0, 0, 0, 0);
    m_layout.AddControl(IDC_SEARCH_CASE, 0, 0, 0, 0);
    m_layout.AddControl(IDC_SEARCH_SIZE_MIN, 0, 0, 0, 0);
    m_layout.AddControl(IDC_SEARCH_SIZE_MAX, 0, 0, 0, 0);
    m_layout.AddControl(IDC_SEARCH_SIZE_UNITS, 0, 0, 0, 0);

    m_layout.OnInitDialog(true);

    m_ctlSearchSizeUnits.SubclassDlgItem(IDC_SEARCH_SIZE_UNITS, this);
    m_ctlSearchSizeUnits.AddString(GetSpec_Bytes());
    m_ctlSearchSizeUnits.AddString(GetSpec_KiB());
    m_ctlSearchSizeUnits.AddString(GetSpec_MiB());
    m_ctlSearchSizeUnits.AddString(GetSpec_GiB());
    m_ctlSearchSizeUnits.AddString(GetSpec_TiB());
    if (DarkMode::IsDarkModeActive()) DarkMode::AdjustControls(m_ctlSearchSizeUnits.Handle());

    SetText(IDC_SEARCH_TERM, COptions::SearchTerm.Obj());
    SetChecked(IDC_SEARCH_WHOLE_PHRASE, COptions::SearchWholePhrase);
    SetChecked(IDC_SEARCH_CASE, COptions::SearchCase);
    SetChecked(IDC_SEARCH_REGEX, COptions::SearchRegex);
    SetText(IDC_SEARCH_SIZE_MIN, std::to_wstring(COptions::SearchSizeMinimum));
    SetText(IDC_SEARCH_SIZE_MAX, std::to_wstring(COptions::SearchSizeMaximum));
    SetComboSelection(IDC_SEARCH_SIZE_UNITS, COptions::SearchSizeUnits);

    OnChangeSearchTerm();
    return true;
}

void SearchDlg::OnBnClickedOk()
{
    COptions::SearchTerm.Obj() = GetText(IDC_SEARCH_TERM);
    COptions::SearchWholePhrase = IsChecked(IDC_SEARCH_WHOLE_PHRASE);
    COptions::SearchCase = IsChecked(IDC_SEARCH_CASE);
    COptions::SearchRegex = IsChecked(IDC_SEARCH_REGEX);

    // An empty or unparsable field leaves that end of the range open
    const auto readBound = [this](const int controlId)
    {
        const std::wstring text = GetText(controlId);
        if (text.empty()) return 0;
        try { return std::max(0, std::stoi(text)); }
        catch (const std::exception&) { return 0; }
    };

    COptions::SearchSizeMinimum = readBound(IDC_SEARCH_SIZE_MIN);
    COptions::SearchSizeMaximum = readBound(IDC_SEARCH_SIZE_MAX);
    COptions::SearchSizeUnits = std::clamp<int>(GetComboSelection(IDC_SEARCH_SIZE_UNITS), 0, 4);

    // Scale both bounds with the selected unit; zero leaves that end of the range open.
    // Saturate rather than wrap, so an implausibly large entry cannot silently turn into
    // a small bound and hide every result.
    const auto scale = [](const int value) -> ULONGLONG
    {
        const int shift = 10 * COptions::SearchSizeUnits;
        const auto scaled = static_cast<ULONGLONG>(value);
        return scaled > (std::numeric_limits<ULONGLONG>::max() >> shift) ?
            std::numeric_limits<ULONGLONG>::max() : scaled << shift;
    };

    CLayoutDialog::OnOK();

    // Process search request
    CFileSearchControl::Get()->ProcessSearch(CWinDirStatModel::Get()->GetRootItem(),
        COptions::SearchTerm, COptions::SearchCase,
        COptions::SearchWholePhrase, COptions::SearchRegex, false,
        scale(COptions::SearchSizeMinimum), scale(COptions::SearchSizeMaximum));

    // Switch focus to search results
    const auto tabbedView = CMainFrame::Get()->GetFileTabbedView();
    tabbedView->SetActiveSearchView();
}

void SearchDlg::OnChangeSearchTerm()
{
    const std::wstring searchTerm = GetText(IDC_SEARCH_TERM);
    const bool searchRegex = IsChecked(IDC_SEARCH_REGEX);
    const bool searchCase = IsChecked(IDC_SEARCH_CASE);

    // Auto-enable whole phrase search if * is present and not in regex mode
    if (!searchRegex && searchTerm.contains(L'*'))
    {
        SetChecked(IDC_SEARCH_WHOLE_PHRASE, true);
    }

    const auto regexTest = CFileSearchControl::ComputeSearchRegex(
        searchTerm, searchCase, searchRegex);
    GetDlgItem(IDOK)->EnableWindow((regexTest.flags() & std::regex_constants::optimize) != 0);
}

HBRUSH SearchDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}
