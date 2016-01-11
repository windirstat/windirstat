// common_rsrcstr.h
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2016 WinDirStat team (windirstat.info)
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

#ifndef __COMMON_RSRCSTR_H_VER__
#define __COMMON_RSRCSTR_H_VER__ 2014021723
#if (defined(_MSC_VER) && (_MSC_VER >= 1020)) || defined(__MCPP)
#pragma once
#endif // Check for "#pragma once" support

#define IDSS_TRANSLATORS        L"<list too long>"

// Version of resource DLL
#define IDSS_RESOURCEDLL        "Resource Version 5"
// Version information in feedback always appears in English
#define IDSS_FROMsPLATFORMs     "From: %1!s!. Platform: %2!s!.\r\n\r\n"
#define IDSS_SEV_CRITICAL       "Critical Bug"
#define IDSS_SEV_GRAVE          "Serious Bug"
#define IDSS_SEV_NORMAL         "Bug"
#define IDSS_SEV_WISH           "Wish"
#define IDSS_SEV_FEEDBACK       "Feedback"

#endif // __COMMON_RSRCSTR_H_VER__
