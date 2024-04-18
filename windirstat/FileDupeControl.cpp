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

#include <execution>
#include <ranges>

#include "WinDirStat.h"
#include "DirStatDoc.h"
#include "ItemDupe.h"
#include "MainFrame.h"
#include "FileDupeView.h"
#include "GlobalHelpers.h"
#include "OsSpecific.h"
#include "Localization.h"

CFileDupeControl::CFileDupeControl() : CTreeListControl(20, COptions::DupeTreeColumnOrder.Ptr(), COptions::DupeTreeColumnWidths.Ptr())
{
    m_singleton = this;
}

bool CFileDupeControl::GetAscendingDefault(int column)
{
    return column == COL_ITEMDUP_SIZE_PHYSICAL || column == COL_ITEMDUP_LASTCHANGE;
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

CFileDupeControl* CFileDupeControl::m_singleton = nullptr;

void CFileDupeControl::OnContextMenu(CWnd* /*pWnd*/, CPoint pt)
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
    menu.LoadMenu(IDR_POPUPLIST);
    Localization::UpdateMenu(menu);
    CMenu* sub = menu.GetSubMenu(0);

    PrepareDefaultMenu(sub, static_cast<CItemDupe*>(item));
    CMainFrame::Get()->AppendUserDefinedCleanups(sub);

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

void CFileDupeControl::ProcessDuplicate(CItem * item)
{
    if (!COptions::ScanForDuplicates) return;
    if (COptions::SkipDuplicationDetectionCloudLinks.Obj() &&
        CDirStatApp::Get()->GetReparseInfo()->IsCloudLink(item->GetPathLong(), item->GetAttributes())) return;

    std::lock_guard lock(m_Mutex);
    const auto size_entry = m_SizeTracker.find(item->GetSizePhysical());
    if (size_entry == m_SizeTracker.end())
    {
        // Add first entry to list
        const auto set = { item };
        m_SizeTracker.emplace(item->GetSizePhysical(), set);
        return;
    }

    // Add to the list of items to track
    size_entry->second.insert(item);

    std::wstring hash_for_this_item;
    auto items_to_hash = size_entry->second;
    for (const ITEMTYPE & hash_type : {ITF_PARTHASH, ITF_FULLHASH })
    {
        // Attempt to hash the file partially
        std::unordered_map<std::wstring, std::unordered_set<CItem*>> hashes_to_display;
        for (auto& item_to_hash : items_to_hash)
        {
            if (item_to_hash->IsType(hash_type)) continue;

            // Compute the hash for the file
            m_Mutex.unlock();
            std::wstring hash = item_to_hash->GetFileHash(hash_type == ITF_PARTHASH);
            m_Mutex.lock();
            item_to_hash->SetType(item_to_hash->GetRawType() | hash_type);
            if (item_to_hash == item) hash_for_this_item = hash;

            // Skip if not hashable
            if (hash.empty()) continue;

            // Mark as the full being completed as well
            if (item_to_hash->GetSizePhysical() <= 1024ull * 1024ull)
                item_to_hash->SetType(item_to_hash->GetRawType() | ITF_FULLHASH);

            // See if hash is already in tracking
            const auto hash_entry = m_HashTracker.find(hash);
            if (hash_entry != m_HashTracker.end()) hash_entry->second.insert(item_to_hash);
            else m_HashTracker.emplace(hash, std::initializer_list<CItem*> { item_to_hash });
        }

        // Return if no hash conflicts
        auto hashes_result = m_HashTracker.find(hash_for_this_item) ;
        if (hashes_result == m_HashTracker.end() || hashes_result->second.size() <= 1) return;
        items_to_hash = hashes_result->second;
    }

    for (const auto& item_to_add : items_to_hash)
    {
        CMainFrame::Get()->InvokeInMessageThread([&]
        {
            const auto root = reinterpret_cast<CItemDupe*>(GetItem(0));
            const auto node_entry = m_NodeTracker.find(hash_for_this_item);
            auto dupe_parent = node_entry != m_NodeTracker.end() ? node_entry->second : nullptr;

            if (dupe_parent == nullptr)
            {
                // Create new root item to hold these duplicates
                dupe_parent = new CItemDupe(hash_for_this_item.c_str(), item_to_add->GetSizePhysical());
                root->AddChild(dupe_parent);
                m_NodeTracker.emplace(hash_for_this_item.c_str(), dupe_parent);
            }

            // See if child is already in list parent
            const auto& children = dupe_parent->GetChildren();
            if (std::ranges::find_if(children, [item_to_add](const auto& child)
                { return child->GetItem() == item_to_add; }) != children.end()) return;

            // Add new item
            const auto dup_child = new CItemDupe(item_to_add);
            dupe_parent->AddChild(dup_child);

            SortItems();
        });
    }
}

void CFileDupeControl::OnItemDoubleClick(int i)
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

void CFileDupeControl::PrepareDefaultMenu(CMenu* menu, const CItemDupe* item)
{
    if (const CItem * ditem = item->GetItem(); ditem != nullptr && ditem->TmiIsLeaf())
    {
        menu->DeleteMenu(0, MF_BYPOSITION); // Remove "Expand/Collapse" item
        menu->DeleteMenu(0, MF_BYPOSITION); // Remove separator
        menu->SetDefaultItem(ID_CLEANUP_OPEN_SELECTED, false);
    }
    else
    {
        const CStringW command = item->IsExpanded() && item->HasChildren() ? Localization::Lookup(IDS_COLLAPSE) : Localization::Lookup(IDS_EXPAND);
        VERIFY(menu->ModifyMenu(ID_POPUP_TOGGLE, MF_BYCOMMAND | MF_STRING, ID_POPUP_TOGGLE, command));
        menu->SetDefaultItem(ID_POPUP_TOGGLE, false);
    }
}

void CFileDupeControl::OnLvnItemchangingList(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

    // determine if a new selection is being made
    const bool requesting_selection =
        (pNMLV->uOldState & LVIS_SELECTED) == 0 &&
        (pNMLV->uNewState & LVIS_SELECTED) != 0;

    if (requesting_selection && reinterpret_cast<CItemDupe*>(GetItem(pNMLV->iItem))->GetItem() == nullptr)
    {
        *pResult = TRUE;
        return;
    }

    return CTreeListControl::OnLvnItemchangingList(pNMHDR, pResult);
}

void CFileDupeControl::InsertItem(int i, CTreeListItem* item)
{
    InsertListItem(i, item);
    item->SetVisible(this, true);
}

void CFileDupeControl::SetRootItem(CTreeListItem* root)
{
    m_NodeTracker.clear();
    m_HashTracker.clear();
    m_SizeTracker.clear();

    CTreeListControl::SetRootItem(root);
}

void CFileDupeControl::OnSetFocus(CWnd* pOldWnd)
{
    CTreeListControl::OnSetFocus(pOldWnd);
    CMainFrame::Get()->SetLogicalFocus(LF_DUPLICATELIST);
}

void CFileDupeControl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
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
