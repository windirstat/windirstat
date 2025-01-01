// ItemDupe.cpp - Implementation of CItemDupe
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
#include "GlobalHelpers.h"
#include "Localization.h"

#include <functional>
#include <queue>

CItemDupe::CItemDupe(const std::wstring& hash, const ULONGLONG sizePhysical, const ULONGLONG sizeLogical) :
    m_Hash(hash), m_SizePhysical(sizePhysical), m_SizeLogical(sizeLogical)
{
    DWORD maxSize = sizeof(m_HashComp);
    CryptStringToBinary(hash.c_str(), sizeof(m_HashComp) * 2, CRYPT_STRING_HEXRAW,
        reinterpret_cast<PBYTE>(&m_HashComp), &maxSize, nullptr, nullptr);
    m_HashComp = _byteswap_uint64(m_HashComp);
}

CItemDupe::CItemDupe(CItem* item) : m_Item(item) {}

CItemDupe::~CItemDupe()
{
    for (const auto& m_Child : m_Children)
    {
        delete m_Child;
    }
}

bool CItemDupe::DrawSubItem(const int subitem, CDC* pdc, const CRect rc, const UINT state, int* width, int* focusLeft)
{
    // Handle individual file items
    if (subitem != COL_ITEMDUP_NAME) return false;
    return CTreeListItem::DrawSubItem(columnMap.at(subitem), pdc, rc, state, width, focusLeft);
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
        if (subitem == COL_ITEMDUP_NAME) return m_Hash;
        if (subitem == COL_ITEMDUP_SIZE_PHYSICAL) return FormatBytes(m_SizePhysical * GetChildren().size());
        if (subitem == COL_ITEMDUP_SIZE_LOGICAL) return FormatBytes(m_SizeLogical * GetChildren().size());
        if (subitem == COL_ITEMDUP_ITEMS) return FormatCount(GetChildren().size());
        return {};
    }

    // Individual file names
    if (subitem == COL_ITEMDUP_NAME) return m_Item->GetPath();
    return m_Item->GetText(columnMap.at(subitem));
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
        if (subitem == COL_ITEMDUP_NAME) return usignum(m_HashComp, other->m_HashComp);
        if (subitem == COL_ITEMDUP_SIZE_PHYSICAL) return usignum(m_SizePhysical * m_Children.size(), other->m_SizePhysical * other->m_Children.size());
        if (subitem == COL_ITEMDUP_SIZE_LOGICAL) return usignum(m_SizeLogical * m_Children.size(), other->m_SizeLogical * other->m_Children.size());
        if (subitem == COL_ITEMDUP_ITEMS) return usignum(m_Children.size(), other->m_Children.size());
        return 0;
    }

    // Individual file names
    return m_Item->CompareSibling(other->m_Item, columnMap.at(subitem));
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

const std::vector<CItemDupe*>& CItemDupe::GetChildren() const
{
    return m_Children;
}

void CItemDupe::AddDupeItemChild(CItemDupe* child)
{
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
    std::lock_guard guard(m_Protect);
    std::erase(m_Children, child);

    if (IsVisible())
    {
        CFileDupeControl::Get()->OnChildRemoved(this, child);
    }

    delete child;
}
