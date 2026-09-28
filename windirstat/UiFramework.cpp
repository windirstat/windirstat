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

void CWinApp::RunTaskWithUiUpdates(const std::function<void()>& task)
{
    CWnd* const mainWindow = GetMainWindow();
    if (mainWindow == nullptr || GetWindowThreadProcessId(mainWindow->m_hWnd, nullptr) != GetCurrentThreadId())
    {
        task();
        return;
    }

    static const UINT taskCompleteMessage = RegisterWindowMessageW(L"WinDirStatTaskComplete");
    const DWORD uiThreadId = GetCurrentThreadId();
    std::atomic<bool> complete = false;
    std::exception_ptr error;
    std::jthread worker([&]
    {
        try { task(); }
        catch (...) { error = std::current_exception(); }
        complete.store(true, std::memory_order_release);
        PostThreadMessageW(uiThreadId, taskCompleteMessage, 0, 0);
    });

    // Each call owns its completion state; the message only wakes the UI.
    std::optional<int> quitCode;
    MSG message{};
    while (!complete.load(std::memory_order_acquire))
    {
        const int result = GetMessageW(&message, nullptr, 0, 0);
        if (result < 0) break;
        if (result == 0) { quitCode = static_cast<int>(message.wParam); continue; }
        if (message.message == taskCompleteMessage) continue;
        if (message.message >= WM_MOUSEFIRST && message.message <= WM_MOUSELAST) continue;
        if (message.message >= WM_KEYFIRST && message.message <= WM_KEYLAST) continue;
        if (message.message == WM_NCLBUTTONDOWN || message.message == WM_NCLBUTTONUP) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    worker.join();
    if (quitCode) PostQuitMessage(*quitCode);
    if (error) std::rethrow_exception(error);
}

void CWinApp::WaitForHandleWithUiUpdates(const HANDLE handle, const DWORD timeout) noexcept
{
    for (;;)
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, WM_PAINT, WM_PAINT, PM_REMOVE)) DispatchMessageW(&message);
        if (MsgWaitForMultipleObjects(1, &handle, false, timeout, QS_PAINT) != WAIT_OBJECT_0 + 1) return;
    }
}

// -----------------------------------------------------------------------------
//  CCmdUI implementation
// -----------------------------------------------------------------------------
void CCmdUI::Enable(const bool bOn)
{
    m_bEnableChanged = true;
    if (m_menu != nullptr)
        m_menu.EnableItem(m_nIndex, MF_BYPOSITION | (bOn ? MF_ENABLED : MF_DISABLED | MF_GRAYED));
}

void CCmdUI::SetCheck(const int nCheck)
{
    if (m_menu != nullptr)
        m_menu.CheckItem(m_nIndex, MF_BYPOSITION | (nCheck ? MF_CHECKED : MF_UNCHECKED));
}

void CCmdUI::SetRadio(const bool bOn)
{
    SetCheck(bOn ? 1 : 0);
    if (m_menu == nullptr) return;

    MENUITEMINFOW info{ .cbSize = sizeof(info), .fMask = MIIM_FTYPE | MIIM_CHECKMARKS };
    if (!m_menu.GetItemInfo(m_nIndex, &info)) return;

    info.fType |= MFT_RADIOCHECK;
    info.hbmpChecked = nullptr;
    info.hbmpUnchecked = nullptr;
    m_menu.SetItemInfo(m_nIndex, &info);
}

void CCmdUI::SetText(const LPCWSTR lpszText)
{
    if (m_menu == nullptr || lpszText == nullptr) return;
    const UINT state = m_menu.GetItemState(m_nIndex, MF_BYPOSITION);
    m_menu.Modify(m_nIndex, MF_BYPOSITION | MF_STRING | (state & ~(MF_BITMAP | MF_OWNERDRAW)),
        m_nID, lpszText);
}

bool CCmdUI::Update(CCmdTarget* pTarget, const bool disableIfUnhandled)
{
    m_bEnableChanged = false;
    bool handled = pTarget != nullptr && pTarget->RouteCommand(m_nID, UpdateCode, this);
    if (!handled && disableIfUnhandled && !m_bEnableChanged)
    {
        if (pTarget != nullptr && pTarget->RouteCommand(m_nID, CommandCode, nullptr, false))
        {
            Enable(true);
            handled = true;
        }
        else
        {
            Enable(false);
        }
    }
    return handled;
}

void CFrameWnd::UpdateMenuCommands(MenuRef menu, const bool bSysMenu)
{
    if (bSysMenu || menu == nullptr || menu.m_hMenu == nullptr) return;

    CCmdUI state;
    state.m_menu = menu;
    for (int i = 0; i < menu.GetItemCount(); ++i)
    {
        const UINT id = menu.GetItemId(i);
        if (id == 0 || id == static_cast<UINT>(-1)) continue;   // separator / popup
        state.m_nIndex = static_cast<UINT>(i);
        state.m_nID = id;
        state.Update(this, true);
    }
}

LPCWSTR RegisterWindowClass(UINT classStyle, HCURSOR hCursor, HBRUSH hbrBackground, HICON hIcon)
{
    static std::map<std::wstring, std::wstring> registered;
    wchar_t name[160];
    swprintf_s(name, L"WdsWindow.%x.%p.%p.%p", classStyle, hCursor, hbrBackground, hIcon);
    if (const auto it = registered.find(name); it != registered.end()) return it->second.c_str();
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = classStyle;
    wc.lpfnWndProc = FrameworkWindowProc;
    wc.hInstance = GetAppInstance();
    wc.hCursor = hCursor ? hCursor : LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = hbrBackground;
    wc.hIcon = hIcon;
    wc.lpszClassName = name;
    ::RegisterClassExW(&wc);
    return registered.emplace(name, name).first->second.c_str();
}

bool WindowRef::CopyTextToClipboard(const std::wstring_view text) const
{
    if (!OpenClipboard(m_hWnd)) return false;
    struct ClipboardGuard final { ~ClipboardGuard() { CloseClipboard(); } } guard;
    if (!EmptyClipboard() || text.size() >= std::numeric_limits<size_t>::max() / sizeof(wchar_t)) return false;

    const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    const HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (data == nullptr) return false;

    auto* buffer = static_cast<wchar_t*>(GlobalLock(data));
    if (buffer == nullptr)
    {
        GlobalFree(data);
        return false;
    }

    if (!text.empty()) std::memcpy(buffer, text.data(), text.size() * sizeof(wchar_t));
    buffer[text.size()] = L'\0';
    GlobalUnlock(data);
    if (SetClipboardData(CF_UNICODETEXT, data) != nullptr) return true;
    GlobalFree(data);
    return false;
}

bool CWnd::InitializeDialogControls(const UINT resourceId)
{
    const HINSTANCE instance = GetAppInstance();
    const HRSRC info = FindResourceW(instance, MAKEINTRESOURCEW(resourceId), MAKEINTRESOURCEW(240));
    if (info == nullptr) return true;

    const HGLOBAL resource = LoadResource(instance, info);
    const auto* p = resource != nullptr ? static_cast<const BYTE*>(LockResource(resource)) : nullptr;
    if (p == nullptr) return false;

    const BYTE* const end = p + SizeofResource(instance, info);
    auto hasBytes = [&p, end](const size_t count) { return p <= end && std::cmp_greater_equal(end - p, count); };
    auto read = [&p, &hasBytes]<typename T>(T & value)
    {
        if (!hasBytes(sizeof(value))) return false;
        std::memcpy(&value, p, sizeof(value));
        p += sizeof(value);
        return true;
    };

    for (;;)
    {
        WORD controlId = 0;
        if (!read(controlId)) return false;
        if (controlId == 0) return true;

        WORD message = 0;
        DWORD length = 0;
        if (!read(message) || !read(length) || !hasBytes(length)) return false;

        // Visual C++ stores these in the legacy Win16 form inside RT_DLGINIT.
        if (message == 0x0401) message = LB_ADDSTRING;
        else if (message == 0x0403) message = CB_ADDSTRING;

        if (message == LB_ADDSTRING || message == CB_ADDSTRING)
        {
            if (length == 0 || std::memchr(p, '\0', length) == nullptr) return false;
            const LRESULT result = SendDlgItemMessageA(m_hWnd, controlId, message, 0, reinterpret_cast<LPARAM>(p));
            if (result == LB_ERR || result == LB_ERRSPACE) return false;
        }
        p += length;
    }
}

void WindowRef::CenterWindow(WindowRef alternate)
{
    HWND hParent = alternate != nullptr ? alternate.Handle() : ::GetParent(m_hWnd); // Use the alternate window if provided, otherwise use the parent window for centering
    if (hParent != nullptr && ::IsIconic(hParent)) hParent = nullptr; // If the parent is minimized, center relative to the desktop instead

    const HWND hTargetWnd = hParent ? hParent : m_hWnd; // Use the parent window if available, otherwise use the dialog itself for centering
    if (hParent == nullptr) hParent = ::GetDesktopWindow(); // If no parent is available, use the desktop window as the parent for centering

    CRect rcParent{}, rcWnd{};
    // Get the dimensions of the parent window and the dialog window
    if (!::GetWindowRect(hParent, &rcParent) || !::GetWindowRect(m_hWnd, &rcWnd)) return;

    // Calculate the x and y coordinates for centering the dialog within the parent window
    LONG x = rcParent.left + (rcParent.Width() - rcWnd.Width()) / 2;
    LONG y = rcParent.top + (rcParent.Height() - rcWnd.Height()) / 2;

    // If the dialog is larger than the parent window, adjust the position to ensure it is fully visible
    if (MONITORINFO mi{ sizeof(MONITORINFO) }; ::GetMonitorInfoW(::MonitorFromWindow(hTargetWnd, MONITOR_DEFAULTTONEAREST), &mi))
    {
        const CRect viewport(mi.rcWork);
        x = std::clamp<LONG>(x, viewport.left, std::max<LONG>(viewport.left, viewport.right - rcWnd.Width()));
        y = std::clamp<LONG>(y, viewport.top, std::max<LONG>(viewport.top, viewport.bottom - rcWnd.Height()));
    }

    ::SetWindowPos(m_hWnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
