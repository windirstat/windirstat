// FileDupeControl.cpp - Implementation of FileDupeControl
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "stdafx.h"

#include "WinDirStat.h"
#include "DirStatDoc.h"
#include "ItemDupe.h"
#include "MainFrame.h"
#include "FileDupeView.h"
#include "Localization.h"

#include <execution>
#include <unordered_map>
#include <ranges>
#include <stack>

CFileDupeControl::CFileDupeControl() : CTreeListControl(20, COptions::DupeViewColumnOrder.Ptr(), COptions::DupeViewColumnWidths.Ptr())
{
    m_Singleton = this;
}

bool CFileDupeControl::GetAscendingDefault(const int column)
{
    return column == COL_ITEMDUP_SIZE_PHYSICAL ||
        column == COL_ITEMDUP_SIZE_LOGICAL ||
        column == COL_ITEMDUP_LASTCHANGE;
}

#pragma warning(push)
#pragma warning(disable:26454)
BEGIN_MESSAGE_MAP(CFileDupeControl, CTreeListControl)
    ON_WM_SETFOCUS()
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()
#pragma warning(pop)

CFileDupeControl* CFileDupeControl::m_Singleton = nullptr;

void CFileDupeControl::ProcessDuplicate(CItem * item, BlockingQueue<CItem*>* queue)
{
    if (!COptions::ScanForDuplicates) return;
    if (COptions::SkipDupeDetectionCloudLinks &&
        CReparsePoints::IsCloudLink(item->GetPathLong(), item->GetAttributes()))
    {
        std::unique_lock lock(m_HashTrackerMutex);
        if (m_ShowCloudWarningOnThisScan &&
            AfxMessageBox(Localization::Lookup(IDS_DUPLICATES_WARNING).c_str(), MB_YESNO) == IDNO)
        {
            COptions::SkipDupeDetectionCloudLinksWarning = false;
        }
        m_ShowCloudWarningOnThisScan = false;
        return;
    }

    std::unique_lock lock(m_HashTrackerMutex);
    const auto sizeEntry = m_SizeTracker.find(item->GetSizeLogical());
    if (sizeEntry == m_SizeTracker.end())
    {
        // Add first entry to list
        const auto set = { item };
        m_SizeTracker.emplace(item->GetSizeLogical(), set);
        return;
    }

    // Add to the list of items to track
    sizeEntry->second.insert(item);

    std::wstring hashForThisItem;
    auto itemsToHash = sizeEntry->second;
    for (const ITEMTYPE & hashType : {ITF_PARTHASH, ITF_FULLHASH })
    {
        // Attempt to hash the file partially
        std::unordered_map<std::wstring, std::unordered_set<CItem*>> hashesToDisplay;
        for (auto& itemToHash : itemsToHash)
        {
            if (itemToHash->IsType(hashType)) continue;
            constexpr auto partialBufferSize = 128ull * 1024ull;

            // Compute the hash for the file
            lock.unlock();
            std::wstring hash = itemToHash->GetFileHash(hashType == ITF_PARTHASH ? partialBufferSize : 0, queue);
            lock.lock();

            itemToHash->SetType(itemToHash->GetRawType() | hashType);
            if (itemToHash == item) hashForThisItem = hash;

            // Skip if not hashable
            if (hash.empty()) continue;

            // Mark as the full being completed as well
            if (itemToHash->GetSizeLogical() <= partialBufferSize)
                itemToHash->SetType(itemToHash->GetRawType() | ITF_FULLHASH);

            // See if hash is already in tracking
            const auto hashEntry = m_HashTracker.find(hash);
            if (hashEntry != m_HashTracker.end()) hashEntry->second.insert(itemToHash);
            else m_HashTracker.emplace(hash, std::initializer_list<CItem*> { itemToHash });
        }

        // Return if no hash conflicts
        const auto hashesResult = m_HashTracker.find(hashForThisItem) ;
        if (hashesResult == m_HashTracker.end() || hashesResult->second.size() <= 1) return;
        itemsToHash = hashesResult->second;
    }

    // Add the hashes to the UI thread 
    m_HashTrackerMutex.unlock();
    for (std::lock_guard guard(m_NodeTrackerMutex); const auto& itemToAdd : itemsToHash)
    {
        const auto nodeEntry = m_NodeTracker.find(hashForThisItem);
        auto dupeParent = nodeEntry != m_NodeTracker.end() ? nodeEntry->second : nullptr;

        if (dupeParent == nullptr)
        {
            // Create new root item to hold these duplicates
            dupeParent = new CItemDupe(hashForThisItem, itemToAdd->GetSizePhysical(), itemToAdd->GetSizeLogical());
            m_PendingListAdds.emplace(nullptr, dupeParent);
            m_NodeTracker.emplace(hashForThisItem, dupeParent);
        }

        // Add new item
        auto& m_HashParentNode = m_ChildTracker[dupeParent];
        if (m_HashParentNode.contains(itemToAdd)) continue;
        const auto dupeChild = new CItemDupe(itemToAdd);
        m_PendingListAdds.emplace(dupeParent, dupeChild);
        m_HashParentNode.emplace(itemToAdd);
    }
    m_HashTrackerMutex.lock();
}

void CFileDupeControl::SortItems()
{
    ASSERT(AfxGetThread() != nullptr);

    // Transfer elements to vector so we do not have to hold the lock 
    m_NodeTrackerMutex.lock();
    std::vector<std::pair<CItemDupe*, CItemDupe*>> pendingAdds;
    while (!m_PendingListAdds.empty())
    {
        pendingAdds.emplace_back(m_PendingListAdds.front());
        m_PendingListAdds.pop();
    }
    m_NodeTrackerMutex.unlock();

    // Add items to the list
    if (!pendingAdds.empty())
    {
        SetRedraw(FALSE);
        const auto root = reinterpret_cast<CItemDupe*>(GetItem(0));
        for (const auto& [parent, child] : pendingAdds)
            (parent == nullptr ? root : parent)->AddDupeItemChild(child);
        SetRedraw(TRUE);
    }

    CSortingListControl::SortItems();
}

void CFileDupeControl::RemoveItem(CItem* item)
{
    // Exit immediately if not doing duplicate detector
    if (m_HashTracker.empty() && m_SizeTracker.empty()) return;

    std::stack<CItem*> queue({ item });
    std::vector<CItem*> itemsToRemove;
    while (!queue.empty())
    {
        const auto& qitem = queue.top();
        queue.pop();
        if (qitem->IsType(IT_FILE)) itemsToRemove.emplace_back(qitem);
        else for (const auto& child : qitem->GetChildren())
        {
            queue.push(child);
        }
    }

    const auto root = reinterpret_cast<CItemDupe*>(GetItem(0));
    for (const auto& itemToRemove : std::ranges::reverse_view(itemsToRemove))
    {
        // Clear our hash bits
        item->SetType(ITF_PARTHASH | ITF_FULLHASH, false);

        // Remove from size tracker
        const auto size = itemToRemove->GetSizeLogical();
        m_SizeTracker.at(size).erase(itemToRemove);

        // Remove from hash tracker
        for (auto& [hashKey, hashSet] : m_HashTracker)
        {
            // Skip if no matches of the item associated with this hash
            if (!hashSet.contains(itemToRemove)) continue;
        
            // Remove from this set
            hashSet.erase(itemToRemove);

            // Continue if this is not present in the node list
            if (!m_NodeTracker.contains(hashKey)) continue;

            // Remove the entry from the visual node list
            auto& hashNode = m_NodeTracker.at(hashKey);
            for (const auto& dupeChild : std::vector(hashNode->GetChildren()))
            {
                if (dupeChild->GetLinkedItem() == itemToRemove)
                {
                    m_ChildTracker[hashNode].erase(itemToRemove);
                    hashNode->RemoveDupeItemChild(dupeChild);
                }
            }

            // Remove parent node if only one item is list
            if (hashNode->GetChildren().size() <= 1)
            {
                m_ChildTracker.erase(hashNode);
                root->RemoveDupeItemChild(hashNode);
                m_NodeTracker.erase(hashKey);
            }
        }
    }

    // Cleanup empty structures
    std::erase_if(m_HashTracker, [&](const auto& pair)
    {
        return pair.second.empty();
    });
    std::erase_if(m_SizeTracker, [&](const auto& pair)
    {
        return pair.second.empty();
    });
    std::erase_if(m_ChildTracker, [&](const auto& pair)
    {
        return pair.second.empty();
    });
}

void CFileDupeControl::OnItemDoubleClick(const int i)
{
    if (const auto item = reinterpret_cast<const CItem*>(GetItem(i)->GetLinkedItem());
        item != nullptr && item->IsType(IT_FILE))
    {
        CDirStatDoc::OpenItem(item);
    }
    else
    {
        CTreeListControl::OnItemDoubleClick(i);
    }
}

void CFileDupeControl::SetRootItem(CTreeListItem* root)
{
    m_ShowCloudWarningOnThisScan = COptions::SkipDupeDetectionCloudLinksWarning;

    // Cleanup node allocations
    for (const auto& item : m_NodeTracker | std::views::values)
    {
        delete item;
    }

    // Clear out any pending visual updates
    while (!m_PendingListAdds.empty())
        m_PendingListAdds.pop();

    m_NodeTracker.clear();
    m_HashTracker.clear();
    m_SizeTracker.clear();

    CTreeListControl::SetRootItem(root);
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
        CMainFrame::Get()->MoveFocus(LF_EXTENSIONLIST);
    }
    else if (nChar == VK_ESCAPE)
    {
        CMainFrame::Get()->MoveFocus(LF_NONE);
    }
    CTreeListControl::OnKeyDown(nChar, nRepCnt, nFlags);
}
