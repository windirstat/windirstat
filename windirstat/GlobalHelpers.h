// GlobalHelpers.h - Declaration of global helper functions
//
// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#pragma once

#include "stdafx.h"

#include <string>
#include <functional>

constexpr auto CONTENT_MENU_MINCMD = 0x1ul;
constexpr auto CONTENT_MENU_MAXCMD = 0x7FFFul;
IContextMenu* GetContextMenu(HWND hwnd, const std::vector<std::wstring>& paths);

constexpr auto FILE_PROVIDER_COMPRESSION_MODERN = 1u << 8;
enum class CompressionAlgorithm {
    NONE = COMPRESSION_FORMAT_NONE,
    LZNT1 = COMPRESSION_FORMAT_LZNT1,
    XPRESS4K = FILE_PROVIDER_COMPRESSION_XPRESS4K | FILE_PROVIDER_COMPRESSION_MODERN,
    XPRESS8K = FILE_PROVIDER_COMPRESSION_XPRESS8K | FILE_PROVIDER_COMPRESSION_MODERN,
    XPRESS16K = FILE_PROVIDER_COMPRESSION_XPRESS16K | FILE_PROVIDER_COMPRESSION_MODERN,
    LZX = FILE_PROVIDER_COMPRESSION_LZX | FILE_PROVIDER_COMPRESSION_MODERN,

};

std::wstring GetLocaleString(LCTYPE lctype, LCID lcid);
std::wstring GetLocaleLanguage(LANGID langid);
std::wstring GetLocaleThousandSeparator();
std::wstring GetLocaleDecimalSeparator();
std::wstring FormatBytes(const ULONGLONG& n);
std::wstring FormatSizeSuffixes(ULONGLONG n);
std::wstring FormatCount(const ULONGLONG& n);
std::wstring FormatDouble(double d);
std::wstring PadWidthBlanks(std::wstring n, int width);
std::wstring FormatFileTime(const FILETIME& t);
std::wstring FormatAttributes(DWORD attr);
std::wstring FormatMilliseconds(ULONGLONG ms);
bool GetVolumeName(const std::wstring & rootPath, std::wstring& volumeName);
std::wstring FormatVolumeNameOfRootPath(const std::wstring& rootPath);
std::wstring FormatVolumeName(const std::wstring& rootPath, const std::wstring& volumeName);
std::wstring GetFolderNameFromPath(const std::wstring & path);
std::wstring GetCOMSPEC();
void WaitForHandleWithRepainting(HANDLE h, DWORD TimeOut = INFINITE);
bool FolderExists(const std::wstring & path);
bool DriveExists(const std::wstring& path);
std::wstring MyQueryDosDevice(const std::wstring & drive);
bool IsSUBSTedDrive(const std::wstring & drive);
const std::wstring& GetSpec_Bytes();
const std::wstring& GetSpec_KB();
const std::wstring& GetSpec_MB();
const std::wstring& GetSpec_GB();
const std::wstring& GetSpec_TB();
bool IsAdmin();
bool EnableReadPrivileges();
void ReplaceString(std::wstring& subject, const std::wstring& search, const std::wstring& replace);
std::wstring& TrimString(std::wstring& s, wchar_t c = L' ');
std::wstring& MakeLower(std::wstring& s);
const std::wstring& GetSysDirectory();
void ProcessMessagesUntilSignaled(const std::function<void()>& callback);
std::wstring GlobToRegex(const std::wstring& glob);
std::vector<BYTE> GetCompressedResource(HRSRC resource);
std::wstring GetVolumePathNameEx(const std::wstring& path);
void DisplayError(const std::wstring& error);
std::wstring TranslateError(const HRESULT hr = static_cast<HRESULT>(GetLastError()));
void DisableHibernate();
bool IsHibernateEnabled();
bool ShellExecuteWrapper(const std::wstring& lpFile, const std::wstring& lpParameters = L"", const std::wstring& lpVerb = L"",
    HWND hwnd = *AfxGetMainWnd(), const std::wstring& lpDirector = L"", INT nShowCmd = SW_NORMAL);
std::wstring GetBaseNameFromPath(const std::wstring& path);
std::wstring GetAppFileName(const std::wstring& ext = L"");
std::wstring GetAppFolder();
std::wstring GetNameFromSid(PSID sid);


bool CompressFile(const std::wstring& filePath, CompressionAlgorithm algorithm);
bool CompressFileAllowed(const std::wstring& filePath, CompressionAlgorithm algorithm);
