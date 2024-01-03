// item.h - Declaration of CItem
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

class CWorkLimiter;

// Columns
enum
{
    COL_NAME,
    COL_SUBTREEPERCENTAGE,
    COL_PERCENTAGE,
    COL_SUBTREETOTAL,
    COL_ITEMS,
    COL_FILES,
    COL_SUBDIRS,
    COL_LASTCHANGE,
    COL_ATTRIBUTES
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
    ITF_ROOTITEM  = 1 << 8, // Indicates root item
    ITF_FLAGS     = 0xFF00, // All potential flag items
};

inline ITEMTYPE operator|(const ITEMTYPE & a, const ITEMTYPE & b)
{
    return static_cast<ITEMTYPE>(static_cast<unsigned short>(a) | static_cast<unsigned short>(b));
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
    // We collect data of files in FILEINFOs before we create items for them,
    // because we need to know their count before we can decide whether or not
    // we have to create a <Files> item. (A <Files> item is only created, when
    // (a) there are more than one files and (b) there are subdirectories.)
    struct FILEINFO
    {
        CStringW name;
        ULONGLONG length;
        FILETIME lastWriteTime;
        DWORD attributes;
    };

public:
    CItem(ITEMTYPE type, LPCWSTR name, bool dontFollow = false);
    ~CItem() override;

    // CTreeListItem Interface
    bool DrawSubitem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) const override;
    CStringW GetText(int subitem) const override;
    COLORREF GetItemTextColor() const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    int GetChildrenCount() const override;
    CTreeListItem* GetTreeListChild(int i) const override;
    int GetImageToCache() const override;
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

    int TmiGetChildrenCount() const override
    {
        return GetChildrenCount();
    }

    CTreemap::Item* TmiGetChild(int c) const override
    {
        return GetChild(c);
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
    const CItem* UpwardGetRoot() const;
    void UpdateLastChange();
    CItem* GetChild(int i) const;
    CItem* GetParent() const;
    int FindChildIndex(const CItem* child) const;
    void AddChild(CItem* child);
    void RemoveChild(int i);
    void RemoveAllChildren();
    void UpwardAddSubdirs(ULONGLONG dirCount);
    void UpwardSubtractSubdirs(ULONGLONG dirCount);
    void UpwardAddFiles(ULONGLONG fileCount);
    void UpwardSubtractFiles(ULONGLONG fileCount);
    void UpwardAddSize(ULONGLONG bytes);
    void UpwardSubtractSize(ULONGLONG bytes);
    void UpwardAddReadJobs(ULONGLONG count);
    void UpwardSubtractReadJobs(ULONGLONG count);
    void UpwardUpdateLastChange(const FILETIME& t);
    void UpwardRecalcLastChange();
    ULONGLONG GetSize() const;
    void SetSize(ULONGLONG ownSize);
    ULONGLONG GetReadJobs() const;
    FILETIME GetLastChange() const;
    void SetLastChange(const FILETIME& t);
    void SetAttributes(DWORD attr);
    DWORD GetAttributes() const;
    int GetSortAttributes() const;
    double GetFraction() const;
    ITEMTYPE GetType() const;
    bool IsType(const ITEMTYPE type) const;
    bool IsRootItem() const;
    CStringW GetPath() const;
    bool HasUncPath() const;
    CStringW GetFindPattern() const;
    CStringW GetFolderPath() const;
    CStringW GetReportPath() const;
    CStringW GetName() const;
    CStringW GetExtension() const;
    ULONGLONG GetFilesCount() const;
    ULONGLONG GetSubdirsCount() const;
    ULONGLONG GetItemsCount() const;
    bool IsReadJobDone() const;
    void SetReadJobDone(bool done = true);
    bool IsDone() const;
    void SetDone();
    ULONGLONG GetTicksWorked() const;
    void AddTicksWorked(ULONGLONG more);
    void DoSomeWork(CWorkLimiter* limiter);
    bool StartRefresh();
    void UpwardSetUndone();
    void RefreshRecycler() const;
    void CreateFreeSpaceItem();
    CItem* FindFreeSpaceItem() const;
    void UpdateFreeSpaceItem() const;
    void RemoveFreeSpaceItem();
    void CreateUnknownItem();
    CItem* FindUnknownItem() const;
    void RemoveUnknownItem();
    CItem* FindDirectoryByPath(const CStringW& path);
    void RecurseCollectExtensionData(CExtensionData* ed);

private:
    ULONGLONG GetProgressRangeMyComputer() const;
    ULONGLONG GetProgressPosMyComputer() const;
    ULONGLONG GetProgressRangeDrive() const;
    ULONGLONG GetProgressPosDrive() const;
    COLORREF GetGraphColor() const;
    bool MustShowReadJobs() const;
    COLORREF GetPercentageColor() const;
    int FindFreeSpaceItemIndex() const;
    int FindUnknownItemIndex() const;
    CStringW UpwardGetPathWithoutBackslash() const;
    void AddDirectory(FileFindEnhanced& finder);
    void AddFile(const FILEINFO& fi);
    void DriveVisualUpdateDuringWork();
    void UpwardDrivePacman();
    void DrivePacman();

    ITEMTYPE m_type; // Indicates our type. See ITEMTYPE.
    CStringW m_name;              // Display name
    mutable CStringW m_extension; // Cache of extension (it's used often)
    mutable bool m_extension_cached;
    ULONGLONG m_size;           // OwnSize, if IT_FILE or IT_FREESPACE, or IT_UNKNOWN; SubtreeTotal else.
    ULONGLONG m_files;          // # Files in subtree
    ULONGLONG m_subdirs;        // # Folder in subtree
    FILETIME m_lastChange;      // Last modification time OF SUBTREE
    unsigned char m_attributes; // Packed file attributes of the item

    bool m_readJobDone;      // FindFiles() (our own read job) is finished.
    bool m_done;             // Whole Subtree is done.
    ULONGLONG m_ticksWorked; // ms time spent on this item.
    ULONGLONG m_readJobs;    // # "read jobs" in subtree.


    // Our children. When "this" is set to "done", this array is sorted by child size.
    CArray<CItem*, CItem*> m_children;

    // For GraphView:
    RECT m_rect; // Finally, this is our coordinates in the Treemap view.
};
