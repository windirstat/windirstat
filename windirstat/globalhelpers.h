// globalhelpers.h - Declaration of global helper functions
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2006, 2008 Oliver Schneider (assarbad.net)
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
// Author(s): - bseifert -> http://windirstat.info/contact/bernhard/
//            - assarbad -> http://windirstat.info/contact/oliver/
//
// $Id$

#ifndef __WDS_GLOBALHELPERS_H__
#define __WDS_GLOBALHELPERS_H__
#pragma once
#include "../common/wds_constants.h"

CString GetLocaleString(LCTYPE lctype, LANGID langid);
CString GetLocaleLanguage(LANGID langid);
CString GetLocaleThousandSeparator();
CString GetLocaleDecimalSeparator();
CString FormatBytes(ULONGLONG n);
CString FormatLongLongHuman(ULONGLONG n);
CString FormatCount(ULONGLONG n);
CString FormatDouble(double d);
CString PadWidthBlanks(CString n, int width);
CString FormatFileTime(const FILETIME& t);
CString FormatAttributes(DWORD attr);
CString FormatMilliseconds(DWORD ms);
CString GetParseNameOfMyComputer();
void GetPidlOfMyComputer(LPITEMIDLIST *ppidl);
void ShellExecuteWithAssocDialog(HWND hwnd, LPCTSTR filename);
bool GetVolumeName(LPCTSTR rootPath, CString& volumeName);
CString FormatVolumeNameOfRootPath(CString rootPath);
CString FormatVolumeName(CString rootPath, CString volumeName);
CString PathFromVolumeName(CString name);
CString GetFolderNameFromPath(LPCTSTR path);
CString GetCOMSPEC();
void WaitForHandleWithRepainting(HANDLE h);
bool FolderExists(LPCTSTR path);
bool DriveExists(const CString& path);
CString GetUserName();
CString MyQueryDosDevice(LPCTSTR drive);
bool IsSUBSTedDrive(LPCTSTR drive);
CString GetSpec_Bytes();
CString GetSpec_KB();
CString GetSpec_MB();
CString GetSpec_GB();
CString GetSpec_TB();

#endif // __WDS_GLOBALHELPERS_H__
