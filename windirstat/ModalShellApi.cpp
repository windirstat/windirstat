// ModalShellApi.cpp - Implementation of CModalShellApi
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

#include "stdafx.h"
#include "windirstat.h"
#include "ModalShellApi.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	enum
	{
		EMPTY_RECYCLE_BIN,
		DELETE_FILE
	};
}


CModalShellApi::CModalShellApi()
{
}

bool CModalShellApi::IsRecycleBinApiSupported()
{
	return m_rbapi.IsSupported();
}

void CModalShellApi::EmptyRecycleBin()
{
	m_operation = EMPTY_RECYCLE_BIN;

	DoModal();
}

void CModalShellApi::DeleteFile(LPCTSTR fileName, bool toRecycleBin)
{
	m_operation = DELETE_FILE;
	m_fileName = fileName;
	m_toRecycleBin = toRecycleBin;
	
	DoModal();
}

void CModalShellApi::DoOperation()
{
	switch (m_operation)
	{
	case EMPTY_RECYCLE_BIN:
		{
			DoEmptyRecycleBin();
		}
		break;

	case DELETE_FILE:
		{
			DoDeleteFile();
		}
		break;
	}
}

void CModalShellApi::DoEmptyRecycleBin()
{
	HRESULT hr = m_rbapi.SHEmptyRecycleBin(*AfxGetMainWnd(), NULL, 0);
	if(FAILED(hr))
	{
		AfxMessageBox(MdGetWinErrorText(hr));
	}
}

void CModalShellApi::DoDeleteFile()
{
	int len = m_fileName.GetLength();
	LPTSTR psz = m_fileName.GetBuffer(len + 2);
	psz[len + 1]= 0;

	SHFILEOPSTRUCT sfos;
	ZeroMemory(&sfos, sizeof(sfos));
	sfos.wFunc = FO_DELETE;
	sfos.pFrom = psz;
	sfos.fFlags = m_toRecycleBin ? FOF_ALLOWUNDO : 0;

	sfos.hwnd = *AfxGetMainWnd();
	
	(void)SHFileOperation(&sfos);

	m_fileName.ReleaseBuffer();
}

// $Log$
// Revision 1.8  2006/07/04 23:37:39  assarbad
// - Added my email address in the header, adjusted "Author" -> "Author(s)"
// - Added CVS Log keyword to those files not having it
// - Added the files which I forgot during last commit
//
// Revision 1.7  2006/07/04 22:49:20  assarbad
// - Replaced CVS keyword "Date" by "Header" in the file headers
//
// Revision 1.6  2006/07/04 20:45:22  assarbad
// - See changelog for the changes of todays previous check-ins as well as this one!
//
// Revision 1.5  2004/11/13 08:17:07  bseifert
// Remove blanks in Unicode Configuration names.
//
// Revision 1.4  2004/11/12 22:14:16  bseifert
// Eliminated CLR_NONE. Minor corrections.
//
// Revision 1.3  2004/11/05 16:53:06  assarbad
// Added Date and History tag where appropriate.
//
