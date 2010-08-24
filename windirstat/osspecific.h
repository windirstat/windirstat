// osspecific.h - Declaration of CVolumeApi, CRecycleBinApi, CPsapi,
//                CGetDiskFreeSpaceApi, CGetCompressedFileSizeApi
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

#ifndef __WDS_OSSPECIFIC_H__
#define __WDS_OSSPECIFIC_H__
#pragma once
#include <Windows.h>
#include <ShellAPI.h>

class CAbstractionLayer
{
    CAbstractionLayer()
    {
    }
};

const LPCTSTR nameKernel32 = _T("kernel32.dll");
const LPCTSTR nameShell32 = _T("shell32.dll");
const LPCTSTR namePsApi = _T("psapi.dll");

///////////////////////////////////////////////////////////////////////////////
///  CDllModule
///  Encapsulates the module handle for a DLL given the DLL name in the ctor
///
///
///  @remarks Should be preferably used as a static instance.
///////////////////////////////////////////////////////////////////////////////
class CDllModule
{
public:
    ///////////////////////////////////////////////////////////////////////////////
    ///  inline public constructor  CDllModule
    ///  Ctor for the CDllModule wrapper class
    ///
    ///  @param [in]       DllName LPCTSTR    Name of the DLL of which we want the handle
    ///
    ///  This function doesn't return a value
    ///////////////////////////////////////////////////////////////////////////////
    CDllModule(LPCTSTR DllName)
    {
        m_hDll = LoadLibrary(DllName);
    }

    ///////////////////////////////////////////////////////////////////////////////
    ///  inline public destructor  ~CDllModule
    ///  Dtor for the CDllModule wrapper class
    ///
    ///  This function doesn't return a value
    ///////////////////////////////////////////////////////////////////////////////
    ~CDllModule()
    {
        if(m_hDll)
        {
            FreeLibrary(m_hDll);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////
    ///  inline public  Handle
    ///  Returns the module handle value of this class instance
    ///
    ///  @return HMODULE Returns the module handle or NULL if there is no valid handle
    ///
    ///  @remarks The caller is responsible to verify the returned handle!
    ///////////////////////////////////////////////////////////////////////////////
    HMODULE Handle()
    {
        return m_hDll;
    }

private:
    ///////////////////////////////////////////////////////////////////////////////
    ///  HMODULE m_hDll
    ///  The private member variable which holds the module handle
    ///
    ///  @remarks This is initialized directly inside the ctor!
    ///////////////////////////////////////////////////////////////////////////////
    HMODULE m_hDll;
};

extern CDllModule dllKernel32;
extern CDllModule dllShell32;
extern CDllModule dllPsApi;

///////////////////////////////////////////////////////////////////////////////
///  CDynamicApi
///  Template class to implement dynamic linking to functions
///
///
///  @remarks Preferably used as a class member variable and initialized in the
///           initializer list.
///////////////////////////////////////////////////////////////////////////////
template <class FctType> class CDynamicApi
{
public:
    ///////////////////////////////////////////////////////////////////////////////
    ///  inline public constructor  CDynamicApi
    ///  Ctor of the dynamic linking template class
    ///
    ///  @param [in]       hDll HMODULE    Module handle to the DLL implementing the wrapped function
    ///  @param [in]       pszFctName LPCSTR    Name of the function (ANSI)
    ///
    ///  This function doesn't return a value
    ///////////////////////////////////////////////////////////////////////////////
    CDynamicApi(HMODULE hDll, LPCSTR pszFctName)
        : pfnFct(0)
    {
        if(hDll)
        {
            pfnFct = reinterpret_cast<FctType>(GetProcAddress(hDll, pszFctName));
        }
    }

    ///////////////////////////////////////////////////////////////////////////////
    ///  inline public  IsSupported
    ///  Used to check whether the wrapped function is supported (found)
    ///
    ///  @return bool true if the function can be called, false otherwise
    ///////////////////////////////////////////////////////////////////////////////
    bool IsSupported()
    {
        return (pfnFct != NULL);
    }

public:
    FctType pfnFct;
};

///////////////////////////////////////////////////////////////////////////////
///  CVolumeApi
///  Wraps a bunch of functions relating to volume mount points and more ...
///
///
///  @remarks The methods are documented in the .cpp file
///////////////////////////////////////////////////////////////////////////////
class CVolumeApi
{
public:
    CVolumeApi();
    ~CVolumeApi();

    bool IsSupported();

    BOOL GetVolumeNameForVolumeMountPoint(LPCTSTR lpszVolumeMountPoint, LPTSTR lpszVolumeName, DWORD cchBufferLength);

    // min.: Windows 2000
    HANDLE FindFirstVolume(LPTSTR lpszVolumeName, DWORD cchBufferLength);
    BOOL FindNextVolume(HANDLE hFindVolume, LPTSTR lpszVolumeName, DWORD cchBufferLength);
    BOOL FindVolumeClose(HANDLE hFindVolume);

    // min.: Windows 2000
    HANDLE FindFirstVolumeMountPoint(LPCTSTR lpszRootPathName, LPTSTR lpszVolumeMountPoint, DWORD cchBufferLength);
    BOOL FindNextVolumeMountPoint(HANDLE hFindVolumeMountPoint, LPTSTR lpszVolumeMountPoint, DWORD cchBufferLength);
    BOOL FindVolumeMountPointClose(HANDLE hFindVolumeMountPoint);

private:
    typedef BOOL (WINAPI *TFNGetVolumeNameForVolumeMountPoint)(LPCTSTR lpszVolumeMountPoint, LPTSTR lpszVolumeName, DWORD cchBufferLength);
    typedef HANDLE (WINAPI *TFNFindFirstVolume)(LPTSTR lpszVolumeName, DWORD cchBufferLength);
    typedef BOOL (WINAPI *TFNFindNextVolume)(HANDLE hFindVolume, LPTSTR lpszVolumeName, DWORD cchBufferLength);
    typedef BOOL (WINAPI *TFNFindVolumeClose)(HANDLE hFindVolume);
    typedef HANDLE (WINAPI *TFNFindFirstVolumeMountPoint)(LPCTSTR lpszRootPathName, LPTSTR lpszVolumeMountPoint, DWORD cchBufferLength);
    typedef BOOL (WINAPI *TFNFindNextVolumeMountPoint)(HANDLE hFindVolumeMountPoint, LPTSTR lpszVolumeMountPoint, DWORD cchBufferLength);
    typedef BOOL (WINAPI *TFNFindVolumeMountPointClose)(HANDLE hFindVolumeMountPoint);

    HMODULE m_hDll;

    CDynamicApi<TFNGetVolumeNameForVolumeMountPoint>    m_GetVolumeNameForVolumeMountPoint;
    CDynamicApi<TFNFindFirstVolume>                     m_FindFirstVolume;
    CDynamicApi<TFNFindNextVolume>                      m_FindNextVolume;
    CDynamicApi<TFNFindVolumeClose>                     m_FindVolumeClose;
    CDynamicApi<TFNFindFirstVolumeMountPoint>           m_FindFirstVolumeMountPoint;
    CDynamicApi<TFNFindNextVolumeMountPoint>            m_FindNextVolumeMountPoint;
    CDynamicApi<TFNFindVolumeMountPointClose>           m_FindVolumeMountPointClose;
};

///////////////////////////////////////////////////////////////////////////////
///  CRecycleBinApi
///  <TODO: insert class description here>
///
///
///  @remarks The methods are documented in the .cpp file
///////////////////////////////////////////////////////////////////////////////
class CRecycleBinApi
{
public:
    CRecycleBinApi();
    ~CRecycleBinApi();

    bool IsSupported();

    // min.: Windows 2000, Windows NT 4.0 with Internet Explorer 4.0, Windows 98, Windows 95 with Internet Explorer 4.0
    HRESULT SHEmptyRecycleBin(HWND hwnd, LPCTSTR pszRootPath, DWORD dwFlags);
    HRESULT SHQueryRecycleBin(LPCTSTR pszRootPath, SHQUERYRBINFO* pSHQueryRBInfo);

private:
    typedef HRESULT (STDAPICALLTYPE *TFNSHEmptyRecycleBin)(HWND hwnd, LPCTSTR pszRootPath, DWORD dwFlags);
    typedef HRESULT (STDAPICALLTYPE *TFNSHQueryRecycleBin)(LPCTSTR pszRootPath, SHQUERYRBINFO* pSHQueryRBInfo);

    HMODULE m_hDll;

    CDynamicApi<TFNSHEmptyRecycleBin>                   m_SHEmptyRecycleBin;
    CDynamicApi<TFNSHQueryRecycleBin>                   m_SHQueryRecycleBin;
};

///////////////////////////////////////////////////////////////////////////////
///  CPsapi
///  <TODO: insert class description here>
///
///
///  @remarks The methods are documented in the .cpp file
///////////////////////////////////////////////////////////////////////////////
class CPsapi
{
public:
    CPsapi();
    ~CPsapi();

    bool IsSupported();

    // min.: Windows NT 4.0 (DLL has to exist!)
    BOOL GetProcessMemoryInfo(HANDLE Process, PPROCESS_MEMORY_COUNTERS ppsmemCounters, DWORD cb);

private:
    typedef BOOL (WINAPI *TFNGetProcessMemoryInfo)(HANDLE Process, PPROCESS_MEMORY_COUNTERS ppsmemCounters, DWORD cb);

    HMODULE m_hDll;

    CDynamicApi<TFNGetProcessMemoryInfo>            m_GetProcessMemoryInfo;
};

///////////////////////////////////////////////////////////////////////////////
///  CQueryDosDeviceApi
///  <TODO: insert class description here>
///
///
///  @remarks The methods are documented in the .cpp file
///////////////////////////////////////////////////////////////////////////////
class CQueryDosDeviceApi
{
public:
    CQueryDosDeviceApi();
    ~CQueryDosDeviceApi();

    bool IsSupported();

    // min.: Windows 98/Windows NT 4.0
    DWORD QueryDosDevice(LPCTSTR lpDeviceName, LPTSTR lpTargetPath, DWORD ucchMax);

private:
    typedef DWORD (WINAPI *TFNQueryDosDevice)(LPCTSTR lpDeviceName, LPTSTR lpTargetPath, DWORD ucchMax);

    HMODULE m_hDll;

    CDynamicApi<TFNQueryDosDevice>                      m_QueryDosDevice;
};

///////////////////////////////////////////////////////////////////////////////
///  CGetCompressedFileSizeApi
///  <TODO: insert class description here>
///
///
///  @remarks The methods are documented in the .cpp file
///////////////////////////////////////////////////////////////////////////////
class CGetCompressedFileSizeApi
{
public:
    CGetCompressedFileSizeApi();
    ~CGetCompressedFileSizeApi();

    bool IsSupported();

    // min.: Windows NT 4.0
    DWORD GetCompressedFileSize(LPCTSTR lpFileName, LPDWORD lpFileSizeHigh);
    ULONGLONG GetCompressedFileSize(LPCTSTR lpFileName);

private:
    typedef DWORD (WINAPI *TFNGetCompressedFileSize)(LPCTSTR lpFileName, LPDWORD lpFileSizeHigh);

    HMODULE m_hDll;

    CDynamicApi<TFNGetCompressedFileSize>               m_GetCompressedFileSize;
};

///////////////////////////////////////////////////////////////////////////////
///  CGetDiskFreeSpaceApi
///  Hides the differences between GetDiskFreeSpaceEx() and GetDiskFreeSpace().
///  This is important, because GetDiskFreeSpaceEx() is available only from
///  Windows 95 ORS2 onwards - not on the very first edition of Windows 95.
///
///
///  @remarks The methods are documented in the .cpp file
///////////////////////////////////////////////////////////////////////////////
class CGetDiskFreeSpaceApi
{
public:
    CGetDiskFreeSpaceApi();
    ~CGetDiskFreeSpaceApi();

    bool IsSupported();

    void GetDiskFreeSpace(LPCTSTR pszRootPath, ULONGLONG& total, ULONGLONG& unused);

private:
    typedef BOOL (WINAPI *TFNGetDiskFreeSpace)(LPCTSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters);
    typedef BOOL (WINAPI *TFNGetDiskFreeSpaceEx)(LPCTSTR lpDirectoryName, ULARGE_INTEGER *lpFreeBytesAvailable, ULARGE_INTEGER *lpTotalNumberOfBytes, ULARGE_INTEGER *lpTotalNumberOfFreeBytes);

    HMODULE m_hDll;

    CDynamicApi<TFNGetDiskFreeSpace>                    m_GetDiskFreeSpace;
    CDynamicApi<TFNGetDiskFreeSpaceEx>                  m_GetDiskFreeSpaceEx;
};

#endif // __WDS_OSSPECIFIC_H__
