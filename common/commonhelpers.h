// commonhelpers.h - Functions used by windirstat.exe and setup.exe
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

#ifndef __WDS_COMMONHELPERS_H__
#define __WDS_COMMONHELPERS_H__
#pragma once

#include "../common/wds_constants.h"

BOOL ShellExecuteThrow(HWND hwnd, LPCTSTR lpVerb, LPCTSTR lpFile, LPCTSTR lpParameters, LPCTSTR lpDirectory, INT nShowCmd);
BOOL ShellExecuteNoThrow(HWND hwnd, LPCTSTR lpVerb, LPCTSTR lpFile, LPCTSTR lpParameters, LPCTSTR lpDirectory, INT nShowCmd);
CString MyStrRetToString(const LPITEMIDLIST pidl, const STRRET *strret);
CString GetBaseNameFromPath(LPCTSTR path);
bool FileExists(LPCTSTR path);
CString LoadString(UINT resId);
CString GetAppFileName();
CString GetAppFolder();
CString MyGetFullPathName(LPCTSTR relativePath);

#endif // __WDS_COMMONHELPERS_H__
