// osspecific.cpp - Implementation of the platform-specific classes
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

#include "stdafx.h"
#include "osspecific.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#ifdef UNICODE
#define TSPEC "W"
#else
#define TSPEC "A"
#endif

#define PROCNAME(name) #name
#define TPROCNAME(name) #name TSPEC

#define CHECK(name) if(!m_##name.IsSupported()) return false

static CDllModule dllKernel32(nameKernel32);
static CDllModule dllShell32(nameShell32);
static CDllModule dllPsApi(namePsApi);

/////////////////////////////////////////////////////////////////////////////

CVolumeApi::CVolumeApi()
    : m_hDll(dllKernel32.Handle())
    , m_GetVolumeNameForVolumeMountPoint(m_hDll, TPROCNAME(GetVolumeNameForVolumeMountPoint))
    , m_FindFirstVolume(m_hDll, TPROCNAME(FindFirstVolume))
    , m_FindNextVolume(m_hDll, TPROCNAME(FindNextVolume))
    , m_FindVolumeClose(m_hDll, PROCNAME(FindVolumeClose))
    , m_FindFirstVolumeMountPoint(m_hDll, TPROCNAME(FindFirstVolumeMountPoint))
    , m_FindNextVolumeMountPoint(m_hDll, TPROCNAME(FindNextVolumeMountPoint))
    , m_FindVolumeMountPointClose(m_hDll, PROCNAME(FindVolumeMountPointClose))
{
}

CVolumeApi::~CVolumeApi()
{
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
    ASSERT(m_GetVolumeNameForVolumeMountPoint.IsSupported());
    return m_GetVolumeNameForVolumeMountPoint.pfnFct(lpszVolumeMountPoint, lpszVolumeName, cchBufferLength);
}

HANDLE CVolumeApi::FindFirstVolume(LPTSTR lpszVolumeName, DWORD cchBufferLength)
{
    ASSERT(m_FindFirstVolume.IsSupported());
    return m_FindFirstVolume.pfnFct(lpszVolumeName, cchBufferLength);
}

BOOL CVolumeApi::FindNextVolume(HANDLE hFindVolume, LPTSTR lpszVolumeName, DWORD cchBufferLength)
{
    ASSERT(m_FindNextVolume.IsSupported());
    return m_FindNextVolume.pfnFct(hFindVolume, lpszVolumeName, cchBufferLength);
}

BOOL CVolumeApi::FindVolumeClose(HANDLE hFindVolume)
{
    ASSERT(m_FindVolumeClose.IsSupported());
    return m_FindVolumeClose.pfnFct(hFindVolume);
}


HANDLE CVolumeApi::FindFirstVolumeMountPoint(LPCTSTR lpszRootPathName, LPTSTR lpszVolumeMountPoint, DWORD cchBufferLength)
{
    ASSERT(m_FindFirstVolumeMountPoint.IsSupported());
    return m_FindFirstVolumeMountPoint.pfnFct(lpszRootPathName, lpszVolumeMountPoint, cchBufferLength);
}

BOOL CVolumeApi::FindNextVolumeMountPoint(HANDLE hFindVolumeMountPoint, LPTSTR lpszVolumeMountPoint, DWORD cchBufferLength)
{
    ASSERT(m_FindNextVolumeMountPoint.IsSupported());
    return m_FindNextVolumeMountPoint.pfnFct(hFindVolumeMountPoint, lpszVolumeMountPoint, cchBufferLength);
}

BOOL CVolumeApi::FindVolumeMountPointClose(HANDLE hFindVolumeMountPoint)
{
    ASSERT(m_FindVolumeMountPointClose.IsSupported());
    return m_FindVolumeMountPointClose.pfnFct(hFindVolumeMountPoint);
}

/////////////////////////////////////////////////////////////////////////////

CRecycleBinApi::CRecycleBinApi()
    : m_hDll(dllShell32.Handle())
    , m_SHEmptyRecycleBin(m_hDll, TPROCNAME(SHEmptyRecycleBin))
    , m_SHQueryRecycleBin(m_hDll, TPROCNAME(SHQueryRecycleBin))
{
}

CRecycleBinApi::~CRecycleBinApi()
{
}

bool CRecycleBinApi::IsSupported()
{
    CHECK(SHEmptyRecycleBin);
    CHECK(SHQueryRecycleBin);

    return true;
}

HRESULT CRecycleBinApi::SHEmptyRecycleBin(HWND hwnd, LPCTSTR pszRootPath, DWORD dwFlags)
{
    ASSERT(m_SHEmptyRecycleBin.IsSupported());
    return m_SHEmptyRecycleBin.pfnFct(hwnd, pszRootPath, dwFlags);
}

HRESULT CRecycleBinApi::SHQueryRecycleBin(LPCTSTR pszRootPath, LPSHQUERYRBINFO pSHQueryRBInfo)
{
    ASSERT(m_SHQueryRecycleBin.IsSupported());
    return m_SHQueryRecycleBin.pfnFct(pszRootPath, pSHQueryRBInfo);
}

/////////////////////////////////////////////////////////////////////////////

CPsapi::CPsapi()
    : m_hDll(dllPsApi.Handle())
    , m_GetProcessMemoryInfo(m_hDll, PROCNAME(GetProcessMemoryInfo))
{
}

CPsapi::~CPsapi()
{
}

bool CPsapi::IsSupported()
{
    CHECK(GetProcessMemoryInfo);

    return true;
}

BOOL CPsapi::GetProcessMemoryInfo(HANDLE Process, PPROCESS_MEMORY_COUNTERS ppsmemCounters, DWORD cb)
{
    ASSERT(m_GetProcessMemoryInfo.IsSupported());
    return m_GetProcessMemoryInfo.pfnFct(Process, ppsmemCounters, cb);
}

/////////////////////////////////////////////////////////////////////////////

CQueryDosDeviceApi::CQueryDosDeviceApi()
    : m_hDll(dllKernel32.Handle())
    , m_QueryDosDevice(m_hDll, TPROCNAME(QueryDosDevice))
{
}

CQueryDosDeviceApi::~CQueryDosDeviceApi()
{
}

bool CQueryDosDeviceApi::IsSupported()
{
    CHECK(QueryDosDevice);

    return true;
}

DWORD CQueryDosDeviceApi::QueryDosDevice(LPCTSTR lpDeviceName, LPTSTR lpTargetPath, DWORD ucchMax)
{
    ASSERT(m_QueryDosDevice.IsSupported());
    return m_QueryDosDevice.pfnFct(lpDeviceName, lpTargetPath, ucchMax);
}

/////////////////////////////////////////////////////////////////////////////

CGetCompressedFileSizeApi::CGetCompressedFileSizeApi()
    : m_hDll(dllKernel32.Handle())
    , m_GetCompressedFileSize(m_hDll, TPROCNAME(GetCompressedFileSize))
{
}

CGetCompressedFileSizeApi::~CGetCompressedFileSizeApi()
{
}

bool CGetCompressedFileSizeApi::IsSupported()
{
    CHECK(GetCompressedFileSize);

    return true;
}

DWORD CGetCompressedFileSizeApi::GetCompressedFileSize(LPCTSTR lpFileName, LPDWORD lpFileSizeHigh)
{
    ASSERT(m_GetCompressedFileSize.IsSupported());
    return m_GetCompressedFileSize.pfnFct(lpFileName, lpFileSizeHigh);
}

ULONGLONG CGetCompressedFileSizeApi::GetCompressedFileSize(LPCTSTR lpFileName)
{
    ULARGE_INTEGER u64ret = {0};
    u64ret.LowPart = this->GetCompressedFileSize(lpFileName, &u64ret.HighPart);
    return u64ret.QuadPart;
}


/////////////////////////////////////////////////////////////////////////////

CGetDiskFreeSpaceApi::CGetDiskFreeSpaceApi()
    : m_hDll(dllKernel32.Handle())
    , m_GetDiskFreeSpace(m_hDll, TPROCNAME(GetDiskFreeSpace))
    , m_GetDiskFreeSpaceEx(m_hDll, TPROCNAME(GetDiskFreeSpaceEx))
{
}

CGetDiskFreeSpaceApi::~CGetDiskFreeSpaceApi()
{
}

bool CGetDiskFreeSpaceApi::IsSupported()
{
    // Either of the functions exists definitely, even on Windows 95 w/o OSR2
    return m_GetDiskFreeSpace.IsSupported() | m_GetDiskFreeSpaceEx.IsSupported();
}

void CGetDiskFreeSpaceApi::GetDiskFreeSpace(LPCTSTR pszRootPath, ULONGLONG& total, ULONGLONG& unused)
{
    ULARGE_INTEGER u64available = {0};
    ULARGE_INTEGER u64total = {0};
    ULARGE_INTEGER u64free = {0};

    if(m_GetDiskFreeSpaceEx.IsSupported())
    {
        // On NT 4.0, the 2nd Parameter to this function must NOT be NULL.
        BOOL b = m_GetDiskFreeSpaceEx.pfnFct(pszRootPath, &u64available, &u64total, &u64free);
        if(!b)
        {
            TRACE(_T("GetDiskFreeSpaceEx(%s) failed.\n"), pszRootPath);
        }
    }
    else /*if(m_GetDiskFreeSpace.IsSupported())*/ // compatibility mode ...
    {
        // Actually this code should only be called on versions not supporting
        // GetDiskFreeSpaceEx(). By chance these versions are those which do
        // not support partition sizes larger than 2 GB anyway :)
        DWORD dwSectorsPerCluster = 0, dwBytesPerSector = 0;
        BOOL b = m_GetDiskFreeSpace.pfnFct(pszRootPath, &dwSectorsPerCluster, &dwBytesPerSector, &u64free.LowPart, &u64total.LowPart);
        if(!b)
        {
            TRACE(_T("GetDiskFreeSpace(%s) failed.\n"), pszRootPath);
        }

        // Now we need to do some arithmetics to get the sizes in byte ...
        u64free.QuadPart *= dwSectorsPerCluster * dwBytesPerSector;
        u64total.QuadPart *= dwSectorsPerCluster * dwBytesPerSector;
    }

    // Will fail, when more than 2^63 Bytes free ... (signed vs. unsigned type)
    total = u64total.QuadPart;
    unused = u64free.QuadPart;

    // Race condition ...
    ASSERT(unused <= total);
}
