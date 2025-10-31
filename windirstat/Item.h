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

#pragma once

#include "TreeListControl.h"
#include "TreeMap.h"
#include "DirStatDoc.h"
#include "Finder.h"
#include "BlockingQueue.h"

#include <shared_mutex>

class Finder;
class FinderNtfsContext;

// Columns
enum ITEMCOLUMNS : std::uint8_t
{
    COL_NAME,
    COL_SUBTREEPERCENTAGE,
    COL_PERCENTAGE,
    COL_OPTIONAL_START,
    COL_SIZE_PHYSICAL,
    COL_SIZE_LOGICAL,
    COL_ITEMS,
    COL_FILES,
    COL_FOLDERS,
    COL_LASTCHANGE,
    COL_ATTRIBUTES,
    COL_OWNER
};

// Item types
enum ITEMTYPE : unsigned short
{
    IT_MYCOMPUTER = 1 << 0,  // Pseudo Container "My Computer"
    IT_DRIVE      = 1 << 1,  // C:\, D:\ etc.
    IT_DIRECTORY  = 1 << 2,  // Folder
    IT_FILE       = 1 << 3,  // Regular file
    IT_FREESPACE  = 1 << 4,  // Pseudo File "<Free Space>"
    IT_UNKNOWN    = 1 << 5,  // Pseudo File "<Unknown>"
    IT_ANY        = 0x00FF,  // Indicates any item type
    ITF_DONE      = 1 << 8,  // Indicates done processing
    ITF_ROOTITEM  = 1 << 9,  // Indicates root item
    ITF_SKIPHASH  = 1 << 10, // Indicates cannot be hased (unreadable)
    ITF_PARTHASH  = 1 << 11, // Indicates a partial hash
    ITF_FULLHASH  = 1 << 12, // Indicates a full hash
    ITF_SYMLINK   = 0b001 << 13, // Indicates a reparse point that is a symlink
    ITF_MOUNTPNT  = 0b010 << 13, // Indicates a reparse point that is a mount point
    ITF_JUNCTION  = 0b011 << 13, // Indicates a reparse point that is a junction
    ITF_CLOUDLINK = 0b100 << 13, // Indicates a reparse point that is a cloud link
    ITF_RPMASK    = 0b111 << 13, // Indicates a reparse point
    ITF_FLAGS     = 0xFF00, // All potential flag items
};

constexpr ITEMTYPE operator|(const ITEMTYPE & a, const ITEMTYPE & b)
{
    return static_cast<ITEMTYPE>(static_cast<unsigned short>(a) | static_cast<unsigned short>(b));
}

constexpr ITEMTYPE operator-(const ITEMTYPE& a, const ITEMTYPE& b)
{
    return static_cast<ITEMTYPE>(static_cast<unsigned short>(a) & ~static_cast<unsigned short>(b));
}

// Compare FILETIMEs
constexpr bool operator<(const FILETIME& t1, const FILETIME& t2)
{
    return t1.dwHighDateTime < t2.dwHighDateTime
    || t1.dwHighDateTime == t2.dwHighDateTime && t1.dwLowDateTime < t2.dwLowDateTime;
}

// Compare FILETIMEs
constexpr bool operator==(const FILETIME& t1, const FILETIME& t2)
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
// The m_Type indicates whether we are a file or a folder or a drive etc.
// It may have been better to design a class hierarchy for this, but I can't help it,
// rather than browsing to virtual functions I like to flatly see what's going on.
// But, of course, now we have quite many switch statements in the member functions.
//
// Naming convention:
// Methods which recurse down to every child (expensive) are named "RecurseDoSomething".
// Methods which recurse up to the parent (not so expensive) are named "UpwardDoSomething".
//
class CItem final : public CTreeListItem, public CTreeMap::Item
{
public:
    // Initial reserve size (16) for path construction based on filesystem limit
    static constexpr int DEFAULT_PATH_RESERVE = 16;

    CItem(const CItem&) = delete;
    CItem(CItem&&) = delete;
    CItem& operator=(const CItem&) = delete;
    CItem& operator=(CItem&&) = delete;
    CItem(ITEMTYPE type, const std::wstring& name);
    CItem(ITEMTYPE type, const std::wstring& name, FILETIME lastChange, ULONGLONG sizePhysical,
        ULONGLONG sizeLogical, DWORD attributes, ULONG files, ULONG subdirs);
    ~CItem() override;

    // CTreeListItem Interface
    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    COLORREF GetItemTextColor() const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    int GetTreeListChildCount() const override;
    CTreeListItem* GetTreeListChild(int i) const override;
    HICON GetIcon() override;
    void DrawAdditionalState(CDC* pdc, const CRect& rcLabel) const override;

    // CTreeMap::Item interface
    bool TmiIsLeaf() const override
    {
        return IsLeaf();
    }

    CRect TmiGetRectangle() const override;
    void TmiSetRectangle(const CRect& rc) override;

    COLORREF TmiGetGraphColor() const override
    {
        return GetGraphColor();
    }

    int TmiGetChildCount() const override
    {
        if (!m_FolderInfo) return 0;
        return static_cast<int>(m_FolderInfo->m_Children.size());
    }

    Item* TmiGetChild(const int c) const override
    {
        return m_FolderInfo->m_Children[c];
    }

    ULONGLONG TmiGetSize() const override
    {
        return COptions::TreeMapUseLogical ? GetSizeLogical() : GetSizePhysical();
    }

    // CItem
    static int GetSubtreePercentageWidth();
    static CItem* FindCommonAncestor(const CItem* item1, const CItem* item2);

    ULONGLONG GetProgressRange() const;
    ULONGLONG GetProgressPos() const;
    void UpdateStatsFromDisk();
    const std::vector<CItem*>& GetChildren() const;
    bool IsLeaf() const { return m_FolderInfo == nullptr; }
    CItem* GetParent() const;
    CItem* GetParentDrive() const;
    void AddChild(CItem* child, bool addOnly = false);
    void RemoveChild(CItem* child);
    void RemoveAllChildren();
    void UpwardAddFolders(ULONG dirCount);
    void UpwardSubtractFolders(ULONG dirCount);
    void UpwardAddFiles(ULONG fileCount);
    void UpwardSubtractFiles(ULONG fileCount);
    void UpwardAddSizePhysical(ULONGLONG bytes);
    void UpwardSubtractSizePhysical(ULONGLONG bytes);
    void UpwardAddSizeLogical(ULONGLONG bytes);
    void UpwardSubtractSizeLogical(ULONGLONG bytes);
    void UpwardAddReadJobs(ULONG count);
    void UpwardSubtractReadJobs(ULONG count);
    void UpwardUpdateLastChange(const FILETIME& t);
    void UpwardRecalcLastChange(bool withoutItem = false);
    void ExtensionDataAdd() const;
    void ExtensionDataRemove() const;
    void ExtensionDataProcessChildren(bool remove = false) const;
    ULONGLONG GetSizePhysical() const;
    ULONGLONG GetSizeLogical() const;
    void SetSizePhysical(ULONGLONG size);
    void SetSizeLogical(ULONGLONG size);
    ULONG GetReadJobs() const;
    FILETIME GetLastChange() const;
    void SetLastChange(const FILETIME& t);
    void SetAttributes(DWORD attr);
    DWORD GetAttributes() const;
    void SetIndex(DWORD index);
    DWORD GetIndex() const;
    DWORD GetReparseTag() const;
    void SetReparseTag(DWORD reparseType);
    unsigned short GetSortAttributes() const;
    double GetFraction() const;
    bool IsRootItem() const;
    std::wstring GetPath() const;
    std::wstring GetPathLong() const;
    std::wstring GetOwner(bool force = false) const;
    bool HasUncPath() const;
    std::wstring GetFolderPath() const;
    std::wstring GetName() const;
    std::wstring GetExtension() const;
    ULONG GetFilesCount() const;
    ULONG GetFoldersCount() const;
    ULONGLONG GetItemsCount() const;
    void SetDone();
    void SortItemsBySizePhysical() const;
    void SortItemsBySizeLogical() const;
    ULONGLONG GetTicksWorked() const;
    void ResetScanStartTime() const;
    static void ScanItems(BlockingQueue<CItem*> *, FinderNtfsContext& contextNtfs);
    static void ScanItemsFinalize(CItem* item);
    void UpwardSetDone();
    void UpwardSetUndone();
    CItem* FindRecyclerItem() const;
    void CreateFreeSpaceItem();
    CItem* FindFreeSpaceItem() const;
    void UpdateFreeSpaceItem();
    void RemoveFreeSpaceItem();
    void CreateUnknownItem();
    CItem* FindUnknownItem() const;
    void UpdateUnknownItem() const;
    void RemoveUnknownItem();
    void UpwardDrivePacman();

    std::vector<BYTE> GetFileHash(ULONGLONG hashSizeLimit, BlockingQueue<CItem*>* queue);

    bool IsDone() const
    {
        return IsType(ITF_DONE);
    }

    ITEMTYPE GetType() const
    {
        return static_cast<ITEMTYPE>(m_Type & ~ITF_FLAGS);
    }

    ITEMTYPE GetRawType() const
    {
        return m_Type;
    }

    constexpr bool IsType(const ITEMTYPE type) const
    {
        return (m_Type & type) != 0;
    }

    constexpr void SetType(const ITEMTYPE type, const bool set = true)
    {
        if (set) m_Type = m_Type | type;
        else m_Type = m_Type - type;
    }

    constexpr bool IsReparseType(const ITEMTYPE type) const
    {
        return (m_Type & ITF_RPMASK) == type;
    }

    static constexpr bool FileTimeIsGreater(const FILETIME& ft1, const FILETIME& ft2)
    {
        return (static_cast<QWORD>(ft1.dwHighDateTime) << 32 | (ft1.dwLowDateTime)) >
            (static_cast<QWORD>(ft2.dwHighDateTime) << 32 | (ft2.dwLowDateTime));
    }

private:
    ULONGLONG GetProgressRangeMyComputer() const;
    ULONGLONG GetProgressRangeDrive() const;
    COLORREF GetGraphColor() const;
    bool MustShowReadJobs() const;
    COLORREF GetPercentageColor() const;
    std::wstring UpwardGetPathWithoutBackslash() const;
    CItem* AddDirectory(const Finder& finder);
    CItem* AddFile(const Finder& finder);

    // Used for initialization of hashing process
    static std::mutex m_HashMutex;
    static BCRYPT_ALG_HANDLE m_HashAlgHandle;
    static DWORD m_HashLength;

    // Special structure for container items that is separately allocated to
    // reduce memory usage.  This operates under the assumption that most
    // containers have files in them.
    using CHILDINFO = struct CHILDINFO
    {
        std::vector<CItem*> m_Children;
        std::shared_mutex m_Protect;
        std::atomic<ULONG> m_Tstart = 0;  // initial time this node started enumerating
        std::atomic<ULONG> m_Tfinish = 0; // initial time this node started enumerating
        std::atomic<ULONG> m_Files = 0;   // # Files in subtree
        std::atomic<ULONG> m_Subdirs = 0; // # Folder in subtree
        std::atomic<ULONG> m_Jobs = 0;    // # "read jobs" in subtree.
    };

    RECT m_Rect;                               // To support TreeMapView
    std::wstring m_Name;                       // Display name
    FILETIME m_LastChange = {0, 0};            // Last modification time of self or subtree
    std::unique_ptr<CHILDINFO> m_FolderInfo;   // Child information for non-files
    std::atomic<ULONGLONG> m_SizePhysical = 0; // Total physical size of self or subtree
    std::atomic<ULONGLONG> m_SizeLogical = 0;  // Total local size of self or subtree
    ULONG m_Index = 0;                         // Index of item for special scan types
    USHORT m_Attributes = 0xFFFF;              // File or directory attributes of the item
    ITEMTYPE m_Type;                           // Indicates our type.
};
