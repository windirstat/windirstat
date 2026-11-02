// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "ItemWrapperNode.h"

// Columns
using ITEMDEDUPSHARECOLUMNS = enum : std::uint8_t
{
    COL_ITEMDEDUPSHARE_NAME,
    COL_ITEMDEDUPSHARE_SHARED,
    COL_ITEMDEDUPSHARE_PCT_QUERIED,
    COL_ITEMDEDUPSHARE_PCT_FILE,
    COL_ITEMDEDUPSHARE_ON_DISK,
    COL_ITEMDEDUPSHARE_SIZE_PHYSICAL,
    COL_ITEMDEDUPSHARE_SIZE_LOGICAL
};

// A file sharing deduplicated chunks with the queried selection; the root row
// describes the query. Byte values are copied when the query runs since the
// accounting is rebuilt whenever the volume is refreshed.
class CItemDedupShare final : public CItemWrapperNode<CItemDedupShare>
{
    ULONGLONG m_sharedBytes = 0;  // on-disk bytes this file shares with the query
    ULONGLONG m_fileBytes = 0;    // on-disk bytes of this file

    // Root only
    std::wstring m_title;
    ULONGLONG m_queriedBytes = 0; // on-disk bytes of the queried data
    ULONGLONG m_outsideBytes = 0;

    // Rows take the query total from the root
    ULONGLONG GetQueriedBytes() const { return static_cast<const CItemDedupShare*>(GetParent())->m_queriedBytes; }

public:
    CItemDedupShare(const CItemDedupShare&) = delete;
    CItemDedupShare(CItemDedupShare&&) = delete;
    CItemDedupShare& operator=(const CItemDedupShare&) = delete;
    CItemDedupShare& operator=(CItemDedupShare&&) = delete;
    CItemDedupShare() = default;
    CItemDedupShare(CItem* item, const ULONGLONG sharedBytes, const ULONGLONG fileBytes) :
        CItemWrapperNode(item), m_sharedBytes(sharedBytes), m_fileBytes(fileBytes) {}

    // CTreeListItem Interface
    std::wstring GetText(int subitem) const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    HICON GetIcon() override;
    CTreeListItem* GetAncestorCheckItem() override { return m_item; }

    void SetSummary(const std::wstring& title, ULONGLONG queriedBytes, ULONGLONG sharedBytes, ULONGLONG outsideBytes);
    void AddShareItemChild(CItemDedupShare* child);
    const std::vector<CItemDedupShare*>& GetShareChildren() const { return m_children; }
};
