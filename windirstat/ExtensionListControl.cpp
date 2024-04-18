// CExtensionListControl.cpp - Implementation of CExtensionListControl
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
#include "Item.h"
#include "MainFrame.h"
#include "DirStatDoc.h"
#include <common/CommonHelpers.h>
#include "ExtensionView.h"
#include "GlobalHelpers.h"
#include "Localization.h"
#include "ExtensionListControl.h"

/////////////////////////////////////////////////////////////////////////////

CExtensionListControl::CListItem::CListItem(CExtensionListControl* list, LPCWSTR extension, const SExtensionRecord& r)
{
    m_list = list;
    m_extension = extension;
    m_record = r;
}

bool CExtensionListControl::CListItem::DrawSubitem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) const
{
    if (subitem == COL_EXTENSION)
    {
        DrawLabel(m_list, GetIconImageList(), pdc, rc, state, width, focusLeft);
    }
    else if (subitem == COL_COLOR)
    {
        DrawColor(pdc, rc, state, width);
    }
    else
    {
        return false;
    }

    return true;
}

void CExtensionListControl::CListItem::DrawColor(CDC* pdc, CRect rc, UINT state, int* width) const
{
    if (width != nullptr)
    {
        *width = 40;
        return;
    }

    DrawSelection(m_list, pdc, rc, state);

    rc.DeflateRect(2, 3);

    if (rc.right <= rc.left || rc.bottom <= rc.top)
    {
        return;
    }

    CTreemap treemap;
    treemap.DrawColorPreview(pdc, rc, m_record.color, &COptions::TreemapOptions);
}

CStringW CExtensionListControl::CListItem::GetText(int subitem) const
{
    switch (subitem)
    {
        case COL_EXTENSION: return GetExtension();
        case COL_COLOR: return {};
        case COL_BYTES: return FormatBytes(m_record.bytes);
        case COL_FILES: return FormatCount(m_record.files);
        case COL_DESCRIPTION: return GetDescription();
        case COL_BYTESPERCENT: return GetBytesPercent();
        default: ASSERT(FALSE); return {};
    }
}

CStringW CExtensionListControl::CListItem::GetExtension() const
{
    return m_extension;
}

int CExtensionListControl::CListItem::GetImage() const
{
    if (m_image == -1)
    {
        m_image = GetIconImageList()->getExtImageAndDescription(m_extension, m_description, 0);
    }
    return m_image;
}

CStringW CExtensionListControl::CListItem::GetDescription() const
{
    if (m_description.IsEmpty())
    {
        m_image = GetIconImageList()->getExtImageAndDescription(m_extension, m_description, 0);
    }

    if (m_extension.IsEmpty())
    {
        m_image = GetIconImageList()->getUnknownImage();
        m_description = Localization::Lookup(IDS_EXTENSION_MISSING);
    }

    return m_description;
}

CStringW CExtensionListControl::CListItem::GetBytesPercent() const
{
    CStringW s;
    s.Format(L"%s%%", FormatDouble(GetBytesFraction() * 100).GetString());
    return s;
}

double CExtensionListControl::CListItem::GetBytesFraction() const
{
    if (m_list->GetRootSize() == 0)
    {
        return 0;
    }

    return static_cast<double>(m_record.bytes) /
        static_cast<double>(m_list->GetRootSize());
}

int CExtensionListControl::CListItem::Compare(const CSortingListItem* baseOther, int subitem) const
{
    const auto other = static_cast<const CListItem*>(baseOther);

    switch (subitem)
    {
        case COL_COLOR:
        case COL_BYTES: return usignum(m_record.bytes, other->m_record.bytes);
        case COL_EXTENSION: return signum(GetExtension().CompareNoCase(other->GetExtension()));
        case COL_FILES: return usignum(m_record.files, other->m_record.files);
        case COL_DESCRIPTION: return signum(GetDescription().CompareNoCase(other->GetDescription()));
        case COL_BYTESPERCENT: return signum(GetBytesFraction() - other->GetBytesFraction());
        default: ASSERT(FALSE); return 0;
    }
}

/////////////////////////////////////////////////////////////////////////////

#pragma warning(push)
#pragma warning(disable:26454)
BEGIN_MESSAGE_MAP(CExtensionListControl, COwnerDrawnListControl)
    ON_WM_MEASUREITEM_REFLECT()
    ON_WM_DESTROY()
    ON_NOTIFY_REFLECT(LVN_DELETEITEM, OnLvnDeleteitem)
    ON_WM_SETFOCUS()
    ON_NOTIFY_REFLECT(LVN_ITEMCHANGED, OnLvnItemchanged)
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()
#pragma warning(pop)

CExtensionListControl::CExtensionListControl(CExtensionView* extensionView)
    : COwnerDrawnListControl(19, COptions::TypesColumnOrder.Ptr(), COptions::TypesColumnWidths.Ptr()) // FIXME: Harcoded value
    , m_extensionView(extensionView) {}

bool CExtensionListControl::GetAscendingDefault(const int column)
{
    switch (column)
    {
        case COL_EXTENSION: 
        case COL_BYTESPERCENT: 
        case COL_DESCRIPTION: return true;
        case COL_COLOR: 
        case COL_BYTES: 
        case COL_FILES: return false;
        default: ASSERT(FALSE); return true;
    }
}

// As we will not receive WM_CREATE, we must do initialization
// in this extra method. The counterpart is OnDestroy().
void CExtensionListControl::Initialize()
{
    SetSorting(COL_BYTES, false);

    InsertColumn(SHORT_MAX, Localization::Lookup(IDS_COL_EXTENSION), LVCFMT_LEFT, 60, COL_EXTENSION);
    InsertColumn(SHORT_MAX, Localization::Lookup(IDS_COL_COLOR), LVCFMT_LEFT, 40, COL_COLOR);
    InsertColumn(SHORT_MAX, Localization::Lookup(IDS_COL_BYTES), LVCFMT_RIGHT, 60, COL_BYTES);
    InsertColumn(SHORT_MAX, L"% " + Localization::Lookup(IDS_COL_BYTES), LVCFMT_RIGHT, 50, COL_BYTESPERCENT);
    InsertColumn(SHORT_MAX, Localization::Lookup(IDS_COL_FILES), LVCFMT_RIGHT, 50, COL_FILES);
    InsertColumn(SHORT_MAX, Localization::Lookup(IDS_COL_DESCRIPTION), LVCFMT_LEFT, 170, COL_DESCRIPTION);

    OnColumnsInserted();

    // We don't use the list control's image list, but attaching an image list
    // to the control ensures a proper line height.
    SetImageList(GetIconImageList(), LVSIL_SMALL);
}

void CExtensionListControl::OnDestroy()
{
    COwnerDrawnListControl::OnDestroy();
}

void CExtensionListControl::SetExtensionData(const CExtensionData* ed)
{
    DeleteAllItems();

    int i = 0;
    POSITION pos = ed->GetStartPosition();
    while (pos != nullptr)
    {
        CStringW ext;
        SExtensionRecord r;
        ed->GetNextAssoc(pos, ext, r);

        const auto item = new CListItem(this, ext, r);
        InsertListItem(i++, item);
    }

    SortItems();
}

void CExtensionListControl::SetRootSize(ULONGLONG totalBytes)
{
    m_rootSize = totalBytes;
}

ULONGLONG CExtensionListControl::GetRootSize() const
{
    return m_rootSize;
}

void CExtensionListControl::SelectExtension(const LPCWSTR ext)
{
    for (int i = 0; i < GetItemCount(); i++)
    {
        if (GetListItem(i)->GetExtension().CompareNoCase(ext) == 0)
        {
            SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            EnsureVisible(i, false);
            return;
        }
    }
}

CStringW CExtensionListControl::GetSelectedExtension() const
{
    POSITION pos = GetFirstSelectedItemPosition();
    if (pos == nullptr)
    {
        return wds::strEmpty;
    }

    const int i = GetNextSelectedItem(pos);
    const CListItem* item = GetListItem(i);
    return item->GetExtension();
}

CExtensionListControl::CListItem* CExtensionListControl::GetListItem(int i) const
{
    return reinterpret_cast<CListItem*>(GetItemData(i));
}

void CExtensionListControl::OnLvnDeleteitem(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto lv = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    delete reinterpret_cast<CListItem*>(lv->lParam);
    *pResult = FALSE;
}

void CExtensionListControl::MeasureItem(LPMEASUREITEMSTRUCT mis)
{
    mis->itemHeight = GetRowHeight();
}

void CExtensionListControl::OnSetFocus(CWnd* pOldWnd)
{
    COwnerDrawnListControl::OnSetFocus(pOldWnd);
    CMainFrame::Get()->SetLogicalFocus(LF_EXTENSIONLIST);
}

void CExtensionListControl::OnLvnItemchanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    if ((pNMLV->uNewState & LVIS_SELECTED) != 0)
    {
        m_extensionView->SetHighlightExtension(GetSelectedExtension());
    }
    *pResult = FALSE;
}

void CExtensionListControl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_TAB)
    {
        CMainFrame::Get()->MoveFocus(LF_DIRECTORYLIST);
    }
    else if (nChar == VK_ESCAPE)
    {
        CMainFrame::Get()->MoveFocus(LF_NONE);
    }
    COwnerDrawnListControl::OnKeyDown(nChar, nRepCnt, nFlags);
}
