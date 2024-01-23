// Item.cpp - Implementation of CItem
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
#include "WinDirStat.h"
#include "DirStatDoc.h" // GetItemColor()
#include "MainFrame.h"
#include <common/CommonHelpers.h>
#include "GlobalHelpers.h"
#include "SelectObject.h"
#include "Item.h"
#include "BlockingQueue.h"

#include <string>
#include <algorithm>
#include <unordered_set>
#include <concurrent_queue.h>
#include <functional>
#include <queue>
#include <shared_mutex>
#include <stack>

namespace
{
    CStringW GetFreeSpaceItemName()
    {
        return LoadString(IDS_FREESPACE_ITEM);
    }

    CStringW GetUnknownItemName()
    {
        return LoadString(IDS_UNKNOWN_ITEM);
    }

    constexpr SIZE sizeDeflatePacman = {1, 2};
}

CItem::CItem(ITEMTYPE type, LPCWSTR name)
    : m_name(name)
      , m_lastChange{0,0}
      , m_size(0)
      , m_files(0)
      , m_subdirs(0)
      , m_ticks(0)
      , m_jobs(0)
      , m_attributes(0)
      , m_type(type)
{
    if (IsType(IT_DRIVE))
    {
        m_name = FormatVolumeNameOfRootPath(m_name);
    }

    if (IsType(IT_FILE))
    {
        const LPCWSTR ext = wcsrchr(name, L'.');
        if (ext == nullptr)
        {
            static LPCWSTR noext = L".";
            m_extension = noext;
        }
        else
        {
            std::wstring exttoadd(&ext[0]);
            _wcslwr_s(exttoadd.data(), exttoadd.size() + 1);

            static std::shared_mutex extlock;
            static std::unordered_set<std::wstring> extcache;
            std::lock_guard lock(extlock);
            const auto cached = extcache.insert(std::move(exttoadd));
            m_extension = cached.first->c_str();
        }
    }
    else
    {
        m_extension = m_name.GetString();
    }
}

CItem::~CItem()
{
    std::lock_guard m_guard(m_protect);
    for (const auto& m_child : m_children)
    {
        delete m_child;
    }
}

CRect CItem::TmiGetRectangle() const
{
    return m_rect;
}

void CItem::TmiSetRectangle(const CRect& rc)
{
    m_rect = rc;
}

bool CItem::DrawSubitem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) const
{
    if (subitem == COL_NAME)
    {
        return CTreeListItem::DrawSubitem(subitem, pdc, rc, state, width, focusLeft);
    }
    if (subitem != COL_SUBTREEPERCENTAGE)
    {
        return false;
    }

    const bool showReadJobs = MustShowReadJobs();

    if (showReadJobs && !GetOptions()->IsPacmanAnimation())
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

    DrawSelection(GetTreeListControl(), pdc, rc, state);

    if (showReadJobs)
    {
        rc.DeflateRect(sizeDeflatePacman);
        DrawPacman(pdc, rc, GetTreeListControl()->GetItemSelectionBackgroundColor(this));
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

CStringW CItem::GetText(int subitem) const
{
    CStringW s;
    switch (subitem)
    {
    case COL_NAME:
        {
            s = m_name;
        }
        break;

    case COL_SUBTREEPERCENTAGE:
        if (IsDone())
        {
            ASSERT(m_jobs == 0);
            //s = "ok";
        }
        else
        {
            if (m_jobs == 1)
                VERIFY(s.LoadString(IDS_ONEREADJOB));
            else
                s.FormatMessage(IDS_sREADJOBS, FormatCount(m_jobs).GetString());
        }
        break;

    case COL_PERCENTAGE:
        if (GetOptions()->IsShowTimeSpent() && MustShowReadJobs() || IsRootItem())
        {
            s.Format(L"[%s s]", FormatMilliseconds(GetTicksWorked()).GetString());
        }
        else
        {
            s.Format(L"%s%%", FormatDouble(GetFraction() * 100).GetString());
        }
        break;

    case COL_SUBTREETOTAL:
        {
            s = FormatBytes(GetSize());
        }
        break;

    case COL_ITEMS:
        if (!IsType(IT_FILE | IT_FREESPACE | IT_UNKNOWN))
        {
            s = FormatCount(GetItemsCount());
        }
        break;

    case COL_FILES:
        if (!IsType(IT_FILE | IT_FREESPACE | IT_UNKNOWN))
        {
            s = FormatCount(GetFilesCount());
        }
        break;

    case COL_SUBDIRS:
        if (!IsType(IT_FILE | IT_FREESPACE | IT_UNKNOWN))
        {
            s = FormatCount(GetSubdirsCount());
        }
        break;

    case COL_LASTCHANGE:
        if (!IsType(IT_FREESPACE | IT_UNKNOWN))
        {
            s = FormatFileTime(m_lastChange);
        }
        break;

    case COL_ATTRIBUTES:
        if (!IsType(IT_FREESPACE | IT_UNKNOWN | IT_MYCOMPUTER))
        {
            s = FormatAttributes(GetAttributes());
        }
        break;

    default:
        {
            ASSERT(0);
        }
        break;
    }
    return s;
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
        return GetWDSApp()->AltColor();
    }

    if (attr & FILE_ATTRIBUTE_ENCRYPTED)
    {
        return GetWDSApp()->AltEncryptionColor();
    }

    // The rest is not colored
    return CTreeListItem::GetItemTextColor();
}

int CItem::CompareSibling(const CTreeListItem* tlib, int subitem) const
{
    const CItem* other = reinterpret_cast<const CItem*>(tlib);

    int r = 0;
    switch (subitem)
    {
    case COL_NAME:
        if (IsType(IT_DRIVE))
        {
            ASSERT(other->IsType(IT_DRIVE));
            r = signum(GetPath().CompareNoCase(other->GetPath()));
        }
        else
        {
            r = signum(m_name.CompareNoCase(other->m_name));
        }
        break;

    case COL_SUBTREEPERCENTAGE:
        if (MustShowReadJobs())
        {
            r = usignum(static_cast<ULONG>(m_jobs), static_cast<ULONG>(other->m_jobs));
        }
        else
        {
            r = signum(GetFraction() - other->GetFraction());
        }
        break;

    case COL_PERCENTAGE:
        {
            r = signum(GetFraction() - other->GetFraction());
        }
        break;

    case COL_SUBTREETOTAL:
        {
            r = usignum(GetSize(), other->GetSize());
        }
        break;

    case COL_ITEMS:
        {
            r = usignum(GetItemsCount(), other->GetItemsCount());
        }
        break;

    case COL_FILES:
        {
            r = usignum(GetFilesCount(), other->GetFilesCount());
        }
        break;

    case COL_SUBDIRS:
        {
            r = usignum(GetSubdirsCount(), other->GetSubdirsCount());
        }
        break;

    case COL_LASTCHANGE:
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
        break;
    case COL_ATTRIBUTES:
        {
            r = signum(GetSortAttributes() - other->GetSortAttributes());
        }
        break;

    default:
        {
            ASSERT(false);
        }
        break;
    }
    return r;
}

int CItem::GetChildrenCount() const
{
    return static_cast<int>(m_children.size());
}

CTreeListItem* CItem::GetTreeListChild(int i) const
{
    return m_children[i];
}

short CItem::GetImageToCache() const
{
    // (Caching is done in CTreeListItem)

    if (IsType(IT_MYCOMPUTER))
    {
        return GetMyImageList()->getMyComputerImage();
    }
    if (IsType(IT_FREESPACE))
    {
        return GetMyImageList()->getFreeSpaceImage();
    }
    if (IsType(IT_UNKNOWN))
    {
        return GetMyImageList()->getUnknownImage();
    }

    const CStringW path = GetPath();
    if (IsType(IT_DIRECTORY) && GetWDSApp()->IsVolumeMountPoint(path))
    {
        return GetMyImageList()->getMountPointImage();
    }
    if (IsType(IT_DIRECTORY) && GetWDSApp()->IsFolderJunction(GetAttributes()))
    {
        constexpr DWORD mask = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
        const bool os_file = (GetAttributes() & mask) == mask;
        return os_file ? GetMyImageList()->getJunctionProtectedImage() : GetMyImageList()->getJunctionImage();
    }
    if (IsType(IT_DIRECTORY) && GetWDSApp()->IsFolderJunction(GetAttributes()))
    {
        return GetMyImageList()->getJunctionImage();
    }

    return GetMyImageList()->getFileImage(path);
}

void CItem::DrawAdditionalState(CDC* pdc, const CRect& rcLabel) const
{
    if (!IsRootItem() && this == GetDocument()->GetZoomItem())
    {
        CRect rc = rcLabel;
        rc.InflateRect(1, 0);
        rc.bottom++;

        CSelectStockObject sobrush(pdc, NULL_BRUSH);
        CPen pen(PS_SOLID, 2, GetDocument()->GetZoomColor());
        CSelectObject sopen(pdc, &pen);

        pdc->Rectangle(rc);
    }
}

int CItem::GetSubtreePercentageWidth()
{
    return 105;
}

CItem* CItem::FindCommonAncestor(const CItem* item1, const CItem* item2)
{
    for (auto parent = item1; parent != nullptr; parent = parent->GetParent())
    {
        if (parent->IsAncestorOf(item2)) return const_cast<CItem*>(parent);
    }

    ASSERT(FALSE);
    return nullptr;
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
    if (IsType(IT_FILE | IT_DIRECTORY))
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
        for (const auto& child : m_children)
        {
            pos += child->GetProgressPos();
        }
        return pos;
    }
    if (IsType(IT_DRIVE))
    {
        ULONGLONG pos = GetSize();
        const CItem* fs = FindFreeSpaceItem();
        pos -= (fs != nullptr) ? fs->GetSize() : 0;
        return pos;
    }
    if (IsType(IT_DIRECTORY))
    {
        return GetItemsCount();
    }

    ASSERT(FALSE);
    return 0;
}

void CItem::UpdateStatsFromDisk()
{
    if (IsType(IT_DIRECTORY | IT_FILE))
    {
        FileFindEnhanced finder;
        if (finder.FindFile(GetFolderPath(),GetName()))
        {
            SetLastChange(finder.GetLastWriteTime());
            SetAttributes(finder.GetAttributes());

            if (IsType(IT_FILE))
            {
                UpwardSubtractSize(m_size);
                UpwardAddSize(finder.GetCompressedLength());
            }
        }
    }
}

CItem* CItem::GetChild(int i) const
{
    return m_children[i];
}

CItem* CItem::GetParent() const
{
    return reinterpret_cast<CItem*>(CTreeListItem::GetParent());
}

void CItem::AddChild(CItem* child)
{
    ASSERT(!IsDone()); // SetDone() computed m_childrenBySize

    UpwardAddSize(child->m_size);
    UpwardUpdateLastChange(child->m_lastChange);
    child->SetParent(this);

    std::lock_guard m_guard(m_protect);
    m_children.push_back(child);

    if (IsVisible())
    {
        GetMainFrame()->InvokeInMessageThread([this, child]
        {
            GetTreeListControl()->OnChildAdded(this, child);
        });
    }
}

void CItem::RemoveChild(CItem* child)
{
    std::lock_guard m_guard(m_protect);
    m_children.erase(std::ranges::find(m_children, child));

    if (IsVisible())
    {
        GetMainFrame()->InvokeInMessageThread([this, child]
        {
            GetTreeListControl()->OnChildRemoved(this, child);
        });
    }

    delete child;
}

void CItem::RemoveAllChildren()
{
    GetMainFrame()->InvokeInMessageThread([this]
    {
        GetTreeListControl()->OnRemovingAllChildren(this);
    });

    std::lock_guard m_guard(m_protect);
    for (const auto& child : m_children)
    {
        delete child;
    }
    m_children.clear();
}

void CItem::UpwardAddSubdirs(const ULONG dirCount)
{
    if (dirCount == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsType(IT_FILE)) continue;
        p->m_subdirs += dirCount;
    }
}

void CItem::UpwardSubtractSubdirs(const ULONG dirCount)
{
    if (dirCount == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        ASSERT(p->m_subdirs - dirCount >= 0);
        if (p->IsType(IT_FILE)) continue;
        p->m_subdirs -= dirCount;
    }
}

void CItem::UpwardAddFiles(const ULONG fileCount)
{
    if (fileCount == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsType(IT_FILE)) continue;
        p->m_files += fileCount;
    }
}

void CItem::UpwardSubtractFiles(const ULONG fileCount)
{
    if (fileCount == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        ASSERT(p->m_files - fileCount >= 0);
        if (p->IsType(IT_FILE)) continue;
        p->m_files -= fileCount;
    }
}

void CItem::UpwardAddSize(const ULONGLONG bytes)
{
    if (bytes == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        p->m_size += bytes;
    }
}

void CItem::UpwardSubtractSize(const ULONGLONG bytes)
{
    if (bytes == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        ASSERT(p->m_size - bytes >= 0);
        p->m_size -= bytes;
    }
}

void CItem::UpwardAddReadJobs(const ULONG count)
{
    if (count == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsType(IT_FILE)) continue;
        p->m_jobs += count;
    }
}

void CItem::UpwardSubtractReadJobs(const ULONG count)
{
    if (count == 0) return;
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        ASSERT(p->m_jobs - count >= 0);
        if (p->IsType(IT_FILE)) continue;
        if (p->IsType(IT_DIRECTORY) && p->m_jobs == 0) SetDone();
        p->m_jobs -= count;
    }
}

// This method increases the last change
void CItem::UpwardUpdateLastChange(const FILETIME& t)
{
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (CompareFileTime(&t, &p->m_lastChange) == 1) p->m_lastChange = t;
    }
}

void CItem::UpwardRecalcLastChange(bool without_item)
{
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        p->UpdateStatsFromDisk();

        for (const auto& child : p->m_children)
        {
            if (without_item && child == this) continue;
            if (CompareFileTime(&child->m_lastChange, &p->m_lastChange) == 1)
                p->m_lastChange = child->m_lastChange;
        }
    }
}

void CItem::UpwardAddTicksWorked(const ULONG delta)
{
    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        p->m_ticks += delta;
    }
}

ULONGLONG CItem::GetSize() const
{
    return m_size;
}

void CItem::SetSize(const ULONGLONG ownSize)
{
    ASSERT(TmiIsLeaf());
    ASSERT(ownSize >= 0);
    m_size = ownSize;
}

ULONG CItem::GetReadJobs() const
{
    return m_jobs;
}

FILETIME CItem::GetLastChange() const
{
    return m_lastChange;
}

void CItem::SetLastChange(const FILETIME& t)
{
    m_lastChange = t;
}

void CItem::SetAttributes(const DWORD attr)
{
    m_attributes = attr;
}

// Decode the attributes encoded by SetAttributes()
DWORD CItem::GetAttributes() const
{
    return m_attributes;
}

// Returns a value which resembles sorting of RHSACE considering gaps
int CItem::GetSortAttributes() const
{
    DWORD ret = 0;

    // We want to enforce the order RHSACE with R being the highest priority
    // attribute and E being the lowest priority attribute.
    ret |= m_attributes & FILE_ATTRIBUTE_READONLY   ? 1 << 5 : 0; // R
    ret |= m_attributes & FILE_ATTRIBUTE_HIDDEN     ? 1 << 4 : 0; // H
    ret |= m_attributes & FILE_ATTRIBUTE_SYSTEM     ? 1 << 3 : 0; // S
    ret |= m_attributes & FILE_ATTRIBUTE_ARCHIVE    ? 1 << 2 : 0; // A
    ret |= m_attributes & FILE_ATTRIBUTE_COMPRESSED ? 1 << 1 : 0; // C
    ret |= m_attributes & FILE_ATTRIBUTE_ENCRYPTED  ? 1 << 0 : 0; // E

    return ret;
}

double CItem::GetFraction() const
{
    if (!GetParent() || GetParent()->GetSize() == 0)
    {
        return 1.0;
    }
    return static_cast<double>(GetSize()) / GetParent()->GetSize();
}

bool CItem::IsRootItem() const
{
    return (m_type & ITF_ROOTITEM) != 0;
}

CStringW CItem::GetPath() const
{
    CStringW path = UpwardGetPathWithoutBackslash();
    if (IsType(IT_DRIVE))
    {
        path += L"\\";
    }
    return path;
}

bool CItem::HasUncPath() const
{
    const CStringW path = GetPath();
    return path.GetLength() >= 2 && path.Left(2) == L"\\\\";
}

CStringW CItem::GetFindPattern() const
{
    CStringW pattern = GetPath();
    if (pattern.Right(1) != wds::chrBackslash)
    {
        pattern += L"\\";
    }
    pattern += L"*.*";
    return pattern;
}

// Returns the path for "Explorer here" or "Command Prompt here"
CStringW CItem::GetFolderPath() const
{
    CStringW path;

    if (IsType(IT_MYCOMPUTER))
    {
        path = GetParseNameOfMyComputer();
    }
    else
    {
        path = GetPath();
        if (IsType(IT_FILE))
        {
            const int i = path.ReverseFind(wds::chrBackslash);
            ASSERT(i != -1);
            path = path.Left(i + 1);
        }
    }
    return path;
}

// returns the path for the mail-report
CStringW CItem::GetReportPath() const
{
    CStringW path = UpwardGetPathWithoutBackslash();
    if (IsType(IT_DRIVE))
    {
        path += L"\\";
    }
    if (IsType(IT_FREESPACE | IT_UNKNOWN))
    {
        path += GetName();
    }

    return path;
}

CStringW CItem::GetName() const
{
    return m_name;
}

CStringW CItem::GetExtension() const
{
    return m_extension;
}

ULONG CItem::GetFilesCount() const
{
    return m_files;
}

ULONG CItem::GetSubdirsCount() const
{
    return m_subdirs;
}

ULONGLONG CItem::GetItemsCount() const
{
    return static_cast<ULONGLONG>(m_files) + static_cast<ULONGLONG>(m_subdirs);
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
    if (IsType(IT_MYCOMPUTER | IT_DRIVE | IT_DIRECTORY))
    {
        // sort by size for proper treemap rendering
        std::lock_guard m_guard(m_protect);
        m_children.shrink_to_fit();
        std::ranges::sort(m_children, [](auto item1, auto item2)
        {
            return item1->GetSize() > item2->GetSize(); // biggest first
        });
    }

    ZeroMemory(&m_rect, sizeof(m_rect));
    SetType(ITF_DONE, true);
}

ULONGLONG CItem::GetTicksWorked() const
{
    return m_ticks;
}

void CItem::ScanItemsFinalize(CItem* item)
{
    std::stack<CItem*> queue;
    queue.push(item);
    while (!queue.empty())
    {
        const auto & qitem = queue.top();
        queue.pop();
        qitem->SetDone();
        for (const auto& child : qitem->m_children)
        {
            queue.push(child);
        }
    }
}

void CItem::ScanItems(BlockingQueue<CItem*> * queue)
{
    const bool skip_hidden = GetOptions()->IsSkipHidden();

    while (CItem * item = queue->pop())
    {
        // Used to trigger thread exit condition
        if (item == nullptr) return;

        const ULONGLONG start = GetTickCount64();

        if (item->IsType(IT_DRIVE | IT_DIRECTORY))
        {
            FileFindEnhanced finder;
            for (BOOL b = finder.FindFile(item->GetPath()); b; b = finder.FindNextFile())
            {
                if (finder.IsDots())
                {
                    continue;
                }
                if (skip_hidden && finder.IsHidden())
                {
                    continue;
                }

                if (finder.IsDirectory())
                {
                    item->UpwardAddSubdirs(1);
                    CItem* newitem = item->AddDirectory(finder);
                    if (newitem->GetReadJobs() > 0)
                    {
                        queue->push(newitem, false);
                    }
                }
                else
                {
                    item->UpwardAddFiles(1);
                    item->AddFile(finder);
                }

                // Update pacman position
                item->UpwardDrivePacman();
            }
        }
        else if (item->IsType(IT_FILE))
        {
            // Only used for refreshes
            item->UpdateStatsFromDisk();
            item->SetDone();
        }

        item->UpwardSubtractReadJobs(1);
        item->UpwardAddTicksWorked(static_cast<ULONG>(GetTickCount64() - start));
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
                p->UpwardSubtractSize(unknown->GetSize());
                unknown->SetSize(0);
            }
        }

        p->SetType(ITF_DONE, false);
    }
}

void CItem::RefreshRecycler() const
{
    ASSERT(IsType(IT_DRIVE));
    DWORD dummy;
    CStringW system;
    const BOOL b = GetVolumeInformation(GetPath(), nullptr, 0, nullptr, &dummy, &dummy, system.GetBuffer(128), 128);
    system.ReleaseBuffer();
    if (!b)
    {
        VTRACE(L"GetVolumeInformation(%s) failed.", GetPath().GetString());
        return;
    }

    CStringW recycler;
    if (system.CompareNoCase(L"NTFS") == 0)
    {
        recycler = L"recycler";
    }
    else if (system.CompareNoCase(L"FAT32") == 0)
    {
        recycler = L"recycled";
    }
    else
    {
        VTRACE(L"%s: unknown file system type %s", GetPath().GetString(), system.GetString());
        return;
    }

    CItem * found = nullptr;
    for (const auto& child : m_children)
    {
        if (child->m_name.CompareNoCase(recycler) == 0)
        {
            found = child;
            break;
        }
    }
    if (!found)
    {
        VTRACE(L"%s: Recycler(%s) not found.", GetPath().GetString(), recycler.GetString());
        return;
    }

    // TODO: Why refresh?
}

void CItem::CreateFreeSpaceItem()
{
    ASSERT(IsType(IT_DRIVE));

    UpwardSetUndone();

    ULONGLONG total;
    ULONGLONG free;
    CDirStatApp::getDiskFreeSpace(GetPath(), total, free);

    const auto freespace = new CItem(IT_FREESPACE, GetFreeSpaceItemName());
    freespace->SetSize(free);
    freespace->SetDone();

    AddChild(freespace);
}

CItem* CItem::FindFreeSpaceItem() const
{
    for (const auto& child : m_children)
    {
        if (child->IsType(IT_FREESPACE))
        {
            return child;
        }
    }
    return nullptr;
}

void CItem::UpdateFreeSpaceItem() const
{
    ASSERT(IsType(IT_DRIVE));

    CItem* freeSpaceItem = FindFreeSpaceItem();
    if (freeSpaceItem == nullptr)
    {
        return;
    }

    ULONGLONG total;
    ULONGLONG free;
    CDirStatApp::getDiskFreeSpace(GetPath(), total, free);

    const ULONGLONG before = freeSpaceItem->GetSize();
    const ULONGLONG diff   = free - before;

    freeSpaceItem->UpwardAddSize(diff);

    ASSERT(freeSpaceItem->GetSize() == free);
}

void CItem::UpdateUnknownItem()
{
    ASSERT(IsType(IT_DRIVE));

    CItem* unknown = FindUnknownItem();
    if (unknown == nullptr)
    {
        return;
    }

    ULONGLONG total;
    ULONGLONG free;
    CDirStatApp::getDiskFreeSpace(GetPath(), total, free);

    ULONGLONG unknownspace = total - GetSize();
    if (!GetDocument()->OptionShowFreeSpace())
    {
        unknownspace -= free;
    }
    unknown->SetSize(unknownspace);

    UpwardAddSize(unknownspace);
}

void CItem::RemoveFreeSpaceItem()
{
    ASSERT(IsType(IT_DRIVE));

    if (const auto freespace = FindFreeSpaceItem(); freespace != nullptr)
    {
        UpwardSetUndone();
        UpwardSubtractSize(freespace->GetSize());
        RemoveChild(freespace);
    }
}

void CItem::CreateUnknownItem()
{
    ASSERT(IsType(IT_DRIVE));

    UpwardSetUndone();

    const auto unknown = new CItem(IT_UNKNOWN, GetUnknownItemName());
    unknown->SetDone();

    AddChild(unknown);
}

CItem* CItem::FindUnknownItem() const
{
    for (const auto& child : m_children)
    {
        if (child->IsType(IT_UNKNOWN))
        {
            return child;
        }
    }
    return nullptr;
}

void CItem::RemoveUnknownItem()
{
    ASSERT(IsType(IT_DRIVE));

    if (const auto unknown = FindUnknownItem(); unknown != nullptr)
    {
        UpwardSetUndone();
        UpwardSubtractSize(unknown->GetSize());
        RemoveChild(unknown);
    }
} 

void CItem::RecurseCollectExtensionData(CExtensionData* ed) const
{
    if (TmiIsLeaf())
    {
        if (IsType(IT_FILE))
        {
            const CStringW & ext = GetExtension();
            SExtensionRecord r;
            if (ed->Lookup(ext, r))
            {
                r.bytes += GetSize();
                r.files++;
            }
            else
            {
                r.bytes = GetSize();
                r.files = 1;
            }
            ed->SetAt(ext, r);
        }
    }
    else
    {
        for (const auto& child : m_children)
        {
            child->RecurseCollectExtensionData(ed);
        }
    }
}

ULONGLONG CItem::GetProgressRangeMyComputer() const
{
    ASSERT(IsType(IT_MYCOMPUTER));

    ULONGLONG range = 0;
    for (const auto& child : m_children)
    {
        range += child->GetProgressRangeDrive();
    }
    return range;
}

ULONGLONG CItem::GetProgressRangeDrive() const
{
    ULONGLONG total;
    ULONGLONG free;
    CDirStatApp::getDiskFreeSpace(GetPath(), total, free);

    total -= free;

    ASSERT(total >= 0);
    return total;
}

COLORREF CItem::GetGraphColor() const
{
    if (IsType(IT_UNKNOWN))
    {
        return RGB(255, 255, 0) | CTreemap::COLORFLAG_LIGHTER;
    }

    if (IsType(IT_FREESPACE))
    {
        return RGB(100, 100, 100) | CTreemap::COLORFLAG_DARKER;
    }

    if (IsType(IT_FILE))
    {
        return GetDocument()->GetCushionColor(GetExtension());
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
    const int i = GetIndent() % GetOptions()->GetTreelistColorCount();
    return GetOptions()->GetTreelistColor(i);
}

CStringW CItem::UpwardGetPathWithoutBackslash() const
{
    // allow persistent storage to prevent constant reallocation
    thread_local CStringW path;
    path = L"\\";

    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsType(IT_DIRECTORY))
        {
            path = p->m_name + L"\\" + path;
        }
        else if (p->IsType(IT_FILE))
        {
            path = p->m_name;
        }
        else if (p->IsType(IT_DRIVE))
        {
            path = PathFromVolumeName(p->m_name) + L"\\" + path;
        }
    }

    return path.TrimRight(L'\\');
}

CItem* CItem::AddDirectory(const FileFindEnhanced& finder)
{
    const bool dontFollow = finder.IsProtectedReparsePoint() ||
        GetWDSApp()->IsVolumeMountPoint(finder.GetFilePath()) && !GetOptions()->IsFollowMountPoints() ||
        GetWDSApp()->IsFolderJunction(finder.GetAttributes()) && !GetOptions()->IsFollowJunctionPoints();

    const auto & child = new CItem(IT_DIRECTORY, finder.GetFileName());
    child->SetLastChange(finder.GetLastWriteTime());
    child->SetAttributes(finder.GetAttributes());
    AddChild(child);
    child->UpwardAddReadJobs(dontFollow ? 0 : 1);
    return child;
}

void CItem::AddFile(const FileFindEnhanced& finder)
{
    const auto & child = new CItem(IT_FILE, finder.GetFileName());
    child->SetSize(finder.GetCompressedLength());
    child->SetLastChange(finder.GetLastWriteTime());
    child->SetAttributes(finder.GetAttributes());
    AddChild(child);
    child->SetDone();
}

void CItem::UpwardDrivePacman()
{
    if (!GetOptions()->IsPacmanAnimation())
    {
        return;
    }

    for (auto p = this; p != nullptr; p = p->GetParent())
    {
        if (p->IsType(IT_FILE) || !p->IsVisible()) continue;
        if (p->m_jobs == 0) p->StopPacman();
        else p->DrivePacman();
    }
}

