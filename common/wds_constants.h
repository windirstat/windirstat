// wds_constants.h
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2006 Oliver Schneider (assarbad.net)
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
// Author(s): - bseifert -> bseifert@users.sourceforge.net, bseifert@daccord.net
//            - assarbad -> http://assarbad.net/en/contact
//
// $Header$

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

// $Log$
// Revision 1.2  2006/07/04 23:40:39  assarbad
// - Added file header to wds_constants.h
//
// Revision 1.1  2006/07/04 23:37:39  assarbad
// - Added my email address in the header, adjusted "Author" -> "Author(s)"
// - Added CVS Log keyword to those files not having it
// - Added the files which I forgot during last commit
//
