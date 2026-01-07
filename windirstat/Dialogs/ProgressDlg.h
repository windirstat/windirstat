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
    CProgressDlg(size_t total, bool noCancel, CWnd* pParent, std::function<void(CProgressDlg*)> task);
    ~CProgressDlg() override = default;

    INT_PTR DoModal() override;
    bool WasCancelled() const noexcept { return m_cancelled; }

    // Methods for task lambda to interact with the dialog
    bool IsCancelled() const noexcept { return m_cancelRequested.load(); }
    void SetCurrent(size_t current) noexcept { m_current.store(current); }
    size_t GetCurrent() const noexcept { return m_current.load(); }
    size_t Increment() noexcept { return ++m_current; }
    size_t GetTotal() const noexcept { return m_total; }

protected:
    enum : std::uint8_t { IDD = IDD_PROGRESS };

    BOOL OnInitDialog() override;
    void DoDataExchange(CDataExchange* pDX) override;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnCancel() override;
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

private:
    void StartWorkerThread();

    std::wstring m_message;
    std::function<void(CProgressDlg*)> m_task;
    
    CStatic m_messageCtrl;
    CProgressCtrl m_progressCtrl;
    CButton m_cancelButton;

    std::atomic<bool> m_cancelRequested = false;
    std::atomic<size_t> m_current = 0;
    const size_t m_total = 0;
    bool m_cancelled = false;
    const bool m_noCancel = false;

    std::optional<std::jthread> m_workerThread;
    static constexpr UINT_PTR TIMER_ID = 1;
    static constexpr UINT TIMER_INTERVAL = 50; // Update every 100ms
};
