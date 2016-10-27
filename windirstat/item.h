// item.h - Declaration of CItem
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2016 WinDirStat team (windirstat.info)
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

#ifndef __WDS_ITEM_H__
#define __WDS_ITEM_H__
#pragma once

#include "Treelistcontrol.h"
#include "treemap.h"
#include "dirstatdoc.h" // CExtensionData
#include "FileFindWDS.h" // CFileFindWDS
#include <common/wds_constants.h>

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
enum ITEMTYPE
{
    IT_MYCOMPUTER,      // Pseudo Container "My Computer"
    IT_DRIVE,           // C:\, D:\ etc.
    IT_DIRECTORY,       // Folder
    IT_FILE,            // Regular file
    IT_FILESFOLDER,     // Pseudo Folder "<Files>"
    IT_FREESPACE,       // Pseudo File "<Free Space>"
    IT_UNKNOWN,         // Pseudo File "<Unknown>"

    ITF_FLAGS    = 0xF000,
    ITF_ROOTITEM = 0x8000   // This is an additional flag, not a type.
};

// Whether an item type is a leaf type
inline bool IsLeaf(ITEMTYPE t) { return ((t == IT_FILE) | (t == IT_FREESPACE) | (t == IT_UNKNOWN)); }

// Compare FILETIMEs
inline bool operator< (const FILETIME& t1, const FILETIME& t2)
{
    // FIXME: it is *not* appropriate to cast like this due to alignment issues
    //        see <http://blogs.msdn.com/b/oldnewthing/archive/2004/08/25/220195.aspx>
    ULONGLONG& u1 = (ULONGLONG&)t1;
    ULONGLONG& u2 = (ULONGLONG&)t2;

    return (u1 < u2);
}

// Compare FILETIMEs
inline bool operator == (const FILETIME& t1, const FILETIME& t2)
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
class CItem: public CTreeListItem, public CTreemap::Item
{
    // We collect data of files in FILEINFOs before we create items for them,
    // because we need to know their count before we can decide whether or not
    // we have to create a <Files> item. (A <Files> item is only created, when
    // (a) there are more than one files and (b) there are subdirectories.)
    struct FILEINFO
    {
        CString name;
        ULONGLONG length;
        FILETIME lastWriteTime;
        DWORD attributes;
    };

public:
    CItem(ITEMTYPE type, LPCTSTR name, bool dontFollow = false);
    ~CItem();

    // CTreeListItem Interface
    virtual bool DrawSubitem(int subitem, CDC *pdc, CRect rc, UINT state, int *width, int *focusLeft) const;
    virtual CString GetText(int subitem) const;
    virtual COLORREF GetItemTextColor() const;
    virtual int CompareSibling(const CTreeListItem *tlib, int subitem) const;
    virtual int GetChildrenCount() const;
    virtual CTreeListItem *GetTreeListChild(int i) const;
    virtual int GetImageToCache() const;
    virtual void DrawAdditionalState(CDC *pdc, const CRect& rcLabel) const;

    // CTreemap::Item interface
    virtual            bool TmiIsLeaf()                const { return IsLeaf(GetType()); }
    virtual           CRect TmiGetRectangle()          const;
    virtual            void TmiSetRectangle(const CRect& rc);
    virtual        COLORREF TmiGetGraphColor()         const { return GetGraphColor(); }
    virtual             int TmiGetChildrenCount()      const { return GetChildrenCount(); }
    virtual CTreemap::Item *TmiGetChild(int c)         const { return GetChild(c); }
    virtual       ULONGLONG TmiGetSize()               const { return GetSize(); }

    // CItem
    static int GetSubtreePercentageWidth();
    static CItem *FindCommonAncestor(const CItem *item1, const CItem *item2);

    bool IsAncestorOf(const CItem *item) const;
    ULONGLONG GetProgressRange() const;
    ULONGLONG GetProgressPos() const;
    const CItem *UpwardGetRoot() const;
    void UpdateLastChange();
    CItem *GetChild(int i) const;
    CItem *GetParent() const;
    int FindChildIndex(const CItem *child) const;
    void AddChild(CItem *child);
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
    bool IsRootItem() const;
    CString GetPath() const;
    bool HasUncPath() const;
    CString GetFindPattern() const;
    CString GetFolderPath() const;
    CString GetReportPath() const;
    CString GetName() const;
    CString GetExtension() const;
    ULONGLONG GetFilesCount() const;
    ULONGLONG GetSubdirsCount() const;
    ULONGLONG GetItemsCount() const;
    bool IsReadJobDone() const;
    void SetReadJobDone(bool done = true);
    bool IsDone() const;
    void SetDone();
    DWORD GetTicksWorked() const;
    void AddTicksWorked(DWORD more);
    void DoSomeWork(CWorkLimiter* limiter);
    bool StartRefresh();
    void UpwardSetUndone();
    void RefreshRecycler();
    void CreateFreeSpaceItem();
    CItem *FindFreeSpaceItem() const;
    void UpdateFreeSpaceItem();
    void RemoveFreeSpaceItem();
    void CreateUnknownItem();
    CItem *FindUnknownItem() const;
    void RemoveUnknownItem();
    CItem *FindDirectoryByPath(const CString& path);
    void RecurseCollectExtensionData(CExtensionData *ed);

private:
    static int __cdecl _compareBySize(const void *p1, const void *p2);
    ULONGLONG GetProgressRangeMyComputer() const;
    ULONGLONG GetProgressPosMyComputer() const;
    ULONGLONG GetProgressRangeDrive() const;
    ULONGLONG GetProgressPosDrive() const;
    COLORREF GetGraphColor() const;
    bool MustShowReadJobs() const;
    COLORREF GetPercentageColor() const;
    int FindFreeSpaceItemIndex() const;
    int FindUnknownItemIndex() const;
    CString UpwardGetPathWithoutBackslash() const;
    void AddDirectory(CFileFindWDS& finder);
    void AddFile(const FILEINFO& fi);
    void DriveVisualUpdateDuringWork();
    void UpwardDrivePacman();
    void DrivePacman();

    ITEMTYPE m_type;            // Indicates our type. See ITEMTYPE.
    ITEMTYPE m_etype;           
    CString m_name;             // Display name
    mutable CString m_extension;		// Cache of extension (it's used often)
    mutable bool m_extension_cached = false;
    ULONGLONG m_size;           // OwnSize, if IT_FILE or IT_FREESPACE, or IT_UNKNOWN; SubtreeTotal else.
    ULONGLONG m_files;          // # Files in subtree
    ULONGLONG m_subdirs;        // # Folder in subtree
    FILETIME m_lastChange;      // Last modification time OF SUBTREE
    unsigned char m_attributes; // Packed file attributes of the item

    bool m_readJobDone;         // FindFiles() (our own read job) is finished.
    bool m_done;                // Whole Subtree is done.
    DWORD m_ticksWorked;        // ms time spent on this item.
    ULONGLONG m_readJobs;       // # "read jobs" in subtree.


    // Our children. When "this" is set to "done", this array is sorted by child size.
    CArray<CItem *, CItem *> m_children;

    // For GraphView:
    RECT m_rect;                // Finally, this is our coordinates in the Treemap view.
};

#endif // __WDS_ITEM_H__
