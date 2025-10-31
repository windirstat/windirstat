﻿// WinDirStat - Directory Statistics
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

#include <string>

//
// SSorting. A sorting specification. We sort by column1, and if two items
// equal in column1, we sort them by column2.
//
struct SSorting
{
    int column1 = 0;
    int column2 = 0;
    int subitem1 = 0;
    int subitem2 = 0;
    bool ascending1 = true;
    bool ascending2 = true;
};

//
// CSortingListItem. An item in a CSortingListControl.
//
class CSortingListItem
{
public:
    virtual ~CSortingListItem() = default;
    virtual std::wstring GetText(int subitem) const = 0;
    virtual int Compare(const CSortingListItem* other, int subitem) const = 0;
    int CompareSort(const CSortingListItem* other, const SSorting& sorting) const;
};

//
// CSortingListControl. The base class for all our ListControls.
// The lParams of the items are pointers to CSortingListItems.
// The items use LPSTR_TEXTCALLBACK and I_IMAGECALLBACK.
// And the items can compare to one another.
// CSortingListControl maintains a SSorting and handles clicks
// on the header items. It also indicates the sorting to the user
// by adding a "<" or ">" to the header items.
//
class CSortingListControl : public CListCtrl
{
    DECLARE_DYNAMIC(CSortingListControl)

    std::vector<int>* m_ColumnOrder;
    std::vector<int>* m_ColumnWidths;

    // Construction
    CSortingListControl(std::vector<int>* columnOrder, std::vector<int>* columnWidths);
    ~CSortingListControl() override = default;

    // Public methods
    void LoadPersistentAttributes();

    void AddExtendedStyle(DWORD exStyle);
    void RemoveExtendedStyle(DWORD exStyle);

    const SSorting& GetSorting() const;

    int ColumnToSubItem(int col) const;
    void SetSorting(const SSorting& sorting);
    void SetSorting(int sortColumn1, bool ascending1, int sortColumn2, bool ascending2);
    void SetSorting(int sortColumn, bool ascending);
    void InsertListItem(int i, CSortingListItem* item);

    // Overridables
    virtual void SortItems();
    virtual bool GetAscendingDefault(int column);

private:
    void SavePersistentAttributes() const;
 
    std::wstring m_Name; // for persistence
    SSorting m_Sorting;

    int m_IndicatedColumn = -1;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnLvnGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnHdnItemClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnHdnItemDblClick(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnDestroy();
};
