// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "DedupStore.h"

enum DEDUPITEMKIND : std::uint8_t
{
    DEDUPITEM_SHARED,
    DEDUPITEM_EXTERNAL,
    DEDUPITEM_GARBAGE,
    DEDUPITEM_OVERHEAD,
    DEDUPITEM_GARBAGE_FULL
};

// The scan model owns immutable volume results. Tree bindings supply display values without changing allocations.
class CDedupAccounting final
{
public:
    struct SharingRow
    {
        CItem* item = nullptr;
        ULONGLONG sharedBytes = 0;
        ULONGLONG itemBytes = 0;
    };

    struct SharingResult
    {
        bool success = false;
        std::wstring error;
        std::vector<SharingRow> rows;
        ULONGLONG queriedBytes = 0;
        ULONGLONG sharedBytes = 0;
        ULONGLONG outsideBytes = 0;
    };

    static CDedupAccounting& Get();
    void RunPostScan(const CDedupStore::CancelCheck& cancelled);
    bool IsProcessing() const noexcept { return m_processing.load(std::memory_order_relaxed); }
    void ExpandRefresh(std::vector<CItem*>& items) const;
    // A removal (deletion, recycling) updates the previous volume results instead of reading the store again.
    void Invalidate(const std::vector<CItem*>& items, bool removal = false);
    void Clear();

    // Imported values are staged separately while the old scan is still visible.
    void SetLoaded(const CItem* item, std::optional<ULONGLONG> physical,
        std::optional<ULONGLONG> freed, std::optional<ULONGLONG> shared);
    void CommitLoaded();
    void DiscardLoaded();

    ULONGLONG GetPhysical(const CItem* item, ULONGLONG allocation) const noexcept;
    std::optional<ULONGLONG> GetFreed(const CItem* item) const;
    std::optional<ULONGLONG> GetShared(const CItem* item) const;
    bool HasData() const;
    bool HasSharingIndex(const CItem* item) const;
    bool IsDedupVolume(const CItem* item);
    static bool IsInSystemVolumeInformation(const CItem* item);
    SharingResult QuerySharing(const std::vector<CItem*>& items) const;
    void CollectRelated(const CItem* item, std::unordered_set<const CItem*>& related, std::size_t limit) const;
    std::vector<std::wstring> TakeFailures();

private:
    static constexpr ULONGLONG Unknown = ULLONG_MAX;
    static constexpr std::uint32_t None = UINT32_MAX;

    struct Values
    {
        LONGLONG delta = 0;
        ULONGLONG freed = Unknown;
        ULONGLONG shared = Unknown;
        std::uint32_t volume = None;
        std::uint32_t file = None;
    };

    struct Pseudo
    {
        CItem* parent;
        ULONGLONG bytes;
        DEDUPITEMKIND kind;
    };

    struct Volume
    {
        std::wstring path;
        std::vector<CItem*> roots;
        std::vector<std::pair<const CItem*, Values>> values;
        std::vector<Pseudo> pseudo;
        std::vector<CItem*> entries;                 // directory entries, grouped by physical file identity
        std::vector<std::uint32_t> entryOffsets;
        std::vector<ULONGLONG> fileBytes;
        std::vector<ULONGLONG> fileExclusive;
        std::vector<std::uint32_t> fileOffsets;       // physical file -> shared chunks
        std::vector<std::uint32_t> fileChunks;
        std::vector<std::uint32_t> chunkOffsets;      // shared chunk -> physical files
        std::vector<std::uint32_t> chunkFiles;
        std::vector<std::uint32_t> chunkBytes;
        std::vector<std::uint32_t> chunkOwner;        // local pseudo item, or None
        std::vector<bool> chunkExternal;

        // Physical files by identity: optimized files (indexed above) first, then ordinary hardlinks.
        std::vector<ULONGLONG> fileIds;
        std::vector<DWORD> fileLinks;
        std::vector<std::uint32_t> fileEntries;       // directory entries in the scan
        std::vector<double> fileEffective;
        CDedupStore::SpaceUsage space;
        ULONGLONG storeId = 0;
        DEDUPMETRIC metric = DEDUP_SHARED_ONCE;
        bool wholeVolume = false;
        bool withSpaceItems = false;
        bool replaceStore = false;
    };

    // Physical files and the chunks they share, without tree bindings; shared chunks may still fold into one file.
    struct Model
    {
        std::vector<ULONGLONG> ids;                   // optimized files first, then ordinary hardlinks
        std::vector<DWORD> links;
        std::vector<ULONGLONG> exclusive;             // one per optimized file
        std::vector<double> effective;
        std::vector<std::uint32_t> chunkOffsets{ 0 };
        std::vector<std::uint32_t> chunkFiles;
        std::vector<std::uint32_t> chunkBytes;
        std::vector<bool> chunkExternal;
        CDedupStore::SpaceUsage space;
        ULONGLONG storeId = 0;
        DEDUPMETRIC metric = DEDUP_SHARED_ONCE;
        bool wholeVolume = false;
        bool withSpaceItems = false;
        bool replaceStore = false;
    };

    struct Snapshot
    {
        std::vector<std::shared_ptr<const Volume>> volumes;
        std::unordered_map<const CItem*, Values> values;
        std::vector<std::vector<CItem*>> owners;
        std::vector<CItem*> pseudo;                  // derived tree rows, owned by their parents
    };

    static std::wstring GetVolumePath(const CItem* item);
    std::expected<std::shared_ptr<const Volume>, std::wstring> BuildVolume(const std::wstring& path,
        const std::vector<CItem*>& roots, const CDedupStore::CancelCheck& cancelled);
    std::optional<std::shared_ptr<const Volume>> UpdateVolume(const Volume& previous,
        const std::vector<CItem*>& roots, const CDedupStore::CancelCheck& cancelled);
    std::expected<std::shared_ptr<const Volume>, std::wstring> Project(const std::wstring& path,
        const std::vector<CItem*>& roots, const std::vector<CItem*>& files, const std::vector<CItem*>& entries,
        const std::vector<std::uint32_t>& entryOffsets, Model&& model, const CDedupStore::CancelCheck& cancelled);
    void Publish(std::vector<std::shared_ptr<const Volume>> volumes);
    void ClearProjection();

    mutable std::shared_mutex m_lock;
    std::unique_ptr<const Snapshot> m_snapshot;
    std::vector<std::shared_ptr<const Volume>> m_volumes;
    std::vector<std::shared_ptr<const Volume>> m_removed;  // results to update after a removal; tree bindings are stale
    std::unordered_map<const CItem*, Values> m_staged;
    std::unordered_map<const CItem*, Values> m_loaded;
    std::unordered_map<const CItem*, bool> m_storeProbes;
    std::vector<std::wstring> m_failures;
    std::atomic<bool> m_processing = false;
    bool m_imported = false;
};
