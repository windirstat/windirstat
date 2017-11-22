// getosplatformstring.cpp - Implementation of GetOsPlatformString()
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

#include "stdafx.h"
#include "windirstat.h"
#include <common/commonhelpers.h>
#include "getosplatformstring.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#ifndef _Return_type_success_
#   define _Return_type_success_(x)
#endif

namespace {
    typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
    /*lint -save -e624 */  // Don't complain about different typedefs.
    typedef NTSTATUS *PNTSTATUS;
    /*lint -restore */  // Resume checking for different typedefs.
}

CString GetOsPlatformString()
{
    static NTSTATUS (WINAPI* RtlGetVersion)(LPOSVERSIONINFOEXW);

    if(!RtlGetVersion)
    {
        *(FARPROC*)&RtlGetVersion = GetProcAddress(GetModuleHandle(_T("ntdll.dll")), "RtlGetVersion");
        ASSERT(RtlGetVersion != NULL);
        if(!RtlGetVersion)
        {
            return LoadString(IDS__UNKNOWN_);
        }
    }
    OSVERSIONINFOEXW osvi = { sizeof(OSVERSIONINFOEXW), 0, 0, 0, 0,{ 0 } };

    CString ret;

    NTSTATUS ntStatus = RtlGetVersion(&osvi);
    if(ntStatus < 0)
    {
        return LoadString(IDS__UNKNOWN_); // FIXME: include the status code
    }

    LPCTSTR lpszMajorName = NULL;

    switch (osvi.dwPlatformId)
    {
    case VER_PLATFORM_WIN32_NT:
        switch(osvi.dwMajorVersion)
        {
        case 5:
            if(osvi.dwMinorVersion == 0)
                lpszMajorName = (VER_NT_WORKSTATION == osvi.wProductType) ? _T("2000") : _T("2000 Server");
            else
                if(osvi.dwMinorVersion == 1)
                    lpszMajorName = _T("XP");
                else
                    if(osvi.dwMinorVersion == 2)
                        lpszMajorName = (VER_NT_WORKSTATION == osvi.wProductType) ? _T("XP x64") : _T("Server 2003");
            break;
        case 6:
            if(osvi.dwMinorVersion == 0)
                lpszMajorName = (VER_NT_WORKSTATION == osvi.wProductType) ? _T("Vista") : _T("Server 2008");
            else
                if(osvi.dwMinorVersion == 1)
                    lpszMajorName = (VER_NT_WORKSTATION == osvi.wProductType) ? _T("7") : _T("Server 2008 R2");
                else
                    if(osvi.dwMinorVersion == 2)
                        lpszMajorName = (VER_NT_WORKSTATION == osvi.wProductType) ? _T("8") : _T("Server 2012");
                    else
                        if(osvi.dwMinorVersion == 3)
                            lpszMajorName = (VER_NT_WORKSTATION == osvi.wProductType) ? _T("8.1") : _T("Server 2012 R2");
            break;
        case 10:
            if(osvi.dwMinorVersion == 0)
                lpszMajorName = (VER_NT_WORKSTATION == osvi.wProductType) ? _T("10") : _T("Server 2016");
            break;
        default:
            break;
        }

        if(!lpszMajorName)
        {
            if(osvi.wServicePackMajor)
                ret.Format(_T("Windows %u.%u.%u, SP %u"), osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber, osvi.wServicePackMajor);
            else
                ret.Format(_T("Windows %u.%u.%u"), osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
        }
        else
        {
            if(osvi.wServicePackMajor)
                ret.Format(_T("Windows %s [%u.%u.%u], SP %u"), lpszMajorName, osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber, osvi.wServicePackMajor);
            else
                ret.Format(_T("Windows %s [%u.%u.%u]"), lpszMajorName, osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
        }
        if(osvi.szCSDVersion[0])
        {
            CString s;
            s.Format(_T(" (%s)"), osvi.szCSDVersion);
            ret += s;
        }
        break;

    default:
        {
            ret.Format(_T("<platform id %u>"), osvi.dwPlatformId);
        }
        break;
    }

    return ret;
}
