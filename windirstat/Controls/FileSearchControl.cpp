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
#include "ItemSearch.h"
#include "FileSearchControl.h"
#include "ProgressDlg.h"

CFileSearchControl::CFileSearchControl() : CTreeListControl(COptions::SearchViewColumnOrder.Ptr(), COptions::SearchViewColumnWidths.Ptr())
{
    m_Singleton = this;
}

bool CFileSearchControl::GetAscendingDefault(const int column)
{
    return column == COL_ITEMSEARCH_SIZE_PHYSICAL ||
        column == COL_ITEMSEARCH_SIZE_LOGICAL ||
        column == COL_ITEMSEARCH_LAST_CHANGE;
}

BEGIN_MESSAGE_MAP(CFileSearchControl, CTreeListControl)
    ON_WM_SETFOCUS()
    ON_WM_KEYDOWN()
    ON_NOTIFY_REFLECT_EX(LVN_DELETEALLITEMS, OnDeleteAllItems)
END_MESSAGE_MAP()

CFileSearchControl* CFileSearchControl::m_Singleton = nullptr;

std::wregex CFileSearchControl::ComputeSearchRegex(const std::wstring & searchTerm, const bool searchCase, const bool useRegex)
{
    try
    {
        // Validate input is valid
        if (searchTerm.empty()) return {};

        // Decode regex flags based on settings
        auto searchFlags = std::regex_constants::optimize;
        if (!searchCase) searchFlags |= std::regex_constants::icase;

        // Precompile regex string
        return std::wregex(useRegex ?
            searchTerm : GlobToRegex(searchTerm, false), searchFlags);
    }
    catch (...)
    {
        return {};
    }
}

void CFileSearchControl::ProcessSearch(CItem* item)
{
    // Remove previous results
    SetRedraw(FALSE);
    CDirStatDoc::Get()->GetRootItemSearch()->RemoveSearchItemResults();
    m_ItemTracker.clear();
    SetRedraw(TRUE);

    // Precompile regex string
    const auto searchRegex = ComputeSearchRegex(COptions::SearchTerm,
        COptions::SearchCase, COptions::SearchRegex);

    // Determine which search function
    const std::function searchFunc = COptions::SearchWholePhrase ?
        [](const std::wstring& str, const std::wregex& regex) { return std::regex_match(str, regex); } :
        [](const std::wstring& str, const std::wregex& regex) { return std::regex_search(str, regex); };

    // Process search request using progress dialog
    CProgressDlg(static_cast<size_t>(item->GetItemsCount()), false, AfxGetMainWnd(),
        [&](const std::atomic<bool>& cancel, std::atomic<size_t>& current)
    {
        // Do search
        std::stack<CItem*> queue({ item });
        while (!queue.empty() && !cancel)
        {
            // Grab item from queue
            ++current;
            CItem* qitem = queue.top();
            queue.pop();

            if (searchFunc(qitem->GetName(), searchRegex))
            {
                CItemSearch* searchItem = new CItemSearch(qitem);
                CMainFrame::Get()->InvokeInMessageThread([this, searchItem]
                {
                    CDirStatDoc::Get()->GetRootItemSearch()->AddSearchItemChild(searchItem);
                });
                m_ItemTracker.emplace(qitem, searchItem);
            }

            // Descend into child items
            if (qitem->IsLeaf()) continue;
            for (const auto& child : qitem->GetChildren())
            {
                queue.push(child);
            }
        }
    }).DoModal();

    // Reenable drawing
    SortItems();

    // Update tab visibility to show search tab if results exist
    CMainFrame::Get()->GetFileTabbedView()->SetSearchTabVisibility(true);
}

void CFileSearchControl::RemoveItem(CItem* item)
{
    const auto& findItem = m_ItemTracker.find(item);
    if (findItem == m_ItemTracker.end()) return;

    // Remove the item from the interface
    CDirStatDoc::Get()->GetRootItemSearch()->RemoveSearchItemChild(findItem->second);
    m_ItemTracker.erase(findItem);
}

void CFileSearchControl::OnItemDoubleClick(const int i)
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

BOOL CFileSearchControl::OnDeleteAllItems(NMHDR*, LRESULT* pResult)
{
    // Allow deletion to proceed
    *pResult = FALSE;
    return FALSE;
}

void CFileSearchControl::OnSetFocus(CWnd* pOldWnd)
{
    CTreeListControl::OnSetFocus(pOldWnd);
    CMainFrame::Get()->SetLogicalFocus(LF_SEARCHLIST);
}

void CFileSearchControl::OnKeyDown(const UINT nChar, const UINT nRepCnt, const UINT nFlags)
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
