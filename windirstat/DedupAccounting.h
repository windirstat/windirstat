// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "DedupStore.h"

// Kind of an IT_DEDUP pseudo item, stored in its index
enum DEDUPITEMKIND : std::uint8_t
{
    DEDUPITEM_SHARED   = 0, // chunks shared by several files below the parent folder
    DEDUPITEM_EXTERNAL = 1, // chunks also used by files outside of the scan
    DEDUPITEM_GARBAGE  = 2, // chunk store data no existing file uses that a regular garbage collection frees
    DEDUPITEM_OVERHEAD = 3, // chunk store headers, stream maps, indexes and logs
    DEDUPITEM_GARBAGE_FULL = 4 // garbage only a full garbage collection frees
};

// Attributes the chunk store of each scanned volume to its optimized files after
// a scan. By default each stored byte is shown once: files keep their exclusive
// chunks, shared chunks go to a pseudo item in the lowest folder holding all
// their users, and store garbage and metadata to pseudo items at the volume root.
// All per-item data lives here, outside CItem.
class CDedupAccounting final
{
public:

    struct FileInfo
    {
        ULONGLONG onDisk = 0;    // compressed size of all distinct chunks
        ULONGLONG exclusive = 0; // freed if this file is deleted
        ULONGLONG applied = 0;   // bytes currently added to the physical size
    };

    struct FolderInfo
    {
        ULONGLONG owned = 0;     // chunks used by several files that all lie below this folder
        ULONGLONG shared = 0;    // chunks used below this folder and elsewhere
        ULONGLONG freed = 0;     // freed if deleted, as restored from a saved report
        bool loaded = false;
    };

    // A file sharing chunks with a queried selection
    struct SharingRow
    {
        CItem* item = nullptr;
        ULONGLONG sharedBytes = 0; // on-disk size of the overlapping chunks
        ULONGLONG itemBytes = 0;   // on-disk size of the listed file
    };

    struct SharingResult
    {
        bool success = false;
        std::wstring error;
        std::vector<SharingRow> rows;  // largest overlap first
        ULONGLONG queriedBytes = 0;    // on-disk size of the queried data
        ULONGLONG sharedBytes = 0;     // part of it also used by the listed files
        ULONGLONG outsideBytes = 0;    // part of it shared with files outside the scan
    };

    static CDedupAccounting& Get();

    // Runs on the scan thread after enumeration finished
    void RunPostScan(const std::vector<CItem*>& scanRoots, const CDedupStore::CancelCheck& cancelled);

    // UI thread, before the subtree of the volume root is changed
    void Revert(const CItem* volumeRoot);

    // Forgets everything without touching sizes (the tree is being destroyed)
    void Clear();

    // Drops the stores kept for later scans; scan thread, while the accounting is turned off
    void ReleaseCaches();

    // Drops the entries of an item that is being destroyed
    void Unregister(const CItem* item);

    // Values of a saved report are staged while it is read and committed once the old tree is gone
    void SetLoaded(const CItem* item, ULONGLONG freed, ULONGLONG shared);
    void CommitLoaded();
    void DiscardLoaded();

    // Loaded sizes include the deduplicated data, so such volumes are only rescanned as a whole
    bool HasLoadedData(const CItem* volumeRoot) const;
    std::vector<CItem*> GetLoadedVolumesBelow(const CItem* volumeRoot) const;

    // Freed if deleted (own clusters, exclusive chunks and chunks shared only below a
    // folder) and bytes shared with data outside the item; nullopt when unknown
    std::optional<ULONGLONG> GetFreed(const CItem* item) const;
    std::optional<ULONGLONG> GetShared(const CItem* item) const;

    // Folder totals for GetFreed, once sizes and hardlink flags are final
    void ComputeTotals(const CItem* root);

    bool HasData() const;
    bool HasSharingIndex(const CItem* item) const;

    // Compression would rehydrate optimized files; Microsoft does not support
    // compressing the folders or chunk store of a deduplicated volume
    bool BlocksCompression(const CItem* item);
    bool IsDedupVolume(const CItem* item);

    // Files sharing chunks with the given files, folders or shared pseudo items
    SharingResult QuerySharing(const std::vector<CItem*>& items) const;

    // Items to highlight together with a selected file or shared pseudo item
    void CollectRelated(const CItem* item, std::unordered_set<const CItem*>& related, std::size_t limit) const;

    // Volumes that could not be accounted, with the reason; cleared by the call
    std::vector<std::wstring> TakeFailures();

    // Progress of the running post-scan in permille, -1 when none runs
    int GetProgress() const noexcept { return m_progress.load(std::memory_order_relaxed); }

    // Bytes of chunk stores a filter keeps out of the scan, so the progress range
    // of their drives covers only what the scan can find. Scan thread, before the
    // drives are enumerated.
    void MeasureExcludedStores(const std::vector<CItem*>& scanRoots);
    ULONGLONG GetExcludedStoreBytes(const CItem* volumeRoot) const;

private:

    // Shared chunks of one volume as two compressed adjacency lists
    struct VolumeIndex
    {
        std::vector<CItem*> files;              // slot -> optimized file (nullptr once destroyed)
        std::vector<ULONGLONG> fileOnDisk;      // slot -> distinct on-disk bytes
        std::vector<ULONGLONG> fileExclusive;   // slot -> exclusive bytes
        std::vector<std::uint32_t> fileOffsets; // slot -> range in fileChunks
        std::vector<std::uint32_t> fileChunks;
        std::vector<std::uint32_t> chunkOffsets; // shared chunk -> range in chunkFiles
        std::vector<std::uint32_t> chunkFiles;
        std::vector<std::uint32_t> chunkBytes;
        std::vector<std::uint8_t> chunkExternal; // also used by files outside the scan
        std::vector<const CItem*> chunkOwner;    // pseudo item holding the chunk
    };

    // Parsed store and reparse data of one volume, kept across scans while the
    // store files stay unchanged, so a refresh rereads neither
    struct VolumeCache
    {
        std::mutex lock; // held while the volume is processed
        std::wstring storePath;
        std::uint64_t signature = 0;         // store snapshot the data below belongs to
        bool filesValid = false;
        CDedupStore::FileMaps files;
        std::unique_ptr<CDedupStore> store;
    };

    CDedupAccounting() = default;

    void ProcessVolume(CItem* volumeRoot, std::vector<CItem*>& files, const CDedupStore::CancelCheck& cancelled,
        const std::function<void(int permille)>& progress);
    static std::wstring GetVolumePath(const CItem* volumeRoot);
    static CItem* AddPseudoItem(CItem* parent, DEDUPITEMKIND kind, ULONGLONG bytes);
    void AddFailure(const std::wstring& volume, const std::wstring& reason);
    ULONGLONG GetFileFreedLocked(const CItem* file, bool checkChunkStore = true) const;

    mutable std::shared_mutex m_lock;
    std::unordered_map<const CItem*, FileInfo> m_files;
    std::unordered_map<const CItem*, FolderInfo> m_folders;
    std::unordered_map<const CItem*, ULONGLONG> m_pseudo;       // pseudo item -> bytes
    std::unordered_map<const CItem*, ULONGLONG> m_neutralized;  // chunk store file -> removed bytes
    std::unordered_map<const CItem*, VolumeIndex> m_indexes;    // volume root -> shared chunk index
    std::unordered_map<const CItem*, std::pair<const CItem*, std::uint32_t>> m_slots; // file -> (volume root, slot)
    std::vector<std::wstring> m_failures;
    std::vector<std::tuple<const CItem*, ULONGLONG, ULONGLONG>> m_staged; // loaded item, freed, shared
    std::unordered_set<const CItem*> m_loadedVolumes; // volume roots holding values of a loaded report
    std::unordered_map<const CItem*, ULONGLONG> m_folderTotals; // folder -> freed if deleted
    std::unordered_set<const CItem*> m_chunkStores;         // chunk store folders in the tree
    std::unordered_set<std::wstring> m_dedupVolumes;        // volume paths holding optimized files
    std::unordered_map<std::wstring, bool> m_storeProbes;   // volume path -> chunk store found
    std::unordered_map<std::wstring, ULONGLONG> m_excludedStores; // volume path -> filtered out store bytes
    std::atomic<int> m_progress = -1;

    // Survives Clear(); the nodes stay in place, so entries are used without m_cacheLock
    std::mutex m_cacheLock;
    std::unordered_map<std::wstring, VolumeCache> m_caches; // volume path -> cache
};
