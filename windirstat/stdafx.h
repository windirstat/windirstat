// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2004 Bernhard Seifert
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

#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#endif

// Modify the following defines if you have to target a platform prior to the ones specified below.
// Refer to MSDN for the latest info on corresponding values for different platforms.
#ifndef WINVER				// Allow use of features specific to Windows 95 and Windows NT 4 or later.
#define WINVER 0x0400		// Change this to the appropriate value to target Windows 98 and Windows 2000 or later.
#endif

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows NT 4 or later.
#define _WIN32_WINNT 0x0400	// Change this to the appropriate value to target Windows 98 and Windows 2000 or later.
#endif						

#ifndef _WIN32_WINDOWS			// Allow use of features specific to Windows 98 or later.
#define _WIN32_WINDOWS 0x0410	// Change this to the appropriate value to target Windows Me or later.
#endif

#ifndef _WIN32_IE			// Allow use of features specific to IE 4.0 or later.
#define _WIN32_IE 0x0400	// Change this to the appropriate value to target IE 5.0 or later.
#endif

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit

// turns off MFC's hiding of some common and often safely ignored warning messages
#define _AFX_ALL_WARNINGS

#include <afxwin.h>         // MFC Core
#include <afxext.h>         // MFC Extensions

#include <afxdtctl.h>		// MFC IE 4
#include <afxcmn.h>			// MFC Common Controls
#include <afxtempl.h>		// MFC Container classes
#include <afxmt.h>			// MFC Multithreading

#include <io.h>				// _access()
#include <math.h>			// floor(), fmod(), sqrt() etc.
#include <psapi.h>			// PROCESS_MEMORY_INFO
#include <mapi.h>			// E-Mail
#include <lmcons.h>			// UNLEN
#include <float.h>			// DBL_MAX

#include <atlbase.h>		// USES_CONVERSION, ComPtr<>

#pragma warning(disable: 4800) // forcing value to bool 'true' or 'false' (performance warning)

// Headers placed in the common directory
// (used by windirstat and by setup)
#include "../common/mdexceptions.h"
#include "../common/cotaskmem.h"
#include "../common/commonhelpers.h"
#include "../common/platform.h"

// General purpose headers
#include "selectobject.h"
#include "set.h"

#define countof(arr) (sizeof(arr)/sizeof((arr)[0]))

template<class T> int signum(T x) { return (x) < 0 ? -1 : (x) == 0 ? 0 : 1; }


