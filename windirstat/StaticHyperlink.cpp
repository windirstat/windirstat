// StaticHyperlink.cpp - implementation file
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

#include "stdafx.h"
#include "windirstat.h"
#include "StaticHyperlink.h"

// CStaticHyperlink

IMPLEMENT_DYNAMIC(CStaticHyperlink, CStatic)
CStaticHyperlink::CStaticHyperlink()
    : m_bHovering(false)
    , m_bShowTooltip(false)
    , m_clHovering(RGB(0, 0, 0xFF))
    , m_clNormal(RGB(0, 0, 0x80))
    , m_curHovering(0)
{
}

CStaticHyperlink::~CStaticHyperlink()
{
}

BEGIN_MESSAGE_MAP(CStaticHyperlink, CStatic)
    ON_WM_CTLCOLOR_REFLECT()
    ON_WM_MOUSEMOVE()
    ON_WM_SETCURSOR()
    ON_CONTROL_REFLECT(STN_CLICKED, OnStnClicked)
END_MESSAGE_MAP()

// CStaticHyperlink message handlers

void CStaticHyperlink::PreSubclassWindow()
{
    ModifyStyle(0, SS_NOTIFY, 0);
//   SetWindowLong(GetSafeHwnd(), GWL_STYLE, GetStyle() | SS_NOTIFY);

    LOGFONT currfnt;
    GetFont()->GetLogFont(&currfnt);
    m_fntNormal.CreateFontIndirect(&currfnt);
    currfnt.lfUnderline = true;
    m_fntHovering.CreateFontIndirect(&currfnt);
    SetFont(&m_fntNormal);

    m_curHovering = GetWDSApp()->LoadCursor(IDC_HANDCURSOR);
    if(m_curHovering)
    {
        SetCursor(m_curHovering);
    }

    CStatic::PreSubclassWindow();

    if(m_curHovering)
    {
        SetCursor(m_curHovering);
    }
}

HBRUSH CStaticHyperlink::CtlColor(CDC* pDC, UINT nCtlColor)
{
    UNREFERENCED_PARAMETER(nCtlColor);
    pDC->SetTextColor((m_bHovering) ? m_clHovering : m_clNormal);
    pDC->SetBkMode(TRANSPARENT);
    return HBRUSH(GetStockObject(NULL_BRUSH));
}

void CStaticHyperlink::OnMouseMove(UINT nFlags, CPoint point)
{
    CStatic::OnMouseMove(nFlags, point);

    CRect rect;
    GetClientRect(rect);
    if(m_bHovering)
    {
        if(!rect.PtInRect(point))
        {
            m_bHovering = false;
            ReleaseCapture();
            SetFont(&m_fntNormal, false);
            ShowWindow(SW_HIDE);
            ShowWindow(SW_SHOW);
            return;
        }
    }
    else
    {
        m_bHovering = true;
        SetFont(&m_fntHovering, false);
        ShowWindow(SW_HIDE);
        ShowWindow(SW_SHOW);
        SetCapture();
        return;
    }
}

BOOL CStaticHyperlink::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    if(m_curHovering)
    {
        ::SetCursor(m_curHovering);
        return true;
    }
    return CStatic::OnSetCursor(pWnd, nHitTest, message);
}

void CStaticHyperlink::SetUrl(const CString &url)
{
    m_url = url;
}

void CStaticHyperlink::SetText(const CString &text)
{
    CString olds;
    GetWindowText(olds);

    if(text != olds)
    {
        SetWindowText(text);
    }
}

CString CStaticHyperlink::GetUrl()
{
    return m_url;
}

CString CStaticHyperlink::GetText()
{
    CString text;
    GetWindowText(text);
    return text;
}

void CStaticHyperlink::SetLinkColor(const COLORREF color)
{
    if(m_clNormal != color)
    {
        m_clNormal = color;
        RedrawWindow();
    }
}

void CStaticHyperlink::SetHoverColor(const COLORREF color)
{
    if(m_clHovering != color)
    {
        m_clHovering = color;
        RedrawWindow();
    }
}

COLORREF CStaticHyperlink::GetLinkColor()
{
    return m_clNormal;
}

COLORREF CStaticHyperlink::GetHoverColor()
{
    return m_clHovering;
}

void CStaticHyperlink::OnStnClicked()
{
    if(!m_url.IsEmpty())
    {
        ::ShellExecute(GetSafeHwnd(), _T("open"), LPCTSTR(m_url), NULL, NULL, SW_SHOWNORMAL);
    }
}
