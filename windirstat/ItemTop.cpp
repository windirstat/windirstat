// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "ItemTop.h"
#include "FileTopControl.h"

CItemTop::CItemTop(CItem* item) : m_item(item) {}

CItemTop::~CItemTop()
{
    for (const auto& m_child : m_children)
    {
        delete m_child;
    }
}

static int GetMappedColumn(const int subitem)
{
    switch (subitem)
    {
    case COL_ITEMTOP_NAME: return COL_NAME;
    case COL_ITEMTOP_SIZE_LOGICAL: return COL_SIZE_LOGICAL;
    case COL_ITEMTOP_SIZE_PHYSICAL: return COL_SIZE_PHYSICAL;
    case COL_ITEMTOP_LAST_CHANGE: return COL_LAST_CHANGE;
    default: return -1;
    }
}

bool CItemTop::DrawSubItem(const int subitem, CDC* pdc, const CRect rc, const UINT state, int* width, int* focusLeft)
{
    // Handle individual file items
    if (subitem != COL_ITEMTOP_NAME) return false;
    return CTreeListItem::DrawSubItem(COL_NAME, pdc, rc, state, width, focusLeft);
}

std::wstring CItemTop::GetText(const int subitem) const
{
    // Root node
    static std::wstring tops = Localization::Lookup(IDS_LARGEST_FILES);
    if (GetParent() == nullptr) return subitem == COL_ITEMTOP_NAME ? tops : std::wstring{};

    // Parent hash nodes
    if (m_item == nullptr) return {};

    // Individual file names
    if (subitem == COL_ITEMTOP_NAME) return m_item->GetPath();
    const int mapped = GetMappedColumn(subitem);
    return mapped != -1 ? m_item->GetText(mapped) : std::wstring{};
}

int CItemTop::CompareSibling(const CTreeListItem* tlib, const int subitem) const
{
    // Root node
    if (GetParent() == nullptr) return 0;

    // Parent hash nodes
    if (m_item == nullptr) return 0;

    // Individual file names
    const auto* other = reinterpret_cast<const CItemTop*>(tlib);
    const int mapped = GetMappedColumn(subitem);
    return mapped != -1 ? m_item->CompareSibling(other->m_item, mapped) : 0;
}

HICON CItemTop::GetIcon()
{
    auto* viewState = GetViewState();

    // No icon to return if not visible yet
    if (viewState == nullptr) return nullptr;

    if (viewState->icon != nullptr) return viewState->icon;

    // Cache icon for parent nodes
    if (m_item == nullptr)
    {
        viewState->icon = GetIconHandler()->GetLargestImage();
        return viewState->icon;
    }

    // Fetch all other icons
    const std::wstring iconPath = m_item->IsTypeOrFlag(ITF_REMOTE) ?
        L"C:\\~" + m_item->GetExtension() : m_item->GetPath();
    CDirStatApp::Get()->GetIconHandler()->DoAsyncShellInfoLookup(std::make_tuple(this,
        viewState->control, iconPath, m_item->GetAttributes(), &viewState->icon, nullptr));
    return viewState->icon;
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
