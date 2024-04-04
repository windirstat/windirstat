// Item.h - Declaration of CItem
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

#pragma once

#include "TreeListControl.h"
#include "TreeMap.h"
#include "DirStatDoc.h" // CExtensionData
#include "FileFind.h" // FileFindEnhanced
#include "BlockingQueue.h"

#include <mutex>

// Columns
enum ITEMCOLUMNS
{
    COL_NAME,
    COL_SUBTREEPERCENTAGE,
    COL_PERCENTAGE,
    COL_SUBTREETOTAL,
    COL_ITEMS,
    COL_FILES,
    COL_SUBDIRS,
    COL_LASTCHANGE,
    COL_ATTRIBUTES,
    COL_OWNER
};

// Item types
enum ITEMTYPE : unsigned short
{
    IT_MYCOMPUTER = 1 << 0, // Pseudo Container "My Computer"
    IT_DRIVE      = 1 << 1, // C:\, D:\ etc.
    IT_DIRECTORY  = 1 << 2, // Folder
    IT_FILE       = 1 << 3, // Regular file
    IT_FREESPACE  = 1 << 4, // Pseudo File "<Free Space>"
    IT_UNKNOWN    = 1 << 5, // Pseudo File "<Unknown>"
    IT_ANY        = 0x00FF, // Indicates any item type
    ITF_DONE      = 1 << 8, // Indicates done processing
    ITF_ROOTITEM  = 1 << 9, // Indicates root item
    ITF_FLAGS     = 0xFF00, // All potential flag items
};

inline ITEMTYPE operator|(const ITEMTYPE & a, const ITEMTYPE & b)
{
    return static_cast<ITEMTYPE>(static_cast<unsigned short>(a) | static_cast<unsigned short>(b));
}

inline ITEMTYPE operator-(const ITEMTYPE& a, const ITEMTYPE& b)
{
    return static_cast<ITEMTYPE>(static_cast<unsigned short>(a) & ~static_cast<unsigned short>(b));
}

// Compare FILETIMEs
inline bool operator<(const FILETIME& t1, const FILETIME& t2)
{
    return t1.dwHighDateTime < t2.dwHighDateTime
    || t1.dwHighDateTime == t2.dwHighDateTime && t1.dwLowDateTime < t2.dwLowDateTime;
}

// Compare FILETIMEs
inline bool operator==(const FILETIME& t1, const FILETIME& t2)
{
    return t1.dwLowDateTime == t2.dwLowDateTime && t1.dwHighDateTime == t2.dwHighDateTime;
}

//
// CItem. This is the object, from which the whole tree is built.
// For every directory, file etc., we find on the Harddisks, there is one CItem.
// It is derived from CTreeListItem because it _may_ become "visible" and therefore
// may be inserted in the TreeList view (we don't clone any data).
//
// Of course, this class and the base classes are optimized rather for size than for speed.
//
// The m_type indicates whether we are a file or a folder or a drive etc.
// It may have been better to design a class hierarchy for this, but I can't help it,
// rather than browsing to virtual functions I like to flatly see what's going on.
// But, of course, now we have quite many switch statements in the member functions.
//
// Naming convention:
// Methods which recurse down to every child (expensive) are named "RecurseDoSomething".
// Methods which recurse up to the parent (not so expensive) are named "UpwardDoSomething".
//
class CItem final : public CTreeListItem, public CTreemap::Item
{
public:
    CItem(const CItem&) = delete;
    CItem(CItem&&) = delete;
    CItem& operator=(const CItem&) = delete;
    CItem& operator=(CItem&&) = delete;
    CItem(ITEMTYPE type, LPCWSTR name);
    CItem(ITEMTYPE type, LPCWSTR name, FILETIME lastChange, ULONGLONG size, DWORD attributes, ULONG files, ULONG subdirs);
    ~CItem() override;

    // CTreeListItem Interface
    bool DrawSubitem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) const override;
    CStringW GetText(int subitem) const override;
    COLORREF GetItemTextColor() const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    int GetTreeListChildCount() const override;
    CTreeListItem* GetTreeListChild(int i) const override;
    short GetImageToCache() const override;
    void DrawAdditionalState(CDC* pdc, const CRect& rcLabel) const override;

    // CTreemap::Item interface
    bool TmiIsLeaf() const override
    {
        return IsType(IT_FILE | IT_FREESPACE | IT_UNKNOWN);
    }

    CRect TmiGetRectangle() const override;
    void TmiSetRectangle(const CRect& rc) override;

    COLORREF TmiGetGraphColor() const override
    {
        return GetGraphColor();
    }

    int TmiGetChildCount() const override
    {
        if (!m_ci) return 0;
        return static_cast<int>(m_ci->m_children.size());
    }

    CTreemap::Item* TmiGetChild(int c) const override
    {
        return m_ci->m_children[c];
    }

    ULONGLONG TmiGetSize() const override
    {
        return GetSize();
    }

    // CItem
    static int GetSubtreePercentageWidth();
    static CItem* FindCommonAncestor(const CItem* item1, const CItem* item2);

    ULONGLONG GetProgressRange() const;
    ULONGLONG GetProgressPos() const;
    void UpdateStatsFromDisk();
    const std::vector<CItem*>& GetChildren() const;
    CItem* GetParent() const;
    void AddChild(CItem* child, bool add_only = false);
    void RemoveChild(CItem* child);
    void RemoveAllChildren();
    void UpwardAddSubdirs(ULONG dirCount);
    void UpwardSubtractSubdirs(ULONG dirCount);
    void UpwardAddFiles(ULONG fileCount);
    void UpwardSubtractFiles(ULONG fileCount);
    void UpwardAddSize(ULONGLONG bytes);
    void UpwardSubtractSize(ULONGLONG bytes);
    void UpwardAddReadJobs(ULONG count);
    void UpwardSubtractReadJobs(ULONG count);
    void UpwardUpdateLastChange(const FILETIME& t);
    void UpwardRecalcLastChange(bool without_item = false);
    ULONGLONG GetSize() const;
    void SetSize(ULONGLONG ownSize);
    ULONG GetReadJobs() const;
    FILETIME GetLastChange() const;
    void SetLastChange(const FILETIME& t);
    void SetAttributes(DWORD attr);
    DWORD GetAttributes() const;
    unsigned short GetSortAttributes() const;
    double GetFraction() const;
    bool IsRootItem() const;
    CStringW GetPath() const;
    CStringW GetOwner(bool force = false) const;
    bool HasUncPath() const;
    CStringW GetFindPattern() const;
    CStringW GetFolderPath() const;
    CStringW GetReportPath() const;
    CStringW GetName() const;
    CStringW GetExtension() const;
    ULONG GetFilesCount() const;
    ULONG GetSubdirsCount() const;
    ULONGLONG GetItemsCount() const;
    void SetDone();
    void SortItemsBySize() const;
    ULONGLONG GetTicksWorked() const;
    static void ScanItems(BlockingQueue<CItem*> *);
    static void ScanItemsFinalize(CItem* item);
    void UpwardSetDone();
    void UpwardSetUndone();
    CItem* FindRecyclerItem() const;
    void CreateFreeSpaceItem();
    CItem* FindFreeSpaceItem() const;
    void UpdateFreeSpaceItem() const;
    void RemoveFreeSpaceItem();
    void CreateUnknownItem();
    CItem* FindUnknownItem() const;
    void UpdateUnknownItem();
    void RemoveUnknownItem();
    void RecurseCollectExtensionData(CExtensionData* ed) const;

    bool IsDone() const
    {
        return IsType(ITF_DONE);
    }

    ITEMTYPE GetType() const
    {
        return static_cast<ITEMTYPE>(m_type & ~ITF_FLAGS);
    }

    ITEMTYPE GetRawType() const
    {
        return m_type;
    }

    constexpr bool IsType(const ITEMTYPE type) const
    {
        return (m_type & type) != 0;
    }

    constexpr void SetType(const ITEMTYPE type, bool set = true)
    {
        if (set) m_type = m_type | type;
        else m_type = m_type - type;
    }

private:
    ULONGLONG GetProgressRangeMyComputer() const;
    ULONGLONG GetProgressRangeDrive() const;
    COLORREF GetGraphColor() const;
    bool MustShowReadJobs() const;
    COLORREF GetPercentageColor() const;
    CStringW UpwardGetPathWithoutBackslash() const;
    CItem* AddDirectory(const FileFindEnhanced& finder);
    void AddFile(const FileFindEnhanced& finder);
    void UpwardDrivePacman();

    // Special structure for container items that is separately allocated to
    // reduce memory usage.  This operates under the assumption that most
    // containers have files in them.
    using CHILDINFO = struct CHILDINFO
    {
        std::vector<CItem*> m_children;
        std::shared_mutex m_protect;
        std::atomic<ULONG> m_tstart = 0;  // initial time this node started enumerating
        std::atomic<ULONG> m_tfinish = 0; // initial time this node started enumerating
        std::atomic<ULONG> m_files = 0;   // # Files in subtree
        std::atomic<ULONG> m_subdirs = 0; // # Folder in subtree
        std::atomic<ULONG> m_jobs = 0;    // # "read jobs" in subtree.
    };

    RECT m_rect;                       // To support GraphView
    CStringW m_name;                   // Display name
    LPCWSTR m_extension;               // Cache of extension (it's used often)
    FILETIME m_lastChange = {0, 0};    // Last modification time OF SUBTREE
    CHILDINFO* m_ci = nullptr;         // Child information for non-files
    std::atomic<ULONGLONG> m_size = 0; // OwnSize, if IT_FILE or IT_FREESPACE, or IT_UNKNOWN; SubtreeTotal else.
    DWORD m_attributes = 0;            // Packed file attributes of the item
    ITEMTYPE m_type;                   // Indicates our type.
};
