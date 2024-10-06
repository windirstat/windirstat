// GlobalHelpers.h - Declaration of global helper functions
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
// Copyright (C) 2010 Chris Wimmer
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

#include <string>
#include <functional>

std::wstring GetLocaleString(LCTYPE lctype, LANGID langid);
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
std::wstring GetParseNameOfMyComputer();
void GetPidlOfMyComputer(LPITEMIDLIST* ppidl);
bool GetVolumeName(const std::wstring & rootPath, std::wstring& volumeName);
std::wstring FormatVolumeNameOfRootPath(const std::wstring& rootPath);
std::wstring FormatVolumeName(const std::wstring& rootPath, const std::wstring& volumeName);
std::wstring PathFromVolumeName(const std::wstring& name);
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
bool FileIconInit();
bool EnableReadPrivileges();
void ReplaceString(std::wstring& subject, const std::wstring& search, const std::wstring& replace);
std::wstring& TrimString(std::wstring& s, wchar_t c = L' ');
std::wstring& MakeLower(std::wstring& s);
const std::wstring& GetSysDirectory();
void ProcessMessagesUntilSignaled(const std::function<void()>& callback);
