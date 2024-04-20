// Constants.h
//
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

#include <string>

namespace wds
{
    // Single character constants
    constexpr WCHAR chrSharp        = L'#';
    constexpr WCHAR chrBracketOpen  = L'(';
    constexpr WCHAR chrBracketClose = L')';
    constexpr WCHAR chrComma        = L',';
    constexpr WCHAR chrDot          = L'.';
    constexpr WCHAR chrColon        = L':';
    constexpr WCHAR chrAt           = L'\x40';
    constexpr WCHAR chrBackslash    = L'\\';
    constexpr WCHAR chrPipe         = L'|';
    constexpr WCHAR chrZero         = L'0';
    constexpr WCHAR chrSmallA       = L'a';
    constexpr WCHAR chrCapA         = L'A';
    constexpr WCHAR chrCapB         = L'B';
    constexpr WCHAR chrCapC         = L'C';
    constexpr WCHAR chrCapZ         = L'Z';

    // Constants strings
    inline auto strEmpty      = L"";
    inline auto strBlankSpace = L' ';
    inline auto strStar       = L'*';
    inline auto strComma      = L',';
    inline auto strDot        = L'.';
    inline auto strBackslash  = L'\\';
    inline auto strPipe       = L'|';
    inline auto strDollar     = L'$';
    inline auto strPercent    = L'%';

    inline auto strExplorerKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer";

    inline auto strInvalidAttributes          = L"??????";
    inline auto strAttributeReadonly          = L"R"; /*FILE_ATTRIBUTE_READONLY*/
    inline auto strAttributeHidden            = L"H"; /*FILE_ATTRIBUTE_HIDDEN*/
    inline auto strAttributeSystem            = L"S"; /*FILE_ATTRIBUTE_SYSTEM*/
    inline auto strAttributeArchive           = L"A"; /*FILE_ATTRIBUTE_ARCHIVE*/
    inline auto strAttributeTemporary         = L"T"; /*FILE_ATTRIBUTE_TEMPORARY*/
    inline auto strAttributeSparse            = L"~"; /*FILE_ATTRIBUTE_SPARSE_FILE*/
    inline auto strAttributeReparsePoint      = L"@"; /*FILE_ATTRIBUTE_REPARSE_POINT*/
    inline auto strAttributeCompressed        = L"C"; /*FILE_ATTRIBUTE_COMPRESSED*/
    inline auto strAttributeOffline           = L"_"; /*FILE_ATTRIBUTE_OFFLINE*/
    inline auto strAttributeNotContentIndexed = L"i"; /*FILE_ATTRIBUTE_NOT_CONTENT_INDEXED*/
    inline auto strAttributeEncrypted         = L"E"; /*FILE_ATTRIBUTE_ENCRYPTED*/
    inline auto strAttributeIntegrityStream   = L"I"; /*FILE_ATTRIBUTE_INTEGRITY_STREAM*/
    inline auto strAttributeVirtual           = L"V"; /*FILE_ATTRIBUTE_VIRTUAL*/
    inline auto strAttributeEA                = L"+"; /*FILE_ATTRIBUTE_EA*/

    const std::wstring albet{ L"ABCDEFGHIJKLMNOPQRSTUVWXYZ" };
    constexpr int iNumDriveLetters = chrCapZ - chrCapA + 1;
}
