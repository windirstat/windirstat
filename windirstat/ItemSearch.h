// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "ItemWrapperNode.h"

// Columns
using ITEMSEARCHCOLUMNS = enum : std::uint8_t
{
    COL_ITEMSEARCH_NAME,
    COL_ITEMSEARCH_SIZE_PHYSICAL,
    COL_ITEMSEARCH_SIZE_LOGICAL,
    COL_ITEMSEARCH_LAST_CHANGE
};

class CItemSearch final : public CItemWrapperNode<CItemSearch>
{
    ULONGLONG m_totalSizeLogical = 0;
    ULONGLONG m_totalSizePhysical = 0;
    bool m_physicalKnown = true;
    bool m_limitExceeded = false;
    bool m_totalsPending = false;

public:
    CItemSearch(const CItemSearch&) = delete;
    CItemSearch(CItemSearch&&) = delete;
    CItemSearch& operator=(const CItemSearch&) = delete;
    CItemSearch& operator=(CItemSearch&&) = delete;
    CItemSearch() = default;
    CItemSearch(CItem* item) : CItemWrapperNode(item) {}

    // CTreeListItem Interface
    std::wstring GetText(int subitem) const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    HICON GetIcon() override;
    CTreeListItem* GetAncestorCheckItem() override { return m_item; }

    void AddSearchItemChild(CItemSearch* child);
    void RemoveSearchItemChild(CItemSearch* child);
    struct SizeTotals { ULONGLONG logical = 0, physical = 0; bool physicalKnown = true; };
    static std::optional<SizeTotals> CalculateTotals(std::span<CItem* const> items,
        const std::function<bool()>& isCancelled = {});
    SizeTotals CalculateTotals() const;
    void SetTotals(const SizeTotals& totals);
    void RecalculateTotals() { SetTotals(CalculateTotals()); }
    void SetTotalsPending(const bool pending) { m_totalsPending = pending; }
    void SetLimitExceeded(const bool exceeded) { m_limitExceeded = exceeded; }
    bool GetLimitExceeded() const { return m_limitExceeded; }
};
