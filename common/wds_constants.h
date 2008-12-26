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
//            - assarbad -> oliver@windirstat.info
//
// $Id$

#ifndef __WDS_CONSTANTS_H__
#define __WDS_CONSTANTS_H__
#pragma once

// Single character constants
const TCHAR chrSharp = TEXT('#');
const TCHAR chrBracketOpen = TEXT('(');
const TCHAR chrBracketClose = TEXT(')');
const TCHAR chrComma = TEXT(',');
const TCHAR chrDot = TEXT('.');
const TCHAR chrColon = TEXT(':');
const TCHAR chrAt = TEXT('@');
const TCHAR chrBackslash = TEXT('\\');
const TCHAR chrPipe = TEXT('|');
const TCHAR chrZero = TEXT('0');
const TCHAR chrSmallA = TEXT('a');
const TCHAR chrCapA = TEXT('A');
const TCHAR chrCapB = TEXT('B');
const TCHAR chrCapC = TEXT('C');

// Constants strings
const LPCTSTR strEmpty = TEXT("");
const LPCTSTR strBlankSpace = TEXT(" ");
const LPCTSTR strStar = TEXT("*");
const LPCTSTR strComma = TEXT(",");
const LPCTSTR strDot = TEXT(".");
const LPCTSTR strBackslash = TEXT("\\");
const LPCTSTR strPipe = TEXT("|");
const LPCTSTR strDollar = TEXT("$");
const LPCTSTR strPercent = TEXT("%");

const LPCTSTR strExplorerKey = TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer");

#endif // __WDS_CONSTANTS_H__
