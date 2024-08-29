// ItemDupe.h - Declaration of CItemDupe
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#pragma once

#include "stdafx.h"
#include "Item.h"

#include <unordered_map>

// Columns
using ITEMDUPCOLUMNS = enum
{
    COL_ITEMDUP_NAME,
    COL_ITEMDUP_ITEMS,
    COL_ITEMDUP_SIZE_PHYSICAL,
    COL_ITEMDUP_SIZE_LOGICAL,
    COL_ITEMDUP_LASTCHANGE
};

class CItemDupe final : public CTreeListItem
{
    std::wstring m_Hash;
    ULONGLONG m_SizePhysical = 0;
    ULONGLONG m_SizeLogical = 0;
    CItem* m_Item = nullptr;
    std::shared_mutex m_Protect;
    std::vector<CItemDupe*> m_Children;

public:
    CItemDupe(const CItemDupe&) = delete;
    CItemDupe(CItemDupe&&) = delete;
    CItemDupe& operator=(const CItemDupe&) = delete;
    CItemDupe& operator=(CItemDupe&&) = delete;
    CItemDupe() = default;
    CItemDupe(const std::wstring & hash, ULONGLONG sizePhysical, ULONGLONG sizeLogical);
    CItemDupe(CItem* item);
    ~CItemDupe() override = default;

    // Translation map for leveraging Item routines
    const std::unordered_map<int, int> columnMap =
    {
        { COL_ITEMDUP_NAME, COL_NAME },
        { COL_ITEMDUP_ITEMS, COL_ITEMS },
        { COL_ITEMDUP_SIZE_LOGICAL, COL_SIZE_LOGICAL },
        { COL_ITEMDUP_SIZE_PHYSICAL, COL_SIZE_PHYSICAL },
        { COL_ITEMDUP_LASTCHANGE, COL_LASTCHANGE }
    };

    // CTreeListItem Interface
    bool DrawSubitem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) const override;
    std::wstring GetText(int subitem) const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    int GetTreeListChildCount() const override;
    CTreeListItem* GetTreeListChild(int i) const override;
    short GetImageToCache() const override;

    CItem* GetItem() const { return m_Item; }
    const std::vector<CItemDupe*>& GetChildren() const;
    CItemDupe* GetParent() const;
    void AddChild(CItemDupe* child);
    void RemoveChild(CItemDupe* child);
    void RemoveAllChildren();
};
