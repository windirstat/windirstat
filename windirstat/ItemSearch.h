// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"

// Columns
using ITEMSEARCHCOLUMNS = enum : std::uint8_t
{
    COL_ITEMSEARCH_NAME,
    COL_ITEMSEARCH_SIZE_PHYSICAL,
    COL_ITEMSEARCH_SIZE_LOGICAL,
    COL_ITEMSEARCH_LAST_CHANGE
};

class CItemSearch final : public CTreeListItem
{
    std::shared_mutex m_protect;
    std::vector<CItemSearch*> m_children;
    CItem* m_item = nullptr;
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
    CItemSearch(CItem* item);
    ~CItemSearch() override;

    // CTreeListItem Interface
    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    int GetTreeListChildCount() const override { return static_cast<int>(m_children.size()); }
    CTreeListItem* GetTreeListChild(const int i) const override { return m_children[i]; }
    HICON GetIcon() override;
    CItem* GetLinkedItem() noexcept override { return m_item; }
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
