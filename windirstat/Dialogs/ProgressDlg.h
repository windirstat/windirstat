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
class CWdsProgressCtrl final : public CProgressCtrl
{
public:
    CWdsProgressCtrl() = default;

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    DECLARE_MESSAGE_MAP()
};

//
// CProgressDlg - Modal progress dialog for long-running operations
// Shows progress bar and allows cancellation
//
class CProgressDlg final : public CDialogEx
{
    DECLARE_DYNAMIC(CProgressDlg)

public:
    enum class Flags : std::uint8_t { None = 0, NoCancel = 1, PercentageOnly = 2 };
    friend constexpr Flags operator|(const Flags lhs, const Flags rhs) noexcept
    {
        return static_cast<Flags>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
    }

    CProgressDlg(size_t total, Flags flags, CWnd* pParent, std::function<void(CProgressDlg*)> task);
    ~CProgressDlg() override = default;

    INT_PTR DoModal() override;
    bool WasCancelled() const noexcept { return m_cancelled; }

    // Methods for task lambda to interact with the dialog
    bool IsCancelled() const noexcept { return m_cancelRequested.load(); }
    size_t Increment() noexcept { return ++m_current; }
    size_t GetTotal() const noexcept { return m_total; }
    bool HasFlag(const Flags flag) const noexcept
    {
        return (static_cast<std::uint8_t>(m_flags) & static_cast<std::uint8_t>(flag)) != 0;
    }

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
    CWdsProgressCtrl m_progressCtrl;
    CButton m_cancelButton;

    std::atomic<bool> m_cancelRequested = false;
    std::atomic<size_t> m_current = 0;
    const size_t m_total = 0;
    const Flags m_flags = Flags::None;
    bool m_cancelled = false;

    std::optional<std::jthread> m_workerThread;
    static constexpr UINT_PTR TIMER_ID = 1;
    static constexpr UINT TIMER_INTERVAL = 50; // Update every 50ms
};
