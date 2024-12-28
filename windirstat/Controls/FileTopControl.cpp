// FileTopControl.cpp - Implementation of FileTopControl
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
#include "ItemTop.h"
#include "MainFrame.h"
#include "FileTopControl.h"
#include "Localization.h"

#include <ranges>
#include <stack>

CFileTopControl::CFileTopControl() : CTreeListControl(20, COptions::TopViewColumnOrder.Ptr(), COptions::TopViewColumnWidths.Ptr())
{
    m_Singleton = this;
}

bool CFileTopControl::GetAscendingDefault(const int column)
{
    return column == COL_ITEMTOP_SIZE_PHYSICAL ||
        column == COL_ITEMTOP_SIZE_LOGICAL ||
        column == COL_ITEMTOP_LASTCHANGE;
}

#pragma warning(push)
#pragma warning(disable:26454)
BEGIN_MESSAGE_MAP(CFileTopControl, CTreeListControl)
    ON_WM_SETFOCUS()
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()
#pragma warning(pop)

CFileTopControl* CFileTopControl::m_Singleton = nullptr;

void CFileTopControl::ProcessTop(CItem * item)
{
    std::lock_guard guard(m_SizeMutex);
    m_SizeMap.emplace(item);
}

void CFileTopControl::SortItems()
{
    ASSERT(AfxGetThread() != nullptr);

    // Verify at least root exists
    if (GetItemCount() == 0) return;

    // Reverse iterate over the multimap
    m_SizeMutex.lock();
    std::unordered_set<CItem*> largestItems;
    for (auto & pair : m_SizeMap | std::views::reverse)
    {
        largestItems.insert(pair);
        if (static_cast<int>(largestItems.size()) >= COptions::LargeFileCount) break;
    }
    m_SizeMutex.unlock();

    SetRedraw(FALSE);
    const auto root = reinterpret_cast<CItemTop*>(GetItem(0));
    auto itemTrackerCopy = std::unordered_map(m_ItemTracker);
    for (auto& largeItem : largestItems)
    {
        if (m_ItemTracker.contains(largeItem))
        {
            itemTrackerCopy.erase(largeItem);
            continue;
        }
        
        const auto itemTop = new CItemTop(largeItem);
        root->AddTopItemChild(itemTop);
        m_ItemTracker[largeItem] = itemTop;
    }

    for (const auto& itemTop : itemTrackerCopy | std::views::values)
    {
        m_ItemTracker.erase(reinterpret_cast<CItem*>(itemTop->GetLinkedItem()));
        root->RemoveTopItemChild(itemTop);
    }
    SetRedraw(TRUE);

    CSortingListControl::SortItems();
}

void CFileTopControl::RemoveItem(CItem* item)
{
    std::stack<CItem*> queue({ item });
    while (!queue.empty())
    {
        const auto& qitem = queue.top();
        queue.pop();
        if (qitem->IsType(IT_FILE))
        {
            m_SizeMap.erase(qitem);
        }
        else for (const auto& child : qitem->GetChildren())
        {
            queue.push(child);
        }
    }

    // Use the sort function to remove visual items
    CMainFrame::Get()->InvokeInMessageThread([&]
    {
        SortItems();
    });
}

void CFileTopControl::OnItemDoubleClick(const int i)
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

void CFileTopControl::SetRootItem(CTreeListItem* root)
{
    // Cleanup node allocations
    for (const auto& item : m_ItemTracker| std::views::values)
    {
        delete item;
    }

    m_SizeMap.clear();
    m_ItemTracker.clear();

    CTreeListControl::SetRootItem(root);
}

void CFileTopControl::OnSetFocus(CWnd* pOldWnd)
{
    CTreeListControl::OnSetFocus(pOldWnd);
    CMainFrame::Get()->SetLogicalFocus(LF_TOPLIST);
}

void CFileTopControl::OnKeyDown(const UINT nChar, const UINT nRepCnt, const UINT nFlags)
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
