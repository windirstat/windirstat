// commonhelpers.cpp	- Implementation of common global helper functions
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2004 Bernhard Seifert
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
// Author: bseifert@users.sourceforge.net, bseifert@daccord.net

#ifndef WINVER				// Allow use of features specific to Windows 95 and Windows NT 4 or later.
#define WINVER 0x0400		// Change this to the appropriate value to target Windows 98 and Windows 2000 or later.
#endif

#include <afxwin.h>         // MFC core and standard components
#include <atlbase.h>		// CComPtr, USES_CONVERSION

#include "mdexceptions.h"

#include "commonhelpers.h"


CString GetShellExecuteError(UINT u)
{
	CString s;

	switch (u)
	{
	case 0:						s=_T("The operating system is out of memory or resources."); break;
	case ERROR_FILE_NOT_FOUND:	s=_T("The specified file was not found."); break;
	case ERROR_PATH_NOT_FOUND:	s=_T("The specified path was not found."); break;
	case ERROR_BAD_FORMAT:		s=_T("The .exe file is invalid (non-Microsoft Win32 .exe or error in .exe image)."); break;
	case SE_ERR_ACCESSDENIED:	s=_T("The operating system denied access to the specified file."); break;
	case SE_ERR_ASSOCINCOMPLETE:s=_T("The file name association is incomplete or invalid."); break;
	case SE_ERR_DDEBUSY:		s=_T("The Dynamic Data Exchange (DDE) transaction could not be completed because other DDE transactions were being processed."); break;
	case SE_ERR_DDEFAIL:		s=_T("The DDE transaction failed."); break;
	case SE_ERR_DDETIMEOUT:		s=_T("The DDE transaction could not be completed because the request timed out."); break;
	case SE_ERR_DLLNOTFOUND:	s=_T("The specified dynamic-link library (DLL) was not found."); break;
	case SE_ERR_NOASSOC:		s=_T("There is no application associated with the given file name extension. This error will also be returned if you attempt to print a file that is not printable."); break;
	case SE_ERR_OOM:			s=_T("There was not enough memory to complete the operation."); break;
	case SE_ERR_SHARE:			s=_T("A sharing violation occurred"); break;
	default:					s.Format(_T("Error Number %d"), u); break;
	}

	return s;
}


CString MyStrRetToString(const LPITEMIDLIST pidl, const STRRET *strret)
{
	// StrRetToStr() is not always available (e.g. on Windows 98).
	// So we use an own function instead.
	USES_CONVERSION;

	CString s;

	switch (strret->uType)
	{
	case STRRET_CSTR:
		s= strret->cStr;
		break;
	case STRRET_OFFSET:
		s= A2T((char *)pidl + strret->uOffset);
		break;
	case STRRET_WSTR:
		s= W2T(strret->pOleStr);
		break;
	}

	return s;
}

void MyShellExecute(HWND hwnd, LPCTSTR lpOperation, LPCTSTR lpFile, LPCTSTR lpParameters, LPCTSTR lpDirectory, INT nShowCmd) throw (CException *)
{
	CWaitCursor wc;

	UINT h= (UINT)ShellExecute(hwnd, lpOperation, lpFile, lpParameters, lpDirectory, nShowCmd);
	if (h <= 32)
		MdThrowStringExceptionF(_T("ShellExecute failed: %1!s!"), GetShellExecuteError(h));
}


CString GetBaseNameFromPath(LPCTSTR path)
{
	CString s= path;
	int i= s.ReverseFind(_T('\\'));
	if (i < 0)
		return s;
	return s.Mid(i + 1);
}

bool FileExists(LPCTSTR path)
{
	CFileFind finder;
	BOOL b= finder.FindFile(path);
	if (b)
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
	CString s= GetAppFileName();
	int i= s.ReverseFind(_T('\\'));
	ASSERT(i >= 0);
	s= s.Left(i);
	return s;
}

