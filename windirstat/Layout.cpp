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
#include "Options.h"
#include "SelectObject.h"
#include "Layout.h"

CLayout::CLayout(CWnd* dialog, RECT* wp)
{
    ASSERT(dialog != NULL);
    m_wp = wp;
    m_dialog = dialog;

    // This is necessary because OnGetMinMaxInfo() will be called
    // before OnInitDialog!
    m_originalDialogSize.cx = 0;
    m_originalDialogSize.cy = 0;
}

int CLayout::AddControl(CWnd* control, double movex, double movey, double stretchx, double stretchy)
{
    SControlInfo info(control, movex, movey, stretchx, stretchy);

    return static_cast<int>(m_control.Add(info));
}

void CLayout::AddControl(UINT id, double movex, double movey, double stretchx, double stretchy)
{
    AddControl(m_dialog->GetDlgItem(id), movex, movey, stretchx, stretchy);
}

void CLayout::OnInitDialog(bool centerWindow)
{
    m_dialog->SetIcon(GetWDSApp()->LoadIcon(IDR_MAINFRAME), false);

    CRect rcDialog;
    m_dialog->GetWindowRect(rcDialog);
    m_originalDialogSize = rcDialog.Size();

    for (int i = 0; i < m_control.GetSize(); i++)
    {
        CRect rc;
        m_control[i].control->GetWindowRect(rc);
        m_dialog->ScreenToClient(rc);
        m_control[i].originalRectangle = rc;
    }

    CRect sg;
    m_dialog->GetClientRect(sg);
    sg.left = sg.right - m_sizeGripper._width;
    sg.top  = sg.bottom - m_sizeGripper._width;
    m_sizeGripper.Create(m_dialog, sg);

    const int i                    = AddControl(&m_sizeGripper, 1, 1, 0, 0);
    m_control[i].originalRectangle = sg;

    m_dialog->MoveWindow(m_wp);
    if (centerWindow)
    {
        m_dialog->CenterWindow();
    }
}

void CLayout::OnDestroy() const
{
    m_dialog->GetWindowRect(m_wp);
}

void CLayout::OnSize()
{
    CRect wrc;
    m_dialog->GetWindowRect(wrc);
    const CSize newDialogSize = wrc.Size();

    const CSize diff = newDialogSize - m_originalDialogSize;

    CPositioner pos(static_cast<int>(m_control.GetSize()));

    for (int i = 0; i < m_control.GetSize(); i++)
    {
        CRect rc = m_control[i].originalRectangle;

        const CSize move(static_cast<int>(diff.cx * m_control[i].movex), static_cast<int>(diff.cy * m_control[i].movey));
        CRect stretch(0, 0, static_cast<int>(diff.cx * m_control[i].stretchx), static_cast<int>(diff.cy * m_control[i].stretchy));

        rc += move;
        rc += stretch;

        pos.SetWindowPos(*m_control[i].control, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOOWNERZORDER | SWP_NOZORDER);
    }
}

void CLayout::OnGetMinMaxInfo(MINMAXINFO* mmi)
{
    mmi->ptMinTrackSize.x = m_originalDialogSize.cx;
    mmi->ptMinTrackSize.y = m_originalDialogSize.cy;
}

/////////////////////////////////////////////////////////////////////////////

const int CLayout::CSizeGripper::_width = 14;

void CLayout::CSizeGripper::Create(CWnd* parent, CRect rc)
{
    VERIFY(CWnd::Create(
        AfxRegisterWndClass(
            0,
            AfxGetApp()->LoadStandardCursor(IDC_ARROW),
            (HBRUSH)(COLOR_BTNFACE + 1),
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

    ASSERT(rc.Width() == _width);
    ASSERT(rc.Height() == _width);

    CPoint start;
    CPoint end;

    start.x = 1;
    start.y = _width;
    end.x   = _width;
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

    if (point.x + point.y >= _width)
    {
        return HTBOTTOMRIGHT;
    }

    return 0;
}

CLayout::CPositioner::CPositioner(int nNumWindows)
    : m_wdp(::BeginDeferWindowPos(nNumWindows))
{
}

CLayout::CPositioner::~CPositioner()
{
    ::EndDeferWindowPos(m_wdp);
}

void CLayout::CPositioner::SetWindowPos(HWND hWnd, int x, int y, int cx, int cy, UINT uFlags)
{
    m_wdp = ::DeferWindowPos(m_wdp, hWnd, nullptr, x, y, cx, cy, uFlags | SWP_NOZORDER);
}
