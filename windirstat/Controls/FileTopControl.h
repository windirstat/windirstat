// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "ItemTop.h"
#include "TreeListControl.h"

class CFileTopControl final : public CTreeListControl
{
public:
    CFileTopControl();
    ~CFileTopControl() override { m_singleton = nullptr; }
    bool GetAscendingDefault(int column) override;
    static CFileTopControl* Get() { return m_singleton; }
    CItemTop* GetRootItem() const { return m_rootItem; }
    void ProcessTop(CItem* item);
    void UpdateItems(std::span<CItem* const> excluded = {}, bool allowRefill = true);
    void SortItems() override;
    void AfterDeleteAllItems() override;

protected:

    // Sort largest first, using item identity to keep equal-size candidates stable across cache updates.
    static constexpr auto CompareBySize = [](const CItem* lhs, const CItem* rhs)
    {
        const auto lhsSize = lhs->GetSizeLogical();
        const auto rhsSize = rhs->GetSizeLogical();
        return lhsSize != rhsSize ? lhsSize > rhsSize : std::less<const CItem*>{}(lhs, rhs);
    };

    inline static CFileTopControl* m_singleton = nullptr;
    CItemTop* m_rootItem = nullptr;
    std::unordered_map<CItem*, CItemTop*> m_itemTracker;

    // Scan workers feed this cache, so access these fields under the mutex while enumeration can run.
    std::mutex m_topMutex;
    std::vector<CItem*> m_topItems;
    size_t m_topLimit = COptions::LargeFileCount;
    bool m_hasUncachedItems = true;
    bool m_needsResort = true;

};
