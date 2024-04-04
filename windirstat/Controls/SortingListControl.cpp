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
//

#include "stdafx.h"
#include "WinDirStat.h"
#include "Options.h"
#include "SortingListControl.h"

/////////////////////////////////////////////////////////////////////////////

CStringW CSortingListItem::GetText(int subitem) const
{
    // Dummy implementation
    CStringW s;
    s.Format(L"subitem %d", subitem);
    return s;
}

int CSortingListItem::GetImage() const
{
    // Dummy implementation
    return 0;
}

// Return value:
// <= -2:   this is less than other regardless of ascending flag
// -1:      this is less than other
// 0:       this equals other
// +1:      this is greater than other
// >= +1:   this is greater than other regardless of ascending flag.
//
int CSortingListItem::Compare(const CSortingListItem* other, int subitem) const
{
    // Default implementation compares strings
    return signum(GetText(subitem).CompareNoCase(other->GetText(subitem)));
}

int CSortingListItem::CompareS(const CSortingListItem* other, const SSorting& sorting) const
{
    int r = Compare(other, sorting.column1);
    if (abs(r) < 2 && !sorting.ascending1)
    {
        r = -r;
    }

    if (r == 0 && sorting.column2 != sorting.column1)
    {
        r = Compare(other, sorting.column2);
        if (abs(r) < 2 && !sorting.ascending2)
        {
            r = -r;
        }
    }
    return r;
}

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CSortingListControl, CListCtrl)

CSortingListControl::CSortingListControl(std::vector<int>* column_order, std::vector<int>* column_widths)
{
    m_column_order = column_order;
    m_column_widths = column_widths;
    m_indicatedColumn = -1;
}

void CSortingListControl::LoadPersistentAttributes()
{
    // Fetch casted column count to avoid signed comparison warnings
    const auto column_count = static_cast<size_t>(GetHeaderCtrl()->GetItemCount());

    // Load default column order values from resource
    if (m_column_order->size() != column_count)
    {
        m_column_order->resize(column_count);
        GetColumnOrderArray(m_column_order->data(), static_cast<int>(m_column_order->size()));
    }

    // Load default column width values from resource
    if (m_column_widths->size() != column_count)
    {
        m_column_widths->resize(column_count,0);
        for (int i = 0; i < static_cast<int>(m_column_widths->size()); i++)
        {
            (*m_column_widths)[i] = GetColumnWidth(i);
        }
    }
    
    // Set based on persisted values
    SetColumnOrderArray(static_cast<int>(m_column_order->size()), m_column_order->data());
    for (int i = 0; i < static_cast<int>(m_column_widths->size()); i++)
    {
        SetColumnWidth(i, min((*m_column_widths)[i], (*m_column_widths)[i] * 2));
    }
}

void CSortingListControl::SavePersistentAttributes() const
{
    GetColumnOrderArray(m_column_order->data(), static_cast<int>(m_column_order->size()));
    for (int i = 0; i < static_cast<int>(m_column_widths->size()); i++)
    {
        (*m_column_widths)[i] = GetColumnWidth(i);
    }
}

void CSortingListControl::AddExtendedStyle(DWORD exStyle)
{
    SetExtendedStyle(GetExtendedStyle() | exStyle);
}

void CSortingListControl::RemoveExtendedStyle(DWORD exStyle)
{
    SetExtendedStyle(GetExtendedStyle() & ~exStyle);
}

const SSorting& CSortingListControl::GetSorting() const
{
    return m_sorting;
}

void CSortingListControl::SetSorting(const SSorting& sorting)
{
    m_sorting = sorting;
}

void CSortingListControl::SetSorting(int sortColumn1, bool ascending1, int sortColumn2, bool ascending2)
{
    m_sorting.column1    = sortColumn1;
    m_sorting.ascending1 = ascending1;
    m_sorting.column2    = sortColumn2;
    m_sorting.ascending2 = ascending2;
}

void CSortingListControl::SetSorting(int sortColumn, bool ascending)
{
    m_sorting.column2    = m_sorting.column1;
    m_sorting.ascending2 = m_sorting.ascending1;
    m_sorting.column1    = sortColumn;
    m_sorting.ascending1 = ascending;
}

void CSortingListControl::InsertListItem(int i, CSortingListItem* item)
{
    LVITEM lvitem;
    ZeroMemory(&lvitem, sizeof(lvitem));

    lvitem.mask = LVIF_TEXT | LVIF_PARAM;
    if (HasImages())
    {
        lvitem.mask |= LVIF_IMAGE;
    }

    lvitem.iItem   = i;
    lvitem.pszText = LPSTR_TEXTCALLBACK;
    lvitem.iImage  = I_IMAGECALLBACK;
    lvitem.lParam  = reinterpret_cast<LPARAM>(item);

    VERIFY(i == CListCtrl::InsertItem(&lvitem));
}

CSortingListItem* CSortingListControl::GetSortingListItem(int i) const
{
    return reinterpret_cast<CSortingListItem*>(GetItemData(i));
}

void CSortingListControl::SortItems()
{
    VERIFY(CListCtrl::SortItems(&_CompareFunc, reinterpret_cast<DWORD_PTR>(&m_sorting)));

    HDITEM hditem;
    ZeroMemory(&hditem, sizeof(hditem));

    if (m_indicatedColumn != -1)
    {
        CStringW text;
        hditem.mask       = HDI_TEXT;
        hditem.pszText    = text.GetBuffer(256);
        hditem.cchTextMax = 256;
        GetHeaderCtrl()->GetItem(m_indicatedColumn, &hditem);
        text.ReleaseBuffer();
        text           = text.Mid(2);
        hditem.pszText = const_cast<LPWSTR>(static_cast<LPCWSTR>(text));
        GetHeaderCtrl()->SetItem(m_indicatedColumn, &hditem);
    }

    CStringW text;
    hditem.mask       = HDI_TEXT;
    hditem.pszText    = text.GetBuffer(256);
    hditem.cchTextMax = 256;
    GetHeaderCtrl()->GetItem(m_sorting.column1, &hditem);
    text.ReleaseBuffer();
    text           = (m_sorting.ascending1 ? L"< " : L"> ") + text;
    hditem.pszText = const_cast<LPWSTR>(static_cast<LPCWSTR>(text));
    GetHeaderCtrl()->SetItem(m_sorting.column1, &hditem);
    m_indicatedColumn = m_sorting.column1;
}

bool CSortingListControl::GetAscendingDefault(int /*column*/)
{
    return true;
}

bool CSortingListControl::HasImages()
{
    return false;
}

int CALLBACK CSortingListControl::_CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    const CSortingListItem* item1 = reinterpret_cast<CSortingListItem*>(lParam1);
    const CSortingListItem* item2 = reinterpret_cast<CSortingListItem*>(lParam2);
    const SSorting* sorting       = reinterpret_cast<SSorting*>(lParamSort);

    return item1->CompareS(item2, *sorting);
}

BEGIN_MESSAGE_MAP(CSortingListControl, CListCtrl)
    ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnLvnGetdispinfo)
    ON_NOTIFY(HDN_ITEMCLICKA, 0, OnHdnItemclick)
    ON_NOTIFY(HDN_ITEMCLICKW, 0, OnHdnItemclick)
    ON_NOTIFY(HDN_ITEMDBLCLICKA, 0, OnHdnItemdblclick)
    ON_NOTIFY(HDN_ITEMDBLCLICKW, 0, OnHdnItemdblclick)
    ON_WM_DESTROY()
END_MESSAGE_MAP()

void CSortingListControl::OnLvnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVDISPINFO* di = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
    *pResult         = 0;

    const CSortingListItem* item = reinterpret_cast<CSortingListItem*>(di->item.lParam);

    if ((di->item.mask & LVIF_TEXT) != 0)
    {
        wcscpy_s(di->item.pszText, di->item.cchTextMax, item->GetText(di->item.iSubItem));
    }

    if ((di->item.mask & LVIF_IMAGE) != 0)
    {
        di->item.iImage = item->GetImage();
    }
}

void CSortingListControl::OnHdnItemclick(NMHDR* pNMHDR, LRESULT* pResult)
{
    const LPNMHEADER phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
    *pResult        = 0;
    const int col   = phdr->iItem;

    if (col == m_sorting.column1)
    {
        m_sorting.ascending1 = !m_sorting.ascending1;
    }
    else
    {
        SetSorting(col, GetAscendingDefault(col));
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
