// osspecific.h		- Declaration of CVolumeApi, CRecycleBinApi, CPsapi, CMapi32Api
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
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
	bool m_UnloadDll;
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
	bool m_UnloadDll;

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


//
// QueryDosDevice. Supported with W98 and higher.
//
class CQueryDosDeviceApi
{
public:
	CQueryDosDeviceApi();
	~CQueryDosDeviceApi();

	bool IsSupported();

	DWORD QueryDosDevice(LPCTSTR lpDeviceName, LPTSTR lpTargetPath, DWORD ucchMax);

private:
	typedef DWORD (WINAPI *TypeQueryDosDevice)(LPCTSTR lpDeviceName, LPTSTR lpTargetPath, DWORD ucchMax);

	HMODULE m_dll;
	bool m_UnloadDll;

	TypeQueryDosDevice m_QueryDosDevice;
};

//
// GetCompressedFileSize. Only supported on the NT platform
//
class CGetCompressedFileSizeApi
{
public:
	CGetCompressedFileSizeApi();
	~CGetCompressedFileSizeApi();

	bool IsSupported();

	DWORD GetCompressedFileSize(LPCTSTR lpFileName, LPDWORD lpFileSizeHigh);
	ULONGLONG GetCompressedFileSize(LPCTSTR lpFileName);

private:
	typedef DWORD (WINAPI *TypeGetCompressedFileSize)(LPCTSTR lpFileName, LPDWORD lpFileSizeHigh);

	HMODULE m_dll;
	bool m_UnloadDll;
	
	TypeGetCompressedFileSize m_GetCompressedFileSize;
};

//
// GetDiskFreeSpaceEx() vs. GetDiskFreeSpace(). This class wraps the
// functions and hides the differences since GetDiskFreeSpaceEx() is only
// available starting with Windows 95 OSR2!
//
class CGetDiskFreeSpaceApi
{
public:
	CGetDiskFreeSpaceApi();
	~CGetDiskFreeSpaceApi();

	bool IsSupported();

	void GetDiskFreeSpace(LPCTSTR pszRootPath, LONGLONG& total, LONGLONG& unused);

private:
	typedef BOOL (WINAPI *TypeGetDiskFreeSpace)(LPCTSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters);
	typedef BOOL (WINAPI *TypeGetDiskFreeSpaceEx)(LPCTSTR lpDirectoryName, PULARGE_INTEGER lpFreeBytesAvailable, PULARGE_INTEGER lpTotalNumberOfBytes, PULARGE_INTEGER lpTotalNumberOfFreeBytes);

	HMODULE m_dll;
	bool m_UnloadDll;
	
	TypeGetDiskFreeSpace m_GetDiskFreeSpace;
	TypeGetDiskFreeSpaceEx m_GetDiskFreeSpaceEx;
};

// $Log$
// Revision 1.8  2005/10/01 11:21:08  assarbad
// *** empty log message ***
//
// Revision 1.7  2005/04/17 12:27:21  assarbad
// - For details see changelog of 2005-04-17
//
// Revision 1.6  2004/11/28 14:40:06  assarbad
// - Extended CFileFindWDS to replace a global function
// - Now packing/unpacking the file attributes. This even spares a call to find encrypted/compressed files.
//
// Revision 1.5  2004/11/07 20:14:31  assarbad
// - Added wrapper for GetCompressedFileSize() so that by default the compressed file size will be shown.
//
// Revision 1.4  2004/11/05 16:53:07  assarbad
// Added Date and History tag where appropriate.
//
