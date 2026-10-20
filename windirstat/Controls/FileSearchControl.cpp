// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "ItemSearch.h"
#include "FileTreeView.h"
#include "FinderBasic.h"

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
    if ((physicalMinimum || physicalMaximum) && !item->HasSizePhysical()) return false;

    return (!sizeMinimum || item->GetSizeLogical() >= *sizeMinimum) &&
        (!sizeMaximum || item->GetSizeLogical() <= *sizeMaximum) &&
        (!physicalMinimum || item->GetSizePhysical() >= *physicalMinimum) &&
        (!physicalMaximum || item->GetSizePhysical() <= *physicalMaximum);
}

bool SearchCriteria::Matches(const CItem* item, const std::wregex& termRegex) const
{
    // Results of an empty-folder search that are restored after a refresh are checked again
    if (emptyFoldersOnly)
    {
        std::unordered_map<std::wstring, bool> checkedFolders;
        return FinderBasic::IsEmptyFolderOnDisk(item, checkedFolders);
    }

    // Apply the size range before the name match, since it is the cheaper test.
    const bool typeWanted = (includeFiles && item->IsTypeOrFlag(IT_FILE)) ||
        (includeFolders && item->IsTypeOrFlag(IT_DRIVE, IT_DIRECTORY));
    if (!MatchesSize(item) || !typeWanted) return false;

    // Check for match
    const std::wstring remoteName = item->IsTypeOrFlag(ITF_REMOTE) ? item->GetName() : std::wstring{};
    const auto nameView = item->IsTypeOrFlag(ITF_REMOTE) ? std::wstring_view(remoteName) : item->GetNameView();
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
    PopulateSearchResults(matchedItems, totals);
}

void CFileSearchControl::PopulateSearchResults(const std::vector<CItem*>& matchedItems, const std::optional<CItemSearch::SizeTotals>& totals)
{
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

void CFileSearchControl::SearchEmptyFolders(const std::vector<CItem*>& items)
{
    // A command can still be posted during a scan, and the walk below reads the tree it changes
    if (!CWinDirStatModel::Get()->IsScanSettled()) return;

    // Show the search tab, which holds the results
    CMainFrame::Get()->GetFileTabbedView()->SetSearchTabVisibility(true);

    // Remove previous results
    SetRootItem();
    m_criteria = { .emptyFoldersOnly = true };
    m_rootItem->SetLimitExceeded(false);

    // Walk from the same starting points the removal uses
    std::vector<CItem*> stack = CWinDirStatModel::GetEmptyFolderRoots(items);
    ULONGLONG totalItems = 0;
    for (const CItem* item : stack) totalItems += item->GetItemsCount();

    // Every empty directory of a branch is listed, not only its topmost one. Deleting the top
    // does take the whole branch with it, but the choice of how much of a branch to remove
    // belongs to the user, so each nested empty directory gets its own selectable entry.
    std::vector<CItem*> results;
    bool limitExceeded = false;
    std::optional<CItemSearch::SizeTotals> totals;
    const auto result = CProgressDlg(static_cast<size_t>(totalItems), CProgressDlg::Flags::None, GetMainWindow(),
        [&](CProgressDlg* pdlg)
    {
        std::unordered_map<std::wstring, bool> checkedFolders;

        while (!stack.empty() && !pdlg->IsCancelled())
        {
            pdlg->Increment();
            CItem* item = stack.back();
            stack.pop_back();
            if (FinderBasic::IsEmptyFolderOnDisk(item, checkedFolders))
            {
                // Cap like the existing text search, so a drive full of leftover empty
                // folders doesn't dump everything into the result view at once; stop the
                // scan itself once hit instead of collecting everything first.
                if (results.size() >= COptions::SearchMaxResults)
                {
                    limitExceeded = true;
                    break;
                }
                results.push_back(item);
            }

            // Descend into a directory that was just listed as well, so the empty directories
            // nested inside it get their own entries. Followed links stay the exception: their
            // children live outside the selected physical tree, so an empty folder found there
            // isn't one the user asked about.
            if (item->HasChildren() && !item->IsTypeOrFlag(ITRP_MASK))
            {
                stack.insert(stack.end(), item->GetChildren().begin(), item->GetChildren().end());
            }
        }
        totals = CItemSearch::CalculateTotals(results, [pdlg] { return pdlg->IsCancelled(); });
    }).ShowModal();
    m_rootItem->SetLimitExceeded(limitExceeded || result == IDCANCEL);

    // Add found items to the interface - a snapshot, like every other scan result: a folder
    // listed here can still gain a file before the user gets around to deleting it, which is
    // why its removal only takes folders that are still empty.
    PopulateSearchResults(results, totals);
}

std::vector<CItem*> CFileSearchControl::GetEmptyFolderResults() const
{
    if (!m_criteria.emptyFoldersOnly) return {};
    auto items = m_itemTracker | std::views::keys;
    return { items.begin(), items.end() };
}

void CFileSearchControl::RemoveItems(std::span<CItem* const> items)
{
    if (items.empty() || m_itemTracker.empty()) return;

    const ScopedRedrawPause lock(this);
    const bool sizeFilterActive = m_criteria.sizeMinimum || m_criteria.sizeMaximum ||
        m_criteria.physicalMinimum || m_criteria.physicalMaximum;
    const std::unordered_set<CItem*> refreshed(items.begin(), items.end());
    std::unordered_set<CItem*> ancestors;
    // Sizes and emptiness depend on descendants, so such results are rechecked for ancestors too
    if (sizeFilterActive || m_criteria.emptyFoldersOnly) for (auto* item : items)
        for (auto* parent = item->GetParent(); parent != nullptr; parent = parent->GetParent())
            if (!ancestors.insert(parent).second) break;

    // Check each result against the entire refresh selection before detaching any rows.
    std::unordered_set<const CTreeListItem*> removed;
    std::erase_if(m_itemTracker, [&](const auto& pair)
    {
        bool insideRefresh = false;
        for (auto* parent = pair.first; parent != nullptr; parent = parent->GetParent())
        {
            if (!refreshed.contains(parent)) continue;
            insideRefresh = true;
            break;
        }
        if (!insideRefresh && !ancestors.contains(pair.first)) return false;

        // Preserve untouched ancestors and distinguish directories from marker objects at the same URI.
        m_removedItems.push_back({ pair.first->GetPath(), pair.first->GetRawType() & IT_MASK,
            IsItemSelected(pair.second), insideRefresh ? nullptr : pair.first });
        removed.insert(pair.second);
        return true;
    });
    m_rootItem->RemoveSearchItemChildren(removed);
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
    CItem* root = CWinDirStatModel::Get()->GetRootItem();
    CItem::ChildNameIndex childNames;
    for (const auto& removed : m_removedItems)
    {
        CItem* item = removed.item;
        if (item == nullptr) item = root->FindItemByPath(removed.path, false, removed.type, &childNames);
        if (item == nullptr || !m_criteria.Matches(item, termRegex)) continue;
        const auto [iter, inserted] = m_itemTracker.try_emplace(item, nullptr);
        if (inserted)
        {
            iter->second = new CItemSearch(item);
            m_rootItem->AddSearchItemChild(iter->second);
        }
        if (removed.selected) selected.push_back(iter->second);
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
