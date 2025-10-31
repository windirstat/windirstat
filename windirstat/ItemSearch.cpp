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

#include "stdafx.h"
#include "ItemSearch.h"
#include "FileTopControl.h"
#include "WinDirStat.h"
#include "MainFrame.h"
#include "Localization.h"

CItemSearch::CItemSearch(CItem* item) : m_Item(item) {}

CItemSearch::~CItemSearch()
{
    for (const auto& m_Child : m_Children)
    {
        delete m_Child;
    }
}

const std::unordered_map<uint8_t, uint8_t> CItemSearch::columnMap =
{
    { COL_ITEMSEARCH_NAME, COL_NAME },
    { COL_ITEMSEARCH_SIZE_LOGICAL, COL_SIZE_LOGICAL },
    { COL_ITEMSEARCH_SIZE_PHYSICAL, COL_SIZE_PHYSICAL },
    { COL_ITEMSEARCH_LASTCHANGE, COL_LASTCHANGE }
};

bool CItemSearch::DrawSubItem(const int subitem, CDC* pdc, const CRect rc, const UINT state, int* width, int* focusLeft)
{
    // Handle individual file items
    if (subitem != COL_ITEMSEARCH_NAME) return false;
    return CTreeListItem::DrawSubItem(columnMap.at(static_cast<uint8_t>(subitem)), pdc, rc, state, width, focusLeft);
}

std::wstring CItemSearch::GetText(const int subitem) const
{
    // Root node
    static std::wstring tops = Localization::Lookup(IDS_SEARCH_RESULTS);
    if (GetParent() == nullptr) return subitem == COL_ITEMSEARCH_NAME ?
        std::format(L"{} ({})", tops, m_Children.size()) : std::wstring{};

    // Parent hash nodes
    if (m_Item == nullptr) return {};

    // Individual file names
    if (subitem == COL_ITEMSEARCH_NAME) return m_Item->GetPath();
    return m_Item->GetText(columnMap.at(static_cast<uint8_t>(subitem)));
}

int CItemSearch::CompareSibling(const CTreeListItem* tlib, const int subitem) const
{
    // Root node
    if (GetParent() == nullptr) return 0;

    // Parent hash nodes
    if (m_Item == nullptr) return 0;

    // Individual file names
    const auto* other = reinterpret_cast<const CItemSearch*>(tlib);
    return m_Item->CompareSibling(other->m_Item, columnMap.at(static_cast<uint8_t>(subitem)));
}

int CItemSearch::GetTreeListChildCount()const
{
    return static_cast<int>(m_Children.size());
}

CTreeListItem* CItemSearch::GetTreeListChild(const int i) const
{
    return m_Children[i];
}

HICON CItemSearch::GetIcon()
{
    // No icon to return if not visible yet
    if (m_VisualInfo == nullptr)
    {
        return nullptr;
    }

    // Return previously cached value
    if (m_VisualInfo->icon != nullptr)
    {
        return m_VisualInfo->icon;
    }

    // Cache icon for parent nodes
    if (m_Item == nullptr)
    {
        m_VisualInfo->icon = GetIconHandler()->GetFreeSpaceImage();
        return m_VisualInfo->icon;
    }

    // Fetch all other icons
    CDirStatApp::Get()->GetIconHandler()->DoAsyncShellInfoLookup(std::make_tuple(this,
        m_VisualInfo->control, m_Item->GetPath(), m_Item->GetAttributes(), &m_VisualInfo->icon, nullptr));
    return nullptr;
}

void CItemSearch::AddSearchItemChild(CItemSearch* child)
{
    child->SetParent(this);

    std::lock_guard guard(m_Protect);
    m_Children.push_back(child);

    if (IsVisible() && IsExpanded())
    {
        CFileSearchControl::Get()->OnChildAdded(this, child);
    }
}

void CItemSearch::RemoveSearchItemChild(CItemSearch* child)
{
    std::lock_guard guard(m_Protect);
    std::erase(m_Children, child);

    if (IsVisible())
    {
        CFileSearchControl::Get()->OnChildRemoved(this, child);
    }

    delete child;
}

void CItemSearch::RemoveSearchItemResults()
{
    for (const auto& m_Child : std::vector(m_Children))
    {
        RemoveSearchItemChild(m_Child);
    }
}
