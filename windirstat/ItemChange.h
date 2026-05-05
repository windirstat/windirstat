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
#include "SnapshotHistory.h"
#include "TreeListControl.h"

using ITEMCHANGECOLUMNS = enum : std::uint8_t
{
    COL_ITEMCHANGE_NAME,
    COL_ITEMCHANGE_DELTA,
    COL_ITEMCHANGE_CURRENT_SIZE,
    COL_ITEMCHANGE_PREVIOUS_SIZE,
    COL_ITEMCHANGE_FILES_DELTA,
    COL_ITEMCHANGE_FOLDERS_DELTA,
};

class CItemChange final : public CTreeListItem
{
    static constexpr size_t CachedTextCount = COL_ITEMCHANGE_FOLDERS_DELTA + 1;

    std::shared_mutex m_protect;
    std::vector<CItemChange*> m_children;
    std::optional<SnapshotGrowthEntry> m_entry;
    std::array<std::wstring, CachedTextCount> m_cachedText;
    std::wstring m_previousSnapshotLabel;

public:
    CItemChange(const CItemChange&) = delete;
    CItemChange(CItemChange&&) = delete;
    CItemChange& operator=(const CItemChange&) = delete;
    CItemChange& operator=(CItemChange&&) = delete;
    CItemChange() = default;
    explicit CItemChange(const SnapshotGrowthEntry& entry);
    ~CItemChange() override;

    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    int GetTreeListChildCount() const override;
    CTreeListItem* GetTreeListChild(int i) const override;
    HICON GetIcon() override;
    CItem* GetLinkedItem() noexcept override { return m_entry.has_value() ? m_entry->currentItem : nullptr; }

    void AddChangeItemChild(CItemChange* child);
    void RemoveChangeItemChild(CItemChange* child);
    void ReserveChangeItemChildren(size_t count);
    void SetPreviousSnapshotLabel(const std::wstring& label) { m_previousSnapshotLabel = label; }
};
