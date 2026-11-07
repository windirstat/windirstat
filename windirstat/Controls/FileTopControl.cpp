// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

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
    // Keep a reserve of replacements without retaining every scanned file.
    std::scoped_lock lock(m_topMutex);
    const size_t capacity = 2 * m_topLimit;

    // Once any file is omitted, admit only files that outrank the smallest cached one.
    if (m_hasUncachedItems || m_topItems.size() == capacity)
    {
        m_hasUncachedItems = true;
        if (m_topItems.empty() || !CompareBySize(item, m_topItems.front())) return;
    }
    if (m_topItems.size() == capacity)
    {
        std::ranges::pop_heap(m_topItems, CompareBySize);
        m_topItems.pop_back();
    }
    m_topItems.push_back(item);
    std::ranges::push_heap(m_topItems, CompareBySize);
    m_needsResort = true;
}

void CFileTopControl::SortItems()
{
    UpdateItems({}, !CWinDirStatModel::Get()->IsScanRunning());
}

void CFileTopControl::UpdateItems(std::span<CItem* const> excluded, const bool allowRefill)
{
    // Verify at least root exists
    if (GetItemCount() == 0) return;

    // Remove refreshed branches before their items are destroyed, including cached spare entries.
    const std::unordered_set<CItem*> excludedRoots(std::from_range, excluded);
    const auto isExcluded = [&](CItem* item)
    {
        for (; item != nullptr; item = item->GetParent())
            if (excludedRoots.contains(item)) return true;
        return false;
    };
    CItem* root = CWinDirStatModel::Get()->GetRootItem();
    const auto topN = static_cast<size_t>(COptions::LargeFileCount.Obj());
    bool refill;
    {
        std::scoped_lock lock(m_topMutex);
        if (!excludedRoots.empty() && std::erase_if(m_topItems, isExcluded) != 0)
        {
            std::ranges::make_heap(m_topItems, CompareBySize);
            m_needsResort = true;
        }
        if (m_topLimit != topN) m_needsResort = true;
        m_topLimit = topN;
        while (m_topItems.size() > 2 * topN)
        {
            std::ranges::pop_heap(m_topItems, CompareBySize);
            m_topItems.pop_back();
            m_hasUncachedItems = true;
        }

        // Only stopped enumeration may refill; a fully cached small tree needs no repeat walk.
        refill = allowRefill && root != nullptr && m_hasUncachedItems && m_topItems.size() < topN;
        if (refill)
        {
            m_topItems.clear();
            m_hasUncachedItems = false;
            m_needsResort = true;
        }
    }
    if (refill)
    {
        std::vector queue{ root };
        while (!queue.empty())
        {
            CItem* item = queue.back();
            queue.pop_back();
            if (excludedRoots.contains(item)) continue;
            if (item->IsTypeOrFlag(IT_FILE)) ProcessTop(item);
            else if (!item->IsLeaf() && !item->IsTypeOrFlag(IT_HLINKS)) queue.append_range(item->GetChildren());
        }
    }

    // Release the worker lock before updating controls or dispatching UI messages.
    std::vector<CItem*> topItems;
    bool topChanged;
    {
        std::scoped_lock lock(m_topMutex);
        topChanged = std::exchange(m_needsResort, false);
        if (topChanged) topItems = m_topItems;
    }

    if (!topChanged)
    {
        CTreeListControl::SortItems();
        return;
    }

    // Display only the requested largest entries, regardless of the user's column sort order.
    if (topItems.size() > topN)
    {
        std::ranges::nth_element(topItems, topItems.begin() + topN, CompareBySize);
        topItems.resize(topN);
    }

    // Collect visual additions and removals before changing the list.
    const ScopedRedrawPause lock(this);
    std::vector<CItemTop*> added;
    auto itemTrackerCopy = std::unordered_map(m_itemTracker);
    for (const auto& largeItem : topItems)
    {
        if (m_itemTracker.contains(largeItem))
        {
            itemTrackerCopy.erase(largeItem);
            continue;
        }

        const auto itemTop = new CItemTop(largeItem);
        added.push_back(itemTop);
        m_itemTracker[largeItem] = itemTop;
    }

    // Apply each batch once so surviving rows only need one index update.
    std::unordered_set<const CTreeListItem*> removed;
    for (const auto& itemTop : itemTrackerCopy | std::views::values)
    {
        m_itemTracker.erase(itemTop->GetLinkedItem());
        removed.insert(itemTop);
    }
    m_rootItem->RemoveTopItemChildren(removed);
    m_rootItem->AddTopItemChildren(added);

    CTreeListControl::SortItems();
}

void CFileTopControl::AfterDeleteAllItems()
{
    // Reset trackers
    m_itemTracker.clear();
    m_topItems.clear();
    m_hasUncachedItems = true;
    m_topLimit = COptions::LargeFileCount;
    m_needsResort = true;

    // Delete and recreate root item
    delete m_rootItem;
    m_rootItem = new CItemTop();
    InsertItem(0, m_rootItem);
    m_rootItem->SetExpanded(true);
}
