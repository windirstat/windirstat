// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
class CWdsProgressCtrl final : public MessageTarget<CWdsProgressCtrl, CProgressCtrl>
{
public:
    CWdsProgressCtrl() = default;

protected:
    void OnPaint();
    bool OnEraseBkgnd(CDC*) { return true; }
public:
    static std::span<const RouteEntry> Routes();

};

//
// CProgressDlg - Modal progress dialog for long-running operations
// Shows progress bar and allows cancellation
//
class CProgressDlg final : public MessageTarget<CProgressDlg, CDialog>
{
public:
    enum class Flags : std::uint8_t { None = 0, NoCancel = 1, PercentageOnly = 2 };
    friend constexpr Flags operator|(const Flags lhs, const Flags rhs) noexcept
    {
        return static_cast<Flags>(std::to_underlying(lhs) | std::to_underlying(rhs));
    }

    CProgressDlg(ULONGLONG total, Flags flags, CWnd* pParent, std::function<void(CProgressDlg*)> task);
    ~CProgressDlg() override = default;

    INT_PTR ShowModal() override;
    bool WasCancelled() const noexcept { return m_cancelRequested.load(); }

    // Methods for task lambda to interact with the dialog
    bool IsCancelled() const noexcept { return m_cancelRequested.load(); }
    ULONGLONG Increment() noexcept { return ++m_current; }
    void SetProgress(const ULONGLONG current) noexcept { m_current = current; }
    ULONGLONG GetTotal() const noexcept { return m_total; }
    bool HasFlag(const Flags flag) const noexcept
    {
        return (std::to_underlying(m_flags) & std::to_underlying(flag)) != 0;
    }

protected:
    enum : std::uint8_t { IDD = IDD_PROGRESS };

    bool OnInitDialog() override;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnTimer(UINT_PTR nIDEvent);
    void OnCancel() override;

private:
    void StartWorkerThread();

    std::wstring m_message;
    std::function<void(CProgressDlg*)> m_task;

    CStatic m_messageCtrl;
    CWdsProgressCtrl m_progressCtrl;
    CButton m_cancelButton;

    std::atomic<bool> m_cancelRequested = false;
    std::atomic<ULONGLONG> m_current = 0;
    const ULONGLONG m_total = 0;
    const Flags m_flags = Flags::None;

    std::jthread m_workerThread;
    static constexpr UINT_PTR TIMER_ID = 1;
    static constexpr UINT TIMER_INTERVAL = 50; // Update every 50ms
};

inline std::span<const RouteEntry> CProgressDlg::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnTimer>(WM_TIMER),
        Route::Control<&OnCancel>(BN_CLICKED, IDCANCEL),
    };
    return entries;
}

inline std::span<const RouteEntry> CWdsProgressCtrl::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnPaint>(WM_PAINT),
        Route::Window<&OnEraseBkgnd>(WM_ERASEBKGND),
    };
    return entries;
}
