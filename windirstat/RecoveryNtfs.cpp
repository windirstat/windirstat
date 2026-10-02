// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "RecoveryNtfs.h"

enum class NtfsAttributeType : DWORD
{
    StandardInformation = 0x10, AttributeList = 0x20, FileName = 0x30,
    Data = 0x80, ReparsePoint = 0xC0, End = 0xFFFFFFFF
};
enum class NtfsAttributeForm : BYTE { Resident = 0, Nonresident = 1 };
enum class NtfsNameSpace : BYTE { Posix = 0, Win32 = 1, Dos = 2, Win32AndDos = 3 };

#pragma pack(push, 1)
struct NtfsRecordHeader
{
    DWORD signature;
    WORD updateSequenceOffset;
    WORD updateSequenceCount;
    ULONGLONG logSequence;
    WORD sequence;
    WORD linkCount;
    WORD firstAttributeOffset;
    WORD flags;
    DWORD usedBytes;
    DWORD allocatedBytes;
    ULONGLONG baseRecord;
    WORD nextAttributeId;
};

struct NtfsAttributeHeader
{
    NtfsAttributeType type;
    DWORD length;
    NtfsAttributeForm form;
    BYTE nameLength;
    WORD nameOffset;
    WORD flags;
    WORD instance;
};

struct NtfsResidentAttribute
{
    NtfsAttributeHeader header;
    DWORD valueLength;
    WORD valueOffset;
    BYTE indexed;
    BYTE reserved;
};

struct NtfsNonresidentAttribute
{
    NtfsAttributeHeader header;
    ULONGLONG lowestVcn;
    ULONGLONG highestVcn;
    WORD mappingPairsOffset;
    WORD compressionUnit;
    DWORD reserved;
    ULONGLONG allocatedLength;
    ULONGLONG fileSize;
    ULONGLONG validDataLength;

    // Sparse/compressed attributes append an additional ULONGLONG totalAllocated field.
};

struct NtfsStandardInformation
{
    FILETIME created;
    FILETIME modified;
    FILETIME recordChanged;
    FILETIME accessed;
    DWORD attributes;

    // Only the fixed prefix used by recovery is required here.
};

struct NtfsFileName
{
    ULONGLONG parent;
    FILETIME created;
    FILETIME modified;
    FILETIME recordChanged;
    FILETIME accessed;
    ULONGLONG allocatedLength;
    ULONGLONG fileSize;
    DWORD attributes;
    DWORD reparseTag;
    BYTE nameLength;
    NtfsNameSpace nameSpace;

    // Followed by nameLength UTF-16 code units.
};
#pragma pack(pop)

static_assert(sizeof(NtfsRecordHeader) == 42 && offsetof(NtfsRecordHeader, baseRecord) == 32);
static_assert(sizeof(NtfsAttributeHeader) == 16 && sizeof(NtfsResidentAttribute) == 24);
static_assert(sizeof(NtfsNonresidentAttribute) == 64 && offsetof(NtfsNonresidentAttribute, fileSize) == 48);
static_assert(sizeof(NtfsStandardInformation) == 36 && sizeof(NtfsFileName) == 66);
constexpr DWORD NtfsFileSignature = 0x454C4946; // "FILE" in little-endian byte order.
constexpr WORD NtfsRecordInUse = 0x0001;
constexpr WORD NtfsRecordDirectory = 0x0002;
constexpr WORD NtfsCompressionMask = 0x00FF;
constexpr WORD NtfsSparseFlag = 0x8000;
constexpr WORD NtfsStandardCompressionShift = 4; // Sixteen clusters per compression unit.
constexpr DWORD NtfsFixupStride = 512; // Update-sequence protection uses 512-byte units, even on larger sectors.
constexpr DWORD NtfsMaxRecordBytes = 64 * 1024;
constexpr DWORD NtfsMaxClusterBytes = 2 * 1024 * 1024;
constexpr DWORD NtfsControlBufferBytes = 64 * 1024;
constexpr DWORD NtfsScanBatchBytes = 4 * 1024 * 1024;
constexpr DWORD NtfsAttributeAlignment = 8;
constexpr ULONGLONG NtfsRootRecord = 5;
constexpr ULONGLONG NtfsFirstUserRecord = 16;
constexpr ULONGLONG NtfsRecordNumberMask = 0x0000FFFFFFFFFFFFull;
constexpr unsigned NtfsSequenceShift = 48;

std::vector<NtfsRecovery::Run> NtfsRecovery::DecodeRuns(const std::span<const BYTE> bytes,
    const ULONGLONG lowestVcn, const ULONGLONG highestVcn, const ULONGLONG clusterCount)
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
            std::erase_if(physical, [](const Run& run)
            {
                // Ignore sparse holes when checking physical overlap.
                return run.lcn < 0;
            });
            std::ranges::sort(physical, {}, &Run::lcn);
            for (size_t i = 1; i < physical.size(); ++i)
                if (static_cast<ULONGLONG>(physical[i].lcn - physical[i - 1].lcn) < physical[i - 1].count)
                    throw Failure{ {}, ERROR_INVALID_DATA };
            return runs;
        }

        // The low nibble gives the run-length width; the high nibble gives the signed LCN-delta width.
        const unsigned countBytes = header & 0x0F, deltaBytes = header >> 4;
        if (countBytes == 0 || countBytes > sizeof(ULONGLONG) || deltaBytes > sizeof(LONGLONG) ||
            countBytes + deltaBytes > bytes.size() - offset) throw Failure{ {}, ERROR_INVALID_DATA };
        ULONGLONG count = 0, deltaBits = 0;
        for (unsigned i = 0; i < countBytes; ++i) count |= ULONGLONG(bytes[offset++]) << (8 * i);
        if (count == 0 || count > highestVcn + 1 - vcn) throw Failure{ {}, ERROR_INVALID_DATA };
        for (unsigned i = 0; i < deltaBytes; ++i) deltaBits |= ULONGLONG(bytes[offset++]) << (8 * i);
        if (deltaBytes != 0)
        {
            // Sign-extend relative offsets before advancing the physical cluster position.
            if (deltaBytes < sizeof(LONGLONG) && (deltaBits & (1ull << (deltaBytes * 8 - 1))))
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
        if (bytes.size() < NtfsFixupStride || bytes.size() > NtfsMaxRecordBytes ||
            bytes.size() % NtfsFixupStride != 0 || clusterSize == 0) return false;
        const auto header = Read<NtfsRecordHeader>(bytes, 0);
        if (header.signature != NtfsFileSignature) return false;

        // Sector trailer signatures must agree before their original bytes can be restored.
        const WORD usaOffset = header.updateSequenceOffset, usaCount = header.updateSequenceCount;
        const WORD first = header.firstAttributeOffset;
        const DWORD used = header.usedBytes;
        if (usaOffset < sizeof(header) || usaOffset % sizeof(WORD) != 0 ||
            usaCount != bytes.size() / NtfsFixupStride + 1 ||
            usaOffset + usaCount * sizeof(WORD) > NtfsFixupStride - sizeof(WORD) ||
            first < usaOffset + usaCount * sizeof(WORD) || first % NtfsAttributeAlignment != 0 ||
            used > bytes.size() || used < first + sizeof(DWORD) || header.allocatedBytes != bytes.size()) return false;
        for (WORD i = 1; i < usaCount; ++i)
            if (Read<WORD>(bytes, size_t(i) * NtfsFixupStride - sizeof(WORD)) !=
                Read<WORD>(bytes, usaOffset)) return false;
        record.number = number;
        record.sequence = header.sequence;
        const WORD flags = header.flags;
        record.inUse = (flags & NtfsRecordInUse) != 0;
        record.directory = (flags & NtfsRecordDirectory) != 0;
        if ((flags & ~(NtfsRecordInUse | NtfsRecordDirectory)) != 0 || header.baseRecord != 0) return false;

        // Live files need no attributes; directory records only supply names and parent references during scans.
        if (scanning && record.inUse && !record.directory) return true;

        // Restore trailers in a private copy because subsequent raw reads reuse the source buffer.
        std::vector<BYTE> fixed(bytes.begin(), bytes.end());
        for (WORD i = 1; i < usaCount; ++i)
            std::memcpy(fixed.data() + size_t(i) * NtfsFixupStride - sizeof(WORD),
                bytes.data() + usaOffset + i * sizeof(WORD), sizeof(WORD));
        const auto data = std::span<const BYTE>(fixed).first(used);
        int namePriority = -1;
        bool hasData = false;
        bool ended = false;

        // Retain the preferred filename, timestamps and main data while walking bounded attributes.
        for (size_t offset = first; offset + sizeof(DWORD) <= data.size();)
        {
            const auto type = Read<NtfsAttributeType>(data, offset);
            if (type == NtfsAttributeType::End) { ended = true; break; }
            const auto attribute = Read<NtfsAttributeHeader>(data, offset);
            const DWORD length = attribute.length;
            if (length < sizeof(NtfsResidentAttribute) || length % NtfsAttributeAlignment != 0) return false;
            const auto attr = Slice(data, offset, length);
            const auto form = attribute.form;
            const BYTE nameLength = attribute.nameLength;
            const WORD attrFlags = attribute.flags;
            const size_t headerSize = form == NtfsAttributeForm::Resident ? sizeof(NtfsResidentAttribute) :
                sizeof(NtfsNonresidentAttribute) + ((attrFlags & (NtfsSparseFlag | NtfsCompressionMask)) ?
                    sizeof(ULONGLONG) : 0);
            if (form > NtfsAttributeForm::Nonresident || length < headerSize) return false;
            const WORD nameOffset = attribute.nameOffset;
            if (nameLength != 0 && (nameOffset < headerSize || nameOffset % sizeof(wchar_t) != 0)) return false;
            if (nameLength != 0) ReadName(attr, nameOffset, nameLength);
            std::span<const BYTE> value;
            if (form == NtfsAttributeForm::Resident)
            {
                const auto resident = Read<NtfsResidentAttribute>(attr, 0);
                if (resident.valueOffset < headerSize || (nameLength &&
                    resident.valueOffset < nameOffset + nameLength * sizeof(wchar_t))) return false;
                value = Slice(attr, resident.valueOffset, resident.valueLength);
            }
            if (type == NtfsAttributeType::StandardInformation && form == NtfsAttributeForm::Resident)
            {
                const auto information = Read<NtfsStandardInformation>(value, 0);
                record.created = information.created;
                record.modified = information.modified;
                if (information.attributes & (FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_REPARSE_POINT))
                    record.supported = false;
            }

            // Attribute lists and reparse data need recovery paths that are not handled here.
            if (type == NtfsAttributeType::AttributeList || type == NtfsAttributeType::ReparsePoint)
                record.supported = false;
            if (type == NtfsAttributeType::FileName && form == NtfsAttributeForm::Resident)
            {
                const auto filename = Read<NtfsFileName>(value, 0);
                if (filename.attributes & (FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_REPARSE_POINT))
                    record.supported = false;
                const auto space = filename.nameSpace;
                if (space > NtfsNameSpace::Win32AndDos) return false;

                // Prefer a full filename over its DOS short-name alias.
                const int priority = space == NtfsNameSpace::Dos ? 0 : 1;
                const auto fileName = ReadName(value, sizeof(filename), filename.nameLength);
                if (fileName.empty()) return false;
                if (priority > namePriority)
                {
                    record.name = fileName;
                    record.parent = filename.parent;
                    namePriority = priority;
                }
            }

            // Only unnamed data is recovered; additional main-data attributes are unsupported.
            if (type == NtfsAttributeType::Data && nameLength == 0 && !(scanning && record.directory))
            {
                Stream stream;
                stream.nonresident = form != NtfsAttributeForm::Resident;
                stream.supported = form == NtfsAttributeForm::Resident ?
                    attrFlags == 0 : (attrFlags & ~NtfsSparseFlag) == 0;
                if (hasData) record.supported = false;
                hasData = true;
                if (form == NtfsAttributeForm::Resident)
                {
                    stream.resident.assign(value.begin(), value.end());
                    stream.size = stream.initialized = value.size();
                }
                else
                {
                    // Nonresident data needs a complete mapping and a valid initialized-data boundary.
                    const auto nonresident = Read<NtfsNonresidentAttribute>(attr, 0);
                    const ULONGLONG lowest = nonresident.lowestVcn, highest = nonresident.highestVcn;
                    stream.size = nonresident.fileSize;
                    stream.initialized = nonresident.validDataLength;
                    const WORD runOffset = nonresident.mappingPairsOffset;
                    if (runOffset < headerSize || runOffset >= attr.size()) return false;
                    const WORD compressionUnit = nonresident.compressionUnit;
                    if (lowest != 0 || (compressionUnit != 0 &&
                        (!(attrFlags & NtfsSparseFlag) || compressionUnit != NtfsStandardCompressionShift)))
                        stream.supported = false;
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
                    if (!(attrFlags & (NtfsSparseFlag | NtfsCompressionMask)) &&
                        std::ranges::any_of(stream.runs, [](const Run& run)
                    {
                        // Detect sparse holes that require the sparse or compression attribute flags.
                        return run.lcn == -1;
                    })) return false;
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

NtfsRecovery::NtfsRecovery(const std::wstring& volumeName, Progress* progress) : RecoveryShared(volumeName)
{
    struct { NTFS_VOLUME_DATA_BUFFER info; NTFS_EXTENDED_VOLUME_DATA extended; } data = {};
    DWORD returned = 0;
    if (!DeviceIoControl(m_volume, FSCTL_GET_NTFS_VOLUME_DATA, nullptr, 0, &data, sizeof(data),
        &returned, nullptr)) throw Failure{ {}, GetLastError() };

    // Validate geometry before using it to calculate raw offsets and allocation sizes.
    m_info = data.info;
    if (returned < sizeof(NTFS_VOLUME_DATA_BUFFER) +
        offsetof(NTFS_EXTENDED_VOLUME_DATA, MinorVersion) + sizeof(WORD) ||
        data.extended.MajorVersion != 3 || data.extended.MinorVersion > 1 ||
        !std::has_single_bit(m_info.BytesPerSector) || m_info.BytesPerSector < NtfsFixupStride ||
        m_info.BytesPerSector > RawBufferAlignment || !std::has_single_bit(m_info.BytesPerCluster) ||
        m_info.BytesPerCluster < m_info.BytesPerSector || m_info.BytesPerCluster > NtfsMaxClusterBytes ||
        !std::has_single_bit(m_info.BytesPerFileRecordSegment) || m_info.BytesPerFileRecordSegment < NtfsFixupStride ||
        m_info.BytesPerFileRecordSegment > NtfsMaxRecordBytes || m_info.TotalClusters.QuadPart <= 0 ||
        m_info.TotalClusters.QuadPart > LLONG_MAX / m_info.BytesPerCluster ||
        m_info.MftValidDataLength.QuadPart <= 0 ||
        m_info.MftValidDataLength.QuadPart % m_info.BytesPerFileRecordSegment != 0)
        throw Failure{ {}, ERROR_INVALID_DATA };
    m_mft = CreateFileW((volumeName + L"$MFT::$DATA").c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (!m_mft.IsValid()) throw Failure{ {}, GetLastError() };
    m_mftRuns = GetMftRuns();
    if (progress) progress->Check();
}

std::vector<NtfsRecovery::Run> NtfsRecovery::GetMftRuns() const
{
    using Extent = std::remove_extent_t<decltype(RETRIEVAL_POINTERS_BUFFER::Extents)>;
    constexpr size_t headerSize = offsetof(RETRIEVAL_POINTERS_BUFFER, Extents);
    std::vector<Run> runs;
    std::vector<BYTE> buffer(NtfsControlBufferBytes);

    // Ignore growth beyond the original scan range while retaining every cluster that supplies its bytes.
    const auto clusters = (static_cast<ULONGLONG>(m_info.MftValidDataLength.QuadPart) - 1) /
        m_info.BytesPerCluster + 1;
    STARTING_VCN_INPUT_BUFFER input = {};
    for (;;)
    {
        DWORD returned = 0;
        const bool done = DeviceIoControl(m_mft, FSCTL_GET_RETRIEVAL_POINTERS, &input, sizeof(input),
            buffer.data(), static_cast<DWORD>(buffer.size()), &returned, nullptr) != 0;
        if (!done && GetLastError() != ERROR_MORE_DATA) throw Failure{ {}, GetLastError() };
        if (returned > buffer.size()) throw Failure{ {}, ERROR_INVALID_DATA };
        const auto data = std::span<const BYTE>(buffer).first(returned);

        // Validate each returned extent before appending its physical clusters to the MFT mapping.
        const auto mapping = Read<RETRIEVAL_POINTERS_BUFFER>(data, 0);
        auto vcn = mapping.StartingVcn.QuadPart;
        if (vcn != input.StartingVcn.QuadPart || mapping.ExtentCount == 0 ||
            mapping.ExtentCount > (data.size() - headerSize) / sizeof(Extent))
            throw Failure{ {}, ERROR_INVALID_DATA };
        for (DWORD i = 0; i < mapping.ExtentCount; ++i)
        {
            const auto extent = Read<Extent>(data, headerSize + size_t(i) * sizeof(Extent));
            const auto next = extent.NextVcn.QuadPart, lcn = extent.Lcn.QuadPart;
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
    if (length == 0 || offset >= volumeSize || length > volumeSize - offset) throw Failure{ {}, ERROR_INVALID_DATA };

    // Align raw reads to full sectors while retaining the requested view's prefix.
    const DWORD prefix = static_cast<DWORD>(offset % m_info.BytesPerSector);
    const ULONGLONG alignedLength = (ULONGLONG(prefix) + length + m_info.BytesPerSector - 1) &
        ~ULONGLONG(m_info.BytesPerSector - 1);
    if (alignedLength > MAXDWORD) throw Failure{ {}, ERROR_INVALID_DATA };
    if (alignedLength > m_bufferSize)
    {
        m_buffer = _aligned_malloc(static_cast<size_t>(alignedLength), RawBufferAlignment);
        m_bufferSize = m_buffer.IsValid() ? static_cast<size_t>(alignedLength) : 0;
    }
    if (!m_buffer.IsValid()) throw Failure{ {}, ERROR_NOT_ENOUGH_MEMORY };

    // Expose the requested view only after the complete aligned read succeeds.
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

    // Check the partial first byte, then the complete bytes and trailing bits.
    while (count != 0 && offset % 8 != 0)
    {
        if (bytes[static_cast<size_t>(offset / 8)] & (1 << (offset % 8))) return false;
        ++offset;
        --count;
    }
    const auto full = bytes.subspan(static_cast<size_t>(offset / 8), static_cast<size_t>(count / 8));
    if (std::ranges::any_of(full, [](const BYTE byte)
    {
        // A set allocation bit makes the corresponding cluster unavailable.
        return byte != 0;
    })) return false;
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
    constexpr size_t headerSize = offsetof(VOLUME_BITMAP_BUFFER, Buffer);
    std::vector<BYTE> fresh;
    auto& buffer = cached == nullptr ? fresh : *cached;
    while (count != 0)
    {
        progress.Check();
        auto data = std::span<const BYTE>(buffer);

        // Bitmap replies need only the fixed header and actual bitmap bytes, without structure padding.
        VOLUME_BITMAP_BUFFER bitmap{};
        if (!data.empty()) std::memcpy(&bitmap, Slice(data, 0, headerSize).data(), headerSize);
        auto start = static_cast<ULONGLONG>(bitmap.StartingLcn.QuadPart);
        auto bits = data.empty() ? 0 : std::min<ULONGLONG>(bitmap.BitmapSize.QuadPart,
            (data.size() - headerSize) * 8);
        if (start > lcn || lcn - start >= bits)
        {
            buffer.resize(NtfsControlBufferBytes);
            STARTING_LCN_INPUT_BUFFER input{ .StartingLcn = { .QuadPart = static_cast<LONGLONG>(lcn) } };
            DWORD returned = 0;
            if (!DeviceIoControl(m_volume, FSCTL_GET_VOLUME_BITMAP, &input, sizeof(input), buffer.data(),
                static_cast<DWORD>(buffer.size()), &returned, nullptr) && GetLastError() != ERROR_MORE_DATA)
                throw Failure{ {}, GetLastError() };
            if (returned < headerSize || returned > buffer.size()) throw Failure{ {}, ERROR_INVALID_DATA };
            buffer.resize(returned);
            data = buffer;

            // Windows may round the bitmap start down; use the returned origin.
            std::memcpy(&bitmap, data.data(), headerSize);
            start = static_cast<ULONGLONG>(bitmap.StartingLcn.QuadPart);
            bits = std::min<ULONGLONG>(bitmap.BitmapSize.QuadPart, (data.size() - headerSize) * 8);
            if (start > lcn || lcn - start >= bits) throw Failure{ {}, ERROR_INVALID_DATA };
        }
        const auto take = std::min(count, bits - (lcn - start));
        if (!BitmapFree(data.subspan(headerSize), lcn - start, take)) return false;
        lcn += take;
        count -= take;
    }
    return true;
}

void NtfsRecovery::Scan(Progress& progress, ScanResult& result,
    const std::function<void(const Record&)>& discovered)
{
    result = {};

    // Scan assessments may use a bounded cache; every recovery allocation check reads a fresh bitmap.
    std::vector<BYTE> bitmap;
    struct Parent { ULONGLONG parent; USHORT sequence; std::wstring name; };
    std::map<ULONGLONG, Parent> parents;
    const auto total = static_cast<ULONGLONG>(m_info.MftValidDataLength.QuadPart);

    // Read the MFT in batches, retaining directory ancestry and usable deleted-file candidates.
    for (ULONGLONG offset = 0; offset < total;)
    {
        progress.Check();
        const auto take = static_cast<DWORD>(std::min<ULONGLONG>(NtfsScanBatchBytes, total - offset));
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
                // Deletion advances the directory sequence; children retain its last allocated sequence.
                if (!record.inUse && record.sequence != 0)
                    record.sequence = record.sequence == 1 ? USHRT_MAX : static_cast<USHORT>(record.sequence - 1);
                parents.emplace(number, Parent{ record.parent, record.sequence, std::move(record.name) });
                continue;
            }

            // Only retain usable deleted-file candidates.
            if (record.inUse || number < NtfsFirstUserRecord || !record.supported || !record.data.supported) continue;
            if (std::ranges::any_of(record.data.runs, [&](const Run& run)
            {
                // Reject deleted-file candidates with allocated data clusters.
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
        while ((reference & NtfsRecordNumberMask) != NtfsRootRecord)
        {
            progress.Check();
            const auto number = reference & NtfsRecordNumberMask;
            const auto parent = parents.find(number);
            if (parent == parents.end() || parent->second.sequence != (reference >> NtfsSequenceShift) ||
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

    // Read nonresident companions after MFT enumeration because raw reads reuse the scan buffer.
    ResolveRecyclePaths(progress, result, [&](const Record& record, const std::span<BYTE> bytes)
    {
        // Read companion runs only while their metadata and allocation remain unchanged.
        ValidateRecord(record);
        size_t position = 0;
        for (const auto& run : record.data.runs)
        {
            progress.Check();
            if (run.lcn < 0 || !ClustersFree(run.lcn, run.count, progress)) throw Failure{ {}, ERROR_INVALID_DATA };
            const auto take = static_cast<DWORD>(std::min<ULONGLONG>(bytes.size() - position,
                run.count * m_info.BytesPerCluster));
            const auto source = ReadAt(run.lcn * ULONGLONG(m_info.BytesPerCluster), take);
            std::memcpy(bytes.data() + position, source.data(), take);
            if (!ClustersFree(run.lcn, run.count, progress)) throw Failure{ {}, ERROR_INVALID_DATA };
            position += take;
            if (position == bytes.size()) break;
        }
        if (position != bytes.size()) throw Failure{ {}, ERROR_INVALID_DATA };
        ValidateRecord(record);
    });
}

void NtfsRecovery::ValidateRecord(const Record& record) const
{
    // Recheck the MFT mapping and saved record bytes before trusting a recovery candidate.
    if (record.number >= static_cast<ULONGLONG>(m_info.MftValidDataLength.QuadPart) /
        m_info.BytesPerFileRecordSegment || GetMftRuns() != m_mftRuns) throw Failure{ L"IDS_RECOVERY_CHANGED" };
    Record fresh;
    const auto bytes = ReadMft(record.number * m_info.BytesPerFileRecordSegment, m_info.BytesPerFileRecordSegment);
    if (!ParseRecord(bytes, record.number, m_info.BytesPerCluster, m_info.TotalClusters.QuadPart, fresh) ||
        fresh.inUse || fresh.snapshot != record.snapshot) throw Failure{ L"IDS_RECOVERY_CHANGED" };
}

std::wstring NtfsRecovery::RecoverFile(const Record& record,
    const std::wstring& canonicalDestination, Progress& progress)
{
    if (record.inUse || record.directory || !record.supported || !record.data.supported)
        throw Failure{ {}, ERROR_INVALID_DATA };
    ValidateRecord(record);
    const auto& stream = record.data;
    progress.Check();
    for (const auto& run : stream.runs)
        if (run.lcn >= 0 && !ClustersFree(run.lcn, run.count, progress)) throw Failure{ L"IDS_RECOVERY_CHANGED" };
    OutputFile output(canonicalDestination, record);

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
                const DWORD take = static_cast<DWORD>(std::min<ULONGLONG>(CopyBufferBytes, runSize - within));
                DWORD initialized = 0;
                if (run.lcn >= 0 && position < stream.initialized)
                {
                    initialized = static_cast<DWORD>(std::min<ULONGLONG>(take, stream.initialized - position));
                    const auto offset = run.lcn * ULONGLONG(m_info.BytesPerCluster) + within;
                    const auto firstCluster = offset / m_info.BytesPerCluster;
                    const auto clusters = (offset % m_info.BytesPerCluster + initialized +
                        m_info.BytesPerCluster - 1) / m_info.BytesPerCluster;

                    // Check allocation on both sides of each raw read to detect concurrent reuse.
                    if (!ClustersFree(firstCluster, clusters, progress)) throw Failure{ L"IDS_RECOVERY_CHANGED" };
                    auto source = ReadAt(offset, initialized);
                    if (!ClustersFree(firstCluster, clusters, progress)) throw Failure{ L"IDS_RECOVERY_CHANGED" };
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
        if (run.lcn >= 0 && !ClustersFree(run.lcn, run.count, progress)) throw Failure{ L"IDS_RECOVERY_CHANGED" };
    output.Commit([&progress]
    {
        // Honor pause and cancellation before finalizing the recovered file.
        progress.Check();
    }, &record.created, &record.modified);
    return output.Path();
}
