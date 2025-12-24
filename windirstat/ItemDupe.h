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
    std::wstring m_HashString;
    std::vector<BYTE> m_Hash;
    std::vector<CItemDupe*> m_Children;
    ULONGLONG m_SizePhysical = 0;
    ULONGLONG m_SizeLogical = 0;
    CItem* m_Item = nullptr;
    std::shared_mutex m_Protect;

public:
    CItemDupe(const CItemDupe&) = delete;
    CItemDupe(CItemDupe&&) = delete;
    CItemDupe& operator=(const CItemDupe&) = delete;
    CItemDupe& operator=(CItemDupe&&) = delete;
    CItemDupe() = default;
    CItemDupe(const std::vector<BYTE> & hash);
    CItemDupe(CItem* item);
    ~CItemDupe() override;

    // Translation map for leveraging Item routines
    static const std::unordered_map<uint8_t, uint8_t> columnMap;

    // Inherited Overrides
    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    int GetTreeListChildCount() const override;
    CTreeListItem* GetTreeListChild(int i) const override;
    HICON GetIcon() override;
    CItem* GetLinkedItem() override { return m_Item; }

    std::wstring GetHash() const { return m_HashString; }
    std::wstring GetHashAndExtensions() const;
    const std::vector<CItemDupe*>& GetChildren() const;
    void AddDupeItemChild(CItemDupe* child);
    void RemoveDupeItemChild(CItemDupe* child);
};
