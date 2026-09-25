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
#include "RecoveryNtfs.h"
#include "RecoveryExFat.h"

std::span<const BYTE> NtfsRecovery::Slice(const std::span<const BYTE> bytes, const size_t offset, const size_t size)
{
    if (offset > bytes.size() || size > bytes.size() - offset) throw Failure{ {}, ERROR_INVALID_DATA };
    return bytes.subspan(offset, size);
}

std::wstring NtfsRecovery::ReadName(const std::span<const BYTE> bytes, const size_t offset, const size_t length)
{
    const auto data = Slice(bytes, offset, length * sizeof(wchar_t));
    std::wstring result(length, L'\0');
    std::memcpy(result.data(), data.data(), data.size());
    if (result.find(L'\0') != std::wstring::npos) throw Failure{ {}, ERROR_INVALID_DATA };
    return result;
}

void NtfsRecovery::Progress::Check() const
{
    while (paused.load(std::memory_order_relaxed) && !cancel.load(std::memory_order_relaxed)) Sleep(25);
    if (cancel.load(std::memory_order_relaxed)) throw Failure{ {}, ERROR_CANCELLED };
}

std::vector<NtfsRecovery::Run> NtfsRecovery::DecodeRuns(const std::span<const BYTE> bytes, const ULONGLONG lowestVcn,
    const ULONGLONG highestVcn, const ULONGLONG clusterCount)
{
    if (lowestVcn > highestVcn || highestVcn >= static_cast<ULONGLONG>(LLONG_MAX))
        throw Failure{ {}, ERROR_INVALID_DATA };
    std::vector<Run> runs;
    ULONGLONG vcn = lowestVcn;
    LONGLONG lcn = 0;
    for (size_t offset = 0; offset < bytes.size();)
    {
        const BYTE header = bytes[offset++];
        if (header == 0)
        {
            if (vcn != highestVcn + 1) throw Failure{ {}, ERROR_INVALID_DATA };

            // Check physical overlap separately from the logical order of the runs.
            auto physical = runs;
            std::erase_if(physical, [](const Run& run) { return run.lcn < 0; });
            std::ranges::sort(physical, {}, &Run::lcn);
            for (size_t i = 1; i < physical.size(); ++i)
                if (static_cast<ULONGLONG>(physical[i].lcn - physical[i - 1].lcn) < physical[i - 1].count)
                    throw Failure{ {}, ERROR_INVALID_DATA };
            return runs;
        }
        const unsigned countBytes = header & 15, deltaBytes = header >> 4;
        if (countBytes == 0 || countBytes > 8 || deltaBytes > 8 ||
            countBytes + deltaBytes > bytes.size() - offset) throw Failure{ {}, ERROR_INVALID_DATA };
        ULONGLONG count = 0, deltaBits = 0;
        for (unsigned i = 0; i < countBytes; ++i) count |= ULONGLONG(bytes[offset++]) << (8 * i);
        if (count == 0 || count > highestVcn + 1 - vcn) throw Failure{ {}, ERROR_INVALID_DATA };
        for (unsigned i = 0; i < deltaBytes; ++i) deltaBits |= ULONGLONG(bytes[offset++]) << (8 * i);
        if (deltaBytes != 0)
        {

            // Sign-extend relative offsets before advancing the physical cluster position.
            if (deltaBytes < 8 && (deltaBits & (1ull << (deltaBytes * 8 - 1))))
                deltaBits |= ~0ull << (deltaBytes * 8);
            const LONGLONG delta = std::bit_cast<LONGLONG>(deltaBits);
            if ((delta < 0 && delta < -lcn) || (delta > 0 && lcn > LLONG_MAX - delta))
                throw Failure{ {}, ERROR_INVALID_DATA };
            lcn += delta;
            if (static_cast<ULONGLONG>(lcn) >= clusterCount ||
                count > clusterCount - lcn) throw Failure{ {}, ERROR_INVALID_DATA };
        }
        runs.push_back({ vcn, deltaBytes == 0 ? -1 : lcn, count });
        vcn += count;
    }
    throw Failure{ {}, ERROR_INVALID_DATA };
}

bool NtfsRecovery::ParseRecord(const std::span<const BYTE> bytes, const ULONGLONG number, const DWORD clusterSize,
    const ULONGLONG clusterCount, Record& record, const bool scanning)
{
    record = {};
    try
    {
        if (bytes.size() < 512 || bytes.size() > 65536 || bytes.size() % 512 != 0 || clusterSize == 0 ||
            Read<DWORD>(bytes, 0) != 0x454c4946) return false;
        // Sector trailer signatures must agree before their original bytes can be restored.
        const WORD usaOffset = Read<WORD>(bytes, 4), usaCount = Read<WORD>(bytes, 6);
        const WORD first = Read<WORD>(bytes, 20);
        const DWORD used = Read<DWORD>(bytes, 24);
        if (usaOffset < 42 || usaOffset % 2 != 0 || usaCount != bytes.size() / 512 + 1 ||
            usaOffset + usaCount * 2 > 510 || first < usaOffset + usaCount * 2 || first % 8 != 0 ||
            used > bytes.size() || used < first + 4u || Read<DWORD>(bytes, 28) != bytes.size()) return false;
        for (WORD i = 1; i < usaCount; ++i)
            if (Read<WORD>(bytes, size_t(i) * 512 - 2) != Read<WORD>(bytes, usaOffset)) return false;
        record.number = number;
        record.sequence = Read<WORD>(bytes, 16);
        const WORD flags = Read<WORD>(bytes, 22);
        record.inUse = (flags & 1) != 0;
        record.directory = (flags & 2) != 0;
        if ((flags & ~3) != 0 || Read<ULONGLONG>(bytes, 32) != 0) return false;
        // Live files need no attributes; directory records only supply names and parent references during scans.
        if (scanning && record.inUse && !record.directory) return true;
        // Restore trailers in a private copy because subsequent raw reads reuse the source buffer.
        std::vector<BYTE> fixed(bytes.begin(), bytes.end());
        for (WORD i = 1; i < usaCount; ++i)
            std::memcpy(fixed.data() + size_t(i) * 512 - 2, bytes.data() + usaOffset + i * 2, 2);
        const auto data = std::span<const BYTE>(fixed).first(used);
        int namePriority = -1;
        bool hasData = false;
        bool ended = false;
        // Retain the preferred filename, timestamps and main data while walking bounded attributes.
        for (size_t offset = first; offset + 4 <= data.size();)
        {
            const DWORD type = Read<DWORD>(data, offset);
            if (type == 0xffffffff) { ended = true; break; }
            const DWORD length = Read<DWORD>(data, offset + 4);
            if (length < 24 || length % 8 != 0) return false;
            const auto attr = Slice(data, offset, length);
            const BYTE form = Read<BYTE>(attr, 8), nameLength = Read<BYTE>(attr, 9);
            const WORD attrFlags = Read<WORD>(attr, 12);
            const size_t headerSize = form == 0 ? 24 : ((attrFlags & 0x80ff) ? 72 : 64);
            if (form > 1 || length < headerSize) return false;
            const WORD nameOffset = Read<WORD>(attr, 10);
            if (nameLength != 0 && (nameOffset < headerSize || nameOffset % 2 != 0)) return false;
            if (nameLength != 0) ReadName(attr, nameOffset, nameLength);
            std::span<const BYTE> value;
            if (form == 0)
            {
                const WORD valueOffset = Read<WORD>(attr, 20);
                if (valueOffset < headerSize || (nameLength && valueOffset < nameOffset + nameLength * 2))
                    return false;
                value = Slice(attr, valueOffset, Read<DWORD>(attr, 16));
            }
            if (type == 0x10 && form == 0)
            {
                record.created = Read<FILETIME>(value, 0);
                record.modified = Read<FILETIME>(value, 8);
                if (Read<DWORD>(value, 32) & (FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_REPARSE_POINT))
                    record.supported = false;
            }

            // Attribute lists and reparse data need recovery paths that are not handled here.
            if (type == 0x20 || type == 0xc0) record.supported = false;
            if (type == 0x30 && form == 0)
            {
                if (Read<DWORD>(value, 56) & (FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_REPARSE_POINT))
                    record.supported = false;
                const BYTE space = Read<BYTE>(value, 65);
                if (space > 3) return false;

                // Prefer a full filename over its DOS short-name alias.
                const int priority = space == 2 ? 0 : 1;
                const auto fileName = ReadName(value, 66, Read<BYTE>(value, 64));
                if (fileName.empty()) return false;
                if (priority > namePriority)
                {
                    record.name = fileName;
                    record.parent = Read<ULONGLONG>(value, 0);
                    namePriority = priority;
                }
            }
            // Only unnamed data is recovered; additional main-data attributes are unsupported.
            if (type == 0x80 && nameLength == 0 && !(scanning && record.directory))
            {
                Stream stream;
                stream.nonresident = form != 0;
                stream.supported = form == 0 ? attrFlags == 0 : (attrFlags & ~0x8000) == 0;
                if (hasData) record.supported = false;
                hasData = true;
                if (form == 0)
                {
                    stream.resident.assign(value.begin(), value.end());
                    stream.size = stream.initialized = value.size();
                }
                else
                {
                    // Nonresident data needs a complete mapping and a valid initialized-data boundary.
                    const ULONGLONG lowest = Read<ULONGLONG>(attr, 16);
                    const ULONGLONG highest = Read<ULONGLONG>(attr, 24);
                    stream.size = Read<ULONGLONG>(attr, 48);
                    stream.initialized = Read<ULONGLONG>(attr, 56);
                    const WORD runOffset = Read<WORD>(attr, 32);
                    if (runOffset < headerSize || runOffset >= attr.size()) return false;
                    const WORD compressionUnit = Read<WORD>(attr, 34);
                    if (lowest != 0 || (compressionUnit != 0 &&
                        (!(attrFlags & 0x8000) || compressionUnit != 4))) stream.supported = false;
                    if (stream.size > static_cast<ULONGLONG>(LLONG_MAX) || stream.initialized > stream.size)
                        return false;
                    if (stream.size == 0 && highest == ULLONG_MAX)
                    {
                        if (lowest != 0 || attr[runOffset] != 0) return false;
                    }
                    else
                    {
                        stream.runs = DecodeRuns(attr.subspan(runOffset), lowest, highest, clusterCount);
                        if (highest >= ULLONG_MAX / clusterSize ||
                            stream.size > (highest + 1) * clusterSize) return false;
                    }
                    if (!(attrFlags & 0x80ff) && std::ranges::any_of(stream.runs,
                        [](const Run& run) { return run.lcn == -1; })) return false;
                }
                record.data = std::move(stream);
            }
            offset += length;
        }
        if (!ended || record.name.empty()) return false;
        if (!hasData) record.supported = false;
        if (!(scanning && record.directory)) record.snapshot = std::move(fixed);
        return true;
    }
    catch (const Failure&) { return false; }
}

std::wstring NtfsRecovery::SafeName(const std::wstring_view name)
{
    std::wstring result(name.substr(0, 160));
    if (!result.empty() && result.back() >= 0xd800 && result.back() <= 0xdbff) result.pop_back();
    for (size_t i = 0; i < result.size(); ++i)
    {
        wchar_t& c = result[i];
        if (c >= 0xd800 && c <= 0xdbff && i + 1 < result.size() &&
            result[i + 1] >= 0xdc00 && result[i + 1] <= 0xdfff) { ++i; continue; }
        if (c < 32 || (c >= 0xd800 && c <= 0xdfff) || std::wstring_view(L"<>:\"/\\|?*").contains(c))
            c = L'_';
    }
    while (!result.empty() && (result.back() == L'.' || result.back() == L' ')) result.pop_back();
    // A prefix also makes DOS device names and dot components harmless.
    return L"_" + result;
}

NtfsRecovery::OutputFile::OutputFile(const std::wstring& folder, const Record& record)
{
    ULARGE_INTEGER free = {};
    if (!GetDiskFreeSpaceExW(folder.c_str(), &free, nullptr, nullptr))
        throw Failure{ {}, GetLastError() };
    if (free.QuadPart < record.data.size) throw Failure{ {}, ERROR_DISK_FULL };
    const auto name = std::format(L"{}-{}{}", record.number, record.sequence, SafeName(record.name));
    m_path = (std::filesystem::path(folder) / name).wstring();
    m_file = CreateFileW((m_path + L".partial").c_str(), GENERIC_WRITE | DELETE,
        0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!m_file.IsValid()) throw Failure{ {}, GetLastError() };
}

NtfsRecovery::OutputFile::~OutputFile()
{
    if (m_committed) return;
    FILE_DISPOSITION_INFO info{ TRUE };
    SetFileInformationByHandle(m_file, FileDispositionInfo, &info, sizeof(info));
}

void NtfsRecovery::OutputFile::Write(const std::span<const BYTE> bytes, Progress& progress) const
{
    progress.Check();
    if (bytes.size() > MAXDWORD) throw Failure{ {}, ERROR_BUFFER_OVERFLOW };
    DWORD written = 0;
    if (!WriteFile(m_file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr))
        throw Failure{ {}, GetLastError() };
    if (written != bytes.size()) throw Failure{ {}, ERROR_WRITE_FAULT };
}

void NtfsRecovery::OutputFile::Commit(const std::function<void()>& check,
    const FILETIME* created, const FILETIME* modified)
{
    check();
    if (((created || modified) && !SetFileTime(m_file, created, nullptr, modified)) || !FlushFileBuffers(m_file))
        throw Failure{ {}, GetLastError() };
    check();

    // Rename the open temporary file without replacing an existing destination.
    const auto size = offsetof(FILE_RENAME_INFO, FileName) + (m_path.size() + 1) * sizeof(wchar_t);
    std::vector<BYTE> bytes(size, 0);
    auto& rename = *reinterpret_cast<FILE_RENAME_INFO*>(bytes.data());
    rename.FileNameLength = static_cast<DWORD>(m_path.size() * sizeof(wchar_t));
    std::memcpy(rename.FileName, m_path.data(), rename.FileNameLength);
    if (!SetFileInformationByHandle(m_file, FileRenameInfo, &rename, static_cast<DWORD>(size)))
        throw Failure{ {}, GetLastError() };
    m_committed = true;
}

NtfsRecovery::NtfsRecovery(const std::wstring& root, Progress* progress)
{
    if (progress) progress->Check();
    wchar_t volume[MAX_PATH] = {};
    if (!GetVolumeNameForVolumeMountPointW(root.c_str(), volume, MAX_PATH))
        throw Failure{ {}, GetLastError() };
    m_name = volume;
    wchar_t filesystem[MAX_PATH] = {};
    if (!GetVolumeInformationW(volume, nullptr, 0, nullptr, nullptr, nullptr, filesystem, MAX_PATH))
        throw Failure{ {}, GetLastError() };
    if (_wcsicmp(filesystem, L"exFAT") == 0)
    {
        m_exfat = std::make_unique<ExFatRecovery>(m_name, progress);
        return;
    }
    if (_wcsicmp(filesystem, L"NTFS") != 0) throw Failure{ {}, ERROR_NOT_SUPPORTED };
    m_volume = CreateFileW(m_name.substr(0, m_name.size() - 1).c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING, nullptr);
    if (!m_volume.IsValid()) throw Failure{ {}, GetLastError() };
    struct { NTFS_VOLUME_DATA_BUFFER info; NTFS_EXTENDED_VOLUME_DATA extended; } data = {};
    DWORD returned = 0;
    if (!DeviceIoControl(m_volume, FSCTL_GET_NTFS_VOLUME_DATA, nullptr, 0, &data, sizeof(data),
        &returned, nullptr)) throw Failure{ {}, GetLastError() };

    // Validate geometry before using it to calculate raw offsets and allocation sizes.
    m_info = data.info;
    if (returned < sizeof(NTFS_VOLUME_DATA_BUFFER) + 8 ||
        data.extended.MajorVersion != 3 || data.extended.MinorVersion > 1 ||
        !std::has_single_bit(m_info.BytesPerSector) || m_info.BytesPerSector < 512 ||
        m_info.BytesPerSector > 65536 || !std::has_single_bit(m_info.BytesPerCluster) ||
        m_info.BytesPerCluster < m_info.BytesPerSector || m_info.BytesPerCluster > 2 * 1024 * 1024 ||
        !std::has_single_bit(m_info.BytesPerFileRecordSegment) || m_info.BytesPerFileRecordSegment < 512 ||
        m_info.BytesPerFileRecordSegment > 65536 || m_info.TotalClusters.QuadPart <= 0 ||
        m_info.TotalClusters.QuadPart > LLONG_MAX / m_info.BytesPerCluster ||
        m_info.MftValidDataLength.QuadPart <= 0 ||
        m_info.MftValidDataLength.QuadPart % m_info.BytesPerFileRecordSegment != 0)
        throw Failure{ {}, ERROR_INVALID_DATA };
    m_mft = CreateFileW((m_name + L"$MFT::$DATA").c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (!m_mft.IsValid()) throw Failure{ {}, GetLastError() };
    m_mftRuns = GetMftRuns();
    if (progress) progress->Check();
}

NtfsRecovery::~NtfsRecovery() = default;

std::vector<NtfsRecovery::Run> NtfsRecovery::GetMftRuns() const
{
    std::vector<Run> runs;
    std::vector<BYTE> buffer(64 * 1024);
    // Ignore growth beyond the original scan range while retaining every cluster that supplies its bytes.
    const auto clusters = (static_cast<ULONGLONG>(m_info.MftValidDataLength.QuadPart) - 1) /
        m_info.BytesPerCluster + 1;
    STARTING_VCN_INPUT_BUFFER input = {};
    for (;;)
    {
        DWORD returned = 0;
        const bool done = DeviceIoControl(m_mft, FSCTL_GET_RETRIEVAL_POINTERS, &input, sizeof(input),
            buffer.data(), static_cast<DWORD>(buffer.size()), &returned, nullptr) != 0;
        if (!done && GetLastError() != ERROR_MORE_DATA)
            throw Failure{ {}, GetLastError() };
        if (returned > buffer.size()) throw Failure{ {}, ERROR_INVALID_DATA };
        const auto data = std::span<const BYTE>(buffer).first(returned);
        const auto count = Read<DWORD>(data, 0);
        auto vcn = Read<LONGLONG>(data, 8);
        if (vcn != input.StartingVcn.QuadPart || count == 0 || count > (data.size() - 16) / 16)
            throw Failure{ {}, ERROR_INVALID_DATA };
        for (DWORD i = 0; i < count; ++i)
        {
            const auto next = Read<LONGLONG>(data, 16 + size_t(i) * 16);
            const auto lcn = Read<LONGLONG>(data, 24 + size_t(i) * 16);
            if (next <= vcn || lcn < 0 || lcn >= m_info.TotalClusters.QuadPart ||
                next - vcn > m_info.TotalClusters.QuadPart - lcn)
                throw Failure{ {}, ERROR_INVALID_DATA };
            const auto take = std::min(static_cast<ULONGLONG>(next - vcn), clusters - vcn);
            if (!runs.empty() && runs.back().lcn + runs.back().count == static_cast<ULONGLONG>(lcn))
                runs.back().count += take;
            else runs.push_back({ static_cast<ULONGLONG>(vcn), lcn, take });
            vcn += take;
            if (static_cast<ULONGLONG>(vcn) == clusters) return runs;
        }
        if (done) throw Failure{ {}, ERROR_INVALID_DATA };

        // Continue from the last extent when the mapping spans multiple responses.
        input.StartingVcn.QuadPart = vcn;
    }
}

std::span<const BYTE> NtfsRecovery::ReadAt(const ULONGLONG offset, const DWORD length) const
{
    const ULONGLONG volumeSize = m_info.TotalClusters.QuadPart * ULONGLONG(m_info.BytesPerCluster);
    if (length == 0 || offset >= volumeSize || length > volumeSize - offset)
        throw Failure{ {}, ERROR_INVALID_DATA };
    const DWORD prefix = static_cast<DWORD>(offset % m_info.BytesPerSector);
    const ULONGLONG alignedLength = (ULONGLONG(prefix) + length + m_info.BytesPerSector - 1) &
        ~ULONGLONG(m_info.BytesPerSector - 1);
    if (alignedLength > MAXDWORD) throw Failure{ {}, ERROR_INVALID_DATA };
    if (alignedLength > m_bufferSize)
    {
        m_buffer = _aligned_malloc(static_cast<size_t>(alignedLength), 65536);
        m_bufferSize = m_buffer.IsValid() ? static_cast<size_t>(alignedLength) : 0;
    }
    if (!m_buffer.IsValid()) throw Failure{ {}, ERROR_NOT_ENOUGH_MEMORY };
    LARGE_INTEGER position{ .QuadPart = static_cast<LONGLONG>(offset - prefix) };
    DWORD returned = 0;
    if (!SetFilePointerEx(m_volume, position, nullptr, FILE_BEGIN) ||
        !ReadFile(m_volume, m_buffer.Get(), static_cast<DWORD>(alignedLength), &returned, nullptr))
        throw Failure{ {}, GetLastError() };
    if (returned != alignedLength) throw Failure{ {}, ERROR_HANDLE_EOF };
    const auto bytes = static_cast<const BYTE*>(m_buffer.Get()) + prefix;
    return { bytes, length };
}

std::span<const BYTE> NtfsRecovery::ReadMft(ULONGLONG offset, const DWORD length) const
{
    if (offset > static_cast<ULONGLONG>(m_info.MftValidDataLength.QuadPart) ||
        length > static_cast<ULONGLONG>(m_info.MftValidDataLength.QuadPart) - offset)
        throw Failure{ {}, ERROR_INVALID_DATA };
    auto& result = m_mftBuffer;
    result.clear();
    while (result.size() < length)
    {
        const auto vcn = offset / m_info.BytesPerCluster;

        // Locate the physical extent containing this logical MFT position.
        const auto run = std::ranges::upper_bound(m_mftRuns, vcn, {}, &Run::vcn);
        if (run == m_mftRuns.begin()) throw Failure{ {}, ERROR_INVALID_DATA };
        const auto& extent = *std::prev(run);
        if (vcn >= extent.vcn + extent.count) throw Failure{ {}, ERROR_INVALID_DATA };
        const auto within = offset - extent.vcn * m_info.BytesPerCluster;
        const auto take = static_cast<DWORD>(std::min<ULONGLONG>(length - result.size(),
            extent.count * m_info.BytesPerCluster - within));
        auto bytes = ReadAt(extent.lcn * ULONGLONG(m_info.BytesPerCluster) + within, take);
        if (take == length) return bytes;
        if (result.empty()) result.reserve(length);
        result.insert(result.end(), bytes.begin(), bytes.end());
        offset += take;
    }
    return result;
}

bool NtfsRecovery::BitmapFree(const std::span<const BYTE> bytes, ULONGLONG offset, ULONGLONG count)
{
    if (offset > ULONGLONG(bytes.size()) * 8 || count > ULONGLONG(bytes.size()) * 8 - offset)
        throw Failure{ {}, ERROR_INVALID_DATA };
    while (count != 0 && offset % 8 != 0)
    {
        if (bytes[static_cast<size_t>(offset / 8)] & (1 << (offset % 8))) return false;
        ++offset;
        --count;
    }
    const auto full = bytes.subspan(static_cast<size_t>(offset / 8), static_cast<size_t>(count / 8));
    if (std::ranges::any_of(full, [](const BYTE byte) { return byte != 0; })) return false;
    offset += (count / 8) * 8;
    count %= 8;
    return count == 0 || (bytes[static_cast<size_t>(offset / 8)] & ((1 << count) - 1)) == 0;
}

// Reuse allocation pages only when explicitly requested by scanning; recovery always queries fresh pages.
bool NtfsRecovery::ClustersFree(ULONGLONG lcn, ULONGLONG count, Progress& progress,
    std::vector<BYTE>* const cached) const
{
    if (lcn >= static_cast<ULONGLONG>(m_info.TotalClusters.QuadPart) ||
        count > static_cast<ULONGLONG>(m_info.TotalClusters.QuadPart) - lcn)
        throw Failure{ {}, ERROR_INVALID_DATA };
    std::vector<BYTE> fresh;
    auto& buffer = cached == nullptr ? fresh : *cached;
    while (count != 0)
    {
        progress.Check();
        auto data = std::span<const BYTE>(buffer);
        auto start = data.empty() ? 0 : Read<ULONGLONG>(data, 0);
        auto bits = data.empty() ? 0 : std::min<ULONGLONG>(Read<ULONGLONG>(data, 8), (data.size() - 16) * 8);
        if (start > lcn || lcn - start >= bits)
        {
            buffer.resize(64 * 1024);
            STARTING_LCN_INPUT_BUFFER input{ .StartingLcn = { .QuadPart = static_cast<LONGLONG>(lcn) } };
            DWORD returned = 0;
            if (!DeviceIoControl(m_volume, FSCTL_GET_VOLUME_BITMAP, &input, sizeof(input), buffer.data(),
                static_cast<DWORD>(buffer.size()), &returned, nullptr) && GetLastError() != ERROR_MORE_DATA)
                throw Failure{ {}, GetLastError() };
            if (returned < 16 || returned > buffer.size()) throw Failure{ {}, ERROR_INVALID_DATA };
            buffer.resize(returned);
            data = buffer;

            // Windows may round the bitmap start down; use the returned origin.
            start = Read<ULONGLONG>(data, 0);
            bits = std::min<ULONGLONG>(Read<ULONGLONG>(data, 8), (data.size() - 16) * 8);
            if (start > lcn || lcn - start >= bits) throw Failure{ {}, ERROR_INVALID_DATA };
        }
        const auto take = std::min(count, bits - (lcn - start));
        if (!BitmapFree(data.subspan(16), lcn - start, take)) return false;
        lcn += take;
        count -= take;
    }
    return true;
}

void NtfsRecovery::Scan(Progress& progress, ScanResult& result,
    const std::function<void(const Record&)>& discovered)
{
    result = {};
    if (m_exfat) return m_exfat->Scan(progress, result, discovered);
    // Scan assessments may use a bounded cache; every recovery allocation check reads a fresh bitmap.
    std::vector<BYTE> bitmap;
    struct Parent { ULONGLONG parent; USHORT sequence; std::wstring name; };
    std::map<ULONGLONG, Parent> parents;
    const auto total = static_cast<ULONGLONG>(m_info.MftValidDataLength.QuadPart);
    // Read the MFT in batches, retaining directory ancestry and usable deleted-file candidates.
    for (ULONGLONG offset = 0; offset < total;)
    {
        progress.Check();
        const auto take = static_cast<DWORD>(std::min<ULONGLONG>(4 * 1024 * 1024, total - offset));
        const auto buffer = ReadMft(offset, take);
        for (DWORD within = 0; within < take; within += m_info.BytesPerFileRecordSegment)
        {
            progress.Check();
            const auto number = (offset + within) / m_info.BytesPerFileRecordSegment;
            Record record;
            const auto raw = std::span<const BYTE>(buffer).subspan(within, m_info.BytesPerFileRecordSegment);
            if (Read<DWORD>(raw, 0) == 0) continue;
            if (!ParseRecord(raw, number, m_info.BytesPerCluster, m_info.TotalClusters.QuadPart, record, true))
            {
                ++result.invalidRecords;
                continue;
            }
            // Parent sequence numbers prevent paths from following directory records that have been reused.
            if (record.directory)
            {
                parents.emplace(number, Parent{ record.parent, record.sequence, std::move(record.name) });
                continue;
            }
            // Only retain usable deleted-file candidates.
            if (record.inUse || number < 16 || !record.supported || !record.data.supported) continue;
            if (std::ranges::any_of(record.data.runs, [&](const Run& run)
            {
                return run.lcn >= 0 && !ClustersFree(run.lcn, run.count, progress, &bitmap);
            })) continue;
            record.condition = record.data.nonresident ? Condition::Unallocated : Condition::Resident;

            // Publish a provisional path while later records may still supply its parents.
            record.path = L"?\\" + record.name;
            result.records.push_back(std::move(record));
            if (discovered) discovered(result.records.back());
        }
        offset += take;
    }
    if (GetMftRuns() != m_mftRuns) throw Failure{ L"IDS_RECOVERY_CHANGED" };
    // Reconstruct paths after the scan; missing, reused or cyclic ancestry stays visibly uncertain.
    for (auto& record : result.records)
    {
        progress.Check();
        std::wstring path = record.name;
        ULONGLONG reference = record.parent;
        std::set<ULONGLONG> seen;
        while ((reference & 0xffffffffffffull) != 5)
        {
            progress.Check();
            const auto number = reference & 0xffffffffffffull;
            const auto parent = parents.find(number);
            if (parent == parents.end() || parent->second.sequence != (reference >> 48) ||
                !seen.insert(number).second)
            {
                path = L"?\\" + path;
                break;
            }
            path = parent->second.name + L"\\" + path;
            reference = parent->second.parent;
        }
        record.path = std::move(path);
    }
}

void NtfsRecovery::ValidateRecord(const Record& record) const
{
    if (record.number >= static_cast<ULONGLONG>(m_info.MftValidDataLength.QuadPart) /
        m_info.BytesPerFileRecordSegment) throw Failure{ L"IDS_RECOVERY_CHANGED" };
    if (GetMftRuns() != m_mftRuns) throw Failure{ L"IDS_RECOVERY_CHANGED" };
    Record fresh;
    const auto bytes = ReadMft(record.number * m_info.BytesPerFileRecordSegment, m_info.BytesPerFileRecordSegment);
    if (!ParseRecord(bytes, record.number, m_info.BytesPerCluster, m_info.TotalClusters.QuadPart, fresh) ||
        fresh.inUse || fresh.snapshot != record.snapshot) throw Failure{ L"IDS_RECOVERY_CHANGED" };
}

std::wstring NtfsRecovery::ValidateDestination(const std::wstring& folder) const
{
    const Handle directory(CloseHandle, CreateFileW(folder.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (!directory.IsValid()) throw Failure{ {}, GetLastError() };
    std::vector<wchar_t> path(32768);

    // Resolve aliases to a volume GUID so same-volume confirmation uses the actual destination.
    DWORD size = GetFinalPathNameByHandleW(directory, path.data(), static_cast<DWORD>(path.size()),
        FILE_NAME_NORMALIZED | VOLUME_NAME_GUID);
    // Network shares have no local volume GUID; DOS naming resolves mapped drives to canonical UNC paths.
    if (size == 0) size = GetFinalPathNameByHandleW(directory, path.data(), static_cast<DWORD>(path.size()),
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (size == 0) throw Failure{ {}, GetLastError() };
    if (size >= path.size()) throw Failure{ {}, ERROR_FILENAME_EXCED_RANGE };
    std::wstring canonical(path.data(), size);
    const auto end = canonical.find(L'}');
    if ((!canonical.starts_with(L"\\\\?\\Volume{") || end == std::wstring::npos) &&
        !canonical.starts_with(L"\\\\?\\UNC\\")) throw Failure{ {}, ERROR_DIRECTORY };
    BY_HANDLE_FILE_INFORMATION info = {};
    if (!GetFileInformationByHandle(directory, &info)) throw Failure{ {}, GetLastError() };
    if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) throw Failure{ {}, ERROR_DIRECTORY };
    if (canonical.back() != L'\\') canonical += L'\\';
    return canonical;
}

std::wstring NtfsRecovery::Recover(const Record& record, const std::wstring& folder, Progress& progress)
{
    progress.Check();
    if (m_exfat) return m_exfat->Recover(record, ValidateDestination(folder), progress);
    if (record.inUse || record.directory || !record.supported || !record.data.supported)
        throw Failure{ {}, ERROR_INVALID_DATA };
    ValidateRecord(record);
    const auto destination = ValidateDestination(folder);
    const auto& stream = record.data;
    progress.Check();
    for (const auto& run : stream.runs)
        if (run.lcn >= 0 && !ClustersFree(run.lcn, run.count, progress))
            throw Failure{ L"IDS_RECOVERY_CHANGED" };
    OutputFile output(destination, record);
    // Resident bytes are already captured; nonresident extents are copied in bounded chunks.
    if (!stream.nonresident) output.Write(stream.resident, progress);
    else
    {
        ULONGLONG position = 0;
        for (const auto& run : stream.runs)
        {
            const auto runSize = std::min(run.count * m_info.BytesPerCluster, stream.size - position);
            for (ULONGLONG within = 0; within < runSize;)
            {
                progress.Check();
                const DWORD take = static_cast<DWORD>(std::min<ULONGLONG>(1024 * 1024, runSize - within));
                DWORD initialized = 0;
                if (run.lcn >= 0 && position < stream.initialized)
                {
                    initialized = static_cast<DWORD>(std::min<ULONGLONG>(take,
                        stream.initialized - position));
                    const auto offset = run.lcn * ULONGLONG(m_info.BytesPerCluster) + within;
                    const auto firstCluster = offset / m_info.BytesPerCluster;
                    const auto clusters = (offset % m_info.BytesPerCluster + initialized +
                        m_info.BytesPerCluster - 1) / m_info.BytesPerCluster;
                    // Check allocation on both sides of each raw read to detect concurrent reuse.
                    if (!ClustersFree(firstCluster, clusters, progress))
                        throw Failure{ L"IDS_RECOVERY_CHANGED" };
                    auto source = ReadAt(offset, initialized);
                    if (!ClustersFree(firstCluster, clusters, progress))
                        throw Failure{ L"IDS_RECOVERY_CHANGED" };
                    output.Write(source, progress);
                }
                // Sparse holes and bytes beyond initialized data must remain zero-filled.
                if (initialized < take) output.Write(std::vector<BYTE>(take - initialized, 0), progress);
                within += take;
                position += take;
            }
            if (position == stream.size) break;
        }
        if (position != stream.size) throw Failure{ {}, ERROR_INVALID_DATA };
    }
    // Revalidate the completed copy before its temporary file can acquire the final output name.
    ValidateRecord(record);
    for (const auto& run : stream.runs)
        if (run.lcn >= 0 && !ClustersFree(run.lcn, run.count, progress))
            throw Failure{ L"IDS_RECOVERY_CHANGED" };
    output.Commit([&progress] { progress.Check(); }, &record.created, &record.modified);
    return output.Path();
}
