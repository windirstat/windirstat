// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
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
#include "SearchDlg.h"
#include "DirStatDoc.h"
#include "Localization.h"
#include "FileSearchControl.h"
#include "FileTabbedView.h"

#include <regex>

#include "MainFrame.h"

// SearchDlg dialog

IMPLEMENT_DYNAMIC(SearchDlg, CDialogEx)

SearchDlg::SearchDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_SEARCH, pParent)
    , m_SearchWholePhrase(FALSE)
    , m_SearchCase(FALSE)
    , m_SearchRegex(FALSE)
    , m_SearchTerm(L"")
    , m_Layout(this, COptions::SearchWindowRect.Ptr())
{

}

void SearchDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_SEARCH_WHOLE_PHRASE, m_SearchWholePhrase);
    DDX_Check(pDX, IDC_SEARCH_CASE, m_SearchCase);
    DDX_Check(pDX, IDC_SEARCH_REGEX, m_SearchRegex);
    DDX_Text(pDX, IDC_SEARCH_TERM, m_SearchTerm);
}


BEGIN_MESSAGE_MAP(SearchDlg, CDialogEx)
    ON_BN_CLICKED(IDOK, &SearchDlg::OnBnClickedOk)
    ON_EN_CHANGE(IDC_SEARCH_TERM, &SearchDlg::OnChangeSearchTerm)
    ON_BN_CLICKED(IDC_SEARCH_REGEX, &SearchDlg::OnChangeSearchTerm)
    ON_WM_DESTROY()
    ON_WM_GETMINMAXINFO()
    ON_WM_SIZE()
END_MESSAGE_MAP()


// SearchDlg message handlers

BOOL SearchDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    Localization::UpdateDialogs(*this);

    ModifyStyle(0, WS_CLIPCHILDREN);

    m_Layout.AddControl(IDOK, 1, 0, 0, 0);
    m_Layout.AddControl(IDCANCEL, 1, 0, 0, 0);
    m_Layout.AddControl(IDC_SEARCH_TERM, 0, 0, 1, 0);
    m_Layout.AddControl(IDC_SEARCH_WHOLE_PHRASE, 0, 0, 0, 0);
    m_Layout.AddControl(IDC_SEARCH_REGEX, 0, 0, 0, 0);
    m_Layout.AddControl(IDC_SEARCH_CASE, 0, 0, 0, 0);

    m_Layout.OnInitDialog(true);

    m_SearchTerm = COptions::SearchTerm.Obj().c_str();
    m_SearchWholePhrase = COptions::SearchWholePhrase;
    m_SearchCase = COptions::SearchCase;
    m_SearchRegex = COptions::SearchRegex;
    UpdateData(FALSE);

    OnChangeSearchTerm();
    return TRUE;
}

void SearchDlg::OnBnClickedOk()
{
    UpdateData();

    COptions::SearchTerm.Obj() = m_SearchTerm;
    COptions::SearchWholePhrase = (FALSE != m_SearchWholePhrase);
    COptions::SearchCase = (FALSE != m_SearchCase);
    COptions::SearchRegex = (FALSE != m_SearchRegex);

    // Process search request
    CFileSearchControl::Get()->ProcessSearch(CDirStatDoc::GetDocument()->GetRootItem());

    // Switch focus to search results
    const auto tabbedView = CMainFrame::Get()->GetFileTabbedView();
    tabbedView->SetActiveSearchView();

    CDialogEx::OnOK();
}

void SearchDlg::OnChangeSearchTerm()
{
    UpdateData();

    const auto regexTest = CFileSearchControl::ComputeSearchRegex(
        m_SearchTerm.GetString(), m_SearchCase, m_SearchRegex);
    GetDlgItem(IDOK)->EnableWindow(regexTest.flags() & std::regex_constants::optimize);
}

void SearchDlg::OnSize(const UINT nType, const int cx, const int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    m_Layout.OnSize();
}

void SearchDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    m_Layout.OnGetMinMaxInfo(lpMMI);
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}

void SearchDlg::OnDestroy()
{
    m_Layout.OnDestroy();
    CDialogEx::OnDestroy();
}
