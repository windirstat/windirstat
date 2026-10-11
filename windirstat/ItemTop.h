// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "ItemWrapperNode.h"

// Columns
using ITEMTOPCOLUMNS = enum : std::uint8_t
{
    COL_ITEMTOP_NAME,
    COL_ITEMTOP_SIZE_PHYSICAL,
    COL_ITEMTOP_SIZE_LOGICAL,
    COL_ITEMTOP_LAST_CHANGE
};

class CItemTop final : public CItemWrapperNode<CItemTop>
{

public:
    CItemTop(const CItemTop&) = delete;
    CItemTop(CItemTop&&) = delete;
    CItemTop& operator=(const CItemTop&) = delete;
    CItemTop& operator=(CItemTop&&) = delete;
    CItemTop() = default;
    CItemTop(CItem* item) : CItemWrapperNode(item) {}

    // CTreeListItem Interface
    std::wstring GetText(int subitem) const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    HICON GetIcon() override;
    CTreeListItem* GetAncestorCheckItem() override { return m_item; }

    void AddTopItemChild(CItemTop* child);
    void RemoveTopItemChild(CItemTop* child);
};
