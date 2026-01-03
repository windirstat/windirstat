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
#include "SelectObject.h"
#include "Layout.h"

/////////////////////////////////////////////////////////////////////////////
// CLayoutDialogEx

IMPLEMENT_DYNCREATE(CLayoutDialogEx, CDialogEx)

BEGIN_MESSAGE_MAP(CLayoutDialogEx, CDialogEx)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CLayoutDialogEx::PreTranslateMessage(MSG* pMsg)
{
    // Check for Ctrl+C key combination
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == 'C' && IsControlKeyDown())
    {
        // Get the mouse cursor position
        CPoint pt;
        GetCursorPos(&pt);
        ScreenToClient(&pt);

        // Find which child window is at this position
        if (CWnd* pWndUnderCursor = ChildWindowFromPoint(pt, CWP_SKIPINVISIBLE);
            pWndUnderCursor != nullptr && pWndUnderCursor != this)
        {
            // Prefer MFC RTTI over raw window class-name checks.
            if (pWndUnderCursor->IsKindOf(RUNTIME_CLASS(CStatic)))
            {
                // Get the text from the static control
                CString text;
                pWndUnderCursor->GetWindowText(text);
                if (!text.IsEmpty())
                {
                    CMainFrame::Get()->CopyToClipboard(text.GetString());
                    return TRUE; // Message handled
                }
            }
        }
    }

    return CDialogEx::PreTranslateMessage(pMsg);
}

void CLayoutDialogEx::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    m_layout.OnSize();
}

void CLayoutDialogEx::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    m_layout.OnGetMinMaxInfo(lpMMI);
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}

void CLayoutDialogEx::OnDestroy()
{
    m_layout.OnDestroy();
    CDialogEx::OnDestroy();
}

/////////////////////////////////////////////////////////////////////////////
// CLayout

CLayout::CLayout(CWnd* dialog, RECT* placement)
    : m_wp(placement), m_dialog(dialog), m_originalDialogSize(0, 0)
{
    ASSERT(dialog != nullptr);
}

int CLayout::AddControl(CWnd* control, double movex, double movey, double stretchx, double stretchy)
{
    m_control.emplace_back(control, movex, movey, stretchx, stretchy);
    return static_cast<int>(m_control.size() - 1);
}

void CLayout::AddControl(const UINT id, const double movex, const double movey, const double stretchx, const double stretchy)
{
    AddControl(m_dialog->GetDlgItem(id), movex, movey, stretchx, stretchy);
}

void CLayout::OnInitDialog(const bool centerWindow)
{
    m_dialog->SetIcon(CDirStatApp::Get()->LoadIcon(IDR_MAINFRAME), false);

    CRect rcDialog;
    m_dialog->GetWindowRect(rcDialog);
    m_originalDialogSize = rcDialog.Size();

    for (auto& info : m_control)
    {
        CRect rc;
        info.control->GetWindowRect(rc);
        m_dialog->ScreenToClient(rc);
        info.originalRectangle = rc;
    }

    // Create size gripper
    CRect sg;
    m_dialog->GetClientRect(sg);
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
    m_dialog->GetWindowRect(m_wp);
}

void CLayout::OnSize()
{
    CRect wrc;
    m_dialog->GetWindowRect(wrc);
    const CSize diff = wrc.Size() - m_originalDialogSize;

    CPositioner pos(static_cast<int>(m_control.size()));

    for (const auto& [control, movex, movey, stretchx, stretchy, originalRectangle] : m_control)
    {
        CRect rc = originalRectangle;

        rc.OffsetRect(static_cast<int>(diff.cx * movex), static_cast<int>(diff.cy * movey));
        rc.right += static_cast<int>(diff.cx * stretchx);
        rc.bottom += static_cast<int>(diff.cy * stretchy);

        pos.SetWindowPos(*control, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOOWNERZORDER | SWP_NOZORDER);
    }

    m_dialog->Invalidate();
}

void CLayout::OnGetMinMaxInfo(MINMAXINFO* mmi)
{
    if (m_originalDialogSize.cx > 0) // Check if initialized
    {
        mmi->ptMinTrackSize = { m_originalDialogSize.cx, m_originalDialogSize.cy };
    }
}

/////////////////////////////////////////////////////////////////////////////

void CLayout::CSizeGripper::Create(CWnd* parent, const CRect rc)
{
    CWnd::Create(AfxRegisterWndClass(0,
        CDirStatApp::Get()->LoadStandardCursor(IDC_ARROW), nullptr, nullptr),
        wds::strEmpty, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, rc, parent, IDC_SIZEGRIPPER);
}

BEGIN_MESSAGE_MAP(CLayout::CSizeGripper, CWnd)
    ON_WM_PAINT()
    ON_WM_NCHITTEST()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL CLayout::CSizeGripper::OnEraseBkgnd(CDC* pDC)
{
    CRect rc;
    GetClientRect(rc);
    pDC->FillSolidRect(rc, DarkMode::WdsSysColor(CTLCOLOR_DLG));
    return TRUE;
}

void CLayout::CSizeGripper::OnPaint()
{
    CPaintDC dc(this);

    CRect rc;
    GetClientRect(rc);

    ASSERT(rc.Width() == m_width);
    ASSERT(rc.Height() == m_width);

    // Draw three diagonal shadow lines
    for (int offset : {1, 5, 9})
    {
        DrawShadowLine(&dc, { offset, m_width }, { m_width, offset });
    }
}

void CLayout::CSizeGripper::DrawShadowLine(CDC* pdc, CPoint start, CPoint end)
{
    // Draw highlight line
    {
        CPen lightPen(PS_SOLID, 1, DarkMode::WdsSysColor(COLOR_3DHIGHLIGHT));
        const CSelectObject sopen(pdc, &lightPen);
        pdc->MoveTo(start);
        pdc->LineTo(end);
    }

    // Draw shadow lines (2 pixels for depth effect)
    CPen darkPen(PS_SOLID, 1, DarkMode::WdsSysColor(COLOR_3DSHADOW));
    const CSelectObject sopen(pdc, &darkPen);

    for (const int i : std::views::iota(1, 3))
    {
        pdc->MoveTo(start.x + i, start.y + i);
        pdc->LineTo(end.x + i, end.y + i);
    }
}

LRESULT CLayout::CSizeGripper::OnNcHitTest(CPoint point)
{
    ScreenToClient(&point);
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

void CLayout::CPositioner::SetWindowPos(HWND hWnd, const int x, const int y, const int cx, const int cy, const UINT uFlags)
{
    m_wdp = DeferWindowPos(m_wdp, hWnd, nullptr, x, y, cx, cy, uFlags | SWP_NOZORDER);
}
