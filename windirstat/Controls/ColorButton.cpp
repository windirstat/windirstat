// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "resource.h"
#include "ColorButton.h"

/////////////////////////////////////////////////////////////////////////////

void CColorButton::CPreview::SetColor(const COLORREF color)
{
    m_color = color;
    if (m_hWnd != nullptr)
    {
        Invalidate();
    }
}

void CColorButton::CPreview::OnPaint()
{
    CPaintDC dc(this);

    CRect rc = GetClientRect();
    dc.DrawEdge(rc, EDGE_BUMP, BF_RECT | BF_ADJUST);

    const bool disabled = (GetParent().GetStyle() & WS_DISABLED) != 0;
    dc.FillSolidRect(rc, disabled ? DarkMode::SystemColor(COLOR_BTNFACE) : m_color);
}

void CColorButton::CPreview::OnLButtonDown(const UINT nFlags, CPoint point) const
{
    point = GetParent().ToClient(ToScreen(point));
    GetParent().SendMessage(WM_LBUTTONDOWN, nFlags, MAKELPARAM(point.x, point.y));
}

/////////////////////////////////////////////////////////////////////////////

void CColorButton::OnPaint()
{
    if (m_preview == nullptr)
    {
        CRect rc = GetClientRect();
        rc.right = rc.left + rc.Width() / 3;
        rc.Deflate(4, 4);

        [[maybe_unused]] const bool created = m_preview.Create(RegisterWindowClass(0), wds::strEmpty,
            WS_CHILD | WS_VISIBLE, rc, this, ID_WDS_CONTROL);
        assert(created);

        ModifyStyle(0, WS_CLIPCHILDREN);
        SetTextOffset({ -4, 0 });
    }
    CButton::OnPaint();
}

void CColorButton::OnDestroy()
{
    if (m_preview != nullptr)
    {
        m_preview.DestroyWindow();
    }
    CButton::OnDestroy();
}

void CColorButton::OnBnClicked()
{
    if (const auto color = CDialog::PickColor(GetColor()))
    {
        SetColor(*color);
        NMHDR hdr{
            .hwndFrom = m_hWnd,
            .idFrom   = static_cast<UINT_PTR>(GetDlgCtrlID()),
            .code     = COLBN_CHANGED
        };

        GetParent().SendMessage(WM_NOTIFY, GetDlgCtrlID(), &hdr);
    }
}

void CColorButton::OnEnable(const bool bEnable)
{
    if (m_preview != nullptr)
    {
        m_preview.Invalidate();
    }
    CButton::OnEnable(bEnable);
}
