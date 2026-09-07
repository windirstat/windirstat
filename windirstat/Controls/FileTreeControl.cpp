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
#include "FileTreeControl.h"

CFileTreeControl::CFileTreeControl() : MessageTarget(COptions::FileTreeColumnOrder.Ptr(), COptions::FileTreeColumnWidths.Ptr(), COptions::FileTreeColumnVisibility.Ptr(), LF_FILETREE, true)
{
    m_singleton = this;
}

bool CFileTreeControl::CreateExtended(const DWORD dwExStyle, const DWORD dwStyle, const RECT& rect,
    CWnd* pParentWnd, const UINT nID)
{
    const bool created = CTreeListControl::CreateExtended(dwExStyle, dwStyle, rect, pParentWnd, nID);
    if (created && m_toolTip.Create(this))
    {
        m_toolTip.AddTool(this, PortionToolTipId, CRect(), LPSTR_TEXTCALLBACKW);
        m_toolTip.SetMaxTipWidth(ScaleForDpi(400));
        m_toolTip.Activate();
    }
    return created;
}

bool CFileTreeControl::GetAscendingDefault(const int column)
{
    return column == COL_NAME || column == COL_LAST_CHANGE;
}

bool CFileTreeControl::GetPortionToolTip(const CPoint point, CRect& rect, std::wstring& text) const
{
    const int index = HitTest(point);
    const auto* item = static_cast<const CItem*>(GetItem(index));
    if (item == nullptr) return false;

    const CItem* parent = item->GetParent();
    if (parent == nullptr || !parent->IsDone()) return false;

    const int column = SubItemToColumn(COL_SIZE_PROPORTION);
    if (column < 0) return false;

    CRect bar = GetWholeSubitemRect(index, column);
    bar.Deflate(2, 4);
    bar.left += item->GetIndent() * ScaleForDpi(COptions::SizeProportionIndent);

    CRect visibleBar;
    const CRect client = ClientRect();
    if (!visibleBar.Intersect(bar, client) || !visibleBar.Contains(point)) return false;

    rect = visibleBar;
    text = Localization::Format(IDS_SIZE_PROPORTION_TOOLTIPss, FormatDouble(item->GetAbsoluteFraction() * 100),
        FormatDouble(item->GetFraction() * 100));
    return true;
}

void CFileTreeControl::ClearPortionToolTip()
{
    m_toolTip.Pop();
    m_portionToolTipRect = CRect();
    m_portionToolTipText.clear();
    m_toolTip.SetToolRect(this, PortionToolTipId, CRect());
}

// Select the first item of the same parent as the first selected item that matches any of the specified ITEMTYPE
void CFileTreeControl::SelectFirstItemByType(const ITEMTYPE itemType)
{
    const CItem* const itemSelected = GetFirstSelectedItem<CItem>();
    if (!itemSelected) return;

    const ScopedRedrawPause lock(this); // Supress redraw until the end of the function
    const CItem* const itemTarget = itemSelected->GetParent();

    const auto it = std::ranges::find_if(m_items, [&](const CWdsListItem* item) {
        auto* const itemCurrent = static_cast<const CItem*>(item);
        return itemCurrent->GetParent() == itemTarget && itemCurrent->IsTypeOrFlag(itemType);
    });

    if (it != m_items.end())
    {
        DeselectAll();
        const int i = static_cast<int>(std::distance(m_items.begin(), it));
        SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        EnsureVisible(i, false);
    }
}

void CFileTreeControl::OnHScroll(const UINT nSBCode, const UINT nPos, CWnd* pScrollBar)
{
    ClearPortionToolTip();
    CTreeListControl::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CFileTreeControl::OnKeyDown(const UINT nChar, const UINT nRepCnt, const UINT nFlags)
{
    ClearPortionToolTip();
    if (IsKeyDown(VK_CONTROL))
    {
        if (nChar == VK_LEFT)
        {
            SelectFirstItemByType(IT_DIRECTORY);
            return;
        }

        if (nChar == VK_RIGHT)
        {
            SelectFirstItemByType(IT_FILE);
            return;
        }
    }

    CTreeListControl::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CFileTreeControl::OnLButtonDown(const UINT nFlags, const CPoint point)
{
    ClearPortionToolTip();
    CTreeListControl::OnLButtonDown(nFlags, point);

    // Hit test
    const int i = HitTest(point);
    if (i == -1) return;

    // Check if item is a hardlink or hardlinks file reference
    const auto* item = static_cast<const CItem*>(GetItem(i));
    if (item == nullptr || !item->IsTypeOrFlag(ITF_HARDLINK, IT_HLINKS_FILE)) return;

    // Validate if in physical size column
    if (!std::ranges::any_of(std::views::iota(0, m_columnCount), [&](const int col)
        {
            LVCOLUMN colInfo{ .mask = LVCF_SUBITEM };
            GetColumn(col, &colInfo);
            return colInfo.iSubItem == COL_SIZE_PHYSICAL && GetWholeSubitemRect(i, col).Contains(point);
        })) return;

    if (item->IsTypeOrFlag(ITF_HARDLINK))
    {
        // Navigate to the hardlink index item
        CItem* indexItem = item->FindHardlinksIndexItem();
        if (indexItem == nullptr) return;

        CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_SELECTION_ACTION, indexItem);
        ExpandItem(indexItem);
    }
    else if (item->IsTypeOrFlag(IT_HLINKS_FILE))
    {
        // Navigate to the actual file in the tree
        CItem* linkedItem = const_cast<CItem*>(item)->GetLinkedItem();
        if (linkedItem == nullptr || linkedItem == item) return;

        CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_SELECTION_ACTION, linkedItem);
    }
}

void CFileTreeControl::OnMouseMove(const UINT nFlags, const CPoint point)
{
    CTreeListControl::OnMouseMove(nFlags, point);

    CRect rect;
    std::wstring text;
    if (!GetPortionToolTip(point, rect, text))
    {
        if (!m_portionToolTipText.empty()) ClearPortionToolTip();
        return;
    }
    if (rect == m_portionToolTipRect && text == m_portionToolTipText) return;

    ClearPortionToolTip();
    m_portionToolTipRect = rect;
    m_portionToolTipText = std::move(text);
    m_toolTip.SetToolRect(this, PortionToolTipId, m_portionToolTipRect);
    MSG message = CurrentMessage();
    m_toolTip.RelayEvent(&message);
}

bool CFileTreeControl::OnMouseWheel(const UINT nFlags, const short zDelta, const CPoint point)
{
    ClearPortionToolTip();
    return CTreeListControl::OnMouseWheel(nFlags, zDelta, point);
}

void CFileTreeControl::OnTtnGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult)
{
    auto* info = reinterpret_cast<NMTTDISPINFOW*>(pNMHDR);
    info->lpszText = m_portionToolTipText.empty() ? nullptr : m_portionToolTipText.data();
    *pResult = 0;
}

void CFileTreeControl::OnVScroll(const UINT nSBCode, const UINT nPos, CWnd* pScrollBar)
{
    ClearPortionToolTip();
    CTreeListControl::OnVScroll(nSBCode, nPos, pScrollBar);
}

bool CFileTreeControl::OnSetCursor(CWnd* pWnd, const UINT nHitTest, const UINT message)
{
    auto defaultReturn = [&] { return CTreeListControl::OnSetCursor(pWnd, nHitTest, message); };
    if (nHitTest != HTCLIENT) return defaultReturn();

    const auto point = ClientCursorPosition();
    if (!point) return defaultReturn();

    // Hit test
    const int i = HitTest(*point);
    if (i == -1) return defaultReturn();

    // Check if item is a hardlink or hardlinks file reference
    const auto* item = static_cast<const CItem*>(GetItem(i));
    if (item == nullptr) return defaultReturn();

    // Check for ITF_HARDLINK or IT_HLINKS_FILE
    const bool isHardlink = item->IsTypeOrFlag(ITF_HARDLINK);
    const bool isHlinksFile = item->IsTypeOrFlag(IT_HLINKS_FILE);

    if (!isHardlink && !isHlinksFile) return defaultReturn();

    // Validate if in physical size column
    if (!std::ranges::any_of(std::views::iota(0, m_columnCount), [&](const int col)
    {
        LVCOLUMN colInfo{ .mask = LVCF_SUBITEM };
        GetColumn(col, &colInfo);
        return colInfo.iSubItem == COL_SIZE_PHYSICAL && GetWholeSubitemRect(i, col).Contains(*point);
    })) return defaultReturn();

    SetCursor(LoadCursorW(nullptr, IDC_HAND));
    return true;
}
