// FileFindWDS.cpp - Implementation of CFileFindWDS
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

#include "StdAfx.h"
#include "FileFindWDS.h"
#include "windirstat.h"

CFileFindWDS::CFileFindWDS(void)
{
}

CFileFindWDS::~CFileFindWDS(void)
{
}

// Function to access the file attributes from outside
DWORD CFileFindWDS::GetAttributes() const
{
	ASSERT(m_hContext != NULL);
	ASSERT_VALID(this);

	if(m_pFoundInfo != NULL)
	{
		return ((LPWIN32_FIND_DATA)m_pFoundInfo)->dwFileAttributes;
	}
	else
	{
		return INVALID_FILE_ATTRIBUTES;
	}
}

// Wrapper for file size retrieval
// This function tries to return compressed file size whenever possible.
// If the file is not compressed the uncompressed size is being returned.
ULONGLONG CFileFindWDS::GetCompressedLength() const
{
	// Try to use the NT-specific API
	if(GetApp()->GetComprSizeApi()->IsSupported())
	{
		ULARGE_INTEGER ret;
		ret.LowPart = GetApp()->GetComprSizeApi()->GetCompressedFileSize(GetFilePath(), &ret.HighPart);
		
		// Check for error
		if((GetLastError() != ERROR_SUCCESS) && (ret.LowPart == INVALID_FILE_SIZE))
		{
			// In case of an error return size from CFileFind object
			return GetLength();
		}
		else
		{
			return ret.QuadPart;
		}
	}
	else
	{
		// Use the file size already found by the finder object
		return GetLength();
	}
}

// $Log$
// Revision 1.6  2006/07/04 23:37:39  assarbad
// - Added my email address in the header, adjusted "Author" -> "Author(s)"
// - Added CVS Log keyword to those files not having it
// - Added the files which I forgot during last commit
//
// Revision 1.5  2006/07/04 22:49:20  assarbad
// - Replaced CVS keyword "Date" by "Header" in the file headers
//
// Revision 1.4  2006/07/04 20:45:22  assarbad
// - See changelog for the changes of todays previous check-ins as well as this one!
//
// Revision 1.3  2004/11/29 07:07:47  bseifert
// Introduced SRECT. Saves 8 Bytes in sizeof(CItem). Formatting changes.
//
// Revision 1.2  2004/11/28 14:40:06  assarbad
// - Extended CFileFindWDS to replace a global function
// - Now packing/unpacking the file attributes. This even spares a call to find encrypted/compressed files.
//
// Revision 1.1  2004/11/25 23:07:24  assarbad
// - Derived CFileFindWDS from CFileFind to correct a problem of the ANSI version
//
