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
    inline constexpr auto chrBracketOpen  = L'(';
    inline constexpr auto chrBracketClose = L')';
    inline constexpr auto chrDot          = L'.';
    inline constexpr auto chrColon        = L':';
    inline constexpr auto chrBackslash    = L'\\';
    inline constexpr auto chrPipe         = L'|';
    inline constexpr auto chrNull         = L'\0';

    // Constants strings
    inline constexpr auto strEmpty      = L"";
    inline constexpr auto strBlankSpace = L' ';
    inline constexpr auto strStar       = L'*';
    inline constexpr auto strDot        = L'.';
    inline constexpr auto strBackslash  = L'\\';
    inline constexpr auto strPipe       = L'|';
    inline constexpr auto strPercent    = L'%';

    inline constexpr auto strExplorerKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer";

    inline constexpr auto strInvalidAttributes     = L"??????";
    inline constexpr auto strAttributeReadonly     = L"R"; /*FILE_ATTRIBUTE_READONLY*/
    inline constexpr auto strAttributeHidden       = L"H"; /*FILE_ATTRIBUTE_HIDDEN*/
    inline constexpr auto strAttributeSystem       = L"S"; /*FILE_ATTRIBUTE_SYSTEM*/
    inline constexpr auto strAttributeArchive      = L"A"; /*FILE_ATTRIBUTE_ARCHIVE*/
    inline constexpr auto strAttributeReparsePoint = L"@"; /*FILE_ATTRIBUTE_REPARSE_POINT*/
    inline constexpr auto strAttributeCompressed   = L"C"; /*FILE_ATTRIBUTE_COMPRESSED*/
    inline constexpr auto strAttributeOffline      = L"O"; /*FILE_ATTRIBUTE_OFFLINE*/
    inline constexpr auto strAttributeEncrypted    = L"E"; /*FILE_ATTRIBUTE_ENCRYPTED*/

    inline std::wstring strAlpha{ L"ABCDEFGHIJKLMNOPQRSTUVWXYZ" };
}
