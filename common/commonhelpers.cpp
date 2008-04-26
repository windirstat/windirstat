// commonhelpers.cpp - Implementation of common global helper functions
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
// Author(s): - bseifert -> bernhard@windirstat.info
//            - assarbad -> oliver@windirstat.info
//
// $Id$

#include <afxwin.h> // MFC core and standard components
#include <atlbase.h> // CComPtr, USES_CONVERSION

#include "mdexceptions.h"

#include "commonhelpers.h"

CString MyStrRetToString(const LPITEMIDLIST pidl, const STRRET *strret)
{
	// StrRetToStr() is not always available (e.g. on Windows 98).
	// So we use an own function instead.
	USES_CONVERSION;

	CString s;

	switch (strret->uType)
	{
	case STRRET_CSTR:
		{
			s = strret->cStr;
		}
		break;
	case STRRET_OFFSET:
		{
			s = A2T((char *)pidl + strret->uOffset);
		}
		break;
	case STRRET_WSTR:
		{
			s = W2T(strret->pOleStr);
		}
		break;
	}

	return s;
}
BOOL ShellExecuteNoThrow(HWND hwnd, LPCTSTR lpVerb, LPCTSTR lpFile, LPCTSTR lpParameters, LPCTSTR lpDirectory, INT nShowCmd)
{
	SHELLEXECUTEINFO sei = {
		sizeof(SHELLEXECUTEINFO),
		0,
		hwnd,
		lpVerb,
		lpFile,
		lpParameters,
		lpDirectory,
		nShowCmd,
		0, // hInstApp
		0,
		0,
		0,
		0, // dwHotKey
		0,
		0
		};

	return ShellExecuteEx(&sei);
}

BOOL ShellExecuteThrow(HWND hwnd, LPCTSTR lpVerb, LPCTSTR lpFile, LPCTSTR lpParameters, LPCTSTR lpDirectory, INT nShowCmd)
{
	CWaitCursor wc;
	BOOL bResult = FALSE;

	bResult = ShellExecuteNoThrow(hwnd, lpVerb, lpFile, lpParameters, lpDirectory, nShowCmd);
	if(!bResult)
	{
		MdThrowStringExceptionF(TEXT("ShellExecute failed: %1!s!"), MdGetWinErrorText(GetLastError()));
	}
	return bResult;
}

CString GetBaseNameFromPath(LPCTSTR path)
{
	CString s = path;
	int i = s.ReverseFind(chrBackslash);
	if(i < 0)
	{
		return s;
	}
	return s.Mid(i + 1);
}

bool FileExists(LPCTSTR path)
{
	CFileFind finder;
	BOOL b = finder.FindFile(path);
	if(b)
	{
		finder.FindNextFile();
		return !finder.IsDirectory();
	}
	else
	{
		return false;
	}
}

CString LoadString(UINT resId)
{
	return MAKEINTRESOURCE(resId);
}

CString GetAppFileName()
{
	CString s;
	VERIFY(GetModuleFileName(NULL, s.GetBuffer(_MAX_PATH), _MAX_PATH));
	s.ReleaseBuffer();
	return s;
}

CString GetAppFolder()
{
	CString s = GetAppFileName();
	int i = s.ReverseFind(chrBackslash);
	ASSERT(i >= 0);
	s = s.Left(i);
	return s;
}

CString MyGetFullPathName(LPCTSTR relativePath)
{
	LPTSTR dummy;
	CString buffer;

	DWORD len = _MAX_PATH;

    DWORD dw = GetFullPathName(relativePath, len, buffer.GetBuffer(len), &dummy);
	buffer.ReleaseBuffer();

	while(dw >= len)
	{
		len += _MAX_PATH;
		dw = GetFullPathName(relativePath, len, buffer.GetBuffer(len), &dummy);
		buffer.ReleaseBuffer();
	}

	if(0 == dw)
	{
		TRACE("GetFullPathName(%s) failed: GetLastError returns %u\r\n", relativePath, GetLastError());
		return relativePath;
	}

	return buffer;
}


