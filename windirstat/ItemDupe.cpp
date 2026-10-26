// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "ItemDupe.h"
#include "FileDupeControl.h"

CItemDupe::CItemDupe(const std::vector<BYTE>& hash, const bool sampled) :
    m_hashString(FormatHex(hash, false)), m_sampled(sampled)
{
}

static constexpr std::array columns{ COL_NAME, COL_ITEMS, COL_SIZE_PHYSICAL, COL_SIZE_LOGICAL, COL_LAST_CHANGE };

std::wstring CItemDupe::GetText(const int subitem) const
{
    // Root node
    static std::wstring duplicates = Localization::Lookup(IDS_DUPLICATE_FILES);
    if (GetParent() == nullptr) return subitem == COL_ITEMDUP_NAME ? duplicates : std::wstring{};

    // Parent hash nodes
    if (m_item == nullptr)
    {
        // Handle top-level hash collection nodes
        if (subitem == COL_ITEMDUP_NAME) return GetHashAndExtensions();
        if (subitem == COL_ITEMDUP_SIZE_PHYSICAL) return FormatBytes(m_sizePhysical);
        if (subitem == COL_ITEMDUP_SIZE_LOGICAL) return FormatBytes(m_sizeLogical);
        if (subitem == COL_ITEMDUP_ITEMS) return FormatCount(GetChildren().size());
        return {};
    }

    // Individual file names
    return GetLinkedText(subitem, columns);
}

int CItemDupe::CompareSibling(const CTreeListItem* tlib, const int subitem) const
{
    // Root node
    if (GetParent() == nullptr) return 0;

    // Parent hash nodes
    const auto* other = reinterpret_cast<const CItemDupe*>(tlib);
    if (m_item == nullptr)
    {
        // Handle top-level hash collection nodes
        // Sibling comparisons must return -1/0/1 for sort direction handling
        if (subitem == COL_ITEMDUP_NAME) return signum(m_hashString.compare(other->m_hashString));
        if (subitem == COL_ITEMDUP_SIZE_PHYSICAL) return usignum(m_sizePhysical, other->m_sizePhysical);
        if (subitem == COL_ITEMDUP_SIZE_LOGICAL) return usignum(m_sizeLogical, other->m_sizeLogical);
        if (subitem == COL_ITEMDUP_ITEMS) return usignum(m_children.size(), other->m_children.size());
        return 0;
    }

    // Individual file names
    return CompareLinked(tlib, subitem, columns);
}

HICON CItemDupe::GetIcon()
{
    // Group rows use the duplicate icon even before they become visible.
    if (m_item == nullptr || GetViewState() == nullptr) return GetIconHandler()->GetDupesImage();
    return GetWrapperIcon(GetIconHandler()->GetDupesImage());
}

std::wstring CItemDupe::GetHashAndExtensions() const
{
    if (!m_captionDirty) return m_caption;

    // Create set of unique extensions
    std::unordered_set<std::wstring> extensionsSet;
    for (const auto& child : m_children)
    {
        const auto & ext = child->m_item->GetExtension();
        if (ext.empty()) extensionsSet.emplace(L".???");
        else extensionsSet.emplace(ext);
    }

    // Create list of comma and space delimited extensions
    std::wstring extensions;
    for (const auto& extension : extensionsSet)
    {
        // Limit number of characters to reasonable amount
        if (extension.size() +
            extensions.size() > 128) break;

        extensions.append(extension);
        extensions.append(L", ");
    }

    // Remove the last two delimiters
    if (!extensions.empty())
    {
        extensions.pop_back();
        extensions.pop_back();
    }

    // Format string as Hash (.exta, .extb)
    m_caption = m_hashString + L" (" + extensions + L")";
    if (m_sampled) m_caption = Localization::Lookup(IDS_PROBABLE_DUPLICATES) + L": " + m_caption;
    m_captionDirty = false;
    return m_caption;
}

void CItemDupe::AddDupeItemChildren(std::span<CItemDupe* const> children)
{
    if (children.empty()) return;

    std::scoped_lock guard(m_protect);

    // Adjust parent item sizes
    for (CItemDupe* child : children)
    {
        if (const auto childItem = child->GetLinkedItem(); childItem != nullptr)
        {
            m_sizeLogical += childItem->GetSizeLogical();
            m_sizePhysical += childItem->GetSizePhysical();
        }
        child->SetParent(this);
    }

    m_children.append_range(children);
    m_captionDirty = true;
    if (!IsVisible() || !IsExpanded()) return;

    // Publish visible rows with a single list insertion.
    const std::vector<CTreeListItem*> rows(children.begin(), children.end());
    CFileDupeControl::Get()->OnChildrenAdded(this, rows);
}

void CItemDupe::RemoveDupeItemChildren(const std::unordered_set<const CTreeListItem*>& children)
{
    // The control detaches all affected rows before pruning groups and their children.
    std::scoped_lock guard(m_protect);
    std::erase_if(m_children, [&](CItemDupe* child)
    {
        if (!children.contains(child)) return false;
        if (const auto childItem = child->GetLinkedItem(); childItem != nullptr)
        {
            m_sizeLogical -= childItem->GetSizeLogical();
            m_sizePhysical -= childItem->GetSizePhysical();
        }
        delete child;
        m_captionDirty = true;
        return true;
    });
}
