// globalhelpers.cpp - Implementation of global helper functions
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
//
// Last modified: $Date$

#include "stdafx.h"
#include "windirstat.h"
#include "globalhelpers.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	CString FormatLongLongNormal(LONGLONG n)
	{
		// Returns formatted number like "123.456.789".

		ASSERT(n >= 0);
		
		CString all;
		
		do
		{
			int rest= (int)(n % 1000);
			n/= 1000;

			CString s;
			if (n > 0)
				s.Format(_T("%s%03d"), GetLocaleThousandSeparator(), rest);
			else
				s.Format(_T("%d"), rest);

			all= s + all;
		} while (n > 0);

		return all;
	}

	void CacheString(CString& s, UINT resId, LPCTSTR defaultVal)
	{
		ASSERT(lstrlen(defaultVal) > 0);

		if (s.IsEmpty())
		{
			s= LoadString(resId);
		
			if (s.IsEmpty())
				s= defaultVal;
		}
	}

}

CString GetLocaleString(LCTYPE lctype, LANGID langid)
{
	LCID lcid= MAKELCID(langid, SORT_DEFAULT);

	int len= GetLocaleInfo(lcid, lctype, NULL, 0);
	CString s;

	GetLocaleInfo(lcid, lctype, s.GetBuffer(len), len);
	s.ReleaseBuffer();

	return s;
}

CString GetLocaleLanguage(LANGID langid)
{
	CString s= GetLocaleString(LOCALE_SNATIVELANGNAME, langid);

	// In the French case, the system returns "francais",
	// but we want "Francais".

	if (s.GetLength() > 0)
		s.SetAt(0, (TCHAR)_totupper(s[0]));

	return s + _T(" - ") + GetLocaleString(LOCALE_SNATIVECTRYNAME, langid);
}

CString GetLocaleThousandSeparator()
{
	return GetLocaleString(LOCALE_STHOUSAND, GetApp()->GetEffectiveLangid());
}

CString GetLocaleDecimalSeparator()
{
	return GetLocaleString(LOCALE_SDECIMAL, GetApp()->GetEffectiveLangid());
}

CString FormatBytes(LONGLONG n)
{
	if (GetOptions()->IsHumanFormat())
		return FormatLongLongHuman(n);
	else
		return FormatLongLongNormal(n);
}

CString FormatLongLongHuman(LONGLONG n)
{
	// Returns formatted number like "12,4 GB".
	ASSERT(n >= 0);
	const int base = 1024;
	const int half = base / 2;

	CString s;

	double B = (int)(n % base);
	n/= base;

	double KB = (int)(n % base);
	n/= base;

	double MB = (int)(n % base);
	n/= base;

	double GB = (int)(n % base);
	n/= base;

	double TB = (int)(n);

	if (TB != 0 || GB == base - 1 && MB >= half)
		s.Format(_T("%s %s"), FormatDouble(TB + GB/base), GetSpec_TB());
	else if (GB != 0 || MB == base - 1 && KB >= half)
		s.Format(_T("%s %s"), FormatDouble(GB + MB/base), GetSpec_GB());
	else if (MB != 0 || KB == base - 1 && B >= half)
		s.Format(_T("%s %s"), FormatDouble(MB + KB/base), GetSpec_MB());
	else if (KB != 0)
		s.Format(_T("%s %s"), FormatDouble(KB + B/base), GetSpec_KB());
	else if (B != 0)
		s.Format(_T("%d %s"), (int)B, GetSpec_Bytes());
	else
		s= _T("0");

	return s;
}


CString FormatCount(LONGLONG n)
{
	return FormatLongLongNormal(n);
}

CString FormatDouble(double d) // "98,4" or "98.4"
{
	ASSERT(d >= 0);

	d+= 0.05;

	int i= (int)floor(d);
	int r= (int)(10 * fmod(d, 1));

	CString s;
    s.Format(_T("%d%s%d"), i, GetLocaleDecimalSeparator(), r);

	return s;
}

CString PadWidthBlanks(CString n, int width)
{
	int blankCount= width - n.GetLength();
	if (blankCount > 0)
	{
		CString b;
		LPTSTR psz= b.GetBuffer(blankCount + 1);
		for (int i=0; i < blankCount; i++)
			psz[i]= _T(' ');
		psz[i]= 0;
		b.ReleaseBuffer();

		n= b + n;
	}
	return n;
}

CString FormatFileTime(const FILETIME& t)
{
	SYSTEMTIME st;
	if (!FileTimeToSystemTime(&t, &st))
		return MdGetWinerrorText(GetLastError());

	LCID lcid = MAKELCID(GetApp()->GetEffectiveLangid(), SORT_DEFAULT);

	CString date;
	VERIFY(0 < GetDateFormat(lcid, DATE_SHORTDATE, &st, NULL, date.GetBuffer(256), 256));
	date.ReleaseBuffer();

	CString time;
	VERIFY(0 < GetTimeFormat(lcid, 0, &st, NULL, time.GetBuffer(256), 256));
	time.ReleaseBuffer();

	return date + _T("  ") + time;
}

CString FormatMilliseconds(DWORD ms)
{
	CString ret;
	DWORD sec= (ms + 500) / 1000;

	DWORD s= sec % 60;
	DWORD min= sec / 60;

	DWORD m= min % 60;

	DWORD h= min / 60;

	if (h > 0)
		ret.Format(_T("%u:%02u:%02u"), h, m, s);
	else
		ret.Format(_T("%u:%02u"), m, s);
	return ret;
}

bool GetVolumeName(LPCTSTR rootPath, CString& volumeName)
{
	CString ret;
	DWORD dummy;

	UINT old= SetErrorMode(SEM_FAILCRITICALERRORS);
	
	bool b= GetVolumeInformation(rootPath, volumeName.GetBuffer(256), 256, &dummy, &dummy, &dummy, NULL, 0);
	volumeName.ReleaseBuffer();

	if (!b)
		TRACE(_T("GetVolumeInformation(%s) failed: %u\n"), rootPath, GetLastError());

	SetErrorMode(old);
	
	return b;
}

// Given a root path like "C:\", this function
// obtains the volume name and returns a complete display string
// like "BOOT (C:)".
CString FormatVolumeNameOfRootPath(CString rootPath)
{
	CString ret;
	CString volumeName;
	bool b= GetVolumeName(rootPath, volumeName);
	if (b)
	{
		ret= FormatVolumeName(rootPath, volumeName);
	}
	else
	{
		ret= rootPath;
	}
	return ret;
}

CString FormatVolumeName(CString rootPath, CString volumeName)
{
	CString ret;
	ret.Format(_T("%s (%s)"), volumeName, rootPath.Left(2));
	return ret;
}

// The inverse of FormatVolumeNameOfRootPath().
// Given a name like "BOOT (C:)", it returns "C:" (without trailing backslash).
// Or, if name like "C:\", it returns "C:".
CString PathFromVolumeName(CString name)
{
	int i= name.ReverseFind(_T(')'));
	if (i == -1)
	{
		ASSERT(name.GetLength() == 3);
		return name.Left(2);
	}

	ASSERT(i != -1);
	int k= name.ReverseFind(_T('('));
	ASSERT(k != -1);
	ASSERT(k < i);
	CString path= name.Mid(k + 1, i - k - 1);
	ASSERT(path.GetLength() == 2);
	ASSERT(path[1] == _T(':'));

	return path;
}

// Retrieve the "fully qualified parse name" of "My Computer"
CString GetParseNameOfMyComputer() throw (CException *)
{
	CComPtr<IShellFolder> sf;
	HRESULT hr= SHGetDesktopFolder(&sf);
	MdThrowFailed(hr, _T("SHGetDesktopFolder"));

	CCoTaskMem<LPITEMIDLIST> pidl;
	hr= SHGetSpecialFolderLocation(NULL, CSIDL_DRIVES, &pidl);
	MdThrowFailed(hr, _T("SHGetSpecialFolderLocation(CSIDL_DRIVES)"));

	STRRET name;
	ZeroMemory(&name, sizeof(name));
	name.uType= STRRET_CSTR;
	hr= sf->GetDisplayNameOf(pidl, SHGDN_FORPARSING, &name);
	MdThrowFailed(hr, _T("GetDisplayNameOf(My Computer)"));

	return MyStrRetToString(pidl, &name);
}

void GetPidlOfMyComputer(LPITEMIDLIST *ppidl) throw (CException *)
{
	CComPtr<IShellFolder> sf;
	HRESULT hr= SHGetDesktopFolder(&sf);
	MdThrowFailed(hr, _T("SHGetDesktopFolder"));

	hr= SHGetSpecialFolderLocation(NULL, CSIDL_DRIVES, ppidl);
	MdThrowFailed(hr, _T("SHGetSpecialFolderLocation(CSIDL_DRIVES)"));
}

void ShellExecuteWithAssocDialog(HWND hwnd, LPCTSTR filename) throw (CException *)
{
	CWaitCursor wc;

	UINT u= (UINT)ShellExecute(hwnd, NULL, filename, NULL, NULL, SW_SHOWNORMAL);
	if (u == SE_ERR_NOASSOC)
	{
		// Q192352
		CString sysDir;
		//-- Get the system directory so that we know where Rundll32.exe resides.
		GetSystemDirectory(sysDir.GetBuffer(_MAX_PATH), _MAX_PATH);
		sysDir.ReleaseBuffer();
		
		CString parameters = _T("shell32.dll,OpenAs_RunDLL ");
		u= (UINT)ShellExecute(hwnd, _T("open"), _T("RUNDLL32.EXE"), parameters + filename, sysDir, SW_SHOWNORMAL);
	}
		
	if (u <= 32)
	{
		MdThrowStringExceptionF(_T("ShellExecute failed: %1!s!"), GetShellExecuteError(u));
	}
}

void MyGetDiskFreeSpace(LPCTSTR pszRootPath, LONGLONG& total, LONGLONG& unused)
{
	ULARGE_INTEGER uavailable;
	ULARGE_INTEGER utotal;
	ULARGE_INTEGER ufree;
	uavailable.QuadPart= 0;
	utotal.QuadPart= 0;
	ufree.QuadPart= 0;

	// On NT 4.0, the 2nd Parameter to this function must NOT be NULL.
	BOOL b= GetDiskFreeSpaceEx(pszRootPath, &uavailable, &utotal, &ufree);
	if (!b)
		TRACE(_T("GetDiskFreeSpaceEx(%s) failed.\n"), pszRootPath);
	
	total= (LONGLONG)utotal.QuadPart; // will fail, when more than 2^63 Bytes free ....
	unused= (LONGLONG)ufree.QuadPart;

	ASSERT(unused <= total);
}

CString GetFolderNameFromPath(LPCTSTR path)
{
	CString s= path;
	int i= s.ReverseFind(_T('\\'));
	if (i < 0)
		return s;
	return s.Left(i);
}

CString GetCOMSPEC()
{
	CString cmd;

	DWORD dw= GetEnvironmentVariable(_T("COMSPEC"), cmd.GetBuffer(_MAX_PATH), _MAX_PATH);
	cmd.ReleaseBuffer();

	if (dw == 0)
	{
		TRACE(_T("COMSPEC not set.\n"));
		cmd= _T("cmd.exe");
	}
	return cmd;
}

void WaitForHandleWithRepainting(HANDLE h)
{ 
	// Code derived from MSDN sample "Waiting in a Message Loop".

	while (true)
	{
		// Read all of the messages in this next loop, removing each message as we read it.
		MSG msg; 
		while (PeekMessage(&msg, NULL, WM_PAINT, WM_PAINT, PM_REMOVE)) 
		{ 
			DispatchMessage(&msg);
		}

		// Wait for WM_PAINT message sent or posted to this queue 
		// or for one of the passed handles be set to signaled.
		DWORD r= MsgWaitForMultipleObjects(1, &h, FALSE, INFINITE, QS_PAINT);

		// The result tells us the type of event we have.
		if (r == WAIT_OBJECT_0 + 1)
		{
			// New messages have arrived. 
			// Continue to the top of the always while loop to dispatch them and resume waiting.
			continue;
		} 
		else 
		{ 
			// The handle became signaled. 
			break;
		}
	}
}

bool FolderExists(LPCTSTR path)
{
	CFileFind finder;
	BOOL b= finder.FindFile(path);
	if (b)
	{
		finder.FindNextFile();
		return finder.IsDirectory();
	}
	else
	{
		// Here we land, if path is an UNC drive. In this case we
		// try another FindFile:
		b= finder.FindFile(CString(path) + _T("\\*.*"));
		if (b)
			return true;

		return false;
	}
}

bool DriveExists(const CString& path)
{
	if (path.GetLength() != 3 || path[1] != _T(':') || path[2] != _T('\\'))
		return false;

	CString letter= path.Left(1);
	letter.MakeLower();
	int d= letter[0] - _T('a');
	
	DWORD mask= 0x1 << d;

	if ((mask & GetLogicalDrives()) == 0)
		return false;

	CString dummy;
	if (!GetVolumeName(path, dummy))
		return false;

	return true;
}

CString GetUserName()
{
	CString s;
	DWORD size= UNLEN + 1;
	(void)GetUserName(s.GetBuffer(size), &size);
	s.ReleaseBuffer();
	return s;
}

bool IsHexDigit(int c)
{
	if (_istdigit((short)c))
		return true;

	if (_T('a') <= c && c <= _T('f'))
		return true;

	if (_T('A') <= c && c <= _T('F'))
		return true;

	return false;
}

// drive is a drive spec like C: or C:\ or C:\path (path is ignored).
//
// This function returns
// "", if QueryDosDevice is unsupported or drive doesn't begin with a drive letter,
// 'Information about MS-DOS device names' otherwise:
// Something like
//
// \Device\Harddisk\Volume1                               for a local drive
// \Device\LanmanRedirector\;T:0000000011e98\spock\temp   for a network drive 
// \??\C:\programme                                       for a SUBSTed local path
// \??\T:\Neuer Ordner                                    for a SUBSTed SUBSTed path
// \??\UNC\spock\temp                                     for a SUBSTed UNC path
//
// As always, I had to experimentally determine these strings, Microsoft
// didn't think it necessary to document them. (Sometimes I think, they
// even don't document such things internally...)
//
// I hope that a drive is SUBSTed iff this string starts with \??\.
//
// assarbad:
//   It cannot be safely determined wether a path is or is not SUBSTed on NT
//	 via this API. You would have to lookup the volume mount points because
//	 SUBST only works per session by definition whereas volume mount points
//	 work accross sessions (after restarts).
//
CString MyQueryDosDevice(LPCTSTR drive)
{
	CString d = drive;

	if (d.GetLength() < 2 || d[1] != _T(':'))
		return _T("");

	d = d.Left(2);

	CQueryDosDeviceApi api;
	
	if (!api.IsSupported())
		return _T("");

	CString info;
	DWORD dw = api.QueryDosDevice(d, info.GetBuffer(512), 512);
	info.ReleaseBuffer();

	if (dw == 0)
	{
		TRACE(_T("QueryDosDevice(%s) failed: %s\r\n"), d, MdGetWinerrorText(GetLastError()));
		return _T("");
	}

	return info;
}

// drive is a drive spec like C: or C:\ or C:\path (path is ignored).
// 
// This function returnes true, if QueryDosDevice() is supported
// and drive is a SUBSTed drive.
//
bool IsSUBSTedDrive(LPCTSTR drive)
{
	CString info = MyQueryDosDevice(drive);
	return (info.GetLength() >= 4 && info.Left(4) == "\\??\\");
}

CString GetSpec_Bytes()
{
	static CString s;
	CacheString(s, IDS_SPEC_BYTES, _T("Bytes"));
	return s;
}

CString GetSpec_KB()
{
	static CString s;
	CacheString(s, IDS_SPEC_KB, _T("KB"));
	return s;
}

CString GetSpec_MB()
{
	static CString s;
	CacheString(s, IDS_SPEC_MB, _T("MB"));
	return s;
}

CString GetSpec_GB()
{
	static CString s;
	CacheString(s, IDS_SPEC_GB, _T("GB"));
	return s;
}

CString GetSpec_TB()
{
	static CString s;
	CacheString(s, IDS_SPEC_TB, _T("TB"));
	return s;
}

/*
// Retrieve an Item ID list from a given path.
// Returns a valid pidl, or throws an exception.
LPCITEMIDLIST SHGetPIDLFromPath(CString path)
{
	USES_CONVERSION;

	CComPtr<IShellFolder> pshf;
	HRESULT hr= SHGetDesktopFolder(&pshf); 
	MdThrowFailed(hr, _T("SHGetDesktopFolder"));

	LPITEMIDLIST pidl;
	hr= pshf->ParseDisplayName(NULL, NULL, const_cast<LPOLESTR>(T2CW(path)), NULL, &pidl, NULL);
	MdThrowFailed(hr, _T("ParseDisplayName"));

	return pidl;
}
*/

// $Log$
// Revision 1.18  2004/11/25 11:58:52  assarbad
// - Minor fixes (odd behavior of coloring in ANSI version, caching of the GetCompressedFileSize API)
//   for details see the changelog.txt
//
// Revision 1.17  2004/11/14 21:50:44  assarbad
// - Pre-select the last used folder
//
// Revision 1.16  2004/11/14 08:49:06  bseifert
// Date/Time/Number formatting now uses User-Locale. New option to force old behavior.
//
// Revision 1.15  2004/11/12 22:14:16  bseifert
// Eliminated CLR_NONE. Minor corrections.
//
// Revision 1.14  2004/11/12 13:19:44  assarbad
// - Minor changes and additions (in preparation for the solution of the "Browse for Folder" problem)
//
// Revision 1.13  2004/11/12 00:47:42  assarbad
// - Fixed the code for coloring of compressed/encrypted items. Now the coloring spans the full row!
//
// Revision 1.12  2004/11/07 21:10:25  assarbad
// - Corrected IF statement
// - Changed solution file to build consistent "Unicode Debug" configuration for "Unicode Debug" of main project
//
// Revision 1.11  2004/11/07 20:14:30  assarbad
// - Added wrapper for GetCompressedFileSize() so that by default the compressed file size will be shown.
//
// Revision 1.10  2004/11/05 16:53:07  assarbad
// Added Date and History tag where appropriate.
//
