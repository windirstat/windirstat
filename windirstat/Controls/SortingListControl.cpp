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
#include "SortingListControl.h"

/////////////////////////////////////////////////////////////////////////////

int CSortingListItem::CompareSort(const CSortingListItem* other, const SSorting& sorting) const
{
    int r = Compare(other, sorting.subitem1);
    if (abs(r) < 2 && !sorting.ascending1)
    {
        r = -r;
    }

    if (r == 0 && sorting.subitem1 != sorting.subitem2)
    {
        r = Compare(other, sorting.subitem2);
        if (abs(r) < 2 && !sorting.ascending2)
        {
            r = -r;
        }
    }
    return r;
}

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CSortingListControl, CListCtrl)

CSortingListControl::CSortingListControl(std::vector<int>* columnOrder, std::vector<int>* columnWidths)
{
    m_ColumnOrder = columnOrder;
    m_ColumnWidths = columnWidths;
}

void CSortingListControl::LoadPersistentAttributes()
{
    // Fetch casted column count to avoid signed comparison warnings
    const auto columnCount = static_cast<size_t>(GetHeaderCtrl()->GetItemCount());

    // Load default column order values from resource
    if (m_ColumnOrder->size() != columnCount)
    {
        m_ColumnOrder->resize(columnCount);
        GetColumnOrderArray(m_ColumnOrder->data(), static_cast<int>(m_ColumnOrder->size()));
    }

    // Load default column width values from resource
    if (m_ColumnWidths->size() != columnCount)
    {
        m_ColumnWidths->resize(columnCount,0);
        for (const int i : std::views::iota(0, static_cast<int>(m_ColumnWidths->size())))
        {
            (*m_ColumnWidths)[i] = GetColumnWidth(i);
        }
    }
    
    // Set based on persisted values
    SetColumnOrderArray(static_cast<int>(m_ColumnOrder->size()), m_ColumnOrder->data());
    for (const int i : std::views::iota(0, static_cast<int>(m_ColumnWidths->size())))
    {
        SetColumnWidth(i, min((*m_ColumnWidths)[i], (*m_ColumnWidths)[i] * 2));
    }
}

void CSortingListControl::SavePersistentAttributes() const
{
    GetColumnOrderArray(m_ColumnOrder->data(), static_cast<int>(m_ColumnOrder->size()));
    for (const int i : std::views::iota(0, static_cast<int>(m_ColumnWidths->size())))
    {
        (*m_ColumnWidths)[i] = GetColumnWidth(i);
    }
}

void CSortingListControl::AddExtendedStyle(const DWORD exStyle)
{
    SetExtendedStyle(GetExtendedStyle() | exStyle);
}

void CSortingListControl::RemoveExtendedStyle(const DWORD exStyle)
{
    SetExtendedStyle(GetExtendedStyle() & ~exStyle);
}

const SSorting& CSortingListControl::GetSorting() const
{
    return m_Sorting;
}

int CSortingListControl::ColumnToSubItem(const int col) const
{
    LVCOLUMN column_info{ LVCF_SUBITEM };
    GetColumn(col, &column_info);
    return column_info.iSubItem;
}

void CSortingListControl::SetSorting(const SSorting& sorting)
{
    m_Sorting = sorting;
}

void CSortingListControl::SetSorting(const int sortColumn1, const bool ascending1, const int sortColumn2, const bool ascending2)
{
    m_Sorting.column1    = sortColumn1;
    m_Sorting.subitem1   = ColumnToSubItem(sortColumn1);
    m_Sorting.ascending1 = ascending1;
    m_Sorting.column2    = sortColumn2;
    m_Sorting.subitem2   = ColumnToSubItem(sortColumn2);
    m_Sorting.ascending2 = ascending2;
}

void CSortingListControl::SetSorting(const int sortColumn, const bool ascending)
{
    m_Sorting.column2    = m_Sorting.column1;
    m_Sorting.subitem2   = m_Sorting.subitem1;
    m_Sorting.ascending2 = m_Sorting.ascending1;
    m_Sorting.column1    = sortColumn;
    m_Sorting.ascending1 = ascending;
    m_Sorting.subitem1   = ColumnToSubItem(sortColumn);
}

void CSortingListControl::InsertListItem(const int i, CSortingListItem* item)
{
    LVITEM lvitem;
    lvitem.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
    lvitem.iItem = i;
    lvitem.pszText = LPSTR_TEXTCALLBACK;
    lvitem.iImage = I_IMAGECALLBACK;
    lvitem.lParam = reinterpret_cast<LPARAM>(item);
    lvitem.stateMask = 0;
    lvitem.iSubItem = 0;
    VERIFY(i == CListCtrl::InsertItem(&lvitem));
}

/*
 * Sorts the list control's items and updates the header to display the correct sorting indicator.
 * This method reorders the list control's items based on the current sorting column and direction.
 * It then updates the header control by using native Windows header flags (HDF_SORTUP and HDF_SORTDOWN)
 * to display a platform-consistent sorting arrow. This approach is superior to manually
 * changing the header text with Unicode characters, which caused visual misalignment.
 */
void CSortingListControl::SortItems()
{
    // Reorder the list items based on the current sorting criteria using a lambda comparison function.
    VERIFY(CListCtrl::SortItems([](LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
        const CSortingListItem* item1 = reinterpret_cast<CSortingListItem*>(lParam1);
        const CSortingListItem* item2 = reinterpret_cast<CSortingListItem*>(lParam2);
        const SSorting* sorting = reinterpret_cast<SSorting*>(lParamSort);
        return item1->CompareSort(item2, *sorting); }, reinterpret_cast<DWORD_PTR>(&m_Sorting)));

    CHeaderCtrl* pHeaderCtrl = GetHeaderCtrl();
    
    // Exit if the header control is unavailable, to prevent a null pointer crash.
    if (pHeaderCtrl == nullptr)
    {
        return;
    }

    HDITEM hditem;
    hditem.mask = HDI_FORMAT;

    // Remove the sort indicator from the previously sorted column if one exists.
    if (m_IndicatedColumn != -1)
    {
        pHeaderCtrl->GetItem(m_IndicatedColumn, &hditem);
        // Use a bitwise operation to clear both the UP and DOWN sort flags.
        hditem.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        pHeaderCtrl->SetItem(m_IndicatedColumn, &hditem);
    }

    // Retrieve the newly sorted column's current format flags.
    pHeaderCtrl->GetItem(m_Sorting.column1, &hditem);
    // Clear any existing sort flags to ensure a clean state before applying the new one.
    hditem.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN); 

    // Apply the correct native sorting indicator based on the sort direction using a ternary operator.
    hditem.fmt |= m_Sorting.ascending1 ? HDF_SORTUP : HDF_SORTDOWN;

    pHeaderCtrl->SetItem(m_Sorting.column1, &hditem);

    // Store the current sorted column's index to be cleared next time.
    m_IndicatedColumn = m_Sorting.column1;
}

bool CSortingListControl::GetAscendingDefault(int /*column*/)
{
    return true;
}

BEGIN_MESSAGE_MAP(CSortingListControl, CListCtrl)
    ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnLvnGetDispInfo)
    ON_NOTIFY(HDN_ITEMCLICK, 0, OnHdnItemClick)
    ON_NOTIFY(HDN_ITEMDBLCLICK, 0, OnHdnItemDblClick)
    ON_WM_DESTROY()
END_MESSAGE_MAP()

void CSortingListControl::OnLvnGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVDISPINFO* displayInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
    *pResult = FALSE;

    const CSortingListItem* item = reinterpret_cast<CSortingListItem*>(displayInfo->item.lParam);

    if ((displayInfo->item.mask & LVIF_TEXT) != 0)
    {
        // The passed subitem value is actually the column id so translate it
        const int subitem = ColumnToSubItem(displayInfo->item.iSubItem);

        // Copy maximum allowed to the provided buffer
        wcsncpy_s(displayInfo->item.pszText, displayInfo->item.cchTextMax,
            item->GetText(subitem).c_str(), displayInfo->item.cchTextMax - 1);
    }
}

void CSortingListControl::OnHdnItemClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    const LPNMHEADER phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
    *pResult = FALSE;
    const int col = phdr->iItem;

    if (col == m_Sorting.column1)
    {
        m_Sorting.ascending1 = !m_Sorting.ascending1;
    }
    else
    {
        SetSorting(col, GetAscendingDefault(ColumnToSubItem(col)));
    }

    SortItems();
}

void CSortingListControl::OnHdnItemDblClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    OnHdnItemClick(pNMHDR, pResult);
}

void CSortingListControl::OnDestroy()
{
    SavePersistentAttributes();
    CListCtrl::OnDestroy();
}
