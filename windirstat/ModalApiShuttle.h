// modalapishuttle.h - Declaration of CModalApiShuttle
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

#ifndef __WDS_MODALAPISHUTTLE_H__
#define __WDS_MODALAPISHUTTLE_H__
#pragma once


//
// CModalApiShuttle. (Base class for CModalShellApi and CModalSendMail.)
//
// The SHFileOperation() function shows a modeless dialog, but we want
// them to be modal.
//
// My first approximation was:
//
// AfxGetMainWnd()->EnableWindow(false);
// Do the operation (SHFileOperation)
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

#endif // __WDS_MODALAPISHUTTLE_H__
