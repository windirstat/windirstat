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

struct SearchCriteria;

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
    bool ReadCriteria(SearchCriteria& criteria) const;
    void OnBnClickedOk();
    void OnChangeSearchTerm();
    void UpdateControlStatus();
    HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};

inline std::span<const RouteEntry> SearchDlg::Routes()
{
    static constexpr std::array entries
    {
        Route::Control<&OnBnClickedOk>(BN_CLICKED, IDOK),
        Route::Control<&OnChangeSearchTerm>(EN_CHANGE, IDC_SEARCH_TERM),
        Route::Control<&OnChangeSearchTerm>(BN_CLICKED, IDC_SEARCH_REGEX),
        Route::Control<&UpdateControlStatus>(BN_CLICKED, IDC_SEARCH_CASE),
        Route::Control<&UpdateControlStatus>(EN_CHANGE, IDC_SEARCH_SIZE_MIN),
        Route::Control<&UpdateControlStatus>(EN_CHANGE, IDC_SEARCH_SIZE_MAX),
        Route::Control<&UpdateControlStatus>(CBN_SELCHANGE, IDC_SEARCH_SIZE_UNITS),
        Route::Control<&UpdateControlStatus>(EN_CHANGE, IDC_SEARCH_PHYSICAL_MIN),
        Route::Control<&UpdateControlStatus>(EN_CHANGE, IDC_SEARCH_PHYSICAL_MAX),
        Route::Control<&UpdateControlStatus>(CBN_SELCHANGE, IDC_SEARCH_PHYSICAL_UNITS),
        Route::Control<&UpdateControlStatus>(BN_CLICKED, IDC_SEARCH_FILES),
        Route::Control<&UpdateControlStatus>(BN_CLICKED, IDC_SEARCH_FOLDERS),
        Route::Window<&OnCtlColor>(WM_CTLCOLOR),
    };
    return entries;
}
