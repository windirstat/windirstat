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
#include "TreeListControl.h"

bool CTreeListItem::DrawSubItem(const int subitem, CDC* pdc, CRect rc, const UINT state, int* width, int* focusLeft)
{
    if (subitem != 0)
    {
        return false;
    }

    CRect rcNode = rc;
    CRect rcPlusMinus;
    m_visualInfo->control->DrawNode(pdc, rcNode, rcPlusMinus, this, width);

    CRect rcLabel = rc;
    rcLabel.left = rcNode.right;
    DrawLabel(m_visualInfo->control, pdc, rcLabel, state, width, focusLeft, false);

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

void CTreeListItem::DrawPacman(CDC* pdc, const CRect& rc) const
{
    ASSERT(IsVisible());
    m_visualInfo->pacman.Draw(pdc, rc);
}

void CTreeListItem::StartPacman() const
{
    if (IsVisible())
    {
        m_visualInfo->pacman.Start();
    }
}

void CTreeListItem::StopPacman() const
{
    if (IsVisible())
    {
        m_visualInfo->pacman.Stop();
    }
}

void CTreeListItem::DrivePacman() const
{
    if (IsVisible())
    {
        m_visualInfo->pacman.UpdatePosition();
    }
}

int CTreeListItem::GetScrollPosition() const
{
    return m_visualInfo->control->GetItemScrollPosition(this);
}

void CTreeListItem::SetScrollPosition(const int top) const
{
    m_visualInfo->control->SetItemScrollPosition(this, top);
}

int CTreeListItem::Compare(const COwnerDrawnListItem* baseOther, const int subitem) const
{
    const auto other = reinterpret_cast<const CTreeListItem*>(baseOther);

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
    for (auto parent = item; parent != nullptr; parent = parent->GetParent())
    {
        if (parent == this)
        {
            return true;
        }
    }
    return false;
}

bool CTreeListItem::HasChildren() const
{
    return GetTreeListChildCount() > 0;
}

bool CTreeListItem::IsExpanded() const
{
    ASSERT(IsVisible());
    return m_visualInfo->isExpanded;
}

void CTreeListItem::SetExpanded(const bool expanded) const
{
    ASSERT(IsVisible());
    m_visualInfo->isExpanded = expanded;
}

void CTreeListItem::SetVisible(CTreeListControl* control, const bool visible)
{
    if (visible)
    {
        ASSERT(!IsVisible());
        const unsigned char indent = GetParent() == nullptr ? 0 : GetParent()->GetIndent() + 1;
        m_visualInfo = std::make_unique<VISIBLEINFO>(indent);
        m_visualInfo->control = control;
    }
    else
    {
        ASSERT(IsVisible());
        m_visualInfo.reset();
    }
}

unsigned char CTreeListItem::GetIndent() const
{
    ASSERT(IsVisible());
    return m_visualInfo->indent;
}

void CTreeListItem::SetIndent(const unsigned char indent) const
{
    ASSERT(IsVisible());
    m_visualInfo->indent = indent;
}

CRect CTreeListItem::GetPlusMinusRect() const
{
    ASSERT(IsVisible());
    return m_visualInfo->rcPlusMinus;
}

void CTreeListItem::SetPlusMinusRect(const CRect& rc) const
{
    ASSERT(IsVisible());
    m_visualInfo->rcPlusMinus = rc;
}

CRect CTreeListItem::GetTitleRect() const
{
    ASSERT(IsVisible());
    return m_visualInfo->rcTitle;
}

void CTreeListItem::SetTitleRect(const CRect& rc) const
{
    ASSERT(IsVisible());
    m_visualInfo->rcTitle = rc;
}

/////////////////////////////////////////////////////////////////////////////
// CTreeListControl

IMPLEMENT_DYNAMIC(CTreeListControl, COwnerDrawnListControl)

CTreeListControl::CTreeListControl(std::vector<int>* columnOrder, std::vector<int>* columnWidths)
    : COwnerDrawnListControl(columnOrder, columnWidths)
{
}

BOOL CTreeListControl::CreateExtended(const DWORD dwExStyle, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, const UINT nID)
{
    dwStyle |= LVS_OWNERDRAWFIXED;

    const BOOL bRet = Create(dwStyle, rect, pParentWnd, nID);
    if (bRet && dwExStyle)
    {
        AddExtendedStyle(dwExStyle);
    }
    return bRet;
}

void CTreeListControl::SysColorChanged()
{
    COwnerDrawnListControl::SysColorChanged();
}

CTreeListItem* CTreeListControl::GetItem(const int i) const
{
    bool doMaxCheck = false;
    DEBUG_ONLY(doMaxCheck = true);
    if (doMaxCheck && i >= GetItemCount()) return nullptr;
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
    AfterDeleteAllItems();
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
                if (!IsControlKeyDown()) CollapseItem(k);
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

BEGIN_MESSAGE_MAP(CTreeListControl, COwnerDrawnListControl)
    ON_NOTIFY_REFLECT(LVN_ITEMCHANGING, OnLvnItemChangingList)
    ON_WM_CONTEXTMENU()
    ON_WM_LBUTTONDOWN()
    ON_WM_KEYDOWN()
    ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()

void CTreeListControl::DrawNode(CDC* pDC, CRect& rcRest, CRect& expanderRect, const CTreeListItem* item, int* width)
{
    const int rowHeight = GetRowHeight();
    const int indentStep = rowHeight * 7 / 8;
    const int indentationLevel = item->GetIndent();
    CRect remainingRect = rcRest;
    remainingRect.left += GetGeneralLeftIndent() + 3; // 3 magic number from original code

    // Handle root item (depth 0) - no connectors
    if (indentationLevel == 0)
    {
        rcRest.right = remainingRect.left;
        if (width != nullptr) *width = rcRest.Width();
        return;
    }

    // Width-only calculation (early exit)
    const int visualIndent = indentationLevel - 1;
    if (width != nullptr)
    {
        remainingRect.left += visualIndent * indentStep + rowHeight;
        rcRest.right = remainingRect.left;
        *width = rcRest.Width();
        return;
    }

    // Determine connector symbols
    const int rowIndex = FindTreeItem(item);
    const COLORREF bgColor = IsItemStripColor(rowIndex) ? GetStripeColor() : GetWindowColor();

    // Cache ancestors to draw connecting lines
    // Use static vector to avoid repeated allocations in the draw loop
    static std::vector<const CTreeListItem*> ancestors;
    ancestors.clear();
    ancestors.reserve(indentationLevel + 1);
    for (auto p = item; p != nullptr; p = p->GetParent())
    {
        ancestors.push_back(p);
    }

    // Lambda to check if a node is the last child of its parent
    // Optimized to use list position for the current item if possible
    auto isVisualLastChild = [&](const CTreeListItem* node, const bool isCurrentItem)
    {
        // For the current item (if not expanded), check the next item in the list
        // This avoids iterating through thousands of siblings for leaf nodes
        if (isCurrentItem && !node->IsExpanded())
        {
            const int nextIndex = rowIndex + 1;

            // If we are at the end of the list, we are definitely the last child
            if (nextIndex >= GetItemCount()) return true;
            return GetItem(nextIndex)->GetIndent() < node->GetIndent();
        }

        const auto* parent = node->GetParent();
        const int count = parent ? parent->GetTreeListChildCount() : 0;
        if (count <= 1) return true;

        const auto& sorting = GetSorting();
        const int sub1 = ColumnToSubItem(sorting.column1);
        const int sub2 = ColumnToSubItem(sorting.column2);

        // Check if any sibling comes after this node in sort order
        for (const int k : std::views::iota(0, count))
        {
            const auto* sibling = parent->GetTreeListChild(k);
            if (sibling == node) continue;

            int compareResult = sibling->Compare(node, sub1);
            const bool use2 = (compareResult == 0);
            if (use2) compareResult = sibling->Compare(node, sub2);

            if (const bool asc = use2 ? sorting.ascending2 : sorting.ascending1;
                (asc && compareResult > 0) || (!asc && compareResult < 0)) return false;
        }
        return true;
    };

    // Draw connectors for each indentation level
    for (const int i : std::views::iota(1, indentationLevel + 1))
    {
        // Ancestors are stored in reverse order (Item -> Root)
        // Ancestors[indentationLevel - i] gives the ancestor at level 'i'
        const CTreeListItem* ancestor = ancestors[indentationLevel - i];
        const bool isCurrentItem = (i == indentationLevel);

        const bool isLast = isVisualLastChild(ancestor, isCurrentItem);
        CRect rcColumn(remainingRect.left, remainingRect.top, remainingRect.left + rowHeight, remainingRect.bottom);

        if (isCurrentItem)
        {
            // Draw L-shaped connector for the item itself
            DrawTreeNodeConnector(pDC, rcColumn, bgColor, true, !isLast, true,
                item->HasChildren() && !item->IsExpanded(),
                item->HasChildren() && item->IsExpanded());
        }
        else
        {
            // Draw vertical line for ancestors if they are not the last child
            if (!isLast) DrawTreeNodeConnector(pDC, rcColumn, bgColor, true, true, false);
            remainingRect.left += indentStep;
        }
    }

    // Draw line under node icon if applicable
    if (item->HasChildren() && item->IsExpanded())
    {
        const CRect rcIcon{POINT{ remainingRect.left + indentStep, remainingRect.top },SIZE{ rowHeight, rowHeight }};
        DrawTreeNodeConnector(pDC, rcIcon, bgColor, false, true, false);
    }

    // Set up plus/minus hit rect for click detection
    const int boxHalf = ((rowHeight / 2) | 1) / 2;
    const int lineCenterX = remainingRect.left + rowHeight / 2;
    const int centerY = remainingRect.top + rowHeight / 2;
    expanderRect = CRect(lineCenterX - boxHalf, centerY - boxHalf,
        lineCenterX + boxHalf + 1, centerY + boxHalf + 1);

    // Update final position
    rcRest.right = remainingRect.left + rowHeight;
}

void CTreeListControl::OnLButtonDown(const UINT nFlags, const CPoint point)
{
    m_lButtonDownItem = -1;

    LVHITTESTINFO hti{ .pt = point };
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

void CTreeListControl::OnLButtonDblClk(const UINT nFlags, const CPoint point)
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
    const auto vkFlag =
        (IsShiftKeyDown() ? MK_SHIFT : 0) |
        (IsControlKeyDown() ? MK_CONTROL : 0);

    // make sure the item is selectable
    ExpandPathToItem(item);
    EnsureItemVisible(item);

    // get the item relative offset
    RECT rect = {}, clientRect = {};
    GetItemRect(FindTreeItem(item), &rect, LVIR_BOUNDS);
    GetClientRect(&clientRect);
    IntersectRect(&rect, &rect, &clientRect);
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
    const CTreeListItem* item = GetItem(i);
    if (!item->IsExpanded())
    {
        return;
    }

    CWaitCursor wc;
    SetRedraw(FALSE);
    LockWindowUpdate();

    int todelete = 0;
    for (const int k : std::views::iota(i + 1, GetItemCount()))
    {
        const CTreeListItem* child = GetItem(k);
        if (child->GetIndent() <= item->GetIndent())
        {
            break;
        }
        todelete++;
    }

    // Correct focus to point to parent if was in the tree
    const int hasFocus = GetNextItem(-1, LVNI_FOCUSED);
    if (std::clamp(hasFocus, i + 1, i + todelete) == hasFocus)
    {
        SetItemState(i, LVIS_FOCUSED, LVIS_FOCUSED);
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
    GetItemRect(FindTreeItem(item), rc, LVIR_BOUNDS);
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
    SetItemCount(GetItemCount() + childItems);
    for (const int c : std::views::iota(0, childItems))
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
    UnlockWindowUpdate();
    SetRedraw(TRUE);

    if (scroll && GetColumnWidth(0) < maxwidth)
    {
        constexpr int padding = 3;
        SetColumnWidth(0, maxwidth + padding);
    }

    item->SetExpanded(true);
    RedrawItems(i, i);

    if (scroll)
    {
        // Scroll up so far, that item is still visible
        // and the first child becomes visible, if possible.
        if (item->GetTreeListChildCount() > 0)
        {
            EnsureVisible(i + 1, false);
        }
        EnsureVisible(i, false);
    }

    // Sort at end so we do not invalidate position data
    if (childItems > 0) SortItems();
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
    if (IsShiftKeyDown() && requestingSelection)
    {
        const auto& potentialSelection = GetItem(pNMLV->iItem);
        const auto& currentSelection = GetItem(GetSelectionMark());
        *pResult = potentialSelection->GetParent() != currentSelection->GetParent();
        return;
    }

    // if in ctrl-extend mode, do not allow selection of parent of child of existing collection
    if (IsControlKeyDown() && requestingSelection)
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

    // Search backwards to find where this parent's children end (last direct child).
    const int nextPos = FindTreeItem(parent) + 1;
    int insertPos = nextPos + parent->GetTreeListChildCount();
    for (; insertPos > nextPos; --insertPos)
    {
        if (const auto* item = GetItem(insertPos - 1);
            item != nullptr && item->GetParent() == parent) break;
    }

    InsertItem(insertPos, child);
}

void CTreeListControl::OnChildRemoved(const CTreeListItem* parent, const CTreeListItem* child)
{
    if (!parent->IsVisible())
    {
        return;
    }

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

    const int p = FindTreeItem(parent);
    ASSERT(p != -1);
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
    
    // Scroll to the left to show the beginning of the item
    if (const int currentScrollPos = GetScrollPos(SB_HORZ); currentScrollPos > 0)
    {
        Scroll(CSize(-currentScrollPos, 0));
    }
}

void CTreeListControl::OnContextMenu(CWnd* /*pWnd*/, const CPoint pt)
{
    const int i = GetSelectionMark();
    if (i == -1) return;

    const CTreeListItem* item = GetItem(i);
    if (item == nullptr) return;
    CRect rc = GetWholeSubitemRect(i, 0);
    const CRect rcTitle = item->GetTitleRect() + rc.TopLeft();

    CMenu menu;
    menu.LoadMenu(IDR_POPUP_TREE);
    Localization::UpdateMenu(menu);
    CMenu* sub = menu.GetSubMenu(0);
    if (sub == nullptr) return;

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
        sub->ModifyMenu(ID_POPUP_TOGGLE, MF_BYCOMMAND | MF_STRING, ID_POPUP_TOGGLE, command.c_str());
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

    TPMPARAMS tp{ .cbSize = sizeof(tp) };
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
