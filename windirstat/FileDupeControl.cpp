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
    ON_NOTIFY_REFLECT(LVN_ITEMCHANGING, OnLvnItemchangingList)
    ON_WM_CONTEXTMENU()
    ON_WM_SETFOCUS()
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()
#pragma warning(pop)

CFileDupeControl* CFileDupeControl::m_Singleton = nullptr;

void CFileDupeControl::OnContextMenu(CWnd* /*pWnd*/, const CPoint pt)
{
    const int i = GetSelectionMark();
    if (i == -1)
    {
        return;
    }

    CTreeListItem* item = GetItem(i);
    if (item->GetParent() == nullptr) return;

    CRect rc = GetWholeSubitemRect(i, 0);
    const CRect rcTitle = item->GetTitleRect() + rc.TopLeft();

    CMenu menu;
    menu.LoadMenu(IDR_POPUP_TREE);
    Localization::UpdateMenu(menu);
    CMenu* sub = menu.GetSubMenu(0);

    PrepareDefaultMenu(sub, static_cast<CItemDupe*>(item));
    CMainFrame::Get()->UpdateDynamicMenuItems(sub);

    // Show popup menu and act accordingly.
    //
    // The menu shall not overlap the label but appear
    // horizontally at the cursor position,
    // vertically under (or above) the label.
    // TrackPopupMenuEx() behaves in the desired way, if
    // we exclude the label rectangle extended to full screen width.

    TPMPARAMS tp;
    tp.cbSize = sizeof(tp);
    tp.rcExclude = rcTitle;
    ClientToScreen(&tp.rcExclude);

    CRect desktop;
    GetDesktopWindow()->GetWindowRect(desktop);

    tp.rcExclude.left = desktop.left;
    tp.rcExclude.right = desktop.right;

    constexpr int overlap = 2; // a little vertical overlapping
    tp.rcExclude.top += overlap;
    tp.rcExclude.bottom -= overlap;

    sub->TrackPopupMenuEx(TPM_LEFTALIGN | TPM_LEFTBUTTON, pt.x, pt.y, AfxGetMainWnd(), &tp);
}

void CFileDupeControl::ProcessDuplicate(CItem * item, BlockingQueue<CItem*>* queue)
{
    if (!COptions::ScanForDuplicates) return;
    if (COptions::SkipDupeDetectionCloudLinks &&
        CReparsePoints::IsCloudLink(item->GetPathLong(), item->GetAttributes()))
    {
        std::unique_lock lock(m_Mutex);
        if (m_ShowCloudWarningOnThisScan &&
            AfxMessageBox(Localization::Lookup(IDS_DUPLICATES_WARNING).c_str(), MB_YESNO) == IDNO)
        {
            COptions::SkipDupeDetectionCloudLinksWarning = false;
        }
        m_ShowCloudWarningOnThisScan = false;
        return;
    }

    std::unique_lock lock(m_Mutex);
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
    
    for (const auto& itemToAdd : itemsToHash)
    {
        CMainFrame::Get()->InvokeInMessageThread([&]
        {
            const auto root = reinterpret_cast<CItemDupe*>(GetItem(0));
            const auto nodeEntry = m_NodeTracker.find(hashForThisItem);
            auto dupeParent = nodeEntry != m_NodeTracker.end() ? nodeEntry->second : nullptr;

            if (dupeParent == nullptr)
            {
                // Create new root item to hold these duplicates
                dupeParent = new CItemDupe(hashForThisItem, itemToAdd->GetSizePhysical(), itemToAdd->GetSizeLogical());
                root->AddChild(dupeParent);
                m_NodeTracker.emplace(hashForThisItem, dupeParent);
            }

            // See if child is already in list parent
            const auto& children = dupeParent->GetChildren();
            if (std::ranges::find_if(children, [itemToAdd](const auto& child)
                { return child->GetItem() == itemToAdd; }) != children.end()) return;

            // Add new item
            const auto dupeChild = new CItemDupe(itemToAdd);
            dupeParent->AddChild(dupeChild);
        });
    }
}

void CFileDupeControl::RemoveItem(CItem* item)
{
    // Exit immediately if not doing duplicate detector
    if (m_HashTracker.empty() && m_SizeTracker.empty()) return;

    std::stack<CItem*> queue({ item });
    std::unordered_set<CItem*> itemsToRemove;
    while (!queue.empty())
    {
        const auto& qitem = queue.top();
        queue.pop();
        if (qitem->IsType(IT_FILE)) itemsToRemove.emplace(qitem);
        else for (const auto& child : qitem->GetChildren())
        {
            queue.push(child);
        }
    }

    const auto root = reinterpret_cast<CItemDupe*>(GetItem(0));
    for (const auto& itemToRemove : std::ranges::reverse_view(itemsToRemove))
    {
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
            for (auto& dupeChild : std::vector(hashNode->GetChildren()))
            {
                if (dupeChild->GetItem() == itemToRemove)
                {
                    hashNode->RemoveChild(dupeChild);
                }
            }

            // Remove parent node if only one item is list
            if (hashNode->GetChildren().size() <= 1)
            {
                root->RemoveChild(hashNode);
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
}

void CFileDupeControl::OnItemDoubleClick(const int i)
{
    if (const auto item = reinterpret_cast<const CItemDupe*>(GetItem(i))->GetItem();
        item != nullptr && item->IsType(IT_FILE))
    {
        CDirStatDoc::OpenItem(item);
    }
    else
    {
        CTreeListControl::OnItemDoubleClick(i);
    }
}

void CFileDupeControl::PrepareDefaultMenu(CMenu* menu, const CItemDupe* item) const
{
    if (const CItem * ditem = item->GetItem(); ditem != nullptr && ditem->TmiIsLeaf())
    {
        menu->DeleteMenu(0, MF_BYPOSITION); // Remove "Expand/Collapse" item
        menu->DeleteMenu(0, MF_BYPOSITION); // Remove separator
        menu->SetDefaultItem(ID_CLEANUP_OPEN_SELECTED, false);
    }
    else
    {
        const std::wstring command = item->IsExpanded() && item->HasChildren() ? Localization::Lookup(IDS_COLLAPSE) : Localization::Lookup(IDS_EXPAND);
        VERIFY(menu->ModifyMenu(ID_POPUP_TOGGLE, MF_BYCOMMAND | MF_STRING, ID_POPUP_TOGGLE, command.c_str()));
        menu->SetDefaultItem(ID_POPUP_TOGGLE, false);
    }
}

void CFileDupeControl::OnLvnItemchangingList(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

    // determine if a new selection is being made
    const bool requestingSelection =
        (pNMLV->uOldState & LVIS_SELECTED) == 0 &&
        (pNMLV->uNewState & LVIS_SELECTED) != 0;

    if (requestingSelection && reinterpret_cast<CItemDupe*>(GetItem(pNMLV->iItem))->GetItem() == nullptr)
    {
        *pResult = TRUE;
        return;
    }

    return CTreeListControl::OnLvnItemchangingList(pNMHDR, pResult);
}

void CFileDupeControl::InsertItem(const int i, CTreeListItem* item)
{
    InsertListItem(i, item);
    item->SetVisible(this, true);
}

void CFileDupeControl::SetRootItem(CTreeListItem* root)
{
    m_ShowCloudWarningOnThisScan = COptions::SkipDupeDetectionCloudLinksWarning;

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
