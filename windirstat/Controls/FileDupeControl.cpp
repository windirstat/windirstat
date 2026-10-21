// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "ItemDupe.h"
#include "FileTreeView.h"

CFileDupeControl::CFileDupeControl() : CTreeListControl(COptions::DupeViewColumnOrder.Ptr(), COptions::DupeViewColumnWidths.Ptr(), COptions::DupeViewColumnVisibility.Ptr(), LF_DUPELIST, false)
{
    m_singleton = this;
}

bool CFileDupeControl::GetAscendingDefault(const int column)
{
    return column == COL_ITEMDUP_NAME || column == COL_ITEMDUP_LAST_CHANGE;
}

void CFileDupeControl::ProcessDuplicate(CItem* item, BlockingQueue<CItem*>* queue)
{
    if (!COptions::ScanForDuplicates || item->IsTypeOrFlag(ITF_REMOTE)) return;
    if (item->IsTypeOrFlag(ITRP_CLOUD) && COptions::SkipDupeDetectionCloudLinks)
    {
        // Show warning and skip
        if (m_showCloudWarningOnThisScan.exchange(false))
        {
            if (const auto [nID, isChecked] = CMessageBoxDlg::Show(Localization::Lookup(IDS_DUPLICATES_WARNING),
                Localization::Lookup(IDS_DONT_SHOW_AGAIN), false, MB_OK | MB_ICONINFORMATION, this);
                nID == IDOK && isChecked)
            {
                COptions::ShowDupeDetectionCloudLinksWarning = false;
            }
        }

        return;
    }

    // First see if there's more than one file of this size since there is no need to
    // hash if there is only a single file of this size
    {
        std::scoped_lock lock(m_trackerMutex);
        auto& sizeSetLookup = m_sizeTracker[item->GetSizeLogical()];
        sizeSetLookup.emplace_back(item);
        if (sizeSetLookup.size() >= 2) m_pendingHashes.emplace_back(item, 0);
        if (sizeSetLookup.size() == 2) m_pendingHashes.emplace_back(sizeSetLookup.front(), 0);
    }

    // Now we have multiple files of the same size, so we need to hash them
    static constexpr std::array hashLevels{ ITHASH_SMALL, ITHASH_MEDIUM, ITHASH_LARGE };
    while (true)
    {
        queue->WaitIfSuspended();
        std::unique_lock lock(m_trackerMutex);
        if (m_pendingHashes.empty()) return;
        const auto [itemToHash, level] = m_pendingHashes.back();
        m_pendingHashes.pop_back();
        const ITEMTYPE hashLevel = hashLevels[level];
        const auto size = itemToHash->GetSizeLogical();
        const bool sampled = m_sampleLargeFiles && hashLevel == ITHASH_LARGE && size > 64ull * wds::Mi;
        const size_t maxHashLevel = size <= HashThreshold(ITHASH_SMALL) ? 0 :
            size <= HashThreshold(ITHASH_MEDIUM) ? 1 : 2;

        // Skip if already marked as unhashable or hashed at this level
        if (itemToHash->IsTypeOrFlag(ITHASH_SKIP, hashLevel)) continue;
        itemToHash->SetFlag(hashLevel);

        // Compute the hash for the file
        lock.unlock();
        std::vector<BYTE> hash;
        try { hash = itemToHash->GetFileHash(HashThreshold(hashLevel), queue, sampled); }
        catch (...)
        {
            lock.lock();
            itemToHash->SetFlag(hashLevel, true);
            m_pendingHashes.emplace_back(itemToHash, level);
            throw;
        }
        lock.lock();

        // Mark as bad if not hashable
        if (hash.empty())
        {
            itemToHash->SetFlag(ITHASH_SKIP);
            continue;
        }

        // Add this hash to the tracker
        HashKey key{ size, std::move(hash) };
        auto& entry = m_hashTrackers[level][key];
        entry.emplace_back(itemToHash);
        if (entry.size() < 2) continue;

        // Promote the first matching pair once, then only each new arrival.
        if (level != maxHashLevel)
        {
            m_pendingHashes.emplace_back(itemToHash, level + 1);
            if (entry.size() == 2) m_pendingHashes.emplace_back(entry.front(), level + 1);
            continue;
        }

        // Already at max level: publish completed matches before another hash can be cancelled.
        CItem* firstMatch = entry.size() == 2 ? entry.front() : nullptr;
        lock.unlock();
        std::scoped_lock nodeLock(m_nodeTrackerMutex);
        auto& dupeParent = m_nodeTracker[key];
        if (dupeParent == nullptr)
        {
            // Create new root item to hold these duplicates
            dupeParent = new CItemDupe(key.second, sampled);
            m_pendingListAdds.push(std::make_pair(nullptr, dupeParent));
        }

        // Add all items under the same parent
        auto& hashParentNode = m_childTracker[dupeParent];
        for (auto* itemToAdd : { firstMatch, itemToHash })
        {
            if (itemToAdd == nullptr || !hashParentNode.emplace(itemToAdd).second) continue;
            m_pendingListAdds.push(std::make_pair(dupeParent, new CItemDupe(itemToAdd)));
        }
    }
}

void CFileDupeControl::SortItems()
{

    // Add items to the list
    if (!m_pendingListAdds.empty())
    {
        const ScopedRedrawPause lock(this);
        std::vector<CItemDupe*> groups;
        std::unordered_map<CItemDupe*, std::vector<CItemDupe*>> additions;
        std::pair<CItemDupe*, CItemDupe*> pair;
        while (m_pendingListAdds.pop(pair))
        {
            const auto& [parent, child] = pair;
            (parent == nullptr ? groups : additions[parent]).push_back(child);
        }

        // Publish new groups before their children so visible parents exist for each batch.
        m_rootItem->AddDupeItemChildren(groups);
        for (const auto& [parent, children] : additions) parent->AddDupeItemChildren(children);

    }

    CTreeListControl::SortItems();
}

void CFileDupeControl::RemoveItems(std::span<CItem* const> items)
{
    // Exit immediately if duplicate detection is disabled
    if (!COptions::ScanForDuplicates || items.empty()) return;

    // Publish queued nodes before pruning references to items that will be refreshed.
    SortItems();

    // Enumerate child items and mark all as unhashed
    std::unordered_set<CItem*> removedItems;
    std::vector queue(items.begin(), items.end());
    while (!queue.empty())
    {
        const auto qitem = queue.back();
        queue.pop_back();
        if (qitem->IsTypeOrFlag(IT_FILE))
        {
            // Mark all files as not being hashed anymore
            qitem->SetHashType(ITHASH_NONE, false);
            removedItems.insert(qitem);
        }
        else if (!qitem->IsLeaf())
        {
            std::ranges::copy(qitem->GetChildren(), std::back_inserter(queue));
        }
    }
    for (auto& sizeSet : m_sizeTracker | std::views::values)
        std::erase_if(sizeSet, [&](CItem* file) { return removedItems.contains(file); });
    std::erase_if(m_sizeTracker, [](const auto& pair)
    {
        return pair.second.empty();
    });

    std::erase_if(m_pendingHashes, [&](const auto& pending) { return removedItems.contains(pending.first); });

    // Remove all unhashed files from hash trackers
    for (auto& hashTracker : m_hashTrackers)
    {
        for (auto& hashSet : hashTracker | std::views::values)
        {
            // Skip if no matches of the item associated with this hash
            std::erase_if(hashSet, [](const auto& hashItem)
            {
                return !hashItem->IsTypeOrFlag(ITHASH_MASK);
            });
        }

        // Clean up empty structures
        std::erase_if(hashTracker, [](const auto& pair)
        {
            return pair.second.empty();
        });
    }

    // Clean up any empty visual nodes in the list
    const ScopedRedrawPause lock(this);
    std::unordered_set<const CTreeListItem*> removed;
    for (auto nodeIter = m_nodeTracker.begin(); nodeIter != m_nodeTracker.end(); )
    {
        auto& [dupeParentKey, dupeParent] = *nodeIter;

        // Remove from child tracker
        auto& childItems = m_childTracker[dupeParent];
        std::erase_if(childItems, [](CItem* child) { return !child->IsTypeOrFlag(ITHASH_MASK); });
        if (childItems.size() <= 1)
        {
            removed.insert(dupeParent);
            m_childTracker.erase(dupeParent);
            nodeIter = m_nodeTracker.erase(nodeIter);
        }
        else
        {
            for (auto* child : dupeParent->GetChildren())
                if (!childItems.contains(child->GetLinkedItem())) removed.insert(child);
            ++nodeIter;
        }
    }

    // Detach visible rows once, then compact each surviving group's children and the root.
    OnItemsRemoved(removed);
    for (auto* parent : m_nodeTracker | std::views::values) parent->RemoveDupeItemChildren(removed);
    m_rootItem->RemoveDupeItemChildren(removed);
}

void CFileDupeControl::AfterDeleteAllItems()
{
    // Snapshot the mode before new scan workers start.
    m_sampleLargeFiles = COptions::SampleLargeFiles;

    // Reset duplicate warning
    m_showCloudWarningOnThisScan = COptions::ShowDupeDetectionCloudLinksWarning;

    // Clean up support lists
    m_pendingListAdds.clear();
    m_nodeTracker.clear();
    for (auto& tracker : m_hashTrackers) tracker.clear();
    m_pendingHashes.clear();
    m_sizeTracker.clear();
    m_childTracker.clear();

    // Delete and recreate root item
    delete m_rootItem;
    m_rootItem = new CItemDupe();
    InsertItem(0, m_rootItem);
    m_rootItem->SetExpanded(true);
}
