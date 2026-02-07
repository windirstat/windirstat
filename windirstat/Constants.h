// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

namespace wds
{
    // String and character constants
    inline constexpr auto chrDot          = L'.';
    inline constexpr auto chrDoubleQuote  = L'"';
    inline constexpr auto chrColon        = L':';
    inline constexpr auto chrBackslash    = L'\\';
    inline constexpr auto chrPipe         = L'|';
    inline constexpr auto chrNull         = L'\0';
    inline constexpr auto strEmpty        = L"";
    inline constexpr auto chrBlankSpace   = L' ';
    inline constexpr auto chrStar         = L'*';
    inline constexpr auto chrEqual        = L'=';
    inline constexpr auto szNpos          = std::wstring::npos;

    inline constexpr auto strExplorerKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer";
    inline constexpr auto strThemesKey   = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    inline constexpr auto strUninstall   = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WinDirStat";

    inline constexpr auto strInvalidAttributes     = L"??????";
    inline constexpr auto chrAttributeReadonly     = L'R'; /*FILE_ATTRIBUTE_READONLY*/
    inline constexpr auto chrAttributeHidden       = L'H'; /*FILE_ATTRIBUTE_HIDDEN*/
    inline constexpr auto chrAttributeSystem       = L'S'; /*FILE_ATTRIBUTE_SYSTEM*/
    inline constexpr auto chrAttributeArchive      = L'A'; /*FILE_ATTRIBUTE_ARCHIVE*/
    inline constexpr auto chrAttributeReparsePoint = L'@'; /*FILE_ATTRIBUTE_REPARSE_POINT*/
    inline constexpr auto chrAttributeCompressed   = L'C'; /*FILE_ATTRIBUTE_COMPRESSED*/
    inline constexpr auto chrAttributeOffline      = L'O'; /*FILE_ATTRIBUTE_OFFLINE*/
    inline constexpr auto chrAttributeEncrypted    = L'E'; /*FILE_ATTRIBUTE_ENCRYPTED*/
    inline constexpr auto chrAttributeSparse       = L'Z'; /*FILE_ATTRIBUTE_SPARSE*/

    inline constexpr auto strWinDirStat = L"WinDirStat";
    inline constexpr std::wstring_view strAlpha{ L"ABCDEFGHIJKLMNOPQRSTUVWXYZ" };
    inline constexpr int alphaSize = std::ssize(strAlpha);
}
