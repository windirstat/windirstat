// TreeListControl.cpp - Implementation of CTreeListItem and CTreeListControl
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
#include "DirStatDoc.h"
#include "DirStatView.h"
#include "SelectObject.h"
#include "TreeListControl.h"

namespace
{
    // Sequence within IDB_NODES
    enum
    {
        NODE_PLUS_SIBLING,
        NODE_PLUS_END,
        NODE_MINUS_SIBLING,
        NODE_MINUS_END,
        NODE_SIBLING,
        NODE_END,
        NODE_LINE
    };

    constexpr UINT NODE_WIDTH   = 15; // Width of a node within IDB_NODES
    constexpr UINT NODE_HEIGHT  = 24; // Height of IDB_NODES
    constexpr UINT INDENT_WIDTH = 18;

    constexpr UINT HOTNODE_CX = 9; // Size and position of the +/- buttons
    constexpr UINT HOTNODE_CY = 9;
    constexpr UINT HOTNODE_X  = 0;
}

CTreeListItem::CTreeListItem()
{
    m_parent = nullptr;
    m_vi     = nullptr;
}

CTreeListItem::~CTreeListItem()
{
    delete m_vi;
}

bool CTreeListItem::DrawSubitem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) const
{
    if (subitem != 0)
    {
        return false;
    }

    CRect rcNode = rc;
    CRect rcPlusMinus;
    GetTreeListControl()->DrawNode(pdc, rcNode, rcPlusMinus, this, width);

    CRect rcLabel = rc;
    rcLabel.left  = rcNode.right;
    DrawLabel(GetTreeListControl(), GetMyImageList(), pdc, rcLabel, state, width, focusLeft, false);

    if (width)
    {
        *width = rcLabel.right - rc.left;
    }
    else
    {
        SetPlusMinusRect(rcPlusMinus - rc.TopLeft());
        SetTitleRect(rcLabel - rc.TopLeft());
    }

    return true;
}

CStringW CTreeListItem::GetText(int /*subitem*/) const
{
    return L"test";
}

int CTreeListItem::GetImage() const
{
    ASSERT(IsVisible());
    if (m_vi->image == -1)
    {
        m_vi->image = GetImageToCache();
    }
    return m_vi->image;
}

void CTreeListItem::DrawPacman(CDC* pdc, const CRect& rc, COLORREF bgColor) const
{
    ASSERT(IsVisible());
    m_vi->pacman.SetBackgroundColor(bgColor);
    m_vi->pacman.Draw(pdc, rc);
}

void CTreeListItem::StartPacman(bool start) const
{
    if (IsVisible())
    {
        m_vi->pacman.Start(start);
    }
}

bool CTreeListItem::DrivePacman(ULONGLONG readJobs) const
{
    if (!IsVisible())
    {
        return false;
    }

    return m_vi->pacman.Drive(readJobs);
}

int CTreeListItem::GetScrollPosition()
{
    return GetTreeListControl()->GetItemScrollPosition(this);
}

void CTreeListItem::SetScrollPosition(int top)
{
    GetTreeListControl()->SetItemScrollPosition(this, top);
}

void CTreeListItem::UncacheImage()
{
    if (IsVisible())
    {
        m_vi->image = -1;
    }
}

void CTreeListItem::SortChildren()
{
    ASSERT(IsVisible());
    m_vi->sortedChildren.SetSize(GetChildrenCount());
    for (int i = 0; i < GetChildrenCount(); i++)
    {
        m_vi->sortedChildren[i] = GetTreeListChild(i);
    }

    qsort(m_vi->sortedChildren.GetData(), m_vi->sortedChildren.GetSize(), sizeof(CTreeListItem*), &_compareProc);
}

int __cdecl CTreeListItem::_compareProc(const void* p1, const void* p2)
{
    const CTreeListItem* item1 = *(CTreeListItem**)p1;
    const CTreeListItem* item2 = *(CTreeListItem**)p2;
    return item1->CompareS(item2, GetTreeListControl()->GetSorting());
}

CTreeListItem* CTreeListItem::GetSortedChild(int i)
{
    return m_vi->sortedChildren[i];
}

int CTreeListItem::Compare(const CSortingListItem* baseOther, int subitem) const
{
    const CTreeListItem* other = (CTreeListItem*)baseOther;

    if (other == this)
    {
        return 0;
    }

    if (m_parent == other->m_parent)
    {
        return CompareSibling(other, subitem);
    }

    if (m_parent == nullptr)
    {
        return -2;
    }

    if (other->m_parent == nullptr)
    {
        return 2;
    }

    if (GetIndent() < other->GetIndent())
    {
        return Compare(other->m_parent, subitem);
    }

    if (GetIndent() > other->GetIndent())
    {
        return m_parent->Compare(other, subitem);
    }
    
     return m_parent->Compare(other->m_parent, subitem);
}

int CTreeListItem::FindSortedChild(const CTreeListItem* child)
{
    for (int i = 0; i < GetChildrenCount(); i++)
    {
        if (child == GetSortedChild(i))
        {
            return i;
        }
    }
    ASSERT(0);
    return 0;
}

CTreeListItem* CTreeListItem::GetParent() const
{
    return m_parent;
}

void CTreeListItem::SetParent(CTreeListItem* parent)
{
    m_parent = parent;
}

bool CTreeListItem::IsAncestorOf(const CTreeListItem* item) const
{
    for (auto parent = item->GetParent(); parent != nullptr; parent = parent->GetParent())
    {
        if (parent == this)
        {
            return true;
        }
    }
    return false;
}

bool CTreeListItem::HasSiblings() const
{
    if (m_parent == nullptr)
    {
        return false;
    }
    const int i = m_parent->FindSortedChild(this);
    return i < m_parent->GetChildrenCount() - 1;
}

bool CTreeListItem::HasChildren() const
{
    return GetChildrenCount() > 0;
}

bool CTreeListItem::IsExpanded() const
{
    ASSERT(IsVisible());
    return m_vi->isExpanded;
}

void CTreeListItem::SetExpanded(bool expanded)
{
    ASSERT(IsVisible());
    m_vi->isExpanded = expanded;
}

bool CTreeListItem::IsVisible() const
{
    return m_vi != nullptr;
}

void CTreeListItem::SetVisible(bool visible)
{
    if (visible)
    {
        ASSERT(!IsVisible());
        m_vi = new VISIBLEINFO(GetParent() == nullptr ? 0 : GetParent()->GetIndent() + 1);
    }
    else
    {
        ASSERT(IsVisible());
        delete m_vi;
        m_vi = nullptr;
    }
}

int CTreeListItem::GetIndent() const
{
    ASSERT(IsVisible());
    return m_vi->indent;
}

CRect CTreeListItem::GetPlusMinusRect() const
{
    ASSERT(IsVisible());
    return m_vi->rcPlusMinus;
}

void CTreeListItem::SetPlusMinusRect(const CRect& rc) const
{
    ASSERT(IsVisible());
    m_vi->rcPlusMinus = rc;
}

CRect CTreeListItem::GetTitleRect() const
{
    ASSERT(IsVisible());
    return m_vi->rcTitle;
}

void CTreeListItem::SetTitleRect(const CRect& rc) const
{
    ASSERT(IsVisible());
    m_vi->rcTitle = rc;
}

CTreeListControl* CTreeListItem::GetTreeListControl()
{
    // As we only have 1 TreeListControl and want to economize memory
    // we simple made the TreeListControl global.
    return CTreeListControl::GetTheTreeListControl();
}


/////////////////////////////////////////////////////////////////////////////
// CTreeListControl

CTreeListControl* CTreeListControl::_theTreeListControl;

CTreeListControl* CTreeListControl::GetTheTreeListControl()
{
    ASSERT(_theTreeListControl != NULL);
    return _theTreeListControl;
}

IMPLEMENT_DYNAMIC(CTreeListControl, COwnerDrawnListControl)

CTreeListControl::CTreeListControl(CDirstatView* dirstatView, int rowHeight)
    : COwnerDrawnListControl(L"treelist", rowHeight)
      , m_dirstatView(dirstatView)
{
    _theTreeListControl = this;
    ASSERT(rowHeight <= NODE_HEIGHT); // can't be higher
    ASSERT(rowHeight % 2 == 0);       // must be an even number
}

void CTreeListControl::SortItems()
{
    COwnerDrawnListControl::SortItems();
}

bool CTreeListControl::HasImages()
{
    return true;
}

void CTreeListControl::MySetImageList(CImageList* il)
{
    m_imageList = il;
}

BOOL CTreeListControl::CreateEx(DWORD dwExStyle, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
    InitializeNodeBitmaps();

    dwStyle |= LVS_OWNERDRAWFIXED;

    BOOL bRet = COwnerDrawnListControl::Create(dwStyle, rect, pParentWnd, nID);
    VERIFY(bRet);
    if (bRet && dwExStyle)
    {
        bRet = COwnerDrawnListControl::ModifyStyleEx(0, dwExStyle);
    }
    return bRet;
}

void CTreeListControl::SysColorChanged()
{
    COwnerDrawnListControl::SysColorChanged();
    InitializeNodeBitmaps();
}

CTreeListItem* CTreeListControl::GetItem(int i)
{
    return reinterpret_cast<CTreeListItem*>(GetItemData(i));
}

bool CTreeListControl::IsItemSelected(const CTreeListItem* item)
{
    for (POSITION pos = GetFirstSelectedItemPosition(); pos != NULL;)
    {
        if (GetItem(GetNextSelectedItem(pos)) == item) return true;
    }
    return false;
}

void CTreeListControl::SelectItem(const CTreeListItem* item, bool deselect, bool focus)
{
    const int itempos = FindTreeItem(item);
    if (deselect) DeselectAll();
    SetItemState(itempos, LVIS_SELECTED, LVIS_SELECTED);
    if (focus) SetItemState(itempos, LVIS_FOCUSED, LVIS_FOCUSED);
    if (focus) SetSelectionMark(itempos);
}

void CTreeListControl::SetRootItem(CTreeListItem* root)
{
    DeleteAllItems();

    if (root != nullptr)
    {
        InsertItem(0, root);
        ExpandItem(0);
    }
}

void CTreeListControl::DeselectAll()
{
    for (POSITION pos = GetFirstSelectedItemPosition(); pos != NULL;)
    {
        int i = GetNextSelectedItem(pos);
        SetItemState(i, 0, LVIS_SELECTED);
    }
}

void CTreeListControl::ExpandPathToItem(const CTreeListItem* item)
{
    int i = 0;
    if (item == nullptr)
    {
        return;
    }

    CArray<const CTreeListItem*, const CTreeListItem*> path;
    const CTreeListItem* p = item;
    while (p != nullptr)
    {
        path.Add(p);
        p = p->GetParent();
    }

    int parent = 0;
    for (i = static_cast<int>(path.GetSize()) - 1; i >= 0; i--)
    {
        int index = FindTreeItem(path[i]);
        if (index == -1)
        {
            ASSERT(i < path.GetSize() - 1);
            ExpandItem(parent, false);
            index = FindTreeItem(path[i]);
        }
        else
        {
            for (int k = parent + 1; k < index; k++)
            {
                CollapseItem(k);
                index = FindTreeItem(path[i]);
            }
        }
        parent = index;
    }

    i = FindTreeItem(path[0]);

    const int w = GetSubItemWidth(GetItem(i), 0) + 5;
    if (GetColumnWidth(0) < w)
    {
        SetColumnWidth(0, w);
    }
}

void CTreeListControl::OnItemDoubleClick(int i)
{
    ToggleExpansion(i);
}

void CTreeListControl::InitializeNodeBitmaps()
{
    m_bmNodes0.DeleteObject();
    m_bmNodes1.DeleteObject();

    COLORMAP cm[1] = {{RGB(255, 0, 255), 0}};

    cm[0].to = GetWindowColor();
    VERIFY(m_bmNodes0.LoadMappedBitmap(IDB_NODES, 0, cm, 1));
    cm[0].to = GetStripeColor();
    VERIFY(m_bmNodes1.LoadMappedBitmap(IDB_NODES, 0, cm, 1));
}

void CTreeListControl::InsertItem(int i, CTreeListItem* item)
{
    COwnerDrawnListControl::InsertListItem(i, item);
    item->SetVisible(true);
}

void CTreeListControl::DeleteItem(int i)
{
    GetItem(i)->SetExpanded(false);
    GetItem(i)->SetVisible(false);
    COwnerDrawnListControl::DeleteItem(i);
}

int CTreeListControl::FindTreeItem(const CTreeListItem* item)
{
    return COwnerDrawnListControl::FindListItem(item);
}

BEGIN_MESSAGE_MAP(CTreeListControl, COwnerDrawnListControl)
    ON_WM_MEASUREITEM_REFLECT()
    ON_NOTIFY_REFLECT(LVN_ITEMCHANGING, OnLvnItemchangingList)
    ON_WM_LBUTTONDOWN()
    ON_WM_KEYDOWN()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_DESTROY()
END_MESSAGE_MAP()


void CTreeListControl::DrawNode(CDC* pdc, CRect& rc, CRect& rcPlusMinus, const CTreeListItem* item, int* width)
{
    CRect rcRest = rc;
    rcRest.left += GetGeneralLeftIndent();
    if (item->GetIndent() > 0)
    {
        rcRest.left += 3;

        CDC dcmem;
        dcmem.CreateCompatibleDC(pdc);
        CSelectObject sonodes(&dcmem, IsItemStripeColor(item) ? &m_bmNodes1 : &m_bmNodes0);

        const int ysrc = NODE_HEIGHT / 2 - GetRowHeight() / 2;

        if (width == nullptr)
        {
            const CTreeListItem* ancestor = item;
            for (int indent = item->GetIndent() - 2; indent >= 0; indent--)
            {
                ancestor = ancestor->GetParent();
                if (ancestor->HasSiblings())
                {
                    pdc->BitBlt(rcRest.left + indent * INDENT_WIDTH, rcRest.top, NODE_WIDTH, NODE_HEIGHT, &dcmem, NODE_WIDTH * NODE_LINE, ysrc, SRCCOPY);
                }
            }
        }

        rcRest.left += (item->GetIndent() - 1) * INDENT_WIDTH;

        if (width == nullptr)
        {
            int node;
            if (item->HasChildren())
            {
                if (item->HasSiblings())
                {
                    if (item->IsExpanded())
                    {
                        node = NODE_MINUS_SIBLING;
                    }
                    else
                    {
                        node = NODE_PLUS_SIBLING;
                    }
                }
                else
                {
                    if (item->IsExpanded())
                    {
                        node = NODE_MINUS_END;
                    }
                    else
                    {
                        node = NODE_PLUS_END;
                    }
                }
            }
            else
            {
                if (item->HasSiblings())
                {
                    node = NODE_SIBLING;
                }
                else
                {
                    node = NODE_END;
                }
            }

            pdc->BitBlt(rcRest.left, rcRest.top, NODE_WIDTH, NODE_HEIGHT, &dcmem, NODE_WIDTH * node, ysrc, SRCCOPY);

            rcPlusMinus.left   = rcRest.left + HOTNODE_X;
            rcPlusMinus.right  = rcPlusMinus.left + HOTNODE_CX;
            rcPlusMinus.top    = rcRest.top + rcRest.Height() / 2 - HOTNODE_CY / 2 - 1;
            rcPlusMinus.bottom = rcPlusMinus.top + HOTNODE_CY;
        }
        rcRest.left += NODE_WIDTH;
    }

    rc.right = rcRest.left;

    if (width != nullptr)
    {
        *width = rc.Width();
    }
}

void CTreeListControl::OnLButtonDown(UINT nFlags, CPoint point)
{
    m_lButtonDownItem = -1;

    LVHITTESTINFO hti;
    ZeroMemory(&hti, sizeof(hti));
    hti.pt = point;

    const int i = HitTest(&hti);
    if (i == -1)
    {
        return;
    }

    const CRect rc = GetWholeSubitemRect(i, 0);
    const CPoint pt = point - rc.TopLeft();

    const CTreeListItem* item = GetItem(i);

    m_lButtonDownItem = i;

    if (item->GetPlusMinusRect().PtInRect(pt))
    {
        m_lButtonDownOnPlusMinusRect = true;
        ToggleExpansion(i);
    }
    else
    {
        m_lButtonDownOnPlusMinusRect = false;
        COwnerDrawnListControl::OnLButtonDown(nFlags, point);
    }
}

void CTreeListControl::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    COwnerDrawnListControl::OnLButtonDblClk(nFlags, point);

    if (m_lButtonDownItem == -1)
    {
        return;
    }

    if (m_lButtonDownOnPlusMinusRect)
    {
        ToggleExpansion(m_lButtonDownItem);
    }
    else
    {
        OnItemDoubleClick(m_lButtonDownItem);
    }
}

void CTreeListControl::EmulateInteractiveSelection(const CTreeListItem* item)
{
    // see if any special keys are set so we can emulate them
    const auto shift_flag = (HSHELL_HIGHBIT & ::GetKeyState(VK_SHIFT)) ? MK_SHIFT : 0;
    const auto control_flag = (HSHELL_HIGHBIT & ::GetKeyState(VK_CONTROL)) ? MK_CONTROL : 0;
    const auto vk_flag = shift_flag | control_flag;

    // make sure the item is selectable
    ExpandPathToItem((CTreeListItem*)item);
    EnsureItemVisible(item);

    // get the item relative offset
    RECT rect = {};
    GetItemRect(FindTreeItem(item), &rect, LVIR_BOUNDS);
    LPARAM lparam = MAKELPARAM(rect.left, rect.top);

    // send the selection message
    if (vk_flag == 0) DeselectAll();
    SendMessage(WM_LBUTTONDOWN, MK_LBUTTON | vk_flag, lparam);
    SendMessage(WM_LBUTTONUP, MK_LBUTTON | vk_flag, lparam);
}

void CTreeListControl::ToggleExpansion(int i)
{
    if (GetItem(i)->IsExpanded())
    {
        CollapseItem(i);
    }
    else
    {
        ExpandItem(i);
    }
}

void CTreeListControl::CollapseItem(int i)
{
    CTreeListItem* item = GetItem(i);
    if (!item->IsExpanded())
    {
        return;
    }

    CWaitCursor wc;
    SetRedraw(FALSE);
    int todelete    = 0;
    for (int k = i + 1; k < GetItemCount(); k++)
    {
        const CTreeListItem* child = GetItem(k);
        if (child->GetIndent() <= item->GetIndent())
        {
            break;
        }
        todelete++;
    }
    for (int m = 0; m < todelete; m++)
    {
        DeleteItem(i + 1);
    }
    item->SetExpanded(false);
    
    SetRedraw(TRUE);
    RedrawItems(i, i);
}

int CTreeListControl::GetItemScrollPosition(CTreeListItem* item)
{
    CRect rc;
    VERIFY(GetItemRect(FindTreeItem(item), rc, LVIR_BOUNDS));
    return rc.top;
}

void CTreeListControl::SetItemScrollPosition(CTreeListItem* item, int top)
{
    const int old = GetItemScrollPosition(item);
    Scroll(CSize(0, top - old));
}


bool CTreeListControl::SelectedItemCanToggle()
{
    const auto & items = GetAllSelected();
    bool allow = !items.empty();
    for (const auto & item : items)
    {
        allow &= item->HasChildren();
    }
    return allow;
}

void CTreeListControl::ToggleSelectedItem()
{
    const auto & items = GetAllSelected();
    for (const auto& item : items)
    {
        ToggleExpansion(FindTreeItem(item));
    }
}

void CTreeListControl::ExpandItem(CTreeListItem* item)
{
    ExpandItem(FindTreeItem(item), false);
}

void CTreeListControl::ExpandItem(int i, bool scroll)
{
    CTreeListItem* item = GetItem(i);
    if (item->IsExpanded())
    {
        return;
    }

    CWaitCursor wc; // TODO: smart WaitCursor. In CollapseItem(), too.
    SetRedraw(FALSE);

    item->SortChildren();

    int maxwidth = GetSubItemWidth(item, 0);
    for (int c = 0; c < item->GetChildrenCount(); c++)
    {
        CTreeListItem* child = item->GetSortedChild(c);
        InsertItem(i + 1 + c, child);
        if (scroll)
        {
            const int w = GetSubItemWidth(child, 0);
            if (w > maxwidth)
            {
                maxwidth = w;
            }
        }
    }

    if (scroll && GetColumnWidth(0) < maxwidth)
    {
        SetColumnWidth(0, maxwidth);
    }

    item->SetExpanded(true);
    SetRedraw(TRUE);
    RedrawItems(i, i);

    if (scroll)
    {
        // Scroll up so far, that i is still visible
        // and the first child becomes visible, if possible.
        if (item->GetChildrenCount() > 0)
        {
            EnsureVisible(i + 1, false);
        }
        EnsureVisible(i, false);
    }
}

void CTreeListControl::OnLvnItemchangingList(NMHDR* pNMHDR, LRESULT* pResult)
{
    auto pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

    // determine if a new selection is being made
    const bool requesting_selection =
        (pNMLV->uOldState & LVIS_SELECTED) == 0 &&
        (pNMLV->uNewState & LVIS_SELECTED) != 0;

    // if in shift-extend mode, prevent selecting of non-adjacement nodes
    const auto shift_pressed = (HSHELL_HIGHBIT & ::GetKeyState(VK_SHIFT)) != 0;
    if (shift_pressed && requesting_selection)
    {
        const auto & potential_selection = GetItem(static_cast<int>(pNMLV->iItem));
        const auto & current_selection = GetItem(GetSelectionMark());
        *pResult = potential_selection->GetParent() != current_selection->GetParent();
        return;
    }

    // if in ctrl-extend mode, do not allow selection of parent of child of existing collection
    const auto ctrl_pressed = (HSHELL_HIGHBIT & ::GetKeyState(VK_CONTROL)) != 0;
    if (ctrl_pressed && requesting_selection)
    {
        const auto & items = GetAllSelected();
        const auto & potential_selection = GetItem(static_cast<int>(pNMLV->iItem));
        for (const auto item : items)
        {
            if (potential_selection->IsAncestorOf(item) || item->IsAncestorOf(potential_selection))
            {
                *pResult = TRUE;
                return;
            }
        }
    }

    *pResult = FALSE;
}

void CTreeListControl::OnChildAdded(CTreeListItem* parent, CTreeListItem* child)
{
    if (!parent->IsVisible())
    {
        return;
    }

    const int p = FindTreeItem(parent);
    ASSERT(p != -1);

    if (parent->IsExpanded())
    {
        InsertItem(p + 1, child);
        RedrawItems(p, p);
        Sort();
    }
    else
    {
        RedrawItems(p, p);
    }
}

void CTreeListControl::OnChildRemoved(CTreeListItem* parent, CTreeListItem* child)
{
    if (!parent->IsVisible())
    {
        return;
    }

    const int p = FindTreeItem(parent);
    ASSERT(p != -1);

    if (parent->IsExpanded())
    {
        for (int i = 0; i < child->GetChildrenCount(); i++)
        {
            OnChildRemoved(child, child->GetTreeListChild(i));
        }

        const int c = FindTreeItem(child);
        ASSERT(c != -1);
        DeleteItem(c);
        parent->SortChildren();
    }

    RedrawItems(p, p);
}

void CTreeListControl::OnRemovingAllChildren(CTreeListItem* parent)
{
    if (!parent->IsVisible())
    {
        return;
    }

    const int p = FindTreeItem(parent);
    ASSERT(p != -1);

    CollapseItem(p);
}

void CTreeListControl::Sort()
{
    for (int i = 0; i < GetItemCount(); i++)
    {
        if (GetItem(i)->IsExpanded())
        {
            GetItem(i)->SortChildren();
        }
    }
    COwnerDrawnListControl::SortItems();
}

void CTreeListControl::EnsureItemVisible(const CTreeListItem* item)
{
    if (item == nullptr)
    {
        return;
    }
    const int i = FindTreeItem(item);
    if (i == -1)
    {
        return;
    }
    EnsureVisible(i, false);
}

void CTreeListControl::MeasureItem(LPMEASUREITEMSTRUCT mis)
{
    mis->itemHeight = GetRowHeight();
}
