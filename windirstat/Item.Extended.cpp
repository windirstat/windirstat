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
// #include "pch.h"

#include "pch.h"
#include "Item.h"

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

// --- CTreeListItem Interface ---

bool CItem::DrawSubItem(const int subitem, CDC* pdc, CRect rc, const UINT state, int* width, int* focusLeft)
{
    if (subitem == COL_NAME)
    {
        return CTreeListItem::DrawSubItem(subitem, pdc, rc, state, width, focusLeft);
    }

    if (subitem != COL_SUBTREE_PERCENTAGE)
    {
        return false;
    }

    const bool showReadJobs = MustShowReadJobs();

    if (showReadJobs && !COptions::PacmanAnimation)
    {
        return false;
    }

    if (showReadJobs && IsDone())
    {
        return false;
    }

    if (width != nullptr)
    {
        *width = GetSubtreePercentageWidth();
        return true;
    }

    DrawSelection(CFileTreeControl::Get(), pdc, rc, state);

    if (showReadJobs)
    {
        constexpr SIZE sizeDeflatePacman = { 1, 2 };
        rc.DeflateRect(sizeDeflatePacman);
        DrawPacman(pdc, rc);
    }
    else
    {
        rc.DeflateRect(2, 5);
        rc.left += GetIndent() * rc.Width() / 10;

        DrawPercentage(pdc, rc, GetFraction(), GetPercentageColor());
    }
    return true;
}

std::wstring CItem::GetText(const int subitem) const
{
    switch (subitem)
    {
    case COL_SIZE_PHYSICAL:
        if (IsTypeOrFlag(ITF_HARDLINK))
        {
            return std::wstring(L"⧉ ") + FormatBytes(GetSizePhysicalRaw());
        }
        if (IsTypeOrFlag(IT_HLINKS_FILE))
        {
            return std::wstring(L"⫘ ") + FormatBytes(GetSizePhysical());
        }
        return FormatBytes(GetSizePhysical());

    case COL_SIZE_LOGICAL:
    {
        return FormatBytes(GetSizeLogical());
    }

    case COL_NAME:
        if (IsTypeOrFlag(IT_DRIVE))
        {
            return std::wstring(GetNameView().substr(std::size(L"?:")));
        }
        return GetName();

    case COL_OWNER:
        if (IsTypeOrFlag(IT_FILE, IT_DIRECTORY))
        {
            return GetOwner();
        }
        break;

    case COL_SUBTREE_PERCENTAGE:
        if (!IsDone())
        {
            if (GetReadJobs() == 1)
            {
                return Localization::Lookup(IDS_ONEREADJOB);
            }

            return Localization::Format(IDS_sREADJOBS, FormatCount(GetReadJobs()));
        }
        break;

    case COL_PERCENTAGE:
        if (COptions::ShowTimeSpent && MustShowReadJobs() || IsRootItem())
        {
            return L"[" + FormatMilliseconds(GetTicksWorked() * 1000) + L"]";
        }
        return FormatDouble(GetFraction() * 100) + L"%";

    case COL_ITEMS:
        if (!IsTypeOrFlag(IT_FILE, IT_FREESPACE, IT_UNKNOWN, IT_HLINKS, IT_HLINKS_SET, IT_HLINKS_IDX))
        {
            return FormatCount(GetItemsCount());
        }
        break;

    case COL_FILES:
        if (!IsTypeOrFlag(IT_FILE, IT_FREESPACE, IT_UNKNOWN, IT_HLINKS, IT_HLINKS_SET, IT_HLINKS_IDX))
        {
            return FormatCount(GetFilesCount());
        }
        break;

    case COL_FOLDERS:
        if (!IsTypeOrFlag(IT_FILE, IT_FREESPACE, IT_UNKNOWN, IT_HLINKS, IT_HLINKS_SET, IT_HLINKS_IDX))
        {
            return FormatCount(GetFoldersCount());
        }
        break;

    case COL_LAST_CHANGE:
        if (!IsTypeOrFlag(IT_FREESPACE, IT_UNKNOWN, IT_HLINKS, IT_HLINKS_SET, IT_HLINKS_IDX))
        {
            return FormatFileTime(m_lastChange);
        }
        break;

    case COL_ATTRIBUTES:
        if (!IsTypeOrFlag(IT_FREESPACE, IT_UNKNOWN, IT_MYCOMPUTER, IT_HLINKS, IT_HLINKS_SET, IT_HLINKS_IDX, IT_HLINKS_FILE))
        {
            return FormatAttributes(GetAttributes());
        }
        break;

    default: ASSERT(FALSE);
    }

    return {};
}

COLORREF CItem::GetItemTextColor() const
{
    // Get the file/folder attributes
    const DWORD attr = GetAttributes();

    // This happens e.g. on a Unicode-capable FS when using ANSI APIs
    // to list files with ("real") Unicode names
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        return CTreeListItem::GetItemTextColor();
    }

    // Check for compressed flag
    if (attr & FILE_ATTRIBUTE_COMPRESSED)
    {
        return CDirStatApp::Get()->AltColor();
    }

    if (attr & FILE_ATTRIBUTE_ENCRYPTED)
    {
        return CDirStatApp::Get()->AltEncryptionColor();
    }

    // The rest is not colored
    return CTreeListItem::GetItemTextColor();
}

int CItem::CompareSibling(const CTreeListItem* tlib, const int subitem) const
{
    const CItem* other = reinterpret_cast<const CItem*>(tlib);

    switch (subitem)
    {
    case COL_NAME:
    {
        if (GetItemType() != other->GetItemType())
        {
            return usignum(GetItemType(), other->GetItemType());
        }
        return signum(_wcsicmp(m_name.get(), other->m_name.get()));
    }

    case COL_SUBTREE_PERCENTAGE:
    {
        if (MustShowReadJobs())
        {
            return usignum(GetReadJobs(), other->GetReadJobs());
        }
        else
        {
            return signum(GetFraction() - other->GetFraction());
        }
    }

    case COL_PERCENTAGE:
    {
        return signum(GetFraction() - other->GetFraction());
    }

    case COL_SIZE_PHYSICAL:
    {
        return usignum(GetSizePhysical(), other->GetSizePhysical());
    }

    case COL_SIZE_LOGICAL:
    {
        return usignum(GetSizeLogical(), other->GetSizeLogical());
    }

    case COL_ITEMS:
    {
        return usignum(GetItemsCount(), other->GetItemsCount());
    }

    case COL_FILES:
    {
        return usignum(GetFilesCount(), other->GetFilesCount());
    }

    case COL_FOLDERS:
    {
        return usignum(GetFoldersCount(), other->GetFoldersCount());
    }

    case COL_LAST_CHANGE:
    {
        if (m_lastChange < other->m_lastChange)
        {
            return -1;
        }
        if (m_lastChange == other->m_lastChange)
        {
            return 0;
        }

        return 1;
    }

    case COL_ATTRIBUTES:
    {
        return signum(GetSortAttributes() - other->GetSortAttributes());
    }

    case COL_OWNER:
    {
        return signum(_wcsicmp(GetOwner().c_str(), other->GetOwner().c_str()));
    }

    default:
    {
        return 0;
    }
    }
}

HICON CItem::GetIcon()
{
    ASSERT(IsVisible());

    // Return cached icon if available
    if (m_visualInfo->icon != nullptr)
    {
        return m_visualInfo->icon;
    }

    if (IsTypeOrFlag(IT_MYCOMPUTER))
    {
        m_visualInfo->icon = GetIconHandler()->GetMyComputerImage();
        return m_visualInfo->icon;
    }
    else if (IsTypeOrFlag(IT_FREESPACE))
    {
        m_visualInfo->icon = GetIconHandler()->GetFreeSpaceImage();
        return m_visualInfo->icon;
    }
    else if (IsTypeOrFlag(IT_UNKNOWN))
    {
        m_visualInfo->icon = GetIconHandler()->GetUnknownImage();
        return m_visualInfo->icon;
    }
    else if (IsTypeOrFlag(IT_HLINKS, IT_HLINKS_SET, IT_HLINKS_IDX))
    {
        m_visualInfo->icon = GetIconHandler()->GetHardlinksImage();
        return m_visualInfo->icon;
    }
    else if (IsTypeOrFlag(ITRP_MOUNT))
    {
        m_visualInfo->icon = GetIconHandler()->GetMountPointImage();
        return m_visualInfo->icon;
    }
    else if (IsTypeOrFlag(ITRP_SYMLINK))
    {
        m_visualInfo->icon = GetIconHandler()->GetSymbolicLinkImage();
        return m_visualInfo->icon;
    }
    else if (IsTypeOrFlag(ITRP_JUNCTION))
    {
        constexpr DWORD mask = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
        const bool osFile = (GetAttributes() & mask) == mask;
        m_visualInfo->icon = osFile ? GetIconHandler()->GetJunctionProtectedImage() : GetIconHandler()->GetJunctionImage();
        return m_visualInfo->icon;
    }

    const CItem* refItem = GetLinkedItem();
    CDirStatApp::Get()->GetIconHandler()->DoAsyncShellInfoLookup(std::make_tuple(const_cast<CItem*>(this),
        m_visualInfo->control, refItem->GetPath(), refItem->GetAttributes(), &m_visualInfo->icon, nullptr));

    return nullptr;
}

void CItem::DrawAdditionalState(CDC* pdc, const CRect& rcLabel) const
{
    if (!IsRootItem() && this == CDirStatDoc::Get()->GetZoomItem())
    {
        CRect rc = rcLabel;
        rc.InflateRect(1, 0);
        rc.bottom++;

        CSelectStockObject sobrush(pdc, NULL_BRUSH);
        CPen pen(PS_SOLID, 2, CDirStatDoc::Get()->GetZoomColor());
        CSelectObject sopen(pdc, &pen);

        pdc->Rectangle(rc);
    }
}

CItem* CItem::GetLinkedItem() noexcept
{
    // For IT_HLINKS_FILE, the name stores the full path
    if (IsTypeOrFlag(IT_HLINKS_FILE))
    {
        const std::wstring storedPath{ m_name.get(), m_nameLen };
        if (CItem* linkedItem = FindItemByPath(storedPath); linkedItem != nullptr)
        {
            return linkedItem;
        }
    }

    // Default: return this item
    return this;
}

// --- CTreeMap Interface ---

int CItem::TmiGetChildCount() const noexcept
{
    if (m_folderInfo == nullptr || IsTypeOrFlag(IT_HLINKS_IDX)) return 0;
    return static_cast<int>(m_folderInfo->m_children.size());
}

ULONGLONG CItem::TmiGetSize() const noexcept
{
    return COptions::TreeMapUseLogical ? GetSizeLogical() : GetSizePhysical();
}

// --- Drive / Volume Specific ---

bool CItem::IsRootItem() const noexcept
{
    return IsTypeOrFlag(ITF_ROOTITEM);
}

std::vector<CItem*> CItem::GetDriveItems() const
{
    // Gets all items of type IT_DRIVE
    std::vector<CItem*> drives;

    // Walk up to find the root item
    const CItem* root = this;
    while (root->GetParent() != nullptr)
    {
        root = root->GetParent();
    }

    if (root == nullptr)
    {
        return drives;
    }

    if (root->IsTypeOrFlag(IT_MYCOMPUTER))
    {
        for (const auto& child : root->GetChildren())
        {
            ASSERT(child->IsTypeOrFlag(IT_DRIVE));
            drives.push_back(child);
        }
    }
    else if (root->IsTypeOrFlag(IT_DRIVE))
    {
        drives.push_back(const_cast<CItem*>(root));
    }

    return drives;
}

ULONGLONG CItem::GetProgressRange() const
{
    if (IsTypeOrFlag(IT_MYCOMPUTER))
    {
        return GetProgressRangeMyComputer();
    }
    if (IsTypeOrFlag(IT_DRIVE))
    {
        return GetProgressRangeDrive();
    }
    if (IsTypeOrFlag(IT_FILE, IT_DIRECTORY))
    {
        return 0;
    }

    ASSERT(FALSE);
    return 0;
}

ULONGLONG CItem::GetProgressPos() const
{
    if (IsTypeOrFlag(IT_MYCOMPUTER))
    {
        ULONGLONG pos = 0;
        for (const auto& child : GetChildren())
        {
            pos += child->GetProgressPos();
        }
        return pos;
    }
    if (IsTypeOrFlag(IT_DRIVE))
    {
        ULONGLONG pos = GetSizePhysical();
        const CItem* fs = FindFreeSpaceItem();
        pos -= (fs != nullptr) ? fs->GetSizePhysical() : 0;
        return pos;
    }

    return 0;
}

int CItem::GetSubtreePercentageWidth()
{
    return 105;
}

CItem* CItem::FindRecyclerItem() const
{
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (!p->IsTypeOrFlag(IT_DRIVE)) continue;

        // There is no cross-platform way to consistently identify the recycle bin
        // so attempt to find an item with the most probable values
        for (const auto possible : { L"$RECYCLE.BIN", L"RECYCLER", L"RECYCLED" })
        {
            for (const auto& child : p->GetChildren())
            {
                if (child->IsTypeOrFlag(IT_DIRECTORY) && _wcsicmp(child->GetNameView().data(), possible) == 0)
                {
                    return child;
                }
            }
        }
    }

    return nullptr;
}

void CItem::CreateFreeSpaceItem()
{
    ASSERT(IsTypeOrFlag(IT_DRIVE));

    UpwardSetUndone();

    auto [total, free] = CDirStatApp::GetFreeDiskSpace(GetPath());

    const auto freespace = new CItem(IT_FREESPACE, Localization::Lookup(IDS_FREESPACE_ITEM));
    freespace->SetSizePhysical(free);
    freespace->SetDone();

    AddChild(freespace);
}

CItem* CItem::FindFreeSpaceItem() const
{
    const auto& children = GetChildren();
    const auto it = std::ranges::find_if(children,
        [](const auto& child) { return child->IsTypeOrFlag(IT_FREESPACE); });

    return it != children.end() ? *it : nullptr;
}

void CItem::UpdateFreeSpaceItem()
{
    if (IsTypeOrFlag(IT_MYCOMPUTER))
    {
        for (const auto& child : GetChildren())
        {
            if (child->IsTypeOrFlag(IT_DRIVE))
                child->UpdateFreeSpaceItem();
        }
    }
    else if (IsTypeOrFlag(IT_DRIVE))
    {
        auto [total, free] = CDirStatApp::GetFreeDiskSpace(GetPath());

        // Recreate name based on updated free space and percentage
        SetName(std::format(L"{:.2}|{} - {} ({:.1f}%)", GetNameView(),
            FormatVolumeNameOfRootPath(GetPath()), Localization::Format(
                IDS_DRIVE_ITEM_FREEsTOTALs, FormatBytes(free), FormatBytes(total)),
            100.0 * free / total));

        // Update freespace item if it exists
        if (CItem* freeSpaceItem = FindFreeSpaceItem(); freeSpaceItem != nullptr)
        {
            freeSpaceItem->UpwardSubtractSizePhysical(freeSpaceItem->GetSizePhysical());
            freeSpaceItem->UpwardAddSizePhysical(free);
        }
    }
    else ASSERT(FALSE);
}

void CItem::RemoveFreeSpaceItem()
{
    ASSERT(IsTypeOrFlag(IT_DRIVE));

    if (const auto freespace = FindFreeSpaceItem(); freespace != nullptr)
    {
        UpwardSetUndone();
        UpwardSubtractSizePhysical(freespace->GetSizePhysical());
        RemoveChild(freespace);
    }
}

void CItem::CreateUnknownItem()
{
    ASSERT(IsTypeOrFlag(IT_DRIVE));

    UpwardSetUndone();

    const auto unknown = new CItem(IT_UNKNOWN, Localization::Lookup(IDS_UNKNOWN_ITEM));
    unknown->SetDone();

    AddChild(unknown);
}

CItem* CItem::FindUnknownItem() const
{
    const auto& children = GetChildren();
    const auto it = std::ranges::find_if(children,
        [](const auto& child) { return child->IsTypeOrFlag(IT_UNKNOWN); });
    return it != children.end() ? *it : nullptr;
}

void CItem::UpdateUnknownItem() const
{
    ASSERT(IsTypeOrFlag(IT_DRIVE));

    CItem* unknown = FindUnknownItem();
    if (unknown == nullptr)
    {
        return;
    }

    // Rebaseline as if unknown size was not shown
    unknown->UpwardSubtractSizePhysical(unknown->GetSizePhysical());

    // Get the tallied size, account for whether the freespace item is part of it
    const CItem* freeSpaceItem = FindFreeSpaceItem();
    const ULONGLONG tallied = GetSizePhysical() - (freeSpaceItem ? freeSpaceItem->GetSizePhysical() : 0);

    auto [total, free] = CDirStatApp::GetFreeDiskSpace(GetPath());
    unknown->UpwardAddSizePhysical((tallied > total - free) ? 0 : total - free - tallied);
}

void CItem::RemoveUnknownItem()
{
    ASSERT(IsTypeOrFlag(IT_DRIVE));

    if (const auto unknown = FindUnknownItem(); unknown != nullptr)
    {
        UpwardSetUndone();
        UpwardSubtractSizePhysical(unknown->GetSizePhysical());
        RemoveChild(unknown);
    }
}

void CItem::UpwardDrivePacman()
{
    if (!COptions::PacmanAnimation)
    {
        return;
    }

    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsTypeOrFlag(IT_FILE) || !p->IsVisible()) continue;
        if (p->GetReadJobs() == 0) p->StopPacman();
        else p->DrivePacman();
    }
}

// --- Hardlinks & Hashing ---

void CItem::CreateHardlinksItem()
{
    ASSERT(IsTypeOrFlag(IT_DRIVE));

    const auto hardlinks = new CItem(IT_HLINKS, Localization::Lookup(IDS_HARDLINKS_ITEM));

    // Create 20 Index Set subfolders (Index Set 1 through Index 20)
    // On file systems with many hardlinks, this helps reduce the items
    // to expand in the interface on when viewing hardlink structures
    constexpr char INDEX_SET_COUNT = 20;
    for (const int i : std::views::iota(1, INDEX_SET_COUNT + 1))
    {
        const auto indexSet = new CItem(IT_HLINKS_SET, std::format(L"{} ≡ 0x{:02X}", Localization::Lookup(IDS_COL_INDEX), i));
        indexSet->SetDone();
        hardlinks->AddChild(indexSet);
    }

    AddChild(hardlinks);
}

CItem* CItem::FindHardlinksItem() const
{
    const auto& children = GetParentDrive()->GetChildren();
    const auto it = std::ranges::find_if(children,
        [](const auto& child) { return child->IsTypeOrFlag(IT_HLINKS); });

    return it != children.end() ? *it : nullptr;
}

CItem* CItem::FindHardlinksIndexItem() const
{
    // Only applicable for files with ITF_HARDLINK flag
    if (!IsTypeOrFlag(ITF_HARDLINK) || GetIndex() == 0)
    {
        return nullptr;
    }

    // Find the <Hardlinks> container
    const CItem* hardlinksItem = FindHardlinksItem();
    if (hardlinksItem == nullptr)
    {
        return nullptr;
    }

    // Search through Index Sets to find the IT_HLINKS_IDX with matching index
    for (const auto* indexSet : hardlinksItem->GetChildren())
    {
        if (!indexSet->IsTypeOrFlag(IT_HLINKS_SET)) continue;
        for (auto* indexFolder : indexSet->GetChildren())
        {
            if (indexFolder->IsTypeOrFlag(IT_HLINKS_IDX) && indexFolder->GetIndex() == GetIndex())
            {
                return indexFolder;
            }
        }
    }

    return nullptr;
}

void CItem::RemoveHardlinksItem()
{
    ASSERT(IsTypeOrFlag(IT_DRIVE));

    if (const auto hardlinks = FindHardlinksItem(); hardlinks != nullptr)
    {
        UpwardSetUndone();
        UpwardSubtractSizePhysical(hardlinks->GetSizePhysical());
        RemoveChild(hardlinks);
    }
}

void CItem::DoHardlinkAdjustment()
{
    if (!IsTypeOrFlag(IT_DRIVE)) return;

    // Create map for duplicate index detection
    std::unordered_map<ULONGLONG, CItem*> indexMapInitial;
    std::unordered_map<ULONGLONG, std::vector<CItem*>> indexDupes;
    indexMapInitial.reserve(GetFilesCount());

    // Look for all indexed items in the tree
    for (std::vector queue({ this }); !queue.empty();)
    {
        // Grab item from queue
        const CItem* qitem = queue.back();
        queue.pop_back();

        // Descend into child items
        if (qitem->IsLeaf()) continue;
        for (const auto& child : qitem->GetChildren())
        {
            // Add for duplicate index detection
            if (const auto index = child->GetIndex(); child->IsTypeOrFlag(IT_FILE) && index > 0)
            {
                if (const auto [it, inserted] = indexMapInitial.try_emplace(index, child); !inserted)
                {
                    auto& existing = indexDupes[index];
                    if (existing.empty()) existing.emplace_back(it->second);
                    existing.emplace_back(child);
                }
            }
            // Do not descend into reparse points since indexes may be from other volumes
            else if (!child->IsLeaf() && (child->GetAttributes() & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
            {
                queue.push_back(child);
            }
        }
    }

    // Get the hardlinks container and its Index Set children
    const auto hardlinksItem = FindHardlinksItem();
    if (hardlinksItem == nullptr) return;

    const auto& indexSets = hardlinksItem->GetChildren();

    // Process hardlinks - create hierarchical structure
    for (const auto& [index, list] : indexDupes)
    {
        bool skipAdd = false;
        auto itemSize = 0ull;

        // Check if any items already have the hardlink flag (already processed)
        for (const auto* item : list)
        {
            if (item->IsTypeOrFlag(ITF_HARDLINK)) { skipAdd = true; break; }
        }

        if (skipAdd) continue;

        // Calculate the maximum physical size among all hardlinks with this index
        for (const auto* item : list)
        {
            itemSize = max(itemSize, item->GetSizePhysicalRaw());
        }

        // Determine which Index Set this belongs to (modulus 20, 0-based index)
        constexpr auto INDEX_SET_COUNT = 20u;
        const size_t setIndex = index % INDEX_SET_COUNT;  // 0-19
        CItem* indexSetItem = setIndex < indexSets.size() ? indexSets[setIndex] : nullptr;

        if (indexSetItem == nullptr) continue;

        // Create "Index N" folder under the appropriate Index Set
        const auto indexFolder = new CItem(IT_HLINKS_IDX, std::format(L"{} 0x{:016X}", Localization::Lookup(IDS_COL_INDEX), index));
        indexFolder->SetIndex(index);

        // Add file reference entries under the Index folder
        for (auto* item : list)
        {
            // Subtract physical size from the file's original parent hierarchy
            item->GetParent()->UpwardSubtractSizePhysical(item->GetSizePhysicalRaw());
            item->GetParent()->UpwardSetUndone();
            item->SetFlag(ITF_HARDLINK);

            // Create a file reference entry with just the full path
            // GetName() will extract the filename, GetLinkedItem() will use the path
            const auto fileRef = new CItem(IT_HLINKS_FILE, item->GetPath());
            fileRef->SetIndex(item->GetIndex());
            fileRef->SetSizePhysical(item->GetSizePhysicalRaw());
            fileRef->SetSizeLogical(item->GetSizeLogical());
            fileRef->SetLastChange(item->GetLastChange());

            // Add to index folder without propagating size upward (addOnly=true)
            indexFolder->AddChild(fileRef, true);
        }

        // Set the physical size on the Index folder - this is what tallies upward
        indexFolder->SetSizePhysical(itemSize);

        // Mark index set as undone so it will be re-sorted
        indexSetItem->SetFlag(ITF_DONE, true);

        // Add to Index Set - this will propagate the size upward
        indexSetItem->AddChild(indexFolder);
    }

    // Now sort all the Index Sets and their children, and mark done
    for (auto* indexSet : hardlinksItem->GetChildren())
    {
        if (!indexSet->IsTypeOrFlag(IT_HLINKS_SET)) continue;

        // Sort Index folders within this Index Set
        for (auto* indexFolder : indexSet->GetChildren())
        {
            if (!indexFolder->IsTypeOrFlag(IT_HLINKS_IDX)) continue;

            // Sort file references within this Index folder by size
            indexFolder->SortItemsBySizePhysical();
            indexFolder->SetFlag(ITF_DONE);
        }

        // Sort Index folders within this Index Set by size
        indexSet->SortItemsBySizePhysical();
        indexSet->SetFlag(ITF_DONE);
    }

    // Sort Index Sets within Hardlinks by size and mark done
    hardlinksItem->SortItemsBySizePhysical();
    hardlinksItem->UpwardSetUndone();
}

std::vector<CItem*> CItem::FindItemsBySameIndex() const
{
    // Only search if we have a valid non-zero index
    const ULONGLONG targetIndex = GetIndex();
    if (targetIndex == 0)
    {
        return {};
    }

    // Get the parent drive - we only search within the same drive
    auto* driveItem = GetParentDrive();
    if (driveItem == nullptr)
    {
        return {};
    }

    // Use a stack-based traversal to search through all items under the drive
    std::vector<CItem*> results;
    for (std::vector itemStack({ driveItem }); !itemStack.empty();)
    {
        CItem* current = itemStack.back();
        itemStack.pop_back();

        // Check if this item has the same index (but is not the current item itself)
        if (current != this && current->GetIndex() == targetIndex)
        {
            results.push_back(current);
        }

        // Add all children to the stack for traversal
        else if (!current->IsLeaf() &&
            (current->GetAttributes() & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
        {
            for (auto* child : current->GetChildren())
            {
                itemStack.push_back(child);
            }
        }
    }

    return results;
}

std::vector<BYTE> CItem::GetFileHash(ULONGLONG hashSizeLimit, BlockingQueue<CItem*>* queue)
{
    thread_local std::vector<BYTE> fileBuffer(1024ull * 1024ull);
    thread_local std::vector<BYTE> hashBuffer;
    thread_local std::once_flag hashInitFlag;
    thread_local SmartPointer<BCRYPT_ALG_HANDLE> hashAlgHandle(
        [](BCRYPT_ALG_HANDLE handle) { BCryptCloseAlgorithmProvider(handle, 0); });
    thread_local SmartPointer<BCRYPT_HASH_HANDLE> hashHandle(BCryptDestroyHash);

    // Initialize shared structures using std::call_once for better performance
    std::call_once(hashInitFlag, []()
        {
            DWORD ResultLength = 0;
            DWORD iHashLength = 0;
            if (BCryptOpenAlgorithmProvider(&hashAlgHandle, BCRYPT_SHA512_ALGORITHM, MS_PRIMITIVE_PROVIDER, BCRYPT_HASH_REUSABLE_FLAG) == 0 &&
                BCryptGetProperty(hashAlgHandle, BCRYPT_HASH_LENGTH, reinterpret_cast<PBYTE>(&iHashLength), sizeof(iHashLength), &ResultLength, 0) == 0 &&
                BCryptCreateHash(hashAlgHandle, &hashHandle, nullptr, 0, nullptr, 0, BCRYPT_HASH_REUSABLE_FLAG) == 0)
            {
                hashBuffer.resize(iHashLength);
            }
        });

    // Check if initialization succeeded
    if (hashBuffer.empty())
    {
        return {};
    }

    // Open file for reading - avoid files that are actively being written to
    SmartPointer<HANDLE> hFile(CloseHandle, CreateFile(GetPathLong().c_str(),
        GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return {};
    }

    // Hash data one read at a time
    DWORD iReadResult = 0;
    DWORD iHashResult = 0;
    DWORD iReadBytes = 0;
    ULONGLONG totalBytesHashed = 0;

    while ((iReadResult = ReadFile(hFile, fileBuffer.data(), static_cast<DWORD>(
        min(hashSizeLimit - totalBytesHashed, fileBuffer.size())),
        &iReadBytes, nullptr)) != 0 && iReadBytes > 0)
    {
        UpwardDrivePacman();

        // Hash the data
        iHashResult = BCryptHashData(hashHandle, fileBuffer.data(), iReadBytes, 0);
        if (iHashResult != 0) break;

        // Stop if we've reached the hash size limit
        totalBytesHashed += iReadBytes;
        if (totalBytesHashed >= hashSizeLimit) break;

        queue->WaitIfSuspended();
    }

    // Complete the hashing process and check on errors
    if (const NTSTATUS iFinishResult = BCryptFinishHash(hashHandle,
        hashBuffer.data(), static_cast<ULONG>(hashBuffer.size()), 0);
        iReadResult == 0 || iHashResult != 0 || iFinishResult != 0)
    {
        return {};
    }

    // We reduce the size of the stored hash since the level of uniqueness required
    // is unnecessary for simple dupe checking. This is preferred to just using a simpler
    // hash alg since SHA512 is FIPS compliant on Windows and more performant than SHA256.
    const auto ReducedHashInBytes = min(16, hashBuffer.size());
    return { hashBuffer.begin(), hashBuffer.begin() + ReducedHashInBytes };
}

// --- Private Helpers ---

ULONGLONG CItem::GetProgressRangeMyComputer() const
{
    ASSERT(IsTypeOrFlag(IT_MYCOMPUTER));

    ULONGLONG range = 0;
    for (const auto& child : GetChildren())
    {
        range += child->GetProgressRangeDrive();
    }
    return range;
}

ULONGLONG CItem::GetProgressRangeDrive() const
{
    auto [total, free] = CDirStatApp::GetFreeDiskSpace(GetPath());

    total -= free;

    return total;
}

COLORREF CItem::GetGraphColor() const
{
    if (IsTypeOrFlag(IT_UNKNOWN))
    {
        return RGB(255, 255, 0) | CTreeMap::COLORFLAG_LIGHTER;
    }

    if (IsTypeOrFlag(ITF_PREVIEW))
    {
        return static_cast<COLORREF>(GetIndex());
    }

    if (IsTypeOrFlag(IT_FREESPACE))
    {
        return RGB(100, 100, 100) | CTreeMap::COLORFLAG_DARKER;
    }

    if (IsTypeOrFlag(IT_HLINKS, IT_HLINKS_SET, IT_HLINKS_IDX, IT_HLINKS_FILE))
    {
        return RGB(200, 150, 100) | CTreeMap::COLORFLAG_LIGHTER;
    }

    if (IsTypeOrFlag(IT_FILE))
    {
        return CDirStatDoc::Get()->GetCushionColor(GetExtension());
    }

    return RGB(0, 0, 0);
}

bool CItem::MustShowReadJobs() const noexcept
{
    if (GetParent() != nullptr)
    {
        return !GetParent()->IsDone();
    }

    return !IsDone();
}

COLORREF CItem::GetPercentageColor() const noexcept
{
    const int i = GetIndent() % COptions::FileTreeColorCount;
    return std::array<COLORREF, 8>
    {
        COptions::FileTreeColor0,
            COptions::FileTreeColor1,
            COptions::FileTreeColor2,
            COptions::FileTreeColor3,
            COptions::FileTreeColor4,
            COptions::FileTreeColor5,
            COptions::FileTreeColor6,
            COptions::FileTreeColor7
    }[i];
}
