// CommonHelpers.h - Functions used by WinDirStat.exe and setup.exe
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
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

bool ShellExecuteThrow(HWND hwnd, const std::wstring& lpVerb, const std::wstring& lpFile, const std::wstring& lpDirectory, INT nShowCmd);
std::wstring GetBaseNameFromPath(const std::wstring& path);
std::wstring GetAppFileName(const std::wstring& ext = L"");
std::wstring GetAppFolder();
std::wstring GetNameFromSid(PSID sid);

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

bool CompressFile(const std::wstring& filePath, CompressionAlgorithm algorithm);
bool CompressFileAllowed(const std::wstring& filePath, CompressionAlgorithm algorithm);
