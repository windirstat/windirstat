// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "ItemWrapperNode.h"

// Columns
using ITEMDUPCOLUMNS = enum : std::uint8_t
{
    COL_ITEMDUP_NAME,
    COL_ITEMDUP_ITEMS,
    COL_ITEMDUP_SIZE_PHYSICAL,
    COL_ITEMDUP_SIZE_LOGICAL,
    COL_ITEMDUP_LAST_CHANGE
};

class CItemDupe final : public CItemWrapperNode<CItemDupe>
{
    std::wstring m_hashString;
    bool m_sampled = false;
    mutable std::wstring m_caption;
    mutable bool m_captionDirty = true;
    ULONGLONG m_sizePhysical = 0;
    ULONGLONG m_sizeLogical = 0;

public:
    CItemDupe(const CItemDupe&) = delete;
    CItemDupe(CItemDupe&&) = delete;
    CItemDupe& operator=(const CItemDupe&) = delete;
    CItemDupe& operator=(CItemDupe&&) = delete;
    CItemDupe() = default;
    CItemDupe(const std::vector<BYTE> & hash, bool sampled = false);
    CItemDupe(CItem* item) : CItemWrapperNode(item) {}

    // Inherited Overrides
    std::wstring GetText(int subitem) const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    HICON GetIcon() override;

    std::wstring GetHash() const { return m_hashString; }
    bool IsSampled() const { return m_sampled; }
    std::wstring GetHashAndExtensions() const;
    const std::vector<CItemDupe*>& GetChildren() const { return m_children; }
    void AddDupeItemChildren(std::span<CItemDupe* const> children);
    void RemoveDupeItemChildren(const std::unordered_set<const CTreeListItem*>& children);
};
