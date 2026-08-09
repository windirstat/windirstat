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
#include "XYSlider.h"

void CXySlider::Initialize()
{
    if (!m_inited && IsWindow(m_hWnd))
    {
        // Make size odd, so that zero lines are central
        CRect rc = GetParent()->WindowRectInClient(Handle());
        if (rc.Width() % 2 == 0) rc.right--;
        if (rc.Height() % 2 == 0) rc.bottom--;
        MoveWindow(rc);

        // Initialize sizes
        m_rcAll = ClientRect();
        constexpr int s_gripperRadius = 8;

        m_zero.x = m_rcAll.Width() / 2;
        m_zero.y = m_rcAll.Height() / 2;
        m_radius.cx = m_rcAll.Width() / 2 - 1;
        m_radius.cy = m_rcAll.Height() / 2 - 1;

        m_rcInner = m_rcAll;
        m_rcInner.Deflate(s_gripperRadius - 3, s_gripperRadius - 3);
        m_gripperRadius.cx = s_gripperRadius;
        m_gripperRadius.cy = s_gripperRadius;
        m_range = m_radius - m_gripperRadius;

        m_inited = true;
    }
}

void CXySlider::GetRange(CSize& range) const
{
    range = m_externalRange;
}

void CXySlider::SetRange(const CSize & range)
{
    m_externalRange = range;
}

CPoint CXySlider::GetPos() const
{
    return m_externalPos;
}

LRESULT CXySlider::OnSetPos(WPARAM, const LPARAM lparam)
{
    const auto point = std::bit_cast<PPOINT>(lparam);
    SetPos(*point);
    return 0;
}

LRESULT CXySlider::OnGetPos(WPARAM, const LPARAM lparam) const
{
    const auto point = std::bit_cast<PPOINT>(lparam);
    *point = GetPos();
    return 0;
}

void CXySlider::SetPos(const CPoint pt)
{
    Initialize();

    m_externalPos = pt;
    ExternToIntern();
    Invalidate();
}

CRect CXySlider::GetGripperRect() const
{
    CRect rc(
        -m_gripperRadius.cx,
        -m_gripperRadius.cy,
        m_gripperRadius.cx + 1,
        m_gripperRadius.cy + 1
    );
    rc.Offset(m_zero);
    rc.Offset(m_pos);
    return rc;
}

void CXySlider::CheckMinMax(LONG& val, const int minVal, const int maxVal) const
{
    assert(minVal <= maxVal);

    val = std::clamp(val, static_cast<LONG>(minVal), static_cast<LONG>(maxVal));
}

void CXySlider::InternToExtern()
{
    m_externalPos.x = static_cast<LONG>(std::round(static_cast<double>(m_pos.x) * m_externalRange.cx / m_range.cx));
    m_externalPos.y = static_cast<LONG>(std::round(static_cast<double>(m_pos.y) * m_externalRange.cy / m_range.cy));
}

void CXySlider::ExternToIntern()
{
    m_pos.x = static_cast<LONG>(std::round(static_cast<double>(m_externalPos.x) * m_range.cx / m_externalRange.cx));
    m_pos.y = static_cast<LONG>(std::round(static_cast<double>(m_externalPos.y) * m_range.cy / m_externalRange.cy));
}

void CXySlider::NotifyParent() const
{
    NMHDR hdr{
        .hwndFrom = m_hWnd,
        .idFrom   = static_cast<UINT_PTR>(GetDlgCtrlID()),
        .code     = XYSLIDER_CHANGED
    };

    GetParent()->SendMessage(WM_NOTIFY, GetDlgCtrlID(), &hdr);
}

void CXySlider::PaintBackground(CDC* pdc)
{
    pdc->FillSolidRect(m_rcAll, DarkMode::SystemColor(COLOR_BTNFACE));

    CRect rc = m_rcInner;
    pdc->DrawEdge(rc, EDGE_SUNKEN, BF_RECT | BF_ADJUST);

    pdc->FillSolidRect(rc, RGB(255, 255, 255));

    const CPen pen(PS_SOLID, 1, DarkMode::SystemColor(COLOR_3DLIGHT));
    GdiObjectSelection sopen(pdc, &pen);

    pdc->MoveTo(rc.left, m_zero.y);
    pdc->LineTo(rc.right, m_zero.y);
    pdc->MoveTo(m_zero.x, rc.top);
    pdc->LineTo(m_zero.x, rc.bottom);

    CRect circle = m_rcAll;
    circle.Deflate(m_gripperRadius);

    StockObjectSelection sobrush(pdc, NULL_BRUSH);
    pdc->Ellipse(circle);

    if (GetFocus() == this)
    {
        pdc->DrawFocusRect(m_rcAll);
    }
}

void CXySlider::PaintGripper(CDC* pdc) const
{
    CRect rc = GetGripperRect();

    COLORREF color = DarkMode::SystemColor(COLOR_BTNFACE);
    if (m_gripperHighlight)
    {
        auto r = GetRValue(color);
        auto g = GetGValue(color);
        auto b = GetBValue(color);
        r += (255 - r) / 3;
        g += (255 - g) / 3;
        b += (255 - b) / 3;
        color = RGB(r, g, b);
    }
    pdc->FillSolidRect(rc, color);
    pdc->DrawEdge(rc, EDGE_RAISED, BF_RECT);

    const CPen pen(PS_SOLID, 1, DarkMode::SystemColor(COLOR_3DSHADOW));
    GdiObjectSelection sopen(pdc, &pen);

    pdc->MoveTo(rc.left, rc.top + rc.Height() / 2);
    pdc->LineTo(rc.right, rc.top + rc.Height() / 2);
    pdc->MoveTo(rc.left + rc.Width() / 2, rc.top);
    pdc->LineTo(rc.left + rc.Width() / 2, rc.bottom);
}

void CXySlider::DoMoveBy(const int cx, const int cy)
{
    m_pos.x += cx;
    CheckMinMax(m_pos.x, -m_range.cx, m_range.cx);

    m_pos.y += cy;
    CheckMinMax(m_pos.y, -m_range.cy, m_range.cy);

    RedrawWindow();

    const CPoint oldpos = m_externalPos;
    InternToExtern();
    if (m_externalPos != oldpos)
    {
        NotifyParent();
    }
}

void CXySlider::DoDrag(const CPoint & point)
{
    CPoint pt0 = point;

    HighlightGripper(true);

    const CSize inGripper = pt0 - GetGripperRect().Center();
    const CPoint ptMin(m_zero - m_range + inGripper);
    const CPoint ptMax(m_zero + m_range + inGripper);

    SetCapture();
    while (true)
    {
        MSG msg;
        if (!GetMessage(&msg, nullptr, 0, 0))
        {
            break;
        }

        if (msg.message == WM_LBUTTONUP)
        {
            break;
        }

        if (GetCapture() != this)
        {
            break;
        }

        if (msg.message == WM_MOUSEMOVE)
        {
            CPoint pt = ToClient(msg.pt);

            CheckMinMax(pt.x, ptMin.x, ptMax.x);
            CheckMinMax(pt.y, ptMin.y, ptMax.y);

            const int dx = pt.x - pt0.x;
            const int dy = pt.y - pt0.y;

            DoMoveBy(dx, dy);

            pt0 = pt;
        }
        else
        {
            DispatchMessage(&msg);
        }
    }
    ReleaseCapture();

    HighlightGripper(false);
}

void CXySlider::DoPage(const CPoint & point)
{
    const CSize sz = point - (m_zero + m_pos);

    assert(sz.cx != 0 || sz.cy != 0);

    const double len = std::hypot(sz.cx, sz.cy);

    constexpr double d = 10;

    const int dx = static_cast<int>(d * sz.cx / len);
    const int dy = static_cast<int>(d * sz.cy / len);

    DoMoveBy(dx, dy);
}

void CXySlider::HighlightGripper(const bool on)
{
    m_gripperHighlight = on;
    RedrawWindow();
}

UINT CXySlider::OnGetDlgCode()
{
    return DLGC_WANTARROWS;
}

LRESULT CXySlider::OnNcHitTest(CPoint /*point*/)
{
    return HTCLIENT;
}

void CXySlider::OnSetFocus(CWnd* pOldWnd)
{
    CStatic::OnSetFocus(pOldWnd);
    Invalidate();
}

void CXySlider::OnKillFocus(CWnd* pNewWnd)
{
    CStatic::OnKillFocus(pNewWnd);
    Invalidate();
}

void CXySlider::OnPaint()
{
    Initialize();

    CPaintDC paintDC(this);
    CBufferedDC dc(paintDC, this);

    PaintBackground(&dc);
    PaintGripper(&dc);
}

void CXySlider::OnKeyDown(const UINT nChar, const UINT nRepCnt, const UINT nFlags)
{
    switch (nChar)
    {
        case VK_LEFT:  DoMoveBy(-1, 0); break;
        case VK_RIGHT: DoMoveBy(1, 0);  break;
        case VK_UP:    DoMoveBy(0, -1); break;
        case VK_DOWN:  DoMoveBy(0, 1);  break;
        default: CStatic::OnKeyDown(nChar, nRepCnt, nFlags);
    }
}

void CXySlider::OnLButtonDown(UINT /*nFlags*/, const CPoint point)
{
    if (GetGripperRect().Contains(point))
    {
        SetFocus();
        DoDrag(point);
    }
}

void CXySlider::OnLButtonDblClk(UINT /*nFlags*/, const CPoint point)
{
    SetFocus();

    if (GetGripperRect().Contains(point))
    {
        DoMoveBy(-m_pos.x, -m_pos.y);
    }
    else
    {
        DoPage(point);
    }
}
