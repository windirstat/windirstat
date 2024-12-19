// ItemTop.h - Declaration of CItemTop
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
using ITEMTOPCOLUMNS = enum : std::uint8_t
{
    COL_ITEMTOP_NAME,
    COL_ITEMTOP_SIZE_PHYSICAL,
    COL_ITEMTOP_SIZE_LOGICAL,
    COL_ITEMTOP_LASTCHANGE
};

class CItemTop final : public CTreeListItem
{
    CItem* m_Item = nullptr;
    std::shared_mutex m_Protect;
    std::vector<CItemTop*> m_Children;

public:
    CItemTop(const CItemTop&) = delete;
    CItemTop(CItemTop&&) = delete;
    CItemTop& operator=(const CItemTop&) = delete;
    CItemTop& operator=(CItemTop&&) = delete;
    CItemTop() = default;
    CItemTop(CItem* item);
    ~CItemTop() override = default;

    // Translation map for leveraging Item routines
    const std::unordered_map<int, int> columnMap =
    {
        { COL_ITEMTOP_NAME, COL_NAME },
        { COL_ITEMTOP_SIZE_LOGICAL, COL_SIZE_LOGICAL },
        { COL_ITEMTOP_SIZE_PHYSICAL, COL_SIZE_PHYSICAL },
        { COL_ITEMTOP_LASTCHANGE, COL_LASTCHANGE }
    };

    // CTreeListItem Interface
    bool DrawSubitem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) const override;
    std::wstring GetText(int subitem) const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    int GetTreeListChildCount() const override;
    CTreeListItem* GetTreeListChild(int i) const override;
    short GetImageToCache() const override;
    int GetImage() const override;

    CItem* GetItem() const { return m_Item; }
    void AddTopItemChild(CItemTop* child);
    void RemoveTopItemChild(CItemTop* child);
};
