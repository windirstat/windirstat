// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "UiTypes.h"

class CWnd : public CCmdTarget, public WindowRef
{
public:
    CWnd() = default;
    CWnd(const CWnd&) = delete;
    CWnd& operator=(const CWnd&) = delete;
    ~CWnd() override
    {
        const HWND hWnd = m_hWnd;
        if (hWnd == nullptr || FindAttached(hWnd) != this) return;

        if (m_pfnSuper != nullptr)
        {
            if (reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hWnd, GWLP_WNDPROC)) == FrameworkWindowProc)
            {
                const auto previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hWnd, GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(m_pfnSuper)));
                if (previous == FrameworkWindowProc)
                {
                    RemovePropW(hWnd, kSuperProp());
                    m_pfnSuper = nullptr;
                    Detach();
                    return;
                }
                if (previous != nullptr)
                    SetWindowLongPtrW(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
            }
            Detach();
            return;
        }

        if (IsWindow(hWnd)) ::DestroyWindow(hWnd);
        if (m_hWnd == hWnd) Detach();
    }

    operator HWND() const noexcept { return m_hWnd; }
    HWND Handle() const noexcept { return m_hWnd; }
    virtual bool IsSplitterWindow() const noexcept { return false; }

    // ---- handle maps ----
    static CWnd* FindAttached(const HWND hWnd) noexcept
    {
        return hWnd != nullptr ? static_cast<CWnd*>(GetPropW(hWnd, kProp())) : nullptr;
    }
    template<typename T>
        requires std::derived_from<T, CWnd>
    T* GetParent() const { return static_cast<T*>(FindAttached(::GetParent(m_hWnd))); }
    using WindowRef::GetParent;
    bool Attach(const HWND hWnd)
    {
        if (hWnd == nullptr) return false;
        if (m_hWnd != nullptr) return m_hWnd == hWnd && FindAttached(hWnd) == this;
        if (const CWnd* pExisting = FindAttached(hWnd); pExisting != nullptr && pExisting != this) return false;
        if (!SetPropW(hWnd, kProp(), this)) return false;

        m_hWnd = hWnd;
        m_ownerThreadId = GetWindowThreadProcessId(hWnd, nullptr);
        return true;
    }
    HWND Detach() noexcept
    {
        const HWND h = m_hWnd;
        if (h != nullptr && FindAttached(h) == this) RemovePropW(h, kProp());
        m_hWnd = nullptr;
        m_ownerThreadId = 0;
        return h;
    }

    // ---- creation ----
    virtual bool PreCreateWindow(CREATESTRUCT& cs);
    virtual void PostNcDestroy();
    virtual bool PreprocessMessage(MSG*) { return false; }
    virtual void OnFontSizeChanged(int, int) {}
    virtual void OnAppearanceChanged() {}
    virtual void SavePersistentAttributes() const {}
    bool InitializeDialogControls(UINT resourceId);

    bool CreateEx(const DWORD dwExStyle, const LPCWSTR lpszClassName, const LPCWSTR lpszWindowName, const DWORD dwStyle,
        const int x, const int y, const int nWidth, const int nHeight, const HWND hwndParent, const HMENU nIDorHMenu, const LPVOID lpParam = nullptr)
    {
        if (m_hWnd != nullptr) return false;

        CREATESTRUCT cs{};
        cs.dwExStyle = dwExStyle; cs.lpszClass = lpszClassName; cs.lpszName = lpszWindowName;
        cs.style = dwStyle; cs.x = x; cs.y = y; cs.cx = nWidth; cs.cy = nHeight;
        cs.hwndParent = hwndParent; cs.hMenu = nIDorHMenu;
        cs.hInstance = GetAppInstance(); cs.lpCreateParams = lpParam;
        try
        {
            if (!PreCreateWindow(cs))
            {
                PostNcDestroy();
                return false;
            }
        }
        catch (...)
        {
            PostNcDestroy();
            throw;
        }
        const WindowCreationScope createScope(this);
        const HWND h = CreateWindowExW(cs.dwExStyle, cs.lpszClass, cs.lpszName, cs.style,
            cs.x, cs.y, cs.cx, cs.cy, cs.hwndParent, cs.hMenu, cs.hInstance, cs.lpCreateParams);
        if (h == nullptr)
        {
            if (!createScope.m_attached) PostNcDestroy();
            return false;
        }
        // If the window used our FrameworkWindowProc class, the creation hook already attached it.
        // Otherwise it is a system/common-control class — subclass it so our message dispatch runs
        // (this is how MFC routes WM_ERASEBKGND, custom-draw, etc. to control-derived classes).
        if (m_hWnd == nullptr)
        {
            try
            {
                if (SubclassNativeWindow(h))
                {
                    SetFont(GetAppFont(m_hWnd));
                    return true;
                }
            }
            catch (...)
            {
                ::DestroyWindow(h);
                PostNcDestroy();
                throw;
            }

            ::DestroyWindow(h);
            PostNcDestroy();
            return false;
        }
        SetFont(GetAppFont(m_hWnd));
        return true;
    }
    bool CreateEx(const DWORD dwExStyle, const LPCWSTR lpszClassName, const LPCWSTR lpszWindowName, const DWORD dwStyle,
        const RECT& rect, CWnd* pParentWnd, const UINT nID, const LPVOID lpParam = nullptr)
    {
        return CreateEx(dwExStyle, lpszClassName, lpszWindowName, dwStyle,
            rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
            pParentWnd ? pParentWnd->m_hWnd : nullptr, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(nID)), lpParam);
    }
    virtual bool Create(const LPCWSTR lpszClassName, const LPCWSTR lpszWindowName, const DWORD dwStyle,
        const RECT& rect, CWnd* pParentWnd, const UINT nID)
    {
        return CreateEx(0, lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID);
    }

    bool SubclassDlgItem(const int id, WindowRef parent)
    {
        return parent != nullptr && SubclassNativeWindow(::GetDlgItem(parent.Handle(), id));
    }

    bool SubclassNativeWindow(const HWND hWnd)
    {
        if (!IsWindow(hWnd)) return false;
        if (m_hWnd != nullptr) return m_hWnd == hWnd && FindAttached(hWnd) == this;
        if (FindAttached(hWnd) != nullptr) return false;
        if (GetPropW(hWnd, kSuperProp()) != nullptr) return false;

        SetLastError(ERROR_SUCCESS);
        const auto old = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hWnd, GWLP_WNDPROC));
        if (old == nullptr || old == FrameworkWindowProc) return false;

        SetLastError(ERROR_SUCCESS);
        const auto previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hWnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(FrameworkWindowProc)));
        if (previous == nullptr)
        {
            if (GetWindowLongPtrW(hWnd, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(FrameworkWindowProc))
                SetWindowLongPtrW(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(old));
            return false;
        }

        if (!SetPropW(hWnd, kSuperProp(), reinterpret_cast<HANDLE>(previous)) ||
            !SetPropW(hWnd, kProp(), this))
        {
            RemovePropW(hWnd, kProp());
            RemovePropW(hWnd, kSuperProp());
            SetWindowLongPtrW(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
            return false;
        }

        m_hWnd = hWnd;
        m_ownerThreadId = GetWindowThreadProcessId(hWnd, nullptr);
        m_pfnSuper = previous;
        return true;

    }

    // ---- message dispatch ----
    LRESULT DispatchWindowMessage(const UINT msg, const WPARAM wParam, const LPARAM lParam) noexcept
    {
        const ScopedValue currentMessage(g_currentMsg, MakeMessageSnapshot(m_hWnd, msg, wParam, lParam));

        LRESULT r = 0;
        try
        {
            const auto result = RouteWindowMessage(msg, wParam, lParam);
            r = result ? *result : DefWindowProc(msg, wParam, lParam);
        }
        catch (...)
        {
            if (msg == WM_PAINT) ValidateRect(m_hWnd, nullptr);
            r = msg == WM_CREATE ? -1 : 0;
        }

        if (msg == WM_NCDESTROY)
        {
            RemovePropW(m_hWnd, kSuperProp());
            Detach();
            m_pfnSuper = nullptr;
            try { PostNcDestroy(); }
            catch (...) {}
        }
        return r;
    }
    MessageResult RouteWindowMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    virtual bool OnCommand(WPARAM wParam, LPARAM lParam);
    virtual MessageResult OnNotify(WPARAM wParam, LPARAM lParam);
    bool RouteReflectedCommand(int code);
    MessageResult RouteReflectedNotification(NMHDR* pNMHDR);

    LRESULT DefWindowProc(const UINT msg, const WPARAM wParam, const LPARAM lParam) const
    {
        return m_pfnSuper ? CallWindowProcW(m_pfnSuper, m_hWnd, msg, wParam, lParam) :
            ::DefWindowProcW(m_hWnd, msg, wParam, lParam);
    }
    LRESULT CallDefaultHandler() { return DefWindowProc(g_currentMsg.message, g_currentMsg.wParam, g_currentMsg.lParam); }
    static const MSG& CurrentMessage() noexcept { return g_currentMsg; }

    // Like SendMessage but, when we have subclassed this window, calls the previous wndproc
    // On the window's own thread: bypass SendMessage dispatch via CallWindowProcW (fast path).
    // From any other thread: use SendMessage so the owning thread's message loop services it,
    // matching real MFC's CWnd::SendMessage which always goes through ::SendMessage.
    LRESULT SendNativeMessage(const UINT msg, const WPARAM wParam = 0, const LPARAM lParam = 0) const
    {
        return m_pfnSuper && m_ownerThreadId == GetCurrentThreadId() ?
            CallWindowProcW(m_pfnSuper, m_hWnd, msg, wParam, lParam) :
            ::SendMessageW(m_hWnd, msg, wParam, lParam);
    }
    template <typename T>
    LRESULT SendNativeMessage(const UINT msg, const WPARAM wParam, T* lParam) const
    {
        return SendNativeMessage(msg, wParam, reinterpret_cast<LPARAM>(lParam));
    }

    // ---- default message handlers (call CallDefaultHandler()) ----
    int  OnCreate(LPCREATESTRUCT) { return static_cast<int>(CallDefaultHandler()); }
    void OnDestroy() { CallDefaultHandler(); }
    void OnPaint() { CallDefaultHandler(); }
    void OnClose() { CallDefaultHandler(); }
    void OnNcPaint() { CallDefaultHandler(); }
    void OnSize(UINT, int, int) { CallDefaultHandler(); }
    bool OnEraseBkgnd(CDC*) { return static_cast<bool>(CallDefaultHandler()); }
    void OnLButtonDown(UINT, CPoint) { CallDefaultHandler(); }
    void OnLButtonUp(UINT, CPoint) { CallDefaultHandler(); }
    void OnLButtonDblClk(UINT, CPoint) { CallDefaultHandler(); }
    void OnMouseMove(UINT, CPoint) { CallDefaultHandler(); }
    bool OnMouseWheel(UINT, short, CPoint) { return static_cast<bool>(CallDefaultHandler()); }
    void OnKeyDown(UINT, UINT, UINT) { CallDefaultHandler(); }
    void OnChar(UINT, UINT, UINT) { CallDefaultHandler(); }
    void OnSetFocus(WindowRef) { CallDefaultHandler(); }
    void OnKillFocus(WindowRef) { CallDefaultHandler(); }
    void OnContextMenu(WindowRef, CPoint) { CallDefaultHandler(); }
    void OnTimer(UINT_PTR) { CallDefaultHandler(); }
    void OnSysColorChange() { CallDefaultHandler(); }
    bool OnNcActivate(bool) { return static_cast<bool>(CallDefaultHandler()); }
    HBRUSH OnCtlColor(CDC*, WindowRef, UINT) { return reinterpret_cast<HBRUSH>(CallDefaultHandler()); }
    LRESULT OnNcHitTest(CPoint) { return CallDefaultHandler(); }
    void OnGetMinMaxInfo(MINMAXINFO*) { CallDefaultHandler(); }
    void OnEnable(bool) { CallDefaultHandler(); }
    bool OnSetCursor(WindowRef, UINT, UINT) { return static_cast<bool>(CallDefaultHandler()); }
    void OnCaptureChanged(WindowRef) { CallDefaultHandler(); }
    void OnSettingChange(UINT, LPCTSTR) { CallDefaultHandler(); }
    void OnShowWindow(bool, UINT) { CallDefaultHandler(); }
    void OnHScroll(UINT, UINT, WindowRef) { CallDefaultHandler(); }
    void OnVScroll(UINT, UINT, WindowRef) { CallDefaultHandler(); }
    void OnNcCalcSize(bool, NCCALCSIZE_PARAMS*) { CallDefaultHandler(); }
    virtual int OnMouseActivate(WindowRef, UINT, UINT) { return static_cast<int>(CallDefaultHandler()); }
    bool IsTopParentActive() const noexcept
    {
        HWND hWnd = m_hWnd;
        while (hWnd != nullptr && (GetWindowLongW(hWnd, GWL_STYLE) & WS_CHILD))
            hWnd = ::GetParent(hWnd);
        return hWnd != nullptr && hWnd == GetActiveWindow();
    }

    // owner-draw reflection targets
    virtual void DrawItem(LPDRAWITEMSTRUCT) {}
    // docked-bar sizing (frame layout queries this)
    virtual CSize PreferredSize() const { return GetWindowRect().Size(); }

protected:
    WNDPROC m_pfnSuper = nullptr;
    DWORD   m_ownerThreadId = 0;
    static LPCWSTR kProp() { return L"_WdsShimCWnd"; }
    static LPCWSTR kSuperProp() { return L"_WdsShimSuperProc"; }
    friend LRESULT CALLBACK FrameworkWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

inline WindowRef::WindowRef(const CWnd* window) noexcept
    : m_hWnd(window != nullptr ? window->Handle() : nullptr) {}

void NotifyAppearanceChanged(HWND window);

// ---- Device-context wrappers tied to a window ----
class CClientDC final : public CDC
{
public:
    explicit CClientDC(WindowRef window)
        : CDC(GetDC(window.Handle()), false), m_hWndDC(window.Handle()) {}
    ~CClientDC() { if (m_hDC) ReleaseDC(m_hWndDC, Detach()); }
private:
    HWND m_hWndDC;
};
class CWindowDC final : public CDC
{
public:
    explicit CWindowDC(WindowRef window)
        : CDC(GetWindowDC(window.Handle()), false), m_hWndDC(window.Handle()) {}
    ~CWindowDC() { if (m_hDC) ReleaseDC(m_hWndDC, Detach()); }
private:
    HWND m_hWndDC;
};
class CPaintDC final : public CDC
{
public:
    PAINTSTRUCT m_ps{};
    explicit CPaintDC(WindowRef window) : m_hWndDC(window.Handle()) { Attach(BeginPaint(m_hWndDC, &m_ps)); }
    ~CPaintDC() { if (m_hDC) { EndPaint(m_hWndDC, &m_ps); Detach(); } }
private:
    HWND m_hWndDC;
};

// -----------------------------------------------------------------------------
//  CMenu
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
//  Type-safe message handlers (defined now that CDC/CWnd/CMenu exist)
// -----------------------------------------------------------------------------
template<typename T> struct MemberFunctionTraits;
template<typename Return, typename Class, typename... Args>
struct MemberFunctionTraits<Return(Class::*)(Args...)>
{
    using ClassType = Class;
    using Signature = Return(Args...);
};
template<typename Return, typename Class, typename... Args>
struct MemberFunctionTraits<Return(Class::*)(Args...) const>
    : MemberFunctionTraits<Return(Class::*)(Args...)>
{};
template<typename Return, typename Class, typename... Args>
struct MemberFunctionTraits<Return(Class::*)(Args...) noexcept>
    : MemberFunctionTraits<Return(Class::*)(Args...)>
{};
template<typename Return, typename Class, typename... Args>
struct MemberFunctionTraits<Return(Class::*)(Args...) const noexcept>
    : MemberFunctionTraits<Return(Class::*)(Args...)>
{};

template<typename> inline constexpr bool InvalidMessageHandler = false;

template<auto Handler>
MessageResult InvokeCommandHandler(CCmdTarget& target, UINT, const WPARAM wParam, LPARAM)
{
    using Traits = MemberFunctionTraits<decltype(Handler)>;
    using Class = Traits::ClassType;
    using Signature = Traits::Signature;
    auto& object = static_cast<Class&>(target);

    if constexpr (std::is_same_v<Signature, void()>) std::invoke(Handler, object);
    else if constexpr (std::is_same_v<Signature, void(UINT)>) std::invoke(Handler, object, static_cast<UINT>(wParam));
    else static_assert(InvalidMessageHandler<Signature>, "Unsupported command-handler signature");
    return 0;
}

template<auto Handler>
MessageResult InvokeUpdateHandler(CCmdTarget& target, UINT, WPARAM, const LPARAM lParam)
{
    using Class = MemberFunctionTraits<decltype(Handler)>::ClassType;
    static_assert(std::is_same_v<typename MemberFunctionTraits<decltype(Handler)>::Signature, void(CCmdUI*)>);
    std::invoke(Handler, static_cast<Class&>(target), reinterpret_cast<CCmdUI*>(lParam));
    return 0;
}

template<auto Handler>
MessageResult InvokeNotifyHandler(CCmdTarget& target, UINT, WPARAM, const LPARAM lParam)
{
    using Traits = MemberFunctionTraits<decltype(Handler)>;
    using Class = Traits::ClassType;
    using Signature = Traits::Signature;
    auto& object = static_cast<Class&>(target);
    auto* notify = reinterpret_cast<NMHDR*>(lParam);
    LRESULT result = 0;

    if constexpr (std::is_same_v<Signature, void()>) std::invoke(Handler, object);
    else if constexpr (std::is_same_v<Signature, void(NMHDR*, LRESULT*)>)
        std::invoke(Handler, object, notify, &result);
    else if constexpr (std::is_same_v<Signature, void(UINT, NMHDR*, LRESULT*)>)
        std::invoke(Handler, object, static_cast<UINT>(notify->idFrom), notify, &result);
    else if constexpr (std::is_same_v<Signature, bool(UINT, NMHDR*, LRESULT*)>)
    {
        if (!std::invoke(Handler, object, static_cast<UINT>(notify->idFrom), notify, &result)) return {};
    }
    else
        static_assert(InvalidMessageHandler<Signature>, "Unsupported notification-handler signature");
    return result;
}

template<auto Handler>
MessageResult InvokeWindowHandler(CCmdTarget& target, const UINT message, WPARAM wParam, LPARAM lParam)
{
    using Traits = MemberFunctionTraits<decltype(Handler)>;
    using Class = Traits::ClassType;
    using Signature = Traits::Signature;
    auto& object = static_cast<Class&>(target);
    LRESULT result = 0;

    if constexpr (std::is_same_v<Signature, void()>)
        std::invoke(Handler, object);
    else if constexpr (std::is_same_v<Signature, int(LPCREATESTRUCT)>)
        result = std::invoke(Handler, object, reinterpret_cast<LPCREATESTRUCT>(lParam));
    else if constexpr (std::is_same_v<Signature, void(UINT, int, int)>)
        std::invoke(Handler, object, static_cast<UINT>(wParam), static_cast<int>(LOWORD(lParam)),
            static_cast<int>(HIWORD(lParam)));
    else if constexpr (std::is_same_v<Signature, bool(CDC*)>)
    {
        auto dc = CDC::Borrow(reinterpret_cast<HDC>(wParam));
        result = std::invoke(Handler, object, &dc);
    }
    else if constexpr (std::is_same_v<Signature, void(UINT, CPoint)>)
        std::invoke(Handler, object, static_cast<UINT>(wParam),
            CPoint(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))));
    else if constexpr (std::is_same_v<Signature, bool(UINT, short, CPoint)>)
    {
        const bool handledResult = std::invoke(Handler, object, LOWORD(wParam), static_cast<short>(HIWORD(wParam)),
            CPoint(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))));
        result = handledResult;
    }
    else if constexpr (std::is_same_v<Signature, void(UINT, UINT, UINT)>)
        std::invoke(Handler, object, static_cast<UINT>(wParam), static_cast<UINT>(lParam & 0xFFFF),
            static_cast<UINT>((lParam >> 16) & 0xFFFF));
    else if constexpr (std::is_same_v<Signature, void(WindowRef)>)
    {
        const auto hwnd = reinterpret_cast<HWND>(message == WM_CAPTURECHANGED ? lParam : wParam);
        std::invoke(Handler, object, WindowRef(hwnd));
    }
    else if constexpr (std::is_same_v<Signature, void(WindowRef, CPoint)>)
        std::invoke(Handler, object, WindowRef(reinterpret_cast<HWND>(wParam)),
            CPoint(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))));
    else if constexpr (std::is_same_v<Signature, void(UINT_PTR)>)
        std::invoke(Handler, object, static_cast<UINT_PTR>(wParam));
    else if constexpr (std::is_same_v<Signature, void(MenuRef, UINT, bool)>)
        std::invoke(Handler, object, MenuRef(reinterpret_cast<HMENU>(wParam)),
            static_cast<UINT>(LOWORD(lParam)), static_cast<bool>(HIWORD(lParam)));
    else if constexpr (std::is_same_v<Signature, UINT(UINT, LPARAM)>)
        result = static_cast<LRESULT>(std::invoke(Handler, object, static_cast<UINT>(wParam), lParam));
    else if constexpr (std::is_same_v<Signature, bool(bool)>)
        result = static_cast<LRESULT>(std::invoke(Handler, object, static_cast<bool>(wParam)));
    else if constexpr (std::is_same_v<Signature, HBRUSH(CDC*, WindowRef, UINT)>)
    {
        auto dc = CDC::Borrow(reinterpret_cast<HDC>(wParam));
        const HBRUSH brush = std::invoke(Handler, object, &dc,
            WindowRef(reinterpret_cast<HWND>(lParam)), message - WM_CTLCOLORMSGBOX);
        if (brush == nullptr) return {};
        result = reinterpret_cast<LRESULT>(brush);
    }
    else if constexpr (std::is_same_v<Signature, LRESULT(CPoint)>)
        result = std::invoke(Handler, object,
            CPoint(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))));
    else if constexpr (std::is_same_v<Signature, void(MINMAXINFO*)>)
        std::invoke(Handler, object, reinterpret_cast<MINMAXINFO*>(lParam));
    else if constexpr (std::is_same_v<Signature, void(bool)>)
        std::invoke(Handler, object, static_cast<bool>(wParam));
    else if constexpr (std::is_same_v<Signature, bool(WindowRef, UINT, UINT)>)
    {
        const bool handledResult = std::invoke(Handler, object, WindowRef(reinterpret_cast<HWND>(wParam)),
            LOWORD(lParam), HIWORD(lParam));
        result = handledResult;
    }
    else if constexpr (std::is_same_v<Signature, void(bool, DWORD)>)
        std::invoke(Handler, object, static_cast<bool>(wParam), static_cast<DWORD>(lParam));
    else if constexpr (std::is_same_v<Signature, void(bool, UINT)>)
        std::invoke(Handler, object, static_cast<bool>(wParam), static_cast<UINT>(lParam));
    else if constexpr (std::is_same_v<Signature, void(UINT, LPCTSTR)>)
        std::invoke(Handler, object, static_cast<UINT>(wParam), reinterpret_cast<LPCTSTR>(lParam));
    else if constexpr (std::is_same_v<Signature, void(UINT, UINT, WindowRef)>)
    {
        WindowRef scrollBar = lParam ? WindowRef(reinterpret_cast<HWND>(lParam)) : nullptr;
        std::invoke(Handler, object, LOWORD(wParam), HIWORD(wParam), scrollBar);
    }
    else if constexpr (std::is_same_v<Signature, UINT()>)
        result = static_cast<LRESULT>(std::invoke(Handler, object));
    else if constexpr (std::is_same_v<Signature, void(bool, NCCALCSIZE_PARAMS*)>)
        std::invoke(Handler, object, static_cast<bool>(wParam), reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam));
    else if constexpr (std::is_same_v<Signature, int(WindowRef, UINT, UINT)>)
        result = static_cast<LRESULT>(std::invoke(Handler, object,
            WindowRef(reinterpret_cast<HWND>(wParam)), LOWORD(lParam), HIWORD(lParam)));
    else if constexpr (std::is_same_v<Signature, LRESULT(WPARAM, LPARAM)>)
        result = std::invoke(Handler, object, wParam, lParam);
    else
        static_assert(InvalidMessageHandler<Signature>, "Unsupported window-message handler signature");

    return result;
}

namespace Route
{
    template<auto Handler>
    constexpr RouteEntry Control(UINT code, UINT firstId, UINT lastId);

    template<auto Handler>
    constexpr RouteEntry Command(const UINT firstId, const UINT lastId)
    {
        return Control<Handler>(static_cast<UINT>(CommandCode), firstId, lastId);
    }

    template<auto Handler>
    constexpr RouteEntry Command(const UINT id)
    {
        return Command<Handler>(id, id);
    }

    template<auto Handler>
    constexpr RouteEntry Update(const UINT firstId, const UINT lastId)
    {
        return { WM_COMMAND, static_cast<UINT>(UpdateCode), firstId, lastId,
            &InvokeUpdateHandler<Handler>, nullptr };
    }

    template<auto Handler>
    constexpr RouteEntry Update(const UINT id)
    {
        return Update<Handler>(id, id);
    }

    template<auto Handler>
    constexpr RouteEntry Control(const UINT code, const UINT firstId, const UINT lastId)
    {
        return { WM_COMMAND, code, firstId, lastId, &InvokeCommandHandler<Handler>, nullptr };
    }

    template<auto Handler>
    constexpr RouteEntry Control(const UINT code, const UINT id)
    {
        return Control<Handler>(code, id, id);
    }

    template<auto Handler>
    constexpr RouteEntry Window(const UINT message)
    {
        return { message, 0, 0, 0, &InvokeWindowHandler<Handler>, nullptr };
    }

    template<auto Handler>
    constexpr RouteEntry Registered(const UINT& message)
    {
        return { 0, 0, 0, 0, &InvokeWindowHandler<Handler>, &message };
    }

    template<auto Handler>
    constexpr RouteEntry Notify(const UINT code, const UINT firstId, const UINT lastId)
    {
        return { WM_NOTIFY, code, firstId, lastId, &InvokeNotifyHandler<Handler>, nullptr };
    }

    template<auto Handler>
    constexpr RouteEntry Notify(const UINT code, const UINT id)
    {
        return Notify<Handler>(code, id, id);
    }

    template<auto Handler>
    constexpr RouteEntry ReflectNotify(const UINT code)
    {
        return Notify<Handler>(code, ReflectedId, ReflectedId);
    }

    template<auto Handler>
    constexpr RouteEntry ReflectControl(const UINT code)
    {
        return Control<Handler>(code, ReflectedId, ReflectedId);
    }
}

// -----------------------------------------------------------------------------
//  Dispatch implementations
// -----------------------------------------------------------------------------
inline bool CCmdTarget::RouteCommand(const UINT nID, const int nCode, void* pExtra, const bool execute)
{
    for (const RouteTable* table = GetRouteTable(); table != nullptr; table = table->base)
    {
        for (const RouteEntry& entry : table->entries)
        {
            if (entry.message != WM_COMMAND) continue;
            if (entry.firstId == ReflectedId) continue;
            if (entry.code != static_cast<UINT>(nCode)) continue;
            if (nID < entry.firstId || nID > entry.lastId) continue;
            if (!execute) return true;
            if (entry.invoke(*this, WM_COMMAND, static_cast<WPARAM>(nID),
                reinterpret_cast<LPARAM>(pExtra))) return true;
        }
    }
    return false;
}

inline MessageResult CWnd::RouteWindowMessage(const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
    if (msg == WM_SETCURSOR && g_waitCursorActive)
    {
        SetCursor(LoadCursorW(nullptr, IDC_WAIT));
        return TRUE;
    }
    if (msg == WM_COMMAND)
    {
        return OnCommand(wParam, lParam) ? MessageResult(0) : std::nullopt;
    }
    if (msg == WM_NOTIFY)
    {
        return OnNotify(wParam, lParam);
    }
    if (msg == WM_DRAWITEM)
    {
        if (auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam); dis->CtlType != ODT_MENU && dis->hwndItem)
            if (CWnd* pChild = FindAttached(dis->hwndItem))
            {
                // Publish item rect so GetItemRect/GetSubItemRect can avoid SendMessage.
                DrawItemContext context{};
                context.hWnd = dis->hwndItem;
                context.item = static_cast<int>(dis->itemID);
                context.rcItem = dis->rcItem;
                const ScopedValue drawContext(g_drawItemCtx, context);
                pChild->DrawItem(dis);
                return TRUE;
            }
    }

    if (msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC)
    {
        for (const RouteTable* table = GetRouteTable(); table != nullptr; table = table->base)
            for (const RouteEntry& entry : table->entries)
            {
                if (entry.message != WM_CTLCOLOR) continue;
                if (const auto result = entry.invoke(*this, msg, wParam, lParam)) return result;
            }
        return {};
    }
    for (const RouteTable* table = GetRouteTable(); table != nullptr; table = table->base)
        for (const RouteEntry& entry : table->entries)
        {
            bool match;
            if (entry.registeredMessage) match = (*entry.registeredMessage != 0 && *entry.registeredMessage == msg);
            else
                match = entry.message == msg && entry.message != WM_COMMAND && entry.message != WM_NOTIFY &&
                entry.message != WM_CTLCOLOR;
            if (!match) continue;
            if (const auto result = entry.invoke(*this, msg, wParam, lParam)) return result;
        }
    return {};
}

inline bool CWnd::OnCommand(const WPARAM wParam, const LPARAM lParam)
{
    const UINT nID = LOWORD(wParam);
    const auto hWndCtrl = reinterpret_cast<HWND>(lParam);
    const int nCode = hWndCtrl == nullptr ? CommandCode : HIWORD(wParam);
    if (hWndCtrl != nullptr)
    {
        if (CWnd* pCtrl = FindAttached(hWndCtrl);
            pCtrl && pCtrl != this && pCtrl->RouteReflectedCommand(nCode)) return true;
    }
    if (nID == 0) return false;

    if (hWndCtrl == nullptr)
    {
        struct CCommandState final : CCmdUI
        {
            bool m_bEnabled = true;
            void Enable(const bool bOn) override
            {
                m_bEnableChanged = true;
                m_bEnabled = bOn;
            }
        } state;
        state.m_nID = nID;
        if (RouteCommand(nID, UpdateCode, &state) &&
            state.m_bEnableChanged && !state.m_bEnabled)
        {
            return true;
        }
    }

    if (RouteCommand(nID, nCode, nullptr)) return true;

    return false;
}

inline bool CWnd::RouteReflectedCommand(const int code)
{
    for (const RouteTable* table = GetRouteTable(); table != nullptr; table = table->base)
        for (const RouteEntry& entry : table->entries)
            if (entry.message == WM_COMMAND && entry.firstId == ReflectedId && std::cmp_equal(entry.code, code))
            {
                if (entry.invoke(*this, WM_COMMAND, 0, 0)) return true;
            }
    return false;
}

inline MessageResult CWnd::OnNotify(WPARAM, const LPARAM lParam)
{
    const auto pNMHDR = reinterpret_cast<NMHDR*>(lParam);
    if (pNMHDR == nullptr) return {};
    if (pNMHDR->hwndFrom)
    {
        if (CWnd* control = FindAttached(pNMHDR->hwndFrom); control != nullptr && control != this)
            if (const auto result = control->RouteReflectedNotification(pNMHDR)) return result;
    }
    for (const RouteTable* table = GetRouteTable(); table != nullptr; table = table->base)
        for (const RouteEntry& entry : table->entries)
        {
            if (entry.message != WM_NOTIFY || entry.firstId == ReflectedId) continue;
            if (entry.code != pNMHDR->code) continue;
            const UINT idFrom = static_cast<UINT>(pNMHDR->idFrom);
            if (idFrom < entry.firstId || idFrom > entry.lastId) continue;
            if (const auto result = entry.invoke(*this, WM_NOTIFY, 0, lParam)) return result;
        }
    return {};
}

inline MessageResult CWnd::RouteReflectedNotification(NMHDR* pNMHDR)
{
    for (const RouteTable* table = GetRouteTable(); table != nullptr; table = table->base)
        for (const RouteEntry& entry : table->entries)
            if (entry.message == WM_NOTIFY && entry.firstId == ReflectedId && entry.code == pNMHDR->code)
            {
                if (const auto result = entry.invoke(*this, WM_NOTIFY, 0,
                    reinterpret_cast<LPARAM>(pNMHDR))) return result;
            }
    return {};
}

inline LRESULT CALLBACK FrameworkWindowProc(const HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
    CWnd* pWnd = CWnd::FindAttached(hWnd);
    if (pWnd == nullptr && msg == WM_NCCREATE && g_pWndInit != nullptr && !g_pWndInit->m_attached &&
        g_pWndInit->m_pWnd->m_hWnd == nullptr)
    {
        pWnd = g_pWndInit->m_pWnd;
        if (!pWnd->Attach(hWnd)) return false;
        g_pWndInit->m_attached = true;
    }
    if (pWnd == nullptr)
    {
        const auto super = reinterpret_cast<WNDPROC>(GetPropW(hWnd, CWnd::kSuperProp()));
        if (super == nullptr) return DefWindowProcW(hWnd, msg, wParam, lParam);

        const LRESULT result = CallWindowProcW(super, hWnd, msg, wParam, lParam);
        if (msg == WM_NCDESTROY) RemovePropW(hWnd, CWnd::kSuperProp());
        return result;
    }
    return pWnd->DispatchWindowMessage(msg, wParam, lParam);
}

inline bool CWnd::PreCreateWindow(CREATESTRUCT& cs)
{
    if (cs.lpszClass == nullptr)
    {
        static const LPCWSTR defaultClass = RegisterWindowClass(CS_DBLCLKS);
        cs.lpszClass = defaultClass;
    }
    return true;
}
inline void CWnd::PostNcDestroy() {}
