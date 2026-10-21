// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "ItemTop.h"
#include "FileTopControl.h"

static constexpr std::array columns{ COL_NAME, COL_SIZE_PHYSICAL, COL_SIZE_LOGICAL, COL_LAST_CHANGE };

std::wstring CItemTop::GetText(const int subitem) const
{
    // Root node
    static std::wstring tops = Localization::Lookup(IDS_LARGEST_FILES);
    if (GetParent() == nullptr) return subitem == COL_ITEMTOP_NAME ? tops : std::wstring{};

    // Parent hash nodes
    if (m_item == nullptr) return {};

    // Individual file names
    return GetLinkedText(subitem, columns);
}

int CItemTop::CompareSibling(const CTreeListItem* tlib, const int subitem) const
{
    // Root node
    if (GetParent() == nullptr) return 0;

    // Parent hash nodes
    if (m_item == nullptr) return 0;

    // Individual file names
    return CompareLinked(tlib, subitem, columns);
}

HICON CItemTop::GetIcon()
{
    return GetWrapperIcon(GetIconHandler()->GetLargestImage());
}

void CItemTop::AddTopItemChildren(std::span<CItemTop* const> children)
{
    std::scoped_lock guard(m_protect);
    for (auto* child : children) child->SetParent(this);
    m_children.append_range(children);

    if (IsVisible() && IsExpanded())
    {
        const std::vector<CTreeListItem*> rows(children.begin(), children.end());
        CFileTopControl::Get()->OnChildrenAdded(this, rows);
    }
}

void CItemTop::RemoveTopItemChildren(const std::unordered_set<const CTreeListItem*>& children)
{
    if (children.empty()) return;
    CFileTopControl::Get()->OnItemsRemoved(children);

    std::scoped_lock guard(m_protect);
    std::erase_if(m_children, [&](CItemTop* child)
    {
        if (!children.contains(child)) return false;
        delete child;
        return true;
    });
}
