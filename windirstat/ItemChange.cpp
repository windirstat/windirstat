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
#include "ItemChange.h"
#include "FileChangeControl.h"

namespace
{
    ULONGLONG Magnitude(const LONGLONG value)
    {
        return value >= 0 ? static_cast<ULONGLONG>(value) : static_cast<ULONGLONG>(-(value + 1)) + 1;
    }

    std::wstring FormatSignedBytes(const LONGLONG value)
    {
        if (value == 0) return FormatBytes(0);
        return (value > 0 ? L"+" : L"-") + FormatBytes(Magnitude(value));
    }

    std::wstring FormatSignedCountValue(const LONGLONG value)
    {
        if (value == 0) return L"0";
        return value > 0 ? L"+" + FormatCount(value) : L"-" + FormatCount(Magnitude(value));
    }
}

CItemChange::CItemChange(const SnapshotGrowthEntry& entry) : m_entry(entry) {}

CItemChange::~CItemChange()
{
    for (const auto& child : m_children)
    {
        delete child;
    }
}

bool CItemChange::DrawSubItem(const int subitem, CDC* pdc, const CRect rc, const UINT state, int* width, int* focusLeft)
{
    return subitem == COL_ITEMCHANGE_NAME ? CTreeListItem::DrawSubItem(subitem, pdc, rc, state, width, focusLeft) : false;
}

std::wstring CItemChange::GetText(const int subitem) const
{
    if (GetParent() == nullptr)
    {
        if (subitem != COL_ITEMCHANGE_NAME) return {};

        std::wstring text = Localization::Lookup(IDS_CHANGES);
        if (!m_previousSnapshotLabel.empty()) text += L" (" + m_previousSnapshotLabel + L", " + FormatCount(m_children.size()) + L")";
        return text;
    }

    if (!m_entry.has_value()) return {};

    switch (subitem)
    {
    case COL_ITEMCHANGE_NAME:
        return m_entry->path;
    case COL_ITEMCHANGE_DELTA:
        return FormatSignedBytes(m_entry->deltaSizePhysical);
    case COL_ITEMCHANGE_CURRENT_SIZE:
        return FormatBytes(m_entry->currentSizePhysical);
    case COL_ITEMCHANGE_PREVIOUS_SIZE:
        return FormatBytes(m_entry->previousSizePhysical);
    case COL_ITEMCHANGE_FILES_DELTA:
        return FormatSignedCountValue(m_entry->deltaFiles);
    case COL_ITEMCHANGE_FOLDERS_DELTA:
        return FormatSignedCountValue(m_entry->deltaFolders);
    default:
        return {};
    }
}

int CItemChange::CompareSibling(const CTreeListItem* tlib, const int subitem) const
{
    if (GetParent() == nullptr || !m_entry.has_value()) return 0;

    const auto* other = reinterpret_cast<const CItemChange*>(tlib);
    if (other == nullptr || !other->m_entry.has_value()) return 0;

    switch (subitem)
    {
    case COL_ITEMCHANGE_DELTA:
        return signum(m_entry->deltaSizePhysical - other->m_entry->deltaSizePhysical);
    case COL_ITEMCHANGE_CURRENT_SIZE:
        return usignum(m_entry->currentSizePhysical, other->m_entry->currentSizePhysical);
    case COL_ITEMCHANGE_PREVIOUS_SIZE:
        return usignum(m_entry->previousSizePhysical, other->m_entry->previousSizePhysical);
    case COL_ITEMCHANGE_FILES_DELTA:
        return signum(m_entry->deltaFiles - other->m_entry->deltaFiles);
    case COL_ITEMCHANGE_FOLDERS_DELTA:
        return signum(m_entry->deltaFolders - other->m_entry->deltaFolders);
    case COL_ITEMCHANGE_NAME:
    default:
        return signum(_wcsicmp(m_entry->path.c_str(), other->m_entry->path.c_str()));
    }
}

int CItemChange::GetTreeListChildCount() const
{
    return static_cast<int>(m_children.size());
}

CTreeListItem* CItemChange::GetTreeListChild(const int i) const
{
    return m_children[i];
}

HICON CItemChange::GetIcon()
{
    if (m_visualInfo == nullptr) return nullptr;
    if (m_visualInfo->icon != nullptr) return m_visualInfo->icon;

    if (!m_entry.has_value() || m_entry->currentItem == nullptr)
    {
        m_visualInfo->icon = GetIconHandler()->GetSearchImage();
        return m_visualInfo->icon;
    }

    CDirStatApp::Get()->GetIconHandler()->DoAsyncShellInfoLookup(std::make_tuple(this,
        m_visualInfo->control, m_entry->currentItem->GetPath(), m_entry->currentItem->GetAttributes(), &m_visualInfo->icon, nullptr));
    return nullptr;
}

void CItemChange::AddChangeItemChild(CItemChange* child)
{
    child->SetParent(this);

    std::scoped_lock guard(m_protect);
    m_children.push_back(child);

    if (IsVisible() && IsExpanded()) CFileChangeControl::Get()->OnChildAdded(this, child);
}

void CItemChange::ReserveChangeItemChildren(const size_t count)
{
    std::scoped_lock guard(m_protect);
    m_children.reserve(count);
}

void CItemChange::RemoveChangeItemChild(CItemChange* child)
{
    if (IsVisible()) CFileChangeControl::Get()->OnChildRemoved(this, child);

    std::scoped_lock guard(m_protect);
    if (const auto it = std::ranges::find(m_children, child); it != m_children.end())
    {
        m_children.erase(it);
    }

    delete child;
}
