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

#include "pch.h"
#include "TreeListControl.h"
#include "TreeMap.h"
#include "Finder.h"

class Finder;
class FinderNtfsContext;
class FinderBasicContext;

// Columns
enum ITEMCOLUMNS : std::uint8_t
{
    COL_NAME,
    COL_SUBTREE_PERCENTAGE,
    COL_PERCENTAGE,
    COL_OPTIONAL_START,
    COL_SIZE_PHYSICAL,
    COL_SIZE_LOGICAL,
    COL_ITEMS,
    COL_FILES,
    COL_FOLDERS,
    COL_LAST_CHANGE,
    COL_ATTRIBUTES,
    COL_OWNER
};

// Item types
using ITEMTYPE = enum ITEMTYPE : unsigned short
{
    IT_NONE       = 0 << 0, // No type
    IT_MYCOMPUTER = 1 << 0, // Pseudo Container "My Computer"
    IT_DRIVE      = 2 << 0, // C:\, D:\ etc.
    IT_DIRECTORY  = 3 << 0, // Folder
    IT_FILE       = 4 << 0, // Regular file
    IT_HARDLINKS  = 5 << 0, // Pseudo File "<Hardlinks>"
    IT_FREESPACE  = 6 << 0, // Pseudo File "<Free Space>"
    IT_UNKNOWN    = 7 << 0, // Pseudo File "<Unknown>"
    IT_MASK       = 0x000F, // Mask for item type

    ITHASH_NONE   = 0 << 4, // Indicates no hash
    ITHASH_SKIP   = 1 << 4, // Indicates cannot be hashed (unreadable)
    ITHASH_PART   = 2 << 4, // Indicates a partial hash
    ITHASH_FULL   = 3 << 4, // Indicates a full hash
    ITHASH_MASK   = 3 << 4, // Any hash state

    ITRP_NONE     = 0 << 6, // Indicates no reparse data
    ITRP_SYMLINK  = 1 << 6, // Indicates a reparse point that is a symlink
    ITRP_MOUNT    = 2 << 6, // Indicates a reparse point that is a mount point
    ITRP_JUNCTION = 3 << 6, // Indicates a reparse point that is a junction
    ITRP_CLOUD    = 4 << 6, // Indicates a reparse point that is a cloud link
    ITRP_MASK     = 7 << 6, // Indicates a reparse point that is a cloud link

    ITF_NONE      = 0 << 9,  // No flags
    ITF_UNUSED1   = 1 << 10, // Unused
    ITF_UNUSED2   = 1 << 11, // Unused
    ITF_BASIC     = 1 << 12, // Forces basic finder
    ITF_HARDLINK  = 1 << 13, // Indicates file is a hardlink (LinkCount > 1)
    ITF_ROOTITEM  = 1 << 14, // Indicates root item
    ITF_DONE      = 1 << 15, // Indicates done processing
    ITF_FLAGS     = 0xFE00,  // All potential flag items
    ITF_ANY       = 0xFFFF   // Indicates any item type or flag
};

constexpr ITEMTYPE operator~(const ITEMTYPE& a)
{
    return static_cast<ITEMTYPE>(~static_cast<USHORT>(a));
}

constexpr ITEMTYPE operator|(const ITEMTYPE & a, const ITEMTYPE & b)
{
    return static_cast<ITEMTYPE>(static_cast<USHORT>(a) | static_cast<USHORT>(b));
}

constexpr ITEMTYPE operator&(const ITEMTYPE& a, const ITEMTYPE& b)
{
    return static_cast<ITEMTYPE>(static_cast<USHORT>(a) & static_cast<USHORT>(b));
}

constexpr bool operator<(const FILETIME& t1, const FILETIME& t2)
{
    return t1.dwHighDateTime < t2.dwHighDateTime ||
        t1.dwHighDateTime == t2.dwHighDateTime && t1.dwLowDateTime < t2.dwLowDateTime;
}

constexpr bool operator==(const FILETIME& t1, const FILETIME& t2)
{
    return t1.dwLowDateTime == t2.dwLowDateTime && t1.dwHighDateTime == t2.dwHighDateTime;
}

//
// CItem. This is the object, from which the whole tree is built.
// For every directory, file etc., we find on the hard disks, there is one CItem.
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
class CItem final : public CTreeListItem, public CTreeMap::Item
{
public:
    CItem(const CItem&) = delete;
    CItem(CItem&&) = delete;
    CItem& operator=(const CItem&) = delete;
    CItem& operator=(CItem&&) = delete;
    CItem(ITEMTYPE type, const std::wstring& name);
    CItem(ITEMTYPE type, const std::wstring& name, FILETIME lastChange, ULONGLONG sizePhysical,
        ULONGLONG sizeLogical, ULONG index, DWORD attributes, ULONG files, ULONG subdirs);
    ~CItem() override;

    // CTreeListItem Interface
    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    COLORREF GetItemTextColor() const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    int GetTreeListChildCount() const override { return IsLeaf() ? 0 : static_cast<int>(GetChildren().size()); }
    CTreeListItem* GetTreeListChild(const int i) const override { return GetChildren()[i]; }
    HICON GetIcon() override;
    void DrawAdditionalState(CDC* pdc, const CRect& rcLabel) const override;

    // CTreeMap::Item interface
    bool TmiIsLeaf() const override { return IsLeaf(); }
    CRect TmiGetRectangle() const override { return tmiRect; };
    void TmiSetRectangle(const CRect& rc) override { tmiRect = rc; }
    COLORREF TmiGetGraphColor() const override { return GetGraphColor(); }
    int TmiGetChildCount() const override { return m_FolderInfo == nullptr ? 0 : static_cast<int>(m_FolderInfo->m_Children.size()); }
    Item* TmiGetChild(const int c) const override { return m_FolderInfo->m_Children[c]; }
    ULONGLONG TmiGetSize() const override { return COptions::TreeMapUseLogical ? GetSizeLogical() : GetSizePhysical(); }

    static int GetSubtreePercentageWidth();
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
    ULONGLONG GetSizePhysicalRaw() const;
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
    USHORT GetSortAttributes() const;
    double GetFraction() const;
    bool IsRootItem() const;
    std::wstring GetPath() const;
    std::wstring GetPathLong() const;
    CItem* FindItemByPath(const std::wstring& path) const;
    std::vector<CItem*> FindItemsBySameIndex() const;
    std::wstring GetOwner(bool force = false) const;
    bool HasUncPath() const;
    std::wstring GetFolderPath() const;
    void SetName(std::wstring_view name);
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
    static void ScanItems(BlockingQueue<CItem*> *, FinderNtfsContext& contextNtfs, FinderBasicContext& contextBasic);
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
    void CreateHardlinksItem();
    CItem* FindHardlinksItem() const;
    void UpwardDrivePacman();
    void DoHardlinkAdjustment();

    std::vector<BYTE> GetFileHash(ULONGLONG hashSizeLimit, BlockingQueue<CItem*>* queue);
    
    bool IsDone() const
    {
        return HasFlag(ITF_DONE);
    }

    ITEMTYPE GetItemType() const
    {
        return m_Type & IT_MASK;
    }

    ITEMTYPE GetRawType() const
    {
        return m_Type;
    }

    template<ITEMTYPE Mask, typename... Args>
    bool HasType(const bool bitOp = false, Args... args) const
    {
        if (bitOp) return ((args == ITF_ANY || ((m_Type & args) != 0)) || ...);
        return ((args == ITF_ANY || ((m_Type & Mask) == args)) || ...);
    }

    template<ITEMTYPE Mask>
    void SetType(const ITEMTYPE type, const bool bitOp = false, const bool unsetVal = false)
    {
        if (unsetVal) m_Type = (m_Type & ~type);
        else m_Type = bitOp ? (m_Type | type) : ((m_Type & ~Mask) | type);
    }

    template<typename... Args>
    bool IsType(Args... args) const { return HasType<IT_MASK>(false, args...); }

    void SetItemType(const ITEMTYPE type) { SetType<IT_MASK>(type); }

    template<typename... Args>
    bool IsReparseType(Args... args) const { return HasType<ITRP_MASK>(false, args...); }

    void SetReparseType(const ITEMTYPE type) { SetType<ITRP_MASK>(type); }

    template<typename... Args>
    bool IsHashType(Args... args) const { return HasType<ITHASH_MASK>(false, args...); }

    void SetHashType(const ITEMTYPE type) { SetType<ITHASH_MASK>(type); }

    template<typename... Args>
    bool HasFlag(Args... args) const { return HasType<ITF_FLAGS>(true, args...); }

    void SetFlag(const ITEMTYPE type, const bool unsetVal = false) { SetType<ITF_FLAGS>(type, true, unsetVal); }

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
    CItem* AddFile(Finder& finder);

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
        std::atomic<ULONG> m_Tstart = 0;  // time this node started enumerating
        std::atomic<ULONG> m_Tfinish = 0; // time this node finished enumerating
        std::atomic<ULONG> m_Files = 0;   // # Files in subtree
        std::atomic<ULONG> m_Subdirs = 0; // # Folders in subtree
        std::atomic<ULONG> m_Jobs = 0;    // # "read jobs" in subtree.
    };

    std::unique_ptr<wchar_t[]> m_Name;         // Display name
    std::unique_ptr<CHILDINFO> m_FolderInfo;   // Child information for non-files
    std::atomic<ULONGLONG> m_SizePhysical = 0; // Total physical size of self or subtree
    std::atomic<ULONGLONG> m_SizeLogical = 0;  // Total local size of self or subtree
    FILETIME m_LastChange = { 0, 0 };          // Last modification time of self or subtree
    ULONG m_Index = 0;                         // Index of item for special scan types
    USHORT m_Attributes = 0xFFFF;              // File or directory attributes of the item
    USHORT m_NameLen = 0;
    ITEMTYPE m_Type;                           // Indicates our type.
    CSmallRect tmiRect = {};                   // Treemap rectangle
};
