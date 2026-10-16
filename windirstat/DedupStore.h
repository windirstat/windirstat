// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"

// Reader for the Windows Data Deduplication chunk store of a single volume
// ("\System Volume Information\Dedup\ChunkStore\{GUID}.ddp"). Reading it
// requires SeBackupPrivilege. The on-disk format is undocumented; every header
// is validated against the layout observed on Windows Server 2016+ and loading
// fails as a whole on any mismatch instead of producing partial data.
//
// Load() parses the store once; Evaluate() then accounts for any set of live
// stream maps without reading the containers again, so the model can be kept
// while the store files stay unchanged (see TakeSnapshot()).
class CDedupStore final
{
public:

    // 16-byte digest identifying a stream map; stored both in the file's
    // reparse data and in the stream map record header.
    using Hash16 = std::array<std::uint8_t, 16>;

    struct Hash16Hasher
    {
        std::size_t operator()(const Hash16& hash) const noexcept
        {
            std::uint64_t low, high;
            std::memcpy(&low, hash.data(), sizeof(low));
            std::memcpy(&high, hash.data() + sizeof(low), sizeof(high));
            return static_cast<std::size_t>(low ^ (high * 0x9E3779B97F4A7C15ull));
        }
    };

    // Decoded reparse data of an optimized file
    struct ReparseInfo
    {
        ULONGLONG fileSize = 0;
        std::vector<Hash16> maps; // one stream map per 1 GB segment
    };

    // Space taken by the chunk store itself, split by purpose
    struct SpaceUsage
    {
        ULONGLONG storeBytes = 0;     // every file below the store directory
        ULONGLONG liveChunkBytes = 0; // chunk payload referenced by live stream maps
        ULONGLONG garbageBytes = 0;   // data records and stream maps no live file uses
        ULONGLONG regularGarbageBytes = 0; // part of it a regular garbage collection reclaims
        ULONGLONG overheadBytes = 0;  // headers, stream maps, indexes, logs, hotspot copies
    };

    // State of the store files; the delete logs are excluded from the signature
    // since Evaluate() reads them again each time
    struct Snapshot
    {
        std::uint64_t signature = 0;
        ULONGLONG storeBytes = 0;
        std::vector<std::wstring> streamDeleteLogs;
        std::vector<std::wstring> dataDeleteLogs;
    };

    using CancelCheck = std::function<bool()>;
    using ProgressCallback = std::function<void(double done)>; // 0 to 1
    using MapSet = std::unordered_set<Hash16, Hash16Hasher>;
    using FileMaps = std::unordered_map<ULONGLONG, std::vector<Hash16>>; // file reference -> stream maps

    // Lets the elements of a parallel loop poll a cancel check that takes a
    // lock, consulting it only every few hundred calls
    class CancelLatch final
    {
    public:
        explicit CancelLatch(const CancelCheck& check) : m_check(check) {}
        bool operator()()
        {
            if (m_cancelled.load(std::memory_order_relaxed)) return true;
            if (!m_check || (m_calls.fetch_add(1, std::memory_order_relaxed) & 0xFF) != 0) return false;
            if (m_check()) m_cancelled.store(true, std::memory_order_relaxed);
            return m_cancelled.load(std::memory_order_relaxed);
        }

    private:
        const CancelCheck& m_check;
        std::atomic<std::uint32_t> m_calls = 0;
        std::atomic<bool> m_cancelled = false;
    };

    // A parallel loop terminates the process on an exception, so running out
    // of memory in an element is rethrown on the calling thread instead
    template <typename Range, typename Function>
    static void ParallelForEach(Range& range, const Function& function)
    {
        std::atomic<bool> outOfMemory = false;
        std::for_each(std::execution::par, std::begin(range), std::end(range), [&](auto& element)
        {
            if (outOfMemory.load(std::memory_order_relaxed)) return;
            try
            {
                function(element);
            }
            catch (const std::bad_alloc&) { outOfMemory = true; }
            catch (const std::length_error&) { outOfMemory = true; }
        });
        if (outOfMemory) throw std::bad_alloc();
    }

    // Locates the chunk store directory ("...\{GUID}.ddp") of a volume root
    // such as "D:\" or "\\?\Volume{...}\"; empty when the volume has none.
    static std::wstring FindStore(const std::wstring& volumeRoot);

    // Total size of every file below a directory
    static ULONGLONG DirectorySize(const std::wstring& dir);

    // Reads and decodes the reparse data of an optimized file by path.
    static std::optional<ReparseInfo> ReadReparseData(const std::wstring& path);

    // Replaces files with the stream maps of every optimized file of an NTFS
    // volume, found through its reparse point index ($Extend\$Reparse)
    // independent of what was scanned. The chunk store only learns about
    // deleted files during garbage collection, so this is the authoritative
    // liveness source. With keepKnown, only files missing from files are read.
    // On failure files is left empty.
    static bool EnumerateVolumeFiles(const std::wstring& volumeRoot, FileMaps& files, bool keepKnown,
        const CancelCheck& cancelled = {}, const ProgressCallback& progress = {});

    static std::optional<Snapshot> TakeSnapshot(const std::wstring& storePath);

    // Parses every stream map record and the record index of every data
    // container, resolving each chunk reference to its data record.
    bool Load(const std::wstring& storePath, const CancelCheck& cancelled = {}, const ProgressCallback& progress = {});

    // Counts chunk references and splits the store size. With liveMaps, a map is
    // live only when an existing file references it; otherwise the store's own
    // delete logs and bitmaps decide. Reads only the delete logs of the snapshot.
    void Evaluate(const MapSet* liveMaps, const Snapshot& snapshot);

    // Distinct chunks of a live stream map; empty when the hash is unknown or dead
    std::span<const std::uint32_t> GetMapChunks(const Hash16& hash) const noexcept;

    std::uint32_t GetChunkRefs(const std::uint32_t chunk) const noexcept { return m_refs[chunk]; }   // live maps using it
    std::uint32_t GetChunkSize(const std::uint32_t chunk) const noexcept { return m_chunkSize[chunk]; } // compressed size
    const SpaceUsage& GetSpaceUsage() const noexcept { return m_space; }
    const std::wstring& GetError() const noexcept { return m_error; }

private:

    // A container (.ccc) with its record index (.cd) and delete bitmap
    struct Container
    {
        std::uint32_t generation = 0;
        ULONGLONG size = 0;
        std::wstring dataPath;
        std::wstring indexPath;
        std::wstring deleteMapPath;
    };

    // Parsed view of one stream map record inside a loaded container buffer
    struct RecordView
    {
        std::uint32_t size = 0; // header plus stream map body
        Hash16 hash{};
        std::span<const std::byte> entries; // 64-byte chunk entries
        std::size_t Count() const noexcept { return entries.size() / EntrySize; }
    };

    // A chunk reference decoded from a stream map entry
    struct EntryView
    {
        std::uint32_t chunkId;
        std::uint32_t container;
        std::uint32_t dataOffset;
        std::uint16_t generation;
        std::uint64_t digestKey;
        std::uint64_t diskSize;
    };

    // One stream map record; records sharing a hash are the same map
    struct MapRecord
    {
        Hash16 hash{};
        std::uint32_t map = 0;       // dense id of the hash
        std::uint32_t container = 0; // index into m_streams
        std::uint32_t size = 0;
        bool bitmapDeleted = false;
    };

    // Data records of one data container, which hold the chunks
    struct DataContainer
    {
        std::uint32_t id = 0;
        std::uint32_t generation = 0;  // high word of the name generation, as stream map entries record it
        std::uint32_t first = 0;       // global index of its first record
        std::uint32_t count = 0;
        bool idsKnown = false;
        std::vector<std::uint32_t> relocatedIds; // local ids of the records kept by compactions, ascending
        std::uint32_t tailBase = 0;              // local id of the first record written since
        std::vector<std::pair<std::uint32_t, std::uint32_t>> headerIds; // id -> record, read from the headers instead
    };

    static constexpr std::size_t EntrySize = 64;
    static constexpr std::size_t RecordHeaderSize = 0x68;
    static constexpr std::size_t DataRecordHeaderSize = 0x58;
    static constexpr std::uint32_t NoRecord = UINT32_MAX;

    // Highest occupancy at which a regular garbage collection still compacts
    // a container (DataMinContainerOccupancyPercent and
    // StreamMapMinContainerOccupancyPercent defaults of the dedup service)
    static constexpr std::size_t DataMinOccupancyPercent = 95;
    static constexpr std::size_t StreamMinOccupancyPercent = 80;

    static std::map<std::uint32_t, Container> ListContainers(const std::wstring& dir);
    static EntryView ReadEntry(const RecordView& record, std::size_t index) noexcept;
    static bool ReadFileBytes(const std::wstring& path, std::vector<std::byte>& buffer, ULONGLONG maxSize = 0);
    static bool ParseRecord(std::span<const std::byte> file, std::size_t offset, RecordView& record) noexcept;
    static bool ParseIndex(std::span<const std::byte> index, std::vector<std::uint32_t>& offsets);
    static std::vector<bool> ParseDeleteBitmap(std::span<const std::byte> bitmap, std::size_t records);
    // Fills the local ids of a data container; lastSpan receives the size of its last record when known
    static bool ReadRelocationTable(const Container& files, const std::vector<std::uint32_t>& offsets,
        DataContainer& container, std::uint32_t& lastSpan);
    static void ReadRecordIds(const Container& files, const std::vector<std::uint32_t>& offsets,
        DataContainer& container, std::uint32_t& lastSpan, const CancelCheck& cancelled);
    std::uint32_t FindRecord(const DataContainer& container, std::uint32_t localId) const noexcept;
    const DataContainer* FindDataContainer(std::uint32_t id) const noexcept;

    std::vector<Container> m_streams;        // current stream containers
    std::vector<MapRecord> m_records;        // in container and index order
    std::vector<std::size_t> m_recordChunks; // record -> range in m_chunkList
    std::vector<std::uint32_t> m_chunkList;  // distinct chunks of each record
    std::unordered_map<Hash16, std::uint32_t, Hash16Hasher> m_maps; // hash -> dense id
    std::vector<DataContainer> m_data;       // by container id
    std::vector<std::uint32_t> m_span;       // data record -> bytes it takes in its container
    std::vector<std::uint32_t> m_chunkSize;  // chunk -> compressed size; data records first, then unresolved chunks

    // Results of the last Evaluate()
    std::vector<std::uint32_t> m_mapRecord;  // map -> record counted for it, NoRecord when dead
    std::vector<std::uint32_t> m_refs;
    SpaceUsage m_space;

    std::wstring m_error;
};
