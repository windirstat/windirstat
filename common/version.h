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

// The output of this file is
// VN_MAJOR, VN_MINOR, VN_REVISION, VN_BUILD, VN_FILEFLAG and VN_STRING


//-------------------------------------------------------------------
// Build categories. Uncomment _one_ line.
//

//#define BC_DEVEL				// Development version. The usual setting. File version is 0.0.0.buildno.
//#define BC_RELEASECANDIDATE		// Release candidate. Version number is relevant but not yet official. About-box shows x.y.zrcn. File version is x.y.z.buildno.
#define BC_RELEASE			// Set this only during official builds. About-box shows x.y.z. File version is x.y.z.buildno


//-------------------------------------------------------------------
// Version number. Relevant for BC_RELEASECANDIDATE and BC_RELEASE.
// 
#define VERNUM_MAJOR		1
#define VERNUM_MINOR		1
#define VERNUM_REVISION		1
// The following line is automatically incremented by linkcounter.exe.
// Format: #define blank LINKCOUNT blanks decimal
// Reset this to zero only when you increment VERNUM_MAJOR/MINOR/REVISION.

#define LINKCOUNT  18

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
#else
	#define UASPEC "Ansi"
#endif

#ifdef _DEBUG
	#define DRSPEC " Debug"
#else
	#define DRSPEC ""
#endif

#define VERVARIANT " (" UASPEC DRSPEC ")"

#if defined(BC_DEVEL)

	#define VN_MAJOR	0
	#define VN_MINOR	0
	#define VN_REVISION	0
	#define VN_FILEFLAG	0
	#define VN_STRING	"devel" VERVARIANT

#elif defined(BC_RELEASECANDIDATE)

	#define VN_MAJOR	VERNUM_MAJOR
	#define VN_MINOR	VERNUM_MINOR
	#define VN_REVISION	VERNUM_REVISION
	#define VN_FILEFLAG	VS_FF_PRERELEASE
	#define VN_STRING	PPS(VN_MAJOR) "." PPS(VN_MINOR) "." PPS(VN_REVISION) "rc" PPS(VERNUM_CANDIDATE) VERVARIANT

#elif defined(BC_RELEASE)

	#define VN_MAJOR	VERNUM_MAJOR
	#define VN_MINOR	VERNUM_MINOR
	#define VN_REVISION	VERNUM_REVISION
	#define VN_FILEFLAG	0
	#define VN_STRING	PPS(VN_MAJOR) "." PPS(VN_MINOR) "." PPS(VN_REVISION) VERVARIANT

#endif


//
// The output of this file is
// VN_MAJOR, VN_MINOR, VN_REVISION, VN_BUILD, VN_FILEFLAG and VN_STRING
//

// ...nothing else.
#undef BC_DEVEL
#undef BC_RELEASECANDIDATE
#undef BC_RELEASE


// $Log$
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
