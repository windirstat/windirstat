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
#include "ItemTop.h"
#include "FileTopControl.h"

CFileTopControl::CFileTopControl() : CTreeListControl(COptions::TopViewColumnOrder.Ptr(), COptions::TopViewColumnWidths.Ptr())
{
    m_Singleton = this;
}

bool CFileTopControl::GetAscendingDefault(const int column)
{
    return column == COL_ITEMTOP_SIZE_PHYSICAL ||
        column == COL_ITEMTOP_SIZE_LOGICAL ||
        column == COL_ITEMTOP_LAST_CHANGE;
}

BEGIN_MESSAGE_MAP(CFileTopControl, CTreeListControl)
    ON_WM_SETFOCUS()
    ON_WM_KEYDOWN()
    ON_NOTIFY_REFLECT_EX(LVN_DELETEALLITEMS, OnDeleteAllItems)
END_MESSAGE_MAP()

CFileTopControl* CFileTopControl::m_Singleton = nullptr;

void CFileTopControl::ProcessTop(CItem * item)
{
    // Do not process if we are not tracking large files
    if (COptions::LargeFileCount == 0) return;

    m_QueuedSet.push(item);
}

void CFileTopControl::SortItems()
{
    ASSERT(AfxGetThread() != nullptr);

    // Verify at least root exists
    if (GetItemCount() == 0) return;

    // Record size and complete resort if top N changed
    const auto topN = static_cast<size_t>(COptions::LargeFileCount.Obj());
    if (topN != m_PreviousTopN)
    {
        std::ranges::sort(m_SizeMap, CompareBySize);
        m_PreviousTopN = topN;
        m_NeedsResort = true;
    }

    // Process queued items - only mark for resort if item could affect top N
    CItem* newItem = nullptr;
    while (m_QueuedSet.pop(newItem))
    {
        // Check if this item could affect the top N
        if (m_SizeMap.size() < topN || newItem->GetSizeLogical() > m_TopNMinSize)
        {
            m_NeedsResort = true;
        }
        m_SizeMap.push_back(newItem);
    }

    // Only sort the vector if we need to update the top N
    if (!m_NeedsResort)
    {
        CTreeListControl::SortItems();
        return;
    }
    
    m_NeedsResort = false;
    const auto sortEnd = m_SizeMap.size() <= topN ? m_SizeMap.end()
        : m_SizeMap.begin() + topN;

    // Partial sort to get top N items at the front
    std::partial_sort(m_SizeMap.begin(), sortEnd, m_SizeMap.end(), CompareBySize);

    // Update minimum size in top N for future comparisons
    if (m_SizeMap.size() >= topN)
    {
        m_TopNMinSize = m_SizeMap[topN - 1]->GetSizeLogical();
    }
    else if (!m_SizeMap.empty())
    {
        m_TopNMinSize = m_SizeMap.back()->GetSizeLogical();
    }
    else
    {
        m_TopNMinSize = 0;
    }

    // Update visual item removals
    const auto root = reinterpret_cast<CItemTop*>(GetItem(0));
    auto itemTrackerCopy = std::unordered_map(m_ItemTracker);
    for (const auto& largeItem : m_SizeMap | std::views::take(topN))
    {
        if (m_ItemTracker.contains(largeItem))
        {
            itemTrackerCopy.erase(largeItem);
            continue;
        }
        
        const auto itemTop = new CItemTop(largeItem);
        root->AddTopItemChild(itemTop);
        m_ItemTracker[largeItem] = itemTop;
    }

    // Handle visual item additions
    for (const auto& itemTop : itemTrackerCopy | std::views::values)
    {
        m_ItemTracker.erase(reinterpret_cast<CItem*>(itemTop->GetLinkedItem()));
        root->RemoveTopItemChild(itemTop);
    }

    CTreeListControl::SortItems();
}

void CFileTopControl::RemoveItem(CItem* item)
{
    // Create list of all items to remove
    std::unordered_set<CItem*> toRemove;
    std::stack<CItem*> queue({ item });
    while (!queue.empty())
    {
        const auto qitem = queue.top();
        queue.pop();

        if (qitem->IsTypeOrFlag(IT_FILE))
        {
            toRemove.emplace(qitem);
        }
        else if (!qitem->IsLeaf()) for (const auto& child : qitem->GetChildren())
        {
            if (child->IsTypeOrFlag(IT_FILE)) toRemove.emplace(child);
            else queue.push(child);
        }
    }

    // Remove items in bulk
    m_NeedsResort = true;
    std::erase_if(m_SizeMap, [&](const auto& item)
    {
        return toRemove.contains(item);
    });

    // Use the sort function to remove visual items
    CMainFrame::Get()->InvokeInMessageThread([&]
    {
        SortItems();
    });
}

void CFileTopControl::OnItemDoubleClick(const int i)
{
    if (const auto item = reinterpret_cast<const CItem*>(GetItem(i)->GetLinkedItem());
        item != nullptr && item->IsTypeOrFlag(IT_FILE))
    {
        CDirStatDoc::OpenItem(item);
    }
    else
    {
        CTreeListControl::OnItemDoubleClick(i);
    }
}

BOOL CFileTopControl::OnDeleteAllItems(NMHDR*, LRESULT* pResult)
{
    // Reset trackers
    m_SizeMap.clear();
    m_ItemTracker.clear();
    m_TopNMinSize = 0;
    m_NeedsResort = true;

    // Allow deletion to proceed
    *pResult = FALSE;
    return FALSE;
}

void CFileTopControl::OnSetFocus(CWnd* pOldWnd)
{
    CTreeListControl::OnSetFocus(pOldWnd);
    CMainFrame::Get()->SetLogicalFocus(LF_TOPLIST);
}

void CFileTopControl::OnKeyDown(const UINT nChar, const UINT nRepCnt, const UINT nFlags)
{
    if (nChar == VK_TAB)
    {
        CMainFrame::Get()->MoveFocus(LF_EXTLIST);
    }
    else if (nChar == VK_ESCAPE)
    {
        CMainFrame::Get()->MoveFocus(LF_NONE);
    }
    CTreeListControl::OnKeyDown(nChar, nRepCnt, nFlags);
}
