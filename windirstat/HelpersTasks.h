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

#pragma once

#include "pch.h"
#include "ProgressDlg.h"

constexpr auto signum(auto x) noexcept { return x < 0 ? -1 : x == 0 ? 0 : 1; };
constexpr auto usignum(auto x, auto y) noexcept { return x < y ? -1 : x == y ? 0 : 1; };

constexpr auto FILE_PROVIDER_COMPRESSION_MODERN = 1u << 8;
using CompressionAlgorithm = enum CompressionAlgorithm {
    NONE = COMPRESSION_FORMAT_NONE,
    LZNT1 = COMPRESSION_FORMAT_LZNT1,
    XPRESS4K = FILE_PROVIDER_COMPRESSION_XPRESS4K | FILE_PROVIDER_COMPRESSION_MODERN,
    XPRESS8K = FILE_PROVIDER_COMPRESSION_XPRESS8K | FILE_PROVIDER_COMPRESSION_MODERN,
    XPRESS16K = FILE_PROVIDER_COMPRESSION_XPRESS16K | FILE_PROVIDER_COMPRESSION_MODERN,
    LZX = FILE_PROVIDER_COMPRESSION_LZX | FILE_PROVIDER_COMPRESSION_MODERN
};

// Used at runtime to distinguish between mount points and junction points since they
// share the same reparse tag on the file system.
constexpr DWORD IO_REPARSE_TAG_JUNCTION_POINT = ~IO_REPARSE_TAG_MOUNT_POINT;

template<typename T>
constexpr T* ByteOffset(void* ptr, const std::ptrdiff_t offset) noexcept
{
    return reinterpret_cast<T*>(static_cast<std::byte*>(ptr) + offset);
}

// Task helpers declarations grouped by functionality

// WMI helpers
void QueryShadowCopies(ULONGLONG& count, ULONGLONG& bytesUsed);
void RemoveWmiInstances(const std::wstring& wmiClass, CProgressDlg* pdlg, const std::wstring& whereClause = L"__PATH IS NOT NULL");

// File system helpers
bool FolderExists(const std::wstring& path) noexcept;
bool DriveExists(const std::wstring& path) noexcept;
bool IsLocalDrive(const std::wstring& rootPath) noexcept;

// Volume utilities
bool GetVolumeName(const std::wstring& rootPath, std::wstring& volumeName);
std::wstring GetVolumePathNameEx(const std::wstring& path);

// Path utilities
std::wstring MyQueryDosDevice(const std::wstring& drive);
bool IsSUBSTedDrive(const std::wstring& drive);

// Hibernation
void DisableHibernate() noexcept;
bool IsHibernateEnabled() noexcept;

// Elevation and privileges
bool IsElevationActive() noexcept;
bool IsElevationAvailable() noexcept;
bool EnableReadPrivileges() noexcept;
void RunElevated(const std::wstring& cmdLine);

// SID helpers
std::wstring GetNameFromSid(PSID sid);

// Compression
bool CompressFileAllowed(const std::wstring& filePath, CompressionAlgorithm algorithm);
bool CompressFile(const std::wstring& filePath, CompressionAlgorithm algorithm);

// File hashing
std::wstring ComputeFileHashes(const std::wstring& filePath);

// I/O priority and VHD optimization
void SetProcessIoPriorityHigh() noexcept;
bool OptimizeVhd(const std::wstring& vhdPath) noexcept;

// Drive mappings
void CopyAllDriveMappings() noexcept;
