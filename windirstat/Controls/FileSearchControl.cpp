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

CFileSearchControl::CFileSearchControl() : CTreeListControl(COptions::SearchViewColumnOrder.Ptr(), COptions::SearchViewColumnWidths.Ptr(), COptions::SearchViewColumnVisibility.Ptr(), LF_SEARCHLIST, false)
{
    m_singleton = this;
}

bool CFileSearchControl::GetAscendingDefault(const int column)
{
    return column == COL_ITEMSEARCH_NAME || column == COL_ITEMSEARCH_LAST_CHANGE;
}

std::wregex CFileSearchControl::ComputeSearchRegex(const std::wstring & searchTerm, const bool searchCase, const bool useRegex)
{
    try
    {
        // Decode regex flags based on settings
        auto searchFlags = std::regex_constants::optimize;
        if (!searchCase) searchFlags |= std::regex_constants::icase;

        // Precompile regex string
        if (searchTerm.empty()) return std::wregex(L".*", searchFlags);
        return std::wregex(useRegex ?
            searchTerm : GlobToRegex(searchTerm, false), searchFlags);
    }
    catch (...)
    {
        return {};
    }
}

bool SearchCriteria::MatchesSize(const CItem* item) const
{
    return (!sizeMinimum || item->GetSizeLogical() >= *sizeMinimum) &&
        (!sizeMaximum || item->GetSizeLogical() <= *sizeMaximum) &&
        (!physicalMinimum || item->GetSizePhysical() >= *physicalMinimum) &&
        (!physicalMaximum || item->GetSizePhysical() <= *physicalMaximum);
}

void CFileSearchControl::ProcessSearch(CItem* item, const SearchCriteria& criteria)
{
    if (!CWinDirStatModel::Get()->IsScanSettled()) return;

    // Update tab visibility to show search tab if results exist
    CMainFrame::Get()->GetFileTabbedView()->SetSearchTabVisibility(true);

    // Remove previous results
    SetRootItem();
    m_sizeFilterActive = criteria.sizeMinimum.has_value() || criteria.sizeMaximum.has_value() ||
        criteria.physicalMinimum.has_value() || criteria.physicalMaximum.has_value();
    m_rootItem->SetLimitExceeded(false);

    // Process search request using progress dialog
    std::vector<CItem*> matchedItems;
    bool limitExceeded = false;
    std::optional<CItemSearch::SizeTotals> totals;
    const auto result = CProgressDlg(static_cast<size_t>(item->GetItemsCount()),
        CProgressDlg::Flags::None, GetMainWindow(),
        [&](CProgressDlg* pdlg)
    {
        // Precompile regex string
        const auto searchTermRegex = ComputeSearchRegex(criteria.term,
            criteria.caseSensitive, criteria.regex);

        // Do search
        const size_t maxResults = COptions::SearchMaxResults;
        const auto bySize = [](const CItem* lhs, const CItem* rhs)
        {
            return lhs->GetSizeLogical() > rhs->GetSizeLogical();
        };
        std::vector queue{ item };
        while (!queue.empty() && !pdlg->IsCancelled())
        {
            // Grab item from queue
            pdlg->Increment();
            CItem* qitem = queue.back();
            queue.pop_back();

            // Apply the size range before the name match, since it is the cheaper test.
            const bool inSizeRange = criteria.MatchesSize(qitem);

            const bool typeWanted = (criteria.includeFiles && qitem->IsTypeOrFlag(IT_FILE)) ||
                (criteria.includeFolders && qitem->IsTypeOrFlag(IT_DRIVE, IT_DIRECTORY));

            // Check for match
            if (inSizeRange && typeWanted)
            {
                const auto nameView = qitem->GetNameView();
                const bool isMatch = criteria.term.empty() || (criteria.wholePhrase ?
                    std::regex_match(nameView.begin(), nameView.end(), searchTermRegex) :
                    std::regex_search(nameView.begin(), nameView.end(), searchTermRegex));

                // Resolving an owner queries the security descriptor of a single item, which is
                // far more expensive than anything above, so it is only asked for once the item
                // has survived every other condition
                const bool isWanted = isMatch && (criteria.owner.empty() ||
                    FindStringOrdinal(FIND_FROMSTART, qitem->GetOwner(true).c_str(), -1,
                        criteria.owner.c_str(), -1, TRUE) >= 0);

                if (isWanted)
                {
                    if (matchedItems.size() < maxResults) matchedItems.push_back(qitem);
                    else
                    {
                        if (!limitExceeded) std::ranges::make_heap(matchedItems, bySize);
                        limitExceeded = true;
                        if (qitem->GetSizeLogical() > matchedItems.front()->GetSizeLogical())
                        {
                            std::ranges::pop_heap(matchedItems, bySize);
                            matchedItems.back() = qitem;
                            std::ranges::push_heap(matchedItems, bySize);
                        }
                    }
                }
            }

            // Descend into child items
            if (qitem->IsLeaf() || qitem->IsTypeOrFlag(IT_HLINKS)) continue;
            for (const auto& child : qitem->GetChildren())
            {
                queue.push_back(child);
            }
        }
        totals = CItemSearch::CalculateTotals(matchedItems, [pdlg] { return pdlg->IsCancelled(); });

    }).ShowModal();
    m_rootItem->SetLimitExceeded(limitExceeded || result == IDCANCEL);

    // Add found items to the interface
    CWaitCursor wait;
    CollapseItem(0);

    // Add to found items
    const ScopedRedrawPause lock(this);
    m_itemTracker.reserve(matchedItems.size());
    for (CItem* matchedItem : matchedItems)
    {
        auto searchItem = new CItemSearch(matchedItem);
        m_itemTracker.emplace(matchedItem, searchItem);
        m_rootItem->AddSearchItemChild(searchItem);
    }

    if (totals) m_rootItem->SetTotals(*totals);
    SortItems();
    ExpandItem(0);
}

void CFileSearchControl::RemoveItem(CItem* item)
{
    const ScopedRedrawPause lock(this);
    std::erase_if(m_itemTracker, [&](const auto& pair)
    {
        if (pair.first != item && !item->IsAncestorOf(pair.first) &&
            !(m_sizeFilterActive && pair.first->IsAncestorOf(item))) return false;
        m_rootItem->RemoveSearchItemChild(pair.second);
        return true;
    });
}

void CFileSearchControl::AfterDeleteAllItems()
{
    // Delete previous search results
    m_itemTracker.clear();
    m_sizeFilterActive = false;

    // Delete and recreate root item
    delete m_rootItem;
    m_rootItem = new CItemSearch();
    InsertItem(0, m_rootItem);
    m_rootItem->SetExpanded(true);
}
