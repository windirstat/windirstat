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
#define VERNUM_REVISION		0
// The following line is automatically incremented by linkcounter.exe.
// Format: #define blank LINKCOUNT blanks decimal
// Reset this to zero only when you increment VERNUM_MAJOR/MINOR/REVISION.
#define LINKCOUNT  116

//-------------------------------------------------------------------
// Release candidate number. Relevant for BC_RELEASECANDIDATE.
//
#define VERNUM_CANDIDATE	3



/////////////////////////////////////////////////////////////////////
// Derived values from here. Do not edit.

#define VN_BUILD	LINKCOUNT

#define PPSX(s) #s				    
#define PPS(s) PPSX(s)

#if defined(BC_DEVEL)

	#define VN_MAJOR	0
	#define VN_MINOR	0
	#define VN_REVISION	0
	#define VN_FILEFLAG	0
	#define VN_STRING	"devel"

#elif defined(BC_RELEASECANDIDATE)

	#define VN_MAJOR	VERNUM_MAJOR
	#define VN_MINOR	VERNUM_MINOR
	#define VN_REVISION	VERNUM_REVISION
	#define VN_FILEFLAG	VS_FF_PRERELEASE
	#define VN_STRING	PPS(VN_MAJOR) "." PPS(VN_MINOR) "." PPS(VN_REVISION) "rc" PPS(VERNUM_CANDIDATE)

#elif defined(BC_RELEASE)

	#define VN_MAJOR	VERNUM_MAJOR
	#define VN_MINOR	VERNUM_MINOR
	#define VN_REVISION	VERNUM_REVISION
	#define VN_FILEFLAG	0
	#define VN_STRING	PPS(VN_MAJOR) "." PPS(VN_MINOR) "." PPS(VN_REVISION)

#endif


//
// The output of this file is
// VN_MAJOR, VN_MINOR, VN_REVISION, VN_BUILD, VN_FILEFLAG and VN_STRING
//

// ...nothing else.
#undef BC_DEVEL
#undef BC_RELEASECANDIDATE
#undef BC_RELEASE

