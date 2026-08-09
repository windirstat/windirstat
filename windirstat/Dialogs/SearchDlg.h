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

#pragma once

#include "pch.h"
#include "Layout.h"

// SearchDlg dialog

class SearchDlg final : public MessageTarget<SearchDlg, CLayoutDialog>
{
public:
    SearchDlg(CWnd* pParent = nullptr); // standard constructor
    ~SearchDlg() override = default;

    // Dialog Data
    enum { IDD = IDD_SEARCH };

protected:
    bool OnInitDialog() override;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnBnClickedOk();
    void OnChangeSearchTerm();
    HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

};

inline std::span<const RouteEntry> SearchDlg::Routes()
{
    using ThisClass = SearchDlg;
    static constexpr std::array entries
    {
        Route::Control<&SearchDlg::OnBnClickedOk>(BN_CLICKED, IDOK),
        Route::Control<&SearchDlg::OnChangeSearchTerm>(EN_CHANGE, IDC_SEARCH_TERM),
        Route::Control<&SearchDlg::OnChangeSearchTerm>(BN_CLICKED, IDC_SEARCH_REGEX),
        Route::Window<&ThisClass::OnCtlColor>(WM_CTLCOLOR),
    };
    return entries;
}
