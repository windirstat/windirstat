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
#include "WinDirStatPane.h"

IMPLEMENT_DYNAMIC(CWinDirStatPane, CWnd)

BEGIN_MESSAGE_MAP(CWinDirStatPane, CWnd)
    ON_WM_CREATE()
    ON_WM_MOUSEACTIVATE()
    ON_WM_PAINT()
END_MESSAGE_MAP()

BOOL CWinDirStatPane::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CWnd::PreCreateWindow(cs))
    {
        return FALSE;
    }

    if (cs.lpszClass == nullptr)
    {
        cs.lpszClass = AfxRegisterWndClass(
            CS_DBLCLKS,
            ::LoadCursor(nullptr, IDC_ARROW),
            nullptr,
            nullptr);
    }

    return TRUE;
}

void CWinDirStatPane::PostNcDestroy()
{
    delete this;
}

int CWinDirStatPane::OnCreate(LPCREATESTRUCT /*lpCreateStruct*/)
{
    return 0;
}

void CWinDirStatPane::OnPaint()
{
    CPaintDC dc(this);
    OnDraw(&dc);
}

void CWinDirStatPane::OnDraw(CDC* /*pDC*/)
{
}

int CWinDirStatPane::OnMouseActivate(CWnd* pDesktopWnd, const UINT nHitTest, const UINT message)
{
    const int result = CWnd::OnMouseActivate(pDesktopWnd, nHitTest, message);
    if (result != MA_NOACTIVATE && result != MA_NOACTIVATEANDEAT)
    {
        const HWND focus = ::GetFocus();
        if (m_hWnd != focus && !::IsChild(m_hWnd, focus) && IsTopParentActive())
        {
            SetFocus();
        }
    }
    return result;
}

void CWinDirStatPane::OnUpdate(CWnd* /*sender*/, MODEL_CHANGE /*change*/, CItem* /*item*/)
{
    InvalidateRect(nullptr);
}

void CWinDirStatPane::OnSize(UINT /*nType*/, int /*cx*/, int /*cy*/)
{
}

void CWinDirStatPane::OnLButtonDblClk(UINT /*nFlags*/, CPoint /*point*/)
{
}

void CWinDirStatPane::OnLButtonDown(UINT /*nFlags*/, CPoint /*point*/)
{
}

void CWinDirStatPane::OnMButtonDown(UINT /*nFlags*/, CPoint /*point*/)
{
}

BOOL CWinDirStatPane::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    return CWnd::OnMouseWheel(nFlags, zDelta, pt);
}

void CWinDirStatPane::NotifyOtherPanes(MODEL_CHANGE change, CItem* item)
{
    CWinDirStatModel::Get()->NotifyPanesExcept(this, change, item);
}
