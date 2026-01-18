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

IMPLEMENT_DYNAMIC(SearchDlg, CLayoutDialogEx)

SearchDlg::SearchDlg(CWnd* pParent /*=nullptr*/)
    : CLayoutDialogEx(IDD_SEARCH, COptions::SearchWindowRect.Ptr(), pParent)
    , m_searchWholePhrase(FALSE)
    , m_searchCase(FALSE)
    , m_searchRegex(FALSE)
    , m_searchTerm(L"")
{
}

void SearchDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_SEARCH_WHOLE_PHRASE, m_searchWholePhrase);
    DDX_Check(pDX, IDC_SEARCH_CASE, m_searchCase);
    DDX_Check(pDX, IDC_SEARCH_REGEX, m_searchRegex);
    DDX_Text(pDX, IDC_SEARCH_TERM, m_searchTerm);
}

BEGIN_MESSAGE_MAP(SearchDlg, CLayoutDialogEx)
    ON_BN_CLICKED(IDOK, &SearchDlg::OnBnClickedOk)
    ON_EN_CHANGE(IDC_SEARCH_TERM, &SearchDlg::OnChangeSearchTerm)
    ON_BN_CLICKED(IDC_SEARCH_REGEX, &SearchDlg::OnChangeSearchTerm)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

// SearchDlg message handlers

BOOL SearchDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(GetSafeHwnd());

    ModifyStyle(0, WS_CLIPCHILDREN);

    m_layout.AddControl(IDOK, 1, 0, 0, 0);
    m_layout.AddControl(IDCANCEL, 1, 0, 0, 0);
    m_layout.AddControl(IDC_SEARCH_TERM, 0, 0, 1, 0);
    m_layout.AddControl(IDC_SEARCH_WHOLE_PHRASE, 0, 0, 0, 0);
    m_layout.AddControl(IDC_SEARCH_REGEX, 0, 0, 0, 0);
    m_layout.AddControl(IDC_SEARCH_CASE, 0, 0, 0, 0);

    m_layout.OnInitDialog(true);

    m_searchTerm = COptions::SearchTerm.Obj().c_str();
    m_searchWholePhrase = COptions::SearchWholePhrase;
    m_searchCase = COptions::SearchCase;
    m_searchRegex = COptions::SearchRegex;
    UpdateData(FALSE);

    OnChangeSearchTerm();
    return TRUE;
}

void SearchDlg::OnBnClickedOk()
{
    UpdateData();

    COptions::SearchTerm.Obj() = m_searchTerm;
    COptions::SearchWholePhrase = (FALSE != m_searchWholePhrase);
    COptions::SearchCase = (FALSE != m_searchCase);
    COptions::SearchRegex = (FALSE != m_searchRegex);

    CLayoutDialogEx::OnOK();

    // Process search request
    CFileSearchControl::Get()->ProcessSearch(CDirStatDoc::Get()->GetRootItem(),
        COptions::SearchTerm, COptions::SearchCase,
        COptions::SearchWholePhrase, COptions::SearchRegex, true);

    // Switch focus to search results
    const auto tabbedView = CMainFrame::Get()->GetFileTabbedView();
    tabbedView->SetActiveSearchView();
}

void SearchDlg::OnChangeSearchTerm()
{
    UpdateData();

    // Auto-enable whole phrase search if * is present and not in regex mode
    if (!m_searchRegex && m_searchTerm.Find(L'*') != -1)
    {
        m_searchWholePhrase = TRUE;
        UpdateData(FALSE);
    }

    const auto regexTest = CFileSearchControl::ComputeSearchRegex(
        m_searchTerm.GetString(), m_searchCase, m_searchRegex);
    GetDlgItem(IDOK)->EnableWindow(regexTest.flags() & std::regex_constants::optimize);
}

HBRUSH SearchDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}
