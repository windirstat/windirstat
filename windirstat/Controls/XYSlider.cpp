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
#include "resource.h"

IMPLEMENT_DYNAMIC(CXySlider, CStatic)

void AFXAPI DDX_XySlider(CDataExchange* pDX, int nIDC, CPoint& value)
{
    pDX->PrepareCtrl(nIDC);
    HWND hWndCtrl;
    pDX->m_pDlgWnd->GetDlgItem(nIDC, &hWndCtrl);
    SendMessage(hWndCtrl, pDX->m_bSaveAndValidate ? CXySlider::XY_GETPOS : CXySlider::XY_SETPOS,
        0, reinterpret_cast<LPARAM>(&value));
}

void CXySlider::Initialize()
{
    if (!m_inited && IsWindow(m_hWnd))
    {
        // Make size odd, so that zero lines are central
        CRect rc;
        GetWindowRect(rc);
        GetParent()->ScreenToClient(rc);
        if (rc.Width() % 2 == 0)
        {
            rc.right--;
        }
        if (rc.Height() % 2 == 0)
        {
            rc.bottom--;
        }
        MoveWindow(rc);

        // Initialize constants
        CalcSizes();

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

LRESULT CXySlider::OnGetPos(WPARAM, const LPARAM lparam)
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

void CXySlider::CalcSizes()
{
    static constexpr int s_gripperRadius = 8;

    GetClientRect(m_rcAll);

    ASSERT(m_rcAll.left == 0);
    ASSERT(m_rcAll.top == 0);
    ASSERT(m_rcAll.Width() % 2 == 1);
    ASSERT(m_rcAll.Height() % 2 == 1);
    ASSERT(m_rcAll.Width() >= s_gripperRadius * 2); // Control must be large enough
    ASSERT(m_rcAll.Height() >= s_gripperRadius * 2);

    m_zero.x = m_rcAll.Width() / 2;
    m_zero.y = m_rcAll.Height() / 2;

    m_radius.cx = m_rcAll.Width() / 2 - 1;
    m_radius.cy = m_rcAll.Height() / 2 - 1;

    m_rcInner = m_rcAll;
    m_rcInner.DeflateRect(s_gripperRadius - 3, s_gripperRadius - 3);

    m_gripperRadius.cx = s_gripperRadius;
    m_gripperRadius.cy = s_gripperRadius;

    m_range = m_radius - m_gripperRadius;
}

CRect CXySlider::GetGripperRect() const
{
    CRect rc(
        -m_gripperRadius.cx,
        -m_gripperRadius.cy,
        m_gripperRadius.cx + 1,
        m_gripperRadius.cy + 1
    );
    rc.OffsetRect(m_zero);
    rc.OffsetRect(m_pos);
    return rc;
}

void CXySlider::CheckMinMax(LONG& val, const int minVal, const int maxVal) const
{
    ASSERT(minVal <= maxVal);

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
    NMHDR hdr;
    hdr.hwndFrom = m_hWnd;
    hdr.idFrom   = GetDlgCtrlID();
    hdr.code     = XYSLIDER_CHANGED;

    GetParent()->SendMessage(WM_NOTIFY, GetDlgCtrlID(), reinterpret_cast<LPARAM>(&hdr));
}

void CXySlider::PaintBackground(CDC* pdc)
{
    pdc->FillSolidRect(m_rcAll, DarkMode::WdsSysColor(COLOR_BTNFACE));

    CRect rc = m_rcInner;
    pdc->DrawEdge(rc, EDGE_SUNKEN, BF_RECT | BF_ADJUST);

    pdc->FillSolidRect(rc, RGB(255, 255, 255));

    CPen pen(PS_SOLID, 1, DarkMode::WdsSysColor(COLOR_3DLIGHT));
    CSelectObject sopen(pdc, &pen);

    pdc->MoveTo(rc.left, m_zero.y);
    pdc->LineTo(rc.right, m_zero.y);
    pdc->MoveTo(m_zero.x, rc.top);
    pdc->LineTo(m_zero.x, rc.bottom);

    CRect circle = m_rcAll;
    circle.DeflateRect(m_gripperRadius);

    CSelectStockObject sobrush(pdc, NULL_BRUSH);
    pdc->Ellipse(circle);

    if (GetFocus() == this)
    {
        pdc->DrawFocusRect(m_rcAll);
    }
}

void CXySlider::PaintGripper(CDC* pdc) const
{
    CRect rc = GetGripperRect();

    COLORREF color = DarkMode::WdsSysColor(COLOR_BTNFACE);
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

    CPen pen(PS_SOLID, 1, DarkMode::WdsSysColor(COLOR_3DSHADOW));
    CSelectObject sopen(pdc, &pen);

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

    const CSize inGripper = pt0 - GetGripperRect().CenterPoint();
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
            CPoint pt = msg.pt;
            ScreenToClient(&pt);

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

    ASSERT(sz.cx != 0 || sz.cy != 0);

    const double len = sqrt(static_cast<double>(sz.cx * sz.cx + sz.cy * sz.cy));

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

BEGIN_MESSAGE_MAP(CXySlider, CStatic)
    ON_WM_GETDLGCODE()
    ON_WM_NCHITTEST()
    ON_WM_SETFOCUS()
    ON_WM_KILLFOCUS()
    ON_WM_PAINT()
    ON_WM_KEYDOWN()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONDBLCLK()
    ON_MESSAGE(CXySlider::XY_SETPOS, OnSetPos)
    ON_MESSAGE(CXySlider::XY_GETPOS, OnGetPos)
END_MESSAGE_MAP()

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

    CPaintDC dc(this);
    CMemDC memDC(dc, this);
    CDC* pDC = &memDC.GetDC();

    PaintBackground(pDC);
    PaintGripper(pDC);
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
    if (GetGripperRect().PtInRect(point))
    {
        SetFocus();
        DoDrag(point);
    }
}

void CXySlider::OnLButtonDblClk(UINT /*nFlags*/, const CPoint point)
{
    SetFocus();

    if (GetGripperRect().PtInRect(point))
    {
        DoMoveBy(-m_pos.x, -m_pos.y);
    }
    else
    {
        DoPage(point);
    }
}
