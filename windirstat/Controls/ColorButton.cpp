// colorbutton.cpp - Implementation of CColorButton
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
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

#include "stdafx.h"
#include "colorbutton.h"
#include <common/Constants.h>

/////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CColorButton::CPreview, CWnd)
    ON_WM_PAINT()
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

CColorButton::CPreview::CPreview()
{
    m_color = 0;
}

COLORREF CColorButton::CPreview::GetColor()
{
    return m_color;
}

void CColorButton::CPreview::SetColor(COLORREF color)
{
    m_color = color;
    if (::IsWindow(m_hWnd))
    {
        InvalidateRect(NULL);
    }
}

void CColorButton::CPreview::OnPaint()
{
    CPaintDC dc(this);

    CRect rc;
    GetClientRect(rc);

    dc.DrawEdge(rc, EDGE_BUMP, BF_RECT | BF_ADJUST);

    COLORREF color = m_color;
    if ((GetParent()->GetStyle() & WS_DISABLED) != 0)
    {
        color = ::GetSysColor(COLOR_BTNFACE);
    }
    dc.FillSolidRect(rc, color);
}

void CColorButton::CPreview::OnLButtonDown(UINT nFlags, CPoint point)
{
    ClientToScreen(&point);
    GetParent()->ScreenToClient(&point);
    GetParent()->SendMessage(WM_LBUTTONDOWN, nFlags, MAKELPARAM(point.x, point.y));
}


/////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CColorButton, CButton)
    ON_WM_PAINT()
    ON_WM_DESTROY()
    ON_CONTROL_REFLECT(BN_CLICKED, OnBnClicked)
    ON_WM_ENABLE()
END_MESSAGE_MAP()

COLORREF CColorButton::GetColor()
{
    return m_preview.GetColor();
}

void CColorButton::SetColor(COLORREF color)
{
    m_preview.SetColor(color);
}

void CColorButton::OnPaint()
{
    if (NULL == m_preview.m_hWnd)
    {
        CRect rc;
        GetClientRect(rc);

        rc.right = rc.left + rc.Width() / 3;
        rc.DeflateRect(4, 4);

        VERIFY(m_preview.Create(AfxRegisterWndClass(0, 0, 0, 0), wds::strEmpty, WS_CHILD | WS_VISIBLE, rc, this, 4711));

        ModifyStyle(0, WS_CLIPCHILDREN);
    }
    CButton::OnPaint();
}

void CColorButton::OnDestroy()
{
    if (::IsWindow(m_preview.m_hWnd))
    {
        m_preview.DestroyWindow();
    }
    CButton::OnDestroy();
}

void CColorButton::OnBnClicked()
{
    CColorDialog dlg(GetColor());
    if (IDOK == dlg.DoModal())
    {
        SetColor(dlg.GetColor());
        NMHDR hdr;
        hdr.hwndFrom = m_hWnd;
        hdr.idFrom   = GetDlgCtrlID();
        hdr.code     = COLBN_CHANGED;

        GetParent()->SendMessage(WM_NOTIFY, GetDlgCtrlID(), (LPARAM)&hdr);
    }
}


void CColorButton::OnEnable(BOOL bEnable)
{
    if (::IsWindow(m_preview.m_hWnd))
    {
        m_preview.InvalidateRect(NULL);
    }
    CButton::OnEnable(bEnable);
}
