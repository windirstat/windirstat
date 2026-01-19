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
#include "ItemDupe.h"
#include "FileDupeControl.h"

CItemDupe::CItemDupe(const std::vector<BYTE>& hash) :
    m_hashString(FormatHex(hash, false)), m_hash(hash)
{
}

CItemDupe::CItemDupe(CItem* item) : m_item(item) {}

CItemDupe::~CItemDupe()
{
    for (const auto& m_child : m_children)
    {
        delete m_child;
    }
}

const std::unordered_map<uint8_t, uint8_t> CItemDupe::s_columnMap =
{
    { COL_ITEMDUP_NAME, COL_NAME },
    { COL_ITEMDUP_ITEMS, COL_ITEMS },
    { COL_ITEMDUP_SIZE_LOGICAL, COL_SIZE_LOGICAL },
    { COL_ITEMDUP_SIZE_PHYSICAL, COL_SIZE_PHYSICAL },
    { COL_ITEMDUP_LAST_CHANGE, COL_LAST_CHANGE }
};

bool CItemDupe::DrawSubItem(const int subitem, CDC* pdc, const CRect rc, const UINT state, int* width, int* focusLeft)
{
    // Handle individual file items
    if (subitem != COL_ITEMDUP_NAME) return false;
    return CTreeListItem::DrawSubItem(s_columnMap.at(static_cast<uint8_t>(subitem)), pdc, rc, state, width, focusLeft);
}

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
    if (subitem == COL_ITEMDUP_NAME) return m_item->GetPath();
    return m_item->GetText(s_columnMap.at(static_cast<uint8_t>(subitem)));
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
        if (subitem == COL_ITEMDUP_NAME) return memcmp(m_hash.data(), other->m_hash.data(), m_hash.size());
        if (subitem == COL_ITEMDUP_SIZE_PHYSICAL) return usignum(m_sizePhysical, other->m_sizePhysical);
        if (subitem == COL_ITEMDUP_SIZE_LOGICAL) return usignum(m_sizeLogical, other->m_sizeLogical);
        if (subitem == COL_ITEMDUP_ITEMS) return usignum(m_children.size(), other->m_children.size());
        return 0;
    }

    // Individual file names
    return m_item->CompareSibling(other->m_item, s_columnMap.at(static_cast<uint8_t>(subitem)));
}

int CItemDupe::GetTreeListChildCount()const
{
    return static_cast<int>(m_children.size());
}

CTreeListItem* CItemDupe::GetTreeListChild(const int i) const
{
    return m_children[i];
}

HICON CItemDupe::GetIcon()
{
    // Return generic node for parent nodes
    if (m_item == nullptr || m_visualInfo == nullptr)
    {
        return GetIconHandler()->GetDupesImage();
    }

    // Return previously cached value
    if (m_visualInfo->icon != nullptr)
    {
        return m_visualInfo->icon;
    }

    // Fetch all other icons
    CDirStatApp::Get()->GetIconHandler()->DoAsyncShellInfoLookup(std::make_tuple(this,
        m_visualInfo->control, m_item->GetPath(), m_item->GetAttributes(), &m_visualInfo->icon, nullptr));
    return nullptr;
}

const std::vector<CItemDupe*>& CItemDupe::GetChildren() const
{
    return m_children;
}

std::wstring CItemDupe::GetHashAndExtensions() const
{
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
    return m_hashString + L" (" + extensions + L")";
}

void CItemDupe::AddDupeItemChild(CItemDupe* child)
{
    // Adjust parent item sizes
    if (const auto childItem = child->GetLinkedItem(); childItem != nullptr)
    {
        m_sizeLogical += childItem->GetSizeLogical();
        m_sizePhysical += childItem->GetSizePhysical();
    }

    child->SetParent(this);

    std::scoped_lock guard(m_protect);
    m_children.push_back(child);

    if (IsVisible() && IsExpanded())
    {
        CFileDupeControl::Get()->OnChildAdded(this, child);
    }
}

void CItemDupe::RemoveDupeItemChild(CItemDupe* child)
{
    if (IsVisible())
    {
        CFileDupeControl::Get()->OnChildRemoved(this, child);
    }

    // Adjust parent item sizes
    std::scoped_lock guard(m_protect);
    if (const auto childItem = child->GetLinkedItem(); childItem != nullptr)
    {
        m_sizeLogical -= childItem->GetSizeLogical();
        m_sizePhysical -= childItem->GetSizePhysical();
    }

    auto& children = m_children;
    if (const auto it = std::ranges::find(children, child); it != children.end())
    {
        children.erase(it);
    }
    delete child;
}
