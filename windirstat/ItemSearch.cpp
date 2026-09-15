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

#include "pch.h"
#include "ItemSearch.h"

CItemSearch::CItemSearch(CItem* item) : m_item(item) {}

CItemSearch::~CItemSearch()
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
    case COL_ITEMSEARCH_NAME: return COL_NAME;
    case COL_ITEMSEARCH_SIZE_LOGICAL: return COL_SIZE_LOGICAL;
    case COL_ITEMSEARCH_SIZE_PHYSICAL: return COL_SIZE_PHYSICAL;
    case COL_ITEMSEARCH_LAST_CHANGE: return COL_LAST_CHANGE;
    default: return -1;
    }
}

bool CItemSearch::DrawSubItem(const int subitem, CDC* pdc, const CRect rc, const UINT state, int* width, int* focusLeft)
{
    // Handle individual file items
    if (subitem != COL_ITEMSEARCH_NAME) return false;
    return CTreeListItem::DrawSubItem(COL_NAME, pdc, rc, state, width, focusLeft);
}

std::wstring CItemSearch::GetText(const int subitem) const
{
    // Root node
    if (GetParent() == nullptr)
    {
        // Totals cover the displayed results and their folder contents, without overlapping subtrees.
        if (subitem == COL_ITEMSEARCH_SIZE_LOGICAL)
            return m_totalsPending ? std::wstring{} : FormatBytes(m_totalSizeLogical);
        if (subitem == COL_ITEMSEARCH_SIZE_PHYSICAL)
            return m_totalsPending ? std::wstring{} : FormatBytes(m_totalSizePhysical);
        if (subitem != COL_ITEMSEARCH_NAME) return {};

        // Mark results that were limited, cancelled, or pruned after a refresh.
        return std::format(L"{} ({}{})", Localization::Lookup(IDS_SEARCH_RESULTS),
            FormatCount(m_children.size()), m_limitExceeded ? L"+" : L"");
    }

    // Parent hash nodes
    if (m_item == nullptr) return {};

    // Individual file names
    if (subitem == COL_ITEMSEARCH_NAME) return m_item->GetPath();
    const int mapped = GetMappedColumn(subitem);
    return mapped != -1 ? m_item->GetText(mapped) : std::wstring{};
}

int CItemSearch::CompareSibling(const CTreeListItem* tlib, const int subitem) const
{
    // Root node
    if (GetParent() == nullptr) return 0;

    // Parent hash nodes
    if (m_item == nullptr) return 0;

    // Individual file names
    const auto* other = reinterpret_cast<const CItemSearch*>(tlib);
    if (subitem == COL_ITEMSEARCH_NAME) return m_item->ComparePath(other->m_item);

    const int mapped = GetMappedColumn(subitem);
    return mapped != -1 ? m_item->CompareSibling(other->m_item, mapped) : 0;
}

HICON CItemSearch::GetIcon()
{
    auto* viewState = GetViewState();

    // No icon to return if not visible yet
    if (viewState == nullptr) return nullptr;

    if (viewState->icon != nullptr) return viewState->icon;

    // Cache icon for parent nodes
    if (m_item == nullptr)
    {
        viewState->icon = GetIconHandler()->GetSearchImage();
        return viewState->icon;
    }

    // Fetch all other icons
    CDirStatApp::Get()->GetIconHandler()->DoAsyncShellInfoLookup(std::make_tuple(this,
        viewState->control, m_item->GetPath(), m_item->GetAttributes(), &viewState->icon, nullptr));
    return viewState->icon;
}

void CItemSearch::AddSearchItemChild(CItemSearch* child)
{
    child->SetParent(this);

    std::scoped_lock guard(m_protect);
    m_children.push_back(child);
    m_totalsPending = true;

    if (IsVisible() && IsExpanded())
    {
        CFileSearchControl::Get()->OnChildAdded(this, child);
    }
}

void CItemSearch::RemoveSearchItemChild(CItemSearch* child)
{
    if (IsVisible())
    {
        CFileSearchControl::Get()->OnChildRemoved(this, child);
    }

    std::scoped_lock guard(m_protect);
    auto& children = m_children;
    if (const auto it = std::ranges::find(children, child); it != children.end())
    {
        children.erase(it);
        m_totalsPending = true;
        m_limitExceeded = true;
    }

    delete child;
}

CItemSearch::SizeTotals CItemSearch::CalculateTotals() const
{
    std::vector<CItem*> items;
    for (const auto* child : m_children)
        if (child->m_item != nullptr) items.push_back(child->m_item);
    return *CalculateTotals(items);
}

void CItemSearch::SetTotals(const SizeTotals& totals)
{
    std::tie(m_totalSizeLogical, m_totalSizePhysical) = totals;
    m_totalsPending = false;
}

std::optional<CItemSearch::SizeTotals> CItemSearch::CalculateTotals(const std::span<CItem* const> items,
    const std::function<bool()>& isCancelled)
{
    ULONGLONG totalLogical = 0;
    ULONGLONG totalPhysical = 0;
    const std::unordered_set<const CItem*> matches(items.begin(), items.end());

    // Count only the outermost matching items, so nested results contribute once.
    for (const auto* item : matches)
    {
        if (isCancelled && isCancelled()) return std::nullopt;
        auto* parent = item->GetParent();
        while (parent != nullptr && !matches.contains(parent)) parent = parent->GetParent();
        if (parent != nullptr) continue;
        totalLogical += item->GetSizeLogical();
        totalPhysical += item->GetSizePhysical();
    }
    return SizeTotals{ totalLogical, totalPhysical };
}
