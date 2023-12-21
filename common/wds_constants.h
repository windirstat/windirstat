// wds_constants.h
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

namespace wds
{
    // Single character constants
    constexpr WCHAR chrSharp = L'#';
    constexpr WCHAR chrBracketOpen = L'(';
    constexpr WCHAR chrBracketClose = L')';
    constexpr WCHAR chrComma = L',';
    constexpr WCHAR chrDot = L'.';
    constexpr WCHAR chrColon = L':';
    constexpr WCHAR chrAt = L'\x40';
    constexpr WCHAR chrBackslash = L'\\';
    constexpr WCHAR chrPipe = L'|';
    constexpr WCHAR chrZero = L'0';
    constexpr WCHAR chrSmallA = L'a';
    constexpr WCHAR chrCapA = L'A';
    constexpr WCHAR chrCapB = L'B';
    constexpr WCHAR chrCapC = L'C';
    constexpr WCHAR chrCapZ = L'Z';

    // Constants strings
    const LPCWSTR strEmpty = L"";
    const LPCWSTR strBlankSpace = L" ";
    const LPCWSTR strStar = L"*";
    const LPCWSTR strComma = L",";
    const LPCWSTR strDot = L".";
    const LPCWSTR strBackslash = L"\\";
    const LPCWSTR strPipe = L"|";
    const LPCWSTR strDollar = L"$";
    const LPCWSTR strPercent = L"%";

    const LPCWSTR strExplorerKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer";

#define STR_LANG_SUFFIX L".wdslng"
    const LPCWSTR strLangSuffix = STR_LANG_SUFFIX;

#   define STR_RESOURCE_PREFIX L"wdsr"
    const LPCWSTR strLangPrefix = STR_RESOURCE_PREFIX;

    const LPCWSTR strInvalidAttributes = L"??????";
    const LPCWSTR strAttributeReadonly = L"R"; /*FILE_ATTRIBUTE_READONLY*/
    const LPCWSTR strAttributeHidden = L"H"; /*FILE_ATTRIBUTE_HIDDEN*/
    const LPCWSTR strAttributeSystem = L"S"; /*FILE_ATTRIBUTE_SYSTEM*/
    /* don't need FILE_ATTRIBUTE_DIRECTORY, directories are visualized in other ways */
    const LPCWSTR strAttributeArchive = L"A"; /*FILE_ATTRIBUTE_ARCHIVE*/
    /* don't need FILE_ATTRIBUTE_DEVICE */
    /* don't need FILE_ATTRIBUTE_NORMAL */
    const LPCWSTR strAttributeTemporary = L"T"; /*FILE_ATTRIBUTE_TEMPORARY*/
    const LPCWSTR strAttributeSparse = L"~"; /*FILE_ATTRIBUTE_SPARSE_FILE*/
    const LPCWSTR strAttributeReparsePoint = L"@"; /*FILE_ATTRIBUTE_REPARSE_POINT*/
    const LPCWSTR strAttributeCompressed = L"C"; /*FILE_ATTRIBUTE_COMPRESSED*/
    const LPCWSTR strAttributeOffline = L"_"; /*FILE_ATTRIBUTE_OFFLINE*/
    const LPCWSTR strAttributeNotContentIndexed = L"i"; /*FILE_ATTRIBUTE_NOT_CONTENT_INDEXED*/
    const LPCWSTR strAttributeEncrypted = L"E"; /*FILE_ATTRIBUTE_ENCRYPTED*/
    const LPCWSTR strAttributeIntegrityStream = L"I"; /*FILE_ATTRIBUTE_INTEGRITY_STREAM*/
    const LPCWSTR strAttributeVirtual = L"V"; /*FILE_ATTRIBUTE_VIRTUAL*/
    /* don't need FILE_ATTRIBUTE_NO_SCRUB_DATA */
    const LPCWSTR strAttributeEA = L"+"; /*FILE_ATTRIBUTE_EA*/

    constexpr int iLangCodeLength = _countof(STR_RESOURCE_PREFIX) - 1;
    constexpr int iNumDriveLetters = chrCapZ - chrCapA + 1;
}
