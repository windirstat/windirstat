// globalhelpers.cpp - Implementation of global helper functions
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003 Bernhard Seifert
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

#include "stdafx.h"
#include "windirstat.h"
#include "globalhelpers.h"

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

	void DistributeFirst(int& first, int& second, int& third)
	{
		int h= (first - 255) / 2;
		first= 255;
		second+= h;
		third+= h;

		if (second > 255)
		{
			int h= second - 255;
			second= 255;
			third+= h;
			ASSERT(third <= 255);
		}
		else if (third > 255)
		{
			int h= third - 255;
			third= 255;
			second+= h;
			ASSERT(second <= 255);
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
	return GetLocaleString(LOCALE_SNATIVELANGNAME, langid) + _T(" - ") + GetLocaleString(LOCALE_SNATIVECTRYNAME, langid);
}

CString GetLocaleThousandSeparator()
{
	return GetLocaleString(LOCALE_STHOUSAND, GetApp()->GetLangid());
}

CString GetLocaleDecimalSeparator()
{
	return GetLocaleString(LOCALE_SDECIMAL, GetApp()->GetLangid());
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
		s= FormatDouble(TB + GB/base) + _T(" TB");
	else if (GB != 0 || MB == base - 1 && KB >= half)
		s= FormatDouble(GB + MB/base) + _T(" GB");
	else if (MB != 0 || KB == base - 1 && B >= half)
		s= FormatDouble(MB + KB/base) + _T(" MB");
	else if (KB != 0)
		s= FormatDouble(KB + B/base) + _T(" KB");
	else if (B != 0)
		s.Format(_T("%d Bytes"), (int)B);
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

	LCID lcid = MAKELCID(GetApp()->GetLangid(), SORT_DEFAULT);

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
CString FormatVolumeName(CString rootPath)
{
	CString ret;
	CString volumeName;
	bool b= GetVolumeName(rootPath, volumeName);
	if (b)
	{
		ret.Format(_T("%s (%s)"), volumeName, rootPath.Left(2));
	}
	else
	{
		ret= rootPath;
	}
	return ret;
}

// The inverse of FormatVolumeName().
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

// Returns true, if the System has 256 Colors or less
bool Is256Colors()
{
	CClientDC dc(CWnd::GetDesktopWindow());
	return (dc.GetDeviceCaps(NUMCOLORS) != -1);
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

void NormalizeColor(int& red, int& green, int& blue)
{
	ASSERT(red + green + blue <= 3 * 255);

	if (red > 255)
	{
		DistributeFirst(red, green, blue);
	}
	else if (green > 255)
	{
		DistributeFirst(green, red, blue);
	}
	else if (blue > 255)
	{
		DistributeFirst(blue, red, green);
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
	if (isdigit(c))
		return true;

	if (_T('a') <= c && c <= _T('f'))
		return true;

	if (_T('A') <= c && c <= _T('F'))
		return true;

	return false;
}

