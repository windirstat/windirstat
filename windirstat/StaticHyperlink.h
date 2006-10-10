// StaticHyperlink.h
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
// $Id$

#pragma once
#include "afxwin.h"


// CStaticHyperlink

class CStaticHyperlink : public CStatic
{
	DECLARE_DYNAMIC(CStaticHyperlink)

public:
	CStaticHyperlink();
	virtual ~CStaticHyperlink();

protected:
	DECLARE_MESSAGE_MAP()
	virtual void PreSubclassWindow();
	bool m_bHovering;
	bool m_bShowTooltip;
	COLORREF m_clHovering;
	COLORREF m_clNormal;
	CFont m_fntHovering;
	CFont m_fntNormal;
	HCURSOR m_curHovering;
	CString m_url;
public:
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg HBRUSH CtlColor(CDC* pDC, UINT nCtlColor);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnStnClicked();
	void SetUrl(const CString &url);
	void SetText(const CString &text);
	CString GetUrl();
	CString GetText();
	void SetLinkColor(const COLORREF color);
	void SetHoverColor(const COLORREF color);
	COLORREF GetLinkColor();
	COLORREF GetHoverColor();
};

// $Log$
// Revision 1.5  2006/10/10 01:41:50  assarbad
// - Added credits for Gerben Wieringa (Dutch translation)
// - Replaced Header tag by Id for the CVS tags in the source files ...
// - Started re-ordering of the files inside the project(s)/solution(s)
//
// Revision 1.4  2006/07/04 23:37:39  assarbad
// - Added my email address in the header, adjusted "Author" -> "Author(s)"
// - Added CVS Log keyword to those files not having it
// - Added the files which I forgot during last commit
//
