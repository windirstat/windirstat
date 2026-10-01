// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

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
    bool PreprocessMessage(MSG* pMsg) override;

public:
    static std::span<const RouteEntry> Routes();

protected:
    bool ReadCriteria(SearchCriteria& criteria) const;
    void OnBnClickedOk();
    void SaveSearchHistory(const SearchCriteria* criteria = nullptr) const;
    void OnSelectSearchTerm();
    void OnChangeSearchTerm();
    void UpdateControlStatus();
    void OnGetMinMaxInfo(MINMAXINFO* pMMI);

    CComboBox m_searchTerm;
    std::vector<std::wstring> m_searchHistory;
    int m_minWidth = 0;
    int m_fixedHeight = 0;
};

inline constexpr int CombineSearchFlags(bool regex, bool wholePhrase, bool caseSensitive) noexcept
{
    return (regex ? 1 : 0) | (wholePhrase ? 2 : 0) | (caseSensitive ? 4 : 0);
}

inline std::span<const RouteEntry> SearchDlg::Routes()
{
    static constexpr std::array entries
    {
        Route::Control<&OnBnClickedOk>(BN_CLICKED, IDOK),
        Route::Control<&OnChangeSearchTerm>(CBN_EDITCHANGE, IDC_SEARCH_TERM),
        Route::Control<&OnSelectSearchTerm>(CBN_SELENDOK, IDC_SEARCH_TERM),
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
        Route::Window<&OnGetMinMaxInfo>(WM_GETMINMAXINFO),
    };
    return entries;
}
