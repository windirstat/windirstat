// osspecific.cpp	- Implementation of the platform-specific classes
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
#include "osspecific.h"

#ifdef UNICODE
#define TSPEC "W"
#else
#define TSPEC "A"
#endif

#define GETPROC(name) m_##name = ( m_dll != 0 ? (Type##name)GetProcAddress(m_dll, #name) : NULL )
#define TGETPROC(name) m_##name = ( m_dll != 0 ? (Type##name)GetProcAddress(m_dll, #name TSPEC) : NULL )

#define CHECK(name) if (m_##name == 0) return false

/////////////////////////////////////////////////////////////////////////////

CVolumeApi::CVolumeApi()
{
	m_dll= LoadLibrary(_T("kernel32.dll"));

	TGETPROC(GetVolumeNameForVolumeMountPoint);
	TGETPROC(FindFirstVolume);
	TGETPROC(FindNextVolume);
	GETPROC(FindVolumeClose);
	TGETPROC(FindFirstVolumeMountPoint);
	TGETPROC(FindNextVolumeMountPoint);
	GETPROC(FindVolumeMountPointClose);
}

CVolumeApi::~CVolumeApi()
{
	if (m_dll != NULL)
		FreeLibrary(m_dll);
	// "It is not safe to call FreeLibrary from DllMain."
	// Therefore, don't use global variables of type CVolumeApi in a DLL.
}

bool CVolumeApi::IsSupported()
{
	CHECK(GetVolumeNameForVolumeMountPoint);
	CHECK(FindFirstVolume);
	CHECK(FindNextVolume);
	CHECK(FindVolumeClose);
	CHECK(FindFirstVolumeMountPoint);
	CHECK(FindNextVolumeMountPoint);
	CHECK(FindVolumeMountPointClose);

	return true;
}


BOOL CVolumeApi::GetVolumeNameForVolumeMountPoint(LPCTSTR lpszVolumeMountPoint, LPTSTR lpszVolumeName, DWORD cchBufferLength)
{
	ASSERT(IsSupported());
	return (*m_GetVolumeNameForVolumeMountPoint)(lpszVolumeMountPoint, lpszVolumeName, cchBufferLength);
}

HANDLE CVolumeApi::FindFirstVolume(LPTSTR lpszVolumeName, DWORD cchBufferLength)
{
	ASSERT(IsSupported());
	return (*m_FindFirstVolume)(lpszVolumeName, cchBufferLength);
}

BOOL CVolumeApi::FindNextVolume(HANDLE hFindVolume, LPTSTR lpszVolumeName, DWORD cchBufferLength)
{
	ASSERT(IsSupported());
	return (*m_FindNextVolume)(hFindVolume, lpszVolumeName, cchBufferLength);
}

BOOL CVolumeApi::FindVolumeClose(HANDLE hFindVolume)
{
	ASSERT(IsSupported());
	return (*m_FindVolumeClose)(hFindVolume);
}


HANDLE CVolumeApi::FindFirstVolumeMountPoint(LPCTSTR lpszRootPathName, LPTSTR lpszVolumeMountPoint, DWORD cchBufferLength)
{
	ASSERT(IsSupported());
	return (*m_FindFirstVolumeMountPoint)(lpszRootPathName, lpszVolumeMountPoint, cchBufferLength);
}

BOOL CVolumeApi::FindNextVolumeMountPoint(HANDLE hFindVolumeMountPoint, LPTSTR lpszVolumeMountPoint, DWORD cchBufferLength)
{
	ASSERT(IsSupported());
	return (*m_FindNextVolumeMountPoint)(hFindVolumeMountPoint, lpszVolumeMountPoint, cchBufferLength);
}

BOOL CVolumeApi::FindVolumeMountPointClose(HANDLE hFindVolumeMountPoint)
{
	ASSERT(IsSupported());
	return (*m_FindVolumeMountPointClose)(hFindVolumeMountPoint);
}



/////////////////////////////////////////////////////////////////////////////

CRecycleBinApi::CRecycleBinApi()
{
	m_dll= LoadLibrary(_T("shell32.dll"));

	TGETPROC(SHEmptyRecycleBin);
	TGETPROC(SHQueryRecycleBin);
}

CRecycleBinApi::~CRecycleBinApi()
{
	if (m_dll != NULL)
		FreeLibrary(m_dll);
}

bool CRecycleBinApi::IsSupported()
{
	CHECK(SHEmptyRecycleBin);
	CHECK(SHQueryRecycleBin);

	return true;
}

HRESULT CRecycleBinApi::SHEmptyRecycleBin(HWND hwnd, LPCTSTR pszRootPath, DWORD dwFlags)
{
	ASSERT(IsSupported());
	return (*m_SHEmptyRecycleBin)(hwnd, pszRootPath, dwFlags);
}

HRESULT CRecycleBinApi::SHQueryRecycleBin(LPCTSTR pszRootPath, LPSHQUERYRBINFO pSHQueryRBInfo)
{
	ASSERT(IsSupported());
	return (*m_SHQueryRecycleBin)(pszRootPath, pSHQueryRBInfo);
}

/////////////////////////////////////////////////////////////////////////////

CPsapi::CPsapi()
{
	m_dll= LoadLibrary(_T("psapi.dll"));

	GETPROC(GetProcessMemoryInfo);
}

CPsapi::~CPsapi()
{
	if (m_dll != NULL)
		FreeLibrary(m_dll);
}

bool CPsapi::IsSupported()
{
	CHECK(GetProcessMemoryInfo);
	return true;
}

BOOL CPsapi::GetProcessMemoryInfo(HANDLE Process, PPROCESS_MEMORY_COUNTERS ppsmemCounters, DWORD cb)
{
	ASSERT(IsSupported());
	return (*m_GetProcessMemoryInfo)(Process, ppsmemCounters, cb);
}


/////////////////////////////////////////////////////////////////////////////

CMapi32Api::CMapi32Api()
{
	m_dll= LoadLibrary(_T("mapi32.dll"));
	GETPROC(MAPISendMail);
}

CMapi32Api::~CMapi32Api()
{
	if (m_dll != NULL)
		FreeLibrary(m_dll);
}

bool CMapi32Api::IsDllPresent()
{
	return SearchPath(NULL, _T("mapi32.dll"), NULL, 0, NULL, NULL) != 0;
}

bool CMapi32Api::IsSupported()
{
	CHECK(MAPISendMail);
	return true;
}

ULONG CMapi32Api::MAPISendMail(LHANDLE lhSession, ULONG ulUIParam, lpMapiMessage lpMessage, FLAGS flFlags, ULONG ulReserved)
{
	ASSERT(IsSupported());
	return (*m_MAPISendMail)(lhSession, ulUIParam, lpMessage, flFlags, ulReserved);
}

/////////////////////////////////////////////////////////////////////////////

CQueryDosDeviceApi::CQueryDosDeviceApi()
{
	m_dll= LoadLibrary(_T("kernel32.dll"));
	TGETPROC(QueryDosDevice);
}

CQueryDosDeviceApi::~CQueryDosDeviceApi()
{
	if (m_dll != NULL)
		FreeLibrary(m_dll);
}

bool CQueryDosDeviceApi::IsSupported()
{
	CHECK(QueryDosDevice);
	return true;
}

DWORD CQueryDosDeviceApi::QueryDosDevice(LPCTSTR lpDeviceName, LPTSTR lpTargetPath, DWORD ucchMax)
{
	ASSERT(IsSupported());
	return (*m_QueryDosDevice)(lpDeviceName, lpTargetPath, ucchMax);
}

