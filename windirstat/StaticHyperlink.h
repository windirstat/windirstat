// StaticHyperlink.h
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
//            - assarbad -> http://windirstat.info/contact/oliver/
//
// $Id$

#ifndef __WDS_STATICHYPERLINK_H__
#define __WDS_STATICHYPERLINK_H__
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

#endif // __WDS_STATICHYPERLINK_H__
