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
#include "Layout.h"

/////////////////////////////////////////////////////////////////////////////
// CLayoutDialog

void CLayoutDialog::OnSize(const UINT nType, const int cx, const int cy)
{
    CDialog::OnSize(nType, cx, cy);
    m_layout.OnSize();
}

void CLayoutDialog::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    m_layout.OnGetMinMaxInfo(lpMMI);
    CDialog::OnGetMinMaxInfo(lpMMI);
}

void CLayoutDialog::OnDestroy()
{
    m_layout.OnDestroy();
    CDialog::OnDestroy();
}

/////////////////////////////////////////////////////////////////////////////
// CLayout

CLayout::CLayout(CWnd* dialog, RECT* placement)
    : m_wp(placement), m_dialog(dialog), m_originalDialogSize(0, 0)
{
    assert(dialog != nullptr);
}

int CLayout::AddControl(CWnd* control, double movex, double movey, double stretchx, double stretchy)
{
    m_control.emplace_back(control->Handle(), movex, movey, stretchx, stretchy);
    return static_cast<int>(m_control.size() - 1);
}

void CLayout::AddControl(const UINT id, const double movex, const double movey, const double stretchx, const double stretchy)
{
    m_control.emplace_back(GetDlgItem(m_dialog->Handle(), id), movex, movey, stretchx, stretchy);
}

void CLayout::OnInitDialog(const bool centerWindow)
{
    m_dialog->SetIcon(LoadIconW(GetAppInstance(), MAKEINTRESOURCEW(IDR_MAINFRAME)), false);

    const CRect rcDialog(m_dialog->Handle());
    m_originalDialogSize = rcDialog.Size();

    for (auto& info : m_control)
    {
        info.originalRectangle = m_dialog->WindowRectInClient(info.control);
    }

    // Create size gripper
    CRect sg = m_dialog->ClientRect();
    sg.left = sg.right - m_sizeGripper.m_width;
    sg.top = sg.bottom - m_sizeGripper.m_width;
    m_sizeGripper.Create(m_dialog, sg);

    const int i = AddControl(&m_sizeGripper, 1, 1, 0, 0);
    m_control[i].originalRectangle = sg;

    m_dialog->MoveWindow(m_wp);
    if (centerWindow)
    {
        m_dialog->CenterWindow();
    }
}

void CLayout::OnDestroy() const
{
    if (m_wp != nullptr) *m_wp = CRect(m_dialog->Handle());
}

void CLayout::OnSize()
{
    const CRect wrc(m_dialog->Handle());
    const CSize diff = wrc.Size() - m_originalDialogSize;

    CPositioner pos(static_cast<int>(m_control.size()));

    for (const auto& [control, movex, movey, stretchx, stretchy, originalRectangle] : m_control)
    {
        CRect rc = originalRectangle;

        rc.Offset(static_cast<int>(diff.cx * movex), static_cast<int>(diff.cy * movey));
        rc.right += static_cast<int>(diff.cx * stretchx);
        rc.bottom += static_cast<int>(diff.cy * stretchy);

        pos.SetWindowPos(control, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOOWNERZORDER | SWP_NOZORDER);
    }

    m_dialog->Invalidate();
}

void CLayout::OnGetMinMaxInfo(MINMAXINFO* mmi) const
{
    if (m_originalDialogSize.cx > 0) // Check if initialized
    {
        mmi->ptMinTrackSize = { m_originalDialogSize.cx, m_originalDialogSize.cy };
    }
}

/////////////////////////////////////////////////////////////////////////////

void CLayout::CSizeGripper::Create(CWnd* parent, const CRect rc)
{
    CWnd::Create(RegisterWindowClass(0, LoadCursorW(nullptr, IDC_ARROW)),
        wds::strEmpty, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, rc, parent, IDC_SIZEGRIPPER);
}

void CLayout::CSizeGripper::OnPaint()
{
    // Draw three diagonal shadow lines
    CPaintDC dc(this);
    DrawThemeParentBackgroundEx(Handle(), dc.Handle(),
        DTPB_USEERASEBKGND | DTPB_USECTLCOLORSTATIC, nullptr);

    for (int offset : {1, 5, 9})
    {
        DrawShadowLine(&dc, { offset, m_width }, { m_width, offset });
    }
}

void CLayout::CSizeGripper::DrawShadowLine(CDC* pdc, const CPoint start, const CPoint end)
{
    // Draw highlight line
    {
        const CPen lightPen(PS_SOLID, 1, DarkMode::SystemColor(COLOR_3DHIGHLIGHT));
        const GdiObjectSelection sopen(pdc, &lightPen);
        pdc->MoveTo(start);
        pdc->LineTo(end);
    }

    // Draw shadow lines (2 pixels for depth effect)
    const CPen darkPen(PS_SOLID, 1, DarkMode::SystemColor(COLOR_3DSHADOW));
    const GdiObjectSelection sopen(pdc, &darkPen);

    for (const int i : std::views::iota(1, 3))
    {
        pdc->MoveTo(start.x + i, start.y + i);
        pdc->LineTo(end.x + i, end.y + i);
    }
}

LRESULT CLayout::CSizeGripper::OnNcHitTest(CPoint point) const
{
    point = ToClient(point);
    return (point.x + point.y >= m_width) ? HTBOTTOMRIGHT : 0;
}

CLayout::CPositioner::CPositioner(const int nNumWindows)
    : m_wdp(BeginDeferWindowPos(nNumWindows))
{
}

CLayout::CPositioner::~CPositioner()
{
    if (m_wdp != nullptr)
    {
        EndDeferWindowPos(m_wdp);
    }
}

void CLayout::CPositioner::SetWindowPos(const HWND hWnd, const int x, const int y, const int cx, const int cy, const UINT uFlags)
{
    m_wdp = DeferWindowPos(m_wdp, hWnd, nullptr, x, y, cx, cy, uFlags | SWP_NOZORDER);
}
