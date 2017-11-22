// wds_constants.h
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2017 WinDirStat Team (windirstat.net)
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

#ifndef __WDS_CONSTANTS_H_VER__
#define __WDS_CONSTANTS_H_VER__ 2017112218
#if (defined(_MSC_VER) && (_MSC_VER >= 1020)) || defined(__MCPP)
#pragma once
#endif // Check for "#pragma once" support

namespace wds
{
    // Single character constants
    const TCHAR chrSharp = _T('#');
    const TCHAR chrBracketOpen = _T('(');
    const TCHAR chrBracketClose = _T(')');
    const TCHAR chrComma = _T(',');
    const TCHAR chrDot = _T('.');
    const TCHAR chrColon = _T(':');
    const TCHAR chrAt = _T('\x40');
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
    const LPCTSTR strAttributeReadonly = _T("R"); /*FILE_ATTRIBUTE_READONLY*/
    const LPCTSTR strAttributeHidden = _T("H"); /*FILE_ATTRIBUTE_HIDDEN*/
    const LPCTSTR strAttributeSystem = _T("S"); /*FILE_ATTRIBUTE_SYSTEM*/
    /* don't need FILE_ATTRIBUTE_DIRECTORY, directories are visualized in other ways */
    const LPCTSTR strAttributeArchive = _T("A"); /*FILE_ATTRIBUTE_ARCHIVE*/
    /* don't need FILE_ATTRIBUTE_DEVICE */
    /* don't need FILE_ATTRIBUTE_NORMAL */
    const LPCTSTR strAttributeTemporary = _T("T"); /*FILE_ATTRIBUTE_TEMPORARY*/
    const LPCTSTR strAttributeSparse = _T("~"); /*FILE_ATTRIBUTE_SPARSE_FILE*/
    const LPCTSTR strAttributeReparsePoint = _T("@"); /*FILE_ATTRIBUTE_REPARSE_POINT*/
    const LPCTSTR strAttributeCompressed = _T("C"); /*FILE_ATTRIBUTE_COMPRESSED*/
    const LPCTSTR strAttributeOffline = _T("_"); /*FILE_ATTRIBUTE_OFFLINE*/
    const LPCTSTR strAttributeNotContentIndexed = _T("i"); /*FILE_ATTRIBUTE_NOT_CONTENT_INDEXED*/
    const LPCTSTR strAttributeEncrypted = _T("E"); /*FILE_ATTRIBUTE_ENCRYPTED*/
    const LPCTSTR strAttributeIntegrityStream = _T("I"); /*FILE_ATTRIBUTE_INTEGRITY_STREAM*/
    const LPCTSTR strAttributeVirtual = _T("V"); /*FILE_ATTRIBUTE_VIRTUAL*/
    /* don't need FILE_ATTRIBUTE_NO_SCRUB_DATA */
    const LPCTSTR strAttributeEA = _T("+"); /*FILE_ATTRIBUTE_EA*/

    const int iLangCodeLength = _countof(STR_RESOURCE_PREFIX);
    const int iNumDriveLetters = (chrCapZ - chrCapA) + 1;
}

#endif // __WDS_CONSTANTS_H_VER__
