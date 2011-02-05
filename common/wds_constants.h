// wds_constants.h
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

#ifndef __WDS_CONSTANTS_H__
#define __WDS_CONSTANTS_H__
#pragma once

namespace wds
{
    // Single character constants
    const TCHAR chrSharp = _T('#');
    const TCHAR chrBracketOpen = _T('(');
    const TCHAR chrBracketClose = _T(')');
    const TCHAR chrComma = _T(',');
    const TCHAR chrDot = _T('.');
    const TCHAR chrColon = _T(':');
    const TCHAR chrAt = _T('@');
    const TCHAR chrBackslash = _T('\\');
    const TCHAR chrPipe = _T('|');
    const TCHAR chrZero = _T('0');
    const TCHAR chrSmallA = _T('a');
    const TCHAR chrCapA = _T('A');
    const TCHAR chrCapB = _T('B');
    const TCHAR chrCapC = _T('C');
    const TCHAR chrCapZ = _T('Z');

    // Constants strings
    const LPCTSTR strEmpty = _T("");
    const LPCTSTR strBlankSpace = _T(" ");
    const LPCTSTR strStar = _T("*");
    const LPCTSTR strComma = _T(",");
    const LPCTSTR strDot = _T(".");
    const LPCTSTR strBackslash = _T("\\");
    const LPCTSTR strPipe = _T("|");
    const LPCTSTR strDollar = _T("$");
    const LPCTSTR strPercent = _T("%");

    const LPCTSTR strExplorerKey = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer");

#define STR_LANG_SUFFIX ".wdslng"
    const LPCTSTR strLangSuffix = _T(STR_LANG_SUFFIX);

#   define STR_RESOURCE_PREFIX "wdsr"
    const LPCTSTR strLangPrefix = _T(STR_RESOURCE_PREFIX);

    const LPCTSTR strInvalidAttributes = _T("??????");
    const LPCTSTR strAttributeReadonly = _T("R");
    const LPCTSTR strAttributeHidden = _T("H");
    const LPCTSTR strAttributeSystem = _T("S");
    const LPCTSTR strAttributeArchive = _T("A");
    const LPCTSTR strAttributeCompressed = _T("R");
    const LPCTSTR strAttributeEncrypted = _T("E");

    const int iLangCodeLength = 4;
    const int iNumDriveLetters = (chrCapZ - chrCapA) + 1;
}

#endif // __WDS_CONSTANTS_H__
