// SearchDlg.h - Declaration of SearchDlg
//
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

#pragma once

#include "stdafx.h"
#include "Layout.h"

// SearchDlg dialog

class SearchDlg final : public CDialogEx
{
	DECLARE_DYNAMIC(SearchDlg)

public:
	SearchDlg(CWnd* pParent = nullptr);   // standard constructor
    ~SearchDlg() override;

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SEARCH };
#endif

protected:
    void DoDataExchange(CDataExchange* pDX) override;    // DDX/DDV support

private:
    DECLARE_MESSAGE_MAP()
    BOOL OnInitDialog() override;
	afx_msg void OnBnClickedOk();
    afx_msg void OnChangeSearchTerm();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnDestroy();

    BOOL m_SearchWholePhrase;
    BOOL m_SearchCase;
    BOOL m_SearchRegex;
    CString m_SearchTerm;
    CLayout m_Layout;
};
