// modalapishuttle.h	- Declaration of CModalApiShuttle
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2004 Bernhard Seifert
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

#pragma once


//
// CModalApiShuttle. (Base class for CModalShellApi and CModalSendMail.)
//
// Both the SHFileOperation() and MAPISendMail() functions show modeless dialogs,
// but we want them to be modal.
//
// My first approximation was:
//
// AfxGetMainWnd()->EnableWindow(false);
// Do the operation (SHFileOperation respectively MAPISendMail)
// AfxGetMainWnd()->EnableWindow(true);
//
// But when the operation window is destroyed, the system brings
// some other window to the foreground and WinDirStat ends up in the background.
// That's because it is still disabled at that moment.
//
// So my solution is this:
// First create an invisible (zero size) (but enabled) modal dialog,
// then do the operation in its OnInitDialog function
// and end the dialog.
//
class CModalApiShuttle: public CDialog
{
	DECLARE_DYNAMIC(CModalApiShuttle)

public:
	CModalApiShuttle(CWnd* pParent = NULL);
	virtual ~CModalApiShuttle();

protected:
	enum { IDD = IDD_MODALAPISHUTTLE };
	virtual BOOL OnInitDialog();
	DECLARE_MESSAGE_MAP()

	virtual void DoOperation() =0;
};

// $Log$
// Revision 1.3  2004/11/05 16:53:06  assarbad
// Added Date and History tag where appropriate.
//
