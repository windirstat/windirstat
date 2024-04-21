// Layout.cpp - Implementation of CLayout
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
#include "WinDirStat.h"
#include "SelectObject.h"
#include "Layout.h"

CLayout::CLayout(CWnd* dialog, RECT* placement)
{
    ASSERT(dialog != nullptr);
    m_Wp = placement;
    m_Dialog = dialog;

    // This is necessary because OnGetMinMaxInfo() will be called
    // before OnInitDialog!
    m_OriginalDialogSize.cx = 0;
    m_OriginalDialogSize.cy = 0;
}

int CLayout::AddControl(CWnd* control, double movex, double movey, double stretchx, double stretchy)
{
    m_Control.emplace_back(control, movex, movey, stretchx, stretchy);
    return static_cast<int>(m_Control.size() - 1);
}

void CLayout::AddControl(const UINT id, const double movex, const double movey, const double stretchx, const double stretchy)
{
    AddControl(m_Dialog->GetDlgItem(id), movex, movey, stretchx, stretchy);
}

void CLayout::OnInitDialog(const bool centerWindow)
{
    m_Dialog->SetIcon(CDirStatApp::Get()->LoadIcon(IDR_MAINFRAME), false);

    CRect rcDialog;
    m_Dialog->GetWindowRect(rcDialog);
    m_OriginalDialogSize = rcDialog.Size();

    for (auto & info : m_Control)
    {
        CRect rc;
        info.control->GetWindowRect(rc);
        m_Dialog->ScreenToClient(rc);
        info.originalRectangle = rc;
    }

    CRect sg;
    m_Dialog->GetClientRect(sg);
    sg.left = sg.right - m_SizeGripper.m_Width;
    sg.top  = sg.bottom - m_SizeGripper.m_Width;
    m_SizeGripper.Create(m_Dialog, sg);

    const int i                    = AddControl(&m_SizeGripper, 1, 1, 0, 0);
    m_Control[i].originalRectangle = sg;

    m_Dialog->MoveWindow(m_Wp);
    if (centerWindow)
    {
        m_Dialog->CenterWindow();
    }
}

void CLayout::OnDestroy() const
{
    m_Dialog->GetWindowRect(m_Wp);
}

void CLayout::OnSize()
{
    CRect wrc;
    m_Dialog->GetWindowRect(wrc);
    const CSize newDialogSize = wrc.Size();

    const CSize diff = newDialogSize - m_OriginalDialogSize;

    CPositioner pos(static_cast<int>(m_Control.size()));

    for (const auto& info : m_Control)
    {
        CRect rc = info.originalRectangle;

        const CSize move(static_cast<int>(diff.cx * info.movex), static_cast<int>(diff.cy * info.movey));
        CRect stretch(0, 0, static_cast<int>(diff.cx * info.stretchx), static_cast<int>(diff.cy * info.stretchy));

        rc += move;
        rc += stretch;

        pos.SetWindowPos(*info.control, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOOWNERZORDER | SWP_NOZORDER);
    }
}

void CLayout::OnGetMinMaxInfo(MINMAXINFO* mmi)
{
    mmi->ptMinTrackSize.x = m_OriginalDialogSize.cx;
    mmi->ptMinTrackSize.y = m_OriginalDialogSize.cy;
}

/////////////////////////////////////////////////////////////////////////////

void CLayout::CSizeGripper::Create(CWnd* parent, const CRect rc)
{
    VERIFY(CWnd::Create(
        AfxRegisterWndClass(
            0,
            CDirStatApp::Get()->LoadStandardCursor(IDC_ARROW),
            reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1),
            nullptr
        ),
        wds::strEmpty,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        rc,
        parent,
        IDC_SIZEGRIPPER
    ));
}

BEGIN_MESSAGE_MAP(CLayout::CSizeGripper, CWnd)
    ON_WM_PAINT()
    ON_WM_NCHITTEST()
END_MESSAGE_MAP()

void CLayout::CSizeGripper::OnPaint()
{
    CPaintDC dc(this);

    CRect rc;
    GetClientRect(rc);

    ASSERT(rc.Width() == m_Width);
    ASSERT(rc.Height() == m_Width);

    CPoint start;
    CPoint end;

    start.x = 1;
    start.y = m_Width;
    end.x   = m_Width;
    end.y   = 1;

    DrawShadowLine(&dc, start, end);

    start.x += 4;
    end.y += 4;

    DrawShadowLine(&dc, start, end);

    start.x += 4;
    end.y += 4;

    DrawShadowLine(&dc, start, end);

    // Do not call CWnd::OnPaint() for painting messages
}

void CLayout::CSizeGripper::DrawShadowLine(CDC* pdc, CPoint start, CPoint end)
{
    {
        CPen lightPen(PS_SOLID, 1, ::GetSysColor(COLOR_3DHIGHLIGHT));
        CSelectObject sopen(pdc, &lightPen);

        pdc->MoveTo(start);
        pdc->LineTo(end);
    }

    start.x++;
    end.y++;

    {
        CPen darkPen(PS_SOLID, 1, ::GetSysColor(COLOR_3DSHADOW));
        CSelectObject sopen(pdc, &darkPen);

        pdc->MoveTo(start);
        pdc->LineTo(end);

        start.x++;
        end.y++;

        pdc->MoveTo(start);
        pdc->LineTo(end);
    }
}

LRESULT CLayout::CSizeGripper::OnNcHitTest(CPoint point)
{
    ScreenToClient(&point);

    if (point.x + point.y >= m_Width)
    {
        return HTBOTTOMRIGHT;
    }

    return 0;
}

CLayout::CPositioner::CPositioner(const int nNumWindows)
    : m_Wdp(::BeginDeferWindowPos(nNumWindows))
{
}

CLayout::CPositioner::~CPositioner()
{
    ::EndDeferWindowPos(m_Wdp);
}

void CLayout::CPositioner::SetWindowPos(HWND hWnd, const int x, const int y, const int cx, const int cy, const UINT uFlags)
{
    m_Wdp = ::DeferWindowPos(m_Wdp, hWnd, nullptr, x, y, cx, cy, uFlags | SWP_NOZORDER);
}
