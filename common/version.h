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

#include "hgid.h"

#if defined(WDS_RESLANG) && (WDS_RESLANG > 0)
#   if (WDS_RESLANG == 0x0405)
#       define TEXT_RESLANG             Czech
#   elif (WDS_RESLANG == 0x0407)
#       define TEXT_RESLANG             German
#   elif (WDS_RESLANG == 0x0409)
#       define TEXT_RESLANG             English (US)
#   elif (WDS_RESLANG == 0x040a)
#       define TEXT_RESLANG             Spanish
#   elif (WDS_RESLANG == 0x040b)
#       define TEXT_RESLANG             Finnish
#   elif (WDS_RESLANG == 0x040c)
#       define TEXT_RESLANG             French
#   elif (WDS_RESLANG == 0x040e)
#       define TEXT_RESLANG             Hungarian
#   elif (WDS_RESLANG == 0x0410)
#       define TEXT_RESLANG             Italian
#   elif (WDS_RESLANG == 0x0413)
#       define TEXT_RESLANG             Dutch
#   elif (WDS_RESLANG == 0x0415)
#       define TEXT_RESLANG             Polish
#   elif (WDS_RESLANG == 0x0416)
#       define TEXT_RESLANG             Portuguese (Brazil)
#   elif (WDS_RESLANG == 0x0419)
#       define TEXT_RESLANG             Russian
#   elif (WDS_RESLANG == 0x0425)
#       define TEXT_RESLANG             Estonian
#   else
#       error The language you defined does not have a name yet. Adjust the file with this error to include a name.
#   endif
#endif

#define PRD_MAJVER                  1 // major product version
#define PRD_MINVER                  3 // minor product version
#define PRD_PATCH                   0 // patch number
#define PRD_BUILD                   HG_REV_NO // build number for product
#define PRD_BUILD_NUMERIC           HG_REV_NO_NUMERIC // build number for product
#if defined(WDS_RESLANG) && (WDS_RESLANG > 0)
#   define FILE_MAJVER              1 // resource language version, changing this denotes incompatibilities
#   define FILE_MINVER              0
#   define FILE_PATCH               0
#else
#   define FILE_MAJVER              PRD_MAJVER // major file version
#   define FILE_MINVER              PRD_MINVER // minor file version
#   define FILE_PATCH               PRD_PATCH // patch number
#endif
#define FILE_BUILD                  PRD_BUILD // build number
#define FILE_BUILD_NUMERIC          PRD_BUILD_NUMERIC // build number for product
#define TEXT_WEBSITE                https:/##/windirstat.net // website
#define TEXT_PRODUCTNAME            WinDirStat // product's name
#if defined(WDS_RESLANG) && (WDS_RESLANG > 0)
#define TEXT_FILEDESC               TEXT_RESLANG language file
#else
#define TEXT_FILEDESC               Windows Directory Statistics visualizes disk space usage // component description
#endif
#if defined(MODNAME)
#   define TEXT_MODULE              MODNAME
#   if (WDS_RESLANG)
#       define TEXT_INTERNALNAME    MODNAME.wdslng
#   else
#       define TEXT_INTERNALNAME    MODNAME.exe
#   endif
#else
#   error You must define MODNAME in the project!
#endif

#define TEXT_COMPANY                WinDirStat Team (windirstat.net) // company
#define TEXT_COPYRIGHT              \xA9 2003-2005 Bernhard Seifert, \xA9 2004-2024 WinDirStat Team // copyright information
#define HG_REPOSITORY               "https://bitbucket.org/windirstat/windirstat"
#define STRING_REPORT_DEFECT_URL    HG_REPOSITORY "/issues?status=new&status=open"
