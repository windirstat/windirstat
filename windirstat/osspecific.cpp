// osspecific.cpp - Implementation of the platform-specific classes
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2006, 2008, 2010 Oliver Schneider (assarbad.net)
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

CDllModule dllKernel32(nameKernel32);
CDllModule dllShell32(nameShell32);
CDllModule dllPsApi(namePsApi);

// Required to use the system image lists
// - http://www.catch22.net/tuts/sysimg
// - http://msdn.microsoft.com/en-us/library/bb776418(VS.85).aspx
BOOL FileIconInit(__in  BOOL fRestoreCache)
{
    typedef BOOL (WINAPI * TFNFileIconInit)(BOOL);
    static HMODULE hShell32 = NULL;
    static TFNFileIconInit pfnFileIconInit = 0;
    if(!hShell32)
    {
        hShell32 = ::LoadLibrary(TEXT("shell32.dll"));
    }
    if(hShell32 && !pfnFileIconInit)
    {
        pfnFileIconInit = reinterpret_cast<TFNFileIconInit>(::GetProcAddress(hShell32, ((LPCSTR)660)));
    }
    if(pfnFileIconInit)
    {
        return pfnFileIconInit(fRestoreCache);
    }
    return TRUE;
}
