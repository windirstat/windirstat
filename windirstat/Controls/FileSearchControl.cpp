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
#include "FileTreeView.h"

CFileSearchControl::CFileSearchControl() : CTreeListControl(COptions::SearchViewColumnOrder.Ptr(), COptions::SearchViewColumnWidths.Ptr(), LF_SEARCHLIST, false)
{
    m_singleton = this;
}

bool CFileSearchControl::GetAscendingDefault(const int column)
{
    return column == COL_ITEMSEARCH_NAME || column == COL_ITEMSEARCH_LAST_CHANGE;
}

BEGIN_MESSAGE_MAP(CFileSearchControl, CTreeListControl)
END_MESSAGE_MAP()

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
    const std::wstring & searchTerm, const bool searchCase,
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
        const auto searchTermRegex = ComputeSearchRegex(searchTerm,
            searchCase, searchRegex);

        // Do search
        std::vector<CItem*> queue{ item };
        while (!queue.empty() && !pdlg->IsCancelled())
        {
            // Grab item from queue
            pdlg->Increment();
            CItem* qitem = queue.back();
            queue.pop_back();

            // Check for match
            if (!onlyFiles || qitem->IsTypeOrFlag(IT_FILE))
            {
                const auto nameView = qitem->GetNameView();
                const bool isMatch = searchWholePhrase ?
                    std::regex_match(nameView.begin(), nameView.end(), searchTermRegex) :
                    std::regex_search(nameView.begin(), nameView.end(), searchTermRegex);

                if (isMatch)
                {
                    matchedItems.push_back(qitem);
                }
            }

            // Descend into child items
            if (qitem->IsLeaf() || qitem->IsTypeOrFlag(IT_HLINKS)) continue;
            for (const auto& child : qitem->GetChildren())
            {
                queue.push_back(child);
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

    // Add to found items
    const CSetRedrawLock lock(this);
    m_itemTracker.reserve(matchedItems.size());
    for (CItem* matchedItem : matchedItems)
    {
        auto searchItem = new CItemSearch(matchedItem);
        m_itemTracker.emplace(matchedItem, searchItem);
        m_rootItem->AddSearchItemChild(searchItem);
    }

    SortItems();
    ExpandItem(0);
}

void CFileSearchControl::RemoveItem(CItem* item)
{
    const CSetRedrawLock lock(this);
    std::erase_if(m_itemTracker, [&](const auto& pair)
    {
        if (pair.first != item && !item->IsAncestorOf(pair.first)) return false;
        m_rootItem->RemoveSearchItemChild(pair.second);
        return true;
    });
}

void CFileSearchControl::AfterDeleteAllItems()
{
    // Delete previous search results
    m_itemTracker.clear();

    // Delete and recreate root item
    delete m_rootItem;
    m_rootItem = new CItemSearch();
    InsertItem(0, m_rootItem);
    m_rootItem->SetExpanded(true);
}
