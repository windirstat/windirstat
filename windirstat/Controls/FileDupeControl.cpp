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
#include "MessageBoxDlg.h"

CFileDupeControl::CFileDupeControl() : CTreeListControl(COptions::DupeViewColumnOrder.Ptr(), COptions::DupeViewColumnWidths.Ptr())
{
    m_Singleton = this;
}

bool CFileDupeControl::GetAscendingDefault(const int column)
{
    return column == COL_ITEMDUP_SIZE_PHYSICAL ||
        column == COL_ITEMDUP_SIZE_LOGICAL ||
        column == COL_ITEMDUP_LAST_CHANGE;
}

BEGIN_MESSAGE_MAP(CFileDupeControl, CTreeListControl)
    ON_WM_SETFOCUS()
    ON_WM_KEYDOWN()
    ON_NOTIFY_REFLECT_EX(LVN_DELETEALLITEMS, OnDeleteAllItems)
END_MESSAGE_MAP()

CFileDupeControl* CFileDupeControl::m_Singleton = nullptr;

void CFileDupeControl::ProcessDuplicate(CItem * item, BlockingQueue<CItem*>* queue)
{
    if (!COptions::ScanForDuplicates) return;
    if (COptions::SkipDupeDetectionCloudLinks && item->IsTypeOrFlag(ITRP_CLOUD))
    {
        std::unique_lock lock(m_HashTrackerMutex);
        CMessageBoxDlg dlg(Localization::Lookup(IDS_DUPLICATES_WARNING), Localization::LookupNeutral(AFX_IDS_APP_TITLE),
            MB_OK | MB_ICONINFORMATION, this, {}, Localization::Lookup(IDS_DONT_SHOW_AGAIN), false);
        if (m_ShowCloudWarningOnThisScan && dlg.DoModal() == IDOK && dlg.IsCheckboxChecked())
        {
            COptions::SkipDupeDetectionCloudLinksWarning = false;
        }
        m_ShowCloudWarningOnThisScan = false;
        return;
    }

    // Determine which hash applies to this object
    auto& m_HashTracker = item->GetSizeLogical() <= m_PartialBufferSize
        ? m_HashTrackerSmall : m_HashTrackerLarge;

    // Add to size tracker and exit early if first item
    std::unique_lock lock(m_HashTrackerMutex);
    auto & sizeSet = m_SizeTracker[item->GetSizeLogical()];
    sizeSet.emplace_back(item);
    if (sizeSet.size() < 2) return;
    
    std::vector<BYTE> hashForThisItem;
    auto itemsToHash = std::vector(sizeSet);
    for (const ITEMTYPE & hashType : {ITHASH_PART, ITHASH_FULL })
    {
        // Attempt to hash the file partially
        for (auto& itemToHash : itemsToHash)
        {
            if (itemToHash->IsTypeOrFlag(ITHASH_SKIP)) continue;

            // Compute the hash for the file
            lock.unlock();
            auto hash = itemToHash->GetFileHash(hashType == ITHASH_PART ? m_PartialBufferSize : 0, queue);
            lock.lock();

            // Skip if not hashable
            if (hash.empty())
            {
                itemToHash->SetHashType(ITHASH_SKIP);
                continue;
            }

            itemToHash->SetFlag(hashType);
            if (itemToHash == item) hashForThisItem = hash;

            // Mark as the full being completed as well
            if (itemToHash->GetSizeLogical() <= m_PartialBufferSize)
                itemToHash->SetHashType(ITHASH_FULL);

            // Add hash to tracking queue
            auto & hashVector = m_HashTracker[hash];
            if (std::ranges::find(hashVector, itemToHash) == hashVector.end())
                hashVector.emplace_back(itemToHash);
        }

        // Return if no hash conflicts
        const auto hashesResult = m_HashTracker.find(hashForThisItem) ;
        if (hashesResult == m_HashTracker.end() || hashesResult->second.size() < 2) return;
        itemsToHash = hashesResult->second;
    }

    // Add the hashes to the UI thread
    if (hashForThisItem.empty() || itemsToHash.empty()) return;
    m_HashTrackerMutex.unlock();
    for (std::scoped_lock guard(m_NodeTrackerMutex); const auto& itemToAdd : itemsToHash)
    {
        const auto nodeEntry = m_NodeTracker.find(hashForThisItem);
        auto dupeParent = nodeEntry != m_NodeTracker.end() ? nodeEntry->second : nullptr;

        if (dupeParent == nullptr)
        {
            // Create new root item to hold these duplicates
            dupeParent = new CItemDupe(hashForThisItem);
            m_PendingListAdds.push(std::make_pair(nullptr, dupeParent));
            m_NodeTracker.emplace(hashForThisItem, dupeParent);
        }

        // Add new item
        auto& m_HashParentNode = m_ChildTracker[dupeParent];
        if (m_HashParentNode.contains(itemToAdd)) continue;
        const auto dupeChild = new CItemDupe(itemToAdd);
        m_PendingListAdds.push(std::make_pair(dupeParent, dupeChild));
        m_HashParentNode.emplace(itemToAdd);
    }
    m_HashTrackerMutex.lock();
}

void CFileDupeControl::SortItems()
{
    ASSERT(AfxGetThread() != nullptr);

    // Add items to the list
    if (m_PendingListAdds.empty()) return;
    const auto root = reinterpret_cast<CItemDupe*>(GetItem(0));
    SetRedraw(FALSE);
    std::pair<CItemDupe*, CItemDupe*> pair;
    while (m_PendingListAdds.pop(pair))
    {
        const auto& [parent, child] = pair;
        (parent == nullptr ? root : parent)->AddDupeItemChild(child);
    }
    SetRedraw(TRUE);

#ifdef _DEBUG
    for (const auto& hashParent : root->GetChildren())
    {
        const auto& hashString = hashParent->GetHashAndExtensions().substr(0, 8);
        if (hashParent->GetChildren().size() < 2)
        {
            VTRACE(L"Debug Dupe Tree Entry < 2 Nodes: {}", hashString);
            continue;
        }

        const auto sizeCheck = reinterpret_cast<CItem*>(hashParent->GetChildren()[0]->GetLinkedItem())->GetSizeLogical();
        for (const auto& hashItem : hashParent->GetChildren())
        {
            const auto sizeCompare = reinterpret_cast<CItem*>(hashItem->GetLinkedItem())->GetSizeLogical();
            if (sizeCheck != sizeCompare)
            {
                VTRACE(L"Debug Dupe Tree: Hash {} Sizes: {} != {}", hashString, sizeCheck, sizeCompare);
            }
        }
    }
#endif

    CSortingListControl::SortItems();
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
            std::erase(m_SizeTracker.at(qitem->GetSizeLogical()), qitem);
            qitem->SetHashType(ITHASH_NONE);
        }
        else if (!qitem->IsLeaf()) for (const auto& child : qitem->GetChildren())
        {
            queue.push(child);
        }
    }
    std::erase_if(m_SizeTracker, [](const auto& pair)
    {
        return pair.second.empty();
    });

    // Remove all unhashed files from hash trackers
    for (auto& hashTracker : { std::ref(m_HashTrackerSmall), std::ref(m_HashTrackerLarge) })
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
    const auto root = reinterpret_cast<CItemDupe*>(GetItem(0));
    bool erasedNode = false;
    for (auto nodeIter = m_NodeTracker.begin(); nodeIter != m_NodeTracker.end();
        erasedNode ? nodeIter : ++nodeIter, erasedNode = false)
    {
        auto& [dupeParentKey, dupeParent] = *nodeIter;

        // Remove from child tracker
        bool erasedChild = false;
        auto& childItems = m_ChildTracker[dupeParent];
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
            root->RemoveDupeItemChild(dupeParent);
            nodeIter = m_NodeTracker.erase(nodeIter);
            erasedNode = true;
        }
    }
    std::erase_if(m_ChildTracker, [](const auto& pair)
    {
        return pair.second.size() <= 1;
    });

    // Resume redrawing and invalidate to force refresh
    SetRedraw(TRUE);
    Invalidate();
}

void CFileDupeControl::OnItemDoubleClick(const int i)
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

BOOL CFileDupeControl::OnDeleteAllItems(NMHDR*, LRESULT* pResult)
{
    // Reset duplicate warning
    m_ShowCloudWarningOnThisScan = COptions::SkipDupeDetectionCloudLinksWarning;

    // Cleanup support lists
    m_PendingListAdds.clear();
    m_NodeTracker.clear();
    m_HashTrackerSmall.clear();
    m_HashTrackerLarge.clear();
    m_SizeTracker.clear();
    m_ChildTracker.clear();

    // Allow deletion to proceed
    *pResult = FALSE;
    return FALSE;
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
