//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
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
#include "WinDirStat.h"
#include "SortingListControl.h"

/////////////////////////////////////////////////////////////////////////////

// Return value:
// <= -2:   this is less than other regardless of ascending flag
// -1:      this is less than other
// 0:       this equals other
// +1:      this is greater than other
// >= +1:   this is greater than other regardless of ascending flag.
//
int CSortingListItem::Compare(const CSortingListItem* other, const int subitem) const
{
    // Default implementation compares strings
    return signum(_wcsicmp(GetText(subitem).c_str(),other->GetText(subitem).c_str()));
}

int CSortingListItem::CompareS(const CSortingListItem* other, const SSorting& sorting) const
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
        for (int i = 0; i < static_cast<int>(m_ColumnWidths->size()); i++)
        {
            (*m_ColumnWidths)[i] = GetColumnWidth(i);
        }
    }
    
    // Set based on persisted values
    SetColumnOrderArray(static_cast<int>(m_ColumnOrder->size()), m_ColumnOrder->data());
    for (int i = 0; i < static_cast<int>(m_ColumnWidths->size()); i++)
    {
        SetColumnWidth(i, min((*m_ColumnWidths)[i], (*m_ColumnWidths)[i] * 2));
    }
}

void CSortingListControl::SavePersistentAttributes() const
{
    GetColumnOrderArray(m_ColumnOrder->data(), static_cast<int>(m_ColumnOrder->size()));
    for (int i = 0; i < static_cast<int>(m_ColumnWidths->size()); i++)
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
    LVCOLUMN column_info;
    column_info.mask = LVCF_SUBITEM;
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
    lvitem.mask = LVIF_TEXT | LVIF_PARAM | (HasImages() ? LVIF_IMAGE : 0);
    lvitem.iItem = i;
    lvitem.pszText = LPSTR_TEXTCALLBACK;
    lvitem.iImage = I_IMAGECALLBACK;
    lvitem.lParam = reinterpret_cast<LPARAM>(item);
    lvitem.stateMask = 0;
    lvitem.iSubItem = 0;
    VERIFY(i == CListCtrl::InsertItem(&lvitem));
}

CSortingListItem* CSortingListControl::GetSortingListItem(const int i) const
{
    return reinterpret_cast<CSortingListItem*>(GetItemData(i));
}

void CSortingListControl::SortItems()
{
    VERIFY(CListCtrl::SortItems([](LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
        const CSortingListItem* item1 = reinterpret_cast<CSortingListItem*>(lParam1);
        const CSortingListItem* item2 = reinterpret_cast<CSortingListItem*>(lParam2);
        const SSorting* sorting = reinterpret_cast<SSorting*>(lParamSort);
        return item1->CompareS(item2, *sorting); }, reinterpret_cast<DWORD_PTR>(&m_Sorting)));

    if (m_IndicatedColumn != -1)
    {
        HDITEM hditem;
        std::wstring text;
        text.resize(256);
        hditem.mask = HDI_TEXT;
        hditem.pszText = text.data();
        hditem.cchTextMax = 256;
        GetHeaderCtrl()->GetItem(m_IndicatedColumn, &hditem);
        text.resize(wcslen(text.data()));
        text = text.substr(2);
        hditem.pszText = text.data();
        GetHeaderCtrl()->SetItem(m_IndicatedColumn, &hditem);
    }

    HDITEM hditem;
    std::wstring text;
    text.resize(256);
    hditem.mask = HDI_TEXT;
    hditem.pszText = text.data();
    hditem.cchTextMax = 256;
    GetHeaderCtrl()->GetItem(m_Sorting.column1, &hditem);
    text.resize(wcslen(text.data()));
    text = (m_Sorting.ascending1 ? L"< " : L"> ") + text;
    hditem.pszText = text.data();
    GetHeaderCtrl()->SetItem(m_Sorting.column1, &hditem);
    m_IndicatedColumn = m_Sorting.column1;
}

bool CSortingListControl::GetAscendingDefault(int /*column*/)
{
    return true;
}

bool CSortingListControl::HasImages()
{
    return false;
}

#pragma warning(push)
#pragma warning(disable:26454)
BEGIN_MESSAGE_MAP(CSortingListControl, CListCtrl)
    ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnLvnGetdispinfo)
    ON_NOTIFY(HDN_ITEMCLICK, 0, OnHdnItemclick)
    ON_NOTIFY(HDN_ITEMDBLCLICK, 0, OnHdnItemdblclick)
    ON_WM_DESTROY()
END_MESSAGE_MAP()
#pragma warning(pop)

void CSortingListControl::OnLvnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVDISPINFO* di = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
    *pResult = FALSE;

    const CSortingListItem* item = reinterpret_cast<CSortingListItem*>(di->item.lParam);

    if ((di->item.mask & LVIF_TEXT) != 0)
    {
        wcscpy_s(di->item.pszText, di->item.cchTextMax, item->GetText(di->item.iSubItem).c_str());
    }

    if ((di->item.mask & LVIF_IMAGE) != 0)
    {
        di->item.iImage = item->GetImage();
    }
}

void CSortingListControl::OnHdnItemclick(NMHDR* pNMHDR, LRESULT* pResult)
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

void CSortingListControl::OnHdnItemdblclick(NMHDR* pNMHDR, LRESULT* pResult)
{
    OnHdnItemclick(pNMHDR, pResult);
}

void CSortingListControl::OnDestroy()
{
    SavePersistentAttributes();
    CListCtrl::OnDestroy();
}
