// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
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
#include "GlobalHelpers.h"
#include "Localization.h"

#include <functional>
#include <queue>
#include <format>

CItemDupe::CItemDupe(const std::vector<BYTE>& hash) : m_Hash(hash)
{
    m_HashString.resize(2ull * m_Hash.size());
    DWORD iHashStringLength = static_cast<DWORD>(m_HashString.size() + 1ull);
    CryptBinaryToStringW(m_Hash.data(), static_cast<DWORD>(m_Hash.size()),
        CRYPT_STRING_HEXRAW | CRYPT_STRING_NOCRLF, m_HashString.data(), &iHashStringLength);
}

CItemDupe::CItemDupe(CItem* item) : m_Item(item) {}

CItemDupe::~CItemDupe()
{
    for (const auto& m_Child : m_Children)
    {
        delete m_Child;
    }
}

const std::unordered_map<uint8_t, uint8_t> CItemDupe::columnMap =
{
    { COL_ITEMDUP_NAME, COL_NAME },
    { COL_ITEMDUP_ITEMS, COL_ITEMS },
    { COL_ITEMDUP_SIZE_LOGICAL, COL_SIZE_LOGICAL },
    { COL_ITEMDUP_SIZE_PHYSICAL, COL_SIZE_PHYSICAL },
    { COL_ITEMDUP_LASTCHANGE, COL_LASTCHANGE }
};

bool CItemDupe::DrawSubItem(const int subitem, CDC* pdc, const CRect rc, const UINT state, int* width, int* focusLeft)
{
    // Handle individual file items
    if (subitem != COL_ITEMDUP_NAME) return false;
    return CTreeListItem::DrawSubItem(columnMap.at(static_cast<uint8_t>(subitem)), pdc, rc, state, width, focusLeft);
}

std::wstring CItemDupe::GetText(const int subitem) const
{
    // Root node
    static std::wstring duplicates = Localization::Lookup(IDS_DUPLICATE_FILES);
    if (GetParent() == nullptr) return subitem == COL_ITEMDUP_NAME ? duplicates : std::wstring{};

    // Parent hash nodes
    if (m_Item == nullptr)
    {
        // Handle top-level hash collection nodes
        if (subitem == COL_ITEMDUP_NAME) return GetHashAndExtensions();
        if (subitem == COL_ITEMDUP_SIZE_PHYSICAL) return FormatBytes(m_SizePhysical);
        if (subitem == COL_ITEMDUP_SIZE_LOGICAL) return FormatBytes(m_SizeLogical);
        if (subitem == COL_ITEMDUP_ITEMS) return FormatCount(GetChildren().size());
        return {};
    }

    // Individual file names
    if (subitem == COL_ITEMDUP_NAME) return m_Item->GetPath();
    return m_Item->GetText(columnMap.at(static_cast<uint8_t>(subitem)));
}

int CItemDupe::CompareSibling(const CTreeListItem* tlib, const int subitem) const
{
    // Root node
    if (GetParent() == nullptr) return 0;
    
    // Parent hash nodes
    const auto* other = reinterpret_cast<const CItemDupe*>(tlib);
    if (m_Item == nullptr)
    {
        // Handle top-level hash collection nodes
        if (subitem == COL_ITEMDUP_NAME) return memcmp(m_Hash.data(), other->m_Hash.data(), m_Hash.size());
        if (subitem == COL_ITEMDUP_SIZE_PHYSICAL) return usignum(m_SizePhysical, other->m_SizePhysical);
        if (subitem == COL_ITEMDUP_SIZE_LOGICAL) return usignum(m_SizeLogical, other->m_SizeLogical);
        if (subitem == COL_ITEMDUP_ITEMS) return usignum(m_Children.size(), other->m_Children.size());
        return 0;
    }

    // Individual file names
    return m_Item->CompareSibling(other->m_Item, columnMap.at(static_cast<uint8_t>(subitem)));
}

int CItemDupe::GetTreeListChildCount()const
{
    return static_cast<int>(m_Children.size());
}

CTreeListItem* CItemDupe::GetTreeListChild(const int i) const
{
    return m_Children[i];
}

HICON CItemDupe::GetIcon()
{
    // Return generic node for parent nodes
    if (m_Item == nullptr || m_VisualInfo == nullptr)
    {
        return GetIconHandler()->GetFreeSpaceImage();
    }

    // Return previously cached value
    if (m_VisualInfo->icon != nullptr)
    {
        return m_VisualInfo->icon;
    }

    // Fetch all other icons
    CDirStatApp::Get()->GetIconHandler()->DoAsyncShellInfoLookup(std::make_tuple(this,
        m_VisualInfo->control, m_Item->GetPath(), m_Item->GetAttributes(), &m_VisualInfo->icon, nullptr));
    return nullptr;
}

const std::vector<CItemDupe*>& CItemDupe::GetChildren() const
{
    return m_Children;
}

std::wstring CItemDupe::GetHashAndExtensions() const
{
    // Create set of unique extensions
    std::unordered_set<std::wstring> extensionsSet;
    for (const auto& child : m_Children)
    {
        const auto & ext = child->m_Item->GetExtension();
        if (ext.empty()) extensionsSet.emplace(L".???");
        else extensionsSet.emplace(ext);
    }

    // Create list of comma and space delimitered extensions
    std::wstring extensions;
    for (const auto& extension : extensionsSet)
    {
        // Limit number of characters to reasonable amount
        if (extension.size() +
            extensions.size() > 128) break;

        extensions.append(extension);
        extensions.append(L", ");
    }

    // Remove the last two delimeters
    if (!extensions.empty())
    {
        extensions.pop_back();
        extensions.pop_back();
    }

    // Format string as Hash (.exta, .extb)
    return std::format(L"{} ({})", m_HashString, extensions);
}

void CItemDupe::AddDupeItemChild(CItemDupe* child)
{
    // Adjust parent item sizes
    if (const auto childItem = reinterpret_cast<CItem*>(child->GetLinkedItem()); childItem != nullptr)
    {
        m_SizeLogical += childItem->GetSizeLogical();
        m_SizePhysical += childItem->GetSizePhysical();
    }

    child->SetParent(this);

    std::lock_guard guard(m_Protect);
    m_Children.push_back(child);

    if (IsVisible() && IsExpanded())
    {
        CFileDupeControl::Get()->OnChildAdded(this, child);
    }
}

void CItemDupe::RemoveDupeItemChild(CItemDupe* child)
{
    // Adjust parent item sizes
    if (const auto childItem = reinterpret_cast<CItem*>(child->GetLinkedItem()); childItem != nullptr)
    {
        m_SizeLogical -= childItem->GetSizeLogical();
        m_SizePhysical -= childItem->GetSizePhysical();
    }

    std::lock_guard guard(m_Protect);
    std::erase(m_Children, child);

    if (IsVisible())
    {
        CFileDupeControl::Get()->OnChildRemoved(this, child);
    }

    delete child;
}
