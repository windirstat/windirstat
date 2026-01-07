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
    m_singleton = this;
}

bool CFileTopControl::GetAscendingDefault(const int column)
{
    return column == COL_ITEMTOP_NAME || column == COL_ITEMTOP_LAST_CHANGE;
}

BEGIN_MESSAGE_MAP(CFileTopControl, CTreeListControl)
    ON_WM_SETFOCUS()
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()

CFileTopControl* CFileTopControl::m_singleton = nullptr;

void CFileTopControl::ProcessTop(CItem * item)
{
    // Do not process if we are not tracking large files
    if (COptions::LargeFileCount == 0) return;

    m_queuedSet.push(item);
}

void CFileTopControl::SortItems()
{
    ASSERT(AfxGetThread() != nullptr);

    // Verify at least root exists
    if (GetItemCount() == 0) return;

    // Record size and complete resort if top N changed
    const auto topN = static_cast<size_t>(COptions::LargeFileCount.Obj());
    if (topN != m_previousTopN)
    {
        std::ranges::sort(m_sizeMap, CompareBySize);
        m_previousTopN = topN;
        m_needsResort = true;
    }

    // Process queued items - only mark for resort if item could affect top N
    CItem* newItem = nullptr;
    while (m_queuedSet.pop(newItem))
    {
        // Check if this item could affect the top N
        if (m_sizeMap.size() < topN || newItem->GetSizeLogical() > m_topNMinSize)
        {
            m_needsResort = true;
        }
        m_sizeMap.push_back(newItem);
    }

    // Only sort the vector if we need to update the top N
    if (!m_needsResort)
    {
        CTreeListControl::SortItems();
        return;
    }
    
    m_needsResort = false;
    const auto sortEnd = m_sizeMap.size() <= topN ? m_sizeMap.end()
        : m_sizeMap.begin() + topN;

    // Partial sort to get top N items at the front
    std::partial_sort(m_sizeMap.begin(), sortEnd, m_sizeMap.end(), CompareBySize);

    // Update minimum size in top N for future comparisons
    if (m_sizeMap.size() >= topN)
    {
        m_topNMinSize = m_sizeMap[topN - 1]->GetSizeLogical();
    }
    else if (!m_sizeMap.empty())
    {
        m_topNMinSize = m_sizeMap.back()->GetSizeLogical();
    }
    else
    {
        m_topNMinSize = 0;
    }

    // Update visual item removals
    auto itemTrackerCopy = std::unordered_map(m_itemTracker);
    for (const auto& largeItem : m_sizeMap | std::views::take(topN))
    {
        if (m_itemTracker.contains(largeItem))
        {
            itemTrackerCopy.erase(largeItem);
            continue;
        }
        
        const auto itemTop = new CItemTop(largeItem);
        m_rootItem->AddTopItemChild(itemTop);
        m_itemTracker[largeItem] = itemTop;
    }

    // Handle visual item additions
    for (const auto& itemTop : itemTrackerCopy | std::views::values)
    {
        m_itemTracker.erase(itemTop->GetLinkedItem());
        m_rootItem->RemoveTopItemChild(itemTop);
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
    m_needsResort = true;
    std::erase_if(m_sizeMap, [&](const auto& item)
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
    if (const auto item = GetItem(i)->GetLinkedItem();
        item != nullptr && item->IsTypeOrFlag(IT_FILE))
    {
        CDirStatDoc::OpenItem(item);
    }
    else
    {
        CTreeListControl::OnItemDoubleClick(i);
    }
}

void CFileTopControl::AfterDeleteAllItems()
{
    // Reset trackers
    m_sizeMap.clear();
    m_itemTracker.clear();
    m_topNMinSize = 0;
    m_needsResort = true;

    // Delete and recreate root item
    if (m_rootItem != nullptr) delete m_rootItem;
    m_rootItem = new CItemTop();
    InsertItem(0, m_rootItem);
    m_rootItem->SetExpanded(true);
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
