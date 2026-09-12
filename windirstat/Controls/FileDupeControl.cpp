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
    if (!COptions::ScanForDuplicates) return;
    if (item->IsTypeOrFlag(ITRP_CLOUD) && COptions::SkipDupeDetectionCloudLinks)
    {
        // Show warning and skip
        if (m_showCloudWarningOnThisScan)
        {
            m_showCloudWarningOnThisScan = false;
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
        std::pair<CItemDupe*, CItemDupe*> pair;
        while (m_pendingListAdds.pop(pair))
        {
            const auto& [parent, child] = pair;
            (parent == nullptr ? m_rootItem : parent)->AddDupeItemChild(child);
        }

    }

    CTreeListControl::SortItems();
}

void CFileDupeControl::RemoveItem(CItem* item)
{
    // Exit immediately if duplicate detection is disabled
    if (!COptions::ScanForDuplicates) return;

    // Publish queued nodes before pruning references to items that will be refreshed.
    SortItems();

    // Enumerate child items and mark all as unhashed
    std::unordered_set<CItem*> removedItems;
    std::vector queue({ item });
    while (!queue.empty())
    {
        const auto qitem = queue.back();
        queue.pop_back();
        if (qitem->IsTypeOrFlag(IT_FILE))
        {
            // Mark all files as not being hashed anymore
            std::erase(m_sizeTracker[qitem->GetSizeLogical()], qitem);
            qitem->SetHashType(ITHASH_NONE, false);
            removedItems.insert(qitem);
        }
        else if (!qitem->IsLeaf())
        {
            std::ranges::copy(qitem->GetChildren(), std::back_inserter(queue));
        }
    }
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
    for (auto nodeIter = m_nodeTracker.begin(); nodeIter != m_nodeTracker.end(); )
    {
        auto& [dupeParentKey, dupeParent] = *nodeIter;

        // Remove from child tracker
        auto& childItems = m_childTracker[dupeParent];
        for (auto childItem = childItems.begin(); childItem != childItems.end(); )
        {
            // Nothing to do if still marked as hashed
            if ((*childItem)->IsTypeOrFlag(ITHASH_MASK))
            {
                ++childItem;
                continue;
            }

            // Remove from child tracker and visual tree
            const auto& children = dupeParent->GetChildren();
            const auto visualIter = std::ranges::find_if(children, [&](CItemDupe* visualChild) {
                return visualChild->GetLinkedItem() == *childItem;
            });
            const bool foundVisualChild = visualIter != children.end();
            if (foundVisualChild)
            {
                dupeParent->RemoveDupeItemChild(*visualIter);
            }

            // Only erase from tracker when the visual child was actually removed;
            // if no visual match exists (pending add not yet flushed), leave the
            // tracker entry intact so it stays in sync with the visual tree.
            if (foundVisualChild)
                childItem = childItems.erase(childItem);
            else
                ++childItem;
        }

        // When only one child left, remove child item
        if (dupeParent->GetChildren().size() == 1)
        {
            dupeParent->RemoveDupeItemChild(dupeParent->GetChildren().front());
        }

        // When no children left, remove parent item
        if (dupeParent->GetChildren().empty())
        {
            m_rootItem->RemoveDupeItemChild(dupeParent);
            nodeIter = m_nodeTracker.erase(nodeIter);
        }
        else
        {
            ++nodeIter;
        }
    }
    std::erase_if(m_childTracker, [](const auto& pair)
    {
        return pair.second.size() <= 1;
    });
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
