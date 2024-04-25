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

#define PRD_MAJVER                  1 // major product version
#define PRD_MINVER                  3 // minor product version
#define PRD_PATCH                   0 // patch number
#define PRD_BUILD                   0 // build number for product
#define PRD_BUILD_NUMERIC           0 // build number for product
#define FILE_MAJVER                 PRD_MAJVER // major file version
#define FILE_MINVER                 PRD_MINVER // minor file version
#define FILE_PATCH                  PRD_PATCH // patch number
#define FILE_BUILD                  PRD_BUILD // build number
#define FILE_BUILD_NUMERIC          PRD_BUILD_NUMERIC // build number for product
#define TEXT_WEBSITE                https:/##/windirstat.net // website
#define TEXT_PRODUCTNAME            WinDirStat // product's name
#if defined(WDS_RESLANG) && (WDS_RESLANG > 0)
#define TEXT_FILEDESC               TEXT_RESLANG language file
#else
#define TEXT_FILEDESC               Windows Directory Statistics (WinDirStat) // component description
#endif

#define STRING_COMPANY              "WinDirStat Team (windirstat.net)"
#define STRING_COPYRIGHT            "© 2003-2005 Bernhard Seifert, © 2004-2024 WinDirStat Team"
#define SOURCE_REPOSITORY           "https://github.com/windirstat/windirstat"
