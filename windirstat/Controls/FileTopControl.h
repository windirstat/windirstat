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
    void ClearPendingItems() { m_queuedSet.clear(); }
    void RemoveItem(CItem* item);
    void SortItems() override;
    void AfterDeleteAllItems() override;

protected:

    // Custom comparator to keep the list organized by size (largest first)
    static constexpr auto CompareBySize = [](const CItem* lhs, const CItem* rhs)
    {
        return lhs->GetSizeLogical() > rhs->GetSizeLogical();
    };

    inline static CFileTopControl* m_singleton = nullptr;
    CItemTop* m_rootItem = nullptr;
    SingleConsumerQueue<CItem*> m_queuedSet;
    std::vector<CItem*> m_sizeMap;
    std::vector<CItem*> m_topItems;
    bool m_needsResort = true;
    std::unordered_map<CItem*, CItemTop*> m_itemTracker;
    size_t m_previousTopN = 0;

};
