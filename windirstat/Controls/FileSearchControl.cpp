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
    m_singleton = this;
}

bool CFileSearchControl::GetAscendingDefault(const int column)
{
    return column == COL_ITEMDUP_NAME || column == COL_ITEMDUP_LAST_CHANGE;
}

BEGIN_MESSAGE_MAP(CFileSearchControl, CTreeListControl)
    ON_WM_SETFOCUS()
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()

CFileSearchControl* CFileSearchControl::m_singleton = nullptr;

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

void CFileSearchControl::ProcessSearch(CItem* item,
    const std::wstring & searchTeam, const bool searchCase,
    const bool searchWholePhrase, const bool searchRegex, const bool onlyFiles)
{
    // Update tab visibility to show search tab if results exist
    CMainFrame::Get()->GetFileTabbedView()->SetSearchTabVisibility(true);

    // Process search request using progress dialog
    std::vector<CItem*> matchedItems;
    CProgressDlg(static_cast<size_t>(item->GetItemsCount()), false, AfxGetMainWnd(),
        [&](CProgressDlg* pdlg)
    {
        // Remove previous results
        SetRootItem();
        m_rootItem->SetLimitExceeded(false);

        // Precompile regex string
        const auto searchTermRegex = ComputeSearchRegex(searchTeam,
            searchCase, searchRegex);

        // Determine which search function
        const std::function searchFunc = searchWholePhrase ?
            [](const std::wstring& str, const std::wregex& regex) { return std::regex_match(str, regex); } :
            [](const std::wstring& str, const std::wregex& regex) { return std::regex_search(str, regex); };
  
        // Do search
        std::stack<CItem*> queue({ item });
        while (!queue.empty() && !pdlg->IsCancelled())
        {
            // Grab item from queue
            pdlg->Increment();
            CItem* qitem = queue.top();
            queue.pop();

            // Check for match
            if ((!onlyFiles || qitem->IsTypeOrFlag(IT_FILE)) &&
                searchFunc(std::wstring(qitem->GetNameView()), searchTermRegex))
            {
                matchedItems.push_back(qitem);
            }

            // Descend into child items
            if (qitem->IsLeaf() || qitem->IsTypeOrFlag(IT_HLINKS)) continue;
            for (const auto& child : qitem->GetChildren())
            {
                queue.push(child);
            }
        }

        // Sort by physical size (largest first) and take top N results
        const size_t maxResults = COptions::SearchMaxResults;
        if (matchedItems.size() > maxResults)
        {
            // Partial sort to get the top N items by physical size
            std::ranges::partial_sort(matchedItems, matchedItems.begin() + maxResults,
                std::ranges::greater{}, &CItem::GetSizeLogical);
            
            // Keep only the top N results
            matchedItems.resize(maxResults);
            m_rootItem->SetLimitExceeded(true);
        }
    }).DoModal();

    // Add found items to the interface
    CWaitCursor wait;
    CollapseItem(0);
    SetRedraw(FALSE);

    // Add to found items
    m_itemTracker.reserve(matchedItems.size());
    for (CItem* matchedItem : matchedItems)
    {
        auto searchItem = new CItemSearch(matchedItem);
        m_itemTracker.emplace(matchedItem, searchItem);
        m_rootItem->AddSearchItemChild(searchItem);
    }

    SetRedraw(TRUE);
    SortItems();
    ExpandItem(0);
}

void CFileSearchControl::RemoveItem(CItem* item)
{
    const auto& findItem = m_itemTracker.find(item);
    if (findItem == m_itemTracker.end()) return;

    // Remove the item from the interface
    SetRedraw(FALSE);
    m_rootItem->RemoveSearchItemChild(findItem->second);
    m_itemTracker.erase(findItem);
    SetRedraw(TRUE);
}

void CFileSearchControl::OnItemDoubleClick(const int i)
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

void CFileSearchControl::AfterDeleteAllItems()
{
    // Delete previous search results
    m_itemTracker.clear();

    // Delete and recreate root item
    if (m_rootItem != nullptr) delete m_rootItem;
    m_rootItem = new CItemSearch();
    InsertItem(0, m_rootItem);
    m_rootItem->SetExpanded(true);
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
