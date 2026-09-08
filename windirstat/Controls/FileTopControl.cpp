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
#include "FileTreeView.h"

CFileTopControl::CFileTopControl() : CTreeListControl(COptions::TopViewColumnOrder.Ptr(), COptions::TopViewColumnWidths.Ptr(), COptions::TopViewColumnVisibility.Ptr(), LF_TOPLIST, false)
{
    m_singleton = this;
}

bool CFileTopControl::GetAscendingDefault(const int column)
{
    return column == COL_ITEMTOP_NAME || column == COL_ITEMTOP_LAST_CHANGE;
}

void CFileTopControl::ProcessTop(CItem * item)
{
    // Do not process if we are not tracking large files
    if (COptions::LargeFileCount == 0) return;

    m_queuedSet.push(item);
}

void CFileTopControl::SortItems()
{

    // Verify at least root exists
    if (GetItemCount() == 0) return;

    // Record size and complete resort if top N changed
    const auto topN = static_cast<size_t>(COptions::LargeFileCount.Obj());
    if (topN != m_previousTopN)
    {
        m_previousTopN = topN;
        m_needsResort = true;
    }

    bool topChanged = m_needsResort;
    const auto considerItem = [&](CItem* candidate)
    {
        if (topN == 0) return;
        if (m_topItems.size() < topN)
        {
            m_topItems.push_back(candidate);
            std::ranges::push_heap(m_topItems, CompareBySize);
        }
        else
        {
            if (!CompareBySize(candidate, m_topItems.front())) return;
            std::ranges::pop_heap(m_topItems, CompareBySize);
            m_topItems.back() = candidate;
            std::ranges::push_heap(m_topItems, CompareBySize);
        }
        topChanged = true;
    };

    // Keep candidates for refilling the top list after deletions or a limit change.
    CItem* newItem = nullptr;
    while (m_queuedSet.pop(newItem))
    {
        m_sizeMap.push_back(newItem);
        if (!m_needsResort) considerItem(newItem);
    }

    if (m_needsResort)
    {
        m_topItems.clear();
        for (auto* candidate : m_sizeMap) considerItem(candidate);
        m_needsResort = false;
    }

    if (!topChanged)
    {
        CTreeListControl::SortItems();
        return;
    }

    // Update visual item removals
    auto itemTrackerCopy = std::unordered_map(m_itemTracker);
    for (const auto& largeItem : m_topItems)
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
    const ScopedRedrawPause lock(this);
    for (const auto& itemTop : itemTrackerCopy | std::views::values)
    {
        m_itemTracker.erase(itemTop->GetLinkedItem());
        m_rootItem->RemoveTopItemChild(itemTop);
    }

    CTreeListControl::SortItems();
}

void CFileTopControl::RemoveItem(CItem* item)
{
    // Fold pending scan results into the tracked list before calculating
    // removals so queued descendants do not get re-added by SortItems().
    CItem* newItem = nullptr;
    while (m_queuedSet.pop(newItem))
    {
        m_sizeMap.push_back(newItem);
    }

    // Create list of all items to remove
    std::unordered_set<CItem*> toRemove;
    std::vector queue{ item };
    while (!queue.empty())
    {
        const auto qitem = queue.back();
        queue.pop_back();

        if (qitem->IsTypeOrFlag(IT_FILE))
        {
            toRemove.emplace(qitem);
        }
        else if (!qitem->IsLeaf()) for (const auto& child : qitem->GetChildren())
        {
            if (child->IsTypeOrFlag(IT_FILE)) toRemove.emplace(child);
            else queue.push_back(child);
        }
    }

    // Remove items in bulk
    m_needsResort = true;
    std::erase_if(m_sizeMap, [&](const auto& itemToRemove)
    {
        return toRemove.contains(itemToRemove);
    });

    // Use the sort function to remove visual items
    CMainFrame::Get()->InvokeInMessageThread([&]
    {
        SortItems();
    });
}

void CFileTopControl::AfterDeleteAllItems()
{
    // Reset trackers
    m_sizeMap.clear();
    m_itemTracker.clear();
    m_topItems.clear();
    m_needsResort = true;

    // Delete and recreate root item
    delete m_rootItem;
    m_rootItem = new CItemTop();
    InsertItem(0, m_rootItem);
    m_rootItem->SetExpanded(true);
}
