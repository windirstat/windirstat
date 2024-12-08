// ExtensionListControl.cpp - Implementation of CExtensionListControl
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
#include "MainFrame.h"
#include "DirStatDoc.h"
#include "ExtensionView.h"
#include "GlobalHelpers.h"
#include "Localization.h"
#include "ExtensionListControl.h"

/////////////////////////////////////////////////////////////////////////////

CExtensionListControl::CListItem::CListItem(CExtensionListControl* list, const std::wstring& extension, const SExtensionRecord& r)
{
    m_List = list;
    m_Extension = extension;
    m_Bytes = r.bytes;
    m_Files = r.files;
    m_Color = r.color;
}

bool CExtensionListControl::CListItem::DrawSubitem(const int subitem, CDC* pdc, CRect rc, const UINT state, int* width, int* focusLeft) const
{
    if (subitem == COL_EXT_EXTENSION)
    {
        DrawLabel(m_List, GetIconImageList(), pdc, rc, state, width, focusLeft);
    }
    else if (subitem == COL_EXT_COLOR)
    {
        DrawColor(pdc, rc, state, width);
    }
    else
    {
        return false;
    }

    return true;
}

void CExtensionListControl::CListItem::DrawColor(CDC* pdc, CRect rc, const UINT state, int* width) const
{
    if (width != nullptr)
    {
        *width = 40;
        return;

    }

    DrawSelection(m_List, pdc, rc, state);

    rc.DeflateRect(2, 3);

    if (rc.right <= rc.left || rc.bottom <= rc.top)
    {
        return;
    }

    CTreeMap treemap;
    treemap.DrawColorPreview(pdc, rc, m_Color, &COptions::TreeMapOptions);
}

std::wstring CExtensionListControl::CListItem::GetText(const int subitem) const
{
    switch (subitem)
    {
        case COL_EXT_EXTENSION: return GetExtension();
        case COL_EXT_COLOR: return {};
        case COL_EXT_BYTES: return FormatBytes(m_Bytes);
        case COL_EXT_FILES: return FormatCount(m_Files);
        case COL_EXT_DESCRIPTION: return GetDescription();
        case COL_EXT_BYTESPERCENT: return GetBytesPercent();
        default: ASSERT(FALSE); return {};
    }
}

std::wstring CExtensionListControl::CListItem::GetExtension() const
{
    return m_Extension;
}

void CExtensionListControl::CListItem::FetchShellInfo()
{
    if (m_Extension.empty())
    {
        m_Description = Localization::Lookup(IDS_EXTENSION_MISSING);
        m_Image = GetIconImageList()->GetUnknownImage();
    }
    else
    {
        m_Image = GetIconImageList()->GetExtImageAndDescription(m_Extension, m_Description, 0);
    }

    const auto i = m_List->FindListItem(this);
    m_List->RedrawItems(i, i);
}

int CExtensionListControl::CListItem::GetImage() const
{
    if (m_Image == -1)
    {
        GetIconImageList()->DoAsyncShellInfoLookup(const_cast<CListItem*>(this));
    }

    return m_Image;
}

std::wstring CExtensionListControl::CListItem::GetDescription() const
{
    return m_Description;
}

std::wstring CExtensionListControl::CListItem::GetBytesPercent() const
{
    return FormatDouble(GetBytesFraction() * 100) + L"%";
}

double CExtensionListControl::CListItem::GetBytesFraction() const
{
    if (m_List->GetRootSize() == 0)
    {
        return 0;
    }

    return static_cast<double>(m_Bytes) /
        static_cast<double>(m_List->GetRootSize());
}

int CExtensionListControl::CListItem::Compare(const CSortingListItem* baseOther, const int subitem) const
{
    const auto other = static_cast<const CListItem*>(baseOther);

    switch (subitem)
    {
        case COL_EXT_COLOR:
        case COL_EXT_BYTES: return usignum(m_Bytes, other->m_Bytes);
        case COL_EXT_EXTENSION: return signum(_wcsicmp(GetExtension().c_str(),other->GetExtension().c_str()));
        case COL_EXT_FILES: return usignum(m_Files, other->m_Files);
        case COL_EXT_DESCRIPTION: return signum(_wcsicmp(GetDescription().c_str(), other->GetDescription().c_str()));
        case COL_EXT_BYTESPERCENT: return signum(GetBytesFraction() - other->GetBytesFraction());
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
    : COwnerDrawnListControl(19, COptions::ExtViewColumnOrder.Ptr(), COptions::ExtViewColumnWidth.Ptr()) // FIXME: Hardcoded value
    , m_ExtensionView(extensionView) {}

bool CExtensionListControl::GetAscendingDefault(const int subitem)
{
    switch (subitem)
    {
        case COL_EXT_EXTENSION:
        case COL_EXT_BYTESPERCENT:
        case COL_EXT_DESCRIPTION: return true;
        case COL_EXT_COLOR:
        case COL_EXT_BYTES:
        case COL_EXT_FILES: return false;
        default: ASSERT(FALSE); return true;
    }
}

// As we will not receive WM_CREATE, we must do initialization
// in this extra method. The counterpart is OnDestroy().
void CExtensionListControl::Initialize()
{
    // Columns should be in the order of definition in order for sort to work
    InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_EXTENSION).c_str(), LVCFMT_LEFT, 60, COL_EXT_EXTENSION);
    InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_COLOR).c_str(), LVCFMT_LEFT, 40, COL_EXT_COLOR);
    InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_DESCRIPTION).c_str(), LVCFMT_LEFT, 170, COL_EXT_DESCRIPTION);
    InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_BYTES).c_str(), LVCFMT_RIGHT, 60, COL_EXT_BYTES);
    InsertColumn(CHAR_MAX, (L"% " + Localization::Lookup(IDS_COL_BYTES)).c_str(), LVCFMT_RIGHT, 50, COL_EXT_BYTESPERCENT);
    InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_FILES).c_str(), LVCFMT_RIGHT, 50, COL_EXT_FILES);

    SetSorting(COL_EXT_BYTES, GetAscendingDefault(COL_EXT_BYTES));

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

    for (int i = 0; const auto & ext : *ed)
    {
        const auto item = new CListItem(this, ext.first, ext.second);
        InsertListItem(i++, item);
    }

    SortItems();
}

void CExtensionListControl::SetRootSize(const ULONGLONG totalBytes)
{
    m_RootSize = totalBytes;
}

ULONGLONG CExtensionListControl::GetRootSize() const
{
    return m_RootSize;
}

void CExtensionListControl::SelectExtension(const std::wstring & ext)
{
    for (int i = 0, iMax = GetItemCount(); i < iMax; i++)
    {
        if (_wcsicmp(GetListItem(i)->GetExtension().c_str(), ext.c_str()) == 0)
        {
            SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            EnsureVisible(i, false);
            return;
        }
    }
}

std::wstring CExtensionListControl::GetSelectedExtension() const
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

CExtensionListControl::CListItem* CExtensionListControl::GetListItem(const int i) const
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
        m_ExtensionView->SetHighlightExtension(GetSelectedExtension());
    }
    *pResult = FALSE;
}

void CExtensionListControl::OnKeyDown(const UINT nChar, const UINT nRepCnt, const UINT nFlags)
{
    if (nChar == VK_TAB)
    {
        CMainFrame::Get()->MoveFocus(LF_FILETREE);
    }
    else if (nChar == VK_ESCAPE)
    {
        CMainFrame::Get()->MoveFocus(LF_NONE);
    }
    COwnerDrawnListControl::OnKeyDown(nChar, nRepCnt, nFlags);
}
