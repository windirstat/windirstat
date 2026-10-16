// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "ItemDedupShare.h"
#include "FileDedupShareControl.h"

// Only the name and size columns are passed on to the linked item
static constexpr std::array columns{ COL_NAME, COL_NAME, COL_NAME, COL_NAME, COL_NAME,
    COL_SIZE_PHYSICAL, COL_SIZE_LOGICAL };

static double Fraction(const ULONGLONG part, const ULONGLONG whole)
{
    // Chunks repeated within one file can push the ratio past one
    return whole == 0 ? 0.0 : std::min(1.0, static_cast<double>(part) / static_cast<double>(whole));
}

static std::wstring FormatFraction(const ULONGLONG part, const ULONGLONG whole)
{
    return whole == 0 ? std::wstring{} : FormatDouble(100.0 * Fraction(part, whole)) + L"%";
}

std::wstring CItemDedupShare::GetText(const int subitem) const
{
    // Root node
    if (GetParent() == nullptr)
    {
        switch (subitem)
        {
        case COL_ITEMDEDUPSHARE_NAME:
        {
            // The title is empty while a refresh recomputes the query
            std::wstring text = m_title.empty() ?
                std::format(L"{} ({})", Localization::Lookup(IDS_DEDUP_SHARING), FormatCount(m_children.size())) :
                std::format(L"{}: {} ({})", Localization::Lookup(IDS_DEDUP_SHARING), m_title, FormatCount(m_children.size()));
            if (m_outsideBytes > 0) text += L" " + Localization::Format(IDS_DEDUP_SHARING_OUTSIDE, FormatBytes(m_outsideBytes));
            return text;
        }
        case COL_ITEMDEDUPSHARE_SHARED: return m_title.empty() ? std::wstring{} : FormatBytes(m_sharedBytes);
        case COL_ITEMDEDUPSHARE_PCT_QUERIED: return FormatFraction(m_sharedBytes, m_queriedBytes);
        case COL_ITEMDEDUPSHARE_ON_DISK: return m_title.empty() ? std::wstring{} : FormatBytes(m_queriedBytes);
        default: return {};
        }
    }

    if (m_item == nullptr) return {};
    switch (subitem)
    {
    case COL_ITEMDEDUPSHARE_SHARED: return FormatBytes(m_sharedBytes);
    case COL_ITEMDEDUPSHARE_PCT_QUERIED: return FormatFraction(m_sharedBytes, GetQueriedBytes());
    case COL_ITEMDEDUPSHARE_PCT_FILE: return FormatFraction(m_sharedBytes, m_fileBytes);
    case COL_ITEMDEDUPSHARE_ON_DISK: return FormatBytes(m_fileBytes);
    default: return GetLinkedText(subitem, columns);
    }
}

int CItemDedupShare::CompareSibling(const CTreeListItem* tlib, const int subitem) const
{
    // Root node
    if (GetParent() == nullptr || m_item == nullptr) return 0;

    const auto* other = static_cast<const CItemDedupShare*>(tlib);
    switch (subitem)
    {
    case COL_ITEMDEDUPSHARE_SHARED: return usignum(m_sharedBytes, other->m_sharedBytes);
    case COL_ITEMDEDUPSHARE_PCT_QUERIED:
        return usignum(Fraction(m_sharedBytes, GetQueriedBytes()), Fraction(other->m_sharedBytes, other->GetQueriedBytes()));
    case COL_ITEMDEDUPSHARE_PCT_FILE:
        return usignum(Fraction(m_sharedBytes, m_fileBytes), Fraction(other->m_sharedBytes, other->m_fileBytes));
    case COL_ITEMDEDUPSHARE_ON_DISK: return usignum(m_fileBytes, other->m_fileBytes);
    default: return CompareLinked(tlib, subitem, columns, true);
    }
}

HICON CItemDedupShare::GetIcon()
{
    return GetWrapperIcon(GetIconHandler()->GetDedupSharedImage());
}

void CItemDedupShare::SetSummary(const std::wstring& title, const ULONGLONG queriedBytes,
    const ULONGLONG sharedBytes, const ULONGLONG outsideBytes)
{
    m_title = title;
    m_queriedBytes = queriedBytes;
    m_sharedBytes = sharedBytes;
    m_outsideBytes = outsideBytes;
}

void CItemDedupShare::AddShareItemChild(CItemDedupShare* child)
{
    child->SetParent(this);

    std::scoped_lock guard(m_protect);
    m_children.push_back(child);

    if (IsVisible() && IsExpanded())
    {
        CFileDedupShareControl::Get()->OnChildAdded(this, child);
    }
}

void CItemDedupShare::RemoveShareItemChild(CItemDedupShare* child)
{
    if (IsVisible())
    {
        CFileDedupShareControl::Get()->OnChildRemoved(this, child);
    }

    std::scoped_lock guard(m_protect);
    if (const auto it = std::ranges::find(m_children, child); it != m_children.end())
    {
        m_children.erase(it);
    }

    delete child;
}
