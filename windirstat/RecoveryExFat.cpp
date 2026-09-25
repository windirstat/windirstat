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

#include "pch.h"
#include "RecoveryExFat.h"

bool ExFatRecovery::ParseBootRegion(const std::span<const BYTE> bytes,
    const ULONGLONG deviceLength, Geometry& geometry)
{
    geometry = {};
    if (bytes.size() < 512 || bytes[0] != 0xeb || bytes[1] != 0x76 || bytes[2] != 0x90 ||
        std::memcmp(bytes.data() + 3, "EXFAT   ", 8) != 0 || bytes[108] < 9 || bytes[108] > 12 ||
        bytes[109] > 25 - bytes[108] || NtfsRecovery::Read<WORD>(bytes, 510) != 0xaa55 ||
        NtfsRecovery::Read<WORD>(bytes, 104) != 0x0100 || bytes[110] != 1 ||
        (NtfsRecovery::Read<WORD>(bytes, 106) & 1) != 0 || (bytes[112] > 100 && bytes[112] != 255) ||
        std::ranges::any_of(bytes.subspan(11, 53), [](const BYTE value) { return value != 0; })) return false;
    const DWORD sector = 1u << bytes[108], cluster = sector << bytes[109];
    if (bytes.size() < sector * 12u) return false;

    // Mutable volume flags and usage percentage are excluded from the boot checksum.
    DWORD checksum = 0;
    for (size_t i = 0; i < sector * 11u; ++i)
        if (i != 106 && i != 107 && i != 112) checksum = std::rotr(checksum, 1) + bytes[i];
    for (size_t i = sector * 11u; i < sector * 12u; i += 4)
        if (NtfsRecovery::Read<DWORD>(bytes, i) != checksum) return false;
    for (size_t i = 1; i <= 8; ++i)
        if (NtfsRecovery::Read<DWORD>(bytes, (i + 1) * sector - 4) != 0xaa550000) return false;

    // Cross-check the FAT and cluster heap against the reported device length.
    const auto sectors = NtfsRecovery::Read<ULONGLONG>(bytes, 72);
    const auto fat = NtfsRecovery::Read<DWORD>(bytes, 80), fatLength = NtfsRecovery::Read<DWORD>(bytes, 84);
    const auto heap = NtfsRecovery::Read<DWORD>(bytes, 88), count = NtfsRecovery::Read<DWORD>(bytes, 92);
    const auto root = NtfsRecovery::Read<DWORD>(bytes, 96);
    if (deviceLength > LLONG_MAX || sectors < 1024 * 1024 / sector || sectors > deviceLength / sector ||
        fat < 24 || fatLength == 0 || ULONGLONG(fat) + fatLength > heap || heap >= sectors ||
        count == 0 || count > 0xfffffff5 || root < 2 || root > ULONGLONG(count) + 1 ||
        count != std::min<ULONGLONG>((sectors - heap) >> bytes[109], 0xfffffff5) ||
        ULONGLONG(fatLength) * sector < (ULONGLONG(count) + 2) * 4) return false;
    geometry = { sector, cluster, count, root, sectors * sector, ULONGLONG(fat) * sector,
        ULONGLONG(heap) * sector };
    return true;
}

FILETIME ExFatRecovery::Timestamp(const DWORD packed, const BYTE increment, const BYTE offset)
{
    FILETIME local = {}, utc = {};
    if (increment > 199 || !DosDateTimeToFileTime(static_cast<WORD>(packed >> 16),
        static_cast<WORD>(packed), &local)) return {};
    ULARGE_INTEGER time{ .LowPart = local.dwLowDateTime, .HighPart = local.dwHighDateTime };
    time.QuadPart += ULONGLONG(increment) * 100000;
    if ((offset & 0x80) == 0)
    {
        local = { time.LowPart, time.HighPart };
        return LocalFileTimeToFileTime(&local, &utc) ? utc : FILETIME{};
    }

    // Stored UTC offsets use signed quarter-hour units.
    const int quarters = (offset & 0x40) ? int(offset & 0x7f) - 128 : offset & 0x7f;
    time.QuadPart -= LONGLONG(quarters) * 15 * 60 * 10000000;
    return { time.LowPart, time.HighPart };
}

bool ExFatRecovery::ParseEntrySet(const std::span<const BYTE> bytes, const Geometry& geometry, Record& record)
{
    record = {};
    if (bytes.size() < 96 || bytes.size() > MaxEntrySetSize || geometry.clusterSize == 0 ||
        geometry.clusterCount == 0 || (bytes[0] != 0x85 && bytes[0] != 0x05) ||
        bytes.size() != (size_t(bytes[1]) + 1) * 32) return false;
    const BYTE inUse = bytes[0] & 0x80;
    const BYTE nameLength = bytes[35], flags = bytes[33];
    const size_t nameEnd = 64 + ((nameLength + 14) / 15) * 32;
    if (bytes[32] != (0x40 | inUse) || (flags != 1 && flags != 3) || nameLength == 0 ||
        bytes.size() < nameEnd || (NtfsRecovery::Read<WORD>(bytes, 4) & ~0x37) != 0) return false;
    // Restore deleted entries' InUse bits when checking their original set checksum.
    WORD checksum = 0;
    for (size_t i = 0; i < bytes.size(); ++i)
        if (i != 2 && i != 3) checksum = static_cast<WORD>(std::rotr(checksum, 1) +
            (i % 32 == 0 ? bytes[i] | 0x80 : bytes[i]));
    if (checksum != NtfsRecovery::Read<WORD>(bytes, 2)) return false;
    for (size_t i = 64; i < bytes.size(); i += 32)
        if (i < nameEnd ? bytes[i] != (0x41 | inUse) : (bytes[i] & 0xe0) != (0x60 | inUse)) return false;

    // Filename fragments hold up to fifteen UTF-16 code units per entry.
    for (size_t i = 0; i < nameLength; ++i)
    {
        const wchar_t c = NtfsRecovery::Read<wchar_t>(bytes, 66 + (i / 15) * 32 + (i % 15) * 2);
        if (c < 32 || std::wstring_view(L"<>:\"/\\|?*").contains(c)) return false;
        record.name += c;
    }
    if (record.name == L"." || record.name == L"..") return false;

    // Reject unpaired surrogates before exposing the filename to the UI or filesystem.
    for (size_t i = 0; i < record.name.size(); ++i)
    {
        const wchar_t c = record.name[i];
        if (c >= 0xdc00 && c <= 0xdfff) return false;
        if (c < 0xd800 || c > 0xdbff) continue;
        if (++i == record.name.size() || record.name[i] < 0xdc00 || record.name[i] > 0xdfff) return false;
    }
    record.inUse = inUse != 0;
    record.directory = (NtfsRecovery::Read<WORD>(bytes, 4) & FILE_ATTRIBUTE_DIRECTORY) != 0;
    record.sequence = checksum;
    record.created = Timestamp(NtfsRecovery::Read<DWORD>(bytes, 8), bytes[20], bytes[22]);
    record.modified = Timestamp(NtfsRecovery::Read<DWORD>(bytes, 12), bytes[21], bytes[23]);
    record.data.size = NtfsRecovery::Read<ULONGLONG>(bytes, 56);
    record.data.initialized = NtfsRecovery::Read<ULONGLONG>(bytes, 40);
    const DWORD first = NtfsRecovery::Read<DWORD>(bytes, 52);
    const ULONGLONG count = (record.data.size / geometry.clusterSize) +
        (record.data.size % geometry.clusterSize != 0);

    // Check stream bounds and require directory data to occupy complete initialized clusters.
    if (record.data.size > LLONG_MAX || count > geometry.clusterCount ||
        record.data.initialized > record.data.size ||
        (count == 0 && (first != 0 || flags != 1)) ||
        (count != 0 && (first < 2 || first > ULONGLONG(geometry.clusterCount) + 1)) ||
        (flags == 3 && count > ULONGLONG(geometry.clusterCount) + 2 - first) ||
        (record.directory && (record.data.size == 0 ||
            record.data.size % geometry.clusterSize != 0 || record.data.initialized != record.data.size)))
        return false;
    // Only NoFatChain extents survive deletion unambiguously; live directories can still use the FAT.
    record.data.nonresident = record.data.size != 0;
    record.supported = record.data.supported = record.inUse ||
        (bytes.size() == nameEnd && (count == 0 || flags == 3));
    if (count != 0 && flags == 3) record.data.runs.push_back({ 0, LONGLONG(first) - 2, count });
    record.snapshot.assign(bytes.begin(), bytes.end());
    record.condition = count == 0 ? Condition::Resident : Condition::Unallocated;
    return true;
}

ExFatRecovery::ExFatRecovery(const std::wstring& volumeName, Progress* const progress)
{
    if (volumeName.empty() || volumeName.back() != L'\\') throw Failure{ {}, ERROR_INVALID_NAME };
    m_volume = CreateFileW(volumeName.substr(0, volumeName.size() - 1).c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING, nullptr);
    if (!m_volume.IsValid()) throw Failure{ {}, GetLastError() };
    GET_LENGTH_INFORMATION length = {};
    DWORD returned = 0;
    if (!DeviceIoControl(m_volume, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &length, sizeof(length),
        &returned, nullptr)) throw Failure{ {}, GetLastError() };
    if (returned < sizeof(length) || length.Length.QuadPart < 1024 * 1024)
        throw Failure{ {}, ERROR_INVALID_DATA };
    m_length = static_cast<ULONGLONG>(length.Length.QuadPart);
    Initialize(progress);
}

void ExFatRecovery::ReadRaw(const ULONGLONG offset, const std::span<BYTE> bytes)
{
    const auto prefix = static_cast<DWORD>(offset % m_alignment);
    const size_t length = (prefix + bytes.size() + m_alignment - 1) & ~size_t(m_alignment - 1);
    if (length > ReadSize + 4096 || offset - prefix > m_length || length > m_length - (offset - prefix))
        throw Failure{ {}, ERROR_INVALID_DATA };
    if (length > m_rawBufferSize)
    {
        m_rawBuffer = _aligned_malloc(length, 4096);
        m_rawBufferSize = m_rawBuffer.IsValid() ? length : 0;
    }
    if (!m_rawBuffer.IsValid()) throw Failure{ {}, ERROR_NOT_ENOUGH_MEMORY };
    LARGE_INTEGER position{ .QuadPart = static_cast<LONGLONG>(offset - prefix) };
    DWORD returned = 0;
    if (!SetFilePointerEx(m_volume, position, nullptr, FILE_BEGIN) ||
        !ReadFile(m_volume, m_rawBuffer.Get(), static_cast<DWORD>(length), &returned, nullptr))
        throw Failure{ {}, GetLastError() };
    if (returned != length) throw Failure{ {}, ERROR_HANDLE_EOF };
    std::memcpy(bytes.data(), static_cast<const BYTE*>(m_rawBuffer.Get()) + prefix, bytes.size());
}

void ExFatRecovery::ReadAt(const ULONGLONG offset, const std::span<BYTE> bytes)
{
    if (bytes.empty() || bytes.size() > ReadSize || offset > m_length || bytes.size() > m_length - offset)
        throw Failure{ {}, ERROR_INVALID_DATA };
    ReadRaw(offset, bytes);
}

ULONGLONG ExFatRecovery::ClusterOffset(const DWORD cluster) const
{
    if (cluster < 2 || cluster > ULONGLONG(m_geometry.clusterCount) + 1)
        throw Failure{ {}, ERROR_INVALID_DATA };
    return m_geometry.heapOffset + ULONGLONG(cluster - 2) * m_geometry.clusterSize;
}

DWORD ExFatRecovery::NextCluster(const DWORD cluster)
{
    ClusterOffset(cluster);
    const ULONGLONG offset = ULONGLONG(cluster) * 4;
    const ULONGLONG page = offset & ~ULONGLONG(65535);
    if (page != m_fatPageOffset)
    {
        m_fatPage.resize(static_cast<size_t>(std::min<ULONGLONG>(65536,
            (ULONGLONG(m_geometry.clusterCount) + 2) * 4 - page)));
        ReadAt(m_geometry.fatOffset + page, m_fatPage);
        m_fatPageOffset = page;
    }
    return NtfsRecovery::Read<DWORD>(m_fatPage, static_cast<size_t>(offset - page));
}

std::vector<DWORD> ExFatRecovery::Chain(const DWORD first, const ULONGLONG length, const bool contiguous,
    Progress& progress)
{
    ClusterOffset(first);
    const ULONGLONG expected = length / m_geometry.clusterSize + (length % m_geometry.clusterSize != 0);
    const ULONGLONG maximum = length == 0 ? m_geometry.clusterCount : expected;
    if (maximum == 0 || maximum > m_geometry.clusterCount ||
        (contiguous && maximum > ULONGLONG(m_geometry.clusterCount) + 2 - first))
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
        if (cluster == 0xffffffff)
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
    std::vector<BYTE> bytes(std::min<DWORD>(65536, m_geometry.clusterSize));
    for (const DWORD cluster : clusters)
    {
        const ULONGLONG physical = ClusterOffset(cluster);
        for (DWORD offset = 0; offset < m_geometry.clusterSize; offset += static_cast<DWORD>(bytes.size()))
        {
            progress.Check();
            ReadAt(physical + offset, bytes);
            for (size_t i = 0; i < bytes.size(); i += 32)
            {
                if (bytes[i] == 0) return;
                if (!visit(std::span<const BYTE>(bytes).subspan(i, 32), physical + offset + i)) return;
            }
        }
    }
}

void ExFatRecovery::Initialize(Progress* const suppliedProgress)
{
    Progress fallback;
    Progress& progress = suppliedProgress ? *suppliedProgress : fallback;
    progress.Check();
    std::array<BYTE, 4096> first{};
    ReadAt(0, first);
    if (first[108] < 9 || first[108] > 12) throw Failure{ {}, ERROR_INVALID_DATA };

    // Read the full boot region after its sector size is known.
    m_boot.resize((1u << first[108]) * 12u);
    progress.Check();
    ReadAt(0, m_boot);
    if (!ParseBootRegion(m_boot, m_length, m_geometry)) throw Failure{ {}, ERROR_INVALID_DATA };
    m_alignment = m_geometry.sectorSize;
    // Locate the allocation bitmap through the root directory and verify its own allocation.
    m_rootClusters = Chain(m_geometry.rootCluster, 0, false, progress);
    VisitEntries(m_rootClusters, progress, [&](const std::span<const BYTE> entry, const ULONGLONG offset)
    {
        if (entry[0] != 0x81) return true;
        if (!m_bitmapEntry.empty() || entry[1] != 0) throw Failure{ {}, ERROR_INVALID_DATA };
        m_bitmapEntry.assign(entry.begin(), entry.end());
        m_bitmapEntryOffset = offset;
        m_bitmapSize = NtfsRecovery::Read<ULONGLONG>(entry, 24);
        if (m_bitmapSize < (ULONGLONG(m_geometry.clusterCount) + 7) / 8 ||
            m_bitmapSize > ULONGLONG(m_geometry.clusterCount) * m_geometry.clusterSize)
            throw Failure{ {}, ERROR_INVALID_DATA };
        m_bitmapClusters = Chain(NtfsRecovery::Read<DWORD>(entry, 20), m_bitmapSize, false, progress);
        return true;
    });
    if (m_bitmapEntry.empty()) throw Failure{ {}, ERROR_INVALID_DATA };
    for (const DWORD cluster : m_bitmapClusters)
        if (!ClustersHaveState(cluster, 1, true, progress)) throw Failure{ {}, ERROR_INVALID_DATA };
}

// The bitmap is itself a FAT-mapped file, so a page can span non-adjacent clusters.
void ExFatRecovery::ReadBitmap(ULONGLONG offset, const std::span<BYTE> bytes)
{
    if (offset > m_bitmapSize || bytes.size() > m_bitmapSize - offset)
        throw Failure{ {}, ERROR_INVALID_DATA };
    size_t copied = 0;
    while (copied < bytes.size())
    {
        const auto index = static_cast<size_t>(offset / m_geometry.clusterSize);
        const auto within = static_cast<DWORD>(offset % m_geometry.clusterSize);
        if (index >= m_bitmapClusters.size()) throw Failure{ {}, ERROR_INVALID_DATA };
        const size_t limit = std::min<size_t>(ReadSize, bytes.size() - copied);
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
    ClusterOffset(first);
    if (count > ULONGLONG(m_geometry.clusterCount) + 2 - first) throw Failure{ {}, ERROR_INVALID_DATA };
    if (fresh) m_bitmapPageOffset = ULLONG_MAX;
    ULONGLONG bit = first - 2;
    while (count != 0)
    {
        progress.Check();
        const ULONGLONG offset = bit / 8;
        if (m_bitmapPageOffset == ULLONG_MAX || offset < m_bitmapPageOffset ||
            offset - m_bitmapPageOffset >= m_bitmapPage.size())
        {
            const ULONGLONG page = fresh ? offset : offset & ~ULONGLONG(65535);
            const ULONGLONG length = fresh ? (bit % 8 + count + 7) / 8 : m_bitmapSize - page;
            m_bitmapPage.resize(static_cast<size_t>(std::min<ULONGLONG>(65536, length)));
            ReadBitmap(page, m_bitmapPage);
            m_bitmapPageOffset = page;
        }
        const ULONGLONG within = bit - m_bitmapPageOffset * 8;
        if (within % 8 == 0 && count >= 8)
        {
            const auto take = static_cast<size_t>(std::min<ULONGLONG>(count / 8,
                m_bitmapPage.size() - within / 8));
            const auto full = std::span<const BYTE>(m_bitmapPage).subspan(static_cast<size_t>(within / 8), take);
            if (std::ranges::any_of(full, [allocated](const BYTE byte) { return byte != (allocated ? 255 : 0); }))
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

void ExFatRecovery::Scan(Progress& progress, NtfsRecovery::ScanResult& result,
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
            if (!entries.empty() && (entry[0] & 0x40) == 0)
            {
                entries.clear();
                offsets.clear();
                ++result.invalidRecords;
            }
            if (entries.empty())
            {
                if (entry[0] != 0x85 && entry[0] != 0x05) return true;
                expected = (size_t(entry[1]) + 1) * 32;
                if (expected < 96 || expected > MaxEntrySetSize) { ++result.invalidRecords; return true; }
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
                pending.push_back({ NtfsRecovery::Read<DWORD>(record.snapshot, 52), record.data.size,
                    (record.snapshot[33] & 2) != 0, record.path + L"\\" });
                return true;
            }
            if (record.inUse || record.directory || !record.supported) return true;
            for (const auto& run : record.data.runs)
                if (!ClustersHaveState(static_cast<DWORD>(run.lcn + 2), run.count, false, progress)) return true;
            result.records.push_back(std::move(record));
            if (discovered) discovered(result.records.back());
            return true;
        });
        if (!entries.empty()) ++result.invalidRecords;
    }
}

void ExFatRecovery::Validate(const Record& record, Progress& progress)
{
    progress.Check();
    m_fatPageOffset = m_bitmapPageOffset = ULLONG_MAX;
    std::vector<BYTE> boot(m_boot.size());
    ReadAt(0, boot);
    Geometry geometry;
    if (!ParseBootRegion(boot, m_length, geometry) || geometry != m_geometry)
        throw Failure{ L"IDS_RECOVERY_CHANGED" };
    for (size_t i = 0; i < boot.size(); ++i)
        if (i != 106 && i != 107 && i != 112 && boot[i] != m_boot[i])
            throw Failure{ L"IDS_RECOVERY_CHANGED" };

    // Confirm that the bitmap and root directory still use their original cluster chains.
    std::array<BYTE, 32> bitmap{};
    ReadAt(m_bitmapEntryOffset, bitmap);
    if (!std::ranges::equal(bitmap, m_bitmapEntry) ||
        Chain(NtfsRecovery::Read<DWORD>(bitmap, 20), m_bitmapSize, false, progress) != m_bitmapClusters ||
        Chain(m_geometry.rootCluster, 0, false, progress) != m_rootClusters ||
        record.snapshot.size() < 96 || record.snapshot.size() > MaxEntrySetSize || record.snapshot.size() % 32 != 0 ||
        record.entryOffsets.size() != record.snapshot.size() / 32 ||
        record.entryOffsets.front() != record.number) throw Failure{ L"IDS_RECOVERY_CHANGED" };
    for (const DWORD cluster : m_bitmapClusters)
        if (!ClustersHaveState(cluster, 1, true, progress)) throw Failure{ L"IDS_RECOVERY_CHANGED" };

    // Reassemble the candidate from its original entry locations before comparing snapshots.
    std::vector<BYTE> entries(record.snapshot.size());
    for (size_t i = 0; i < record.entryOffsets.size(); ++i)
    {
        const auto offset = record.entryOffsets[i];
        if (offset % 32 != 0 || offset < m_geometry.heapOffset ||
            offset >= m_geometry.heapOffset + ULONGLONG(m_geometry.clusterCount) * m_geometry.clusterSize)
            throw Failure{ L"IDS_RECOVERY_CHANGED" };
        ReadAt(offset, std::span<BYTE>(entries).subspan(i * 32, 32));
    }
    Record current;
    if (entries != record.snapshot || !ParseEntrySet(entries, m_geometry, current) || current.inUse ||
        current.directory || !current.supported || current.name != record.name ||
        current.data.size != record.data.size || current.data.initialized != record.data.initialized ||
        current.data.runs != record.data.runs) throw Failure{ L"IDS_RECOVERY_CHANGED" };
    for (const auto& run : current.data.runs)
        if (!ClustersHaveState(static_cast<DWORD>(run.lcn + 2), run.count, false, progress))
            throw Failure{ L"IDS_RECOVERY_CHANGED" };
}

std::wstring ExFatRecovery::Recover(const Record& record, const std::wstring& canonicalDestination, Progress& progress)
{
    Validate(record, progress);
    NtfsRecovery::OutputFile output(canonicalDestination, record);
    std::vector<BYTE> bytes(static_cast<size_t>(std::min<ULONGLONG>(ReadSize, record.data.size)));
    for (ULONGLONG position = 0; position < record.data.size;)
    {
        progress.Check();
        const auto take = static_cast<size_t>(std::min<ULONGLONG>(bytes.size(), record.data.size - position));
        const auto initialized = static_cast<size_t>(position < record.data.initialized ?
            std::min<ULONGLONG>(take, record.data.initialized - position) : 0);
        if (initialized != 0)
        {
            // Refresh allocation state on both sides of each source read to detect concurrent reuse.
            const DWORD first = static_cast<DWORD>(record.data.runs.front().lcn + 2 +
                position / m_geometry.clusterSize);
            const ULONGLONG count = (position % m_geometry.clusterSize + initialized +
                m_geometry.clusterSize - 1) / m_geometry.clusterSize;
            if (!ClustersHaveState(first, count, false, progress, true)) throw Failure{ L"IDS_RECOVERY_CHANGED" };
            ReadAt(ClusterOffset(static_cast<DWORD>(record.data.runs.front().lcn + 2)) + position,
                std::span<BYTE>(bytes).first(initialized));
            if (!ClustersHaveState(first, count, false, progress, true)) throw Failure{ L"IDS_RECOVERY_CHANGED" };
        }
        // Bytes beyond ValidDataLength are logically zero, regardless of their on-disk contents.
        std::fill(bytes.begin() + initialized, bytes.begin() + take, BYTE{ 0 });
        output.Write(std::span<const BYTE>(bytes).first(take), progress);
        position += take;
    }
    // Revalidate around the final flush; OutputFile removes any uncommitted partial output.
    output.Commit([&] { Validate(record, progress); },
        record.created.dwHighDateTime != 0 ? &record.created : nullptr,
        record.modified.dwHighDateTime != 0 ? &record.modified : nullptr);
    return output.Path();
}
