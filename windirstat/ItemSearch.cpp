// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "ItemSearch.h"

static constexpr std::array columns{ COL_NAME, COL_SIZE_PHYSICAL, COL_SIZE_LOGICAL, COL_LAST_CHANGE };

std::wstring CItemSearch::GetText(const int subitem) const
{
    // Root node
    if (GetParent() == nullptr)
    {
        // Totals cover the displayed results and their folder contents, without overlapping subtrees.
        if (subitem == COL_ITEMSEARCH_SIZE_LOGICAL)
            return m_totalsPending ? std::wstring{} : FormatBytes(m_totalSizeLogical);
        if (subitem == COL_ITEMSEARCH_SIZE_PHYSICAL)
            return m_totalsPending || !m_physicalKnown ? std::wstring{} : FormatBytes(m_totalSizePhysical);
        if (subitem != COL_ITEMSEARCH_NAME) return {};

        // Mark results that were limited, cancelled, or pruned after a refresh.
        return std::format(L"{} ({}{})", Localization::Lookup(IDS_SEARCH_RESULTS),
            FormatCount(m_children.size()), m_limitExceeded ? L"+" : L"");
    }

    // Parent hash nodes
    if (m_item == nullptr) return {};

    // Individual file names
    return GetLinkedText(subitem, columns);
}

int CItemSearch::CompareSibling(const CTreeListItem* tlib, const int subitem) const
{
    // Root node
    if (GetParent() == nullptr) return 0;

    // Parent hash nodes
    if (m_item == nullptr) return 0;

    // Individual file names
    return CompareLinked(tlib, subitem, columns, true);
}

HICON CItemSearch::GetIcon()
{
    return GetWrapperIcon(GetIconHandler()->GetSearchImage());
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
    m_totalSizeLogical = totals.logical;
    m_totalSizePhysical = totals.physical;
    m_physicalKnown = totals.physicalKnown;
    m_totalsPending = false;
}

std::optional<CItemSearch::SizeTotals> CItemSearch::CalculateTotals(const std::span<CItem* const> items,
    const std::function<bool()>& isCancelled)
{
    ULONGLONG totalLogical = 0;
    ULONGLONG totalPhysical = 0;
    bool physicalKnown = true;
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
        physicalKnown &= item->HasSizePhysical();
    }
    return SizeTotals{ totalLogical, totalPhysical, physicalKnown };
}
