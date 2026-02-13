// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "pch.h"
#include "resource.h"
#include "ColorButton.h"

/////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CColorButton::CPreview, CWnd)
    ON_WM_PAINT()
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

CColorButton::CPreview::CPreview() : m_color(0)
{
}

COLORREF CColorButton::CPreview::GetColor() const
{
    return m_color;
}

void CColorButton::CPreview::SetColor(const COLORREF color)
{
    m_color = color;
    if (IsWindow(m_hWnd))
    {
        InvalidateRect(nullptr);
    }
}

void CColorButton::CPreview::OnPaint()
{
    CPaintDC dc(this);

    CRect rc;
    GetClientRect(rc);

    dc.DrawEdge(rc, EDGE_BUMP, BF_RECT | BF_ADJUST);

    const bool disabled = (GetParent()->GetStyle() & WS_DISABLED) != 0;
    dc.FillSolidRect(rc, disabled ? DarkMode::WdsSysColor(COLOR_BTNFACE) : m_color);
}

void CColorButton::CPreview::OnLButtonDown(const UINT nFlags, CPoint point)
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

COLORREF CColorButton::GetColor() const
{
    return m_preview.GetColor();
}

void CColorButton::SetColor(const COLORREF color)
{
    m_preview.SetColor(color);
}

void CColorButton::OnPaint()
{
    if (nullptr == m_preview.m_hWnd)
    {
        CRect rc;
        GetClientRect(rc);

        rc.right = rc.left + rc.Width() / 3;
        rc.DeflateRect(4, 4);

        VERIFY(m_preview.Create(AfxRegisterWndClass(0, nullptr, nullptr, nullptr), wds::strEmpty, WS_CHILD | WS_VISIBLE, rc, this, ID_WDS_CONTROL));

        ModifyStyle(0, WS_CLIPCHILDREN);
    }
    CButton::OnPaint();
}

void CColorButton::OnDestroy()
{
    if (IsWindow(m_preview.m_hWnd))
    {
        m_preview.DestroyWindow();
    }
    CButton::OnDestroy();
}

void CColorButton::OnBnClicked()
{
    if (CMFCColorDialog dlg(GetColor()); IDOK == dlg.DoModal())
    {
        SetColor(dlg.GetColor());
        NMHDR hdr;
        hdr.hwndFrom = m_hWnd;
        hdr.idFrom   = GetDlgCtrlID();
        hdr.code     = COLBN_CHANGED;

        GetParent()->SendMessage(WM_NOTIFY, GetDlgCtrlID(), reinterpret_cast<LPARAM>(&hdr));
    }
}

void CColorButton::OnEnable(const BOOL bEnable)
{
    if (IsWindow(m_preview.m_hWnd))
    {
        m_preview.InvalidateRect(nullptr);
    }
    CButton::OnEnable(bEnable);
}
