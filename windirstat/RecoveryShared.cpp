// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "RecoveryShared.h"
#include "RecoveryNtfs.h"
#include "RecoveryExFat.h"
#include "RecoveryFat.h"

// Recycle Bin $I layouts have no padding between their on-disk fields.
#pragma pack(push, 1)
struct RecycleHeader
{
    ULONGLONG version;
    ULONGLONG originalSize;
    FILETIME deleted;
};

struct RecycleRecordV1
{
    RecycleHeader header;
    wchar_t path[MAX_PATH];
};

struct RecycleHeaderV2
{
    RecycleHeader header;
    DWORD pathLength;

    // Followed by pathLength UTF-16 characters, including the null terminator.
};
#pragma pack(pop)

static_assert(sizeof(RecycleHeader) == 24 && sizeof(RecycleRecordV1) == 544 && sizeof(RecycleHeaderV2) == 28);

constexpr ULONGLONG RecycleVersionFixedPath = 1;
constexpr ULONGLONG RecycleVersionCountedPath = 2;
constexpr DWORD RecoveryPausePollMilliseconds = 25;

std::span<const BYTE> RecoveryShared::Slice(const std::span<const BYTE> bytes, const size_t offset, const size_t size)
{
    // Reject ranges outside the source buffer before creating a view.
    if (offset > bytes.size() || size > bytes.size() - offset) throw Failure{ {}, ERROR_INVALID_DATA };
    return bytes.subspan(offset, size);
}

std::wstring RecoveryShared::ReadName(const std::span<const BYTE> bytes, const size_t offset, const size_t length)
{
    // Copy the requested UTF-16 field and reject embedded null characters.
    const auto data = Slice(bytes, offset, length * sizeof(wchar_t));
    std::wstring result(length, L'\0');
    std::memcpy(result.data(), data.data(), data.size());
    if (result.find(L'\0') != std::wstring::npos) throw Failure{ {}, ERROR_INVALID_DATA };
    return result;
}

void RecoveryShared::Progress::Check() const
{
    // Wait while paused, and propagate cancellation as a recovery failure.
    while (paused.load(std::memory_order_relaxed) && !cancel.load(std::memory_order_relaxed))
        Sleep(RecoveryPausePollMilliseconds);
    if (cancel.load(std::memory_order_relaxed)) throw Failure{ {}, ERROR_CANCELLED };
}

std::unique_ptr<RecoveryShared> RecoveryShared::Open(const std::wstring& root, Progress* progress)
{
    if (progress) progress->Check();

    // Identify the volume before selecting its filesystem's recovery reader.
    wchar_t volume[MAX_PATH] = {}, filesystem[MAX_PATH] = {};
    if (!GetVolumeNameForVolumeMountPointW(root.c_str(), volume, MAX_PATH) ||
        !GetVolumeInformationW(volume, nullptr, 0, nullptr, nullptr, nullptr, filesystem, MAX_PATH))
        throw Failure{ {}, GetLastError() };
    if (_wcsicmp(filesystem, L"NTFS") == 0) return std::make_unique<NtfsRecovery>(volume, progress);
    if (_wcsicmp(filesystem, L"exFAT") == 0) return std::make_unique<ExFatRecovery>(volume, progress);
    if (_wcsicmp(filesystem, L"FAT") == 0 || _wcsicmp(filesystem, L"FAT32") == 0)
        return std::make_unique<FatRecovery>(volume, progress);
    throw Failure{ {}, ERROR_NOT_SUPPORTED };
}

RecoveryShared::RecoveryShared(const std::wstring& volumeName) : m_name(volumeName)
{
    // Open the source volume for shared, unbuffered reads.
    if (volumeName.empty() || volumeName.back() != L'\\') throw Failure{ {}, ERROR_INVALID_NAME };
    m_volume = CreateFileW(volumeName.substr(0, volumeName.size() - 1).c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING, nullptr);
    if (!m_volume.IsValid()) throw Failure{ {}, GetLastError() };
}

std::wstring RecoveryShared::Recover(const Record& record, const std::wstring& folder, Progress& progress)
{
    // Resolve the destination and honor cancellation before filesystem-specific recovery.
    progress.Check();
    return RecoverFile(record, ValidateDestination(folder), progress);
}

void RecoveryShared::ReadVolume(const ULONGLONG offset, const std::span<BYTE> bytes,
    const ULONGLONG length, const DWORD alignment)
{
    // Validate the byte range before rounding it out to complete sectors.
    if (!std::has_single_bit(alignment) || alignment > RawBufferAlignment || bytes.empty() ||
        bytes.size() > MAXDWORD - 2 * alignment || length > LLONG_MAX ||
        offset > length || bytes.size() > length - offset) throw Failure{ {}, ERROR_INVALID_DATA };
    const DWORD prefix = static_cast<DWORD>(offset % alignment);
    const size_t take = (prefix + bytes.size() + alignment - 1) & ~size_t(alignment - 1);
    if (take > length - (offset - prefix)) throw Failure{ {}, ERROR_INVALID_DATA };

    // Grow the aligned buffer only when the current read needs more space.
    if (take > m_rawBufferSize)
    {
        m_rawBuffer = _aligned_malloc(take, RawBufferAlignment);
        m_rawBufferSize = m_rawBuffer.IsValid() ? take : 0;
    }
    if (!m_rawBuffer.IsValid()) throw Failure{ {}, ERROR_NOT_ENOUGH_MEMORY };

    // Verify the full raw read before copying the requested bytes.
    LARGE_INTEGER position{ .QuadPart = static_cast<LONGLONG>(offset - prefix) };
    DWORD returned = 0;
    if (!SetFilePointerEx(m_volume, position, nullptr, FILE_BEGIN) ||
        !ReadFile(m_volume, m_rawBuffer.Get(), static_cast<DWORD>(take), &returned, nullptr))
        throw Failure{ {}, GetLastError() };
    if (returned != take) throw Failure{ {}, ERROR_HANDLE_EOF };
    std::memcpy(bytes.data(), static_cast<const BYTE*>(m_rawBuffer.Get()) + prefix, bytes.size());
}

std::wstring RecoveryShared::ValidateDestination(const std::wstring& folder)
{
    const Handle directory(CloseHandle, CreateFileW(folder.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (!directory.IsValid()) throw Failure{ {}, GetLastError() };
    std::vector<wchar_t> path(MaxPathLength);

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

std::optional<RecoveryShared::RecycleInfo> RecoveryShared::ParseRecycleInfo(const std::span<const BYTE> bytes)
{
    if (bytes.size() < sizeof(RecycleHeaderV2) ||
        bytes.size() > sizeof(RecycleHeaderV2) + MaxPathLength * sizeof(wchar_t)) return {};
    const auto header = Read<RecycleHeader>(bytes, 0);
    std::wstring path;

    // Vista records have a fixed path buffer; newer records store its UTF-16 character count.
    if (header.version == RecycleVersionFixedPath)
    {
        if (bytes.size() != sizeof(RecycleRecordV1)) return {};
        const auto record = Read<RecycleRecordV1>(bytes, 0);
        const auto end = std::ranges::find(record.path, L'\0');
        if (end == std::end(record.path)) return {};
        path.assign(std::begin(record.path), end);
    }
    else if (header.version == RecycleVersionCountedPath)
    {
        const auto record = Read<RecycleHeaderV2>(bytes, 0);
        if (record.pathLength < 2 || record.pathLength > MaxPathLength ||
            bytes.size() != sizeof(record) + record.pathLength * sizeof(wchar_t) ||
            Read<wchar_t>(bytes, bytes.size() - sizeof(wchar_t)) != L'\0') return {};
        path = ReadName(bytes, sizeof(record), record.pathLength - 1);
    }
    else return {};
    const std::filesystem::path original(path);
    const auto name = original.filename().wstring();
    if (!original.is_absolute() || name.empty() || name == L"." || name == L"..") return {};

    // Damaged metadata must not supply control characters or incomplete Unicode names.
    for (size_t i = 0; i < path.size(); ++i)
    {
        const wchar_t c = path[i];
        if (c < FirstPrintableCharacter || IS_LOW_SURROGATE(c)) return {};
        if (!IS_HIGH_SURROGATE(c)) continue;
        if (++i == path.size() || !IS_LOW_SURROGATE(path[i])) return {};
    }
    return RecycleInfo{ header.originalSize, std::move(path) };
}

void RecoveryShared::ResolveRecyclePaths(Progress& progress, ScanResult& result,
    const std::function<void(const Record&, std::span<BYTE>)>& read)
{
    const auto less = [](const std::wstring& left, const std::wstring& right)
    {
        // Group companion paths without regard to letter case.
        return _wcsicmp(left.c_str(), right.c_str()) < 0;
    };
    std::map<std::wstring, RecycleInfo, decltype(less)> recycled(less);

    // Capture small, readable $I companions before changing any filesystem names or paths.
    for (const auto& record : result.records)
    {
        progress.Check();
        std::wstring_view path = record.path;
        if (path.starts_with(L"\\")) path.remove_prefix(1);
        constexpr std::wstring_view root = L"$Recycle.Bin\\";
        if (path.size() < root.size() || _wcsnicmp(path.data(), root.data(), root.size()) != 0 ||
            record.name.size() < RecycleNameStemLength || _wcsnicmp(record.name.c_str(), L"$I", 2) != 0 ||
            (record.name.size() > RecycleNameStemLength && record.name[RecycleNameStemLength] != L'.') ||
            record.data.size < sizeof(RecycleHeaderV2) ||
            record.data.size > sizeof(RecycleHeaderV2) + MaxPathLength * sizeof(wchar_t) ||
            record.data.initialized != record.data.size) continue;
        try
        {
            std::vector<BYTE> bytes;
            std::span<const BYTE> data = record.data.resident;
            if (record.data.nonresident)
            {
                bytes.resize(static_cast<size_t>(record.data.size));
                read(record, bytes);
                data = bytes;
            }
            auto info = ParseRecycleInfo(data);
            if (!info) continue;
            auto key = record.path;
            key[key.size() - record.name.size() + 1] = L'R';
            const auto [entry, inserted] = recycled.try_emplace(std::move(key), *info);
            if (!inserted && (entry->second.size != info->size || entry->second.path != info->path))
                entry->second.path.clear();
        }
        catch (const Failure& failure)
        {
            if (failure.error == ERROR_CANCELLED || failure.error == ERROR_OPERATION_ABORTED) throw;
        }
    }
    if (recycled.empty()) return;

    // Match a file or recycled ancestor within the same bin directory, including the user's SID.
    for (auto& record : result.records)
    {
        progress.Check();
        for (size_t end = record.path.size(); end != std::wstring::npos && end != 0;
            end = record.path.rfind(L'\\', end - 1))
        {
            const auto entry = recycled.find(record.path.substr(0, end));
            if (entry == recycled.end() || entry->second.path.empty() ||
                (end == record.path.size() && entry->second.size != record.data.size)) continue;
            record.path = entry->second.path + record.path.substr(end);
            record.name = std::filesystem::path(record.path).filename().wstring();
            break;
        }
    }
}

std::wstring RecoveryShared::SafeName(const std::wstring_view name)
{
    std::wstring result(name.substr(0, MaxFileNameLength));

    // Prefix only reserved device names, leaving ordinary recovered filenames unchanged.
    auto stem = result.substr(0, result.find(L'.'));
    while (!stem.empty() && stem.back() == L' ') stem.pop_back();
    bool reserved = stem.size() == 4 && std::wstring_view(L"123456789\u00B9\u00B2\u00B3").contains(stem[3]) &&
        (_wcsnicmp(stem.c_str(), L"COM", 3) == 0 || _wcsnicmp(stem.c_str(), L"LPT", 3) == 0);
    for (const auto device : { L"CON", L"PRN", L"AUX", L"NUL", L"CONIN$", L"CONOUT$" })
        reserved |= _wcsicmp(stem.c_str(), device) == 0;
    if (reserved) result = L"_" + result.substr(0, MaxFileNameLength - 1);

    // Preserve complete surrogate pairs while replacing characters that Windows cannot use in filenames.
    if (!result.empty() && IS_HIGH_SURROGATE(result.back())) result.pop_back();
    for (size_t i = 0; i < result.size(); ++i)
    {
        wchar_t& c = result[i];
        if (IS_HIGH_SURROGATE(c) && i + 1 < result.size() &&
            IS_LOW_SURROGATE(result[i + 1])) { ++i; continue; }
        if (c < FirstPrintableCharacter || IS_HIGH_SURROGATE(c) || IS_LOW_SURROGATE(c) ||
            std::wstring_view(L"<>:\"/\\|?*").contains(c)) c = L'_';
    }
    while (!result.empty() && (result.back() == L'.' || result.back() == L' ')) result.pop_back();
    return result.empty() ? L"_" : result;
}

RecoveryShared::OutputFile::OutputFile(const std::wstring& folder, const Record& record)
{
    ULARGE_INTEGER free = {};
    if (!GetDiskFreeSpaceExW(folder.c_str(), &free, nullptr, nullptr)) throw Failure{ {}, GetLastError() };
    if (free.QuadPart < record.data.size) throw Failure{ {}, ERROR_DISK_FULL };

    // Keep record identifiers on temporary files so completed files retain their recovered names.
    m_path = (std::filesystem::path(folder) / SafeName(record.name)).wstring();
    auto temporary = (std::filesystem::path(folder) /
        std::format(L"{}-{}.partial", record.number, record.sequence)).wstring();
    if (_wcsicmp(temporary.c_str(), m_path.c_str()) == 0) temporary += L".partial";
    m_file = CreateFileW(temporary.c_str(), GENERIC_WRITE | DELETE,
        0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!m_file.IsValid()) throw Failure{ {}, GetLastError() };
}

RecoveryShared::OutputFile::~OutputFile()
{
    // Delete an unfinished temporary file when recovery exits before commit.
    if (m_committed) return;
    FILE_DISPOSITION_INFO info{ TRUE };
    SetFileInformationByHandle(m_file, FileDispositionInfo, &info, sizeof(info));
}

void RecoveryShared::OutputFile::Write(const std::span<const BYTE> bytes, Progress& progress) const
{
    // Honor cancellation and require the entire output chunk to be written.
    progress.Check();
    if (bytes.size() > MAXDWORD) throw Failure{ {}, ERROR_BUFFER_OVERFLOW };
    DWORD written = 0;
    if (!WriteFile(m_file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr))
        throw Failure{ {}, GetLastError() };
    if (written != bytes.size()) throw Failure{ {}, ERROR_WRITE_FAULT };
}

void RecoveryShared::OutputFile::Commit(const std::function<void()>& check,
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
