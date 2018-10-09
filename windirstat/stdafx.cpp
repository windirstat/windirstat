// stdafx.cpp - source file that includes just the standard includes
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

#if !defined(HAVE_WIN7_SDK) || !HAVE_WIN7_SDK
#   if _MSC_VER <= 1500
#       if !defined(_ANSISTRING) || !defined(ANSISTRING)
#           define _ANSISTRING(text) #text
#           define ANSISTRING(text) _ANSISTRING(text)
#       endif
#       pragma message (ANSISTRING(__FILE__) "(" ANSISTRING(__LINE__) ") : warning: You're building a feature-incomplete WinDirStat ('#define HAVE_WIN7_SDK' missing or 0). Refer to https://bitbucket.org/windirstat/windirstat/wiki/Building for details on how to build with this version of Visual Studio.")
#   endif // Visual C/C++ 2008 and below
#endif // HAVE_WIN7_SDK

#if (_WIN32_WINNT < _WIN32_WINNT_VISTA)
namespace {
    // Borrowed from: http://terryto-blog.tumblr.com/post/6722591298/gettickcount64-alternatives
    static ULONGLONG WINAPI CompatibleGetTickCount64_()
    {
        static __declspec(thread) ULONGLONG high = 0;
        static __declspec(thread) ULONG lastLow = 0;
#pragma warning(suppress: 28159)
        const ULONG low = GetTickCount();
        if (lastLow > low)
        { /* wrapped */
            high += 0x100000000I64;
        } /* else... not wrapped */
        lastLow = low;
        return high | (ULONGLONG)low;
    }
}

typedef ULONGLONG(WINAPI *GetTickCount64_t)(void);
EXTERN_C GetTickCount64_t _GetTickCount64 = NULL;

void InitGetTickCount64()
{
    if (!_GetTickCount64)
    {
        static HMODULE hKernel32 = ::GetModuleHandle(_T("kernel32.dll"));
        if (hKernel32)
        {
            GetTickCount64_t pfnGetTickCount64 = (GetTickCount64_t)::GetProcAddress(hKernel32, "GetTickCount64");
            if (pfnGetTickCount64)
            {
                _GetTickCount64 = pfnGetTickCount64;
            }
        }
    }
    if (!_GetTickCount64)
    {
        // Fallback
        _GetTickCount64 = CompatibleGetTickCount64_;
    }
}

#endif /* (_WIN32_WINNT < _WIN32_WINNT_VISTA) */
