// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "TreeListControl.h"

// Columns
using ITEMTOPCOLUMNS = enum : std::uint8_t
{
    COL_ITEMTOP_NAME,
    COL_ITEMTOP_SIZE_PHYSICAL,
    COL_ITEMTOP_SIZE_LOGICAL,
    COL_ITEMTOP_LAST_CHANGE
};

class CItemTop final : public CTreeListItem
{
    std::shared_mutex m_protect;
    std::vector<CItemTop*> m_children;
    CItem* m_item = nullptr;

public:
    CItemTop(const CItemTop&) = delete;
    CItemTop(CItemTop&&) = delete;
    CItemTop& operator=(const CItemTop&) = delete;
    CItemTop& operator=(CItemTop&&) = delete;
    CItemTop() = default;
    CItemTop(CItem* item);
    ~CItemTop() override;

    // CTreeListItem Interface
    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    int GetTreeListChildCount() const override { return static_cast<int>(m_children.size()); }
    CTreeListItem* GetTreeListChild(const int i) const override { return m_children[i]; }
    HICON GetIcon() override;
    CItem* GetLinkedItem() noexcept override { return m_item; }
    CTreeListItem* GetAncestorCheckItem() override { return m_item; }

    void AddTopItemChild(CItemTop* child);
    void RemoveTopItemChild(CItemTop* child);
};
