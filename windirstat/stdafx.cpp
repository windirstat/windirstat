// stdafx.cpp - source file that includes just the standard includes
// windirstat.pch will be the pre-compiled header
// stdafx.obj will contain the pre-compiled type information
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

#include "stdafx.h"

#if !defined(HAVE_WIN7_SDK) || !HAVE_WIN7_SDK
#   if _MSC_VER <= 1500
#       pragma message ("WARNING: You're building a feature-incomplete WinDirStat ('#define HAVE_WIN7_SDK' missing or 0). Refer to https://bitbucket.org/windirstat/windirstat/wiki/Building for details on how to build with this version of Visual Studio.")
#   endif // Visual C/C++ 2008 and below
#endif // HAVE_WIN7_SDK
