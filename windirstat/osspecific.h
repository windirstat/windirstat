// osspecific.h		- Declaration of CVolumeApi, CRecycleBinApi, CPsapi, CMapi32Api
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


// I try not to blow up such classes; they shall _only_ hide away the dynamic linking.

#pragma once


//
// CVolumeApi. Supported with Windows 2000 or higher.
//
class CVolumeApi
{
public:
	CVolumeApi();
	~CVolumeApi();

	bool IsSupported();

	BOOL GetVolumeNameForVolumeMountPoint(LPCTSTR lpszVolumeMountPoint, LPTSTR lpszVolumeName, DWORD cchBufferLength);

	HANDLE FindFirstVolume(LPTSTR lpszVolumeName, DWORD cchBufferLength);
	BOOL FindNextVolume(HANDLE hFindVolume, LPTSTR lpszVolumeName, DWORD cchBufferLength);
	BOOL FindVolumeClose(HANDLE hFindVolume);

	HANDLE FindFirstVolumeMountPoint(LPCTSTR lpszRootPathName, LPTSTR lpszVolumeMountPoint, DWORD cchBufferLength);
	BOOL FindNextVolumeMountPoint(HANDLE hFindVolumeMountPoint, LPTSTR lpszVolumeMountPoint, DWORD cchBufferLength);
	BOOL FindVolumeMountPointClose(HANDLE hFindVolumeMountPoint);


private:
	typedef BOOL (WINAPI *TypeGetVolumeNameForVolumeMountPoint)(LPCTSTR lpszVolumeMountPoint, LPTSTR lpszVolumeName, DWORD cchBufferLength);
	typedef HANDLE (WINAPI *TypeFindFirstVolume)(LPTSTR lpszVolumeName, DWORD cchBufferLength);
	typedef BOOL (WINAPI *TypeFindNextVolume)(HANDLE hFindVolume, LPTSTR lpszVolumeName, DWORD cchBufferLength);
	typedef BOOL (WINAPI *TypeFindVolumeClose)(HANDLE hFindVolume);
	typedef HANDLE (WINAPI *TypeFindFirstVolumeMountPoint)(LPCTSTR lpszRootPathName, LPTSTR lpszVolumeMountPoint, DWORD cchBufferLength);
	typedef BOOL (WINAPI *TypeFindNextVolumeMountPoint)(HANDLE hFindVolumeMountPoint, LPTSTR lpszVolumeMountPoint, DWORD cchBufferLength);
	typedef BOOL (WINAPI *TypeFindVolumeMountPointClose)(HANDLE hFindVolumeMountPoint);

	HMODULE m_dll;
	TypeGetVolumeNameForVolumeMountPoint	m_GetVolumeNameForVolumeMountPoint;
	TypeFindFirstVolume						m_FindFirstVolume;
	TypeFindNextVolume						m_FindNextVolume;
	TypeFindVolumeClose						m_FindVolumeClose;
	TypeFindFirstVolumeMountPoint			m_FindFirstVolumeMountPoint;
	TypeFindNextVolumeMountPoint			m_FindNextVolumeMountPoint;
	TypeFindVolumeMountPointClose			m_FindVolumeMountPointClose;
};


//
// CRecycleBinApi. Not always supported on NT and W95/98.
//
class CRecycleBinApi
{
public:
	CRecycleBinApi();
	~CRecycleBinApi();

	bool IsSupported();

	HRESULT SHEmptyRecycleBin(HWND hwnd, LPCTSTR pszRootPath, DWORD dwFlags);
	HRESULT SHQueryRecycleBin(LPCTSTR pszRootPath, LPSHQUERYRBINFO pSHQueryRBInfo);

private:
	typedef HRESULT (STDAPICALLTYPE *TypeSHEmptyRecycleBin)(HWND hwnd, LPCTSTR pszRootPath, DWORD dwFlags);
	typedef HRESULT (STDAPICALLTYPE *TypeSHQueryRecycleBin)(LPCTSTR pszRootPath, LPSHQUERYRBINFO pSHQueryRBInfo);

	HMODULE m_dll;

	TypeSHEmptyRecycleBin m_SHEmptyRecycleBin;
	TypeSHQueryRecycleBin m_SHQueryRecycleBin;
};


//
// CPsapi. Not Supported on W95/W98/me.
//
class CPsapi
{
public:
	CPsapi();
	~CPsapi();

	bool IsSupported();

	BOOL GetProcessMemoryInfo(HANDLE Process, PPROCESS_MEMORY_COUNTERS ppsmemCounters, DWORD cb);

private:
	typedef BOOL (WINAPI *TypeGetProcessMemoryInfo)(HANDLE Process, PPROCESS_MEMORY_COUNTERS ppsmemCounters, DWORD cb);

	HMODULE m_dll;
	TypeGetProcessMemoryInfo m_GetProcessMemoryInfo;
};


//
// CMapi32Api. CDocument::OnFileSendMail() loads mapi32.dll dynamically. 
// So we do, too.
//
class CMapi32Api
{
public:
	CMapi32Api();
	~CMapi32Api();

	static bool IsDllPresent();
	bool IsSupported();

	ULONG MAPISendMail(LHANDLE lhSession, ULONG ulUIParam, lpMapiMessage lpMessage, FLAGS flFlags, ULONG ulReserved);
 
private:
	typedef ULONG (FAR PASCAL *TypeMAPISendMail)(LHANDLE lhSession, ULONG ulUIParam, lpMapiMessage lpMessage, FLAGS flFlags, ULONG ulReserved);
 
	HMODULE m_dll;
	TypeMAPISendMail m_MAPISendMail;
};

