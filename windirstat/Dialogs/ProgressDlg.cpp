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

CProgressDlg::CProgressDlg(const size_t total, const bool noCancel, CWnd* pParent, std::function<void(std::atomic<bool>&, std::atomic<size_t>&)> task)
    : CDialogEx(IDD, pParent)
    , m_Total(total)
    , m_Message(Localization::Lookup(IDS_PROGRESS))
    , m_Task(std::move(task))
    , m_NoCancel(noCancel)
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
    DDX_Control(pDX, IDC_PROGRESS_MESSAGE, m_MessageCtrl);
    DDX_Control(pDX, IDC_PROGRESS_BAR, m_ProgressCtrl);
    DDX_Control(pDX, IDCANCEL, m_CancelButton);
}

BOOL CProgressDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(GetSafeHwnd());

    // Set window title and message
    SetWindowText(Localization::LookupNeutral(AFX_IDS_APP_TITLE).c_str());
    m_MessageCtrl.SetWindowText(m_Message.c_str());

    // Configure cancel button
    if (m_NoCancel) m_CancelButton.ShowWindow(SW_HIDE);

    // Configure progress bar
    if (m_Total > 0)
    {
        m_ProgressCtrl.SetRange(0, 100);
        m_ProgressCtrl.SetPos(0);

        // Start timer for progress updates
        SetTimer(TIMER_ID, TIMER_INTERVAL, nullptr);
    }
    else
    {
        m_ProgressCtrl.ModifyStyle(0, PBS_MARQUEE);
        m_ProgressCtrl.SetMarquee(TRUE, 30);
    }

    // Center dialog
    CenterWindow();

    // Start worker thread
    StartWorkerThread();

    return TRUE;
}

void CProgressDlg::StartWorkerThread()
{
    m_WorkerThread = new std::jthread([this]()
    {
        // Execute the task
        m_Task(m_CancelRequested, m_Current);
        
        // Post message to close dialog when complete
        if (!m_CancelRequested)
        {
            PostMessage(WM_COMMAND, IDOK);
        }
    });
}

void CProgressDlg::UpdateProgress()
{
    const int percent = static_cast<int>((m_Current.load() * 100) / m_Total);
    m_ProgressCtrl.SetPos(percent);

    // Update message with progress
    const std::wstring progressText = std::format(L"{}: {} / {}",
        m_Message, m_Current.load(), m_Total);
    m_MessageCtrl.SetWindowText(progressText.c_str());
}

void CProgressDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_ID)
    {
        UpdateProgress();
    }
    CDialogEx::OnTimer(nIDEvent);
}

void CProgressDlg::OnCancel()
{
    // Request cancellation
    CWaitCursor wc;
    m_CancelRequested = true;
    m_Cancelled = true;

    // Disable cancel button to prevent multiple clicks
    m_CancelButton.EnableWindow(FALSE);

    // Wait for worker thread to complete
    if (m_WorkerThread != nullptr)
    {
        ProcessMessagesUntilSignaled([this]
        {
            if (m_WorkerThread->joinable())
            {
                m_WorkerThread->join();
            }
        });

        delete m_WorkerThread;
        m_WorkerThread = nullptr;
    }

    CDialogEx::OnCancel();
}

INT_PTR CProgressDlg::DoModal()
{
    const INT_PTR result = CDialogEx::DoModal();

    // Clean up worker thread if still running
    if (m_WorkerThread != nullptr)
    {
        if (m_WorkerThread->joinable())
        {
            m_WorkerThread->join();
        }
        delete m_WorkerThread;
        m_WorkerThread = nullptr;
    }

    return result;
}

HBRUSH CProgressDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}
