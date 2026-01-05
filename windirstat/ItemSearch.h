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

public:
    CItemSearch(const CItemSearch&) = delete;
    CItemSearch(CItemSearch&&) = delete;
    CItemSearch& operator=(const CItemSearch&) = delete;
    CItemSearch& operator=(CItemSearch&&) = delete;
    CItemSearch() = default;
    CItemSearch(CItem* item);
    ~CItemSearch() override;

    // Translation map for leveraging Item routines
    static const std::unordered_map<uint8_t, uint8_t> s_columnMap;

    // CTreeListItem Interface
    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    int GetTreeListChildCount() const override;
    CTreeListItem* GetTreeListChild(int i) const override;
    HICON GetIcon() override;
    CItem* GetLinkedItem() override { return m_item; }

    void AddSearchItemChild(CItemSearch* child);
    void RemoveSearchItemChild(CItemSearch* child);
};
