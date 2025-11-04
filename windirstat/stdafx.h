// stdafx.h - include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently
//
// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
#include <sddl.h>
#include <winternl.h>
#include <powrprof.h>
#include <aclapi.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <dwmapi.h>
