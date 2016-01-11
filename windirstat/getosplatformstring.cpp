// getosplatformstring.cpp - Implementation of GetOsPlatformString()
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2016 WinDirStat team (windirstat.info)
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
//            - oliver   -> http://windirstat.info/contact/oliver/
//

#include "stdafx.h"
#include "windirstat.h"
#include "getosplatformstring.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CString GetOsPlatformString()
{
    CString ret;

    OSVERSIONINFO osvi;
    ZeroMemory(&osvi, sizeof(osvi));
    osvi.dwOSVersionInfoSize = sizeof(osvi);

    if(!GetVersionEx(&osvi))
    {
        return LoadString(IDS__UNKNOWN_);
    }

    // FIXME: Update this to include Windows Vista/2008 and 7/2008 R2
    switch (osvi.dwPlatformId)
    {
    case VER_PLATFORM_WIN32_NT:
        if(osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2)
        {
            ret = _T("Windows Server 2003");
        }
        else if(osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1)
        {
            ret = _T("Windows XP");
        }
        else if(osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0)
        {
            ret = _T("Windows 2000");
        }
        else if(osvi.dwMajorVersion <= 4)
        {
            ret = _T("Windows NT");
        }
        else
        {
            ret.Format(_T("Windows %u.%u"), osvi.dwMajorVersion, osvi.dwMinorVersion);
        }
        if(_tcslen(osvi.szCSDVersion) > 0)
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
