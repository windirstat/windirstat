// ItemDup.cpp - Implementation of CItemDupe
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

#include "stdafx.h"
#include "ItemDupe.h"
#include "FileDupeControl.h"
#include "WinDirStat.h"
#include "MainFrame.h"

#include <functional>
#include <queue>

#include "GlobalHelpers.h"

CItemDupe::CItemDupe(const CStringW& hash, ULONGLONG size) : m_hash(hash), m_size(size) {}

CItemDupe::CItemDupe(CItem* item) : m_item(item) {}

CItemDupe::~CItemDupe()
{
    std::lock_guard m_guard(m_protect);
    for (const auto& child : m_children)
    {
        delete child;
    }
    m_children.clear();
}

bool CItemDupe::DrawSubitem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) const
{
    // Handle individual file items
    if (subitem != COL_ITEMDUP_NAME) return false;
    return CTreeListItem::DrawSubitem(column_map.at(subitem), pdc, rc, state, width, focusLeft);
}

CStringW CItemDupe::GetText(int subitem) const
{
    // Root node
    if (GetParent() == nullptr) return subitem == COL_ITEMDUP_NAME ? L"Duplicates" : L"";

    // Parent hash nodes
    if (m_item == nullptr)
    {
        // Handle top-level hash collection nodes
        if (subitem == COL_ITEMDUP_NAME) return m_hash;
        if (subitem == COL_ITEMDUP_SIZE) return FormatBytes(m_size * GetChildren().size());
        if (subitem == COL_ITEMDUP_ITEMS) return FormatCount(GetChildren().size());
        return {};
    }

    // Individual file names
    if (subitem == COL_ITEMDUP_NAME) return m_item->GetPath();
    return m_item->GetText(column_map.at(subitem));
}

int CItemDupe::CompareSibling(const CTreeListItem* tlib, int subitem) const
{
    // Root node
    if (GetParent() == nullptr) return 0;

    // Parent hash nodes
    const auto* other = reinterpret_cast<const CItemDupe*>(tlib);
    if (m_item == nullptr)
    {
        // Handle top-level hash collection nodes
        if (subitem == COL_ITEMDUP_NAME) return signum(m_hash.CompareNoCase(other->m_hash));
        if (subitem == COL_ITEMDUP_SIZE) return usignum(m_size * m_children.size(), other->m_size * other->m_children.size());
        if (subitem == COL_ITEMDUP_ITEMS) return usignum(m_children.size(), other->m_children.size());
        return 0;
    }

    // Individual file names
    return m_item->CompareSibling(other->m_item, column_map.at(subitem));
}

int CItemDupe::GetTreeListChildCount()const
{
    return static_cast<int>(m_children.size());
}

CTreeListItem* CItemDupe::GetTreeListChild(int i) const
{
    return m_children[i];
}

short CItemDupe::GetImageToCache() const
{
    // Root node
    if (GetParent() == nullptr) return GetIconImageList()->getFreeSpaceImage();

    // Parent hash nodes
    if (m_item == nullptr) return GetIconImageList()->getFreeSpaceImage();

    // Individual file names
    return m_item->GetImageToCache();
}

const std::vector<CItemDupe*>& CItemDupe::GetChildren() const
{
    return m_children;
}

CItemDupe* CItemDupe::GetParent() const
{
    return reinterpret_cast<CItemDupe*>(CTreeListItem::GetParent());
}

void CItemDupe::AddChild(CItemDupe* child)
{
    child->SetParent(this);

    std::lock_guard m_guard(m_protect);
    m_children.push_back(child);

    if (IsVisible() && IsExpanded())
    {
        CMainFrame::Get()->InvokeInMessageThread([this, child]
        {
            m_vi->control->OnChildAdded(this, child);
        });
    }
}

void CItemDupe::RemoveChild(CItemDupe* child)
{
    std::lock_guard m_guard(m_protect);
    std::erase(m_children, child);

    if (IsVisible())
    {
        CMainFrame::Get()->InvokeInMessageThread([this, child]
        {
            CFileTreeControl::Get()->OnChildRemoved(this, child);
        });
    }

    delete child;
}

void CItemDupe::RemoveAllChildren()
{
    CMainFrame::Get()->InvokeInMessageThread([this]
    {
        CFileTreeControl::Get()->OnRemovingAllChildren(this);
    });

    std::lock_guard m_guard(m_protect);
    for (const auto& child : m_children)
    {
        delete child;
    }
    m_children.clear();
}
