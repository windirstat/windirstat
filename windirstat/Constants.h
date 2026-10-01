// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

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

    // Binary size constants
    inline constexpr auto Ki = 1024ull;
    inline constexpr auto Mi = Ki * Ki;
    inline constexpr auto Gi = Mi * Ki;
    inline constexpr auto Ti = Gi * Ki;

    inline constexpr auto strAccessibilityKey = L"Software\\Microsoft\\Accessibility";
    inline constexpr auto strExplorerKey      = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer";
    inline constexpr auto strThemesKey        = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    inline constexpr auto strUninstall        = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WinDirStat";

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

    // Font name constants
    inline constexpr auto strFontSegoeUI = L"Segoe UI";
    inline constexpr auto strFontArial = L"Arial";
    inline constexpr auto strFontLucidaConsole = L"Lucida Console";
    inline constexpr auto strFontSegoeUISymbol = L"Segoe UI Symbol";
}
