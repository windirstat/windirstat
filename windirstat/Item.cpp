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
#include "Item.h"
#include "FinderBasic.h"
#include "FinderNtfs.h"

// --- Construction / Destruction ---

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

// --- Hierarchy ---

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

// --- Size & Stats ---

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

void CItem::UpwardAddSizePhysical(const ULONGLONG bytes) noexcept
{
    if (bytes == 0) return;

    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        p->m_sizePhysical += bytes;
        if (p->IsTypeOrFlag(ITF_HARDLINK)) break;
    }
}

void CItem::UpwardSubtractSizePhysical(const ULONGLONG bytes) noexcept
{
    if (bytes == 0) return;

    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        ASSERT(bytes <= p->m_sizePhysical);
        p->m_sizePhysical -= bytes;
        if (p->IsTypeOrFlag(ITF_HARDLINK)) break;
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

double CItem::GetFraction() const noexcept
{
    if (!GetParent() || GetParent()->GetSizePhysical() == 0)
    {
        return 1.0;
    }
    return static_cast<double>(GetSizePhysical()) /
        static_cast<double>(GetParent()->GetSizePhysical());
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

void CItem::ExtensionDataAdd()
{
    if (!IsTypeOrFlag(IT_FILE) || IsTypeOrFlag(ITF_EXTDATA)) return;
    const auto record = CDirStatDoc::Get()->GetExtensionDataRecord(GetExtension());
    record->AddFile(GetSizeLogical());
    SetFlag(ITF_EXTDATA);
}

void CItem::ExtensionDataRemove()
{
    if (!IsTypeOrFlag(IT_FILE) || !IsTypeOrFlag(ITF_EXTDATA)) return;
    const auto record = CDirStatDoc::Get()->GetExtensionDataRecord(GetExtension());
    record->RemoveFile(GetSizeLogical());
    if (record->GetFiles() == 0) CDirStatDoc::Get()->GetExtensionData()->erase(GetExtension());
    SetFlag(ITF_EXTDATA, true);
}

void CItem::ExtensionDataProcessChildren(const bool remove)
{
    std::vector childStack({ this });
    while (!childStack.empty()) [[msvc::forceinline_calls]]
    {
        const auto& item = childStack.back();
        childStack.pop_back();

        if (item->IsTypeOrFlag(IT_MYCOMPUTER, IT_DIRECTORY, IT_DRIVE))
        {
            for (const auto& child : item->GetChildren())
            {
                childStack.push_back(child);
            }
        }
        else if (item->IsTypeOrFlag(IT_FILE))
        {
            remove ? item->ExtensionDataRemove() : item->ExtensionDataAdd();
        }
    }
}

// --- Attributes & Properties ---

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

void CItem::UpwardUpdateLastChange(const FILETIME& t) noexcept
{
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (t > p->m_lastChange) p->m_lastChange = t;
    }
}

void CItem::UpwardRecalcLastChange()
{
    // ignore current object in recalculation 
    for (auto p = GetParent(); p != nullptr; p = p->GetParent())
    {
        const auto newMax = (std::ranges::max)(
            p->GetChildren() | std::views::transform(&CItem::m_lastChange));

        if (p->m_lastChange == newMax) break;
        p->m_lastChange = newMax;
    }
}

// --- Paths & Names ---

void CItem::SetName(const std::wstring_view name)
{
    m_nameLen = static_cast<std::uint8_t>(name.size());
    m_name = std::make_unique_for_overwrite<wchar_t[]>(m_nameLen + 1);
    while (m_nameLen > 0 && name[m_nameLen - 1] == L'\\') m_nameLen--;
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

std::wstring CItem::GetPath() const
{
    if (IsTypeOrFlag(IT_HLINKS_SET, IT_HLINKS_IDX))
    {
        return {};
    }

    std::wstring path = GetPathWithoutSlash();
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

bool CItem::HasUncPath() const
{
    return GetPath().starts_with(L"\\\\");
}

CItem* CItem::FindItemByPath(const std::wstring& path) const
{
    auto* pathDrive = GetParentDrive();
    if (pathDrive == nullptr) return nullptr;

    // Split the path into components, filtering out empty strings
    const std::vector<std::wstring> components = SplitString(path, wds::chrBackslash);
    if (components.empty()) return nullptr;

    // First component should match the drive (e.g., "C:")
    if (components[0] != GetDrive(GetNameView())) return nullptr;

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

std::wstring CItem::GetPathWithoutSlash() const
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

// --- Scanning & Done State ---

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

ULONG CItem::GetReadJobs() const noexcept
{
    if (IsLeaf()) return 0;
    return m_folderInfo->m_jobs;
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
                UpwardSubtractSizePhysical(GetSizePhysicalRaw());
                UpwardSubtractSizeLogical(GetSizeLogical());
                UpwardAddSizePhysical(finder.GetFileSizePhysical());
                UpwardAddSizeLogical(finder.GetFileSizeLogical());
                SetIndex(finder.GetIndex());
                ExtensionDataAdd();
            }
        }
    }
    else if (IsTypeOrFlag(IT_DRIVE))
    {
        const SmartPointer<HANDLE> handle(CloseHandle, CreateFile(GetPathLong().c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
        if (handle != INVALID_HANDLE_VALUE)
        {
            GetFileTime(handle, nullptr, nullptr, &m_lastChange);
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

void CItem::ScanItemsFinalize(CItem* item)
{
    if (item == nullptr) return;
    std::vector<CItem*> queue({item});
    while (!queue.empty()) [[msvc::forceinline_calls]]
    {
        const auto & qitem = queue.back();
        queue.pop_back();
        qitem->SetDone();
        if (qitem->m_folderInfo == nullptr) continue;
        for (const auto& child : qitem->GetChildren())
        {
            if (!child->IsDone()) queue.push_back(child);
        }
    }
}

std::vector<CItem*> CItem::GetItemsRecursive(const std::vector<CItem*>& initialItems, const std::function<bool(CItem*)>& task)
{
    std::vector<CItem*> files;
    std::vector childStack{ initialItems };
    while (!childStack.empty())
    {
        const auto& item = childStack.back();
        childStack.pop_back();
        if (item->HasChildren())
        {
            for (const auto& child : item->GetChildren())
            {
                childStack.push_back(child);
            }
        }
        else if (task(item))
        {
            files.emplace_back(item);
        }
    }
    return files;
}
