// globalhelpers.h - Declaration of global helper functions
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2017 WinDirStat Team (windirstat.net)
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
#include <common/wds_constants.h>

CStringW GetLocaleString(LCTYPE lctype, LANGID langid);
CStringW GetLocaleLanguage(LANGID langid);
CStringW GetLocaleThousandSeparator();
CStringW GetLocaleDecimalSeparator();
CStringW FormatBytes(ULONGLONG const& n);
CStringW FormatLongLongHuman(ULONGLONG n);
CStringW FormatCount(ULONGLONG const& n);
CStringW FormatDouble(double d);
CStringW PadWidthBlanks(CStringW n, int width);
CStringW FormatFileTime(const FILETIME& t);
CStringW FormatAttributes(DWORD attr);
CStringW FormatMilliseconds(ULONGLONG ms);
CStringW GetParseNameOfMyComputer();
void GetPidlOfMyComputer(LPITEMIDLIST *ppidl);
void ShellExecuteWithAssocDialog(HWND hwnd, LPCWSTR filename);
bool GetVolumeName(LPCWSTR rootPath, CStringW& volumeName);
CStringW FormatVolumeNameOfRootPath(CStringW rootPath);
CStringW FormatVolumeName(CStringW rootPath, CStringW volumeName);
CStringW PathFromVolumeName(CStringW name);
CStringW GetFolderNameFromPath(LPCWSTR path);
CStringW GetCOMSPEC();
DWORD WaitForHandleWithRepainting(HANDLE h, DWORD TimeOut = INFINITE);
bool FolderExists(LPCWSTR path);
bool DriveExists(const CStringW& path);
CStringW GetUserName();
CStringW MyQueryDosDevice(LPCWSTR drive);
bool IsSUBSTedDrive(LPCWSTR drive);
CStringW GetSpec_Bytes();
CStringW GetSpec_KB();
CStringW GetSpec_MB();
CStringW GetSpec_GB();
CStringW GetSpec_TB();
BOOL IsAdmin(); 
