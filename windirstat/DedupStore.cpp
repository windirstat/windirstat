// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "DedupStore.h"
#include "Finder.h"

// Reads an unaligned little-endian integer from a byte buffer
template <typename T>
static T ReadAt(const std::span<const std::byte> data, const std::size_t offset) noexcept
{
    T value{};
    std::memcpy(&value, data.data() + offset, sizeof(T));
    return value;
}

static bool HasMagic(const std::span<const std::byte> data, const std::size_t offset, const char (&magic)[5]) noexcept
{
    return offset + 4 <= data.size() && std::memcmp(data.data() + offset, magic, 4) == 0;
}

constexpr char RecordVersion[] = "\x01\x03\x03\x01";
constexpr char StreamMapVersion[] = "\x01\x04\x04\x01";

// Parses an 8-hex-digit component of a container file name
static std::optional<std::uint32_t> ParseHexComponent(const std::wstring_view text) noexcept
{
    if (text.size() != 8) return std::nullopt;
    std::uint32_t value = 0;
    for (const wchar_t ch : text)
    {
        value <<= 4;
        if (ch >= L'0' && ch <= L'9') value |= ch - L'0';
        else if (ch >= L'a' && ch <= L'f') value |= ch - L'a' + 10;
        else if (ch >= L'A' && ch <= L'F') value |= ch - L'A' + 10;
        else return std::nullopt;
    }
    return value;
}

// Container files are named <id>.<generation>.<suffix>; the generation
// that stream map entries record is the high word of the name component
struct ContainerName
{
    std::uint32_t id;
    std::uint32_t generation;
    std::wstring_view suffix;
};

static std::optional<ContainerName> ParseContainerName(const std::wstring_view name) noexcept
{
    if (name.size() < 21 || name[8] != L'.' || name[17] != L'.') return std::nullopt;
    const auto id = ParseHexComponent(name.substr(0, 8));
    const auto generation = ParseHexComponent(name.substr(9, 8));
    if (!id.has_value() || !generation.has_value()) return std::nullopt;
    return ContainerName{ *id, *generation, name.substr(17) };
}

static std::wstring JoinPath(const std::wstring& dir, const std::wstring_view name)
{
    std::wstring path = dir;
    if (!path.empty() && path.back() != L'\\') path += L'\\';
    path += name;
    return path;
}

// Backup intent permits reading protected chunk store directories with SeBackupPrivilege.
static bool EnumerateDirectory(const std::wstring& dir,
    const std::function<bool(std::wstring_view, DWORD, ULONGLONG)>& callback)
{
    const SmartPointer handle(CloseHandle, CreateFileW(dir.c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (handle == INVALID_HANDLE_VALUE) return false;

    alignas(FILE_ID_BOTH_DIR_INFO) std::array<std::byte, 64 * wds::Ki> buffer;
    while (GetFileInformationByHandleEx(handle, FileIdBothDirectoryInfo, buffer.data(),
        static_cast<DWORD>(buffer.size())))
    {
        auto* entry = reinterpret_cast<const FILE_ID_BOTH_DIR_INFO*>(buffer.data());
        for (;;)
        {
            const std::wstring_view name(entry->FileName, entry->FileNameLength / sizeof(wchar_t));
            if (name != L"." && name != L".." &&
                !callback(name, entry->FileAttributes, entry->AllocationSize.QuadPart)) return false;
            if (entry->NextEntryOffset == 0) break;
            entry = reinterpret_cast<const FILE_ID_BOTH_DIR_INFO*>(
                reinterpret_cast<const std::byte*>(entry) + entry->NextEntryOffset);
        }
    }
    return GetLastError() == ERROR_NO_MORE_FILES;
}

// Count allocated bytes, including cluster slack, without following directory reparse points.
static std::optional<ULONGLONG> DirectorySize(const std::wstring& dir, const CDedupStore::CancelCheck& cancelled)
{
    ULONGLONG total = 0;
    for (std::vector<std::wstring> pending{ dir }; !pending.empty();)
    {
        const auto path = std::move(pending.back());
        pending.pop_back();
        if (cancelled && cancelled()) return std::nullopt;
        if (!EnumerateDirectory(path, [&](const std::wstring_view name, const DWORD attributes, const ULONGLONG bytes)
        {
            if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) total += bytes;
            else if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) pending.push_back(JoinPath(path, name));
            else return false;
            return true;
        })) return std::nullopt;
    }
    return total;
}

template <typename T>
static void SortUnique(std::vector<T>& values)
{
    std::ranges::sort(values);
    const auto duplicates = std::ranges::unique(values);
    values.erase(duplicates.begin(), duplicates.end());
    values.shrink_to_fit();
}

// Reads the reparse data of an open file into a per-thread buffer
static std::optional<std::span<const std::byte>> QueryReparsePoint(const HANDLE handle)
{
    thread_local std::vector<std::byte> buffer(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
    DWORD returned = 0;
    if (handle == INVALID_HANDLE_VALUE || !DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, nullptr, 0, buffer.data(),
        static_cast<DWORD>(buffer.size()), &returned, nullptr)) return std::nullopt;
    return std::span<const std::byte>(buffer.data(), returned);
}

// Decodes a REPARSE_DATA_BUFFER (including its 8-byte header) of a dedup file
static std::optional<CDedupStore::ReparseInfo> ParseReparseData(const std::span<const std::byte> buffer)
{
    // REPARSE_DATA_BUFFER header: tag, data length, reserved
    if (buffer.size() < 8 + 0x10) return std::nullopt;
    if (ReadAt<ULONG>(buffer, 0) != static_cast<ULONG>(IO_REPARSE_TAG_DEDUP)) return std::nullopt;
    const std::size_t dataLength = ReadAt<USHORT>(buffer, 4);
    if (dataLength > buffer.size() - 8) return std::nullopt;
    const std::span data = buffer.subspan(8, dataLength);

    // The payload is a chain of property blocks ('FeRp', 'RbRp', 'DdRp'), each
    // starting with a 4-byte magic, a CRC, the block size and a table of
    // (type, size, offset) entries relative to the block start.
    const auto findBlock = [&](const char (&magic)[5]) -> std::optional<std::size_t>
    {
        for (std::size_t i = 0; i + 0x10 <= data.size(); i += 4)
        {
            if (HasMagic(data, i, magic)) return i;
        }
        return std::nullopt;
    };

    const auto forEachProperty = [&](const std::size_t block, const auto& callback)
    {
        const std::size_t count = ReadAt<USHORT>(data, block + 0x0e);
        for (std::size_t i = 0; i < count; i++)
        {
            const std::size_t entry = block + 0x10 + 8 * i;
            if (entry + 8 > data.size()) return false;
            const USHORT type = ReadAt<USHORT>(data, entry);
            const USHORT size = ReadAt<USHORT>(data, entry + 2);
            const std::uint64_t offset = static_cast<std::uint64_t>(block) + ReadAt<ULONG>(data, entry + 4);
            if (offset + size > data.size()) return false;
            if (!callback(type, size, static_cast<std::size_t>(offset))) return false;
        }
        return true;
    };

    CDedupStore::ReparseInfo info;
    const auto ddrp = findBlock("DdRp");
    if (!ddrp.has_value()) return std::nullopt;
    if (!forEachProperty(*ddrp, [&](const USHORT type, const USHORT size, const std::size_t offset)
    {
        // Type 0x0d carries one 64-byte stream map reference per 1 GB segment:
        // locator, container offset, flags, map size, 16-byte digest, end, start.
        if (type != 0x0d) return true;
        if (size < 0x40 || size % 0x40 != 0) return false;
        for (std::size_t base = offset; base < offset + size; base += 0x40)
        {
            auto& hash = info.maps.emplace_back();
            std::memcpy(hash.data(), data.data() + base + 32, hash.size());
        }
        return true;
    })) return std::nullopt;
    if (info.maps.empty()) return std::nullopt;

    return info;
}

std::wstring CDedupStore::FindStore(const std::wstring& volumeRoot)
{
    std::wstring root = Finder::MakeLongPathCompatible(volumeRoot);
    if (root.empty()) return {};
    if (root.back() != L'\\') root += L'\\';

    const std::wstring chunkStore = root + std::wstring(wds::strSystemVolumeInformation) + L"\\Dedup\\ChunkStore";
    std::wstring found;
    EnumerateDirectory(chunkStore, [&](const std::wstring_view name, const DWORD attributes, ULONGLONG)
    {
        if (!found.empty() || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 || !name.ends_with(L".ddp")) return true;

        const std::wstring candidate = JoinPath(chunkStore, name);
        const SmartPointer stream(CloseHandle, CreateFileW(JoinPath(candidate, L"Stream").c_str(), FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS, nullptr));
        if (stream != INVALID_HANDLE_VALUE) found = candidate;
        return true;
    });
    return found;
}

std::optional<CDedupStore::MapReferences> CDedupStore::EnumerateVolumeMaps(const std::wstring& volumeRoot,
    const CancelCheck& cancelled, const FileVisitor& visitor)
{
    using NtQueryDirectoryFileFn = NTSTATUS(WINAPI*)(HANDLE, HANDLE, PVOID, PVOID, PIO_STATUS_BLOCK, PVOID,
        ULONG, FILE_INFORMATION_CLASS, BOOLEAN, PUNICODE_STRING, BOOLEAN);
    static const auto queryDirectory = reinterpret_cast<NtQueryDirectoryFileFn>(
        GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQueryDirectoryFile"));
    if (queryDirectory == nullptr) return std::nullopt;

    std::wstring root = Finder::MakeLongPathCompatible(volumeRoot);
    if (root.empty()) return std::nullopt;
    if (root.back() != L'\\') root += L'\\';

    // The $R index of $Extend\$Reparse lists every reparse point of the volume
    // as (file reference, tag) pairs
    const SmartPointer index(CloseHandle, CreateFileW((root + L"$Extend\\$Reparse:$R:$INDEX_ALLOCATION").c_str(),
        GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (index == INVALID_HANDLE_VALUE) return std::nullopt;

    constexpr auto FileReparsePointInformation = static_cast<FILE_INFORMATION_CLASS>(33);
    constexpr NTSTATUS StatusNoMoreFiles = static_cast<NTSTATUS>(0x80000006L);
    constexpr NTSTATUS StatusNoSuchFile = static_cast<NTSTATUS>(0xc000000fL);
    struct ReparsePointInformation
    {
        LONGLONG fileReference;
        ULONG tag;
    };

    const SmartPointer rootHandle(CloseHandle, CreateFileW(root.c_str(), 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (rootHandle == INVALID_HANDLE_VALUE) return std::nullopt;

    struct FileMaps
    {
        LONGLONG id;
        std::vector<Hash16> hashes;
        DWORD links = 0;
    };
    MapReferences maps;
    CancelLatch latch(cancelled);
    std::vector<FileMaps> files;
    std::vector<std::byte> buffer(64 * wds::Ki);

    // Bound temporary file metadata to one index page, retaining only aggregate map references.
    for (bool restart = true;; restart = false)
    {
        IO_STATUS_BLOCK status{};
        const NTSTATUS result = queryDirectory(index, nullptr, nullptr, nullptr, &status, buffer.data(),
            static_cast<ULONG>(buffer.size()), FileReparsePointInformation, FALSE, nullptr, restart ? TRUE : FALSE);
        // An empty reparse index reports no-such-file on its first query.
        if (result == StatusNoMoreFiles || restart && result == StatusNoSuchFile) break;
        if (result < 0 || (cancelled && cancelled())) return std::nullopt;

        const std::size_t returned = status.Information;
        if (returned == 0 || returned > buffer.size() || returned % sizeof(ReparsePointInformation) != 0)
            return std::nullopt;
        files.clear();
        for (std::size_t offset = 0; offset + sizeof(ReparsePointInformation) <= returned; offset += sizeof(ReparsePointInformation))
        {
            const auto entry = ReadAt<ReparsePointInformation>(buffer, offset);
            if (entry.tag == static_cast<ULONG>(IO_REPARSE_TAG_DEDUP)) files.push_back({ .id = entry.fileReference });
        }

        // Read the reparse data of each optimized file by its id.
        std::atomic<bool> failed = false;
        std::exception_ptr failure;
        std::mutex failureLock;
        std::for_each(std::execution::par, files.begin(), files.end(), [&](FileMaps& file)
        {
            try
            {
                if (failed || latch()) return;
                FILE_ID_DESCRIPTOR id{ .dwSize = sizeof(FILE_ID_DESCRIPTOR), .Type = FileIdType };
                id.FileId.QuadPart = file.id;
                const SmartPointer handle(CloseHandle, OpenFileById(rootHandle, &id, FILE_READ_ATTRIBUTES,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT));
                const auto reparse = QueryReparsePoint(handle);
                if (!reparse.has_value())
                {
                    failed = true;
                    return;
                }
                auto info = ParseReparseData(*reparse);
                FILE_STANDARD_INFO standard{};
                if (!info.has_value() || !GetFileInformationByHandleEx(handle, FileStandardInfo, &standard, sizeof(standard)) ||
                    standard.DeletePending || standard.NumberOfLinks == 0)
                {
                    failed = true;
                    return;
                }
                file.hashes = std::move(info->maps);
                file.links = standard.NumberOfLinks;
            }
            catch (...)
            {
                std::lock_guard lock(failureLock);
                failure = std::current_exception();
                failed = true;
            }
        });
        if (failure) std::rethrow_exception(failure);
        if (failed || latch() || (cancelled && cancelled())) return std::nullopt;

        // A repeated segment of one file is one map reference; every hardlink keeps it alive.
        for (auto& file : files)
        {
            if (latch()) return std::nullopt;
            std::ranges::sort(file.hashes);
            file.hashes.erase(std::ranges::unique(file.hashes).begin(), file.hashes.end());
            for (const auto& hash : file.hashes)
            {
                auto& refs = maps[hash];
                if (UINT32_MAX - refs < file.links) return std::nullopt;
                refs += file.links;
            }
            if (visitor) visitor(static_cast<ULONGLONG>(file.id), file.links, std::move(file.hashes));
        }
    }
    if (cancelled && cancelled()) return std::nullopt;
    return maps;
}

std::optional<CDedupStore::ReparseInfo> CDedupStore::ReadReparseData(const std::wstring& path)
{
    const SmartPointer handle(CloseHandle, CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    const auto reparse = QueryReparsePoint(handle);
    return reparse.has_value() ? ParseReparseData(*reparse) : std::nullopt;
}

bool CDedupStore::ReadFileBytes(const std::wstring& path, std::vector<std::byte>& buffer, const ULONGLONG maxSize)
{
    const SmartPointer handle(CloseHandle, CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (handle == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(handle, &size) || size.QuadPart < 0) return false;
    if (static_cast<ULONGLONG>(size.QuadPart) > buffer.max_size() ||
        (maxSize != 0 && static_cast<ULONGLONG>(size.QuadPart) > maxSize)) return false;

    // A fresh allocation avoids holding the old and the new contents at once
    if (buffer.capacity() < static_cast<ULONGLONG>(size.QuadPart)) std::vector<std::byte>().swap(buffer);
    buffer.resize(static_cast<std::size_t>(size.QuadPart));
    for (std::size_t done = 0; done < buffer.size();)
    {
        const DWORD request = static_cast<DWORD>(std::min<std::size_t>(buffer.size() - done, 64ull * wds::Mi));
        DWORD read = 0;
        if (!ReadFile(handle, buffer.data() + done, request, &read, nullptr) || read == 0) return false;
        done += read;
    }
    return true;
}

bool CDedupStore::ParseIndex(const std::span<const std::byte> index, std::vector<std::uint32_t>& offsets)
{
    // 'Cdir' header followed by the record count at 0x10 and offsets from 0x1c
    if (!HasMagic(index, 0, "Cdir") || index.size() < 0x1c) return false;
    const std::size_t count = ReadAt<std::uint32_t>(index, 0x10);
    if (0x1cull + 4ull * count > index.size()) return false;
    offsets.resize(count);
    for (std::size_t i = 0; i < count; i++) offsets[i] = ReadAt<std::uint32_t>(index, 0x1c + 4 * i);
    return true;
}

bool CDedupStore::ParseRecord(const std::span<const std::byte> file, const std::size_t offset, RecordView& record) noexcept
{
    // 'Ckhr' record header (0x68 bytes) followed by an 8-byte 'Smap' header and
    // 64-byte chunk entries; the record size at 0x0c covers the 'Smap' part.
    if (static_cast<std::uint64_t>(offset) + RecordHeaderSize + 8 > file.size()) return false;
    if (!HasMagic(file, offset, "Ckhr") || !HasMagic(file, offset + 4, RecordVersion)) return false;
    if (!HasMagic(file, offset + RecordHeaderSize, "Smap") ||
        !HasMagic(file, offset + RecordHeaderSize + 4, StreamMapVersion)) return false;

    const std::size_t size = ReadAt<std::uint32_t>(file, offset + 0x0c);
    if (size < 8 || (size - 8) % EntrySize != 0 || static_cast<std::uint64_t>(offset) + RecordHeaderSize + size > file.size()) return false;

    record.size = static_cast<std::uint32_t>(RecordHeaderSize + size);
    std::memcpy(record.hash.data(), file.data() + offset + 0x38, record.hash.size());
    record.entries = file.subspan(offset + RecordHeaderSize + 8, size - 8);
    return true;
}

CDedupStore::EntryView CDedupStore::ReadEntry(const RecordView& record, const std::size_t index) noexcept
{
    // Entry: chunk id, data container, record offset, padding, generation,
    // cumulative logical end, 32-byte digest, on-disk size
    const std::size_t entry = index * EntrySize;
    return {
        ReadAt<std::uint32_t>(record.entries, entry),
        ReadAt<std::uint32_t>(record.entries, entry + 4),
        ReadAt<std::uint32_t>(record.entries, entry + 8),
        ReadAt<std::uint16_t>(record.entries, entry + 14),
        ReadAt<std::uint64_t>(record.entries, entry + 24),
        ReadAt<std::uint64_t>(record.entries, entry + 56),
    };
}

std::optional<std::uint32_t> CDedupStore::FindChunkIndex(const std::uint64_t digestKey) const noexcept
{
    const auto it = std::ranges::lower_bound(m_chunkKeys, digestKey);
    if (it == m_chunkKeys.end() || *it != digestKey) return std::nullopt;
    return static_cast<std::uint32_t>(it - m_chunkKeys.begin());
}

std::optional<std::map<std::uint32_t, CDedupStore::Container>> CDedupStore::ListContainers(const std::wstring& dir, std::vector<std::wstring>& deleteLogs)
{
    // Only the newest <id>.<generation>.ccc of an id is current. Its record
    // index <id>.<generation>.01.cd (or its .02.cd copy) shares only the high
    // word of the generation, so they are matched on that.
    const auto sideKey = [](const std::uint32_t id, const std::uint32_t generation)
    {
        return (static_cast<std::uint64_t>(id) << 32) | (generation >> 16);
    };
    std::map<std::uint32_t, Container> containers;
    std::map<std::uint64_t, std::array<std::wstring, 2>> sideFiles; // .01.cd, .02.cd
    if (!EnumerateDirectory(dir, [&](const std::wstring_view fileName, const DWORD attributes, ULONGLONG)
    {
        if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) return true;
        const auto name = ParseContainerName(fileName);
        if (!name.has_value()) return true;

        std::wstring path = JoinPath(dir, fileName);
        if (name->suffix == L".ccc")
        {
            auto& container = containers[name->id];
            if (!container.dataPath.empty() && container.generation >= name->generation) return true;
            container.dataPath = std::move(path);
            container.generation = name->generation;
        }
        else if (name->suffix == L".01.cd") sideFiles[sideKey(name->id, name->generation)][0] = std::move(path);
        else if (name->suffix == L".02.cd") sideFiles[sideKey(name->id, name->generation)][1] = std::move(path);
        else if (name->suffix == L".delete.log") deleteLogs.push_back(std::move(path));
        return true;
    })) return std::nullopt;

    for (auto& [id, container] : containers)
    {
        const auto it = sideFiles.find(sideKey(id, container.generation));
        if (it == sideFiles.end()) continue;
        auto& [index, indexCopy] = it->second;
        container.indexPath = std::move(index.empty() ? indexCopy : index);
    }
    return containers;
}

bool CDedupStore::LoadDeleteLogs(const std::vector<std::wstring>& logs)
{
    // Delete logs ('Dlog', 16-byte header, 40-byte entries of id + digest) list
    // the stream maps of deleted files that garbage collection has not yet
    // processed; afterwards the delete bitmaps take over.
    std::vector<std::byte> bytes;
    for (const auto& log : logs)
    {
        if (!ReadFileBytes(log, bytes, 64ull * wds::Mi)) return Fail(ERROR_READ_FAULT, log);
        if (bytes.size() < 0x10 || !HasMagic(bytes, 0, "Dlog")) return Fail(ERROR_INVALID_DATA, log);
        for (std::size_t offset = 0x10; offset + 40 <= bytes.size(); offset += 40)
        {
            Hash16 hash;
            std::memcpy(hash.data(), bytes.data() + offset + 4, hash.size());
            m_deadHashes.insert(hash);
        }
    }
    return true;
}

// A window covers nearby records without retaining entire multi-gigabyte containers.
class CDedupStore::Reader final
{
public:
    explicit Reader(const std::wstring& path) : m_handle(CloseHandle, CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_SEQUENTIAL_SCAN, nullptr))
    {
        LARGE_INTEGER size{};
        if (m_handle != INVALID_HANDLE_VALUE && GetFileSizeEx(m_handle, &size) && size.QuadPart > 0)
            m_size = static_cast<ULONGLONG>(size.QuadPart);
    }

    std::span<const std::byte> Read(const ULONGLONG offset, const std::size_t size)
    {
        if (size == 0 || size > 64 * wds::Mi || offset > m_size || size > m_size - offset) return {};
        if (offset >= m_start && offset - m_start <= m_buffer.size() &&
            size <= m_buffer.size() - static_cast<std::size_t>(offset - m_start))
            return std::span(m_buffer).subspan(static_cast<std::size_t>(offset - m_start), size);
        m_start = offset;
        const auto window = static_cast<std::size_t>(std::min<ULONGLONG>(m_size - offset,
            std::max<std::size_t>(4 * wds::Mi, size)));
        if (m_buffer.capacity() < window) std::vector<std::byte>().swap(m_buffer);
        m_buffer.resize(window);
        LARGE_INTEGER position{};
        position.QuadPart = static_cast<LONGLONG>(offset);
        if (!SetFilePointerEx(m_handle, position, nullptr, FILE_BEGIN)) { m_buffer.clear(); return {}; }
        DWORD done = 0;
        while (done < window)
        {
            DWORD read = 0;
            if (!ReadFile(m_handle, m_buffer.data() + done, static_cast<DWORD>(window - done), &read, nullptr) || read == 0)
            {
                m_buffer.clear();
                return {};
            }
            done += read;
        }
        return std::span(m_buffer).first(size);
    }

    bool ReadRecord(const std::uint32_t offset, RecordView& record)
    {
        const auto header = Read(offset, RecordHeaderSize);
        if (header.empty() || !HasMagic(header, 0, "Ckhr")) return false;
        const ULONGLONG size = RecordHeaderSize + static_cast<ULONGLONG>(ReadAt<std::uint32_t>(header, 0x0c));
        return size <= 64 * wds::Mi && ParseRecord(Read(offset, static_cast<std::size_t>(size)), 0, record);
    }

private:
    SmartPointer<HANDLE, decltype(&CloseHandle)> m_handle;
    ULONGLONG m_size = 0, m_start = 0;
    std::vector<std::byte> m_buffer;
};

bool CDedupStore::ForEachMap(const CancelCheck& cancelled, const MapCallback& callback)
{
    std::vector<std::byte> indexBytes;
    std::vector<std::uint32_t> offsets;
    CancelLatch latch(cancelled);
    for (const auto [containerIndex, container] : std::views::enumerate(m_containers))
    {
        if (!ReadFileBytes(container.indexPath, indexBytes, 64ull * wds::Mi) || !ParseIndex(indexBytes, offsets))
            return Fail(ERROR_INVALID_DATA, container.indexPath);
        std::ranges::sort(offsets);
        Reader reader(container.dataPath);
        for (const auto offset : offsets)
        {
            if (latch()) return Fail(ERROR_CANCELLED);
            RecordView record;
            if (!reader.ReadRecord(offset, record)) return Fail(ERROR_INVALID_DATA, container.dataPath);
            callback(static_cast<std::uint32_t>(containerIndex), offset, record, m_liveMaps->contains(record.hash));
        }
    }
    return true;
}

bool CDedupStore::Load(const std::wstring& storePath, const MapReferences& liveMaps, const CancelCheck& cancelled)
{
    *this = CDedupStore{};
    m_liveMaps = &liveMaps;
    m_path = storePath;

    const auto containers = ListContainers(JoinPath(storePath, L"Stream"), m_deleteLogs);
    if (!containers.has_value()) return Fail(ERROR_READ_FAULT, storePath);
    for (const auto& container : *containers | std::views::values) m_containers.push_back(container);
    if (m_containers.empty() && !liveMaps.empty() ||
        std::ranges::any_of(m_containers, [](const Container& container) { return container.indexPath.empty(); }))
    {
        return Fail(ERROR_NOT_FOUND, m_path);
    }
    // Compact batches of map references into the sorted chunk table, bounding duplicate storage.
    struct ChunkRef
    {
        std::uint64_t key;
        ChunkInfo info;
    };
    std::vector<ChunkRef> refs, mapRefs;
    std::size_t nextCompaction = 65536;
    bool valid = true;
    const auto compact = [&]
    {
        std::ranges::stable_sort(refs, {}, &ChunkRef::key);
        std::size_t count = 0;
        for (const auto ref : refs)
        {
            if (count == 0 || refs[count - 1].key != ref.key) refs[count++] = ref;
            else
            {
                auto& info = refs[count - 1].info;
                if (UINT32_MAX - info.refs < ref.info.refs) valid = false;
                else info.refs += ref.info.refs;
                info.diskSize = ref.info.diskSize;
            }
        }
        refs.resize(count);
        nextCompaction = std::max<std::size_t>(65536, 2 * count);
    };
    if (!ForEachMap(cancelled, [&](const std::uint32_t container, const std::uint32_t offset,
        const RecordView& record, const bool live)
    {
        if (!valid || !live || !m_maps.try_emplace(record.hash, MapLocation{ container, offset }).second) return;
        mapRefs.clear();
        for (std::size_t i = record.Count(); i-- > 0;)
        {
            const auto entry = ReadEntry(record, i);
            if (entry.diskSize > UINT32_MAX) { valid = false; return; }
            mapRefs.push_back({ entry.digestKey, { 1, static_cast<std::uint32_t>(entry.diskSize) } });
        }
        std::ranges::stable_sort(mapRefs, {}, &ChunkRef::key);
        mapRefs.erase(std::ranges::unique(mapRefs, {}, &ChunkRef::key).begin(), mapRefs.end());
        refs.append_range(mapRefs);
        if (refs.size() >= nextCompaction) compact();
    })) return false;
    compact();
    if (!valid || refs.size() >= UINT32_MAX) return Fail(ERROR_INVALID_DATA);
    if (m_maps.size() != liveMaps.size()) return Fail(ERROR_NOT_FOUND);
    m_chunkKeys.reserve(refs.size());
    m_chunks.reserve(refs.size());
    for (const auto& ref : refs)
    {
        m_chunkKeys.push_back(ref.key);
        m_chunks.push_back(ref.info);
    }

    m_loaded = true;
    VTRACE(L"Dedup store {}: {} live maps, {} chunks", m_path, m_maps.size(), m_chunks.size());
    return true;
}

bool CDedupStore::Resolve(const std::span<const Hash16> requests, const ChunkVisitor& visitor, const CancelCheck& cancelled)
{
    if (!m_loaded) return false;

    // Group the wanted records by container so each container is read once
    std::vector<std::map<std::uint32_t, std::vector<std::size_t>>> perContainer(m_containers.size());
    for (const auto [requestIndex, hash] : std::views::enumerate(requests))
    {
        const auto location = m_maps.find(hash);
        if (location == m_maps.end()) return Fail(ERROR_NOT_FOUND);
        perContainer[location->second.container][location->second.offset].push_back(static_cast<std::size_t>(requestIndex));
    }

    CancelLatch latch(cancelled);
    std::vector<std::uint32_t> mapChunks;
    for (const auto [containerIndex, wantedRecords] : std::views::enumerate(perContainer))
    {
        if (wantedRecords.empty()) continue;
        if (cancelled && cancelled())
        {
            return Fail(ERROR_CANCELLED);
        }

        // The store may have been compacted since it was loaded; partial results
        // would leave files unaccounted while the whole store is neutralized
        const std::wstring& dataPath = m_containers[containerIndex].dataPath;
        Reader reader(dataPath);

        for (const auto& [offset, requestIndices] : wantedRecords)
        {
            if (latch()) return Fail(ERROR_CANCELLED);
            RecordView record;
            if (!reader.ReadRecord(offset, record) || record.hash != requests[requestIndices.front()])
            {
                return Fail(ERROR_INVALID_DATA, dataPath);
            }

            mapChunks.clear();
            for (std::size_t i = 0; i < record.Count(); i++)
            {
                const auto index = FindChunkIndex(ReadEntry(record, i).digestKey);
                if (!index.has_value()) return Fail(ERROR_INVALID_DATA, dataPath);
                mapChunks.push_back(*index);
            }
            std::ranges::sort(mapChunks);
            const auto duplicates = std::ranges::unique(mapChunks);
            mapChunks.erase(duplicates.begin(), duplicates.end());

            for (const std::uint32_t index : mapChunks)
            {
                for (const std::size_t requestIndex : requestIndices) visitor(requestIndex, index, m_chunks[index]);
            }
        }
    }
    return true;
}

// Relocation tables preserve chunk ids across compaction; appended records use consecutive ids.
static bool ReadRelocationTable(const HANDLE handle, const std::span<const std::uint32_t> offsets,
    std::vector<std::uint32_t>& relocatedIds, std::uint32_t& tailBase)
{
    if (offsets.empty() || offsets.front() < 0x3020 || offsets.front() > 64 * wds::Mi) return false;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle, &size) || size.QuadPart < 0 ||
        static_cast<ULONGLONG>(size.QuadPart) < static_cast<ULONGLONG>(offsets.back()) + 0x58)
        return false;
    for (std::size_t i = 1; i < offsets.size(); i++)
        if (offsets[i] <= offsets[i - 1] || offsets[i] - offsets[i - 1] < 0x58) return false;

    std::array<std::byte, 0x3020> header{};
    OVERLAPPED position{};
    DWORD read = 0;
    if (!ReadFile(handle, header.data(), static_cast<DWORD>(header.size()), &read, &position) || read != header.size())
        return false;

    // The newest of three container headers must agree with the index and physical file bounds.
    std::optional<std::size_t> current;
    for (std::size_t base = 0; base < 0x3000; base += 0x1000)
    {
        if (HasMagic(header, base, "Cthr") && (!current ||
            ReadAt<std::uint64_t>(header, base + 0x08) > ReadAt<std::uint64_t>(header, *current + 0x08))) current = base;
    }
    if (!current || ReadAt<std::uint32_t>(header, *current + 0x18) != offsets.size()) return false;
    const auto nextId = ReadAt<std::uint32_t>(header, *current + 0x1c);
    const auto validLength = ReadAt<std::uint32_t>(header, *current + 0x20);
    if (validLength < static_cast<ULONGLONG>(offsets.back()) + 0x58 || validLength > size.QuadPart) return false;

    const std::size_t relocated = HasMagic(header, 0x3000, "Rrtl") ? ReadAt<std::uint32_t>(header, 0x3008) : 0;
    if (relocated > offsets.size() || 0x3020 + 8ull * relocated > offsets.front()) return false;

    // Read relocation pairs in bounded blocks, retaining only the ids needed for the record walk.
    relocatedIds.resize(relocated);
    std::array<std::uint32_t, 8192> pairs;
    for (std::size_t first = 0; first < relocated;)
    {
        const auto count = std::min(relocated - first, pairs.size() / 2);
        const auto bytes = static_cast<DWORD>(8 * count);
        position.Offset = static_cast<DWORD>(0x3020 + 8 * first);
        if (!ReadFile(handle, pairs.data(), bytes, &read, &position) || read != bytes) return false;
        for (std::size_t i = 0; i < count; i++)
        {
            const auto id = pairs[2 * i];
            if (id == 0 || pairs[2 * i + 1] != offsets[first + i] ||
                first + i != 0 && id <= relocatedIds[first + i - 1]) return false;
            relocatedIds[first + i] = id;
        }
        first += count;
    }

    // The implicit tail follows the relocated ids, or starts at one in an uncompacted container.
    const auto tail = static_cast<std::uint32_t>(offsets.size() - relocated);
    if (nextId < tail) return false;
    tailBase = nextId - tail;
    return relocated == 0 ? tailBase == 1 : tailBase > relocatedIds.back();
}

bool CDedupStore::ComputeSpaceUsage(const CancelCheck& cancelled)
{
    if (!m_loaded) return false;
    m_space = {};

    // Delete logs classify the garbage included in volume-wide space accounting.
    m_deadHashes.clear();
    if (!LoadDeleteLogs(m_deleteLogs)) return false;
    const auto storeBytes = DirectorySize(m_path, cancelled);
    if (!storeBytes.has_value()) return Fail(ERROR_READ_FAULT);
    m_space.storeBytes = *storeBytes;
    for (const ChunkInfo& chunk : m_chunks) m_space.liveChunkBytes += chunk.diskSize;

    std::vector<std::wstring> dataDeleteLogs;
    const auto data = ListContainers(JoinPath(m_path, L"Data"), dataDeleteLogs);
    if (!data.has_value()) return Fail(ERROR_READ_FAULT);

    // Chunks a garbage collection already found dead ('Dlog': container id at
    // 0x08, then 8-byte entries starting with the chunk id)
    std::vector<std::uint64_t> loggedIds;
    {
        std::vector<std::byte> bytes;
        for (const auto& log : dataDeleteLogs)
        {
            if (!ReadFileBytes(log, bytes, 256ull * wds::Mi)) return Fail(ERROR_READ_FAULT, log);
            if (bytes.size() < 0x10 || !HasMagic(bytes, 0, "Dlog")) return Fail(ERROR_INVALID_DATA, log);
            const std::uint64_t container = ReadAt<std::uint32_t>(bytes, 0x08);
            for (std::size_t offset = 0x10; offset + 8 <= bytes.size(); offset += 8)
                loggedIds.push_back((container << 32) | ReadAt<std::uint32_t>(bytes, offset));
        }
        SortUnique(loggedIds);
    }

    // Live chunk locations: an entry whose generation matches the current
    // container points straight at its record; an older generation means the
    // record moved during compaction and is matched by its chunk id instead
    std::vector<std::uint64_t> liveOffsets;
    std::vector<std::uint64_t> liveIds;
    std::vector<std::uint64_t> pendingOffsets; // chunks of deleted files a regular collection processes
    std::vector<std::uint64_t> pendingIds;
    std::unordered_map<const void*, std::size_t> compactAt;
    const auto compact = [&](std::vector<std::uint64_t>& locations)
    {
        auto& threshold = compactAt[&locations];
        if (locations.size() < std::max<std::size_t>(65536, threshold)) return;
        SortUnique(locations);
        threshold = 2 * locations.size();
    };
    const auto collect = [&](const RecordView& record, std::vector<std::uint64_t>& byOffset, std::vector<std::uint64_t>& byId)
    {
        for (std::size_t i = 0; i < record.Count(); i++)
        {
            const EntryView entry = ReadEntry(record, i);
            const auto container = data->find(entry.container);
            if (container != data->end() && (container->second.generation >> 16) == entry.generation)
                byOffset.push_back((static_cast<std::uint64_t>(entry.container) << 32) | entry.dataOffset);
            else byId.push_back((static_cast<std::uint64_t>(entry.container) << 32) | entry.chunkId);
        }
        compact(byOffset);
        compact(byId);
    };

    // Stream maps no live file uses are garbage. A regular garbage collection
    // only handles maps listed in the delete logs and compacts a stream
    // container once at most 80% of it stays.
    struct StreamTally
    {
        std::size_t records = 0;
        std::size_t logged = 0;
        ULONGLONG loggedBytes = 0;
    };
    std::vector<StreamTally> tallies(m_containers.size());
    ULONGLONG garbage = 0;
    if (!ForEachMap(cancelled, [&](const std::uint32_t containerIndex, std::uint32_t, const RecordView& record, const bool live)
    {
        StreamTally& tally = tallies[containerIndex];
        tally.records++;
        if (live)
        {
            collect(record, liveOffsets, liveIds);
            return;
        }
        garbage += record.size;
        if (m_deadHashes.contains(record.hash))
        {
            collect(record, pendingOffsets, pendingIds);
            tally.logged++;
            tally.loggedBytes += record.size;
        }
    })) return false;
    ULONGLONG regularGarbage = 0;
    for (const StreamTally& tally : tallies)
    {
        if (tally.logged > 0 && 100 * (tally.records - tally.logged) / tally.records <= StreamMinOccupancyPercent)
            regularGarbage += tally.loggedBytes;
    }
    SortUnique(liveOffsets);
    SortUnique(liveIds);
    SortUnique(pendingOffsets);
    SortUnique(pendingIds);

    // Every data record that no live map reaches is garbage awaiting collection
    std::vector<std::byte> indexBytes;
    std::vector<std::uint32_t> offsets;
    std::vector<std::uint32_t> relocatedIds;
    std::array<std::byte, DataRecordHeaderSize> header{};
    CancelLatch latch(cancelled);
    for (const auto& [id, container] : *data)
    {
        if (cancelled && cancelled()) return Fail(ERROR_CANCELLED);
        if (container.indexPath.empty() || !ReadFileBytes(container.indexPath, indexBytes, 256ull * wds::Mi) ||
            !ParseIndex(indexBytes, offsets)) return Fail(ERROR_INVALID_DATA, container.indexPath);
        if (offsets.empty()) continue;

        // Current-generation live offsets need neither relocation data nor record headers.
        const bool ordered = std::ranges::is_sorted(offsets);
        if (!ordered) std::ranges::sort(offsets);
        const auto firstUnknown = std::ranges::find_if(offsets, [&](const std::uint32_t offset)
        {
            return latch() || !std::ranges::binary_search(liveOffsets, (static_cast<std::uint64_t>(id) << 32) | offset);
        });
        if (latch()) return Fail(ERROR_CANCELLED);
        if (firstUnknown == offsets.end()) continue;

        const SmartPointer handle(CloseHandle, CreateFileW(container.dataPath.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_RANDOM_ACCESS, nullptr));
        if (handle == INVALID_HANDLE_VALUE) return Fail(GetLastError(), container.dataPath);
        const auto readHeader = [&](const std::uint32_t offset)
        {
            OVERLAPPED position{};
            position.Offset = offset;
            DWORD read = 0;
            return ReadFile(handle, header.data(), static_cast<DWORD>(header.size()), &read, &position) &&
                read == header.size() && HasMagic(header, 0, "Ckhr");
        };

        std::uint32_t tailBase = 0;
        bool idsKnown = ordered && ReadRelocationTable(handle, offsets, relocatedIds, tailBase);
        if (idsKnown)
        {
            const auto firstId = relocatedIds.empty() ? tailBase : relocatedIds.front();
            const auto lastId = offsets.size() == relocatedIds.size() ? relocatedIds.back() :
                tailBase + static_cast<std::uint32_t>(offsets.size() - relocatedIds.size() - 1);
            idsKnown = readHeader(offsets.front()) && ReadAt<std::uint32_t>(header, 0x08) == firstId &&
                readHeader(offsets.back()) && ReadAt<std::uint32_t>(header, 0x08) == lastId;
        }
        std::size_t deleted = 0;           // dead chunks a regular collection knows about
        ULONGLONG deletedBytes = 0;
        for (std::size_t i = static_cast<std::size_t>(firstUnknown - offsets.begin()); i < offsets.size(); i++)
        {
            if (latch()) return Fail(ERROR_CANCELLED);
            const std::uint32_t offset = offsets[i];
            const std::uint64_t offsetKey = (static_cast<std::uint64_t>(id) << 32) | offset;
            if (std::ranges::binary_search(liveOffsets, offsetKey)) continue;

            // Unknown layouts use record headers; the last record also needs its exact byte length.
            const bool needsHeader = !idsKnown || i + 1 == offsets.size();
            if (needsHeader && !readHeader(offset)) return Fail(ERROR_READ_FAULT, container.dataPath);
            const auto chunkId = idsKnown ? (i < relocatedIds.size() ? relocatedIds[i] :
                tailBase + static_cast<std::uint32_t>(i - relocatedIds.size())) : ReadAt<std::uint32_t>(header, 0x08);
            if (idsKnown && needsHeader && chunkId != ReadAt<std::uint32_t>(header, 0x08))
                return Fail(ERROR_INVALID_DATA, container.dataPath);
            const std::uint64_t idKey = (static_cast<std::uint64_t>(id) << 32) | chunkId;
            if (std::ranges::binary_search(liveIds, idKey)) continue;

            // Records are 8-byte aligned; the last one is sized from its header
            const ULONGLONG span = i + 1 < offsets.size() ? offsets[i + 1] - offset :
                (DataRecordHeaderSize + ReadAt<std::uint32_t>(header, 0x0c) + 7ull) & ~7ull;
            garbage += span;
            if (std::ranges::binary_search(loggedIds, idKey) || std::ranges::binary_search(pendingOffsets, offsetKey) ||
                std::ranges::binary_search(pendingIds, idKey))
            {
                deleted++;
                deletedBytes += span;
            }
        }

        // A regular collection compacts a container only once at most 95% of
        // its chunks stay; a full one compacts every container with garbage
        if (deleted > 0 && 100 * (offsets.size() - deleted) / offsets.size() <= DataMinOccupancyPercent)
            regularGarbage += deletedBytes;
    }

    m_space.garbageBytes = garbage;
    m_space.regularGarbageBytes = std::min(regularGarbage, garbage);
    const ULONGLONG accounted = m_space.liveChunkBytes + m_space.garbageBytes;
    if (accounted > m_space.storeBytes) return Fail(ERROR_INVALID_DATA);
    m_space.overheadBytes = m_space.storeBytes - accounted;
    VTRACE(L"Dedup store space: store {} live {} garbage {} (regular {}) overhead {}",
        m_space.storeBytes, m_space.liveChunkBytes, m_space.garbageBytes, m_space.regularGarbageBytes, m_space.overheadBytes);
    return true;
}

bool CDedupStore::Fail(const DWORD error, const std::wstring& path)
{
    m_error = std::format(L"{}: {}", path.empty() ? m_path : path, TranslateError(error));
    return false;
}
