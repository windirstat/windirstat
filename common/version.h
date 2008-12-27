// version.h - Version number. Used by all resource scripts and by aboutdlg.cpp.
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
// $Id$

/*-------------------------------------------------------------------
  This file defines the following:
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  - VN_FILEFLAG_EXE (contains information about RC or debug build)
  - VN_STRING_EXE (contains info about unicode and debug)
  - VN_STRING_DLL (just major.minor.rev.build)
  - VN_FILEVERSION_EXE and VN_PRODVERSION_EXE (only different from
    the DLL version if a development release)
  - VN_FILEVERSION_DLL and VN_PRODVERSION_DLL (numeric representation
    of VN_STRING_DLL)
  - VN_FILEOS_EXE (this depends on Unicode and ANSI!
      For Unicode: VN_FILEOS_EXE == VOS_NT_WINDOWS32
      For ANSI   : VN_FILEOS_EXE == VOS__WINDOWS32 )
  - VN_COPYRIGHTSTRING (copyright to include in the VERSIONINFO)
  - VN_RESOURCEDLL (version of resource DLLs must be the same for DLL
    and EXE at runtime)
  -------------------------------------------------------------------*/

//-------------------------------------------------------------------
// Build categories. Uncomment _one_ line.
//
#ifndef __WDS_VERSION_H__
#define __WDS_VERSION_H__

#define BC_DEVEL                    // Development version. The usual setting. File version is 0.0.0.buildno.
//#define BC_RELEASECANDIDATE       // Release candidate. Version number is relevant but not yet official. About-box shows x.y.zrcn. File version is x.y.z.buildno.
//#define BC_RELEASE                // Set this only during official builds. About-box shows x.y.z. File version is x.y.z.buildno

// A release version must not contain debug information. Raise an error!
#if defined(_DEBUG) && defined(BC_RELEASE)
  #error BC_RELEASE _and_ _DEBUG are defined. This must not happen. Releases contain no debug information.
#endif

// This will not change to often, but the years need to be modified
// regularly, so it can be in one central place
#define VN_COPYRIGHTSTRING "Copyright (C) 2003-2005 Bernhard Seifert, (C) 2004-2006, 2008 Oliver Schneider"

//-------------------------------------------------------------------
// Version number. Relevant for BC_RELEASECANDIDATE and BC_RELEASE.
//
#define VERNUM_MAJOR        1
#define VERNUM_MINOR        3
#define VERNUM_REVISION     0
// The following line is automatically incremented by linkcounter.exe.
// Format: #define blank LINKCOUNT blanks decimal
// Reset this to zero only when you increment VERNUM_MAJOR/MINOR/REVISION.

#include "../common/buildnumber.h"

//-------------------------------------------------------------------
// Release candidate number. Relevant for BC_RELEASECANDIDATE.
//
#define VERNUM_CANDIDATE    1



/////////////////////////////////////////////////////////////////////
// Derived values from here. Do not edit.

#define VN_BUILD    LINKCOUNT

#define PPSX(s) #s
#define PPS(s) PPSX(s)

#ifdef _UNICODE
    #define UASPEC "Unicode"
    // OS version is only relevant for the EXE
    #define VN_FILEOS_EXE VOS_NT_WINDOWS32
#else
    #define UASPEC "Ansi"
    // OS version is only relevant for the EXE
    #define VN_FILEOS_EXE VOS__WINDOWS32
#endif

#ifdef _DEBUG
    #define DRSPEC " Debug"
#else
    #define DRSPEC ""
#endif

#define VERVARIANT " (" UASPEC DRSPEC ")"

// This is just major.minor.rev.build always!
#define VN_STRING_DLL   PPS(VERNUM_MAJOR) "." PPS(VERNUM_MINOR) "." PPS(VERNUM_REVISION) "." PPS(VN_BUILD)

#if defined(BC_DEVEL)

    #define VN_MAJOR    0
    #define VN_MINOR    0
    #define VN_REVISION 0
    #define VN_FILEFLAG 0
    #define VN_STRING_DLL   PPS(VERNUM_MAJOR) "." PPS(VERNUM_MINOR) "." PPS(VERNUM_REVISION) "." PPS(VN_BUILD)
    // The variant (debug or not/ Unicode or not) is not relevant for resource DLLs, but for EXEs
    #define VN_STRING_EXE   VN_STRING_DLL " devel" VERVARIANT

#elif defined(BC_RELEASECANDIDATE)

    #define VN_MAJOR    VERNUM_MAJOR
    #define VN_MINOR    VERNUM_MINOR
    #define VN_REVISION VERNUM_REVISION
    #define VN_FILEFLAG VS_FF_PRERELEASE
    // The variant (debug or not/ Unicode or not) is not relevant for resource DLLs, but for EXEs
    #define VN_STRING_EXE   VN_STRING_DLL "rc" PPS(VERNUM_CANDIDATE) VERVARIANT

#elif defined(BC_RELEASE)

    #define VN_MAJOR    VERNUM_MAJOR
    #define VN_MINOR    VERNUM_MINOR
    #define VN_REVISION VERNUM_REVISION
    #define VN_FILEFLAG 0
    // The variant (debug or not/ Unicode or not) is not relevant for resource DLLs, but for EXEs
    #define VN_STRING_EXE   VN_STRING_DLL VERVARIANT

#endif

// EXE files have a different version number in development releases
#define VN_FILEVERSION_EXE VN_MAJOR,VN_MINOR,VN_REVISION,VN_BUILD
#define VN_PRODVERSION_EXE VN_FILEVERSION_EXE
// Resource DLLs need no different version number
#define VN_FILEVERSION_DLL VERNUM_MAJOR,VERNUM_MINOR,VERNUM_REVISION,VN_BUILD
#define VN_PRODVERSION_DLL VN_FILEVERSION_DLL

// Whether debug or not is not relevant for resource DLLs
#ifdef _DEBUG
 #define VN_FILEFLAG_EXE VS_FF_DEBUG | VN_FILEFLAG
#else
 #define VN_FILEFLAG_EXE VN_FILEFLAG
#endif

#ifdef BC_DEVEL
#define IDSS_CHECKUPDATESRV      "localhost"
#else
#define IDSS_CHECKUPDATESRV      "windirstat.info"
#endif
#define IDSS_CHECKUPDATEURI      "checkupdate.php"
#define IDXS_CHECKUPDATEPORT     80

// ...nothing else.
#undef BC_DEVEL
#undef BC_RELEASECANDIDATE
#undef BC_RELEASE

#endif // __WDS_VERSION_H__
