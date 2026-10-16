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

    // Per-chunk accounting shared by every stream map referencing the chunk
    struct ChunkInfo
    {
        std::uint32_t refs = 0;     // live stream maps referencing this chunk
        std::uint32_t diskSize = 0; // compressed size stored in the data container
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

    // Called by Resolve() once per distinct chunk of every requested stream map
    using ChunkVisitor = std::function<void(std::size_t requestIndex, std::uint32_t chunkIndex, const ChunkInfo& chunk)>;
    using CancelCheck = std::function<bool()>;

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

    // Locates the chunk store directory ("...\{GUID}.ddp") of a volume root
    // such as "D:\" or "\\?\Volume{...}\"; empty when the volume has none.
    static std::wstring FindStore(const std::wstring& volumeRoot);

    // Reads and decodes the reparse data of an optimized file by path.
    static std::optional<ReparseInfo> ReadReparseData(const std::wstring& path);

    using MapSet = std::unordered_set<Hash16, Hash16Hasher>;

    // Collects the stream maps referenced by every optimized file of an NTFS
    // volume through the reparse point index ($Extend\$Reparse), independent
    // of what was scanned. The chunk store only learns about deleted files
    // during garbage collection, so this is the authoritative liveness source.
    static std::optional<MapSet> EnumerateVolumeMaps(const std::wstring& volumeRoot, const CancelCheck& cancelled = {});

    // Parses every live stream map of the store and counts chunk references.
    // With liveMaps, a map is live only when an existing file references it;
    // otherwise the store's own delete logs and bitmaps decide.
    bool Load(const std::wstring& storePath, const MapSet* liveMaps, const CancelCheck& cancelled = {});

    // Streams the requested stream maps again and reports their distinct
    // chunks. Requests whose hash is unknown or dead are skipped; false when
    // the store could not be read consistently (see GetError()).
    bool Resolve(std::span<const Hash16> requests, const ChunkVisitor& visitor, const CancelCheck& cancelled = {});

    // Splits the store size into live data, garbage and overhead. Reads the
    // data container indexes and a few record headers; call after Load().
    bool ComputeSpaceUsage(const CancelCheck& cancelled = {});

    const std::wstring& GetError() const noexcept { return m_error; }
    const ChunkInfo& GetChunk(const std::uint32_t index) const noexcept { return m_chunks[index]; }
    const SpaceUsage& GetSpaceUsage() const noexcept { return m_space; }

private:

    // A container (.ccc) with its record index (.cd) and delete bitmap
    struct Container
    {
        std::uint32_t generation = 0;
        std::wstring dataPath;
        std::wstring indexPath;
        std::wstring deleteMapPath;
    };

    // Where a live stream map record lives
    struct MapLocation
    {
        std::uint32_t container = 0; // index into m_containers
        std::uint32_t offset = 0;    // byte offset of the 'Ckhr' header
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

    static constexpr std::size_t EntrySize = 64;
    static constexpr std::size_t RecordHeaderSize = 0x68;
    static constexpr std::size_t DataRecordHeaderSize = 0x58;

    // Highest occupancy at which a regular garbage collection still compacts
    // a container (DataMinContainerOccupancyPercent and
    // StreamMapMinContainerOccupancyPercent defaults of the dedup service)
    static constexpr std::size_t DataMinOccupancyPercent = 95;
    static constexpr std::size_t StreamMinOccupancyPercent = 80;

    // Called for every stream map record, live or not
    using MapCallback = std::function<void(std::uint32_t containerIndex, std::uint32_t offset, const RecordView& record, bool live)>;

    static std::map<std::uint32_t, Container> ListContainers(const std::wstring& dir, std::vector<std::wstring>& deleteLogs);
    void LoadDeleteLogs(const std::vector<std::wstring>& logs);
    bool ForEachMap(const CancelCheck& cancelled, const MapCallback& callback);
    static EntryView ReadEntry(const RecordView& record, std::size_t index) noexcept;
    static bool ReadFileBytes(const std::wstring& path, std::vector<std::byte>& buffer, ULONGLONG maxSize = 0);
    static bool ParseRecord(std::span<const std::byte> file, std::size_t offset, RecordView& record) noexcept;
    static bool ParseIndex(std::span<const std::byte> index, std::vector<std::uint32_t>& offsets);
    static std::vector<bool> ParseDeleteBitmap(std::span<const std::byte> bitmap, std::size_t records);
    std::optional<std::uint32_t> FindChunkIndex(std::uint64_t digestKey) const noexcept;

    std::vector<Container> m_containers;    // current stream containers
    std::vector<std::uint64_t> m_chunkKeys; // sorted digest prefixes (compaction moves chunks), parallel to m_chunks
    std::vector<ChunkInfo> m_chunks;
    std::unordered_map<Hash16, MapLocation, Hash16Hasher> m_maps;
    MapSet m_deadHashes;
    const MapSet* m_liveMaps = nullptr;
    SpaceUsage m_space;
    std::wstring m_path;
    std::wstring m_error;
    bool m_loaded = false;
};
