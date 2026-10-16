// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "DedupStore.h"
#include "Finder.h"

namespace
{
    // Reads an unaligned little-endian integer from a byte buffer
    template <typename T>
    T ReadAt(const std::span<const std::byte> data, const std::size_t offset) noexcept
    {
        T value{};
        std::memcpy(&value, data.data() + offset, sizeof(T));
        return value;
    }

    bool HasMagic(const std::span<const std::byte> data, const std::size_t offset, const char (&magic)[5]) noexcept
    {
        return offset + 4 <= data.size() && std::memcmp(data.data() + offset, magic, 4) == 0;
    }

    constexpr char RecordVersion[] = "\x01\x03\x03\x01";
    constexpr char StreamMapVersion[] = "\x01\x04\x04\x01";

    // Parses an 8-hex-digit component of a container file name
    std::optional<std::uint32_t> ParseHexComponent(const std::wstring_view text) noexcept
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

    std::optional<ContainerName> ParseContainerName(const std::wstring_view name) noexcept
    {
        if (name.size() < 21 || name[8] != L'.' || name[17] != L'.') return std::nullopt;
        const auto id = ParseHexComponent(name.substr(0, 8));
        const auto generation = ParseHexComponent(name.substr(9, 8));
        if (!id.has_value() || !generation.has_value()) return std::nullopt;
        return ContainerName{ *id, *generation, name.substr(17) };
    }

    std::wstring JoinPath(const std::wstring& dir, const std::wstring_view name)
    {
        std::wstring path = dir;
        if (!path.empty() && path.back() != L'\\') path += L'\\';
        path += name;
        return path;
    }

    // Enumerates directory entries, invoking the callback with each find record
    bool EnumerateDirectory(const std::wstring& dir, const std::function<void(const WIN32_FIND_DATAW&)>& callback)
    {
        WIN32_FIND_DATAW data;
        const SmartPointer handle(FindClose, FindFirstFileExW(JoinPath(dir, L"*").c_str(),
            FindExInfoBasic, &data, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH));
        if (handle == INVALID_HANDLE_VALUE) return false;
        do
        {
            if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
            callback(data);
        }
        while (FindNextFileW(handle, &data));
        return true;
    }

    ULONGLONG FileSizeOf(const WIN32_FIND_DATAW& data) noexcept
    {
        return (static_cast<ULONGLONG>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    }

    // Total size of every file below a directory
    ULONGLONG DirectorySize(const std::wstring& dir)
    {
        ULONGLONG total = 0;
        EnumerateDirectory(dir, [&](const WIN32_FIND_DATAW& data)
        {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) total += DirectorySize(JoinPath(dir, data.cFileName));
            else total += FileSizeOf(data);
        });
        return total;
    }

    template <typename T>
    void SortUnique(std::vector<T>& values)
    {
        std::ranges::sort(values);
        const auto duplicates = std::ranges::unique(values);
        values.erase(duplicates.begin(), duplicates.end());
        values.shrink_to_fit();
    }

    // Reads the reparse data of an open file into a per-thread buffer
    std::optional<std::span<const std::byte>> QueryReparsePoint(const HANDLE handle)
    {
        thread_local std::vector<std::byte> buffer(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
        DWORD returned = 0;
        if (handle == INVALID_HANDLE_VALUE || !DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, nullptr, 0, buffer.data(),
            static_cast<DWORD>(buffer.size()), &returned, nullptr)) return std::nullopt;
        return std::span<const std::byte>(buffer.data(), returned);
    }

    // Decodes a REPARSE_DATA_BUFFER (including its 8-byte header) of a dedup file
    std::optional<CDedupStore::ReparseInfo> ParseReparseData(const std::span<const std::byte> buffer)
    {
        // REPARSE_DATA_BUFFER header: tag, data length, reserved
        if (buffer.size() < 8 + 0x10) return std::nullopt;
        if (ReadAt<ULONG>(buffer, 0) != static_cast<ULONG>(IO_REPARSE_TAG_DEDUP)) return std::nullopt;
        const std::size_t dataLength = std::min<std::size_t>(ReadAt<USHORT>(buffer, 4), buffer.size() - 8);
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
                if (entry + 8 > data.size()) return;
                const USHORT type = ReadAt<USHORT>(data, entry);
                const USHORT size = ReadAt<USHORT>(data, entry + 2);
                const std::size_t offset = block + ReadAt<ULONG>(data, entry + 4);
                if (offset + size > data.size()) return;
                callback(type, size, offset);
            }
        };

        CDedupStore::ReparseInfo info;
        ULONGLONG coveredEnd = 0;
        const auto ddrp = findBlock("DdRp");
        if (!ddrp.has_value()) return std::nullopt;
        forEachProperty(*ddrp, [&](const USHORT type, const USHORT size, const std::size_t offset)
        {
            // Type 0x0d carries one 64-byte stream map reference per 1 GB segment:
            // locator, container offset, flags, map size, 16-byte digest, end, start.
            if (type != 0x0d || size < 0x40 || size % 0x40 != 0) return;
            for (std::size_t base = offset; base < offset + size; base += 0x40)
            {
                auto& hash = info.maps.emplace_back();
                std::memcpy(hash.data(), data.data() + base + 32, hash.size());
                coveredEnd = std::max(coveredEnd, ReadAt<ULONGLONG>(data, base + 48));
            }
        });
        if (info.maps.empty()) return std::nullopt;

        // The file size lives in the 'FeRp' block as the 8-byte property of type 6
        if (const auto ferp = findBlock("FeRp"); ferp.has_value())
        {
            forEachProperty(*ferp, [&](const USHORT type, const USHORT size, const std::size_t offset)
            {
                if (type == 0x06 && size == 8) info.fileSize = ReadAt<ULONGLONG>(data, offset);
            });
        }
        if (info.fileSize == 0) info.fileSize = coveredEnd;
        return info;
    }
}

std::wstring CDedupStore::FindStore(const std::wstring& volumeRoot)
{
    std::wstring root = Finder::MakeLongPathCompatible(volumeRoot);
    if (root.empty()) return {};
    if (root.back() != L'\\') root += L'\\';

    const std::wstring chunkStore = root + L"System Volume Information\\Dedup\\ChunkStore";
    std::wstring found;
    EnumerateDirectory(chunkStore, [&](const WIN32_FIND_DATAW& data)
    {
        if (!found.empty() || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) return;
        const std::wstring_view name(data.cFileName);
        if (!name.ends_with(L".ddp")) return;

        // Only accept stores that actually carry stream containers
        const std::wstring candidate = JoinPath(chunkStore, name);
        if (const DWORD attributes = GetFileAttributesW(JoinPath(candidate, L"Stream").c_str());
            attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            found = candidate;
        }
    });
    return found;
}

std::optional<CDedupStore::MapSet> CDedupStore::EnumerateVolumeMaps(const std::wstring& volumeRoot, const CancelCheck& cancelled)
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
    struct ReparsePointInformation
    {
        LONGLONG fileReference;
        ULONG tag;
    };

    std::vector<LONGLONG> files;
    std::vector<std::byte> buffer(64 * wds::Ki);
    for (bool restart = true;; restart = false)
    {
        IO_STATUS_BLOCK status{};
        const NTSTATUS result = queryDirectory(index, nullptr, nullptr, nullptr, &status, buffer.data(),
            static_cast<ULONG>(buffer.size()), FileReparsePointInformation, FALSE, nullptr, restart ? TRUE : FALSE);
        if (result == StatusNoMoreFiles) break;
        if (result < 0 || (cancelled && cancelled())) return std::nullopt;

        const std::size_t returned = std::min<std::size_t>(status.Information, buffer.size());
        for (std::size_t offset = 0; offset + sizeof(ReparsePointInformation) <= returned; offset += sizeof(ReparsePointInformation))
        {
            const auto entry = ReadAt<ReparsePointInformation>(buffer, offset);
            if (entry.tag == static_cast<ULONG>(IO_REPARSE_TAG_DEDUP)) files.push_back(entry.fileReference);
        }
    }
    if (cancelled && cancelled()) return std::nullopt;

    // Read the reparse data of each optimized file by its id
    const SmartPointer rootHandle(CloseHandle, CreateFileW(root.c_str(), 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (rootHandle == INVALID_HANDLE_VALUE) return std::nullopt;

    std::vector<std::vector<Hash16>> hashes(files.size());
    std::atomic<std::size_t> failures = 0;
    CancelLatch latch(cancelled);
    std::for_each(std::execution::par, hashes.begin(), hashes.end(), [&](std::vector<Hash16>& list)
    {
        if (latch()) return;
        FILE_ID_DESCRIPTOR id{ .dwSize = sizeof(FILE_ID_DESCRIPTOR), .Type = FileIdType };
        id.FileId.QuadPart = files[static_cast<std::size_t>(&list - hashes.data())];
        const SmartPointer handle(CloseHandle, OpenFileById(rootHandle, &id, FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT));
        const auto reparse = QueryReparsePoint(handle);
        if (!reparse.has_value())
        {
            failures++;
            return;
        }
        if (auto info = ParseReparseData(*reparse); info.has_value()) list = std::move(info->maps);
    });

    // Files deleted between the index query and the read are expected; a
    // large share of failures means the enumeration cannot be trusted
    if (latch() || (cancelled && cancelled()) || failures > files.size() / 100 + 16) return std::nullopt;

    MapSet maps;
    for (const auto& list : hashes) maps.insert(list.begin(), list.end());
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
    if (maxSize != 0 && static_cast<ULONGLONG>(size.QuadPart) > maxSize) return false;

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

std::vector<bool> CDedupStore::ParseDeleteBitmap(const std::span<const std::byte> bitmap, const std::size_t records)
{
    // 'Bmap' header (32 bytes: id, generation, bitmap length, record count,
    // checksum) followed by one bit per index record; a set bit marks the
    // stream map of a deleted file. Unknown layouts mark nothing.
    std::vector<bool> deleted(records, false);
    if (!HasMagic(bitmap, 0, "Bmap") || bitmap.size() < 0x20) return deleted;
    const std::size_t bytes = ReadAt<std::uint32_t>(bitmap, 0x10);
    const std::size_t count = ReadAt<std::uint32_t>(bitmap, 0x14);
    if (count != records || 0x20ull + bytes > bitmap.size() || 8ull * bytes < count) return deleted;
    for (std::size_t i = 0; i < count; i++)
    {
        deleted[i] = (std::to_integer<unsigned>(bitmap[0x20 + i / 8]) >> (i % 8) & 1) != 0;
    }
    return deleted;
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

std::map<std::uint32_t, CDedupStore::Container> CDedupStore::ListContainers(const std::wstring& dir, std::vector<std::wstring>& deleteLogs)
{
    // Only the newest <id>.<generation>.ccc of an id is current. Its record
    // index <id>.<generation>.01.cd (or the .02.cd copy) and delete bitmap
    // <id>.<generation>.delete.map only share the high word of the generation
    // with it, so they are matched on that.
    const auto sideKey = [](const std::uint32_t id, const std::uint32_t generation)
    {
        return (static_cast<std::uint64_t>(id) << 32) | (generation >> 16);
    };
    std::map<std::uint32_t, Container> containers;
    std::map<std::uint64_t, std::array<std::wstring, 3>> sideFiles; // .01.cd, .02.cd, .delete.map
    EnumerateDirectory(dir, [&](const WIN32_FIND_DATAW& data)
    {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) return;
        const auto name = ParseContainerName(data.cFileName);
        if (!name.has_value()) return;

        std::wstring path = JoinPath(dir, data.cFileName);
        if (name->suffix == L".ccc")
        {
            auto& container = containers[name->id];
            if (!container.dataPath.empty() && container.generation >= name->generation) return;
            container.dataPath = std::move(path);
            container.generation = name->generation;
        }
        else if (name->suffix == L".01.cd") sideFiles[sideKey(name->id, name->generation)][0] = std::move(path);
        else if (name->suffix == L".02.cd") sideFiles[sideKey(name->id, name->generation)][1] = std::move(path);
        else if (name->suffix == L".delete.map") sideFiles[sideKey(name->id, name->generation)][2] = std::move(path);
        else if (name->suffix == L".delete.log") deleteLogs.push_back(std::move(path));
    });

    for (auto& [id, container] : containers)
    {
        const auto it = sideFiles.find(sideKey(id, container.generation));
        if (it == sideFiles.end()) continue;
        auto& [index, indexCopy, deleteMap] = it->second;
        container.indexPath = std::move(index.empty() ? indexCopy : index);
        container.deleteMapPath = std::move(deleteMap);
    }
    return containers;
}

void CDedupStore::LoadDeleteLogs(const std::vector<std::wstring>& logs)
{
    // Delete logs ('Dlog', 16-byte header, 40-byte entries of id + digest) list
    // the stream maps of deleted files that garbage collection has not yet
    // processed; afterwards the delete bitmaps take over.
    std::vector<std::byte> bytes;
    for (const auto& log : logs)
    {
        if (!ReadFileBytes(log, bytes, 64ull * wds::Mi) || !HasMagic(bytes, 0, "Dlog")) continue;
        for (std::size_t offset = 0x10; offset + 40 <= bytes.size(); offset += 40)
        {
            Hash16 hash;
            std::memcpy(hash.data(), bytes.data() + offset + 4, hash.size());
            m_deadHashes.insert(hash);
        }
    }
}

bool CDedupStore::ForEachMap(const CancelCheck& cancelled, const MapCallback& callback)
{
    std::vector<std::byte> indexBytes;
    std::vector<std::byte> dataBytes;
    std::vector<std::byte> bitmapBytes;
    std::vector<std::uint32_t> offsets;
    for (const auto [containerIndex, container] : std::views::enumerate(m_containers))
    {
        if (cancelled && cancelled())
        {
            m_error = L"Cancelled";
            return false;
        }

        if (!ReadFileBytes(container.indexPath, indexBytes, 64ull * wds::Mi) || !ParseIndex(indexBytes, offsets))
        {
            m_error = L"Invalid stream index: " + container.indexPath;
            return false;
        }
        if (!ReadFileBytes(container.dataPath, dataBytes, 4ull * wds::Gi))
        {
            m_error = L"Cannot read stream container: " + container.dataPath;
            return false;
        }

        // The bitmap only matters when no file-based liveness is available
        std::vector<bool> deleted(offsets.size(), false);
        if (m_liveMaps == nullptr && !container.deleteMapPath.empty() &&
            ReadFileBytes(container.deleteMapPath, bitmapBytes, 64ull * wds::Mi))
        {
            deleted = ParseDeleteBitmap(bitmapBytes, offsets.size());
        }

        for (const auto [recordIndex, offset] : std::views::enumerate(offsets))
        {
            RecordView record;
            if (!ParseRecord(dataBytes, offset, record))
            {
                m_error = std::format(L"Unexpected stream map record at 0x{:x} in {}", offset, container.dataPath);
                return false;
            }
            const bool live = m_liveMaps != nullptr ? m_liveMaps->contains(record.hash) :
                !deleted[static_cast<std::size_t>(recordIndex)] && !m_deadHashes.contains(record.hash);
            callback(static_cast<std::uint32_t>(containerIndex), offset, record, live);
        }
    }
    return true;
}

bool CDedupStore::Load(const std::wstring& storePath, const MapSet* liveMaps, const CancelCheck& cancelled)
{
    m_liveMaps = liveMaps;
    m_path = storePath;

    std::vector<std::wstring> deleteLogs;
    m_containers = ListContainers(JoinPath(storePath, L"Stream"), deleteLogs) | std::views::values |
        std::ranges::to<std::vector<Container>>();
    if (m_containers.empty() || std::ranges::any_of(m_containers, [](const Container& container) { return container.indexPath.empty(); }))
    {
        m_error = L"No stream containers found";
        return false;
    }
    LoadDeleteLogs(deleteLogs);

    // Register where every live map lives and collect its distinct chunks.
    // A chunk keeps the disk size of its last entry in visiting order, so the
    // entries are pushed backwards and both sorts are stable.
    struct ChunkRef
    {
        std::uint64_t digestKey;
        std::uint64_t diskSize;
    };
    std::vector<ChunkRef> refs;
    if (!ForEachMap(cancelled, [&](const std::uint32_t containerIndex, const std::uint32_t offset, const RecordView& record, const bool live)
    {
        // A hash stored twice is resolved once, so it is counted once
        if (!live || !m_maps.try_emplace(record.hash, MapLocation{ containerIndex, offset }).second) return;

        const std::size_t first = refs.size();
        for (std::size_t i = record.Count(); i-- > 0;)
        {
            const EntryView entry = ReadEntry(record, i);
            refs.push_back({ entry.digestKey, entry.diskSize });
        }
        const std::ranges::subrange mapRefs(refs.begin() + static_cast<std::ptrdiff_t>(first), refs.end());
        std::ranges::stable_sort(mapRefs, {}, &ChunkRef::digestKey);
        refs.erase(std::ranges::unique(mapRefs, {}, &ChunkRef::digestKey).begin(), refs.end());
    })) return false;

    // Count each chunk once per live stream map
    std::ranges::stable_sort(refs, {}, &ChunkRef::digestKey);
    for (const ChunkRef& ref : refs)
    {
        if (ref.diskSize > UINT32_MAX)
        {
            m_error = L"Unexpected chunk entry in a stream map";
            return false;
        }
        if (m_chunkKeys.empty() || m_chunkKeys.back() != ref.digestKey)
        {
            m_chunkKeys.push_back(ref.digestKey);
            m_chunks.emplace_back();
        }
        m_chunks.back().refs++;
        m_chunks.back().diskSize = static_cast<std::uint32_t>(ref.diskSize);
    }
    std::vector<ChunkRef>().swap(refs);
    m_chunkKeys.shrink_to_fit();
    m_chunks.shrink_to_fit();

    m_loaded = true;
    VTRACE(L"Dedup store {}: {} live maps, {} chunks", m_path, m_maps.size(), m_chunks.size());
    return true;
}

bool CDedupStore::Resolve(const std::span<const Hash16> requests, const ChunkVisitor& visitor, const CancelCheck& cancelled)
{
    if (!m_loaded) return false;

    // Group the wanted records by container so each container is read once
    std::vector<std::unordered_map<std::uint32_t, std::vector<std::size_t>>> perContainer(m_containers.size());
    for (const auto [requestIndex, hash] : std::views::enumerate(requests))
    {
        const auto location = m_maps.find(hash);
        if (location == m_maps.end()) continue;
        perContainer[location->second.container][location->second.offset].push_back(static_cast<std::size_t>(requestIndex));
    }

    std::vector<std::byte> dataBytes;
    std::vector<std::uint32_t> mapChunks;
    for (const auto [containerIndex, wantedRecords] : std::views::enumerate(perContainer))
    {
        if (wantedRecords.empty()) continue;
        if (cancelled && cancelled())
        {
            m_error = L"Cancelled";
            return false;
        }

        // The store may have been compacted since it was loaded; partial results
        // would leave files unaccounted while the whole store is neutralized
        const std::wstring& dataPath = m_containers[containerIndex].dataPath;
        if (!ReadFileBytes(dataPath, dataBytes, 4ull * wds::Gi))
        {
            m_error = L"Cannot read stream container: " + dataPath;
            return false;
        }

        for (const auto& [offset, requestIndices] : wantedRecords)
        {
            RecordView record;
            if (!ParseRecord(dataBytes, offset, record) || record.hash != requests[requestIndices.front()])
            {
                m_error = std::format(L"Stream map record at 0x{:x} in {} changed since loading", offset, dataPath);
                return false;
            }

            mapChunks.clear();
            for (std::size_t i = 0; i < record.Count(); i++)
            {
                if (const auto index = FindChunkIndex(ReadEntry(record, i).digestKey); index.has_value()) mapChunks.push_back(*index);
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

bool CDedupStore::ComputeSpaceUsage(const CancelCheck& cancelled)
{
    if (!m_loaded) return false;
    m_space = {};
    m_space.storeBytes = DirectorySize(m_path);
    for (const ChunkInfo& chunk : m_chunks) m_space.liveChunkBytes += chunk.diskSize;

    std::vector<std::wstring> dataDeleteLogs;
    const auto data = ListContainers(JoinPath(m_path, L"Data"), dataDeleteLogs);

    // Chunks a garbage collection already found dead ('Dlog': container id at
    // 0x08, then 8-byte entries starting with the chunk id)
    std::vector<std::uint64_t> loggedIds;
    {
        std::vector<std::byte> bytes;
        for (const auto& log : dataDeleteLogs)
        {
            if (!ReadFileBytes(log, bytes, 256ull * wds::Mi) || bytes.size() < 0x10 || !HasMagic(bytes, 0, "Dlog")) continue;
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
    const auto collect = [&](const RecordView& record, std::vector<std::uint64_t>& byOffset, std::vector<std::uint64_t>& byId)
    {
        for (std::size_t i = 0; i < record.Count(); i++)
        {
            const EntryView entry = ReadEntry(record, i);
            const auto container = data.find(entry.container);
            if (container != data.end() && (container->second.generation >> 16) == entry.generation)
                byOffset.push_back((static_cast<std::uint64_t>(entry.container) << 32) | entry.dataOffset);
            else byId.push_back((static_cast<std::uint64_t>(entry.container) << 32) | entry.chunkId);
        }
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
    std::array<std::byte, DataRecordHeaderSize> header{};
    for (const auto& [id, container] : data)
    {
        if (cancelled && cancelled()) return false;
        if (container.indexPath.empty() || !ReadFileBytes(container.indexPath, indexBytes, 256ull * wds::Mi) ||
            !ParseIndex(indexBytes, offsets) || offsets.empty()) continue;

        const SmartPointer handle(CloseHandle, CreateFileW(container.dataPath.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_RANDOM_ACCESS, nullptr));
        if (handle == INVALID_HANDLE_VALUE) continue;
        const auto readHeader = [&](const std::uint32_t offset)
        {
            OVERLAPPED position{};
            position.Offset = offset;
            DWORD read = 0;
            return ReadFile(handle, header.data(), static_cast<DWORD>(header.size()), &read, &position) &&
                read == header.size() && HasMagic(header, 0, "Ckhr");
        };

        std::ranges::sort(offsets);
        std::size_t deleted = 0;           // dead chunks a regular collection knows about
        ULONGLONG deletedBytes = 0;
        for (std::size_t i = 0; i < offsets.size(); i++)
        {
            const std::uint32_t offset = offsets[i];
            const std::uint64_t offsetKey = (static_cast<std::uint64_t>(id) << 32) | offset;
            if (std::ranges::binary_search(liveOffsets, offsetKey)) continue;
            if (!readHeader(offset)) continue;
            const std::uint64_t idKey = (static_cast<std::uint64_t>(id) << 32) | ReadAt<std::uint32_t>(header, 0x08);
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
    m_space.overheadBytes = m_space.storeBytes > accounted ? m_space.storeBytes - accounted : 0;
    VTRACE(L"Dedup store space: store {} live {} garbage {} (regular {}) overhead {}",
        m_space.storeBytes, m_space.liveChunkBytes, m_space.garbageBytes, m_space.regularGarbageBytes, m_space.overheadBytes);
    return true;
}
