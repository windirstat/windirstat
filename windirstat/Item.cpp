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
#include "FinderBasic.h"
#include "FinderNtfs.h"

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

CItem::CItem(const ITEMTYPE type, const std::wstring & name) : m_type(type)
{
    if (IsTypeOrFlag(IT_MYCOMPUTER, IT_DRIVE, IT_DIRECTORY, IT_HLINKS, IT_HLINKS_SET, IT_HLINKS_IDX))
    {
        m_folderInfo = std::make_unique<CHILDINFO>();

        // My computer node will never have these attributes set externally
        if (IsTypeOrFlag(IT_MYCOMPUTER))
        {
            m_attributes = 0;
        }
    }

    if (IsTypeOrFlag(IT_DRIVE))
    {
        // Store drive paths with a backslash
        std::wstring nameTmp = name;
        if (nameTmp.ends_with(L":")) nameTmp.append(L"\\");

        // The name string on the drive is two parts separated by a pipe. For example,
        // C:\|Local Disk (C:) is the true path followed by the name description
        SetName(std::format(L"{:.2}|{}", nameTmp, FormatVolumeNameOfRootPath(nameTmp)));
        m_attributes = LOWORD(GetFileAttributes(GetPathLong().c_str()));
    }
    else
    {
        SetName(name);
    }
}

CItem::CItem(const ITEMTYPE type, const std::wstring& name, const FILETIME lastChange,
    const ULONGLONG sizePhysical, const ULONGLONG sizeLogical, const ULONGLONG index,
    const DWORD attributes, const ULONG files, const ULONG subdirs)
{
    SetName(name);
    m_type = type;
    m_lastChange = lastChange;
    m_sizePhysical = sizePhysical;
    m_sizeLogical = sizeLogical;
    m_index = index;
    m_attributes = LOWORD(attributes);

    if (IsTypeOrFlag(IT_DRIVE))
    {
        SetName(std::format(L"{:.2}|{}", name, FormatVolumeNameOfRootPath(name)));
    }

    if (IsTypeOrFlag(IT_MYCOMPUTER, IT_DRIVE, IT_DIRECTORY, IT_HLINKS, IT_HLINKS_SET, IT_HLINKS_IDX))
    {
        m_folderInfo = std::make_unique<CHILDINFO>();
        m_folderInfo->m_subdirs = subdirs;
        m_folderInfo->m_files = files;
    }
}

CItem::~CItem()
{
    if (m_folderInfo != nullptr)
    {
        for (const auto& child : m_folderInfo->m_children)
        {
            delete child;
        }
    }
}

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

int CItem::GetSubtreePercentageWidth()
{
    return 105;
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

void CItem::UpdateStatsFromDisk()
{
    if (IsTypeOrFlag(IT_DIRECTORY, IT_FILE))
    {
        FinderBasic finder(true);
        if (finder.FindFile(GetFolderPath(), IsTypeOrFlag(ITF_ROOTITEM) ? std::wstring() : GetName(), GetAttributes()))
        {
            SetLastChange(finder.GetLastWriteTime());
            SetAttributes(finder.GetAttributes());

            if (IsTypeOrFlag(IT_FILE))
            {
                ExtensionDataRemove();
                UpwardSubtractSizePhysical(GetSizePhysical());
                UpwardSubtractSizeLogical(GetSizeLogical());
                UpwardAddSizePhysical(finder.GetFileSizePhysical());
                UpwardAddSizeLogical(finder.GetFileSizeLogical());
                ExtensionDataAdd();
            }
        }
    }
    else if (IsTypeOrFlag(IT_DRIVE))
    {
        SmartPointer<HANDLE> handle(CloseHandle, CreateFile(GetPathLong().c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
        if (handle != INVALID_HANDLE_VALUE)
        {
            GetFileTime(handle, nullptr, nullptr, &m_lastChange);
        }
    }
}

const std::vector<CItem*>& CItem::GetChildren() const noexcept
{
    ASSERT(m_folderInfo != nullptr);
    return m_folderInfo->m_children;
}

CItem* CItem::GetParent() const noexcept
{
    return reinterpret_cast<CItem*>(CTreeListItem::GetParent());
}

CItem* CItem::GetParentDrive() const noexcept
{
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsTypeOrFlag(IT_DRIVE)) return const_cast<CItem*>(p);
    }
    return nullptr;
}

CItem* CItem::GetVolumeRoot() const noexcept
{
    auto p = const_cast<CItem*>(this);
    for (; p->GetParent() != nullptr; p = p->GetParent())
    {
        if (p->IsTypeOrFlag(IT_DRIVE)) return p;
        if (p->IsTypeOrFlag(ITRP_MOUNT)) return p;
    }

    return p;
}

void CItem::AddChild(CItem* child, const bool addOnly)
{
    if (!addOnly)
    {
        UpwardAddSizePhysical(child->GetSizePhysical());
        UpwardAddSizeLogical(child->GetSizeLogical());
        UpwardUpdateLastChange(child->GetLastChange());
        ExtensionDataAdd();
    }

    child->SetParent(this);
    if (IsVisible() && IsExpanded())
    {
        CMainFrame::Get()->InvokeInMessageThread([this, child]
        {
            // Add child in UI thread since UI thread immediately use it
            m_folderInfo->m_children.push_back(child);
            CFileTreeControl::Get()->OnChildAdded(this, child);
        });
    }
    else m_folderInfo->m_children.push_back(child);
}

void CItem::RemoveChild(CItem* child)
{
    if (IsVisible())
    {
        CMainFrame::Get()->InvokeInMessageThread([this, child]
        {
            CFileTreeControl::Get()->OnChildRemoved(this, child);
        });
    }

    auto& children = m_folderInfo->m_children;
    if (const auto it = std::ranges::find(children, child); it != children.end())
    {
        children.erase(it);
    }

    // Check if this child is a hardlink
    if (COptions::ProcessHardlinks && child->IsTypeOrFlag(ITF_HARDLINK) && child->GetIndex() > 0)
    {
        // Find remaining items with the same index
        const auto sameIndexItems = child->FindItemsBySameIndex();
        
        if (sameIndexItems.size() == 1)
        {
            // Only one remaining item - it's no longer a hardlink
            CItem* remainingItem = sameIndexItems[0];
            remainingItem->SetFlag(ITF_HARDLINK, true);  // Clear the flag

            // Find and update the hardlink structure
            if (const CItem* hardlinksItem = remainingItem->FindHardlinksItem(); hardlinksItem != nullptr)
            {
                // Find the Index folder for this index and remove it
                for (auto* indexSet : hardlinksItem->GetChildren())
                {
                    if (!indexSet->IsTypeOrFlag(IT_HLINKS_SET)) continue;
                    for (auto* indexFolder : indexSet->GetChildren())
                    {
                        if (indexFolder->IsTypeOrFlag(IT_HLINKS_IDX) && indexFolder->GetIndex() == child->GetIndex())
                        {
                            // Subtract size and remove the index folder
                            indexSet->UpwardSubtractSizePhysical(indexFolder->GetSizePhysical());
                            indexSet->RemoveChild(indexFolder);
                            

                            // Restore size to the remaining item's hierarchy
                            remainingItem->GetParent()->UpwardAddSizePhysical(remainingItem->GetSizePhysicalRaw());
                            break;
                        }
                    }
                }
            }
        }
        else if (sameIndexItems.size() > 1)
        {
            // Multiple items still exist - just remove this file's reference from the hardlink structure
            if (const CItem* hardlinksItem = child->FindHardlinksItem(); hardlinksItem != nullptr)
            {
                for (const auto* indexSet : hardlinksItem->GetChildren())
                {
                    if (!indexSet->IsTypeOrFlag(IT_HLINKS_SET)) continue;
                    for (auto* indexFolder : indexSet->GetChildren())
                    {
                        if (indexFolder->IsTypeOrFlag(IT_HLINKS_IDX) && indexFolder->GetIndex() == child->GetIndex())
                        {
                            // Find and remove the file reference matching this child
                            for (auto* fileRef : indexFolder->GetChildren())
                            {
                                if (fileRef->IsTypeOrFlag(IT_HLINKS_FILE) && fileRef->GetNameView() == child->GetNameView())
                                {
                                    indexFolder->RemoveChild(fileRef);
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    delete child;
}

void CItem::RemoveAllChildren()
{
    if (IsRootItem())
    {
        CDirStatDoc::Get()->GetExtensionData()->clear();
    }

    if (IsLeaf()) return;
    CMainFrame::Get()->InvokeInMessageThread([this]
    {
        CFileTreeControl::Get()->OnRemovingAllChildren(this);
    });

    for (const auto& child : m_folderInfo->m_children)
    {
        delete child;
    }
    m_folderInfo->m_children.clear();
}

void CItem::UpwardAddFolders(const ULONG dirCount) noexcept
{
    if (dirCount == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsTypeOrFlag(IT_FILE)) continue;
        p->m_folderInfo->m_subdirs += dirCount;
    }
}

void CItem::UpwardSubtractFolders(const ULONG dirCount) noexcept
{
    if (dirCount == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        ASSERT(p->m_folderInfo->m_subdirs >= dirCount);
        if (p->IsTypeOrFlag(IT_FILE)) continue;
        p->m_folderInfo->m_subdirs -= dirCount;
    }
}

void CItem::UpwardAddFiles(const ULONG fileCount) noexcept
{
    if (fileCount == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsTypeOrFlag(IT_FILE)) continue;
        p->m_folderInfo->m_files += fileCount;
    }
}

void CItem::UpwardSubtractFiles(const ULONG fileCount) noexcept
{
    if (fileCount == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsTypeOrFlag(IT_FILE)) continue;
        ASSERT(p->m_folderInfo->m_files >= fileCount);
        p->m_folderInfo->m_files -= fileCount;
    }
}

void CItem::UpwardAddSizePhysical(const ULONGLONG bytes) noexcept
{
    if (bytes == 0) return;

    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        p->m_sizePhysical += bytes;
    }
}

void CItem::UpwardSubtractSizePhysical(const ULONGLONG bytes) noexcept
{
    if (bytes == 0) return;

    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        ASSERT(bytes <= p->m_sizePhysical);
        p->m_sizePhysical -= bytes;
    }
}

void CItem::UpwardAddSizeLogical(const ULONGLONG bytes) noexcept
{
    if (bytes == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        p->m_sizeLogical += bytes;
    }
}

void CItem::UpwardSubtractSizeLogical(const ULONGLONG bytes) noexcept
{
    if (bytes == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        ASSERT(p->m_sizeLogical - bytes >= 0);
        p->m_sizeLogical -= bytes;
    }
}

void CItem::ExtensionDataAdd() const
{
    if (!IsTypeOrFlag(IT_FILE)) return;
    const auto record = CDirStatDoc::Get()->GetExtensionDataRecord(GetExtension());
    record->AddFile(GetSizePhysical());
}

void CItem::ExtensionDataRemove() const
{
    if (!IsTypeOrFlag(IT_FILE)) return;
    const auto record = CDirStatDoc::Get()->GetExtensionDataRecord(GetExtension());
    record->RemoveFile(GetSizePhysical());
    if (record->GetFiles() == 0) CDirStatDoc::Get()->GetExtensionData()->erase(GetExtension());
}

void CItem::ExtensionDataProcessChildren(const bool remove) const
{
    std::stack<const CItem*> childStack({ this });
    while (!childStack.empty()) [[msvc::forceinline_calls]]
    {
        const auto& item = childStack.top();
        childStack.pop();

        if (item->IsTypeOrFlag(IT_MYCOMPUTER, IT_DIRECTORY, IT_DRIVE))
        {
            for (const auto& child : item->GetChildren())
            {
                childStack.push(child);
            }
        }
        else if (item->IsTypeOrFlag(IT_FILE))
        {
            remove ? item->ExtensionDataRemove() : item->ExtensionDataAdd();
        }
    }
}

void CItem::UpwardAddReadJobs(const ULONG count) noexcept
{
    if (IsLeaf() || count == 0) return;
    if (m_folderInfo->m_jobs == 0) m_folderInfo->m_tstart = static_cast<ULONG>(GetTickCount64() / 1000ull);
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsTypeOrFlag(IT_FILE)) continue;
        p->m_folderInfo->m_jobs += count;
    }
}

void CItem::UpwardSubtractReadJobs(const ULONG count) noexcept
{
    if (count == 0 || IsTypeOrFlag(IT_FILE)) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        const ULONG previous = p->m_folderInfo->m_jobs.fetch_sub(count);
        if (previous >= count && previous - count == 0)
        {
            p->SetDone();
        }
    }
}

// This method increases the last change
void CItem::UpwardUpdateLastChange(const FILETIME& t) noexcept
{
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (FileTimeIsGreater(t, p->m_lastChange)) p->m_lastChange = t;
    }
}

void CItem::UpwardRecalcLastChange(const bool withoutItem)
{
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        p->UpdateStatsFromDisk();

        if (IsLeaf()) continue;
        for (const auto& child : p->GetChildren())
        {
            if (withoutItem && child == this) continue;
            if (FileTimeIsGreater(child->m_lastChange, p->m_lastChange))
                p->m_lastChange = child->m_lastChange;
        }
    }
}

ULONGLONG CItem::GetSizePhysical() const noexcept
{
    return (IsTypeOrFlag(ITF_HARDLINK) && COptions::ProcessHardlinks) ? 0 : m_sizePhysical.load();
}

ULONGLONG CItem::GetSizeLogical() const noexcept
{
    return m_sizeLogical;
}

ULONGLONG CItem::GetSizePhysicalRaw() const noexcept
{
    return m_sizePhysical;
}

void CItem::SetSizePhysical(const ULONGLONG size) noexcept
{
    ASSERT(size >= 0);
    m_sizePhysical = size;
}

void CItem::SetSizeLogical(const ULONGLONG size) noexcept
{
    ASSERT(size >= 0);
    m_sizeLogical = size;
}

ULONG CItem::GetReadJobs() const noexcept
{
    if (IsLeaf()) return 0;
    return m_folderInfo->m_jobs;
}

FILETIME CItem::GetLastChange() const noexcept
{
    return m_lastChange;
}

void CItem::SetLastChange(const FILETIME& t) noexcept
{
    m_lastChange = t;
}

void CItem::SetAttributes(const DWORD attr) noexcept
{
    m_attributes = LOWORD(attr);
}

DWORD CItem::GetAttributes() const noexcept
{
    return m_attributes == LOWORD(INVALID_FILE_ATTRIBUTES)
        ? INVALID_FILE_ATTRIBUTES : m_attributes;
}

void CItem::SetIndex(const ULONGLONG index) noexcept
{
    m_index = index;
}

ULONGLONG CItem::GetIndex() const noexcept
{
    return m_index;
}

DWORD CItem::GetReparseTag() const noexcept
{
    if (IsTypeOrFlag(ITRP_SYMLINK)) return IO_REPARSE_TAG_SYMLINK;
    if (IsTypeOrFlag(ITRP_MOUNT)) return IO_REPARSE_TAG_MOUNT_POINT;
    if (IsTypeOrFlag(ITRP_JUNCTION)) return IO_REPARSE_TAG_JUNCTION_POINT;
    if (IsTypeOrFlag(ITRP_CLOUD)) return IO_REPARSE_TAG_CLOUD_MASK;
    return 0;
}

void CItem::SetReparseTag(const DWORD reparseType) noexcept
{
    if (reparseType == 0) (void) false;
    else if (reparseType == IO_REPARSE_TAG_SYMLINK) SetReparseType(ITRP_SYMLINK);
    else if (reparseType == IO_REPARSE_TAG_MOUNT_POINT) SetReparseType(ITRP_MOUNT);
    else if (reparseType == IO_REPARSE_TAG_JUNCTION_POINT) SetReparseType(ITRP_JUNCTION);
    else if ((reparseType & 0xF00000FF) == IO_REPARSE_TAG_CLOUD) SetReparseType(ITRP_CLOUD);
}

// Returns a value which resembles sorting of RHSACE considering gaps
USHORT CItem::GetSortAttributes() const noexcept
{
    USHORT ret = 0;

    // We want to enforce the order RHSACE with R being the highest priority
    // attribute and E being the lowest priority attribute.
    ret |= m_attributes & FILE_ATTRIBUTE_READONLY    ? 1 << 6 : 0; // R
    ret |= m_attributes & FILE_ATTRIBUTE_HIDDEN      ? 1 << 5 : 0; // H
    ret |= m_attributes & FILE_ATTRIBUTE_SYSTEM      ? 1 << 4 : 0; // S
    ret |= m_attributes & FILE_ATTRIBUTE_ARCHIVE     ? 1 << 3 : 0; // A
    ret |= m_attributes & FILE_ATTRIBUTE_COMPRESSED  ? 1 << 2 : 0; // C
    ret |= m_attributes & FILE_ATTRIBUTE_ENCRYPTED   ? 1 << 1 : 0; // E
    ret |= m_attributes & FILE_ATTRIBUTE_SPARSE_FILE ? 1 << 0 : 0; // Z

    return ret;
}

double CItem::GetFraction() const noexcept
{
    if (!GetParent() || GetParent()->GetSizePhysical() == 0)
    {
        return 1.0;
    }
    return static_cast<double>(GetSizePhysical()) /
        static_cast<double>(GetParent()->GetSizePhysical());
}

bool CItem::IsRootItem() const noexcept
{
    return IsTypeOrFlag(ITF_ROOTITEM);
}

std::wstring CItem::GetPath() const
{
    if (IsTypeOrFlag(IT_HLINKS_SET, IT_HLINKS_IDX))
    {
        return {};
    }

    std::wstring path = UpwardGetPathWithoutBackslash();
    if (IsTypeOrFlag(IT_DRIVE))
    {
        path += L"\\";
    }
    return path;
}

std::wstring CItem::GetPathLong() const
{
    return FinderBasic::MakeLongPathCompatible(GetPath());
}

std::wstring CItem::GetOwner(const bool force) const
{
    if (!IsVisible() && !force)
    {
        return {};
    }

    // If visible, use cached variable
    std::wstring tmp;
    std::wstring & ret = (force) ? tmp : m_visualInfo->owner;
    if (!ret.empty()) return ret;

    // Fetch owner information from drive
    SmartPointer<PSECURITY_DESCRIPTOR> ps(LocalFree);
    PSID sid = nullptr;
    GetNamedSecurityInfo(GetPathLong().c_str(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
        &sid, nullptr, nullptr, nullptr, &ps);
    ret = GetNameFromSid(sid);
    return ret;
}

bool CItem::HasUncPath() const
{
    return GetPath().starts_with(L"\\\\");

}

// Returns the path for "Explorer here" or "Command Prompt here"
std::wstring CItem::GetFolderPath() const
{
    std::wstring path = GetPath();
    if (IsTypeOrFlag(IT_FILE))
    {
        const auto i = path.find_last_of(wds::chrBackslash);
        ASSERT(i != std::wstring::npos);
        path = path.substr(0, i + 1);
    }

    return path;
}

void CItem::SetName(std::wstring_view name)
{
    m_nameLen = static_cast<std::uint8_t>(name.size());
    m_name = std::make_unique_for_overwrite<wchar_t[]>(m_nameLen + 1);
    if (m_nameLen) std::wmemcpy(m_name.get(), name.data(), m_nameLen);
    m_name[m_nameLen] = L'\0';
}

std::wstring CItem::GetName() const noexcept
{
    return { m_name.get(), m_nameLen };
}

std::wstring_view CItem::GetNameView() const noexcept
{
    return { m_name.get(), m_nameLen };
}

std::wstring CItem::GetExtension() const
{
    if (!IsTypeOrFlag(IT_FILE)) return GetName();
    const auto extName = GetNameView();
    const auto pos = extName.rfind('.');
    if (pos == std::wstring_view::npos) return {};
    std::wstring extLower(extName.substr(pos));
    _wcslwr_s(extLower.data(), extLower.size() + 1);
    return extLower;
}

ULONG CItem::GetFilesCount() const noexcept
{
    if (IsLeaf()) return 0;
    return m_folderInfo->m_files;
}

ULONG CItem::GetFoldersCount() const noexcept
{
    if (IsLeaf()) return 0;
    return m_folderInfo->m_subdirs;
}

ULONGLONG CItem::GetItemsCount() const noexcept
{
    if (IsLeaf()) return 0;
    return static_cast<ULONGLONG>(m_folderInfo->m_files) + static_cast<ULONGLONG>(m_folderInfo->m_subdirs);
}

void CItem::SetDone()
{
    if (IsDone())
    {
        return;
    }

    if (IsTypeOrFlag(IT_DRIVE))
    {
        UpdateFreeSpaceItem();
        UpdateUnknownItem();
    }

    // Sort and set finish time
    if (!IsLeaf())
    {
        COptions::TreeMapUseLogical ? SortItemsBySizeLogical() : SortItemsBySizePhysical();
        m_folderInfo->m_tfinish = static_cast<ULONG>(GetTickCount64() / 1000ull);
    }

    // Mark as done just so other functions do not sort at the same time
    SetFlag(ITF_DONE);
}

void CItem::SortItemsBySizePhysical() const
{
    if (IsLeaf()) return;

    // sort by size for proper treemap rendering
    m_folderInfo->m_children.shrink_to_fit();
    std::ranges::sort(m_folderInfo->m_children, std::ranges::greater{}, &CItem::GetSizePhysical);
}

void CItem::SortItemsBySizeLogical() const
{
    if (IsLeaf()) return;
    
    // sort by size for proper treemap rendering
    m_folderInfo->m_children.shrink_to_fit();
    std::ranges::sort(m_folderInfo->m_children, std::ranges::greater{}, &CItem::GetSizeLogical);
}

ULONGLONG CItem::GetTicksWorked() const noexcept
{
    if (IsLeaf()) return 0;
    return m_folderInfo->m_tfinish > 0 ? (m_folderInfo->m_tfinish - m_folderInfo->m_tstart) :
        (m_folderInfo->m_tstart > 0) ? ((GetTickCount64() / 1000ull) - m_folderInfo->m_tstart) : 0;
}

void CItem::ResetScanStartTime() const noexcept
{
    if (IsLeaf()) return;
    m_folderInfo->m_tfinish = 0;
    m_folderInfo->m_tstart = static_cast<ULONG>(GetTickCount64() / 1000ull);
}

void CItem::ScanItemsFinalize(CItem* item)
{
    if (item == nullptr) return;
    std::stack<CItem*> queue({item});
    while (!queue.empty()) [[msvc::forceinline_calls]]
    {
        const auto & qitem = queue.top();
        queue.pop();
        qitem->SetDone();
        if (qitem->m_folderInfo == nullptr) continue;
        for (const auto& child : qitem->GetChildren())
        {
            if (!child->IsDone()) queue.push(child);
        }
    }
}

void CItem::ScanItems(BlockingQueue<CItem*> * queue, FinderNtfsContext& contextNtfs, FinderBasicContext& contextBasic)
{
    FinderNtfs finderNtfs(&contextNtfs);
    FinderBasic finderBasic(&contextBasic);

    for (auto itemOpt = queue->Pop(); itemOpt.has_value(); itemOpt = queue->Pop())
    {
        // Fetch item from queue
        CItem* const item = itemOpt.value();

        // Mark the time we started evaluating this node
        item->ResetScanStartTime();

        // Try to load NTFS MFT
        if (item->IsTypeOrFlag(IT_DRIVE) && COptions::UseFastScanEngine)
        {
            contextNtfs.LoadRoot(item);
        }

        if (item->IsTypeOrFlag(IT_DRIVE, IT_DIRECTORY))
        {
            Finder* finder = contextNtfs.IsLoaded() && !item->IsTypeOrFlag(ITF_BASIC) ?
                reinterpret_cast<Finder*>(&finderNtfs) : reinterpret_cast<Finder*>(&finderBasic);

            for (BOOL b = finder->FindFile(item); b; b = finder->FindNext())
            {
                if (finder->IsDirectory())
                {
                    if (COptions::ExcludeHiddenDirectory && finder->IsHidden() ||
                        COptions::ExcludeProtectedDirectory && finder->IsHiddenSystem())
                    {
                        continue;
                    }
  
                    // Exclude directories matching path filter
                    if (!COptions::FilteringExcludeDirsRegex.empty() && std::ranges::any_of(COptions::FilteringExcludeDirsRegex,
                        [&finder](const auto& pattern) { return std::regex_match(finder->GetFilePath(), pattern); }))
                    {
                        continue;
                    }

                    item->UpwardAddFolders(1);
                    if (CItem* newitem = item->AddDirectory(*finder); newitem->GetReadJobs() > 0)
                    {
                        queue->Push(newitem);
                    }
                }
                else
                {
                    if (COptions::ExcludeHiddenFile && finder->IsHidden() ||
                        COptions::ExcludeProtectedFile && finder->IsHiddenSystem() ||
                        COptions::ExcludeSymbolicLinksFile && finder->GetReparseTag() == IO_REPARSE_TAG_SYMLINK)
                    {
                        continue;
                    }

                    // Exclude files matching name filter
                    if (!COptions::FilteringExcludeFilesRegex.empty() && std::ranges::any_of(COptions::FilteringExcludeFilesRegex,
                        [&finder](const auto& pattern) { return std::regex_match(finder->GetFileName(), pattern); }))
                    {
                        continue;
                    }

                    // Exclude files matching size filter
                    if (COptions::FilteringSizeMinimumCalculated > 0 && finder->GetFileSizeLogical() < COptions::FilteringSizeMinimumCalculated)
                    {
                        continue;
                    }

                    item->UpwardAddFiles(1);
                    CItem* newitem = item->AddFile(*finder);
                    CFileDupeControl::Get()->ProcessDuplicate(newitem, queue);
                    CFileTopControl::Get()->ProcessTop(newitem);
                    queue->WaitIfSuspended();
                }

                // Update pacman position
                item->UpwardDrivePacman();
            }
        }
        else if (item->IsTypeOrFlag(IT_FILE))
        {
            // Only used for refreshes
            item->UpdateStatsFromDisk();
            CFileDupeControl::Get()->ProcessDuplicate(item, queue);
            CFileTopControl::Get()->ProcessTop(item);
            item->SetDone();
        }
        else if (item->IsTypeOrFlag(IT_MYCOMPUTER))
        {
            for (const auto & child : item->GetChildren())
            {
                child->UpwardAddReadJobs(1);
                queue->Push(child);
            }
        }
        item->UpwardSubtractReadJobs(1);
        item->UpwardDrivePacman();
    }
}

void CItem::UpwardSetDone() noexcept
{
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        p->SetDone();
    }
}

void CItem::UpwardSetUndone() noexcept
{
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsTypeOrFlag(IT_DRIVE) && p->IsDone())
        {
            if (CItem* unknown = p->FindUnknownItem(); unknown != nullptr)
            {
                p->UpwardSubtractSizePhysical(unknown->GetSizePhysical());
                unknown->SetSizePhysical(0);
            }
        }

        p->SetFlag(ITF_DONE, true);
    }
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
    const auto & children = GetParentDrive()->GetChildren();
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

void CItem::DoHardlinkAdjustment()
{
    if (!IsTypeOrFlag(IT_DRIVE)) return;

    // Create map for duplicate index detection
    std::unordered_map<ULONGLONG, CItem*> indexMapInitial;
    std::unordered_map<ULONGLONG, std::vector<CItem*>> indexDupes;
    indexMapInitial.reserve(GetFilesCount());

    // Look for all indexed items in the tree
    for (std::stack<CItem*> queue({  this }); !queue.empty();)
    {
        // Grab item from queue
        const CItem* qitem = queue.top();
        queue.pop();

        // Descend into child items
        if (qitem->IsLeaf()) continue;
        for (const auto& child : qitem->GetChildren())
        {
            // Add for duplicate index detection
            if (child->IsTypeOrFlag(IT_FILE) && child->GetIndex() > 0)
            {
                if (indexMapInitial.contains(child->GetIndex()))
                {
                    auto & existing = indexDupes[child->GetIndex()];
                    if (existing.empty()) existing.emplace_back(indexMapInitial[child->GetIndex()]);
                    existing.emplace_back(child);
                }
                else indexMapInitial.emplace(child->GetIndex(), child);
            }

            // Do not descend into reparse points since indexes may be from other volumes
            else if (!child->IsLeaf() &&
                (child->GetAttributes() & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
            {
                queue.push(child);
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
        constexpr int INDEX_SET_COUNT = 20;
        const int setIndex = index % INDEX_SET_COUNT;  // 0-19
        CItem* indexSetItem = (setIndex < static_cast<int>(indexSets.size())) ? indexSets[setIndex] : nullptr;
        
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
    } [i] ;
}

std::wstring CItem::UpwardGetPathWithoutBackslash() const
{
    // create vector of the path structure in thread scope so we can reverse it
    thread_local std::vector<const CItem*> pathParts;
    thread_local std::wstring path;

    // make sure the vector is cleared before use
    pathParts.clear();
    path.clear();

    // walk backwards to get a list of pointers to each part of the path
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        pathParts.emplace_back(p);
    }

    // append the strings in reverse order
    for (auto it = pathParts.rbegin(); it != pathParts.rend(); ++it) [[msvc::forceinline_calls]]
    {
        if (const auto & pathPart = *it; pathPart->IsTypeOrFlag(IT_DIRECTORY))
        {
            path.append(pathPart->m_name.get(), pathPart->m_nameLen).append(L"\\");
        }
        else if (pathPart->IsTypeOrFlag(IT_DRIVE))
        {
            path.append(pathPart->m_name.get(), 2).append(L"\\");
        }
        else if (!pathPart->IsTypeOrFlag(IT_MYCOMPUTER))
        {
            path.append(pathPart->m_name.get(), pathPart->m_nameLen);
        }
    }

    // Remove trailing backslashes
    if (const auto pos = path.find_last_not_of(L'\\'); pos != std::wstring::npos)
    {
        path.erase(pos + 1);
    }
    return path;
}

CItem* CItem::AddDirectory(const Finder& finder)
{
    const bool follow = !finder.IsProtectedReparsePoint() &&
        CDirStatApp::Get()->IsFollowingAllowed(finder.GetReparseTag());

    const auto & child = new CItem(IT_DIRECTORY, finder.GetFileName());
    child->SetIndex(finder.GetIndex());
    child->SetLastChange(finder.GetLastWriteTime());
    child->SetAttributes(finder.GetAttributes());
    child->SetReparseTag(finder.GetReparseTag());
    if (finder.IsReserved() || this->IsTypeOrFlag(ITF_RESERVED)) child->SetFlag(ITF_RESERVED);
    if (finder.IsOffVolumeReparsePoint() && follow) child->SetFlag(ITF_BASIC);
    AddChild(child);
    child->UpwardAddReadJobs(follow ? 1 : 0);

    return child;
}

CItem* CItem::AddFile(const Finder& finder)
{
    const auto & child = new CItem(IT_FILE, finder.GetFileName());
    child->SetIndex(finder.GetIndex());
    child->SetSizePhysical(finder.GetFileSizePhysical());
    child->SetSizeLogical(finder.GetFileSizeLogical());
    child->SetLastChange(finder.GetLastWriteTime());
    child->SetAttributes(finder.GetAttributes());
    child->SetReparseTag(finder.GetReparseTag());
    if (finder.IsReserved() || this->IsTypeOrFlag(ITF_RESERVED)) child->SetFlag(ITF_RESERVED);
    child->ExtensionDataAdd();
    AddChild(child);
    child->SetDone();
    return child;
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

CItem* CItem::FindItemByPath(const std::wstring& path) const
{
    auto* pathDrive = GetParentDrive();
    if (pathDrive == nullptr) return nullptr;

    // Split the path into components, filtering out empty strings
    std::vector<std::wstring> components;
    for (auto&& part : path | std::views::split(L'\\')) {
        if (auto str = std::wstring(part.begin(), part.end()); !str.empty()) {
            components.emplace_back(std::move(str));
        }
    }

    if (components.empty()) return nullptr;

    // First component should match the drive (e.g., "C:")
    if (components[0] != GetNameView().substr(0, 2)) return nullptr;

    // Start from the drive and process remaining components
    CItem* current = pathDrive;
    for (const auto i : std::views::iota(1u, components.size()))
    {
        if (current->IsLeaf()) return nullptr;

        // Find the matching child using GetNameView for comparison
        auto it = std::ranges::find_if(current->GetChildren(),
            [&](const CItem* child) { return child->GetNameView() == components[i]; });
        if (it == current->GetChildren().end()) return nullptr;
        current = *it;
    }

    return current;
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
    for (std::stack<CItem*> itemStack({ driveItem }); !itemStack.empty();)
    {
        CItem* current = itemStack.top();
        itemStack.pop();

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
                itemStack.push(child);
            }
        }
    }

    return results;
}

CItem* CItem::GetLinkedItem()
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
