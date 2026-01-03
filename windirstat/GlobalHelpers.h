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

constexpr auto CONTENT_MENU_MINCMD = 0x1ul;
constexpr auto CONTENT_MENU_MAXCMD = 0x7FFFul;

IContextMenu* GetContextMenu(HWND hwnd, const std::vector<std::wstring>& paths);

inline bool IsControlKeyDown() { return (HSHELL_HIGHBIT & GetKeyState(VK_CONTROL)) != 0; };
inline bool IsShiftKeyDown() { return (HSHELL_HIGHBIT & GetKeyState(VK_SHIFT)) != 0; };

constexpr auto signum(auto x) { return x < 0 ? -1 : x == 0 ? 0 : 1; };
constexpr auto usignum(auto x, auto y) { return x < y ? -1 : x == y ? 0 : 1; };

constexpr auto FILE_PROVIDER_COMPRESSION_MODERN = 1u << 8;
using CompressionAlgorithm = enum CompressionAlgorithm {
    NONE = COMPRESSION_FORMAT_NONE,
    LZNT1 = COMPRESSION_FORMAT_LZNT1,
    XPRESS4K = FILE_PROVIDER_COMPRESSION_XPRESS4K | FILE_PROVIDER_COMPRESSION_MODERN,
    XPRESS8K = FILE_PROVIDER_COMPRESSION_XPRESS8K | FILE_PROVIDER_COMPRESSION_MODERN,
    XPRESS16K = FILE_PROVIDER_COMPRESSION_XPRESS16K | FILE_PROVIDER_COMPRESSION_MODERN,
    LZX = FILE_PROVIDER_COMPRESSION_LZX | FILE_PROVIDER_COMPRESSION_MODERN
};

bool CompressFile(const std::wstring& filePath, CompressionAlgorithm algorithm);
bool CompressFileAllowed(const std::wstring& filePath, CompressionAlgorithm algorithm);

// Used at runtime to distinguish between mount points and junction points since they
// share the same reparse tag on the file system.
constexpr DWORD IO_REPARSE_TAG_JUNCTION_POINT = ~IO_REPARSE_TAG_MOUNT_POINT;

template<typename T>
constexpr T* ByteOffset(void* ptr, const std::ptrdiff_t offset)
{
    return reinterpret_cast<T*>(static_cast<std::byte*>(ptr) + offset);
}

std::wstring GetLocaleString(LCTYPE lctype, LCID lcid);
std::wstring GetLocaleLanguage(LANGID langid);
wchar_t GetLocaleThousandSeparator();
wchar_t GetLocaleDecimalSeparator();
std::wstring FormatBytes(ULONGLONG n);
std::wstring FormatSizeSuffixes(ULONGLONG n);
std::wstring FormatCount(ULONGLONG n);
std::wstring FormatDouble(double d);
std::wstring FormatFileTime(const FILETIME& t);
std::wstring FormatAttributes(DWORD attr);
std::wstring FormatMilliseconds(ULONGLONG ms);
bool GetVolumeName(const std::wstring& rootPath, std::wstring& volumeName);
std::wstring FormatVolumeNameOfRootPath(const std::wstring& rootPath);
std::wstring FormatVolumeName(const std::wstring& rootPath, const std::wstring& volumeName);
std::wstring GetFolderNameFromPath(const std::wstring& path);
std::wstring GetCOMSPEC();
void WaitForHandleWithRepainting(HANDLE h, DWORD TimeOut = INFINITE);
bool FolderExists(const std::wstring& path);
bool DriveExists(const std::wstring& path);
std::wstring MyQueryDosDevice(const std::wstring& drive);
bool IsSUBSTedDrive(const std::wstring& drive);
const std::wstring& GetSpec_Bytes();
const std::wstring& GetSpec_KiB();
const std::wstring& GetSpec_MiB();
const std::wstring& GetSpec_GiB();
const std::wstring& GetSpec_TiB();
bool IsElevationActive();
bool IsElevationAvailable();
void RunElevated(const std::wstring& cmdLine);
bool EnableReadPrivileges();
void SetProcessIoPriorityHigh();
void ReplaceString(std::wstring& subject, const std::wstring& search, const std::wstring& replace);
std::wstring& TrimString(std::wstring& s, wchar_t c = L' ', bool endOnly = false);
std::wstring MakeLower(const std::wstring& s);
const std::wstring& GetSysDirectory();
void ProcessMessagesUntilSignaled(const std::function<void()>& callback);
std::wstring GlobToRegex(const std::wstring& glob, bool useAnchors = true);
std::vector<BYTE> GetCompressedResource(HRSRC resource);
std::wstring GetVolumePathNameEx(const std::wstring& path);
void DisplayError(const std::wstring& error);
std::wstring TranslateError(HRESULT hr = static_cast<HRESULT>(GetLastError()));
void DisableHibernate();
bool IsHibernateEnabled();
bool ShellExecuteWrapper(const std::wstring& lpFile, const std::wstring& lpParameters = L"", const std::wstring& lpVerb = L"",
    HWND hwnd = *AfxGetMainWnd(), const std::wstring& lpDirector = L"", INT nShowCmd = SW_NORMAL);
std::wstring GetBaseNameFromPath(const std::wstring& path);
std::wstring GetAppFileName(const std::wstring& ext = L"");
std::wstring GetAppFolder();
std::wstring GetNameFromSid(PSID sid);
std::wstring ComputeFileHashes(const std::wstring& filePath);
void QueryShadowCopies(ULONGLONG& count, ULONGLONG& bytesUsed);
void RemoveWmiInstances(const std::wstring& wmiClass, std::atomic<size_t>& progress,
    const std::atomic<bool>& cancelRequested, const std::wstring& whereClause = L"__PATH IS NOT NULL");
bool OptimizeVhd(const std::wstring& vhdPath);

using CSmallRect = struct CSmallRect
{
    WORD left;
    WORD top;
    WORD right;
    WORD bottom;

    // Default constructor
    constexpr CSmallRect() : left(0), top(0), right(0), bottom(0) {}

    // Constructor from CRect
    explicit constexpr CSmallRect(const CRect& rect) :
        left(static_cast<WORD>(rect.left)), top(static_cast<WORD>(rect.top)),
        right(static_cast<WORD>(rect.right)), bottom(static_cast<WORD>(rect.bottom))
    {
    }

    // Assignment from CRect
    constexpr CSmallRect& operator=(const CRect& rect)
    {
        left = static_cast<WORD>(rect.left);
        top = static_cast<WORD>(rect.top);
        right = static_cast<WORD>(rect.right);
        bottom = static_cast<WORD>(rect.bottom);
        return *this;
    }

    // Conversion to CRect
    operator CRect() const
    {
        return { left, top, right, bottom };
    }
};
