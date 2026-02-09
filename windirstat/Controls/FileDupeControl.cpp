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

CFileDupeControl::CFileDupeControl() : CTreeListControl(COptions::DupeViewColumnOrder.Ptr(), COptions::DupeViewColumnWidths.Ptr())
{
    m_singleton = this;
}

bool CFileDupeControl::GetAscendingDefault(const int column)
{
    return column == COL_ITEMDUP_NAME || column == COL_ITEMDUP_LAST_CHANGE;
}

BEGIN_MESSAGE_MAP(CFileDupeControl, CTreeListControl)
    ON_WM_SETFOCUS()
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()

CFileDupeControl* CFileDupeControl::m_singleton = nullptr;

void CFileDupeControl::ProcessDuplicate(CItem* item, BlockingQueue<CItem*>* queue)
{
    if (!COptions::ScanForDuplicates) return;
    if (COptions::SkipDupeDetectionCloudLinks && item->IsTypeOrFlag(ITRP_CLOUD) && m_showCloudWarningOnThisScan)
    {
        // Disable remainder for test of this scan
        m_showCloudWarningOnThisScan = false;

        // Show warning dialog
        CMessageBoxDlg dlg(Localization::Lookup(IDS_DUPLICATES_WARNING), Localization::LookupNeutral(AFX_IDS_APP_TITLE),
            MB_OK | MB_ICONINFORMATION, this, {}, Localization::Lookup(IDS_DONT_SHOW_AGAIN), false);
        if (dlg.DoModal() == IDOK && dlg.IsCheckboxChecked())
        {
            COptions::ShowDupeDetectionCloudLinksWarning = false;
        }
        return;
    }

    // Fetch the configuration information based on file size
    const auto size = item->GetSizeLogical();
    auto [hashTracker, hashTrackerMutex, maxHashLevel] = [&]()->std::tuple
        <std::map<std::vector<BYTE>, std::vector<CItem*>>&, std::mutex&, ITEMTYPE>
    {
        if (size <= HashThresold(ITHASH_SMALL)) return { m_trackerSmall, m_trackerSmallMutex, ITHASH_SMALL };
        if (size > HashThresold(ITHASH_MEDIUM)) return { m_trackerLarge, m_trackerLargeMutex, ITHASH_LARGE };
        else return { m_trackerMedium, m_trackerMediumMutex, ITHASH_MEDIUM };
    }();

    // First see if there's more than one size of this file since there is no need to
    // hash if there is only a single file of this size
    std::vector<CItem*> hashSet;
    if (std::scoped_lock lock(m_sizeTrackerMutex); true)
    {
        auto& sizeSetLookup = m_sizeTracker[size];
        sizeSetLookup.emplace_back(item);
        hashSet.assign(sizeSetLookup.begin(), sizeSetLookup.end());
        if (sizeSetLookup.size() < 2) return;
    }

    // Now we have multiple files of the same size, so we need to hash them
    ITEMTYPE hashLevel = ITHASH_SMALL;
    std::map<std::vector<BYTE>, std::set<CItem*>> hashItemsWithDupes;
    for (std::unique_lock lock(hashTrackerMutex); !hashSet.empty();)
    {
        // Work on a snapshot of the current set
        std::set<CItem*> nextLevelSet;
        for (auto* itemToHash : std::vector(hashSet))
        {
            // Skip if already marked as unhashable or hashed at this level
            if (itemToHash->IsTypeOrFlag(ITHASH_SKIP, hashLevel)) continue;
            itemToHash->SetFlag(hashLevel);

            // Compute the hash for the file
            const auto hashSize = HashThresold(hashLevel);
            lock.unlock();
            auto hash = itemToHash->GetFileHash(hashSize, queue);
            lock.lock();

            // Mark as bad if not hashable
            if (hash.empty())
            {
                itemToHash->SetFlag(ITHASH_SKIP);
                continue;
            }

            // Add this hash to the tracker
            auto& entry = hashTracker[hash];
            entry.emplace_back(itemToHash);
            auto view = entry | std::views::filter([&](const CItem* x)
                { return x->GetSizeLogical() == size; });
            std::vector subset(view.begin(), view.end());
            if (subset.size() < 2) continue;

            // See if this hash has duplicates
            if (hashLevel == maxHashLevel)
            {
                // Already at max level: record duplicates and stop
                hashItemsWithDupes[hash].insert(subset.begin(), subset.end());
            }
            else
            {
                // Schedule next-level hashing
                nextLevelSet.insert(subset.begin(), subset.end());
            }
        }

        hashSet.assign(nextLevelSet.begin(), nextLevelSet.end());
        if (hashSet.empty()) break;

        // Determine the appropriate hash level for this item based on what's already been hashed
        hashLevel = min(maxHashLevel,
            hashLevel == ITHASH_SMALL ? ITHASH_MEDIUM :
            hashLevel == ITHASH_MEDIUM ? ITHASH_LARGE : ITHASH_LARGE);
    }

    // Lock once and lookup dupeParent before iterating
    if (hashItemsWithDupes.empty()) return;
    std::scoped_lock nodeLock(m_nodeTrackerMutex);
    for (const auto& [hash, itemsWithHash] : hashItemsWithDupes)
    {
        const auto nodeEntry = m_nodeTracker.find(hash);
        auto dupeParent = nodeEntry != m_nodeTracker.end() ? nodeEntry->second : nullptr;

        if (dupeParent == nullptr)
        {
            // Create new root item to hold these duplicates
            dupeParent = new CItemDupe(hash);
            m_pendingListAdds.push(std::make_pair(nullptr, dupeParent));
            m_nodeTracker.emplace(hash, dupeParent);
        }

        // Add all items under the same parent
        for (const auto& itemToAdd : itemsWithHash)
        {
            auto& hashParentNode = m_childTracker[dupeParent];
            if (hashParentNode.contains(itemToAdd)) continue;
            const auto dupeChild = new CItemDupe(itemToAdd);
            m_pendingListAdds.push(std::make_pair(dupeParent, dupeChild));
            hashParentNode.emplace(itemToAdd);
        }
    }
}

void CFileDupeControl::SortItems()
{
    ASSERT(AfxGetThread() != nullptr);

    // Add items to the list
    if (!m_pendingListAdds.empty())
    {
        SetRedraw(FALSE);
        std::pair<CItemDupe*, CItemDupe*> pair;
        while (m_pendingListAdds.pop(pair))
        {
            const auto& [parent, child] = pair;
            (parent == nullptr ? m_rootItem : parent)->AddDupeItemChild(child);
        }
        SetRedraw(TRUE);
    }

    CTreeListControl::SortItems();
}

void CFileDupeControl::RemoveItem(CItem* item)
{
    // Exit immediately if not doing duplicate detector
    if (!COptions::ScanForDuplicates) return;

    // Enumerate child items and mark all as unhashed
    std::vector queue({ item });
    while (!queue.empty())
    {
        const auto qitem = queue.back();
        queue.pop_back();
        if (qitem->IsTypeOrFlag(IT_FILE))
        {
            // Mark as all files as not being hashed anymore
            std::erase(m_sizeTracker[qitem->GetSizeLogical()], qitem);
            qitem->SetHashType(ITHASH_NONE, false);
        }
        else if (!qitem->IsLeaf()) for (const auto& child : qitem->GetChildren())
        {
            queue.push_back(child);
        }
    }
    std::erase_if(m_sizeTracker, [](const auto& pair)
    {
        return pair.second.empty();
    });

    // Remove all unhashed files from hash trackers
    for (auto* hashTracker : { &m_trackerSmall, &m_trackerMedium, &m_trackerLarge })
    {
        for (auto& hashSet : *hashTracker | std::views::values)
        {
            // Skip if no matches of the item associated with this hash
            std::erase_if(hashSet, [](const auto& hashItem)
            {
                return !hashItem->IsTypeOrFlag(ITHASH_MASK);
            });
        }

        // Cleanup empty structures
        std::erase_if(*hashTracker, [](const auto& pair)
        {
            return pair.second.empty();
        });
    }

    // Pause redrawing for mass node removal
    SetRedraw(FALSE);

    // Cleanup any empty visual nodes in the list
    bool erasedNode = false;
    for (auto nodeIter = m_nodeTracker.begin(); nodeIter != m_nodeTracker.end();
        erasedNode ? nodeIter : ++nodeIter, erasedNode = false)
    {
        auto& [dupeParentKey, dupeParent] = *nodeIter;

        // Remove from child tracker
        bool erasedChild = false;
        auto& childItems = m_childTracker[dupeParent];
        for (auto childItem = childItems.begin(); childItem != childItems.end();
            erasedChild ? childItem : ++childItem, erasedChild = false)
        {
            // Nothing to do if still marked as hashed
            if ((*childItem)->IsTypeOrFlag(ITHASH_MASK)) continue;

            // Remove from child tracker and visual tree
            for (auto& visualChild : dupeParent->GetChildren())
            {
                if (visualChild->GetLinkedItem() != (*childItem)) continue;

                dupeParent->RemoveDupeItemChild(visualChild);
                childItem = childItems.erase(childItem);
                erasedChild = true;
                break;
            }
        }

        // When only one child left, remove child item
        if (dupeParent->GetChildren().size() == 1)
        {
            dupeParent->RemoveDupeItemChild(dupeParent->GetChildren().at(0));
        }

        // When no children left, remove parent item
        if (dupeParent->GetChildren().empty())
        {
            m_rootItem->RemoveDupeItemChild(dupeParent);
            nodeIter = m_nodeTracker.erase(nodeIter);
            erasedNode = true;
        }
    }
    std::erase_if(m_childTracker, [](const auto& pair)
    {
        return pair.second.size() <= 1;
    });

    // Resume redrawing and invalidate to force refresh
    SetRedraw(TRUE);
    Invalidate();
}

void CFileDupeControl::OnItemDoubleClick(const int i)
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

void CFileDupeControl::AfterDeleteAllItems()
{
    // Reset duplicate warning
    m_showCloudWarningOnThisScan = COptions::ShowDupeDetectionCloudLinksWarning;

    // Cleanup support lists
    m_pendingListAdds.clear();
    m_nodeTracker.clear();
    m_trackerSmall.clear();
    m_trackerMedium.clear();
    m_trackerLarge.clear();
    m_sizeTracker.clear();
    m_childTracker.clear();

    // Delete and recreate root item
    delete m_rootItem;
    m_rootItem = new CItemDupe();
    InsertItem(0, m_rootItem);
    m_rootItem->SetExpanded(true);
}

void CFileDupeControl::OnSetFocus(CWnd* pOldWnd)
{
    CTreeListControl::OnSetFocus(pOldWnd);
    CMainFrame::Get()->SetLogicalFocus(LF_DUPELIST);
}

void CFileDupeControl::OnKeyDown(const UINT nChar, const UINT nRepCnt, const UINT nFlags)
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
