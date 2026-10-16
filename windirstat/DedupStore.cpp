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

    // FNV-1a over raw bytes
    std::uint64_t HashBytes(const void* data, const std::size_t size, std::uint64_t hash = 0xcbf29ce484222325ull) noexcept
    {
        for (std::size_t i = 0; i < size; i++)
        {
            hash ^= static_cast<const std::uint8_t*>(data)[i];
            hash *= 0x100000001b3ull;
        }
        return hash;
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

ULONGLONG CDedupStore::DirectorySize(const std::wstring& dir)
{
    ULONGLONG total = 0;
    EnumerateDirectory(dir, [&](const WIN32_FIND_DATAW& data)
    {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) total += DirectorySize(JoinPath(dir, data.cFileName));
        else total += FileSizeOf(data);
    });
    return total;
}

bool CDedupStore::EnumerateVolumeFiles(const std::wstring& volumeRoot, FileMaps& files, const bool keepKnown,
    const CancelCheck& cancelled, const ProgressCallback& progress)
{
    FileMaps known = keepKnown ? std::move(files) : FileMaps{};
    files.clear();

    using NtQueryDirectoryFileFn = NTSTATUS(WINAPI*)(HANDLE, HANDLE, PVOID, PVOID, PIO_STATUS_BLOCK, PVOID,
        ULONG, FILE_INFORMATION_CLASS, BOOLEAN, PUNICODE_STRING, BOOLEAN);
    static const auto queryDirectory = reinterpret_cast<NtQueryDirectoryFileFn>(
        GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtQueryDirectoryFile"));
    if (queryDirectory == nullptr) return false;

    std::wstring root = Finder::MakeLongPathCompatible(volumeRoot);
    if (root.empty()) return false;
    if (root.back() != L'\\') root += L'\\';

    // The $R index of $Extend\$Reparse lists every reparse point of the volume
    // as (file reference, tag) pairs
    const SmartPointer index(CloseHandle, CreateFileW((root + L"$Extend\\$Reparse:$R:$INDEX_ALLOCATION").c_str(),
        GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (index == INVALID_HANDLE_VALUE) return false;

    constexpr auto FileReparsePointInformation = static_cast<FILE_INFORMATION_CLASS>(33);
    constexpr NTSTATUS StatusNoMoreFiles = static_cast<NTSTATUS>(0x80000006L);
    struct ReparsePointInformation
    {
        LONGLONG fileReference;
        ULONG tag;
    };

    // Known files keep their stream maps: those only change when a file is
    // optimized again, which changes the store as well
    FileMaps found;
    std::vector<ULONGLONG> unknown;
    std::vector<std::byte> buffer(64 * wds::Ki);
    for (bool restart = true;; restart = false)
    {
        IO_STATUS_BLOCK status{};
        const NTSTATUS result = queryDirectory(index, nullptr, nullptr, nullptr, &status, buffer.data(),
            static_cast<ULONG>(buffer.size()), FileReparsePointInformation, FALSE, nullptr, restart ? TRUE : FALSE);
        if (result == StatusNoMoreFiles) break;
        if (result < 0 || (cancelled && cancelled())) return false;

        const std::size_t returned = std::min<std::size_t>(status.Information, buffer.size());
        for (std::size_t offset = 0; offset + sizeof(ReparsePointInformation) <= returned; offset += sizeof(ReparsePointInformation))
        {
            const auto entry = ReadAt<ReparsePointInformation>(buffer, offset);
            if (entry.tag != static_cast<ULONG>(IO_REPARSE_TAG_DEDUP)) continue;
            if (auto node = known.extract(static_cast<ULONGLONG>(entry.fileReference)); !node.empty()) found.insert(std::move(node));
            else unknown.push_back(static_cast<ULONGLONG>(entry.fileReference));
        }
    }
    if (cancelled && cancelled()) return false;

    // Read the reparse data of each new optimized file by its id
    const SmartPointer rootHandle(CloseHandle, CreateFileW(root.c_str(), 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (rootHandle == INVALID_HANDLE_VALUE) return false;

    std::vector<std::vector<Hash16>> hashes(unknown.size());
    std::atomic<std::size_t> failures = 0;
    std::atomic<std::size_t> done = 0;
    CancelLatch latch(cancelled);
    ParallelForEach(hashes, [&](std::vector<Hash16>& list)
    {
        if (latch()) return;
        FILE_ID_DESCRIPTOR id{ .dwSize = sizeof(FILE_ID_DESCRIPTOR), .Type = FileIdType };
        id.FileId.QuadPart = static_cast<LONGLONG>(unknown[static_cast<std::size_t>(&list - hashes.data())]);
        const SmartPointer handle(CloseHandle, OpenFileById(rootHandle, &id, FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT));
        if (const auto reparse = QueryReparsePoint(handle); !reparse.has_value()) failures++;
        else if (auto info = ParseReparseData(*reparse); info.has_value()) list = std::move(info->maps);
        if (progress && (++done & 0x3FF) == 0) progress(static_cast<double>(done) / static_cast<double>(hashes.size()));
    });

    // Files deleted between the index query and the read are expected; a
    // large share of failures means the enumeration cannot be trusted
    if (latch() || (cancelled && cancelled()) || failures > unknown.size() / 100 + 16) return false;

    const std::size_t kept = found.size();
    for (const auto [reference, list] : std::views::zip(unknown, hashes))
    {
        if (!list.empty()) found.emplace(reference, std::move(list));
    }
    files = std::move(found);
    VTRACE(L"Dedup volume {}: {} optimized files, {} kept, {} read ({} unreadable)",
        volumeRoot, files.size(), kept, unknown.size(), failures.load());
    return true;
}

std::optional<CDedupStore::ReparseInfo> CDedupStore::ReadReparseData(const std::wstring& path)
{
    const SmartPointer handle(CloseHandle, CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    const auto reparse = QueryReparsePoint(handle);
    return reparse.has_value() ? ParseReparseData(*reparse) : std::nullopt;
}

std::optional<CDedupStore::Snapshot> CDedupStore::TakeSnapshot(const std::wstring& storePath)
{
    // The container files define the model: their names, sizes and write times
    // are combined independently of the enumeration order
    Snapshot snapshot;
    bool listed = true;
    for (const std::wstring_view part : { L"Stream", L"Data" })
    {
        const std::wstring dir = JoinPath(storePath, part);
        auto& deleteLogs = part == L"Stream" ? snapshot.streamDeleteLogs : snapshot.dataDeleteLogs;
        listed = listed && EnumerateDirectory(dir, [&](const WIN32_FIND_DATAW& data)
        {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) return;
            const std::wstring_view name(data.cFileName);
            if (name.ends_with(L".delete.log"))
            {
                deleteLogs.push_back(JoinPath(dir, name));
                return;
            }
            std::uint64_t hash = HashBytes(part.data(), part.size() * sizeof(wchar_t));
            hash = HashBytes(name.data(), name.size() * sizeof(wchar_t), hash);
            const ULONGLONG size = FileSizeOf(data);
            hash = HashBytes(&size, sizeof(size), hash);
            hash = HashBytes(&data.ftLastWriteTime, sizeof(data.ftLastWriteTime), hash);
            snapshot.signature += hash * 0x9E3779B97F4A7C15ull;
        });
    }
    if (!listed) return std::nullopt;
    snapshot.storeBytes = DirectorySize(storePath);
    return snapshot;
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

std::map<std::uint32_t, CDedupStore::Container> CDedupStore::ListContainers(const std::wstring& dir)
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
            container.size = FileSizeOf(data);
        }
        else if (name->suffix == L".01.cd") sideFiles[sideKey(name->id, name->generation)][0] = std::move(path);
        else if (name->suffix == L".02.cd") sideFiles[sideKey(name->id, name->generation)][1] = std::move(path);
        else if (name->suffix == L".delete.map") sideFiles[sideKey(name->id, name->generation)][2] = std::move(path);
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

bool CDedupStore::ReadRelocationTable(const Container& files, const std::vector<std::uint32_t>& offsets,
    DataContainer& container, std::uint32_t& lastSpan)
{
    // The container header ('Cthr') has three copies at 4 KB steps; the one with
    // the highest sequence number at 0x08 is current. It holds the record count
    // at 0x18, the next local id at 0x1c and the valid data length at 0x20.
    // Compactions keep the local ids of the records they move in a relocation
    // table ('Rrtl' at 0x3000: count at 0x08, (id, offset) pairs from 0x20) in
    // record order; records written since get consecutive ids up to the next one.
    if (offsets.empty())
    {
        container.idsKnown = true;
        return true;
    }
    const std::uint32_t headerEnd = offsets.front();
    if (headerEnd < 0x3020 || headerEnd > 64 * wds::Mi) return false;

    std::vector<std::byte> header(headerEnd);
    {
        const SmartPointer handle(CloseHandle, CreateFileW(files.dataPath.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
        DWORD read = 0;
        if (handle == INVALID_HANDLE_VALUE || !ReadFile(handle, header.data(), headerEnd, &read, nullptr) || read != headerEnd) return false;
    }

    std::optional<std::size_t> current;
    for (std::size_t copy = 0; copy < 3; copy++)
    {
        const std::size_t base = copy * 0x1000;
        if (HasMagic(header, base, "Cthr") && (!current.has_value() ||
            ReadAt<std::uint64_t>(header, base + 0x08) > ReadAt<std::uint64_t>(header, *current + 0x08))) current = base;
    }
    if (!current.has_value() || ReadAt<std::uint32_t>(header, *current + 0x18) != offsets.size()) return false;
    const std::uint32_t nextId = ReadAt<std::uint32_t>(header, *current + 0x1c);
    const std::uint32_t validLength = ReadAt<std::uint32_t>(header, *current + 0x20);
    if (validLength > offsets.back()) lastSpan = validLength - offsets.back();

    const std::size_t relocated = HasMagic(header, 0x3000, "Rrtl") ? ReadAt<std::uint32_t>(header, 0x3008) : 0;
    if (relocated > offsets.size() || 0x3020 + 8ull * relocated > headerEnd) return false;
    container.relocatedIds.resize(relocated);
    for (std::size_t i = 0; i < relocated; i++)
    {
        const std::uint32_t id = ReadAt<std::uint32_t>(header, 0x3020 + 8 * i);
        if (ReadAt<std::uint32_t>(header, 0x3024 + 8 * i) != offsets[i] || (i > 0 && id <= container.relocatedIds[i - 1])) return false;
        container.relocatedIds[i] = id;
    }

    // A container that was never compacted numbers its records from 1
    const auto tail = static_cast<std::uint32_t>(offsets.size() - relocated);
    if (nextId < tail) return false;
    container.tailBase = nextId - tail;
    if (relocated == 0 && container.tailBase != 1) return false;
    if (relocated > 0 && tail > 0 && container.tailBase <= container.relocatedIds.back()) return false;
    container.idsKnown = true;
    return true;
}

void CDedupStore::ReadRecordIds(const Container& files, const std::vector<std::uint32_t>& offsets,
    DataContainer& container, std::uint32_t& lastSpan, const CancelCheck& cancelled)
{
    // Record headers ('Ckhr', 0x58 bytes) hold the local id at 0x08 and the size at 0x0c
    const SmartPointer handle(CloseHandle, CreateFileW(files.dataPath.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_RANDOM_ACCESS, nullptr));
    if (handle == INVALID_HANDLE_VALUE) return;

    std::array<std::byte, DataRecordHeaderSize> header{};
    container.headerIds.reserve(offsets.size());
    for (const auto [record, offset] : std::views::enumerate(offsets))
    {
        OVERLAPPED position{};
        position.Offset = offset;
        DWORD read = 0;
        if ((record & 0xFFF) == 0xFFF && cancelled && cancelled() ||
            !ReadFile(handle, header.data(), static_cast<DWORD>(header.size()), &read, &position) ||
            read != header.size() || !HasMagic(header, 0, "Ckhr"))
        {
            container.headerIds.clear();
            return;
        }
        container.headerIds.emplace_back(ReadAt<std::uint32_t>(header, 0x08), static_cast<std::uint32_t>(record));
        if (static_cast<std::size_t>(record) + 1 == offsets.size())
            lastSpan = (DataRecordHeaderSize + ReadAt<std::uint32_t>(header, 0x0c) + 7) & ~7u;
    }
    std::ranges::sort(container.headerIds);
    container.idsKnown = true;
}

const CDedupStore::DataContainer* CDedupStore::FindDataContainer(const std::uint32_t id) const noexcept
{
    const auto it = std::ranges::lower_bound(m_data, id, {}, &DataContainer::id);
    return it != m_data.end() && it->id == id ? &*it : nullptr;
}

std::uint32_t CDedupStore::FindRecord(const DataContainer& container, const std::uint32_t localId) const noexcept
{
    if (!container.idsKnown) return NoRecord;
    if (!container.headerIds.empty())
    {
        const auto it = std::ranges::lower_bound(container.headerIds, localId, {}, &std::pair<std::uint32_t, std::uint32_t>::first);
        return it != container.headerIds.end() && it->first == localId ? container.first + it->second : NoRecord;
    }

    const auto& relocated = container.relocatedIds;
    if (const auto it = std::ranges::lower_bound(relocated, localId); it != relocated.end() && *it == localId)
        return container.first + static_cast<std::uint32_t>(it - relocated.begin());
    const std::uint32_t tail = container.count - static_cast<std::uint32_t>(relocated.size());
    if (localId >= container.tailBase && localId - container.tailBase < tail)
        return container.first + static_cast<std::uint32_t>(relocated.size()) + (localId - container.tailBase);
    return NoRecord;
}

bool CDedupStore::Load(const std::wstring& storePath, const CancelCheck& cancelled, const ProgressCallback& progress)
{
    const auto isCancelled = [&]
    {
        if (!cancelled || !cancelled()) return false;
        m_error = L"Cancelled";
        return true;
    };

    m_streams = ListContainers(JoinPath(storePath, L"Stream")) | std::views::values | std::ranges::to<std::vector<Container>>();
    if (m_streams.empty() || std::ranges::any_of(m_streams, [](const Container& container) { return container.indexPath.empty(); }))
    {
        m_error = L"No stream containers found";
        return false;
    }

    // Data containers: record offsets from the index, local ids from the
    // relocation table or, should it not match the index, the record headers.
    // A container without a readable index is left out; references to it stay
    // unresolved and its garbage counts as overhead.
    const auto dataFiles = ListContainers(JoinPath(storePath, L"Data"));
    std::vector<const Container*> dataPaths;
    m_data.clear();
    for (const auto& [id, files] : dataFiles)
    {
        if (files.indexPath.empty()) continue;
        m_data.push_back({ .id = id, .generation = files.generation >> 16 });
        dataPaths.push_back(&files);
    }
    std::vector<std::vector<std::uint32_t>> offsets(m_data.size());
    std::vector<std::uint32_t> lastSpans(m_data.size(), static_cast<std::uint32_t>(DataRecordHeaderSize));
    std::atomic<bool> stopped = false;
    const CancelCheck stop = [&]
    {
        if (!stopped.load(std::memory_order_relaxed) && cancelled && cancelled()) stopped = true;
        return stopped.load(std::memory_order_relaxed);
    };
    ParallelForEach(m_data, [&](DataContainer& container)
    {
        if (stop()) return;
        const std::size_t index = static_cast<std::size_t>(&container - m_data.data());
        std::vector<std::byte> bytes;
        auto& recordOffsets = offsets[index];
        if (!ReadFileBytes(dataPaths[index]->indexPath, bytes, 256ull * wds::Mi) || !ParseIndex(bytes, recordOffsets))
        {
            recordOffsets.clear();
            return;
        }

        // Records are stored in index order; anything else is matched through the headers
        const bool ordered = std::ranges::is_sorted(recordOffsets);
        if (!ordered) std::ranges::sort(recordOffsets);
        if (!ordered || !ReadRelocationTable(*dataPaths[index], recordOffsets, container, lastSpans[index]))
        {
            container.relocatedIds.clear();
            ReadRecordIds(*dataPaths[index], recordOffsets, container, lastSpans[index], stop);
        }
    });
    if (stopped || isCancelled()) return false;

    // Chunks are numbered in 32 bits, data records first
    std::uint64_t dataRecords = 0;
    for (const auto [container, recordOffsets] : std::views::zip(m_data, offsets))
    {
        container.first = static_cast<std::uint32_t>(dataRecords);
        container.count = static_cast<std::uint32_t>(recordOffsets.size());
        dataRecords += container.count;
    }
    if (dataRecords >= NoRecord)
    {
        m_error = L"Too many chunks in the store";
        return false;
    }
    if (progress) progress(0.1);

    // Every stream map record, live or not, with its distinct chunks. An entry
    // whose generation matches the data container points straight at its
    // record; an older generation means a compaction moved the record, which
    // kept its local id. Unresolved chunks are told apart by their digest.
    const auto resolve = [&](const EntryView& entry) -> std::uint32_t
    {
        const DataContainer* container = FindDataContainer(entry.container);
        if (container == nullptr || entry.generation > container->generation) return NoRecord;
        if (entry.generation == container->generation)
        {
            const auto& recordOffsets = offsets[static_cast<std::size_t>(container - m_data.data())];
            if (const auto it = std::ranges::lower_bound(recordOffsets, entry.dataOffset);
                it != recordOffsets.end() && *it == entry.dataOffset) return container->first + static_cast<std::uint32_t>(it - recordOffsets.begin());
        }
        return FindRecord(*container, entry.chunkId);
    };

    m_chunkSize.assign(static_cast<std::size_t>(dataRecords), 0);
    m_records.clear();
    m_recordChunks.assign(1, 0);
    m_chunkList.clear();
    m_maps.clear();
    std::unordered_map<std::uint64_t, std::uint32_t> unresolved; // digest prefix -> chunk after the data records
    std::vector<std::byte> indexBytes;
    std::vector<std::byte> dataBytes;
    std::vector<std::byte> bitmapBytes;
    std::vector<std::uint32_t> streamOffsets;
    std::vector<std::uint32_t> chunks;
    ULONGLONG streamBytes = 0;
    ULONGLONG streamBytesDone = 0;
    for (const Container& container : m_streams) streamBytes += container.size;
    for (const auto [containerIndex, container] : std::views::enumerate(m_streams))
    {
        if (isCancelled()) return false;
        if (!ReadFileBytes(container.indexPath, indexBytes, 64ull * wds::Mi) || !ParseIndex(indexBytes, streamOffsets))
        {
            m_error = L"Invalid stream index: " + container.indexPath;
            return false;
        }
        if (!ReadFileBytes(container.dataPath, dataBytes, 4ull * wds::Gi))
        {
            m_error = L"Cannot read stream container: " + container.dataPath;
            return false;
        }

        // The bitmap only decides liveness when no file-based liveness is available
        std::vector<bool> deleted(streamOffsets.size(), false);
        if (!container.deleteMapPath.empty() && ReadFileBytes(container.deleteMapPath, bitmapBytes, 64ull * wds::Mi))
            deleted = ParseDeleteBitmap(bitmapBytes, streamOffsets.size());

        for (const auto [recordIndex, offset] : std::views::enumerate(streamOffsets))
        {
            RecordView record;
            if (!ParseRecord(dataBytes, offset, record))
            {
                m_error = std::format(L"Unexpected stream map record at 0x{:x} in {}", offset, container.dataPath);
                return false;
            }

            chunks.clear();
            for (std::size_t i = 0; i < record.Count(); i++)
            {
                const EntryView entry = ReadEntry(record, i);
                if (entry.diskSize > UINT32_MAX)
                {
                    m_error = L"Unexpected chunk entry in a stream map";
                    return false;
                }
                std::uint32_t chunk = resolve(entry);
                if (chunk == NoRecord)
                {
                    if (m_chunkSize.size() >= NoRecord)
                    {
                        m_error = L"Too many chunks in the store";
                        return false;
                    }
                    const auto [it, added] = unresolved.try_emplace(entry.digestKey, static_cast<std::uint32_t>(m_chunkSize.size()));
                    if (added) m_chunkSize.push_back(static_cast<std::uint32_t>(entry.diskSize));
                    chunk = it->second;
                }
                else if (m_chunkSize[chunk] == 0) m_chunkSize[chunk] = static_cast<std::uint32_t>(entry.diskSize);
                chunks.push_back(chunk);
            }
            std::ranges::sort(chunks);
            m_chunkList.insert(m_chunkList.end(), chunks.begin(), std::ranges::unique(chunks).begin());
            m_recordChunks.push_back(m_chunkList.size());

            const auto map = m_maps.try_emplace(record.hash, static_cast<std::uint32_t>(m_maps.size())).first->second;
            m_records.push_back({ record.hash, map, static_cast<std::uint32_t>(containerIndex), record.size,
                deleted[static_cast<std::size_t>(recordIndex)] });
        }

        streamBytesDone += container.size;
        if (progress) progress(0.1 + 0.9 * static_cast<double>(streamBytesDone) / static_cast<double>(std::max(streamBytes, 1ull)));
    }
    m_chunkList.shrink_to_fit();
    m_chunkSize.shrink_to_fit();

    // Bytes each data record takes; records are 8-byte aligned, the last one
    // is sized from a stream map entry or else the valid data length
    m_span.assign(static_cast<std::size_t>(dataRecords), 0);
    for (const auto [container, recordOffsets, lastSpan] : std::views::zip(m_data, offsets, lastSpans))
    {
        for (std::uint32_t i = 0; i < container.count; i++)
        {
            const std::uint32_t record = container.first + i;
            if (i + 1 < container.count) m_span[record] = recordOffsets[i + 1] - recordOffsets[i];
            else m_span[record] = m_chunkSize[record] != 0 ?
                static_cast<std::uint32_t>((DataRecordHeaderSize + m_chunkSize[record] + 7) & ~7ull) : lastSpan;
        }
    }

    VTRACE(L"Dedup store {}: {} stream maps, {} data records, {} unresolved chunks",
        storePath, m_records.size(), dataRecords, unresolved.size());
    return true;
}

void CDedupStore::Evaluate(const MapSet* liveMaps, const Snapshot& snapshot)
{
    // Delete logs ('Dlog', 16-byte header, 40-byte entries of id + digest) list
    // the stream maps of deleted files that garbage collection has not yet
    // processed; afterwards the delete bitmaps take over.
    MapSet deadHashes;
    std::vector<std::byte> bytes;
    for (const auto& log : snapshot.streamDeleteLogs)
    {
        if (!ReadFileBytes(log, bytes, 64ull * wds::Mi) || !HasMagic(bytes, 0, "Dlog")) continue;
        for (std::size_t offset = 0x10; offset + 40 <= bytes.size(); offset += 40)
        {
            Hash16 hash;
            std::memcpy(hash.data(), bytes.data() + offset + 4, hash.size());
            deadHashes.insert(hash);
        }
    }

    enum : std::uint8_t { Live = 1, Deleted = 2 }; // deleted: known to a regular garbage collection
    std::vector<std::uint8_t> flags(m_chunkSize.size(), 0);
    m_refs.assign(m_chunkSize.size(), 0);
    m_mapRecord.assign(m_maps.size(), NoRecord);
    m_space = {};
    m_space.storeBytes = snapshot.storeBytes;

    // Stream maps no live file uses are garbage. A regular garbage collection
    // only handles maps listed in the delete logs and compacts a stream
    // container once at most 80% of it stays.
    struct StreamTally
    {
        std::size_t records = 0;
        std::size_t logged = 0;
        ULONGLONG loggedBytes = 0;
    };
    std::vector<StreamTally> tallies(m_streams.size());
    ULONGLONG garbage = 0;
    ULONGLONG regularGarbage = 0;
    for (const auto [recordIndex, record] : std::views::enumerate(m_records))
    {
        const auto index = static_cast<std::size_t>(recordIndex);
        const auto chunks = std::span(m_chunkList).subspan(m_recordChunks[index], m_recordChunks[index + 1] - m_recordChunks[index]);
        StreamTally& tally = tallies[record.container];
        tally.records++;
        if (liveMaps != nullptr ? liveMaps->contains(record.hash) : !record.bitmapDeleted && !deadHashes.contains(record.hash))
        {
            for (const std::uint32_t chunk : chunks) flags[chunk] |= Live;

            // A hash stored twice is one map, counted once
            if (m_mapRecord[record.map] != NoRecord) continue;
            m_mapRecord[record.map] = static_cast<std::uint32_t>(index);
            for (const std::uint32_t chunk : chunks) m_refs[chunk]++;
            continue;
        }
        garbage += record.size;
        if (deadHashes.contains(record.hash))
        {
            for (const std::uint32_t chunk : chunks) flags[chunk] |= Deleted;
            tally.logged++;
            tally.loggedBytes += record.size;
        }
    }
    for (const StreamTally& tally : tallies)
    {
        if (tally.logged > 0 && 100 * (tally.records - tally.logged) / tally.records <= StreamMinOccupancyPercent)
            regularGarbage += tally.loggedBytes;
    }
    for (const auto [refs, size] : std::views::zip(m_refs, m_chunkSize))
    {
        if (refs != 0) m_space.liveChunkBytes += size;
    }

    // Chunks a garbage collection already found dead ('Dlog': container id at
    // 0x08, then 8-byte entries starting with the local id)
    for (const auto& log : snapshot.dataDeleteLogs)
    {
        if (!ReadFileBytes(log, bytes, 256ull * wds::Mi) || bytes.size() < 0x10 || !HasMagic(bytes, 0, "Dlog")) continue;
        const DataContainer* container = FindDataContainer(ReadAt<std::uint32_t>(bytes, 0x08));
        if (container == nullptr) continue;
        for (std::size_t offset = 0x10; offset + 8 <= bytes.size(); offset += 8)
        {
            if (const std::uint32_t record = FindRecord(*container, ReadAt<std::uint32_t>(bytes, offset)); record != NoRecord)
                flags[record] |= Deleted;
        }
    }

    // Every data record that no live map reaches is garbage awaiting collection.
    // A regular collection compacts a container only once at most 95% of its
    // chunks stay; a full one compacts every container with garbage.
    for (const DataContainer& container : m_data)
    {
        // Moved records of a container without local ids were counted as unresolved chunks, so
        // its unreached records may be live; they stay in the overhead
        if (!container.idsKnown) continue;
        std::size_t deleted = 0;
        ULONGLONG deletedBytes = 0;
        for (std::uint32_t record = container.first; record < container.first + container.count; record++)
        {
            if ((flags[record] & Live) != 0) continue;
            garbage += m_span[record];
            if ((flags[record] & Deleted) == 0) continue;
            deleted++;
            deletedBytes += m_span[record];
        }
        if (deleted > 0 && 100 * (container.count - deleted) / container.count <= DataMinOccupancyPercent)
            regularGarbage += deletedBytes;
    }

    m_space.garbageBytes = garbage;
    m_space.regularGarbageBytes = std::min(regularGarbage, garbage);
    const ULONGLONG accounted = m_space.liveChunkBytes + m_space.garbageBytes;
    m_space.overheadBytes = m_space.storeBytes > accounted ? m_space.storeBytes - accounted : 0;
    VTRACE(L"Dedup store space: store {} live {} garbage {} (regular {}) overhead {}",
        m_space.storeBytes, m_space.liveChunkBytes, m_space.garbageBytes, m_space.regularGarbageBytes, m_space.overheadBytes);
}

std::span<const std::uint32_t> CDedupStore::GetMapChunks(const Hash16& hash) const noexcept
{
    const auto map = m_maps.find(hash);
    if (map == m_maps.end() || map->second >= m_mapRecord.size()) return {};
    const std::uint32_t record = m_mapRecord[map->second];
    if (record == NoRecord) return {};
    return std::span(m_chunkList).subspan(m_recordChunks[record], m_recordChunks[record + 1] - m_recordChunks[record]);
}
