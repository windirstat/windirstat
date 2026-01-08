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

// Item types - all values are bitmasks for efficient type checking
using ITEMTYPE = enum ITEMTYPE : std::uint32_t
{
    IT_NONE        = 0,       // No type
    IT_MYCOMPUTER  = 1 << 0,  // Pseudo Container "My Computer"
    IT_DRIVE       = 1 << 1,  // C:\, D:\ etc.
    IT_DIRECTORY   = 1 << 2,  // Folder
    IT_FILE        = 1 << 3,  // Regular file
    IT_FREESPACE   = 1 << 4,  // Pseudo File "<Free Space>"
    IT_UNKNOWN     = 1 << 5,  // Pseudo File "<Unknown>"
    IT_HLINKS      = 1 << 6,  // Pseudo Container "<Hardlinks>"
    IT_HLINKS_SET  = 1 << 7,  // Pseudo Folder "Index Set N" under <Hardlinks>
    IT_HLINKS_IDX  = 1 << 8,  // Pseudo Folder "Index N" under Index Set
    IT_HLINKS_FILE = 1 << 9,  // Pseudo File reference under Index N
    IT_MASK        = 0x0000FFFF,

    ITHASH_NONE    = 0,       // Indicates no hash
    ITHASH_SKIP    = 1 << 16, // Indicates cannot be hashed (unreadable)
    ITHASH_PART    = 1 << 17, // Indicates a partial hash
    ITHASH_FULL    = 1 << 18, // Indicates a full hash
    ITHASH_MASK    = 0x000F0000,

    ITRP_NONE      = 0,       // Indicates no reparse data
    ITRP_SYMLINK   = 1 << 20, // Indicates a reparse point that is a symlink
    ITRP_MOUNT     = 1 << 21, // Indicates a reparse point that is a mount point
    ITRP_JUNCTION  = 1 << 22, // Indicates a reparse point that is a junction
    ITRP_CLOUD     = 1 << 23, // Indicates a reparse point that is a cloud link
    ITRP_MASK      = 0x00F00000,

    ITF_NONE       = 0,       // No flags
    ITF_RESERVED   = 1 << 24, // Indicates special reserved file
    ITF_BASIC      = 1 << 25, // Forces basic finder
    ITF_HARDLINK   = 1 << 26, // Indicates file is a hardlink
    ITF_ROOTITEM   = 1 << 27, // Indicates root item
    ITF_DONE       = 1 << 28, // Indicates done processing
    ITF_MASK       = 0xFF000000,

    ITF_ANY        = 0xFFFFFFFF, // Indicates any item type or flag
};

constexpr ITEMTYPE operator~(const ITEMTYPE& a)
{
    return static_cast<ITEMTYPE>(~static_cast<std::uint32_t>(a));
}

constexpr ITEMTYPE operator|(const ITEMTYPE & a, const ITEMTYPE & b)
{
    return static_cast<ITEMTYPE>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

constexpr ITEMTYPE operator&(const ITEMTYPE& a, const ITEMTYPE& b)
{
    return static_cast<ITEMTYPE>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

constexpr auto operator<=>(const FILETIME& t1, const FILETIME& t2)
{
    if (const auto cmp = t1.dwHighDateTime <=> t2.dwHighDateTime; cmp != 0) return cmp;
    return t1.dwLowDateTime <=> t2.dwLowDateTime;
}

constexpr bool operator==(const FILETIME& t1, const FILETIME& t2)
{
    return (t1 <=> t2) == 0;
}

//
// CItem. This is the object, from which the whole tree is built.
// For every directory, file etc., we find on the hard disks, there is one CItem.
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
class CItem final : public CTreeListItem, public CTreeMap::Item
{
public:
    CItem(const CItem&) = delete;
    CItem(CItem&&) = delete;
    CItem& operator=(const CItem&) = delete;
    CItem& operator=(CItem&&) = delete;
    CItem(ITEMTYPE type, const std::wstring& name);
    CItem(ITEMTYPE type, const std::wstring& name, FILETIME lastChange, ULONGLONG sizePhysical,
        ULONGLONG sizeLogical, ULONGLONG index, DWORD attributes, ULONG files, ULONG subdirs);
    ~CItem() override;

    // CTreeListItem Interface
    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    COLORREF GetItemTextColor() const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    int GetTreeListChildCount() const noexcept override { return IsLeaf() ? 0 : static_cast<int>(GetChildren().size()); }
    CTreeListItem* GetTreeListChild(const int i) const noexcept override { return GetChildren()[i]; }
    HICON GetIcon() override;
    void DrawAdditionalState(CDC* pdc, const CRect& rcLabel) const override;
    CItem* GetLinkedItem() override;

    // CTreeMap::Item interface
    bool TmiIsLeaf() const noexcept override { return IsLeaf() || IsTypeOrFlag(IT_HLINKS_IDX); }
    CRect TmiGetRectangle() const noexcept override { return tmiRect; };
    void TmiSetRectangle(const CRect& rc) noexcept override { tmiRect = rc; }
    COLORREF TmiGetGraphColor() const override { return GetGraphColor(); }
    int TmiGetChildCount() const noexcept override { 
        if (m_folderInfo == nullptr || IsTypeOrFlag(IT_HLINKS_IDX)) return 0;
        return static_cast<int>(m_folderInfo->m_children.size()); 
    }
    Item* TmiGetChild(const int c) const noexcept override { return m_folderInfo->m_children[c]; }
    ULONGLONG TmiGetSize() const noexcept override { return COptions::TreeMapUseLogical ? GetSizeLogical() : GetSizePhysical(); }

    static int GetSubtreePercentageWidth();
    ULONGLONG GetProgressRange() const;
    ULONGLONG GetProgressPos() const;
    void UpdateStatsFromDisk();
    const std::vector<CItem*>& GetChildren() const noexcept;
    bool IsLeaf() const noexcept { return m_folderInfo == nullptr; }
    CItem* GetParent() const noexcept;
    CItem* GetParentDrive() const noexcept;
    void AddChild(CItem* child, bool addOnly = false);
    void RemoveChild(CItem* child);
    void RemoveAllChildren();
    void UpwardAddFolders(ULONG dirCount) noexcept;
    void UpwardSubtractFolders(ULONG dirCount) noexcept;
    void UpwardAddFiles(ULONG fileCount) noexcept;
    void UpwardSubtractFiles(ULONG fileCount) noexcept;
    void UpwardAddSizePhysical(ULONGLONG bytes) noexcept;
    void UpwardSubtractSizePhysical(ULONGLONG bytes) noexcept;
    void UpwardAddSizeLogical(ULONGLONG bytes) noexcept;
    void UpwardSubtractSizeLogical(ULONGLONG bytes) noexcept;
    void UpwardAddReadJobs(ULONG count) noexcept;
    void UpwardSubtractReadJobs(ULONG count) noexcept;
    void UpwardUpdateLastChange(const FILETIME& t) noexcept;
    void UpwardRecalcLastChange(bool withoutItem = false);
    void ExtensionDataAdd() const;
    void ExtensionDataRemove() const;
    void ExtensionDataProcessChildren(bool remove = false) const;
    ULONGLONG GetSizePhysical() const noexcept;
    ULONGLONG GetSizeLogical() const noexcept;
    ULONGLONG GetSizePhysicalRaw() const noexcept;
    void SetSizePhysical(ULONGLONG size) noexcept;
    void SetSizeLogical(ULONGLONG size) noexcept;
    ULONG GetReadJobs() const noexcept;
    FILETIME GetLastChange() const noexcept;
    void SetLastChange(const FILETIME& t) noexcept;
    void SetAttributes(DWORD attr) noexcept;
    DWORD GetAttributes() const noexcept;
    void SetIndex(ULONGLONG index) noexcept;
    ULONGLONG GetIndex() const noexcept;
    DWORD GetReparseTag() const noexcept;
    void SetReparseTag(DWORD reparseType) noexcept;
    USHORT GetSortAttributes() const noexcept;
    double GetFraction() const noexcept;
    bool IsRootItem() const noexcept;
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
    ULONG GetFilesCount() const noexcept;
    ULONG GetFoldersCount() const noexcept;
    ULONGLONG GetItemsCount() const noexcept;
    void SetDone();
    void SortItemsBySizePhysical() const;
    void SortItemsBySizeLogical() const;
    ULONGLONG GetTicksWorked() const noexcept;
    void ResetScanStartTime() const noexcept;
    static void ScanItems(BlockingQueue<CItem*> *, FinderNtfsContext& contextNtfs, FinderBasicContext& contextBasic);
    static void ScanItemsFinalize(CItem* item);
    void UpwardSetDone() noexcept;
    void UpwardSetUndone() noexcept;
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
    CItem* FindHardlinksIndexItem() const;
    void RemoveHardlinksItem();
    void UpwardDrivePacman();
    void DoHardlinkAdjustment();
    std::vector<CItem*> GetDriveItems() const;

    std::vector<BYTE> GetFileHash(ULONGLONG hashSizeLimit, BlockingQueue<CItem*>* queue);
    
    bool IsDone() const noexcept
    {
        return IsTypeOrFlag(ITF_DONE);
    }

    ITEMTYPE GetItemType() const noexcept
    {
        return m_type & IT_MASK;
    }

    ITEMTYPE GetRawType() const noexcept
    {
        return m_type;
    }

    template<typename... Args>
    constexpr bool IsTypeOrFlag(Args... args) const noexcept
    {
        const ITEMTYPE combinedMask = (args | ...);
        return (m_type & combinedMask) != IT_NONE;
    }

    template<ITEMTYPE Mask>
    void SetType(const ITEMTYPE type, const bool bitOp = false, const bool unsetVal = false) noexcept
    {
        if (unsetVal) m_type = (m_type & ~type);
        else m_type = bitOp ? (m_type | type) : ((m_type & ~Mask) | type);
    }

    void SetReparseType(const ITEMTYPE type) noexcept { SetType<ITRP_MASK>(type); }
    void SetHashType(const ITEMTYPE type, const bool addType = true) noexcept { SetType<ITHASH_MASK>(type, addType); }
    void SetFlag(const ITEMTYPE type, const bool unsetVal = false) noexcept { SetType<ITF_MASK>(type, true, unsetVal); }

    static constexpr bool FileTimeIsGreater(const FILETIME& ft1, const FILETIME& ft2) noexcept
    {
        return (static_cast<QWORD>(ft1.dwHighDateTime) << 32 | (ft1.dwLowDateTime)) >
            (static_cast<QWORD>(ft2.dwHighDateTime) << 32 | (ft2.dwLowDateTime));
    }

private:
    ULONGLONG GetProgressRangeMyComputer() const;
    ULONGLONG GetProgressRangeDrive() const;
    COLORREF GetGraphColor() const;
    bool MustShowReadJobs() const noexcept;
    COLORREF GetPercentageColor() const noexcept;
    std::wstring UpwardGetPathWithoutBackslash() const;
    CItem* AddDirectory(const Finder& finder);
    CItem* AddFile(Finder& finder);

    // Used for initialization of hashing process
    static std::once_flag s_HashInitFlag;
    static BCRYPT_ALG_HANDLE s_HashAlgHandle;
    static DWORD s_HashLength;

    // Special structure for container items that is separately allocated to
    // reduce memory usage.  This operates under the assumption that most
    // containers have files in them.
    using CHILDINFO = struct CHILDINFO
    {
        std::vector<CItem*> m_children;
        std::atomic<ULONG> m_tstart = 0;  // time this node started enumerating
        std::atomic<ULONG> m_tfinish = 0; // time this node finished enumerating
        std::atomic<ULONG> m_files = 0;   // # Files in subtree
        std::atomic<ULONG> m_subdirs = 0; // # Folders in subtree
        std::atomic<ULONG> m_jobs = 0;    // # "read jobs" in subtree.
    };

    std::unique_ptr<wchar_t[]> m_name;         // Display name
    std::unique_ptr<CHILDINFO> m_folderInfo;   // Child information for non-files
    std::atomic<ULONGLONG> m_sizePhysical = 0; // Total physical size of self or subtree
    std::atomic<ULONGLONG> m_sizeLogical = 0;  // Total local size of self or subtree
    FILETIME m_lastChange = { 0, 0 };          // Last modification time of self or subtree
    ULONGLONG m_index = 0;                     // Index of item for special scan types
    CSmallRect tmiRect = {};                   // Treemap rectangle
    ITEMTYPE m_type;                           // Indicates our type.
    USHORT m_attributes = 0xFFFF;              // File or directory attributes of the item
    USHORT m_nameLen = 0;                      // Length of name string
};
