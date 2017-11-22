// stdafx.h - include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently
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

#ifndef __STDAFX_H_VER__
#define __STDAFX_H_VER__ 2017112218
#if (defined(_MSC_VER) && (_MSC_VER >= 1020)) || defined(__MCPP)
#pragma once
#endif /* Check for "#pragma once" support */

#if (defined(HAVE_WIN7_SDK) && HAVE_WIN7_SDK) || (_MSC_VER >= 1600)
#   define SUPPORT_W7_TASKBAR 1
#   define SUPPORT_ELEVATION  1
#endif // HAVE_WIN7_SDK

#ifndef _WIN32_WINNT
#   ifdef WINVER
#       define _WIN32_WINNT WINVER
#   else
#       define _WIN32_WINNT _WIN32_WINNT_WINXP
#       define WINVER       _WIN32_WINNT_WINXP
#   endif // WINVER
#endif // !_WIN32_WINNT

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS  // some CString constructors will be explicit

// turns off MFC's hiding of some common and often safely ignored warning messages
#define _AFX_ALL_WARNINGS

#include <afxwin.h>         // MFC Core
#include <afxext.h>         // MFC Extensions

#include <afxdtctl.h>       // MFC IE 4
#include <afxcmn.h>         // MFC Common Controls
#include <afxtempl.h>       // MFC Container classes
#include <afxmt.h>          // MFC Multi-threading
#include <afxinet.h>        // For CInternet* classes

#include <io.h>             // _access()
#include <cmath>            // floor(), fmod(), sqrt() etc.
#include <cfloat>           // DBL_MAX
#include <psapi.h>          // PROCESS_MEMORY_INFO

#include <atlbase.h>        // ComPtr<>

// Aliasing the macros from wingdi.h
#define RGB_GET_RVALUE(rgb) GetRValue(rgb)
#define RGB_GET_GVALUE(rgb) GetGValue(rgb)
#define RGB_GET_BVALUE(rgb) GetBValue(rgb)

template<typename T> int signum(T x) { return (x) < 0 ? -1 : (x) == 0 ? 0 : 1; }

/// signum function for unsigned numbers.
template<typename T> int usignum(T x, T y) { return (x) < (y) ? -1 : (x) == (y) ? 0 : 1; }

#define WEAK_ASSERT /##/ ASSERT

#endif /* __STDAFX_H_VER__ */
