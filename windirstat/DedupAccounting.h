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
    void Invalidate(const std::vector<CItem*>& items);
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
    void Publish(std::vector<std::shared_ptr<const Volume>> volumes);
    void ClearProjection();

    mutable std::shared_mutex m_lock;
    std::unique_ptr<const Snapshot> m_snapshot;
    std::vector<std::shared_ptr<const Volume>> m_volumes;
    std::unordered_map<const CItem*, Values> m_staged;
    std::unordered_map<const CItem*, Values> m_loaded;
    std::unordered_map<const CItem*, bool> m_storeProbes;
    std::vector<std::wstring> m_failures;
    std::atomic<bool> m_processing = false;
    bool m_imported = false;
};
