// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "ItemSearch.h"
#include "TreeListControl.h"

struct SearchCriteria
{
    std::wstring term;
    bool caseSensitive = false;
    bool wholePhrase = false;
    bool regex = false;
    bool includeFiles = true;
    bool includeFolders = true;
    std::optional<ULONGLONG> sizeMinimum;
    std::optional<ULONGLONG> sizeMaximum;
    std::optional<ULONGLONG> physicalMinimum;
    std::optional<ULONGLONG> physicalMaximum;
    std::wstring owner;

    bool MatchesSize(const CItem* item) const;
    bool Matches(const CItem* item, const std::wregex& termRegex) const;
};

class CFileSearchControl final : public CTreeListControl
{
public:
    CFileSearchControl();
    ~CFileSearchControl() override { m_singleton = nullptr; }
    bool GetAscendingDefault(int column) override;
    static CFileSearchControl* Get() { return m_singleton; }
    CItemSearch* GetRootItem() const { return m_rootItem; }
    static std::wregex ComputeSearchRegex(const std::wstring& searchTerm, bool searchCase, bool useRegex);
    void ProcessSearch(CItem* item, const SearchCriteria& criteria);
    void RemoveItem(CItem* item);
    void RestoreItems();
    void AfterDeleteAllItems() override;

protected:

    inline static CFileSearchControl* m_singleton = nullptr;
    CItemSearch* m_rootItem = nullptr;
    std::unordered_map<CItem*, CItemSearch*> m_itemTracker;
    SearchCriteria m_criteria;
    std::vector<std::pair<std::wstring, bool>> m_removedItems;

};
