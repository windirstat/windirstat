// ModalApiShuttle.cpp - Implementation of CModalApiShuttle
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
// Author: bseifert@users.sourceforge.net, bseifert@daccord.net
//
// Last modified: $Date$

#include "stdafx.h"
#include "windirstat.h"
#include "ModalApiShuttle.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CModalApiShuttle, CDialog)

CModalApiShuttle::CModalApiShuttle(CWnd* pParent /*=NULL*/)
	: CDialog(CModalApiShuttle::IDD, pParent)
{
}

CModalApiShuttle::~CModalApiShuttle()
{
}

BEGIN_MESSAGE_MAP(CModalApiShuttle, CDialog)
END_MESSAGE_MAP()


BOOL CModalApiShuttle::OnInitDialog()
{
	CDialog::OnInitDialog();

	CRect rc;
	AfxGetMainWnd()->GetWindowRect(rc);
	rc.right = rc.left;
	rc.bottom = rc.top;

	MoveWindow(rc, false);

	EnableWindow(true);
	ShowWindow(SW_SHOW);

	DoOperation();

	EndDialog(IDOK);
	return TRUE;
}


// $Log$
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
