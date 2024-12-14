// stdafx.h - include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
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

// Exclude rarely-used stuff from Windows headers
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS  // some CStringW constructors will be explicit

// enables new gdiplus version
#define GDIPVER 0x0110

#include <afxwin.h>         // MFC Core
#include <afxext.h>         // MFC Extensions

#include <afxdtctl.h>       // MFC IE 4
#include <afxcmn.h>         // MFC Common Controls
#include <afxmt.h>          // MFC Multi-threading
#include <afxcontrolbars.h> // MFC support for ribbons and control bars

#include <VersionHelpers.h>
#include <cmath>
#include <psapi.h>
#include <winioctl.h>
#include <bcrypt.h>
#include <sal.h>
#include <wincrypt.h>
#include <winioctl.h>
#include <sddl.h>
#include <winternl.h>
#include <powrprof.h>

constexpr auto RGB_GET_RVALUE(auto rgb) { return (rgb >>  0) & 0xFF; }
constexpr auto RGB_GET_GVALUE(auto rgb) { return (rgb >>  8) & 0xFF; }
constexpr auto RGB_GET_BVALUE(auto rgb) { return (rgb >> 16) & 0xFF; }

constexpr auto roundaway(auto x) { return static_cast<int>(x + (x < 0.0 ? -0.5 : 0.5)); };
constexpr auto signum(auto x) { return x < 0 ? -1 : x == 0 ? 0 : 1; };
constexpr auto usignum(auto x, auto y) { return x < y ? -1 : x == y ? 0 : 1; };
