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
#include "FileDupeView.h"

CFileDupeControl::CFileDupeControl() : CTreeListControl(COptions::DupeViewColumnOrder.Ptr(), COptions::DupeViewColumnWidths.Ptr())
{
    m_singleton = this;
}

bool CFileDupeControl::GetAscendingDefault(const int column)
{
    return column == COL_ITEMDUP_NAME;
}

BEGIN_MESSAGE_MAP(CFileDupeControl, CTreeListControl)
    ON_WM_SETFOCUS()
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()

CFileDupeControl* CFileDupeControl::m_singleton = nullptr;

void CFileDupeControl::ProcessDuplicate(CItem * item, BlockingQueue<CItem*>* queue)
{
    if (!COptions::ScanForDuplicates) return;
    if (COptions::SkipDupeDetectionCloudLinks && item->IsTypeOrFlag(ITRP_CLOUD))
    {
        std::unique_lock lock(m_hashTrackerMutex);
        CMessageBoxDlg dlg(Localization::Lookup(IDS_DUPLICATES_WARNING), Localization::LookupNeutral(AFX_IDS_APP_TITLE),
            MB_OK | MB_ICONINFORMATION, this, {}, Localization::Lookup(IDS_DONT_SHOW_AGAIN), false);
        if (m_showCloudWarningOnThisScan && dlg.DoModal() == IDOK && dlg.IsCheckboxChecked())
        {
            COptions::SkipDupeDetectionCloudLinksWarning = false;
        }
        m_showCloudWarningOnThisScan = false;
        return;
    }

    // Determine which hash applies to this object
    auto& m_hashTracker = item->GetSizeLogical() <= m_partialBufferSize
        ? m_hashTrackerSmall : m_hashTrackerLarge;

    // Add to size tracker and exit early if first item
    std::unique_lock lock(m_hashTrackerMutex);
    auto & sizeSet = m_sizeTracker[item->GetSizeLogical()];
    sizeSet.emplace_back(item);
    if (sizeSet.size() < 2) return;

    std::vector<BYTE> hashForThisItem;
    auto itemsToHash = std::vector(sizeSet);
    for (const ITEMTYPE& hashType : { ITHASH_PART, ITHASH_FULL })
    {
        // Attempt to hash the file partially
        for (auto& itemToHash : itemsToHash)
        {
            if (itemToHash->IsTypeOrFlag(hashType, ITHASH_SKIP)) continue;

            // Compute the hash for the file
            lock.unlock();
            auto hash = itemToHash->GetFileHash(hashType == ITHASH_PART ? m_partialBufferSize : 0, queue);
            lock.lock();

            // Skip if not hashable
            if (hash.empty())
            {
                itemToHash->SetHashType(ITHASH_SKIP);
                continue;
            }

            itemToHash->SetHashType(hashType);
            if (itemToHash == item) hashForThisItem = hash;

            // Mark as the full being completed as well
            if (itemToHash->GetSizeLogical() <= m_partialBufferSize)
                itemToHash->SetHashType(ITHASH_FULL);

            // Add hash to tracking queue
            auto& hashVector = m_hashTracker[hash];
            if (std::ranges::find(hashVector, itemToHash) == hashVector.end())
                hashVector.emplace_back(itemToHash);
        }

        // Return if no hash conflicts
        const auto hashesResult = m_hashTracker.find(hashForThisItem);
        if (hashesResult == m_hashTracker.end() || hashesResult->second.size() < 2) return;
        itemsToHash = hashesResult->second;
    }

    // Add the hashes to the UI thread
    if (hashForThisItem.empty() || itemsToHash.empty()) return;
    m_hashTrackerMutex.unlock();
    for (std::scoped_lock guard(m_nodeTrackerMutex); const auto& itemToAdd : itemsToHash)
    {
        const auto nodeEntry = m_nodeTracker.find(hashForThisItem);
        auto dupeParent = nodeEntry != m_nodeTracker.end() ? nodeEntry->second : nullptr;

        if (dupeParent == nullptr)
        {
            // Create new root item to hold these duplicates
            dupeParent = new CItemDupe(hashForThisItem);
            m_pendingListAdds.push(std::make_pair(nullptr, dupeParent));
            m_nodeTracker.emplace(hashForThisItem, dupeParent);
        }

        // Add new item
        auto& m_hashParentNode = m_childTracker[dupeParent];
        if (m_hashParentNode.contains(itemToAdd)) continue;
        const auto dupeChild = new CItemDupe(itemToAdd);
        m_pendingListAdds.push(std::make_pair(dupeParent, dupeChild));
        m_hashParentNode.emplace(itemToAdd);
    }
    m_hashTrackerMutex.lock();
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

#ifdef _DEBUG
    for (const auto& hashParent : m_rootItem->GetChildren())
    {
        const auto& hashString = hashParent->GetHashAndExtensions().substr(0, 8);
        if (hashParent->GetChildren().size() < 2)
        {
            VTRACE(L"Debug Dupe Tree Entry < 2 Nodes: {}", hashString);
            continue;
        }

        const auto sizeCheck = hashParent->GetChildren()[0]->GetLinkedItem()->GetSizeLogical();
        for (const auto& hashItem : hashParent->GetChildren())
        {
            const auto sizeCompare = hashItem->GetLinkedItem()->GetSizeLogical();
            if (sizeCheck != sizeCompare)
            {
                VTRACE(L"Debug Dupe Tree: Hash {} Sizes: {} != {}", hashString, sizeCheck, sizeCompare);
            }
        }
    }
#endif

    CTreeListControl::SortItems();
}

void CFileDupeControl::RemoveItem(CItem* item)
{
    // Exit immediately if not doing duplicate detector
    if (!COptions::ScanForDuplicates) return;

    // Enumerate child items and mark all as unhashed
    std::stack<CItem*> queue({ item });
    while (!queue.empty())
    {
        const auto qitem = queue.top();
        queue.pop();
        if (qitem->IsTypeOrFlag(IT_FILE))
        {
            // Mark as all files as not being hashed anymore
            std::erase(m_sizeTracker.at(qitem->GetSizeLogical()), qitem);
            qitem->SetHashType(ITHASH_NONE, false);
        }
        else if (!qitem->IsLeaf()) for (const auto& child : qitem->GetChildren())
        {
            queue.push(child);
        }
    }
    std::erase_if(m_sizeTracker, [](const auto& pair)
    {
        return pair.second.empty();
    });

    // Remove all unhashed files from hash trackers
    for (auto& hashTracker : { std::ref(m_hashTrackerSmall), std::ref(m_hashTrackerLarge) })
    {
        for (auto& hashSet : hashTracker.get() | std::views::values)
        {
            // Skip if no matches of the item associated with this hash
            std::erase_if(hashSet, [](const auto& hashItem)
            {
                return !hashItem->IsTypeOrFlag(ITHASH_PART, ITHASH_FULL);
            });
        }

        // Cleanup empty structures
        std::erase_if(hashTracker.get(), [](const auto& pair)
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
            if ((*childItem)->IsTypeOrFlag(ITHASH_PART, ITHASH_FULL)) continue;

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
    m_showCloudWarningOnThisScan = COptions::SkipDupeDetectionCloudLinksWarning;

    // Cleanup support lists
    m_pendingListAdds.clear();
    m_nodeTracker.clear();
    m_hashTrackerSmall.clear();
    m_hashTrackerLarge.clear();
    m_sizeTracker.clear();
    m_childTracker.clear();

    // Delete and recreate root item
    if (m_rootItem != nullptr) delete m_rootItem;
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
