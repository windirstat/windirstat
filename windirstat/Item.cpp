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
#include "SelectObject.h"
#include "FinderBasic.h"
#include "FinderNtfs.h"

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

CItem::CItem(const ITEMTYPE type, const std::wstring & name) : m_Type(type)
{
    if (IsType(IT_MYCOMPUTER, IT_DRIVE, IT_DIRECTORY))
    {
        m_FolderInfo = std::make_unique<CHILDINFO>();

        // My computer node will never have these attributes set externally
        if (IsType(IT_MYCOMPUTER))
        {
            m_Attributes = 0;
        }
    }

    if (IsType(IT_DRIVE))
    {
        // Store drive paths with a backslash
        std::wstring nameTmp = name;
        if (nameTmp.ends_with(L":")) nameTmp.append(L"\\");

        // The name string on the drive is two parts separated by a pipe. For example,
        // C:\|Local Disk (C:) is the true path followed by the name description
        SetName(std::format(L"{:.2}|{}", nameTmp, FormatVolumeNameOfRootPath(nameTmp)));
        m_Attributes = LOWORD(GetFileAttributesW(GetPathLong().c_str()));

        CreateHardlinksItem();
    }
    else
    {
        SetName(name);
    }
}

CItem::CItem(const ITEMTYPE type, const std::wstring& name, const FILETIME lastChange,
    const ULONGLONG sizePhysical, const ULONGLONG sizeLogical, const ULONG index,
    const DWORD attributes, const ULONG files, const ULONG subdirs)
{
    SetName(name);
    m_Type = type;
    m_LastChange = lastChange;
    m_SizePhysical = sizePhysical;
    m_SizeLogical = sizeLogical;
    m_Index = index;
    m_Attributes = LOWORD(attributes);

    if (IsType(IT_DRIVE))
    {
        SetName(std::format(L"{:.2}|{}", name, FormatVolumeNameOfRootPath(name)));
    }

    if (IsType(IT_MYCOMPUTER, IT_DRIVE, IT_DIRECTORY))
    {
        m_FolderInfo = std::make_unique<CHILDINFO>();
        m_FolderInfo->m_Subdirs = subdirs;
        m_FolderInfo->m_Files = files;
    }
}

CItem::~CItem()
{
    if (m_FolderInfo != nullptr)
    {
        for (const auto& child : m_FolderInfo->m_Children)
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
        for (int i = 0; i < GetIndent(); i++)
        {
            rc.left += rc.Width() / 10;
        }

        DrawPercentage(pdc, rc, GetFraction(), GetPercentageColor());
    }
    return true;
}

std::wstring CItem::GetText(const int subitem) const
{
    switch (subitem)
    {
    case COL_SIZE_PHYSICAL: return FormatBytes(GetSizePhysical());
    case COL_SIZE_LOGICAL: return FormatBytes(GetSizeLogical());

    case COL_NAME:
        if (IsType(IT_DRIVE))
        {
            return GetName().substr(std::size(L"?:"));
        }
        return GetName();

    case COL_OWNER:
        if (IsType(IT_FILE, IT_DIRECTORY))
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
        if (!IsType(IT_FILE, IT_FREESPACE, IT_UNKNOWN, IT_HARDLINKS))
        {
            return FormatCount(GetItemsCount());
        }
        break;

    case COL_FILES:
        if (!IsType(IT_FILE, IT_FREESPACE, IT_UNKNOWN, IT_HARDLINKS))
        {
            return FormatCount(GetFilesCount());
        }
        break;

    case COL_FOLDERS:
        if (!IsType(IT_FILE, IT_FREESPACE, IT_UNKNOWN, IT_HARDLINKS))
        {
            return FormatCount(GetFoldersCount());
        }
        break;

    case COL_LAST_CHANGE:
        if (!IsType(IT_FREESPACE, IT_UNKNOWN, IT_HARDLINKS))
        {
            return FormatFileTime(m_LastChange);
        }
        break;

    case COL_ATTRIBUTES:
        if (!IsType(IT_FREESPACE, IT_UNKNOWN, IT_MYCOMPUTER, IT_HARDLINKS))
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
            return signum(_wcsicmp(m_Name.get(), other->m_Name.get()));
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
            if (m_LastChange < other->m_LastChange)
            {
                return -1;
            }
            if (m_LastChange == other->m_LastChange)
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
    if (m_VisualInfo->icon != nullptr)
    {
        return m_VisualInfo->icon;
    }

    if (IsType(IT_MYCOMPUTER))
    {
        m_VisualInfo->icon = GetIconHandler()->GetMyComputerImage();
        return m_VisualInfo->icon;
    }
    if (IsType(IT_FREESPACE))
    {
        m_VisualInfo->icon = GetIconHandler()->GetFreeSpaceImage();
        return m_VisualInfo->icon;
    }
    if (IsType(IT_UNKNOWN))
    {
        m_VisualInfo->icon = GetIconHandler()->GetUnknownImage();
        return m_VisualInfo->icon;
    }
    if (IsType(IT_HARDLINKS))
    {
        m_VisualInfo->icon = GetIconHandler()->GetHardlinksImage();
        return m_VisualInfo->icon;
    }

    if (IsReparseType(ITRP_MOUNT))
    {
        m_VisualInfo->icon = GetIconHandler()->GetMountPointImage();
        return m_VisualInfo->icon;
    }
    if (IsReparseType(ITRP_SYMLINK, ITRP_JUNCTION))
    {
        constexpr DWORD mask = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
        const bool osFile = (GetAttributes() & mask) == mask;
        m_VisualInfo->icon = osFile ? GetIconHandler()->GetJunctionProtectedImage() : GetIconHandler()->GetJunctionImage();
        return m_VisualInfo->icon;
    }

    CDirStatApp::Get()->GetIconHandler()->DoAsyncShellInfoLookup(std::make_tuple(const_cast<CItem*>(this),
        m_VisualInfo->control, GetPath(), GetAttributes(), &m_VisualInfo->icon, nullptr));

    return nullptr;
}

void CItem::DrawAdditionalState(CDC* pdc, const CRect& rcLabel) const
{
    if (!IsRootItem() && this == CDirStatDoc::GetDocument()->GetZoomItem())
    {
        CRect rc = rcLabel;
        rc.InflateRect(1, 0);
        rc.bottom++;

        CSelectStockObject sobrush(pdc, NULL_BRUSH);
        CPen pen(PS_SOLID, 2, CDirStatDoc::GetDocument()->GetZoomColor());
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
    if (IsType(IT_MYCOMPUTER))
    {
        return GetProgressRangeMyComputer();
    }
    if (IsType(IT_DRIVE))
    {
        return GetProgressRangeDrive();
    }
    if (IsType(IT_FILE, IT_DIRECTORY))
    {
        return 0;
    }

    ASSERT(FALSE);
    return 0;
}

ULONGLONG CItem::GetProgressPos() const
{
    if (IsType(IT_MYCOMPUTER))
    {
        ULONGLONG pos = 0;
        for (const auto& child : GetChildren())
        {
            pos += child->GetProgressPos();
        }
        return pos;
    }
    if (IsType(IT_DRIVE))
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
    if (IsType(IT_DIRECTORY, IT_FILE))
    {
        FinderBasic finder;
        if (finder.FindFile(GetFolderPath(), HasFlag(ITF_ROOTITEM) ? std::wstring() : GetName(), GetAttributes()))
        {
            SetLastChange(finder.GetLastWriteTime());
            SetAttributes(finder.GetAttributes());

            if (IsType(IT_FILE))
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
    else if (IsType(IT_DRIVE))
    {
        SmartPointer<HANDLE> handle(CloseHandle, CreateFile(GetPathLong().c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
        if (handle != INVALID_HANDLE_VALUE)
        {
            GetFileTime(handle, nullptr, nullptr, &m_LastChange);
        }
    }
}

const std::vector<CItem*>& CItem::GetChildren() const
{
    ASSERT(m_FolderInfo != nullptr);
    return m_FolderInfo->m_Children;
}

CItem* CItem::GetParent() const
{
    return reinterpret_cast<CItem*>(CTreeListItem::GetParent());
}

CItem* CItem::GetParentDrive() const
{
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsType(IT_DRIVE)) return const_cast<CItem*>(p);
    }
    return nullptr;
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
    m_FolderInfo->m_Children.push_back(child);
  
    if (IsVisible() && IsExpanded())
    {
        CMainFrame::Get()->InvokeInMessageThread([this, child]
        {
            CFileTreeControl::Get()->OnChildAdded(this, child);
        });
    }
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

    auto& children = m_FolderInfo->m_Children;
    if (auto it = std::ranges::find(children, child); it != children.end())
    {
        children.erase(it);
    }

    // Check if this child is a hardlink
    if (child->HasFlag(ITF_HARDLINK) && child->GetIndex() > 0)
    {
        if (const auto sameIndexItems = child->FindItemsBySameIndex(); sameIndexItems.size() == 1)
        {
            // Get the remaining item and clear its hardlink flag
            CItem* remainingItem = sameIndexItems[0];
            remainingItem->SetFlag(ITF_HARDLINK, true);

            // Subtract the physical size from the hardlinks node since it's no longer a hardlink
            if (CItem* hardlinksItem = remainingItem->FindHardlinksItem(); hardlinksItem != nullptr)
            {
                hardlinksItem->UpwardSubtractSizePhysical(remainingItem->GetSizePhysicalRaw());
            }
        }
    }

    delete child;
}

void CItem::RemoveAllChildren()
{
    if (IsRootItem())
    {
        CDirStatDoc::GetDocument()->GetExtensionData()->clear();
    }

    if (IsLeaf()) return;
    CMainFrame::Get()->InvokeInMessageThread([this]
    {
        CFileTreeControl::Get()->OnRemovingAllChildren(this);
    });

    for (const auto& child : m_FolderInfo->m_Children)
    {
        delete child;
    }
    m_FolderInfo->m_Children.clear();
}

void CItem::UpwardAddFolders(const ULONG dirCount)
{
    if (dirCount == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsType(IT_FILE)) continue;
        p->m_FolderInfo->m_Subdirs += dirCount;
    }
}

void CItem::UpwardSubtractFolders(const ULONG dirCount)
{
    if (dirCount == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        ASSERT(p->m_FolderInfo->m_Subdirs - dirCount >= 0);
        if (p->IsType(IT_FILE)) continue;
        p->m_FolderInfo->m_Subdirs -= dirCount;
    }
}

void CItem::UpwardAddFiles(const ULONG fileCount)
{
    if (fileCount == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsType(IT_FILE)) continue;
        p->m_FolderInfo->m_Files += fileCount;
    }
}

void CItem::UpwardSubtractFiles(const ULONG fileCount)
{
    if (fileCount == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsType(IT_FILE)) continue;
        ASSERT(p->m_FolderInfo->m_Files - fileCount >= 0);
        p->m_FolderInfo->m_Files -= fileCount;
    }
}

void CItem::UpwardAddSizePhysical(const ULONGLONG bytes)
{
    if (bytes == 0) return;

    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        p->m_SizePhysical += bytes;
    }
}

void CItem::UpwardSubtractSizePhysical(const ULONGLONG bytes)
{
    if (bytes == 0) return;

    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        ASSERT(bytes <= p->m_SizePhysical);
        p->m_SizePhysical -= bytes;
    }
}

void CItem::UpwardAddSizeLogical(const ULONGLONG bytes)
{
    if (bytes == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        p->m_SizeLogical += bytes;
    }
}

void CItem::UpwardSubtractSizeLogical(const ULONGLONG bytes)
{
    if (bytes == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        ASSERT(p->m_SizeLogical - bytes >= 0);
        p->m_SizeLogical -= bytes;
    }
}

void CItem::ExtensionDataAdd() const
{
    if (!IsType(IT_FILE)) return;
    const auto record = CDirStatDoc::GetDocument()->GetExtensionDataRecord(GetExtension());
    record->bytes += GetSizePhysical();
    record->files += 1;
}

void CItem::ExtensionDataRemove() const
{
    if (!IsType(IT_FILE)) return;
    const auto record = CDirStatDoc::GetDocument()->GetExtensionDataRecord(GetExtension());
    record->bytes -= GetSizePhysical();
    record->files -= 1;
    if (record->files == 0) CDirStatDoc::GetDocument()->GetExtensionData()->erase(GetExtension());
}

void CItem::ExtensionDataProcessChildren(const bool remove) const
{
    std::stack<const CItem*> childStack({ this });
    while (!childStack.empty())
    {
        const auto& item = childStack.top();
        childStack.pop();

        if (item->IsType(IT_MYCOMPUTER, IT_DIRECTORY, IT_DRIVE))
        {
            for (const auto& child : item->GetChildren())
            {
                childStack.push(child);
            }
        }
        else if (item->IsType(IT_FILE))
        {
            remove ? item->ExtensionDataRemove() : item->ExtensionDataAdd();
        }
    }
}

void CItem::UpwardAddReadJobs(const ULONG count)
{
    if (IsLeaf() || count == 0) return;
    if (m_FolderInfo->m_Jobs == 0) m_FolderInfo->m_Tstart = static_cast<ULONG>(GetTickCount64() / 1000ull);
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsType(IT_FILE)) continue;
        p->m_FolderInfo->m_Jobs += count;
    }
}

void CItem::UpwardSubtractReadJobs(const ULONG count)
{
    if (count == 0 || IsType(IT_FILE)) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        ULONG previous = p->m_FolderInfo->m_Jobs.fetch_sub(count);
        if (previous >= count && previous - count == 0)
        {
            p->SetDone();
        }
    }
}

// This method increases the last change
void CItem::UpwardUpdateLastChange(const FILETIME& t)
{
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (FileTimeIsGreater(t, p->m_LastChange)) p->m_LastChange = t;
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
            if (FileTimeIsGreater(child->m_LastChange, p->m_LastChange))
                p->m_LastChange = child->m_LastChange;
        }
    }
}

ULONGLONG CItem::GetSizePhysical() const
{
    return HasFlag(ITF_HARDLINK) ? 0 : m_SizePhysical.load();
}

ULONGLONG CItem::GetSizeLogical() const
{
    return m_SizeLogical;
}

ULONGLONG CItem::GetSizePhysicalRaw() const
{
    return m_SizePhysical;
}

void CItem::SetSizePhysical(const ULONGLONG size)
{
    ASSERT(size >= 0);
    m_SizePhysical = size;
}

void CItem::SetSizeLogical(const ULONGLONG size)
{
    ASSERT(size >= 0);
    m_SizeLogical = size;
}

ULONG CItem::GetReadJobs() const
{
    if (IsLeaf()) return 0;
    return m_FolderInfo->m_Jobs;
}

FILETIME CItem::GetLastChange() const
{
    return m_LastChange;
}

void CItem::SetLastChange(const FILETIME& t)
{
    m_LastChange = t;
}

void CItem::SetAttributes(const DWORD attr)
{
    m_Attributes = LOWORD(attr);
}

DWORD CItem::GetAttributes() const
{
    return m_Attributes == LOWORD(INVALID_FILE_ATTRIBUTES)
        ? INVALID_FILE_ATTRIBUTES : m_Attributes;
}

void CItem::SetIndex(const DWORD index)
{
    m_Index = index;
}

DWORD CItem::GetIndex() const
{
    return m_Index;
}

DWORD CItem::GetReparseTag() const
{
    if (IsReparseType(ITRP_SYMLINK)) return IO_REPARSE_TAG_SYMLINK;
    if (IsReparseType(ITRP_MOUNT)) return IO_REPARSE_TAG_MOUNT_POINT;
    if (IsReparseType(ITRP_JUNCTION)) return IO_REPARSE_TAG_JUNCTION_POINT;
    if (IsReparseType(ITRP_CLOUD)) return IO_REPARSE_TAG_CLOUD_MASK;
    return 0;
}

void CItem::SetReparseTag(const DWORD reparseType)
{
    if (reparseType == 0) (void) false;
    else if (reparseType == IO_REPARSE_TAG_SYMLINK) SetReparseType(ITRP_SYMLINK);
    else if (reparseType == IO_REPARSE_TAG_MOUNT_POINT) SetReparseType(ITRP_MOUNT);
    else if (reparseType == IO_REPARSE_TAG_JUNCTION_POINT) SetReparseType(ITRP_JUNCTION);
    else if ((reparseType & 0xF00000FF) == IO_REPARSE_TAG_CLOUD) SetReparseType(ITRP_CLOUD);
}

// Returns a value which resembles sorting of RHSACE considering gaps
USHORT CItem::GetSortAttributes() const
{
    USHORT ret = 0;

    // We want to enforce the order RHSACE with R being the highest priority
    // attribute and E being the lowest priority attribute.
    ret |= m_Attributes & FILE_ATTRIBUTE_READONLY    ? 1 << 6 : 0; // R
    ret |= m_Attributes & FILE_ATTRIBUTE_HIDDEN      ? 1 << 5 : 0; // H
    ret |= m_Attributes & FILE_ATTRIBUTE_SYSTEM      ? 1 << 4 : 0; // S
    ret |= m_Attributes & FILE_ATTRIBUTE_ARCHIVE     ? 1 << 3 : 0; // A
    ret |= m_Attributes & FILE_ATTRIBUTE_COMPRESSED  ? 1 << 2 : 0; // C
    ret |= m_Attributes & FILE_ATTRIBUTE_ENCRYPTED   ? 1 << 1 : 0; // E
    ret |= m_Attributes & FILE_ATTRIBUTE_SPARSE_FILE ? 1 << 0 : 0; // Z

    return ret;
}

double CItem::GetFraction() const
{
    if (!GetParent() || GetParent()->GetSizePhysical() == 0)
    {
        return 1.0;
    }
    return static_cast<double>(GetSizePhysical()) /
        static_cast<double>(GetParent()->GetSizePhysical());
}

bool CItem::IsRootItem() const
{
    return HasFlag(ITF_ROOTITEM);
}

std::wstring CItem::GetPath() const
{
    std::wstring path = UpwardGetPathWithoutBackslash();
    if (IsType(IT_DRIVE))
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
    std::wstring & ret = (force) ? tmp : m_VisualInfo->owner;
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
    if (IsType(IT_FILE))
    {
        const auto i = path.find_last_of(wds::chrBackslash);
        ASSERT(i != std::wstring::npos);
        path = path.substr(0, i + 1);
    }

    return path;
}

void CItem::SetName(std::wstring_view name)
{
    m_NameLen = static_cast<std::uint8_t>(name.size());
    m_Name = std::make_unique_for_overwrite<wchar_t[]>(m_NameLen + 1);
    if (m_NameLen) std::wmemcpy(m_Name.get(), name.data(), m_NameLen);
    m_Name[m_NameLen] = L'\0';
}

std::wstring CItem::GetName() const
{
    return { m_Name.get(), m_NameLen };
}

std::wstring CItem::GetExtension() const
{
    if (!IsType(IT_FILE)) return GetName();
    const auto & extName = GetName();
    const auto pos = extName.rfind('.');
    if (pos == std::string::npos) return {};
    std::wstring extLower = extName.substr(pos);
    _wcslwr_s(extLower.data(), extLower.size() + 1);
    return extLower;
}

ULONG CItem::GetFilesCount() const
{
    if (IsLeaf()) return 0;
    return m_FolderInfo->m_Files;
}

ULONG CItem::GetFoldersCount() const
{
    if (IsLeaf()) return 0;
    return m_FolderInfo->m_Subdirs;
}

ULONGLONG CItem::GetItemsCount() const
{
    if (IsLeaf()) return 0;
    return static_cast<ULONGLONG>(m_FolderInfo->m_Files) + static_cast<ULONGLONG>(m_FolderInfo->m_Subdirs);
}

void CItem::SetDone()
{
    if (IsDone())
    {
        return;
    }

    if (IsType(IT_DRIVE))
    {
        UpdateFreeSpaceItem();
        UpdateUnknownItem();
    }

    // Sort and set finish time
    if (!IsLeaf())
    {
        COptions::TreeMapUseLogical ? SortItemsBySizeLogical() : SortItemsBySizePhysical();
        m_FolderInfo->m_Tfinish = static_cast<ULONG>(GetTickCount64() / 1000ull);
    }

    // Mark as done just so other functions do not sort at the same time
    SetFlag(ITF_DONE);
}

void CItem::SortItemsBySizePhysical() const
{
    if (IsLeaf()) return;

    // sort by size for proper treemap rendering
    m_FolderInfo->m_Children.shrink_to_fit();
    std::ranges::sort(m_FolderInfo->m_Children, [](auto item1, auto item2)
    {
        return item1->GetSizePhysical() > item2->GetSizePhysical(); // biggest first
    });
}

void CItem::SortItemsBySizeLogical() const
{
    if (IsLeaf()) return;
    
    // sort by size for proper treemap rendering
    m_FolderInfo->m_Children.shrink_to_fit();
    std::ranges::sort(m_FolderInfo->m_Children, [](auto item1, auto item2)
    {
        return item1->GetSizeLogical() > item2->GetSizeLogical(); // biggest first
    });
}

ULONGLONG CItem::GetTicksWorked() const
{
    if (IsLeaf()) return 0;
    return m_FolderInfo->m_Tfinish > 0 ? (m_FolderInfo->m_Tfinish - m_FolderInfo->m_Tstart) :
        (m_FolderInfo->m_Tstart > 0) ? ((GetTickCount64() / 1000ull) - m_FolderInfo->m_Tstart) : 0;
}

void CItem::ResetScanStartTime() const
{
    if (IsLeaf()) return;
    m_FolderInfo->m_Tfinish = 0;
    m_FolderInfo->m_Tstart = static_cast<ULONG>(GetTickCount64() / 1000ull);
}

void CItem::ScanItemsFinalize(CItem* item)
{
    if (item == nullptr) return;
    std::stack<CItem*> queue({item});
    while (!queue.empty())
    {
        const auto & qitem = queue.top();
        queue.pop();
        qitem->SetDone();
        if (qitem->m_FolderInfo == nullptr) continue;
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

    while (CItem * const item = queue->Pop())
    {
        // Mark the time we started evaluating this node
        item->ResetScanStartTime();

        // Try to load NTFS MFT
        if (item->IsType(IT_DRIVE) && COptions::UseFastScanEngine)
        {
            contextNtfs.LoadRoot(item);
        }

        if (item->IsType(IT_DRIVE, IT_DIRECTORY))
        {
            Finder* finder = contextNtfs.IsLoaded && !item->HasFlag(ITF_BASIC) ?
                reinterpret_cast<Finder*>(&finderNtfs) : reinterpret_cast<Finder*>(&finderBasic);

            for (BOOL b = finder->FindFile(item); b; b = finder->FindNext())
            {
                if (finder->IsDots())
                {
                    continue;
                }

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
        else if (item->IsType(IT_FILE))
        {
            // Only used for refreshes
            item->UpdateStatsFromDisk();
            CFileDupeControl::Get()->ProcessDuplicate(item, queue);
            CFileTopControl::Get()->ProcessTop(item);
            item->SetDone();
        }
        else if (item->IsType(IT_MYCOMPUTER))
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

void CItem::UpwardSetDone()
{
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        p->SetDone();
    }
}

void CItem::UpwardSetUndone()
{
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsType(IT_DRIVE) && p->IsDone())
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
        if (!p->IsType(IT_DRIVE)) continue;

        // There is no cross-platform way to consistently identify the recycle bin 
        // so attempt to find an item with the most probable values
        for (const std::wstring& possible : { L"$RECYCLE.BIN", L"RECYCLER", L"RECYCLED" })
        {
            for (const auto& child : p->GetChildren())
            {
                if (child->IsType(IT_DIRECTORY) && _wcsicmp(child->GetName().c_str(), possible.c_str()) == 0)
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
    ASSERT(IsType(IT_DRIVE));

    UpwardSetUndone();

    auto [total, free] = CDirStatApp::GetFreeDiskSpace(GetPath());

    const auto freespace = new CItem(IT_FREESPACE, Localization::Lookup(IDS_FREESPACE_ITEM));
    freespace->SetSizePhysical(free);
    freespace->SetDone();

    AddChild(freespace);
}

CItem* CItem::FindFreeSpaceItem() const
{
    const auto it = std::ranges::find_if(GetChildren(),
        [](const auto& child) { return child->IsType(IT_FREESPACE); });

    return it != GetChildren().end() ? *it : nullptr;
}

void CItem::UpdateFreeSpaceItem()
{
    if (IsType(IT_MYCOMPUTER))
    {
        for (const auto& child : GetChildren())
        {
            if (child->IsType(IT_DRIVE))
                child->UpdateFreeSpaceItem();
        }
    }
    else if (IsType(IT_DRIVE))
    {
        auto [total, free] = CDirStatApp::GetFreeDiskSpace(GetPath());

        // Recreate name based on updated free space and percentage
        SetName(std::format(L"{:.2}|{} - {} ({:.1f}%)", GetName(),
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
    ASSERT(IsType(IT_DRIVE));

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
    ASSERT(IsType(IT_DRIVE));

    if (const auto freespace = FindFreeSpaceItem(); freespace != nullptr)
    {
        UpwardSetUndone();
        UpwardSubtractSizePhysical(freespace->GetSizePhysical());
        RemoveChild(freespace);
    }
}

void CItem::CreateUnknownItem()
{
    ASSERT(IsType(IT_DRIVE));

    UpwardSetUndone();

    const auto unknown = new CItem(IT_UNKNOWN, Localization::Lookup(IDS_UNKNOWN_ITEM));
    unknown->SetDone();

    AddChild(unknown);
}

CItem* CItem::FindUnknownItem() const
{
    const auto it = std::ranges::find_if(GetChildren(),
        [](const auto& child) { return child->IsType(IT_UNKNOWN); });

    return it != GetChildren().end() ? *it : nullptr;
}

void CItem::RemoveUnknownItem()
{
    ASSERT(IsType(IT_DRIVE));

    if (const auto unknown = FindUnknownItem(); unknown != nullptr)
    {
        UpwardSetUndone();
        UpwardSubtractSizePhysical(unknown->GetSizePhysical());
        RemoveChild(unknown);
    }
}

void CItem::CreateHardlinksItem()
{
    ASSERT(IsType(IT_DRIVE));

    const auto hardlinks = new CItem(IT_HARDLINKS, Localization::Lookup(IDS_HARDLINKS_ITEM));
    AddChild(hardlinks);
}

CItem* CItem::FindHardlinksItem() const
{
    const auto & children = GetParentDrive()->GetChildren();
    const auto it = std::ranges::find_if(children,
        [](const auto& child) { return child->IsType(IT_HARDLINKS); });

    return it != children.end() ? *it : nullptr;
}

void CItem::DoHardlinkAdjustment()
{
    if (!IsType(IT_DRIVE)) return;

    // Create map for duplicate index detection
    std::unordered_map<ULONG, CItem*> indexMapInitial;
    std::unordered_map<ULONG, std::vector<CItem*>> indexDupes;
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
            if (child->IsType(IT_FILE) && child->GetIndex() > 0)
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

    // Process hardlinks
    const auto hardlinksItem = FindHardlinksItem();
    auto itemSizeTotal = 0ull;
    for (const auto& list : indexDupes | std::views::values)
    {
        bool skipAdd = false;
        auto itemSize = 0ull;
        for (auto* item : list)
        {
            if (item->HasFlag(ITF_HARDLINK)) { skipAdd = true; continue; }
            itemSize = max(itemSize, item->GetSizePhysicalRaw());
            item->GetParent()->UpwardSubtractSizePhysical(item->GetSizePhysicalRaw());
            item->GetParent()->UpwardSetUndone();
            item->SetFlag(ITF_HARDLINK);
        }

        if (!skipAdd) itemSizeTotal += itemSize;
    }

    hardlinksItem->UpwardAddSizePhysical(itemSizeTotal);
    hardlinksItem->UpwardSetUndone();
}

ULONGLONG CItem::GetProgressRangeMyComputer() const
{
    ASSERT(IsType(IT_MYCOMPUTER));

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

    ASSERT(total >= 0);
    return total;
}

COLORREF CItem::GetGraphColor() const
{
    if (IsType(IT_UNKNOWN))
    {
        return RGB(255, 255, 0) | CTreeMap::COLORFLAG_LIGHTER;
    }

    if (IsType(IT_FREESPACE))
    {
        return RGB(100, 100, 100) | CTreeMap::COLORFLAG_DARKER;
    }

    if (IsType(IT_HARDLINKS))
    {
        return RGB(200, 150, 100) | CTreeMap::COLORFLAG_LIGHTER;
    }

    if (IsType(IT_FILE))
    {
        return CDirStatDoc::GetDocument()->GetCushionColor(GetExtension());
    }

    return RGB(0, 0, 0);
 }

bool CItem::MustShowReadJobs() const
{
    if (GetParent() != nullptr)
    {
        return !GetParent()->IsDone();
    }

    return !IsDone();
}

COLORREF CItem::GetPercentageColor() const
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
    for (auto it = pathParts.rbegin(); it != pathParts.rend(); ++it)
    {
        if (const auto & pathPart = *it; pathPart->IsType(IT_DIRECTORY))
        {
            path.append(pathPart->m_Name.get(), pathPart->m_NameLen).append(L"\\");
        }
        else if (pathPart->IsType(IT_FILE, IT_HARDLINKS, IT_UNKNOWN, IT_FREESPACE))
        {
            path.append(pathPart->m_Name.get(), pathPart->m_NameLen);
        }
        else if (pathPart->IsType(IT_DRIVE))
        {
            path.append(pathPart->m_Name.get(), 2).append(L"\\");
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
    if (finder.IsOffVolumeReparsePoint() && follow) child->SetFlag(ITF_BASIC);
    AddChild(child);
    child->UpwardAddReadJobs(follow ? 1 : 0);

    return child;
}

CItem* CItem::AddFile(Finder& finder)
{
    const auto & child = new CItem(IT_FILE, finder.GetFileName());
    child->SetIndex(finder.GetIndex());
    child->SetSizePhysical(finder.GetFileSizePhysical());
    child->SetSizeLogical(finder.GetFileSizeLogical());
    child->SetLastChange(finder.GetLastWriteTime());
    child->SetAttributes(finder.GetAttributes());
    child->SetReparseTag(finder.GetReparseTag());
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
        if (p->IsType(IT_FILE) || !p->IsVisible()) continue;
        if (p->GetReadJobs() == 0) p->StopPacman();
        else p->DrivePacman();
    }
}

std::mutex CItem::m_HashMutex;
BCRYPT_ALG_HANDLE CItem::m_HashAlgHandle = nullptr;
DWORD CItem::m_HashLength = 0;

std::vector<BYTE> CItem::GetFileHash(ULONGLONG hashSizeLimit, BlockingQueue<CItem*>* queue)
{
    thread_local std::vector<BYTE> FileBuffer(1024ull * 1024ull);
    thread_local std::vector<BYTE> Hash;
    thread_local SmartPointer<BCRYPT_HASH_HANDLE> HashHandle(BCryptDestroyHash);

    // Initialize shared structures
    if (m_HashLength == 0) if (std::scoped_lock guard(m_HashMutex); m_HashLength == 0)
    {
        DWORD ResultLength = 0;
        if (BCryptOpenAlgorithmProvider(&m_HashAlgHandle, BCRYPT_SHA512_ALGORITHM, MS_PRIMITIVE_PROVIDER, BCRYPT_HASH_REUSABLE_FLAG) != 0 ||
            BCryptGetProperty(m_HashAlgHandle, BCRYPT_HASH_LENGTH, reinterpret_cast<PBYTE>(&m_HashLength), sizeof(m_HashLength), &ResultLength, 0) != 0)
        {
            return {};
        }
    }

    // Initialize per-thread hashing handle
    if (HashHandle == nullptr)
    {
        if (BCryptCreateHash(m_HashAlgHandle, &HashHandle, nullptr, 0, nullptr, 0, BCRYPT_HASH_REUSABLE_FLAG) != 0)
        {
            return {};
        }
    }

    // Open file for reading - avoid files that are actively being written to
    SmartPointer<HANDLE> hFile(CloseHandle, CreateFile(GetPathLong().c_str(),
        GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return {};
    }
    
    // Hash data one read at a time
    DWORD iReadResult = 0;
    DWORD iHashResult = 0;
    DWORD iReadBytes = 0;
    while ((iReadResult = ReadFile(hFile, FileBuffer.data(), static_cast<DWORD>(
        hashSizeLimit > 0 ? min(hashSizeLimit, FileBuffer.size()) : FileBuffer.size()),
        &iReadBytes, nullptr)) != 0 && iReadBytes > 0)
    {
        UpwardDrivePacman();
        iHashResult = BCryptHashData(HashHandle, FileBuffer.data(), iReadBytes, 0);
        if (iHashResult != 0 || hashSizeLimit > 0) break;
        queue->WaitIfSuspended();
    }

    // Complete hash data
    Hash.resize(m_HashLength);
    if (iHashResult != 0 || iReadResult == 0 ||
        BCryptFinishHash(HashHandle, Hash.data(), m_HashLength, 0) != 0)
    {
        return {};
    }

    // We reduce the size of the stored hash since the level of uniqueness required
    // is unnecessary for simple dupe checking. This is preferred to just using a simpler
    // hash alg since SHA512 is FIPS compliant on Windows and more performant than SHA256.
    constexpr auto ReducedHashInBytes = 16;
    Hash.resize(ReducedHashInBytes);
    Hash.shrink_to_fit();
    return Hash;
}

CItem* CItem::FindItemByPath(const std::wstring& path) const
{
    auto* pathDrive = GetParentDrive();
    if (pathDrive == nullptr) return nullptr;

    // Create a root enumerator to find the node
    CItem falseRoot(IT_MYCOMPUTER, L"");
    falseRoot.m_FolderInfo->m_Children.push_back(pathDrive);
    CItem* current = pathDrive->GetParent() ? pathDrive->GetParent() : &falseRoot;

    // Split the path into components
    std::vector<std::wstring> components;
    for (auto&& part : path | std::views::split(L'\\')) {
        components.emplace_back(part.begin(), part.end());
    }

    // Loop over each string component
    for (const auto& component : components)
    {
        // See if there is a match item in the structure
        auto it = std::ranges::find_if(current->GetChildren(), [&](const CItem* child)
        {
            // For drives, only compare the first two characters (e.g. "C:") otherwise full name
            return component == (child->IsType(IT_DRIVE) ? child->GetName().substr(0, 2) : child->GetName());
        });

        // If not found, return null
        if (it == current->GetChildren().end()) return nullptr;
        current = *it;
    }

    return current;
}

std::vector<CItem*> CItem::FindItemsBySameIndex() const
{
    std::vector<CItem*> results;

    // Get the index of the current item
    const DWORD targetIndex = GetIndex();
    
    // Only search if we have a valid non-zero index
    if (targetIndex == 0)
    {
        return results;
    }

    // Get the parent drive - we only search within the same drive
    auto* driveItem = GetParentDrive();
    if (driveItem == nullptr)
    {
        return results;
    }

    // Use a stack-based traversal to search through all items under the drive
    std::stack<CItem*> itemStack;
    itemStack.push(driveItem);

    while (!itemStack.empty())
    {
        CItem* current = itemStack.top();
        itemStack.pop();

        // Check if this item has the same index (but is not the current item itself)
        if (current != this && current->GetIndex() == targetIndex)
        {
            results.push_back(current);
        }

        // Add all children to the stack for traversal
        if (!current->IsLeaf())
        {
            for (auto* child : current->GetChildren())
            {
                itemStack.push(child);
            }
        }
    }

    return results;
}
