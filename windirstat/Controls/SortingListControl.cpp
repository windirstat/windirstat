// sortinglistcontrol.cpp - Implementation of CSortingListItem and CSortingListControl
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

CSortingListControl::CSortingListControl(LPCWSTR name)
{
    m_name            = name;
    m_indicatedColumn = -1;
}

void CSortingListControl::LoadPersistentAttributes()
{
    int i = 0;
    CArray<int, int> arr;
    arr.SetSize(GetHeaderCtrl()->GetItemCount());

    GetColumnOrderArray(arr.GetData(), static_cast<int>(arr.GetSize()));
    CPersistence::GetColumnOrder(m_name.c_str(), arr);
    SetColumnOrderArray(static_cast<int>(arr.GetSize()), arr.GetData());

    for (i = 0; i < arr.GetSize(); i++)
    {
        arr[i] = GetColumnWidth(i);
    }
    CPersistence::GetColumnWidths(m_name.c_str(), arr);
    for (i = 0; i < arr.GetSize(); i++)
    {
        // To avoid "insane" settings we set the column width to
        // maximal twice the default width.
        int maxWidth = GetColumnWidth(i) * 2;
        const int w  = min(arr[i], maxWidth);
        SetColumnWidth(i, w);
    }

    // Not so good: CPersistence::GetSorting(m_name, GetHeaderCtrl()->GetItemCount(), m_sorting.column1, m_sorting.ascending1, m_sorting.column2, m_sorting.ascending2);
    // We refrain from saving the sorting because it is too likely, that
    // users start up with insane settings and don't get it.
}

void CSortingListControl::SavePersistentAttributes() const
{
    CArray<int, int> arr;
    arr.SetSize(GetHeaderCtrl()->GetItemCount());

    GetColumnOrderArray(arr.GetData(), static_cast<int>(arr.GetSize()));
    CPersistence::SetColumnOrder(m_name.c_str(), arr);

    for (int i = 0; i < arr.GetSize(); i++)
    {
        arr[i] = GetColumnWidth(i);
    }
    CPersistence::SetColumnWidths(m_name.c_str(), arr);

    // Not so good: CPersistence::SetSorting(m_name, m_sorting.column1, m_sorting.ascending1, m_sorting.column2, m_sorting.ascending2);
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
    lvitem.lParam  = (LPARAM)item;

    VERIFY(i == CListCtrl::InsertItem(&lvitem));
}

CSortingListItem* CSortingListControl::GetSortingListItem(int i)
{
    return (CSortingListItem*)GetItemData(i);
}

void CSortingListControl::SortItems()
{
    VERIFY(CListCtrl::SortItems(&_CompareFunc, (DWORD_PTR)&m_sorting));

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
    const CSortingListItem* item1 = (CSortingListItem*)lParam1;
    const CSortingListItem* item2 = (CSortingListItem*)lParam2;
    const SSorting* sorting       = (SSorting*)lParamSort;

    return item1->CompareS(item2, *sorting);
}

BEGIN_MESSAGE_MAP(CSortingListControl, CListCtrl)
#pragma warning(suppress: 26454)
    ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnLvnGetdispinfo)
#pragma warning(suppress: 26454)
    ON_NOTIFY(HDN_ITEMCLICKA, 0, OnHdnItemclick)
#pragma warning(suppress: 26454)
    ON_NOTIFY(HDN_ITEMCLICKW, 0, OnHdnItemclick)
#pragma warning(suppress: 26454)
    ON_NOTIFY(HDN_ITEMDBLCLICKA, 0, OnHdnItemdblclick)
#pragma warning(suppress: 26454)
    ON_NOTIFY(HDN_ITEMDBLCLICKW, 0, OnHdnItemdblclick)
    ON_WM_DESTROY()
END_MESSAGE_MAP()

void CSortingListControl::OnLvnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVDISPINFO* di = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
    *pResult         = 0;

    const CSortingListItem* item = (CSortingListItem*)di->item.lParam;

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
    LPNMHEADER phdr = reinterpret_cast<LPNMHEADER>(pNMHDR);
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
