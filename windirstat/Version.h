// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#ifndef GIT_COMMIT
#define GIT_COMMIT "deadbeef"
#endif

#ifndef GIT_DATE
#define GIT_DATE "0000-00-00"
#endif

#ifndef GIT_COUNT
#define GIT_COUNT 0
#endif

#ifndef PRODUCTION
#define PRODUCTION 0
#endif

#define PRD_MAJVER                  2 // major product version
#define PRD_MINVER                  9 // minor product version
#define PRD_PATCH                   2 // patch number for product
#define PRD_BUILD                   GIT_COUNT // build number for product
#define FILE_MAJVER                 PRD_MAJVER // major file version
#define FILE_MINVER                 PRD_MINVER // minor file version
#define FILE_PATCH                  PRD_PATCH // patch number for version
#define FILE_BUILD                  PRD_BUILD // build number for version
#define TEXT_WEBSITE                https:/##/windirstat.net // website
#define TEXT_PRODUCTNAME            WinDirStat // product's name
#define TEXT_FILEDESC               Windows Directory Statistics (WinDirStat) // component description

#define STRING_COMPANY              WinDirStat Team
#define STRING_COPYRIGHT            "© WinDirStat Team"
#define STRING_EXENAME              WinDirStat.exe
#define SOURCE_REPOSITORY           https://github.com/windirstat/windirstat
