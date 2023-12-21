// osspecific.h - Declaration of CVolumeApi, CRecycleBinApi, CPsapi,
//                CGetDiskFreeSpaceApi, CGetCompressedFileSizeApi
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2017 WinDirStat Team (windirstat.net)
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

#pragma once

#include <Windows.h>
#include <ShellAPI.h>

BOOL FileIconInit(__in  BOOL fRestoreCache);
CString GetCurrentDesktopName();
CString GetCurrentWinstaName();

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
