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

#include "stdafx.h"
#include "WinDirStat.h"
#include "DirStatDoc.h"
#include "ItemTop.h"
#include "MainFrame.h"
#include "FileTopControl.h"
#include "Localization.h"

CFileTopControl::CFileTopControl() : CTreeListControl(20, COptions::TopViewColumnOrder.Ptr(), COptions::TopViewColumnWidths.Ptr())
{
    m_Singleton = this;
}

bool CFileTopControl::GetAscendingDefault(const int column)
{
    return column == COL_ITEMTOP_SIZE_PHYSICAL ||
        column == COL_ITEMTOP_SIZE_LOGICAL ||
        column == COL_ITEMTOP_LAST_CHANGE;
}

BEGIN_MESSAGE_MAP(CFileTopControl, CTreeListControl)
    ON_WM_SETFOCUS()
    ON_WM_KEYDOWN()
    ON_NOTIFY_REFLECT_EX(LVN_DELETEALLITEMS, OnDeleteAllItems)
END_MESSAGE_MAP()

CFileTopControl* CFileTopControl::m_Singleton = nullptr;

void CFileTopControl::ProcessTop(CItem * item)
{
    // Do not process if we are not tracking large files
    if (COptions::LargeFileCount == 0) return;

    std::scoped_lock guard(m_SizeMutex);
    m_QueuedSet.emplace_back(item);
}

void CFileTopControl::SortItems()
{
    ASSERT(AfxGetThread() != nullptr);

    // Verify at least root exists
    if (GetItemCount() == 0) return;

    // Quickly copy the items to a vector to free mutex
    m_SizeMutex.lock();
    std::vector<CItem*> queuedItems = m_QueuedSet;
    m_QueuedSet.clear();
    m_SizeMutex.unlock();

    // Insert into map
    m_SizeMap.insert(queuedItems.begin(), queuedItems.end());

    SetRedraw(FALSE);
    const auto root = reinterpret_cast<CItemTop*>(GetItem(0));
    auto itemTrackerCopy = std::unordered_map(m_ItemTracker);
    for (const auto& largeItem : m_SizeMap | std::views::take(COptions::LargeFileCount.Obj()))
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
        else if (!qitem->IsLeaf()) for (const auto& child : qitem->GetChildren())
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

BOOL CFileTopControl::OnDeleteAllItems(NMHDR*, LRESULT* pResult)
{
    // Reset trackers
    m_SizeMap.clear();
    m_ItemTracker.clear();

    // Allow deletion to proceed
    *pResult = FALSE;
    return FALSE;
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
        CMainFrame::Get()->MoveFocus(LF_EXTLIST);
    }
    else if (nChar == VK_ESCAPE)
    {
        CMainFrame::Get()->MoveFocus(LF_NONE);
    }
    CTreeListControl::OnKeyDown(nChar, nRepCnt, nFlags);
}
