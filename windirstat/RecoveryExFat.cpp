// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "RecoveryExFat.h"

// exFAT boot-sector and directory-entry layouts, including their reserved byte ranges.
// https://learn.microsoft.com/windows/win32/fileio/exfat-specification
#pragma pack(push, 1)
struct ExFatBootSector
{
    BYTE jump[3];
    char fileSystemName[8];
    BYTE mustBeZero[53];
    ULONGLONG partitionOffset;
    ULONGLONG volumeLength;
    DWORD fatOffset;
    DWORD fatLength;
    DWORD clusterHeapOffset;
    DWORD clusterCount;
    DWORD rootCluster;
    DWORD serialNumber;
    WORD revision;
    WORD volumeFlags;
    BYTE sectorShift;
    BYTE clusterShift;
    BYTE fatCount;
    BYTE driveSelect;
    BYTE percentInUse;
    BYTE reserved[7];
    BYTE bootCode[390];
    WORD signature;
};

struct ExFatFileEntry
{
    BYTE type;
    BYTE secondaryCount;
    WORD checksum;
    WORD attributes;
    WORD reserved1;
    DWORD created;
    DWORD modified;
    DWORD accessed;
    BYTE createdIncrement;
    BYTE modifiedIncrement;
    BYTE createdUtcOffset;
    BYTE modifiedUtcOffset;
    BYTE accessedUtcOffset;
    BYTE reserved2[7];
};

struct ExFatStreamEntry
{
    BYTE type;
    BYTE flags;
    BYTE reserved1;
    BYTE nameLength;
    WORD nameHash;
    WORD reserved2;
    ULONGLONG initializedSize;
    DWORD reserved3;
    DWORD firstCluster;
    ULONGLONG size;
};

struct ExFatNameEntry
{
    BYTE type;
    BYTE flags;
    wchar_t name[15];
};

struct ExFatBitmapEntry
{
    BYTE type;
    BYTE flags;
    BYTE reserved[18];
    DWORD firstCluster;
    ULONGLONG size;
};
#pragma pack(pop)

static_assert(sizeof(ExFatBootSector) == 512 && offsetof(ExFatBootSector, signature) == 510);
static_assert(offsetof(ExFatBootSector, volumeFlags) == 106 && offsetof(ExFatBootSector, sectorShift) == 108);
static_assert(sizeof(ExFatFileEntry) == 32 && sizeof(ExFatStreamEntry) == 32);
static_assert(sizeof(ExFatNameEntry) == 32 && sizeof(ExFatBitmapEntry) == 32);
static_assert(offsetof(ExFatStreamEntry, firstCluster) == 20 && offsetof(ExFatNameEntry, name) == 2);
constexpr std::array<BYTE, 3> ExFatBootJump{ 0xEB, 0x76, 0x90 };
constexpr WORD ExFatBootSignature = 0xAA55;
constexpr DWORD ExFatExtendedBootSignature = 0xAA550000;
constexpr WORD ExFatRevision = 0x0100; // Version 1.0.
constexpr WORD ExFatActiveFat = 0x0001;
constexpr DWORD ExFatMinSectorShift = 9;
constexpr DWORD ExFatMaxSectorShift = 12;
constexpr DWORD ExFatMaxClusterShift = 25; // Cluster sizes up to 32 MiB.
constexpr DWORD ExFatBootRegionSectors = 12;
constexpr DWORD ExFatChecksumSector = ExFatBootRegionSectors - 1;
constexpr DWORD ExFatExtendedBootSectors = 8;
constexpr ULONGLONG ExFatMinVolumeBytes = 1024 * 1024;
constexpr DWORD ExFatMaxClusterCount = 0xFFFFFFF5;
constexpr DWORD ExFatFirstDataCluster = 2;
constexpr DWORD ExFatEndOfChain = 0xFFFFFFFF;
constexpr BYTE ExFatUnknownUsage = 0xFF;
constexpr BYTE ExFatInUse = 0x80;
constexpr BYTE ExFatSecondary = 0x40;
constexpr BYTE ExFatBenign = 0x20;
constexpr BYTE ExFatFileType = 0x05;
constexpr BYTE ExFatStreamType = 0x40;
constexpr BYTE ExFatNameType = 0x41;
constexpr BYTE ExFatBitmapType = 0x81;
constexpr BYTE ExFatAllocationPossible = 0x01;
constexpr BYTE ExFatNoFatChain = 0x02;
constexpr BYTE ExFatContiguousFlags = ExFatAllocationPossible | ExFatNoFatChain;
constexpr WORD ExFatValidAttributes = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN |
    FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_ARCHIVE;
constexpr BYTE ExFatUtcOffsetValid = 0x80;
constexpr BYTE ExFatUtcOffsetSign = 0x40;
constexpr BYTE ExFatUtcOffsetMask = 0x7F;
constexpr size_t ExFatNameCharacters = std::size(ExFatNameEntry{}.name);

constexpr bool IsExFatMutableBootByte(const size_t offset)
{
    // Identify volume fields that Windows may update without changing the boot checksum.
    return (offset >= offsetof(ExFatBootSector, volumeFlags) &&
        offset < offsetof(ExFatBootSector, volumeFlags) + sizeof(WORD)) ||
        offset == offsetof(ExFatBootSector, percentInUse);
}

bool ExFatRecovery::ParseBootRegion(const std::span<const BYTE> bytes,
    const ULONGLONG deviceLength, Geometry& geometry)
{
    geometry = {};
    if (bytes.size() < sizeof(ExFatBootSector)) return false;
    const auto boot = Read<ExFatBootSector>(bytes, 0);
    if (!std::ranges::equal(boot.jump, ExFatBootJump) ||
        std::memcmp(boot.fileSystemName, "EXFAT   ", sizeof(boot.fileSystemName)) != 0 ||
        boot.sectorShift < ExFatMinSectorShift || boot.sectorShift > ExFatMaxSectorShift ||
        boot.clusterShift > ExFatMaxClusterShift - boot.sectorShift || boot.signature != ExFatBootSignature ||
        boot.revision != ExFatRevision || boot.fatCount != 1 || (boot.volumeFlags & ExFatActiveFat) != 0 ||
        (boot.percentInUse > 100 && boot.percentInUse != ExFatUnknownUsage) ||
        std::ranges::any_of(boot.mustBeZero, [](const BYTE value)
    {
        // Reject nonzero bytes in the boot sector's required zero-filled area.
        return value != 0;
    })) return false;
    const DWORD sector = 1u << boot.sectorShift, cluster = sector << boot.clusterShift;
    if (bytes.size() < sector * ExFatBootRegionSectors) return false;

    // Mutable volume flags and usage percentage are excluded from the boot checksum.
    DWORD checksum = 0;
    for (size_t i = 0; i < sector * ExFatChecksumSector; ++i)
        if (!IsExFatMutableBootByte(i)) checksum = std::rotr(checksum, 1) + bytes[i];
    for (size_t i = sector * ExFatChecksumSector; i < sector * ExFatBootRegionSectors; i += sizeof(DWORD))
        if (Read<DWORD>(bytes, i) != checksum) return false;
    for (size_t i = 1; i <= ExFatExtendedBootSectors; ++i)
        if (Read<DWORD>(bytes, (i + 1) * sector - sizeof(DWORD)) != ExFatExtendedBootSignature) return false;

    // Cross-check the FAT and cluster heap against the reported device length.
    const auto sectors = boot.volumeLength;
    const auto fat = boot.fatOffset, fatLength = boot.fatLength;
    const auto heap = boot.clusterHeapOffset, count = boot.clusterCount;
    const auto root = boot.rootCluster;
    if (deviceLength > LLONG_MAX || sectors < ExFatMinVolumeBytes / sector || sectors > deviceLength / sector ||
        fat < 2 * ExFatBootRegionSectors || fatLength == 0 || ULONGLONG(fat) + fatLength > heap || heap >= sectors ||
        count == 0 || count > ExFatMaxClusterCount || root < ExFatFirstDataCluster || root > ULONGLONG(count) + 1 ||
        count != std::min<ULONGLONG>((sectors - heap) >> boot.clusterShift, ExFatMaxClusterCount) ||
        ULONGLONG(fatLength) * sector < (ULONGLONG(count) + ExFatFirstDataCluster) * sizeof(DWORD)) return false;
    geometry = { sector, cluster, count, root, sectors * sector, ULONGLONG(fat) * sector,
        ULONGLONG(heap) * sector };
    return true;
}

FILETIME ExFatRecovery::Timestamp(const DWORD packed, const BYTE increment, const BYTE offset)
{
    FILETIME local = {}, utc = {};
    if (increment > MaxFatTimestampIncrement || !DosDateTimeToFileTime(static_cast<WORD>(packed >> 16),
        static_cast<WORD>(packed), &local)) return {};
    ULARGE_INTEGER time{ .LowPart = local.dwLowDateTime, .HighPart = local.dwHighDateTime };
    time.QuadPart += ULONGLONG(increment) * FileTimeTicksPer10Milliseconds;
    if ((offset & ExFatUtcOffsetValid) == 0)
    {
        local = { time.LowPart, time.HighPart };
        return LocalFileTimeToFileTime(&local, &utc) ? utc : FILETIME{};
    }

    // Stored UTC offsets use signed quarter-hour units.
    const int quarters = (offset & ExFatUtcOffsetSign) ?
        int(offset & ExFatUtcOffsetMask) - (ExFatUtcOffsetMask + 1) : offset & ExFatUtcOffsetMask;
    time.QuadPart -= LONGLONG(quarters) * 15 * 60 * FileTimeTicksPerSecond;
    return { time.LowPart, time.HighPart };
}

bool ExFatRecovery::ParseEntrySet(const std::span<const BYTE> bytes, const Geometry& geometry, Record& record)
{
    record = {};
    if (bytes.size() < MinEntrySetSize || bytes.size() > MaxEntrySetSize ||
        geometry.clusterSize == 0 || geometry.clusterCount == 0) return false;
    const auto file = Read<ExFatFileEntry>(bytes, 0);
    const auto stream = Read<ExFatStreamEntry>(bytes, EntryBytes);
    if ((file.type & ~ExFatInUse) != ExFatFileType ||
        bytes.size() != (size_t(file.secondaryCount) + 1) * EntryBytes) return false;
    const BYTE inUse = file.type & ExFatInUse;
    const BYTE nameLength = stream.nameLength, flags = stream.flags;
    const size_t nameEnd = 2 * EntryBytes +
        ((nameLength + ExFatNameCharacters - 1) / ExFatNameCharacters) * EntryBytes;
    if (stream.type != (ExFatStreamType | inUse) ||
        (flags != ExFatAllocationPossible && flags != ExFatContiguousFlags) || nameLength == 0 ||
        bytes.size() < nameEnd || (file.attributes & ~ExFatValidAttributes) != 0) return false;

    // Restore deleted entries' InUse bits when checking their original set checksum.
    WORD checksum = 0;
    for (size_t i = 0; i < bytes.size(); ++i)
        if (i < offsetof(ExFatFileEntry, checksum) || i >= offsetof(ExFatFileEntry, checksum) + sizeof(WORD))
            checksum = static_cast<WORD>(std::rotr(checksum, 1) +
                (i % EntryBytes == 0 ? bytes[i] | ExFatInUse : bytes[i]));
    if (checksum != file.checksum) return false;
    for (size_t i = 2 * EntryBytes; i < bytes.size(); i += EntryBytes)
        if (i < nameEnd ? bytes[i] != (ExFatNameType | inUse) :
            (bytes[i] & (ExFatInUse | ExFatSecondary | ExFatBenign)) != (ExFatSecondary | ExFatBenign | inUse))
            return false;

    // Filename fragments hold up to fifteen UTF-16 code units per entry.
    for (size_t i = 0; i < nameLength; ++i)
    {
        const wchar_t c = Read<wchar_t>(bytes, 2 * EntryBytes + offsetof(ExFatNameEntry, name) +
            (i / ExFatNameCharacters) * EntryBytes + (i % ExFatNameCharacters) * sizeof(wchar_t));
        if (c < FirstPrintableCharacter || std::wstring_view(L"<>:\"/\\|?*").contains(c)) return false;
        record.name += c;
    }
    if (record.name == L"." || record.name == L"..") return false;

    // Reject unpaired surrogates before exposing the filename to the UI or filesystem.
    for (size_t i = 0; i < record.name.size(); ++i)
    {
        const wchar_t c = record.name[i];
        if (IS_LOW_SURROGATE(c)) return false;
        if (!IS_HIGH_SURROGATE(c)) continue;
        if (++i == record.name.size() || !IS_LOW_SURROGATE(record.name[i])) return false;
    }
    record.inUse = inUse != 0;
    record.directory = (file.attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    record.sequence = checksum;
    record.created = Timestamp(file.created, file.createdIncrement, file.createdUtcOffset);
    record.modified = Timestamp(file.modified, file.modifiedIncrement, file.modifiedUtcOffset);
    record.data.size = stream.size;
    record.data.initialized = stream.initializedSize;
    const DWORD first = stream.firstCluster;
    const ULONGLONG count = (record.data.size / geometry.clusterSize) +
        (record.data.size % geometry.clusterSize != 0);

    // Check stream bounds and require directory data to occupy complete initialized clusters.
    if (record.data.size > LLONG_MAX || count > geometry.clusterCount ||
        record.data.initialized > record.data.size ||
        (count == 0 && (first != 0 || flags != ExFatAllocationPossible)) ||
        (count != 0 && (first < ExFatFirstDataCluster || first > ULONGLONG(geometry.clusterCount) + 1)) ||
        (flags == ExFatContiguousFlags && count > ULONGLONG(geometry.clusterCount) + ExFatFirstDataCluster - first) ||
        (record.directory && (record.data.size == 0 ||
            record.data.size % geometry.clusterSize != 0 || record.data.initialized != record.data.size)))
        return false;

    // Only NoFatChain extents survive deletion unambiguously; live directories can still use the FAT.
    record.data.nonresident = record.data.size != 0;
    record.supported = record.data.supported = record.inUse ||
        (bytes.size() == nameEnd && (count == 0 || flags == ExFatContiguousFlags));
    if (count != 0 && flags == ExFatContiguousFlags)
        record.data.runs.push_back({ 0, LONGLONG(first) - ExFatFirstDataCluster, count });
    record.snapshot.assign(bytes.begin(), bytes.end());
    record.condition = count == 0 ? Condition::Resident : Condition::Unallocated;
    return true;
}

ExFatRecovery::ExFatRecovery(const std::wstring& volumeName, Progress* const progress) : RecoveryShared(volumeName)
{
    // Read the device size before validating the boot region and allocation metadata.
    GET_LENGTH_INFORMATION length = {};
    DWORD returned = 0;
    if (!DeviceIoControl(m_volume, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &length, sizeof(length),
        &returned, nullptr)) throw Failure{ {}, GetLastError() };
    if (returned < sizeof(length) || length.Length.QuadPart < ExFatMinVolumeBytes)
        throw Failure{ {}, ERROR_INVALID_DATA };
    m_length = static_cast<ULONGLONG>(length.Length.QuadPart);
    Initialize(progress);
}

void ExFatRecovery::ReadAt(const ULONGLONG offset, const std::span<BYTE> bytes)
{
    // Bound each raw read before applying the volume's sector alignment.
    if (bytes.empty() || bytes.size() > CopyBufferBytes || offset > m_length || bytes.size() > m_length - offset)
        throw Failure{ {}, ERROR_INVALID_DATA };
    ReadVolume(offset, bytes, m_length, m_alignment);
}

ULONGLONG ExFatRecovery::ClusterOffset(const DWORD cluster) const
{
    // Translate exFAT cluster numbers into byte offsets in the cluster heap.
    if (cluster < ExFatFirstDataCluster || cluster > ULONGLONG(m_geometry.clusterCount) + 1)
        throw Failure{ {}, ERROR_INVALID_DATA };
    return m_geometry.heapOffset + ULONGLONG(cluster - ExFatFirstDataCluster) * m_geometry.clusterSize;
}

DWORD ExFatRecovery::NextCluster(const DWORD cluster)
{
    // Read a FAT link through the cached page containing that cluster's entry.
    ClusterOffset(cluster);
    const ULONGLONG offset = ULONGLONG(cluster) * sizeof(DWORD);
    const ULONGLONG page = offset & ~ULONGLONG(CachePageBytes - 1);
    if (page != m_fatPageOffset)
    {
        m_fatPage.resize(static_cast<size_t>(std::min<ULONGLONG>(CachePageBytes,
            (ULONGLONG(m_geometry.clusterCount) + ExFatFirstDataCluster) * sizeof(DWORD) - page)));
        ReadAt(m_geometry.fatOffset + page, m_fatPage);
        m_fatPageOffset = page;
    }
    return Read<DWORD>(m_fatPage, static_cast<size_t>(offset - page));
}

std::vector<DWORD> ExFatRecovery::Chain(const DWORD first, const ULONGLONG length, const bool contiguous,
    Progress& progress)
{
    // Build a bounded cluster list from a contiguous extent or the FAT chain.
    ClusterOffset(first);
    const ULONGLONG expected = length / m_geometry.clusterSize + (length % m_geometry.clusterSize != 0);
    const ULONGLONG maximum = length == 0 ? m_geometry.clusterCount : expected;
    if (maximum == 0 || maximum > m_geometry.clusterCount ||
        (contiguous && maximum > ULONGLONG(m_geometry.clusterCount) + ExFatFirstDataCluster - first))
        throw Failure{ {}, ERROR_INVALID_DATA };
    std::vector<DWORD> clusters;
    std::unordered_set<DWORD> seen;
    DWORD cluster = first;
    for (ULONGLONG i = 0; i < maximum; ++i)
    {
        progress.Check();
        ClusterOffset(cluster);
        if (!contiguous && !seen.insert(cluster).second) throw Failure{ {}, ERROR_INVALID_DATA };
        clusters.push_back(cluster);
        if (contiguous) { ++cluster; continue; }
        cluster = NextCluster(cluster);
        if (cluster == ExFatEndOfChain)
        {
            if (expected != 0 && clusters.size() != expected) throw Failure{ {}, ERROR_INVALID_DATA };
            return clusters;
        }
    }
    if (!contiguous) throw Failure{ {}, ERROR_INVALID_DATA };
    return clusters;
}

void ExFatRecovery::VisitEntries(const std::vector<DWORD>& clusters, Progress& progress,
    const std::function<bool(std::span<const BYTE>, ULONGLONG)>& visit)
{
    // Visit directory entries until their end marker or the callback stops enumeration.
    std::vector<BYTE> bytes(std::min<DWORD>(CachePageBytes, m_geometry.clusterSize));
    for (const DWORD cluster : clusters)
    {
        const ULONGLONG physical = ClusterOffset(cluster);
        for (DWORD offset = 0; offset < m_geometry.clusterSize; offset += static_cast<DWORD>(bytes.size()))
        {
            progress.Check();
            ReadAt(physical + offset, bytes);
            for (size_t i = 0; i < bytes.size(); i += EntryBytes)
                if (bytes[i] == 0 || !visit(std::span<const BYTE>(bytes).subspan(i, EntryBytes),
                    physical + offset + i)) return;
        }
    }
}

void ExFatRecovery::Initialize(Progress* const suppliedProgress)
{
    Progress fallback;
    Progress& progress = suppliedProgress ? *suppliedProgress : fallback;
    progress.Check();
    std::array<BYTE, InitialSectorAlignment> first{};
    ReadAt(0, first);
    const auto boot = Read<ExFatBootSector>(first, 0);
    if (boot.sectorShift < ExFatMinSectorShift || boot.sectorShift > ExFatMaxSectorShift)
        throw Failure{ {}, ERROR_INVALID_DATA };

    // Read the full boot region after its sector size is known.
    m_boot.resize((1u << boot.sectorShift) * ExFatBootRegionSectors);
    progress.Check();
    ReadAt(0, m_boot);
    if (!ParseBootRegion(m_boot, m_length, m_geometry)) throw Failure{ {}, ERROR_INVALID_DATA };
    m_alignment = m_geometry.sectorSize;

    // Locate the allocation bitmap through the root directory and verify its own allocation.
    m_rootClusters = Chain(m_geometry.rootCluster, 0, false, progress);
    VisitEntries(m_rootClusters, progress, [&](const std::span<const BYTE> entry, const ULONGLONG offset)
    {
        // Capture the active allocation bitmap's location and cluster chain.
        const auto bitmap = Read<ExFatBitmapEntry>(entry, 0);
        if (bitmap.type != ExFatBitmapType) return true;
        if (!m_bitmapEntry.empty() || bitmap.flags != 0) throw Failure{ {}, ERROR_INVALID_DATA };
        m_bitmapEntry.assign(entry.begin(), entry.end());
        m_bitmapEntryOffset = offset;
        m_bitmapSize = bitmap.size;
        if (m_bitmapSize < (ULONGLONG(m_geometry.clusterCount) + 7) / 8 ||
            m_bitmapSize > ULONGLONG(m_geometry.clusterCount) * m_geometry.clusterSize)
            throw Failure{ {}, ERROR_INVALID_DATA };
        m_bitmapClusters = Chain(bitmap.firstCluster, m_bitmapSize, false, progress);
        return true;
    });
    if (m_bitmapEntry.empty()) throw Failure{ {}, ERROR_INVALID_DATA };
    for (const DWORD cluster : m_bitmapClusters)
        if (!ClustersHaveState(cluster, 1, true, progress)) throw Failure{ {}, ERROR_INVALID_DATA };
}

// The bitmap is itself a FAT-mapped file, so a page can span non-adjacent clusters.
void ExFatRecovery::ReadBitmap(ULONGLONG offset, const std::span<BYTE> bytes)
{
    // Read bitmap bytes across its physical cluster chain in bounded chunks.
    if (offset > m_bitmapSize || bytes.size() > m_bitmapSize - offset) throw Failure{ {}, ERROR_INVALID_DATA };
    size_t copied = 0;
    while (copied < bytes.size())
    {
        const auto index = static_cast<size_t>(offset / m_geometry.clusterSize);
        const auto within = static_cast<DWORD>(offset % m_geometry.clusterSize);
        if (index >= m_bitmapClusters.size()) throw Failure{ {}, ERROR_INVALID_DATA };
        const size_t limit = std::min<size_t>(CopyBufferBytes, bytes.size() - copied);
        size_t take = std::min<size_t>(limit, m_geometry.clusterSize - within);
        for (size_t next = index + 1; take < limit && next < m_bitmapClusters.size() &&
            m_bitmapClusters[next] == m_bitmapClusters[next - 1] + 1; ++next)
            take += std::min<size_t>(limit - take, m_geometry.clusterSize);
        ReadAt(ClusterOffset(m_bitmapClusters[index]) + within, bytes.subspan(copied, take));
        offset += take;
        copied += take;
    }
}

bool ExFatRecovery::ClustersHaveState(const DWORD first, ULONGLONG count, const bool allocated,
    Progress& progress, const bool fresh)
{
    // Check allocation bits, refreshing the bitmap cache when fresh data is required.
    ClusterOffset(first);
    if (count > ULONGLONG(m_geometry.clusterCount) + ExFatFirstDataCluster - first)
        throw Failure{ {}, ERROR_INVALID_DATA };
    if (fresh) m_bitmapPageOffset = ULLONG_MAX;
    ULONGLONG bit = first - ExFatFirstDataCluster;
    while (count != 0)
    {
        progress.Check();
        const ULONGLONG offset = bit / 8;
        if (m_bitmapPageOffset == ULLONG_MAX || offset < m_bitmapPageOffset ||
            offset - m_bitmapPageOffset >= m_bitmapPage.size())
        {
            const ULONGLONG page = fresh ? offset : offset & ~ULONGLONG(CachePageBytes - 1);
            const ULONGLONG length = fresh ? (bit % 8 + count + 7) / 8 : m_bitmapSize - page;
            m_bitmapPage.resize(static_cast<size_t>(std::min<ULONGLONG>(CachePageBytes, length)));
            ReadBitmap(page, m_bitmapPage);
            m_bitmapPageOffset = page;
        }
        const ULONGLONG within = bit - m_bitmapPageOffset * 8;
        if (within % 8 == 0 && count >= 8)
        {
            const auto take = static_cast<size_t>(std::min<ULONGLONG>(count / 8,
                m_bitmapPage.size() - within / 8));
            const auto full = std::span<const BYTE>(m_bitmapPage).subspan(static_cast<size_t>(within / 8), take);
            if (std::ranges::any_of(full, [allocated](const BYTE byte)
            {
                // Compare all eight allocation bits with the requested cluster state.
                return byte != (allocated ? UCHAR_MAX : 0);
            }))
                return false;
            bit += take * 8;
            count -= take * 8;
            continue;
        }
        if ((m_bitmapPage[static_cast<size_t>(within / 8)] & (1 << (within % 8))) !=
            (allocated ? 1 << (within % 8) : 0)) return false;
        ++bit;
        --count;
    }
    return true;
}

void ExFatRecovery::Scan(Progress& progress, ScanResult& result,
    const std::function<void(const Record&)>& discovered)
{
    result = {};
    progress.Check();
    m_fatPageOffset = m_bitmapPageOffset = ULLONG_MAX;
    struct Directory { DWORD first; ULONGLONG length; bool contiguous; std::wstring path; };

    // Traverse live directories while detecting reused or cyclic cluster chains.
    std::vector<Directory> pending{ { m_geometry.rootCluster, 0, false, L"\\" } };
    std::unordered_set<DWORD> visited(m_bitmapClusters.begin(), m_bitmapClusters.end());
    while (!pending.empty())
    {
        progress.Check();
        Directory directory = std::move(pending.back());
        pending.pop_back();
        std::vector<DWORD> clusters;
        try
        {
            clusters = Chain(directory.first, directory.length, directory.contiguous, progress);
            for (const DWORD cluster : clusters)
                if (visited.contains(cluster) || !ClustersHaveState(cluster, 1, true, progress))
                    throw Failure{ {}, ERROR_INVALID_DATA };
        }
        catch (const Failure& failure)
        {
            // Skip malformed directory chains, but propagate I/O failures and cancellation.
            if (failure.error != ERROR_INVALID_DATA) throw;
            ++result.invalidRecords;
            continue;
        }
        visited.insert(clusters.begin(), clusters.end());
        std::vector<BYTE> entries;
        std::vector<ULONGLONG> offsets;
        size_t expected = 0;
        VisitEntries(clusters, progress, [&](const std::span<const BYTE> entry, const ULONGLONG offset)
        {
            if (!entries.empty() && (entry[0] & ExFatSecondary) == 0)
            {
                entries.clear();
                offsets.clear();
                ++result.invalidRecords;
            }
            if (entries.empty())
            {
                if ((entry[0] & ~ExFatInUse) != ExFatFileType) return true;
                expected = (size_t(Read<ExFatFileEntry>(entry, 0).secondaryCount) + 1) * EntryBytes;
                if (expected < MinEntrySetSize || expected > MaxEntrySetSize) { ++result.invalidRecords; return true; }
            }

            // Collect complete entry sets even when they cross a directory cluster boundary.
            entries.insert(entries.end(), entry.begin(), entry.end());
            offsets.push_back(offset);
            if (entries.size() != expected) return true;
            Record record;
            const bool valid = ParseEntrySet(entries, m_geometry, record);
            entries.clear();
            if (!valid) { offsets.clear(); ++result.invalidRecords; return true; }
            record.number = offsets.front();
            record.entryOffsets = std::move(offsets);
            offsets.clear();
            record.path = directory.path + record.name;

            // Only live directories provide reliable ancestry for further traversal.
            if (record.directory && record.inUse)
            {
                const auto stream = Read<ExFatStreamEntry>(record.snapshot, EntryBytes);
                pending.push_back({ stream.firstCluster, record.data.size,
                    (stream.flags & ExFatNoFatChain) != 0, record.path + L"\\" });
                return true;
            }
            if (record.inUse || record.directory || !record.supported) return true;
            for (const auto& run : record.data.runs)
                if (!ClustersHaveState(static_cast<DWORD>(run.lcn + ExFatFirstDataCluster), run.count, false, progress))
                    return true;
            result.records.push_back(std::move(record));
            if (discovered) discovered(result.records.back());
            return true;
        });
        if (!entries.empty()) ++result.invalidRecords;
    }

    // Deleted NoFatChain companions can be read without trusting a released FAT chain.
    ResolveRecyclePaths(progress, result, [&](const Record& record, const std::span<BYTE> bytes)
    {
        // Check the companion's directory entry and allocation before and after reading.
        Validate(record, progress);
        ReadAt(ClusterOffset(static_cast<DWORD>(record.data.runs.front().lcn + ExFatFirstDataCluster)), bytes);
        Validate(record, progress);
    });
}

void ExFatRecovery::Validate(const Record& record, Progress& progress)
{
    progress.Check();
    m_fatPageOffset = m_bitmapPageOffset = ULLONG_MAX;
    std::vector<BYTE> boot(m_boot.size());
    ReadAt(0, boot);
    Geometry geometry;
    if (!ParseBootRegion(boot, m_length, geometry) || geometry != m_geometry) throw Failure{ L"IDS_RECOVERY_CHANGED" };
    for (size_t i = 0; i < boot.size(); ++i)
        if (!IsExFatMutableBootByte(i) && boot[i] != m_boot[i]) throw Failure{ L"IDS_RECOVERY_CHANGED" };

    // Confirm that the bitmap and root directory still use their original cluster chains.
    std::array<BYTE, sizeof(ExFatBitmapEntry)> bitmap{};
    ReadAt(m_bitmapEntryOffset, bitmap);
    if (!std::ranges::equal(bitmap, m_bitmapEntry) ||
        Chain(Read<ExFatBitmapEntry>(bitmap, 0).firstCluster, m_bitmapSize, false, progress) != m_bitmapClusters ||
        Chain(m_geometry.rootCluster, 0, false, progress) != m_rootClusters ||
        record.snapshot.size() < MinEntrySetSize || record.snapshot.size() > MaxEntrySetSize ||
        record.snapshot.size() % EntryBytes != 0 ||
        record.entryOffsets.size() != record.snapshot.size() / EntryBytes ||
        record.entryOffsets.front() != record.number) throw Failure{ L"IDS_RECOVERY_CHANGED" };
    for (const DWORD cluster : m_bitmapClusters)
        if (!ClustersHaveState(cluster, 1, true, progress)) throw Failure{ L"IDS_RECOVERY_CHANGED" };

    // Reassemble the candidate from its original entry locations before comparing snapshots.
    std::vector<BYTE> entries(record.snapshot.size());
    for (size_t i = 0; i < record.entryOffsets.size(); ++i)
    {
        const auto offset = record.entryOffsets[i];
        if (offset % EntryBytes != 0 || offset < m_geometry.heapOffset ||
            offset >= m_geometry.heapOffset + ULONGLONG(m_geometry.clusterCount) * m_geometry.clusterSize)
            throw Failure{ L"IDS_RECOVERY_CHANGED" };
        ReadAt(offset, std::span<BYTE>(entries).subspan(i * EntryBytes, EntryBytes));
    }
    Record current;
    if (entries != record.snapshot || !ParseEntrySet(entries, m_geometry, current) || current.inUse ||
        current.directory || !current.supported ||
        current.data.size != record.data.size || current.data.initialized != record.data.initialized ||
        current.data.runs != record.data.runs) throw Failure{ L"IDS_RECOVERY_CHANGED" };
    for (const auto& run : current.data.runs)
        if (!ClustersHaveState(static_cast<DWORD>(run.lcn + ExFatFirstDataCluster), run.count, false, progress))
            throw Failure{ L"IDS_RECOVERY_CHANGED" };
}

std::wstring ExFatRecovery::RecoverFile(const Record& record,
    const std::wstring& canonicalDestination, Progress& progress)
{
    Validate(record, progress);
    OutputFile output(canonicalDestination, record);
    std::vector<BYTE> bytes(static_cast<size_t>(std::min<ULONGLONG>(CopyBufferBytes, record.data.size)));
    for (ULONGLONG position = 0; position < record.data.size;)
    {
        progress.Check();
        const auto take = static_cast<size_t>(std::min<ULONGLONG>(bytes.size(), record.data.size - position));
        const auto initialized = static_cast<size_t>(position < record.data.initialized ?
            std::min<ULONGLONG>(take, record.data.initialized - position) : 0);
        if (initialized != 0)
        {
            // Refresh allocation state on both sides of each source read to detect concurrent reuse.
            const DWORD first = static_cast<DWORD>(record.data.runs.front().lcn + ExFatFirstDataCluster +
                position / m_geometry.clusterSize);
            const ULONGLONG count = (position % m_geometry.clusterSize + initialized +
                m_geometry.clusterSize - 1) / m_geometry.clusterSize;
            if (!ClustersHaveState(first, count, false, progress, true)) throw Failure{ L"IDS_RECOVERY_CHANGED" };
            ReadAt(ClusterOffset(static_cast<DWORD>(record.data.runs.front().lcn + ExFatFirstDataCluster)) + position,
                std::span<BYTE>(bytes).first(initialized));
            if (!ClustersHaveState(first, count, false, progress, true)) throw Failure{ L"IDS_RECOVERY_CHANGED" };
        }

        // Bytes beyond ValidDataLength are logically zero, regardless of their on-disk contents.
        std::fill(bytes.begin() + initialized, bytes.begin() + take, BYTE{ 0 });
        output.Write(std::span<const BYTE>(bytes).first(take), progress);
        position += take;
    }

    // Revalidate around the final flush; OutputFile removes any uncommitted partial output.
    output.Commit([&]
    {
        // Recheck the source metadata around the destination flush.
        Validate(record, progress);
    },
        record.created.dwHighDateTime != 0 ? &record.created : nullptr,
        record.modified.dwHighDateTime != 0 ? &record.modified : nullptr);
    return output.Path();
}
