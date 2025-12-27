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

#pragma once

#include "pch.h"

//
// CProgressDlg - Modal progress dialog for long-running operations
// Shows progress bar and allows cancellation
//
class CProgressDlg final : public CDialogEx
{
    DECLARE_DYNAMIC(CProgressDlg)

public:
    CProgressDlg(size_t total, bool noCancel, CWnd* pParent, std::function<void(std::atomic<bool>&, std::atomic<size_t>&)> task);
    ~CProgressDlg() override = default;

    INT_PTR DoModal() override;
    bool WasCancelled() const { return m_cancelled; }

protected:
    enum : std::uint8_t { IDD = IDD_PROGRESS };

    BOOL OnInitDialog() override;
    void DoDataExchange(CDataExchange* pDX) override;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnCancel() override;
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

private:
    void UpdateProgress();
    void StartWorkerThread();

    std::wstring m_message;
    std::function<void(std::atomic<bool>&, std::atomic<size_t>&)> m_task;
    
    CStatic m_messageCtrl;
    CProgressCtrl m_progressCtrl;
    CButton m_cancelButton;

    std::atomic<bool> m_cancelRequested = false;
    std::atomic<size_t> m_current = 0;
    const size_t m_total = 0;
    bool m_cancelled = false;
    const bool m_noCancel = false;
    UINT_PTR m_nTimerID = 0;

    std::optional<std::jthread> m_workerThread;
    static constexpr UINT_PTR TIMER_ID = 1;
    static constexpr UINT TIMER_INTERVAL = 100; // Update every 100ms
};
