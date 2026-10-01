// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"

// Columns
using ITEMDUPCOLUMNS = enum : std::uint8_t
{
    COL_ITEMDUP_NAME,
    COL_ITEMDUP_ITEMS,
    COL_ITEMDUP_SIZE_PHYSICAL,
    COL_ITEMDUP_SIZE_LOGICAL,
    COL_ITEMDUP_LAST_CHANGE
};

class CItemDupe final : public CTreeListItem
{
    std::wstring m_hashString;
    bool m_sampled = false;
    mutable std::wstring m_caption;
    mutable bool m_captionDirty = true;
    std::vector<CItemDupe*> m_children;
    ULONGLONG m_sizePhysical = 0;
    ULONGLONG m_sizeLogical = 0;
    CItem* m_item = nullptr;
    std::shared_mutex m_protect;

public:
    CItemDupe(const CItemDupe&) = delete;
    CItemDupe(CItemDupe&&) = delete;
    CItemDupe& operator=(const CItemDupe&) = delete;
    CItemDupe& operator=(CItemDupe&&) = delete;
    CItemDupe() = default;
    CItemDupe(const std::vector<BYTE> & hash, bool sampled = false);
    CItemDupe(CItem* item);
    ~CItemDupe() override;

    // Inherited Overrides
    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    int GetTreeListChildCount() const override { return static_cast<int>(m_children.size()); }
    CTreeListItem* GetTreeListChild(const int i) const override { return m_children[i]; }
    HICON GetIcon() override;
    CItem* GetLinkedItem() noexcept override { return m_item; }

    std::wstring GetHash() const { return m_hashString; }
    bool IsSampled() const { return m_sampled; }
    std::wstring GetHashAndExtensions() const;
    const std::vector<CItemDupe*>& GetChildren() const { return m_children; }
    void AddDupeItemChildren(std::span<CItemDupe* const> children);
    void RemoveDupeItemChild(CItemDupe* child);
};
