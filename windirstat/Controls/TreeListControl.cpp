// TreeListControl.cpp - Implementation of CTreeListItem and CTreeListControl
//
// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
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
#include "SelectObject.h"
#include "TreeListControl.h"
#include "MainFrame.h"
#include "Localization.h"

#include <algorithm>
#include <ranges>

namespace
{
    // Sequence within IDB_NODES
    enum : std::uint8_t
    {
        NODE_PLUS_SIBLING,
        NODE_PLUS_END,
        NODE_MINUS_SIBLING,
        NODE_MINUS_END,
        NODE_SIBLING,
        NODE_END,
        NODE_LINE
    };

    constexpr auto NODE_WIDTH = 15; // Width of a node within IDB_NODES
    constexpr auto NODE_HEIGHT = 24; // Height of IDB_NODES
    constexpr auto INDENT_WIDTH = 18;

    constexpr auto HOTNODE_CX = 9; // Size and position of the +/- buttons
    constexpr auto HOTNODE_CY = 9;
    constexpr auto HOTNODE_X = 0;
}

bool CTreeListItem::DrawSubItem(const int subitem, CDC* pdc, CRect rc, const UINT state, int* width, int* focusLeft)
{
    if (subitem != 0)
    {
        return false;
    }

    CRect rcNode = rc;
    CRect rcPlusMinus;
    m_VisualInfo->control->DrawNode(pdc, rcNode, rcPlusMinus, this, width);

    CRect rcLabel = rc;
    rcLabel.left = rcNode.right;
    DrawLabel(m_VisualInfo->control, pdc, rcLabel, state, width, focusLeft, false);

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

std::wstring CTreeListItem::GetText(int /*subitem*/) const
{
    return {};
}

void CTreeListItem::DrawPacman(const CDC* pdc, const CRect& rc, const COLORREF bgColor) const
{
    ASSERT(IsVisible());
    m_VisualInfo->pacman.SetBackgroundColor(bgColor);
    m_VisualInfo->pacman.Draw(pdc, rc);
}

void CTreeListItem::StartPacman() const
{
    if (IsVisible())
    {
        m_VisualInfo->pacman.Start();
    }
}

void CTreeListItem::StopPacman() const
{
    if (IsVisible())
    {
        m_VisualInfo->pacman.Stop();
    }
}

void CTreeListItem::DrivePacman() const
{
    if (IsVisible())
    {
        m_VisualInfo->pacman.UpdatePosition();
    }
}

int CTreeListItem::GetScrollPosition() const
{
    return m_VisualInfo->control->GetItemScrollPosition(this);
}

void CTreeListItem::SetScrollPosition(const int top) const
{
    m_VisualInfo->control->SetItemScrollPosition(this, top);
}

int CTreeListItem::Compare(const CSortingListItem* baseOther, const int subitem) const
{
    const auto other = reinterpret_cast<const CTreeListItem*>(baseOther);

    if (other == this)
    {
        return 0;
    }

    if (m_Parent == other->m_Parent)
    {
        return CompareSibling(other, subitem);
    }

    if (m_Parent == nullptr)
    {
        return -2;
    }

    if (other->m_Parent == nullptr)
    {
        return 2;
    }

    if (GetIndent() < other->GetIndent())
    {
        return Compare(other->m_Parent, subitem);
    }

    if (GetIndent() > other->GetIndent())
    {
        return m_Parent->Compare(other, subitem);
    }

    return m_Parent->Compare(other->m_Parent, subitem);
}

CTreeListItem* CTreeListItem::GetParent() const
{
    return m_Parent;
}

void CTreeListItem::SetParent(CTreeListItem* parent)
{
    m_Parent = parent;
}

bool CTreeListItem::IsAncestorOf(const CTreeListItem* item) const
{
    for (auto parent = item; parent != nullptr; parent = parent->GetParent())
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
    if (m_Parent == nullptr)
    {
        return false;
    }
    return m_Parent->GetTreeListChildCount() > 1;
}

bool CTreeListItem::HasChildren() const
{
    return GetTreeListChildCount() > 0;
}

bool CTreeListItem::IsExpanded() const
{
    ASSERT(IsVisible());
    return m_VisualInfo->isExpanded;
}

void CTreeListItem::SetExpanded(const bool expanded) const
{
    ASSERT(IsVisible());
    m_VisualInfo->isExpanded = expanded;
}

void CTreeListItem::SetVisible(CTreeListControl* control, const bool visible)
{
    if (visible)
    {
        ASSERT(!IsVisible());
        const unsigned char indent = GetParent() == nullptr ? 0 : GetParent()->GetIndent() + 1;
        m_VisualInfo = std::make_unique<VISIBLEINFO>(indent);
        m_VisualInfo->control = control;
    }
    else
    {
        ASSERT(IsVisible());
        m_VisualInfo.reset();
    }
}

unsigned char CTreeListItem::GetIndent() const
{
    ASSERT(IsVisible());
    return m_VisualInfo->indent;
}

void CTreeListItem::SetIndent(const unsigned char indent) const
{
    ASSERT(IsVisible());
    m_VisualInfo->indent = indent;
}

CRect CTreeListItem::GetPlusMinusRect() const
{
    ASSERT(IsVisible());
    return m_VisualInfo->rcPlusMinus;
}

void CTreeListItem::SetPlusMinusRect(const CRect& rc) const
{
    ASSERT(IsVisible());
    m_VisualInfo->rcPlusMinus = rc;
}

CRect CTreeListItem::GetTitleRect() const
{
    ASSERT(IsVisible());
    return m_VisualInfo->rcTitle;
}

void CTreeListItem::SetTitleRect(const CRect& rc) const
{
    ASSERT(IsVisible());
    m_VisualInfo->rcTitle = rc;
}

/////////////////////////////////////////////////////////////////////////////
// CTreeListControl

IMPLEMENT_DYNAMIC(CTreeListControl, COwnerDrawnListControl)

CTreeListControl::CTreeListControl(int rowHeight, std::vector<int>* columnOrder, std::vector<int>* columnWidths)
    : COwnerDrawnListControl(rowHeight, columnOrder, columnWidths)
{
    ASSERT(rowHeight <= NODE_HEIGHT); // can't be higher
    ASSERT(rowHeight % 2 == 0);       // must be an even number
}

BOOL CTreeListControl::CreateExtended(const DWORD dwExStyle, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, const UINT nID)
{
    InitializeNodeBitmaps();

    dwStyle |= LVS_OWNERDRAWFIXED;

    const BOOL bRet = Create(dwStyle, rect, pParentWnd, nID);
    VERIFY(bRet);
    if (bRet && dwExStyle)
    {
        AddExtendedStyle(dwExStyle);
    }
    return bRet;
}

void CTreeListControl::SysColorChanged()
{
    COwnerDrawnListControl::SysColorChanged();
    InitializeNodeBitmaps();
}

CTreeListItem* CTreeListControl::GetItem(const int i) const
{
    return reinterpret_cast<CTreeListItem*>(GetItemData(i));
}

bool CTreeListControl::IsItemSelected(const CTreeListItem* item) const
{
    for (POSITION pos = GetFirstSelectedItemPosition(); pos != nullptr;)
    {
        if (GetItem(GetNextSelectedItem(pos)) == item) return true;
    }
    return false;
}

void CTreeListControl::SelectItem(const CTreeListItem* item, const bool deselect, const bool focus)
{
    const int itempos = FindTreeItem(item);
    if (deselect) DeselectAll();
    SetItemState(itempos, LVIS_SELECTED, LVIS_SELECTED);
    if (focus) SetItemState(itempos, LVIS_FOCUSED, LVIS_FOCUSED);
    if (focus) SetSelectionMark(itempos);
    EnsureItemVisible(item);
}

void CTreeListControl::SetRootItem(CTreeListItem* root)
{
    DeleteAllItems();
    Invalidate();

    if (root != nullptr)
    {
        InsertItem(0, root);
        ExpandItem(0);
    }
}

void CTreeListControl::DeselectAll()
{
    for (POSITION pos = GetFirstSelectedItemPosition(); pos != nullptr;)
    {
        const int i = GetNextSelectedItem(pos);
        SetItemState(i, 0, LVIS_SELECTED);
    }
}

void CTreeListControl::ExpandPathToItem(const CTreeListItem* item)
{
    if (item == nullptr)
    {
        return;
    }

    // Create vector containing all path components
    std::vector<const CTreeListItem*> paths;
    for (const CTreeListItem* p = item; p != nullptr; p = p->GetParent())
    {
        paths.push_back(p);
    }

    int parent = 0;
    for (const auto& path : paths | std::views::reverse)
    {
        int index = FindTreeItem(path);
        if (index == -1)
        {
            ExpandItem(parent, false);
            index = FindTreeItem(path);
        }
        else
        {
            for (int k = parent + 1; k < index; k++)
            {
                // Do not collapse if in multiple selection mode (holding control)
                if ((HSHELL_HIGHBIT & GetKeyState(VK_CONTROL)) == 0) CollapseItem(k);
                index = FindTreeItem(path);
            }
        }
        parent = index;
    }

    const int w = GetSubItemWidth(GetItem(FindTreeItem(paths[0])), 0) + 5;
    if (GetColumnWidth(0) < w)
    {
        SetColumnWidth(0, w);
    }
}

void CTreeListControl::OnItemDoubleClick(const int i)
{
    ToggleExpansion(i);
}

void CTreeListControl::InitializeNodeBitmaps()
{
    m_BmNodes0.DeleteObject();
    m_BmNodes1.DeleteObject();

    COLORMAP cm[1] = { {RGB(255, 0, 255), 0} };

    cm[0].to = GetWindowColor();
    VERIFY(m_BmNodes0.LoadMappedBitmap(IDB_NODES, 0, cm, 1));
    cm[0].to = GetStripeColor();
    VERIFY(m_BmNodes1.LoadMappedBitmap(IDB_NODES, 0, cm, 1));
}

void CTreeListControl::InsertItem(const int i, CTreeListItem* item)
{
    InsertListItem(i, item);
    item->SetVisible(this, true);
}

void CTreeListControl::DeleteItem(const int i)
{
    GetItem(i)->SetExpanded(false);
    GetItem(i)->SetVisible(this, false);
    COwnerDrawnListControl::DeleteItem(i);
}

int CTreeListControl::FindTreeItem(const CTreeListItem* item) const
{
    return FindListItem(item);
}

#pragma warning(push)
#pragma warning(disable:26454)
BEGIN_MESSAGE_MAP(CTreeListControl, COwnerDrawnListControl)
    ON_WM_MEASUREITEM_REFLECT()
    ON_NOTIFY_REFLECT(LVN_ITEMCHANGING, OnLvnItemChangingList)
    ON_WM_CONTEXTMENU()
    ON_WM_LBUTTONDOWN()
    ON_WM_KEYDOWN()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_DESTROY()
END_MESSAGE_MAP()
#pragma warning(pop)

void CTreeListControl::DrawNode(CDC* pdc, CRect& rc, CRect& rcPlusMinus, const CTreeListItem* item, int* width)
{
    CRect rcRest = rc;
    rcRest.left += GetGeneralLeftIndent();
    if (item->GetIndent() > 0)
    {
        rcRest.left += 3;

        CDC dcmem;
        dcmem.CreateCompatibleDC(pdc);
        CSelectObject sonodes(&dcmem, IsItem_stripeColor(item) ? &m_BmNodes1 : &m_BmNodes0);

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

            rcPlusMinus.left = rcRest.left + HOTNODE_X;
            rcPlusMinus.right = rcPlusMinus.left + HOTNODE_CX;
            rcPlusMinus.top = rcRest.top + rcRest.Height() / 2 - HOTNODE_CY / 2 - 1;
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

void CTreeListControl::OnLButtonDown(const UINT nFlags, const CPoint point)
{
    m_LButtonDownItem = -1;

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

    m_LButtonDownItem = i;

    if (item->GetPlusMinusRect().PtInRect(pt))
    {
        m_LButtonDownOnPlusMinusRect = true;
        ToggleExpansion(i);
    }
    else
    {
        m_LButtonDownOnPlusMinusRect = false;
        COwnerDrawnListControl::OnLButtonDown(nFlags, point);
    }
}

void CTreeListControl::OnLButtonDblClk(const UINT nFlags, const CPoint point)
{
    COwnerDrawnListControl::OnLButtonDblClk(nFlags, point);

    if (m_LButtonDownItem == -1)
    {
        return;
    }

    if (m_LButtonDownOnPlusMinusRect)
    {
        ToggleExpansion(m_LButtonDownItem);
    }
    else
    {
        OnItemDoubleClick(m_LButtonDownItem);
    }
}

void CTreeListControl::EmulateInteractiveSelection(const CTreeListItem* item)
{
    // see if any special keys are set so we can emulate them
    const auto shiftFlag = (HSHELL_HIGHBIT & GetKeyState(VK_SHIFT)) ? MK_SHIFT : 0;
    const auto controlFlag = (HSHELL_HIGHBIT & GetKeyState(VK_CONTROL)) ? MK_CONTROL : 0;
    const auto vkFlag = shiftFlag | controlFlag;

    // make sure the item is selectable
    ExpandPathToItem(item);
    EnsureItemVisible(item);

    // get the item relative offset
    RECT rect = {};
    GetItemRect(FindTreeItem(item), &rect, LVIR_BOUNDS);
    const LPARAM lparam = MAKELPARAM(rect.left, rect.top);

    // send the selection message
    if (vkFlag == 0) DeselectAll();
    (void)SendMessage(WM_LBUTTONDOWN, MK_LBUTTON | vkFlag, lparam);
    (void)SendMessage(WM_LBUTTONUP, MK_LBUTTON | vkFlag, lparam);
}

void CTreeListControl::ToggleExpansion(const int i)
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

void CTreeListControl::CollapseItem(const int i)
{
    CTreeListItem* item = GetItem(i);
    if (!item->IsExpanded())
    {
        return;
    }

    CWaitCursor wc;
    SetRedraw(FALSE);
    LockWindowUpdate();

    int todelete = 0;
    for (int k = i + 1, kMax = GetItemCount(); k < kMax; k++)
    {
        const CTreeListItem* child = GetItem(k);
        if (child->GetIndent() <= item->GetIndent())
        {
            break;
        }
        todelete++;
    }

    for (int m = i + todelete; m > i; m--)
    {
        DeleteItem(m);
    }
    item->SetExpanded(false);

    UnlockWindowUpdate();
    SetRedraw(TRUE);
    RedrawItems(i, i);
}

int CTreeListControl::GetItemScrollPosition(const CTreeListItem* item) const
{
    CRect rc;
    VERIFY(GetItemRect(FindTreeItem(item), rc, LVIR_BOUNDS));
    return rc.top;
}

void CTreeListControl::SetItemScrollPosition(const CTreeListItem* item, const int top)
{
    const int old = GetItemScrollPosition(item);
    Scroll(CSize(0, top - old));
}

bool CTreeListControl::SelectedItemCanToggle()
{
    const auto& items = GetAllSelected(true);
    bool allow = !items.empty();
    for (const auto& item : items)
    {
        allow &= item->HasChildren();
    }
    return allow;
}

void CTreeListControl::ToggleSelectedItem()
{
    const auto& items = GetAllSelected(true);
    for (const auto& item : items)
    {
        ToggleExpansion(FindTreeItem(item));
    }
}

void CTreeListControl::ExpandItem(const CTreeListItem* item)
{
    ExpandItem(FindTreeItem(item), false);
}

void CTreeListControl::ExpandItem(const int i, const bool scroll)
{
    CTreeListItem* item = GetItem(i);
    if (item->IsExpanded())
    {
        return;
    }

    CWaitCursor wc;

    SetRedraw(FALSE);
    LockWindowUpdate();
    int maxwidth = GetSubItemWidth(item, 0);
    const auto childItems = item->GetTreeListChildCount();
    for (int c = 0; c < childItems; c++)
    {
        CTreeListItem* child = item->GetTreeListChild(c);
        InsertItem(i + 1 + c, child);

        // The calculation of item width is very expensive for
        // very large lists so limit calculation based on the
        // first few bunch of visible items
        if (COptions::AutomaticallyResizeColumns && scroll && c < 50)
        {
            maxwidth = max(maxwidth, GetSubItemWidth(child, 0));
        }
    }
    if (childItems > 0) SortItems();
    UnlockWindowUpdate();
    SetRedraw(TRUE);

    if (scroll && GetColumnWidth(0) < maxwidth)
    {
        SetColumnWidth(0, maxwidth);
    }

    item->SetExpanded(true);
    RedrawItems(i, i);

    if (scroll)
    {
        // Scroll up so far, that i is still visible
        // and the first child becomes visible, if possible.
        if (item->GetTreeListChildCount() > 0)
        {
            EnsureVisible(i + 1, false);
        }
        EnsureVisible(i, false);
    }
}

void CTreeListControl::OnKeyDown(const UINT nChar, const UINT nRepCnt, const UINT nFlags)
{
    if (const auto& items = GetAllSelected(true); items.size() == 1)
    {
        if (nChar == VK_RIGHT)
        {
            if (items[0]->IsExpanded() && items[0]->HasChildren())
            {
                SelectItem(GetItem(FindTreeItem(items[0]) + 1), true, true);
            }
            if (!items[0]->IsExpanded() && items[0]->HasChildren())
            {
                ExpandItem(FindTreeItem(items[0]));
            }
            return;
        }
        if (nChar == VK_LEFT)
        {
            if (items[0]->IsExpanded())
            {
                CollapseItem(FindTreeItem(items[0]));
            }
            else if (items[0]->GetParent() != nullptr)
            {
                SelectItem(items[0]->GetParent(), true, true);
            }
            return;
        }
        if (nChar == VK_SPACE)
        {
            if (items[0]->HasChildren())
            {
                ToggleExpansion(FindTreeItem(items[0]));
                return;
            }
        }
    }

    COwnerDrawnListControl::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CTreeListControl::OnLvnItemChangingList(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

    // determine if a new selection is being made
    const bool requestingSelection =
        (pNMLV->uOldState & LVIS_SELECTED) == 0 &&
        (pNMLV->uNewState & LVIS_SELECTED) != 0;

    // if in shift-extend mode, prevent selecting of non-adjacent nodes
    const auto shiftPressed = (HSHELL_HIGHBIT & GetKeyState(VK_SHIFT)) != 0;
    if (shiftPressed && requestingSelection)
    {
        const auto& potentialSelection = GetItem(pNMLV->iItem);
        const auto& currentSelection = GetItem(GetSelectionMark());
        *pResult = potentialSelection->GetParent() != currentSelection->GetParent();
        return;
    }

    // if in ctrl-extend mode, do not allow selection of parent of child of existing collection
    const auto ctrlPressed = (HSHELL_HIGHBIT & GetKeyState(VK_CONTROL)) != 0;
    if (ctrlPressed && requestingSelection)
    {
        const auto& items = GetAllSelected(true);
        const auto& potentialSelection = GetItem(pNMLV->iItem);
        for (const auto item : items)
        {
            if (potentialSelection->IsAncestorOf(item) || item->IsAncestorOf(potentialSelection))
            {
                *pResult = TRUE;
                return;
            }
        }
    }

    *pResult = FALSE;
}

void CTreeListControl::OnChildAdded(const CTreeListItem* parent, CTreeListItem* child)
{
    if (!parent->IsVisible() || !parent->IsExpanded())
    {
        return;
    }

    const int p = FindTreeItem(parent);
    ASSERT(p != -1);
    InsertItem(p + parent->GetTreeListChildCount(), child);
}

void CTreeListControl::OnChildRemoved(const CTreeListItem* parent, CTreeListItem* child)
{
    if (!parent->IsVisible())
    {
        return;
    }

    const int p = FindTreeItem(parent);
    ASSERT(p != -1);

    if (parent->IsExpanded())
    {
        for (int i = 0; i < child->GetTreeListChildCount(); i++)
        {
            OnChildRemoved(child, child->GetTreeListChild(i));
        }

        const int c = FindTreeItem(child);
        ASSERT(c != -1);
        DeleteItem(c);
    }

    RedrawItems(p, p);
}

void CTreeListControl::OnRemovingAllChildren(const CTreeListItem* parent)
{
    if (!parent->IsVisible())
    {
        return;
    }

    const int p = FindTreeItem(parent);
    ASSERT(p != -1);

    CollapseItem(p);
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

void CTreeListControl::OnContextMenu(CWnd* /*pWnd*/, const CPoint pt)
{
    const int i = GetSelectionMark();
    if (i == -1)
    {
        return;
    }

    CTreeListItem* item = GetItem(i);
    CRect rc = GetWholeSubitemRect(i, 0);
    const CRect rcTitle = item->GetTitleRect() + rc.TopLeft();

    CMenu menu;
    menu.LoadMenu(IDR_POPUP_TREE);
    Localization::UpdateMenu(menu);
    CMenu* sub = menu.GetSubMenu(0);

    // Populate default menu items
    if (item != nullptr && item->GetTreeListChildCount() == 0)
    {
        sub->DeleteMenu(0, MF_BYPOSITION); // Remove "Expand/Collapse" item
        sub->DeleteMenu(0, MF_BYPOSITION); // Remove separator
        sub->SetDefaultItem(ID_CLEANUP_OPEN_SELECTED, false);
    }
    else
    {
        const std::wstring command = item->IsExpanded() && item->HasChildren() ? Localization::Lookup(IDS_COLLAPSE) : Localization::Lookup(IDS_EXPAND);
        VERIFY(sub->ModifyMenu(ID_POPUP_TOGGLE, MF_BYCOMMAND | MF_STRING, ID_POPUP_TOGGLE, command.c_str()));
        sub->SetDefaultItem(ID_POPUP_TOGGLE, false);
    }

    // Update dynamic menu items
    CMainFrame::Get()->UpdateDynamicMenuItems(sub);

    // Show popup menu and act accordingly.
    //
    // The menu shall not overlap the label but appear
    // horizontally at the cursor position,
    // vertically under (or above) the label.
    // TrackPopupMenuEx() behaves in the desired way, if
    // we exclude the label rectangle extended to full screen width.

    TPMPARAMS tp;
    tp.cbSize = sizeof(tp);
    tp.rcExclude = rcTitle;
    ClientToScreen(&tp.rcExclude);

    CRect desktop;
    GetDesktopWindow()->GetWindowRect(desktop);

    tp.rcExclude.left = desktop.left;
    tp.rcExclude.right = desktop.right;

    constexpr int overlap = 2; // a little vertical overlapping
    tp.rcExclude.top += overlap;
    tp.rcExclude.bottom -= overlap;

    sub->TrackPopupMenuEx(TPM_LEFTALIGN | TPM_LEFTBUTTON, pt.x, pt.y, AfxGetMainWnd(), &tp);
}
