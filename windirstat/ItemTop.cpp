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

void CItemTop::AddTopItemChild(CItemTop* child)
{
    child->SetParent(this);

    std::scoped_lock guard(m_protect);
    m_children.push_back(child);

    if (IsVisible() && IsExpanded())
    {
        CFileTopControl::Get()->OnChildAdded(this, child);
    }
}

void CItemTop::RemoveTopItemChild(CItemTop* child)
{
    if (IsVisible())
    {
        CFileTopControl::Get()->OnChildRemoved(this, child);
    }

    std::scoped_lock guard(m_protect);
    auto& children = m_children;
    if (const auto it = std::ranges::find(children, child); it != children.end())
    {
        children.erase(it);
    }

    delete child;
}
