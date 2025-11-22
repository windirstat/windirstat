// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
    COL_ITEMTOP_LAST_CHANGE
};

class CItemTop final : public CTreeListItem
{
    std::shared_mutex m_Protect;
    std::vector<CItemTop*> m_Children;
    CItem* m_Item = nullptr;

public:
    CItemTop(const CItemTop&) = delete;
    CItemTop(CItemTop&&) = delete;
    CItemTop& operator=(const CItemTop&) = delete;
    CItemTop& operator=(CItemTop&&) = delete;
    CItemTop() = default;
    CItemTop(CItem* item);
    ~CItemTop() override;

    // Translation map for leveraging Item routines
    static const std::unordered_map<uint8_t, uint8_t> columnMap;

    // CTreeListItem Interface
    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    int GetTreeListChildCount() const override;
    CTreeListItem* GetTreeListChild(int i) const override;
    HICON GetIcon() override;
    CTreeListItem* GetLinkedItem() override { return m_Item; }

    void AddTopItemChild(CItemTop* child);
    void RemoveTopItemChild(CItemTop* child);
};
