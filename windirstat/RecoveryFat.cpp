// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "RecoveryFat.h"

// FAT boot parameters and directory entries are packed on disk, including unaligned UTF-16 fields.
#pragma pack(push, 1)
struct FatBootHeader
{
    BYTE jump[3];
    BYTE oem[8];
    WORD sectorSize;
    BYTE sectorsPerCluster;
    WORD reservedSectors;
    BYTE fatCount;
    WORD rootEntries;
    WORD sectors16;
    BYTE media;
    WORD fatSectors16;
    WORD sectorsPerTrack;
    WORD heads;
    DWORD hiddenSectors;
    DWORD sectors32;
};

struct Fat32Extension
{
    DWORD fatSectors;
    WORD flags;
    WORD version;
    DWORD rootCluster;
    WORD infoSector;
    WORD backupSector;
    BYTE reserved[12];
};

struct FatVolumeExtension
{
    BYTE driveNumber;
    BYTE state;
    BYTE signature;
    DWORD serialNumber;
    char label[11];
    char fileSystem[8];
};

struct FatDirectoryEntry
{
    BYTE name[11];
    BYTE attributes;
    BYTE lowercase;
    BYTE createdTenths;
    WORD createdTime;
    WORD createdDate;
    WORD accessedDate;
    WORD clusterHigh;
    WORD modifiedTime;
    WORD modifiedDate;
    WORD clusterLow;
    DWORD size;
};

struct FatLongEntry
{
    BYTE ordinal;
    wchar_t name1[5];
    BYTE attributes;
    BYTE type;
    BYTE checksum;
    wchar_t name2[6];
    WORD cluster;
    wchar_t name3[2];
};
#pragma pack(pop)

static_assert(sizeof(FatBootHeader) == 36 && sizeof(Fat32Extension) == 28);
static_assert(sizeof(FatDirectoryEntry) == 32 && sizeof(FatLongEntry) == 32);
static_assert(sizeof(FatVolumeExtension) == 26 && offsetof(FatVolumeExtension, state) == 1);

// FAT format markers and limits; cluster-count cutoffs are exclusive.
constexpr size_t FatEntryBytes = sizeof(FatDirectoryEntry);
constexpr size_t FatLongNameCharacters = std::size(FatLongEntry{}.name1) +
    std::size(FatLongEntry{}.name2) + std::size(FatLongEntry{}.name3);
constexpr size_t FatMaxLongNameEntries = 20;
constexpr size_t FatShortBaseLength = 8;
constexpr size_t FatShortExtensionLength = 3;
constexpr BYTE FatDeletedEntry = 0xE5;
constexpr BYTE FatEscapedInitial = 0x05;
constexpr BYTE FatLastNameFragment = 0x40;
constexpr wchar_t FatNamePadding = 0xFFFF;
constexpr BYTE FatVolumeLabel = 0x08;
constexpr BYTE FatLongNameAttributes = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN |
    FILE_ATTRIBUTE_SYSTEM | FatVolumeLabel;
constexpr BYTE FatReservedAttributes = 0xC0;
constexpr BYTE FatLowercaseBase = 0x08;
constexpr BYTE FatLowercaseExtension = 0x10;
constexpr WORD FatBootSignature = 0xAA55;
constexpr BYTE FatNearJump = 0xE9;
constexpr BYTE FatShortJump = 0xEB;
constexpr BYTE FatNoOperation = 0x90;
constexpr DWORD FatMaxSectorBytes = 4096;
constexpr DWORD FatMaxSectorsPerCluster = 128;
constexpr DWORD FatMaxClusterBytes = 64 * 1024;
constexpr DWORD FatFirstDataCluster = 2;
constexpr DWORD Fat12ClusterLimit = 4085;
constexpr DWORD Fat16ClusterLimit = 65525;
constexpr DWORD Fat12ClusterMask = 0x0FFF;
constexpr DWORD Fat32ClusterMask = 0x0FFFFFFF;
constexpr DWORD Fat12ReservedCluster = 0x0FF0;
constexpr DWORD Fat16ReservedCluster = 0xFFF0;
constexpr DWORD Fat32ReservedCluster = 0x0FFFFFF0;
constexpr DWORD FatEndOfChainOffset = 8;
constexpr WORD Fat32ActiveFatMask = 0x000F;
constexpr WORD Fat32MirroringDisabled = 0x0080;
constexpr WORD Fat32ValidFlags = Fat32ActiveFatMask | Fat32MirroringDisabled;

bool FatRecovery::ParseBoot(const std::span<const BYTE> bytes,
    const ULONGLONG deviceLength, Geometry& geometry)
{
    geometry = {};
    if (bytes.size() < BootSectorBytes ||
        Read<WORD>(bytes, BootSectorBytes - sizeof(WORD)) != FatBootSignature) return false;
    const auto boot = Read<FatBootHeader>(bytes, 0);
    if ((boot.jump[0] != FatNearJump && (boot.jump[0] != FatShortJump || boot.jump[2] != FatNoOperation)) ||
        !std::has_single_bit(boot.sectorSize) ||
        boot.sectorSize < BootSectorBytes || boot.sectorSize > FatMaxSectorBytes ||
        !std::has_single_bit(boot.sectorsPerCluster) || boot.sectorsPerCluster > FatMaxSectorsPerCluster ||
        DWORD(boot.sectorSize) * boot.sectorsPerCluster > FatMaxClusterBytes ||
        boot.reservedSectors == 0 || boot.fatCount == 0 || deviceLength > LLONG_MAX) return false;
    const auto extended = Read<Fat32Extension>(bytes, sizeof(boot));
    const ULONGLONG sectors = boot.sectors16 != 0 ? boot.sectors16 : boot.sectors32;
    const ULONGLONG fatSectors = boot.fatSectors16 != 0 ? boot.fatSectors16 : extended.fatSectors;
    const ULONGLONG rootSectors = (ULONGLONG(boot.rootEntries) * FatEntryBytes + boot.sectorSize - 1) / boot.sectorSize;
    const ULONGLONG root = boot.reservedSectors + boot.fatCount * fatSectors;
    const ULONGLONG data = root + rootSectors;
    if (fatSectors == 0 || sectors > deviceLength / boot.sectorSize || data >= sectors) return false;

    // Cluster count determines the FAT variant; the textual filesystem label is not authoritative.
    const ULONGLONG count = (sectors - data) / boot.sectorsPerCluster;
    const DWORD bits = count < Fat12ClusterLimit ? 12 : count < Fat16ClusterLimit ? 16 : 32;
    if (count == 0 || count > Fat32ReservedCluster - FatFirstDataCluster ||
        ((count + FatFirstDataCluster) * bits + 7) / 8 > fatSectors * boot.sectorSize) return false;
    if (bits == 32 ? (boot.fatSectors16 != 0 || boot.rootEntries != 0 || boot.sectors16 != 0 ||
        extended.version != 0 || (extended.flags & ~Fat32ValidFlags) != 0 ||
        extended.rootCluster < FatFirstDataCluster || extended.rootCluster > count + 1) :
        (boot.fatSectors16 == 0 || boot.rootEntries == 0)) return false;
    const bool mirrored = bits != 32 || (extended.flags & Fat32MirroringDisabled) == 0;
    const DWORD active = mirrored ? 0 : extended.flags & Fat32ActiveFatMask;
    if (active >= boot.fatCount) return false;
    geometry = { bits, boot.sectorSize, DWORD(boot.sectorSize) * boot.sectorsPerCluster,
        static_cast<DWORD>(count), boot.fatCount, active, mirrored, boot.rootEntries,
        bits == 32 ? extended.rootCluster : 0, sectors * boot.sectorSize,
        ULONGLONG(boot.reservedSectors) * boot.sectorSize, fatSectors * boot.sectorSize,
        root * boot.sectorSize, data * boot.sectorSize };
    return true;
}

std::wstring FatRecovery::LongName(const std::span<const BYTE> bytes)
{
    if (bytes.size() < 2 * FatEntryBytes || bytes.size() > (FatMaxLongNameEntries + 1) * FatEntryBytes ||
        bytes.size() % FatEntryBytes != 0) return {};
    auto entry = Read<FatDirectoryEntry>(bytes, bytes.size() - FatEntryBytes);
    const bool deleted = entry.name[0] == FatDeletedEntry;
    const auto first = Read<FatLongEntry>(bytes, 0);

    // Deletion overwrites ordinals and the short name's first byte; checksum reversal recovers that byte.
    if (deleted)
    {
        BYTE value = first.checksum;
        for (size_t i = std::size(entry.name) - 1; i != 0; --i)
            value = std::rotl(static_cast<BYTE>(value - entry.name[i]), 1);
        if ((value < FirstPrintableCharacter && value != FatEscapedInitial) ||
            value == FatDeletedEntry || value == ' ' || value == '.') return {};
        entry.name[0] = value;
    }
    BYTE checksum = 0;
    for (const BYTE c : entry.name) checksum = static_cast<BYTE>(std::rotr(checksum, 1) + c);
    std::wstring name;
    const size_t count = bytes.size() / FatEntryBytes - 1;
    for (size_t ordinal = 1; ordinal <= count; ++ordinal)
    {
        const auto part = Read<FatLongEntry>(bytes, (count - ordinal) * FatEntryBytes);
        const size_t expected = deleted ? FatDeletedEntry : ordinal | (ordinal == count ? FatLastNameFragment : 0);
        if (part.ordinal != expected || part.attributes != FatLongNameAttributes || part.type != 0 ||
            part.cluster != 0 || part.checksum != checksum) return {};
        name.append(part.name1, std::size(part.name1));
        name.append(part.name2, std::size(part.name2));
        name.append(part.name3, std::size(part.name3));
    }

    // A terminator belongs only to the final fragment; all remaining code units must be padding.
    const auto end = name.find(L'\0');
    if (end != std::wstring::npos)
    {
        if (end < (count - 1) * FatLongNameCharacters || std::ranges::any_of(name.substr(end + 1),
            [](const wchar_t c)
        {
            // Require padding after the long filename's null terminator.
            return c != FatNamePadding;
        })) return {};
        name.resize(end);
    }
    if (name.empty() || name.size() > MaxFileNameLength || name == L"." || name == L"..") return {};
    for (size_t i = 0; i < name.size(); ++i)
    {
        const wchar_t c = name[i];
        if (c < FirstPrintableCharacter || c == FatNamePadding || std::wstring_view(L"<>:\"/\\|?*").contains(c) ||
            IS_LOW_SURROGATE(c)) return {};
        if (!IS_HIGH_SURROGATE(c)) continue;
        if (++i == name.size() || !IS_LOW_SURROGATE(name[i])) return {};
    }
    return name;
}

bool FatRecovery::ParseEntry(const std::span<const BYTE> bytes, const Geometry& geometry, Record& record)
{
    record = {};
    if (bytes.size() < FatEntryBytes || bytes.size() > (FatMaxLongNameEntries + 1) * FatEntryBytes ||
        bytes.size() % FatEntryBytes != 0) return false;
    auto entry = Read<FatDirectoryEntry>(bytes, bytes.size() - FatEntryBytes);
    if (entry.name[0] == 0 || entry.name[0] == ' ' ||
        (entry.attributes & (FatReservedAttributes | FatVolumeLabel)) != 0 ||
        (entry.lowercase & ~(FatLowercaseBase | FatLowercaseExtension)) != 0 || geometry.clusterSize == 0) return false;
    record.inUse = entry.name[0] != FatDeletedEntry;
    record.directory = (entry.attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (entry.name[0] == '.') return false;

    // Short names use the OEM code page. An underscore represents an erased initial character.
    if (!record.inUse) entry.name[0] = '_';
    else if (entry.name[0] == FatEscapedInitial) entry.name[0] = FatDeletedEntry;
    const auto decode = [](const BYTE* data, size_t size, const bool lowercase)
    {
        // Decode the space-padded OEM name and apply its lowercase flag.
        while (size != 0 && data[size - 1] == ' ') --size;
        std::wstring name(size, L'\0');
        const int length = MultiByteToWideChar(CP_OEMCP, MB_ERR_INVALID_CHARS,
            reinterpret_cast<const char*>(data), static_cast<int>(size), name.data(), static_cast<int>(size));
        if (size != 0 && length == 0) return std::wstring{};
        name.resize(length);
        if (std::ranges::any_of(name, [](const wchar_t c)
        {
            // Reject characters forbidden in a FAT short name.
            return c < FirstPrintableCharacter || std::wstring_view(L"\"*+,./:;<=>?[\\]|").contains(c);
        })) return std::wstring{};
        if (lowercase && !name.empty()) CharLowerBuffW(name.data(), static_cast<DWORD>(name.size()));
        return name;
    };
    record.name = LongName(bytes);
    const bool longName = !record.name.empty();
    if (!longName)
    {
        record.name = decode(entry.name, FatShortBaseLength, (entry.lowercase & FatLowercaseBase) != 0);
        const auto extension = decode(entry.name + FatShortBaseLength, FatShortExtensionLength,
            (entry.lowercase & FatLowercaseExtension) != 0);
        if (record.name.empty() || (entry.name[FatShortBaseLength] != ' ' && extension.empty())) return false;
        if (!extension.empty()) record.name += L'.' + extension;
    }
    const DWORD first = entry.clusterLow | (geometry.bits == 32 ? DWORD(entry.clusterHigh) << 16 : 0);
    const ULONGLONG count = (ULONGLONG(entry.size) + geometry.clusterSize - 1) / geometry.clusterSize;
    if ((record.directory && (entry.size != 0 || first < FatFirstDataCluster)) ||
        (!record.directory && ((count == 0 && first != 0) || (count != 0 && first < FatFirstDataCluster))) ||
        first > geometry.clusterCount + 1 ||
        (!record.inUse && count > ULONGLONG(geometry.clusterCount) + FatFirstDataCluster - first)) return false;

    const auto timestamp = [](const WORD date, const WORD time, const BYTE tenths)
    {
        // Apply the creation-time increment before converting the local timestamp to UTC.
        FILETIME local{}, utc{};
        if (tenths > MaxFatTimestampIncrement || !DosDateTimeToFileTime(date, time, &local)) return utc;
        ULARGE_INTEGER ticks{ .LowPart = local.dwLowDateTime, .HighPart = local.dwHighDateTime };
        ticks.QuadPart += ULONGLONG(tenths) * FileTimeTicksPer10Milliseconds;
        local = { ticks.LowPart, ticks.HighPart };
        return LocalFileTimeToFileTime(&local, &utc) ? utc : FILETIME{};
    };
    record.created = timestamp(entry.createdDate, entry.createdTime, entry.createdTenths);
    record.modified = timestamp(entry.modifiedDate, entry.modifiedTime, 0);
    record.data.size = record.data.initialized = entry.size;
    record.data.nonresident = count != 0;
    if (count != 0 || record.directory) record.data.runs.push_back({ 0, LONGLONG(first) - FatFirstDataCluster, count });
    record.condition = count == 0 ? Condition::Resident : Condition::Unallocated;
    const auto snapshot = longName ? bytes : bytes.last(FatEntryBytes);
    record.snapshot.assign(snapshot.begin(), snapshot.end());
    return true;
}

FatRecovery::FatRecovery(const std::wstring& volumeName, Progress* progress) : RecoveryShared(volumeName)
{
    // Read the boot parameters before preparing caches for the active or mirrored FATs.
    if (progress) progress->Check();
    GET_LENGTH_INFORMATION length{};
    DWORD returned = 0;
    if (!DeviceIoControl(m_volume, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &length, sizeof(length),
        &returned, nullptr)) throw Failure{ {}, GetLastError() };
    if (returned < sizeof(length) || length.Length.QuadPart < InitialSectorAlignment)
        throw Failure{ {}, ERROR_INVALID_DATA };
    m_length = static_cast<ULONGLONG>(length.Length.QuadPart);
    ReadVolume(0, m_boot, m_length, InitialSectorAlignment);
    if (!ParseBoot(m_boot, m_length, m_geometry)) throw Failure{ {}, ERROR_INVALID_DATA };
    m_fatPages.resize(m_geometry.mirrored ? m_geometry.fatCount : 1);
    if (progress) progress->Check();
}

void FatRecovery::ReadAt(const ULONGLONG offset, const std::span<BYTE> bytes)
{
    // Limit each read before applying the FAT volume's bounds and sector alignment.
    if (bytes.size() > CopyBufferBytes) throw Failure{ {}, ERROR_INVALID_DATA };
    ReadVolume(offset, bytes, m_geometry.volumeSize, m_geometry.sectorSize);
}

ULONGLONG FatRecovery::ClusterOffset(const DWORD cluster) const
{
    // Convert FAT data-cluster numbers to validated byte offsets.
    if (cluster < FatFirstDataCluster || cluster > m_geometry.clusterCount + 1) throw Failure{ {}, ERROR_INVALID_DATA };
    return m_geometry.dataOffset + ULONGLONG(cluster - FatFirstDataCluster) * m_geometry.clusterSize;
}

DWORD FatRecovery::FatValue(const DWORD cluster)
{
    ClusterOffset(cluster);
    const ULONGLONG offset = ULONGLONG(cluster) * m_geometry.bits / 8;
    const ULONGLONG pageOffset = offset & ~ULONGLONG(CachePageBytes - 1);
    DWORD value = 0;
    for (size_t i = 0; i < m_fatPages.size(); ++i)
    {
        auto& page = m_fatPages[i];
        if (page.offset != pageOffset)
        {
            // Include overlapping bytes so a packed FAT12 entry may straddle the page boundary.
            page.bytes.resize(static_cast<size_t>(std::min<ULONGLONG>(CachePageBytes + sizeof(DWORD),
                m_geometry.fatSize - pageOffset)));
            const auto copy = m_geometry.mirrored ? i : m_geometry.activeFat;
            ReadAt(m_geometry.fatOffset + copy * m_geometry.fatSize + pageOffset, page.bytes);
            page.offset = pageOffset;
        }
        const auto within = static_cast<size_t>(offset - pageOffset);
        DWORD current = m_geometry.bits == 32 ? Read<DWORD>(page.bytes, within) & Fat32ClusterMask :
            Read<WORD>(page.bytes, within);
        if (m_geometry.bits == 12) current = (current >> ((cluster & 1) * 4)) & Fat12ClusterMask;
        if (i != 0 && current != value) throw Failure{ {}, ERROR_INVALID_DATA };
        value = current;
    }
    return value;
}

std::vector<DWORD> FatRecovery::Chain(const DWORD first, Progress& progress)
{
    // Follow FAT links to their end marker, rejecting loops and invalid clusters.
    std::vector<DWORD> clusters;
    std::unordered_set<DWORD> seen;
    const DWORD reserved = m_geometry.bits == 12 ? Fat12ReservedCluster :
        m_geometry.bits == 16 ? Fat16ReservedCluster : Fat32ReservedCluster;
    for (DWORD cluster = first;;)
    {
        progress.Check();
        ClusterOffset(cluster);
        if (!seen.insert(cluster).second) throw Failure{ {}, ERROR_INVALID_DATA };
        clusters.push_back(cluster);
        cluster = FatValue(cluster);
        if (cluster >= reserved + FatEndOfChainOffset) return clusters;
        if (cluster < FatFirstDataCluster || cluster >= reserved) throw Failure{ {}, ERROR_INVALID_DATA };
    }
}

void FatRecovery::VisitEntries(const DWORD first, const std::vector<DWORD>& clusters, Progress& progress,
    const std::function<bool(std::span<const BYTE>, ULONGLONG)>& visit)
{
    // Enumerate fixed root entries or directory clusters until an end marker or callback stop.
    const ULONGLONG size = first == 0 ? ULONGLONG(m_geometry.rootEntries) * FatEntryBytes : m_geometry.clusterSize;
    std::vector<BYTE> bytes(static_cast<size_t>(std::min<ULONGLONG>(CachePageBytes, size)));
    const size_t regions = first == 0 ? 1 : clusters.size();
    for (size_t i = 0; i < regions; ++i)
    {
        const auto start = first == 0 ? m_geometry.rootOffset : ClusterOffset(clusters[i]);
        for (ULONGLONG offset = 0; offset < size;)
        {
            progress.Check();
            const auto take = static_cast<size_t>(std::min<ULONGLONG>(bytes.size(), size - offset));
            ReadAt(start + offset, std::span<BYTE>(bytes).first(take));
            for (size_t within = 0; within < take; within += FatEntryBytes)
                if (bytes[within] == 0 || !visit(std::span<const BYTE>(bytes).subspan(within, FatEntryBytes),
                    start + offset + within)) return;
            offset += take;
        }
    }
}

bool FatRecovery::ClustersFree(const DWORD first, const ULONGLONG count, Progress& progress, const bool fresh)
{
    // Require every candidate cluster to remain free, refreshing the FAT cache when requested.
    ClusterOffset(first);
    if (count > ULONGLONG(m_geometry.clusterCount) + FatFirstDataCluster - first)
        throw Failure{ {}, ERROR_INVALID_DATA };
    if (fresh) for (auto& page : m_fatPages) page.offset = ULLONG_MAX;
    for (ULONGLONG i = 0; i < count; ++i)
    {
        progress.Check();
        if (FatValue(first + static_cast<DWORD>(i)) != 0) return false;
    }
    return true;
}

void FatRecovery::Scan(Progress& progress, ScanResult& result,
    const std::function<void(const Record&)>& discovered)
{
    result = {};
    m_directories.clear();
    for (auto& page : m_fatPages) page.offset = ULLONG_MAX;
    struct Directory { DWORD first; std::wstring path; };
    std::vector<Directory> pending{ { m_geometry.rootCluster, L"\\" } };
    std::unordered_set<DWORD> visited;

    // Follow allocated directory chains only; a deleted directory has lost its own chain.
    while (!pending.empty())
    {
        progress.Check();
        auto directory = std::move(pending.back());
        pending.pop_back();
        try
        {
            auto clusters = directory.first == 0 ? std::vector<DWORD>{} : Chain(directory.first, progress);
            if (std::ranges::any_of(clusters, [&](DWORD cluster)
            {
                // Reject directories whose clusters were already visited.
                return visited.contains(cluster);
            }))
                throw Failure{ {}, ERROR_INVALID_DATA };
            visited.insert(clusters.begin(), clusters.end());
            m_directories.emplace(directory.first, clusters);
            std::vector<BYTE> entries;
            std::vector<ULONGLONG> offsets;

            // Collect name fragments and their physical offsets before parsing the short entry.
            VisitEntries(directory.first, clusters, progress, [&](const std::span<const BYTE> entry,
                const ULONGLONG offset)
            {
                const auto directoryEntry = Read<FatDirectoryEntry>(entry, 0);
                if (directoryEntry.attributes == FatLongNameAttributes)
                {
                    if (entries.size() == FatMaxLongNameEntries * FatEntryBytes || (!entries.empty() &&
                        ((entry.front() == FatDeletedEntry) != (entries.front() == FatDeletedEntry) ||
                        entry[offsetof(FatLongEntry, checksum)] != entries[offsetof(FatLongEntry, checksum)] ||
                        (entry.front() != FatDeletedEntry && (entry.front() & FatLastNameFragment) != 0))))
                    { entries.clear(); offsets.clear(); }
                    entries.insert(entries.end(), entry.begin(), entry.end());
                    offsets.push_back(offset);
                    return true;
                }
                entries.insert(entries.end(), entry.begin(), entry.end());
                offsets.push_back(offset);
                Record record;
                const bool valid = ParseEntry(entries, m_geometry, record);
                if (record.snapshot.size() == FatEntryBytes) offsets.assign(1, offset);
                record.entryOffsets = std::move(offsets);
                entries.clear();
                offsets.clear();
                if (!valid)
                {
                    if ((directoryEntry.attributes & FatVolumeLabel) == 0 && entry.front() != '.')
                        ++result.invalidRecords;
                    return true;
                }
                record.number = offset;
                record.parent = directory.first;
                record.path = directory.path + record.name;
                if (record.directory && record.inUse)
                    pending.push_back({ static_cast<DWORD>(record.data.runs.front().lcn + FatFirstDataCluster),
                        record.path + L"\\" });
                if (record.inUse || record.directory) return true;

                // FAT deletion erases the chain: larger candidates explicitly assume consecutive free clusters.
                try
                {
                    for (const auto& run : record.data.runs)
                        if (!ClustersFree(static_cast<DWORD>(run.lcn + FatFirstDataCluster), run.count, progress))
                            return true;
                }
                catch (const Failure& failure)
                {
                    if (failure.error != ERROR_INVALID_DATA) throw;
                    ++result.invalidRecords;
                    return true;
                }
                result.records.push_back(std::move(record));
                if (discovered) discovered(result.records.back());
                return true;
            });
        }
        catch (const Failure& failure)
        {
            if (failure.error != ERROR_INVALID_DATA) throw;
            ++result.invalidRecords;
        }
    }

    // Match erased '$' initials only inside the Recycle Bin, and restore every unresolved display name.
    std::vector<std::pair<size_t, std::wstring>> normalized;
    const SmartPointer restoreNames([&](ScanResult* scan)
    {
        // Restore erased initials for paths that Recycle Bin metadata did not resolve.
        for (const auto& [index, path] : normalized)
        {
            auto& record = scan->records[index];
            if (record.path != path) continue;
            record.path[record.path.size() - record.name.size()] = L'_';
            record.name[0] = L'_';
        }
    }, &result);
    for (size_t i = 0; i < result.records.size(); ++i)
    {
        auto& record = result.records[i];
        constexpr std::wstring_view root = L"\\$Recycle.Bin\\";
        if (record.snapshot.size() != FatEntryBytes || record.name.size() < RecycleNameStemLength ||
            record.name[0] != L'_' || (towupper(record.name[1]) != L'I' && towupper(record.name[1]) != L'R') ||
            record.path.size() < root.size() || _wcsnicmp(record.path.c_str(), root.data(), root.size()) != 0)
            continue;
        auto path = record.path;
        path[path.size() - record.name.size()] = L'$';
        normalized.emplace_back(i, std::move(path));
        record.path[record.path.size() - record.name.size()] = L'$';
        record.name[0] = L'$';
    }

    // Read $I metadata only when all bytes fit in its known first cluster.
    ResolveRecyclePaths(progress, result, [&](const Record& record, const std::span<BYTE> bytes)
    {
        // Validate the companion before and after reading its known first cluster.
        if (record.data.runs.size() != 1 || record.data.runs.front().count != 1)
            throw Failure{ {}, ERROR_INVALID_DATA };
        Validate(record, progress);
        ReadAt(ClusterOffset(static_cast<DWORD>(record.data.runs.front().lcn + FatFirstDataCluster)), bytes);
        Validate(record, progress);
    });
}

void FatRecovery::Validate(const Record& record, Progress& progress)
{
    progress.Check();
    for (auto& page : m_fatPages) page.offset = ULLONG_MAX;
    std::array<BYTE, BootSectorBytes> boot{};
    ReadAt(0, boot);

    // Windows may change the extended BPB's dirty-state byte while the volume stays mounted.
    const size_t state = sizeof(FatBootHeader) + (m_geometry.bits == 32 ? sizeof(Fat32Extension) : 0) +
        offsetof(FatVolumeExtension, state);
    boot[state] = m_boot[state];
    if (boot != m_boot || record.inUse || record.directory || !record.supported || !record.data.supported ||
        record.parent > MAXDWORD || record.snapshot.empty() ||
        record.snapshot.size() > (FatMaxLongNameEntries + 1) * FatEntryBytes ||
        record.snapshot.size() % FatEntryBytes != 0 ||
        record.entryOffsets.size() != record.snapshot.size() / FatEntryBytes ||
        record.entryOffsets.back() != record.number) throw Failure{ L"IDS_RECOVERY_CHANGED" };

    // Verify the containing directory's allocation and every saved name fragment before copying data.
    const auto directory = m_directories.find(static_cast<DWORD>(record.parent));
    if (directory == m_directories.end()) throw Failure{ L"IDS_RECOVERY_CHANGED" };
    try
    {
        if (record.parent != 0 && Chain(static_cast<DWORD>(record.parent), progress) != directory->second)
            throw Failure{ L"IDS_RECOVERY_CHANGED" };
    }
    catch (const Failure& failure)
    {
        if (failure.error == ERROR_INVALID_DATA) throw Failure{ L"IDS_RECOVERY_CHANGED" };
        throw;
    }
    std::vector<BYTE> entries(record.snapshot.size());
    for (size_t i = 0; i < record.entryOffsets.size(); ++i)
    {
        const auto offset = record.entryOffsets[i];
        const bool belongs = record.parent == 0 ? offset >= m_geometry.rootOffset &&
            offset - m_geometry.rootOffset < ULONGLONG(m_geometry.rootEntries) * FatEntryBytes :
            offset >= m_geometry.dataOffset && std::ranges::contains(directory->second,
                (offset - m_geometry.dataOffset) / m_geometry.clusterSize + FatFirstDataCluster);
        if (offset % FatEntryBytes != 0 || !belongs) throw Failure{ L"IDS_RECOVERY_CHANGED" };
        ReadAt(offset, std::span<BYTE>(entries).subspan(i * FatEntryBytes, FatEntryBytes));
    }
    Record current;
    if (entries != record.snapshot || !ParseEntry(entries, m_geometry, current) ||
        current.inUse || current.directory || current.data.size != record.data.size ||
        current.data.initialized != record.data.initialized || current.data.runs != record.data.runs)
        throw Failure{ L"IDS_RECOVERY_CHANGED" };
    for (const auto& run : current.data.runs)
        if (!ClustersFree(static_cast<DWORD>(run.lcn + FatFirstDataCluster), run.count, progress))
            throw Failure{ L"IDS_RECOVERY_CHANGED" };
}

std::wstring FatRecovery::RecoverFile(const Record& record,
    const std::wstring& canonicalDestination, Progress& progress)
{
    Validate(record, progress);
    OutputFile output(canonicalDestination, record);
    std::vector<BYTE> bytes(static_cast<size_t>(std::min<ULONGLONG>(CopyBufferBytes, record.data.size)));

    // Copy bounded chunks, checking allocation before and after every source read.
    for (ULONGLONG position = 0; position < record.data.size;)
    {
        progress.Check();
        const auto take = static_cast<size_t>(std::min<ULONGLONG>(bytes.size(), record.data.size - position));
        const DWORD first = static_cast<DWORD>(record.data.runs.front().lcn + FatFirstDataCluster +
            position / m_geometry.clusterSize);
        const ULONGLONG count = (position % m_geometry.clusterSize + take + m_geometry.clusterSize - 1) /
            m_geometry.clusterSize;
        if (!ClustersFree(first, count, progress, true)) throw Failure{ L"IDS_RECOVERY_CHANGED" };
        ReadAt(ClusterOffset(static_cast<DWORD>(record.data.runs.front().lcn + FatFirstDataCluster)) + position,
            std::span<BYTE>(bytes).first(take));
        if (!ClustersFree(first, count, progress, true)) throw Failure{ L"IDS_RECOVERY_CHANGED" };
        output.Write(std::span<const BYTE>(bytes).first(take), progress);
        position += take;
    }

    // Recheck directory entries and allocation on both sides of the final flush.
    output.Commit([&]
    {
        // Recheck the source metadata around the destination flush.
        Validate(record, progress);
    },
        record.created.dwHighDateTime != 0 ? &record.created : nullptr,
        record.modified.dwHighDateTime != 0 ? &record.modified : nullptr);
    return output.Path();
}
