// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

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

bool SearchCriteria::Matches(const CItem* item, const std::wregex& termRegex) const
{
    // Apply the size range before the name match, since it is the cheaper test.
    const bool typeWanted = (includeFiles && item->IsTypeOrFlag(IT_FILE)) ||
        (includeFolders && item->IsTypeOrFlag(IT_DRIVE, IT_DIRECTORY));
    if (!MatchesSize(item) || !typeWanted) return false;

    // Check for match
    const auto nameView = item->GetNameView();
    const bool isMatch = term.empty() || (wholePhrase ?
        std::regex_match(nameView.begin(), nameView.end(), termRegex) :
        std::regex_search(nameView.begin(), nameView.end(), termRegex));

    // Resolving an owner queries the security descriptor of a single item, which is
    // far more expensive than anything above, so it is only asked for once the item
    // has survived every other condition
    return isMatch && (owner.empty() || FindStringOrdinal(FIND_FROMSTART,
        item->GetOwner(true).c_str(), -1, owner.c_str(), -1, TRUE) >= 0);
}

void CFileSearchControl::ProcessSearch(CItem* item, const SearchCriteria& criteria)
{
    if (!CWinDirStatModel::Get()->IsScanSettled()) return;

    // Update tab visibility to show search tab if results exist
    CMainFrame::Get()->GetFileTabbedView()->SetSearchTabVisibility(true);

    // Remove previous results
    SetRootItem();
    m_criteria = criteria;
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

            if (criteria.Matches(qitem, searchTermRegex))
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
    const bool sizeFilterActive = m_criteria.sizeMinimum || m_criteria.sizeMaximum ||
        m_criteria.physicalMinimum || m_criteria.physicalMaximum;
    std::erase_if(m_itemTracker, [&](const auto& pair)
    {
        if (pair.first != item && !item->IsAncestorOf(pair.first) &&
            !(sizeFilterActive && pair.first->IsAncestorOf(item))) return false;

        // Preserve result identities before their scan-tree items can be deleted.
        m_removedItems.emplace_back(pair.first->GetPath(), IsItemSelected(pair.second));
        m_rootItem->RemoveSearchItemChild(pair.second);
        return true;
    });
}

void CFileSearchControl::RestoreItems()
{
    if (m_removedItems.empty()) return;

    // Rebind surviving results to the refreshed tree and reapply the original criteria.
    const ScopedRedrawPause lock(this);
    const auto termRegex = ComputeSearchRegex(m_criteria.term, m_criteria.caseSensitive, m_criteria.regex);
    auto selected = GetAllSelected<CItemSearch>(true);
    const bool expanded = m_rootItem->IsExpanded();
    CollapseItem(0);
    for (const auto& [path, wasSelected] : m_removedItems)
    {
        CItem* item = CWinDirStatModel::Get()->GetRootItem()->FindItemByPath(path);
        if (item == nullptr || !m_criteria.Matches(item, termRegex)) continue;
        const auto [iter, inserted] = m_itemTracker.try_emplace(item, nullptr);
        if (inserted)
        {
            iter->second = new CItemSearch(item);
            m_rootItem->AddSearchItemChild(iter->second);
        }
        if (wasSelected) selected.push_back(iter->second);
    }
    m_removedItems.clear();
    if (expanded) ExpandItem(0, false);
    for (const auto* item : selected) SelectItem(item, false, true);
}

void CFileSearchControl::AfterDeleteAllItems()
{
    // Delete previous search results
    m_itemTracker.clear();
    m_removedItems.clear();
    m_criteria = {};

    // Delete and recreate root item
    delete m_rootItem;
    m_rootItem = new CItemSearch();
    InsertItem(0, m_rootItem);
    m_rootItem->SetExpanded(true);
}
