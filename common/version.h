// version.h - Version number. Used by all resource scripts and by aboutdlg.cpp.
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
//
// Last modified: $Date$

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

//#define BC_DEVEL			// Development version. The usual setting. File version is 0.0.0.buildno.
//#define BC_RELEASECANDIDATE		// Release candidate. Version number is relevant but not yet official. About-box shows x.y.zrcn. File version is x.y.z.buildno.
#define BC_RELEASE			// Set this only during official builds. About-box shows x.y.z. File version is x.y.z.buildno

// A release version must not contain debug information. Raise an error!
#if defined(_DEBUG) && defined(BC_RELEASE)
  #error BC_RELEASE _and_ _DEBUG are defined. This must not happen. Releases contain no debug information.
#endif

// This will not change to often, but the years need to be modified
// regularly, so it can be in one central place
#define VN_COPYRIGHTSTRING "Copyright (C) 2003-2005 Bernhard Seifert"

//-------------------------------------------------------------------
// Version number. Relevant for BC_RELEASECANDIDATE and BC_RELEASE.
//
#define VERNUM_MAJOR		1
#define VERNUM_MINOR		1
#define VERNUM_REVISION		2
// The following line is automatically incremented by linkcounter.exe.
// Format: #define blank LINKCOUNT blanks decimal
// Reset this to zero only when you increment VERNUM_MAJOR/MINOR/REVISION.

#define LINKCOUNT  80

//-------------------------------------------------------------------
// Release candidate number. Relevant for BC_RELEASECANDIDATE.
//
#define VERNUM_CANDIDATE	1



/////////////////////////////////////////////////////////////////////
// Derived values from here. Do not edit.

#define VN_BUILD	LINKCOUNT

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
#define VN_STRING_DLL	PPS(VERNUM_MAJOR) "." PPS(VERNUM_MINOR) "." PPS(VERNUM_REVISION) "." PPS(VN_BUILD)

#if defined(BC_DEVEL)

	#define VN_MAJOR	0
	#define VN_MINOR	0
	#define VN_REVISION	0
	#define VN_FILEFLAG	0
	#define VN_STRING_DLL	PPS(VERNUM_MAJOR) "." PPS(VERNUM_MINOR) "." PPS(VERNUM_REVISION) "." PPS(VN_BUILD)
	// The variant (debug or not/ unicode or not) is not relevant for resource DLLs, but for EXEs
	#define VN_STRING_EXE	VN_STRING_DLL " devel" VERVARIANT

#elif defined(BC_RELEASECANDIDATE)

	#define VN_MAJOR	VERNUM_MAJOR
	#define VN_MINOR	VERNUM_MINOR
	#define VN_REVISION	VERNUM_REVISION
	#define VN_FILEFLAG	VS_FF_PRERELEASE
	// The variant (debug or not/ unicode or not) is not relevant for resource DLLs, but for EXEs
	#define VN_STRING_EXE	VN_STRING_DLL "rc" PPS(VERNUM_CANDIDATE) VERVARIANT

#elif defined(BC_RELEASE)

	#define VN_MAJOR	VERNUM_MAJOR
	#define VN_MINOR	VERNUM_MINOR
	#define VN_REVISION	VERNUM_REVISION
	#define VN_FILEFLAG	0
	// The variant (debug or not/ unicode or not) is not relevant for resource DLLs, but for EXEs
	#define VN_STRING_EXE	VN_STRING_DLL VERVARIANT

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

// ...nothing else.
#undef BC_DEVEL
#undef BC_RELEASECANDIDATE
#undef BC_RELEASE


// $Log$
// Revision 1.38  2005/07/16 13:58:05  assarbad
// *** empty log message ***
//
// Revision 1.37  2005/07/16 13:38:44  assarbad
// - Preparation for release 1.1.2
//
// Revision 1.36  2005/04/17 20:45:15  assarbad
// - Now the list of translators is shared among all translations. See changelog for details.
//
// Revision 1.35  2005/04/17 18:13:42  assarbad
// - Moved some "static" resource strings into the respective *.rc2 files
// - Corrected typo in Russian DLL
// - Modified behavior of VERSIONINFO for DLLs. "version.h" has changed therefore
// ... for details as usual, see the changelog.
//
// Revision 1.34  2005/04/17 12:27:17  assarbad
// - For details see changelog of 2005-04-17
//
// Revision 1.33  2005/04/10 16:49:22  assarbad
// - Some smaller fixes including moving the resource string version into the rc2 files
//
// Revision 1.32  2005/04/10 14:57:09  assarbad
// - Added Italian and Czech. See changelog for details.
//
// Revision 1.31  2005/02/14 23:03:22  bseifert
// Add Hungarian help file. Minor resource file corrections.
//
// Revision 1.30  2005/02/07 16:22:28  assarbad
// - Adjusted all resources to show Spanish. Missing translation of the word
//  "Spanish" for Hungarian and of the words "Spanish" and "Hungarian" for French.
//
// Revision 1.29  2005/01/29 07:08:13  bseifert
// Added Hungarian resource dll.
//
// Revision 1.28  2005/01/02 03:56:04  bseifert
// Copyright -2005. Release 1.1.1
//
// Revision 1.27  2004/12/31 16:01:38  bseifert
// Bugfixes. See changelog 2004-12-31.
//
// Revision 1.26  2004/12/30 11:26:12  bseifert
// Decided to use serif font for help files. Incremented version number. rc1.
//
// Revision 1.25  2004/12/30 10:18:39  bseifert
// Updated RESOURCEVERSION. Updated testplan. setup: "minimize size".
//
// Revision 1.24  2004/12/30 08:01:11  bseifert
// helpfile updated.
//
// Revision 1.23  2004/12/25 13:41:46  bseifert
// Synced help files.
//
// Revision 1.22  2004/12/24 10:39:27  bseifert
// Added Polish setup and  Polish help file.
//
// Revision 1.21  2004/12/19 10:52:36  bseifert
// Minor fixes.
//
// Revision 1.20  2004/12/12 13:40:47  bseifert
// Improved image coloring. Junction point image now with awxlink overlay.
//
// Revision 1.19  2004/12/12 08:34:56  bseifert
// Aboutbox: added Authors-Tab. Removed license.txt from resource dlls (saves 16 KB per dll).
//
// Revision 1.18  2004/11/27 07:19:36  bseifert
// Unicode/Ansi/Debug-specification in version.h/Aboutbox. Minor fixes.
//
// Revision 1.17  2004/11/25 23:07:22  assarbad
// - Derived CFileFindWDS from CFileFind to correct a problem of the ANSI version
//
// Revision 1.16  2004/11/13 18:48:29  bseifert
// Few corrections in Polish windirstat.rc. Thanks to Darek.
//
// Revision 1.15  2004/11/13 08:17:04  bseifert
// Remove blanks in Unicode Configuration names.
//
// Revision 1.14  2004/11/12 21:03:53  bseifert
// Added wdsr0415.dll. New output dirs for Unicode. Minor corrections.
//
// Revision 1.13  2004/11/12 00:47:42  assarbad
// - Fixed the code for coloring of compressed/encrypted items. Now the coloring spans the full row!
//
// Revision 1.12  2004/11/09 23:23:03  bseifert
// Committed new LINKCOUNT to demonstrate merge conflict.
//
// Revision 1.11  2004/11/07 20:14:30  assarbad
// - Added wrapper for GetCompressedFileSize() so that by default the compressed file size will be shown.
//
// Revision 1.10  2004/11/07 00:51:30  assarbad
// *** empty log message ***
//
// Revision 1.9  2004/11/07 00:06:34  assarbad
// - Fixed minor bug with ampersand (details in changelog.txt)
//
// Revision 1.8  2004/11/05 16:53:05  assarbad
// Added Date and History tag where appropriate.
//
