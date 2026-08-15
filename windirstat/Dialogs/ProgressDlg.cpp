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

CProgressDlg::CProgressDlg(const size_t total, const Flags flags, CWnd* pParent, std::function<void(CProgressDlg*)> task)
    : MessageTarget(IDD, pParent)
    , m_message(Localization::Lookup(IDS_PROGRESS))
    , m_task(std::move(task))
    , m_total(total)
    , m_flags(flags)
{
}

bool CProgressDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(Handle());

    m_messageCtrl.SubclassDlgItem(IDC_PROGRESS_MESSAGE, this);
    m_progressCtrl.SubclassDlgItem(IDC_PROGRESS_BAR, this);
    m_cancelButton.SubclassDlgItem(IDCANCEL, this);

    // Set window title and message
    SetText(wds::strWinDirStat);
    m_messageCtrl.SetText(m_message.c_str());

    // Configure cancel button
    if (HasFlag(Flags::NoCancel)) m_cancelButton.ShowWindow(SW_HIDE);

    // Start timer for progress updates or marquee repaints.
    SetTimer(TIMER_ID, TIMER_INTERVAL);

    // Configure progress bar
    if (m_total == 0)
    {
        m_progressCtrl.ModifyStyle(0, PBS_MARQUEE);
        m_progressCtrl.SetMarquee(true, 30);
    }

    // Center dialog
    CenterWindow();

    // Start worker thread
    StartWorkerThread();

    return true;
}

void CProgressDlg::StartWorkerThread()
{
    m_workerThread = std::jthread([this]
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

void CProgressDlg::OnTimer(const UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_ID)
    {
        if (m_total == 0)
        {
            m_progressCtrl.Invalidate(false);
            CDialog::OnTimer(nIDEvent);
            return;
        }

        const size_t current = m_current.load();
        const double percent = (static_cast<double>(current) * 100) / m_total;

        // Update progress bar position
        m_progressCtrl.SetPos(static_cast<int>(std::clamp(percent, 0.0, 100.0)));

        // Update message with progress
        const std::wstring progressText = HasFlag(Flags::PercentageOnly) ?
            std::format(L"{}: {}%", m_message, FormatDouble(percent)) :
            std::format(L"{}: {}% ({} / {})",
                m_message, FormatDouble(percent), FormatCount(current), FormatCount(m_total));
        m_messageCtrl.SetText(progressText.c_str());
    }
    CDialog::OnTimer(nIDEvent);
}

void CProgressDlg::OnCancel()
{
    // Request cancellation
    CWaitCursor wc;
    m_cancelRequested = true;

    // Disable cancel button to prevent multiple clicks
    m_cancelButton.EnableWindow(false);

    // Wait for worker thread to complete
    if (m_workerThread.joinable())
    {
        CWinApp::RunTaskWithUiUpdates([this]
        {
            m_workerThread.join();
        });
        m_workerThread = {};
    }

    CDialog::OnCancel();
}

INT_PTR CProgressDlg::ShowModal()
{
    const INT_PTR result = CDialog::ShowModal();

    // Clean up worker thread if still running
    if (m_workerThread.joinable())
    {
        m_workerThread.join();
        m_workerThread = {};
    }

    return result;
}

HBRUSH CProgressDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CWdsProgressCtrl::OnPaint()
{
    CPaintDC dc(this);
    const CRect rect = ClientRect();

    // Draw track background and border
    {
        const bool isDark = DarkMode::IsDarkModeActive();
        const COLORREF trackPenColor = isDark ? DarkMode::SystemColor(COLOR_WINDOWFRAME) : GetSysColor(COLOR_3DSHADOW);
        const COLORREF trackBrushColor = isDark ? DarkMode::SystemColor(COLOR_WINDOWFRAME) : GetSysColor(COLOR_WINDOW);
        const CPen trackPen(PS_SOLID, 1, trackPenColor);
        const CBrush trackBrush(trackBrushColor);
        const GdiObjectSelection soPen(&dc, &trackPen);
        const GdiObjectSelection soBrush(&dc, &trackBrush);
        dc.RoundRect(&rect, CPoint(4, 4));
    }

    // Draw progress fill (square)
    const COLORREF progColor = DarkMode::SystemColor(COLOR_HIGHLIGHT);
    CRect progRect = rect;
    progRect.Deflate(1, 1);

    CRgn clipRgn(rect.left + 2, rect.top + 2, rect.right - 1, rect.bottom - 1, 2, 2);
    dc.SelectClipRgn(&clipRgn);

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
        const auto [lower, upper] = Range();
        if (const float percent = upper > lower
                ? std::clamp(static_cast<float>(GetPos() - lower) / (upper - lower), 0.0f, 1.0f)
                : 0.0f;
            percent > 0.0f)
        {
            progRect.right = std::max(progRect.left, progRect.left + static_cast<int>(progRect.Width() * percent));
            dc.FillSolidRect(&progRect, progColor);
        }
    }

    dc.SelectClipRgn(nullptr);
}
