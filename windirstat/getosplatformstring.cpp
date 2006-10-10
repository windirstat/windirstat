// getosplatformstring.cpp - Implementation of GetOsPlatformString()
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2006 Oliver Schneider (assarbad.net)
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
// Author(s): - bseifert -> bseifert@users.sourceforge.net, bseifert@daccord.net
//            - assarbad -> http://assarbad.net/en/contact
//
// $Id$

#include "stdafx.h"
#include "windirstat.h"
#include "getosplatformstring.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CString GetOsPlatformString()
{
	CString ret;

	OSVERSIONINFO osvi;
	ZeroMemory(&osvi, sizeof(osvi));
	osvi.dwOSVersionInfoSize = sizeof(osvi);

	if(!GetVersionEx(&osvi))
	{
		return LoadString(IDS__UNKNOWN_);
	}

	switch (osvi.dwPlatformId)
	{
	case VER_PLATFORM_WIN32_NT:
		if(osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2)
		{
			ret = TEXT("Windows Server 2003");
		}
		else if(osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1)
		{
			ret = TEXT("Windows XP");
		}
		else if(osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0)
		{
			ret = TEXT("Windows 2000");
		}
		else if(osvi.dwMajorVersion <= 4)
		{
			ret = TEXT("Windows NT");
		}
		else
		{
			ret.Format(TEXT("Windows %u.%u"), osvi.dwMajorVersion, osvi.dwMinorVersion);
		}
		if(_tcslen(osvi.szCSDVersion) > 0)
		{
			CString s;
			s.Format(TEXT(" (%s)"), osvi.szCSDVersion);
			ret += s;
		}
		break;

	case VER_PLATFORM_WIN32_WINDOWS:
		if(osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0)
		{
			ret = TEXT("Windows 95");
			if(osvi.szCSDVersion[1] == chrCapC || osvi.szCSDVersion[1] == chrCapB)
			{
				ret += TEXT(" OSR2");
			}
		} 
		else if(osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 10)
		{
			ret = TEXT("Windows 98");
			if(osvi.szCSDVersion[1] == chrCapA)
			{
				ret += TEXT(" SE");
			}
		} 
		else if(osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 90)
		{
			ret = TEXT("Windows ME");
		}
		else
		{
			ret.Format(TEXT("<platform %u %u.%u>"), osvi.dwPlatformId, osvi.dwMajorVersion, osvi.dwMinorVersion);
		}
		break;

	case VER_PLATFORM_WIN32s:
		{
			ret = TEXT("Win32s\n"); // ooops!!
		}
		break;

	default:
		{
			ret.Format(TEXT("<platform id %u>"), osvi.dwPlatformId);
		}
		break;
	}

	return ret;
}

// $Log$
// Revision 1.7  2006/10/10 01:41:50  assarbad
// - Added credits for Gerben Wieringa (Dutch translation)
// - Replaced Header tag by Id for the CVS tags in the source files ...
// - Started re-ordering of the files inside the project(s)/solution(s)
//
// Revision 1.6  2006/07/04 23:37:39  assarbad
// - Added my email address in the header, adjusted "Author" -> "Author(s)"
// - Added CVS Log keyword to those files not having it
// - Added the files which I forgot during last commit
//
// Revision 1.5  2006/07/04 22:49:20  assarbad
// - Replaced CVS keyword "Date" by "Header" in the file headers
//
// Revision 1.4  2006/07/04 20:45:22  assarbad
// - See changelog for the changes of todays previous check-ins as well as this one!
//
// Revision 1.3  2004/11/05 16:53:07  assarbad
// Added Date and History tag where appropriate.
//
