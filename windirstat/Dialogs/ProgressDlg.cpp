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
#include "ProgressDlg.h"

IMPLEMENT_DYNAMIC(CProgressDlg, CDialogEx)

CProgressDlg::CProgressDlg(const size_t total, const bool noCancel, CWnd* pParent, std::function<void(CProgressDlg*)> task)
    : CDialogEx(IDD, pParent)
    , m_message(Localization::Lookup(IDS_PROGRESS))
    , m_task(std::move(task))
    , m_total(total)
    , m_noCancel(noCancel)
{
}

BEGIN_MESSAGE_MAP(CProgressDlg, CDialogEx)
    ON_WM_TIMER()
    ON_WM_CTLCOLOR()
    ON_BN_CLICKED(IDCANCEL, OnCancel)
END_MESSAGE_MAP()

void CProgressDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_PROGRESS_MESSAGE, m_messageCtrl);
    DDX_Control(pDX, IDC_PROGRESS_BAR, m_progressCtrl);
    DDX_Control(pDX, IDCANCEL, m_cancelButton);
}

BOOL CProgressDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(GetSafeHwnd());

    // Set window title and message
    SetWindowText(wds::strWinDirStat);
    m_messageCtrl.SetWindowText(m_message.c_str());

    // Configure cancel button
    if (m_noCancel) m_cancelButton.ShowWindow(SW_HIDE);

    // Configure progress bar
    if (m_total > 0)
    {
        // Start timer for progress updates
        SetTimer(TIMER_ID, TIMER_INTERVAL, nullptr);
    }
    else
    {
        m_progressCtrl.ModifyStyle(0, PBS_MARQUEE);
        m_progressCtrl.SetMarquee(TRUE, 30);
    }

    // Center dialog
    CenterWindow();

    // Start worker thread
    StartWorkerThread();

    return TRUE;
}

void CProgressDlg::StartWorkerThread()
{
    m_workerThread.emplace([this]()
    {
        // Execute the task, passing the dialog pointer
        m_task(this);

        // Attempt to have timer fire one last time to update progress
        if (m_total > 0) (void) SendMessage(WM_TIMER, TIMER_ID);

        // Post message to close dialog when complete
        if (!m_cancelRequested)
        {
            PostMessage(WM_COMMAND, IDOK);
        }
    });
}

void CProgressDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_ID)
    {
        // Update progress bar position
        m_progressCtrl.SetPos(static_cast<int>((m_current.load() * 100) / m_total));

        // Update message with progress
        const std::wstring progressText = std::format(L"{}: {} / {}",
            m_message, FormatCount(m_current.load()), FormatCount(m_total));
        m_messageCtrl.SetWindowText(progressText.c_str());
    }
    CDialogEx::OnTimer(nIDEvent);
}

void CProgressDlg::OnCancel()
{
    // Request cancellation
    CWaitCursor wc;
    m_cancelRequested = true;
    m_cancelled = true;

    // Disable cancel button to prevent multiple clicks
    m_cancelButton.EnableWindow(FALSE);

    // Wait for worker thread to complete
    if (m_workerThread.has_value())
    {
        ProcessMessagesUntilSignaled([this]
        {
            if (m_workerThread->joinable()) m_workerThread->join();
        });
        m_workerThread.reset();
    }

    CDialogEx::OnCancel();
}

INT_PTR CProgressDlg::DoModal()
{
    const INT_PTR result = CDialogEx::DoModal();

    // Clean up worker thread if still running
    if (m_workerThread.has_value())
    {
        if (m_workerThread->joinable()) m_workerThread->join();
        m_workerThread.reset();
    }

    return result;
}

HBRUSH CProgressDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}

BEGIN_MESSAGE_MAP(CWdsProgressCtrl, CProgressCtrl)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL CWdsProgressCtrl::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

void CWdsProgressCtrl::OnPaint()
{
    CPaintDC dc(this);
    CRect rect;
    GetClientRect(&rect);

    const bool isDark = DarkMode::IsDarkModeActive();
    const COLORREF trackPenColor = isDark ? DarkMode::WdsSysColor(COLOR_WINDOWFRAME) : GetSysColor(COLOR_3DSHADOW);
    const COLORREF trackBrushColor = isDark ? DarkMode::WdsSysColor(COLOR_WINDOWFRAME) : GetSysColor(COLOR_WINDOW);

    // 1. Draw track background (no border)
    {
        CPen nullPen(PS_NULL, 0, RGB(0,0,0));
        CBrush trackBrush(trackBrushColor);
        const CSelectObject soPen(&dc, &nullPen);
        const CSelectObject soBrush(&dc, &trackBrush);
        dc.RoundRect(&rect, CPoint(4, 4));
    }

    // 2. Draw progress fill (square)
    const COLORREF progColor = DarkMode::WdsSysColor(COLOR_HIGHLIGHT);
    CRect progRect = rect;
    progRect.DeflateRect(1, 1);

    if (GetStyle() & PBS_MARQUEE)
    {
        const ULONGLONG tick = GetTickCount64();
        const float cycle = static_cast<float>(tick % 1500) / 1500.0f;
        const int blockWidth = std::max(10, progRect.Width() / 5);
        const int xOffset = static_cast<int>(cycle * (progRect.Width() + blockWidth)) - blockWidth;

        const int origLeft = progRect.left;
        const int origRight = progRect.right;

        progRect.left = std::clamp(origLeft + xOffset, origLeft, origRight);
        progRect.right = std::clamp(origLeft + xOffset + blockWidth, origLeft, origRight);

        if (progRect.left < progRect.right)
        {
            dc.FillSolidRect(&progRect, progColor);
        }
    }
    else
    {
        int lower = 0, upper = 100;
        GetRange(lower, upper);
        const float percent = (upper > lower) ? std::clamp(static_cast<float>(GetPos() - lower) / (upper - lower), 0.0f, 1.0f) : 0.0f;

        if (percent > 0.0f)
        {
            progRect.right = std::max(progRect.left, progRect.left + static_cast<int>(progRect.Width() * percent));
            dc.FillSolidRect(&progRect, progColor);
        }
    }

    // 3. Draw track border (hollow)
    {
        CPen trackPen(PS_SOLID, 1, trackPenColor);
        LOGBRUSH lb = { BS_NULL, 0, 0 };
        CBrush nullBrush;
        nullBrush.CreateBrushIndirect(&lb);

        const CSelectObject soPen(&dc, &trackPen);
        const CSelectObject soBrush(&dc, &nullBrush);
        dc.RoundRect(&rect, CPoint(4, 4));
    }
}
