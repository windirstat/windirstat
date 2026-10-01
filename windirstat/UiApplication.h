// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "UiControls.h"

inline HWND GetDialogOwner(WindowRef parent = {}) noexcept
{
    HWND hOwner = parent != nullptr ? parent.Handle() : GetActiveWindow();
    if (hOwner == nullptr)
    {
        if (const CWnd* pMainWnd = GetMainWindow()) hOwner = pMainWnd->Handle();
    }
    if (hOwner == nullptr) return nullptr;

    if (const HWND hRoot = GetAncestor(hOwner, GA_ROOT); hRoot != nullptr) hOwner = hRoot;
    if (const HWND hPopup = GetLastActivePopup(hOwner);
        hPopup != nullptr && IsWindowVisible(hPopup) && IsWindowEnabled(hPopup))
    {
        hOwner = hPopup;
    }
    return hOwner;
}

// -----------------------------------------------------------------------------
//  CDialog / CDialog + DDX
// -----------------------------------------------------------------------------
INT_PTR CALLBACK FrameworkDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
inline bool PreTranslateWindowTree(HWND hWndStop, MSG* pMsg);

inline thread_local std::vector<CWnd*> g_modalPreTranslateStack;
inline thread_local HHOOK g_modalPreTranslateHook = nullptr;

inline LRESULT CALLBACK ModalMessageHookProc(const int code, const WPARAM wParam, const LPARAM lParam)
{
    if (code >= 0 && lParam != 0 && !g_modalPreTranslateStack.empty())
    {
        const auto pMsg = reinterpret_cast<MSG*>(lParam);
        if (const CWnd* pDlg = g_modalPreTranslateStack.back();
            pDlg != nullptr && pDlg->Handle() != nullptr &&
            pMsg->hwnd != nullptr &&
            (pMsg->hwnd == pDlg->m_hWnd || pDlg->IsChild(pMsg->hwnd)) &&
            PreTranslateWindowTree(pDlg->m_hWnd, pMsg))
        {
            return true;
        }
    }

    return CallNextHookEx(g_modalPreTranslateHook, code, wParam, lParam);
}

class CDialog : public MessageTarget<CDialog, CWnd>
{
public:
    enum class FilePickerMode : char { Open, Save };

    explicit CDialog(const UINT nIDTemplate, WindowRef parent = {}) : m_nIDTemplate(nIDTemplate), m_parent(parent) {}

    static std::optional<std::wstring> PickFile(FilePickerMode mode, std::wstring filter, WindowRef parent = nullptr);
    static std::optional<std::wstring> PickFolder(WindowRef parent = nullptr);
    static std::vector<std::wstring> PickFolders(WindowRef parent = nullptr, bool multiSelect = true);
    static std::optional<COLORREF> PickColor(COLORREF initial, WindowRef parent = nullptr);

    virtual INT_PTR ShowModal() { return ShowModalTemplate(); }

    INT_PTR ShowModalTemplate(const DLGTEMPLATE* layout = nullptr)
    {
        if (m_hWnd != nullptr) return -1;
        struct ModalPreTranslateScope final
        {
            explicit ModalPreTranslateScope(CWnd* dialog)
            {
                if (g_modalPreTranslateHook == nullptr)
                    g_modalPreTranslateHook = SetWindowsHookExW(WH_MSGFILTER,
                        ModalMessageHookProc, nullptr, GetCurrentThreadId());
                g_modalPreTranslateStack.push_back(dialog);
            }
            ~ModalPreTranslateScope()
            {
                if (!g_modalPreTranslateStack.empty()) g_modalPreTranslateStack.pop_back();
                if (!g_modalPreTranslateStack.empty() || g_modalPreTranslateHook == nullptr) return;
                UnhookWindowsHookEx(g_modalPreTranslateHook);
                g_modalPreTranslateHook = nullptr;
            }
            ModalPreTranslateScope(const ModalPreTranslateScope&) = delete;
            ModalPreTranslateScope& operator=(const ModalPreTranslateScope&) = delete;
        };

        const HWND hParent = GetDialogOwner(m_parent);
        struct OwnerEnableScope final
        {
            HWND window; bool enabled;
            ~OwnerEnableScope() { if (IsWindow(window)) ::EnableWindow(window, enabled); }
        } ownerEnable{ hParent, hParent != nullptr && ::IsWindowEnabled(hParent) };
        const ModalPreTranslateScope modalPreTranslate(this);
        const LPARAM dialog = reinterpret_cast<LPARAM>(static_cast<CWnd*>(this));
        return layout != nullptr ? DialogBoxIndirectParamW(GetAppInstance(), layout, hParent,
            FrameworkDialogProc, dialog) : DialogBoxParamW(GetAppInstance(), MAKEINTRESOURCEW(m_nIDTemplate),
            hParent, FrameworkDialogProc, dialog);
    }

    bool PreprocessMessage(MSG* pMsg) override
    {
        if (pMsg != nullptr && pMsg->message == WM_KEYDOWN && pMsg->wParam == 'C' && IsKeyDown(VK_CONTROL))
        {
            if (const auto point = GetClientCursorPos())
            {
                wchar_t className[16]{};
                if (const auto control = ChildWindowFromPoint(*point, CWP_SKIPINVISIBLE);
                    control != nullptr && control.Handle() != m_hWnd &&
                    GetClassNameW(control.m_hWnd, className, static_cast<int>(std::size(className))) != 0 &&
                    _wcsicmp(className, WC_STATIC) == 0)
                {
                    if (const std::wstring value = control.GetText();
                        !value.empty() && CopyTextToClipboard(value)) return true;
                }
            }
        }

        return CWnd::PreprocessMessage(pMsg);
    }

    virtual bool OnInitDialog()
    {
        if (InitializeDialogControls(m_nIDTemplate)) return true;
        CloseModal(-1);
        return false;
    }
    virtual void OnOK() { CloseModal(IDOK); }
    virtual void OnCancel() { CloseModal(IDCANCEL); }
    void CloseModal(const int result) { EndDialog(m_hWnd, result); }
    HBRUSH OnCtlColor(CDC* pDC, WindowRef pWnd, UINT nCtlColor);

    static std::span<const RouteEntry> Routes()
    {
        static constexpr std::array entries
        {
            Route::Command<&OnOK>(IDOK),
            Route::Command<&OnCancel>(IDCANCEL),
            Route::Window<&OnCtlColor>(WM_CTLCOLOR),
        };
        return entries;
    }

    UINT  m_nIDTemplate = 0;
    WindowRef m_parent;
};

inline INT_PTR CALLBACK FrameworkDialogProc(HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
    // Make CurrentMessage()/the WM_CTLCOLOR thunk see the real message (needed for
    // dark-mode OnCtlColor, which derives the control type from the current message).
    const ScopedValue currentMessage(g_currentMsg, MakeMessageSnapshot(hWnd, msg, wParam, lParam));

    if (msg == WM_INITDIALOG)
    {
        const auto pDlg = static_cast<CDialog*>(reinterpret_cast<CWnd*>(lParam));
        if (pDlg == nullptr) return false;

        const bool destroyOnFailure = (GetWindowLongPtrW(hWnd, GWL_STYLE) & WS_CHILD) != 0;

        const auto abortDialog = [destroyOnFailure, hWnd]
            {
                if (destroyOnFailure) DestroyWindow(hWnd);
                else EndDialog(hWnd, -1);
            };
        if (!pDlg->Attach(hWnd))
        {
            abortDialog();
            return false;
        }

        try
        {
            InitializeDialogFontAndSize(hWnd);
            return pDlg->OnInitDialog();
        }
        catch (...)
        {
            abortDialog();
            return false;
        }
    }
    CWnd* pDlg = CWnd::FindAttached(hWnd);
    if (pDlg == nullptr) return false;
    MessageResult result;
    try { result = pDlg->RouteWindowMessage(msg, wParam, lParam); }
    catch (...) {}
    if (msg == WM_NCDESTROY)
    {
        pDlg->Detach();
        try { pDlg->PostNcDestroy(); }
        catch (...) {}
        return false;
    }
    if (result)
    {
        if ((msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC) ||
            msg == WM_COMPAREITEM || msg == WM_VKEYTOITEM || msg == WM_CHARTOITEM ||
            msg == WM_QUERYDRAGICON) return *result;
        SetWindowLongPtrW(hWnd, DWLP_MSGRESULT, *result);
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
//  Application object
// -----------------------------------------------------------------------------
class CWinApp;

inline CWinApp* g_pApp = nullptr;
inline thread_local std::function<void()> g_idleCmdUiUpdate;   // set by the frame to refresh toolbar UI

class CWinApp : public MessageTarget<CWinApp, CCmdTarget>
{
public:
    static std::span<const RouteEntry> Routes();
    CWnd* m_pMainWnd = nullptr;
    MSG m_msgCur{};
    LPWSTR m_lpCmdLine = nullptr;
    int m_nCmdShow = SW_SHOWNORMAL;

    CWinApp() { g_pApp = this; }
    ~CWinApp() override { if (g_pApp == this) g_pApp = nullptr; }

    virtual bool InitInstance() { return true; }
    virtual int ExitInstance() const noexcept { return static_cast<int>(m_msgCur.wParam); }
    virtual bool OnIdle(const LONG lCount)
    {
        if (lCount != 0) return false;
        if (g_idleCmdUiUpdate) g_idleCmdUiUpdate();
        return true;
    }
    virtual bool IsIdleMessage(const MSG* pMsg) const noexcept
    {
        return !(pMsg->message == WM_MOUSEMOVE || pMsg->message == WM_NCMOUSEMOVE ||
            pMsg->message == WM_PAINT || pMsg->message == 0x0118 /*WM_SYSTIMER*/);
    }
    virtual bool PreprocessMessage(MSG* pMsg);
    virtual bool PumpMessage();
    virtual int Run();

    static void RunTaskWithUiUpdates(const std::function<void()>& task);
    static void WaitForHandleWithUiUpdates(HANDLE handle, DWORD timeout = INFINITE) noexcept;

    void OnAppExit() const
    {
        if (m_pMainWnd != nullptr) m_pMainWnd->SendMessage(WM_CLOSE);
    }

};

// ---- message pump --------------------------------------------------------------
inline bool PreTranslateWindowTree(const HWND hWndStop, MSG* pMsg)
{
    for (HWND hWnd = pMsg->hwnd; hWnd != nullptr; hWnd = GetParent(hWnd))
    {
        if (CWnd* pWnd = CWnd::FindAttached(hWnd))
            if (pWnd->PreprocessMessage(pMsg)) return true;
        if (hWnd == hWndStop) break;
    }
    return false;
}
inline bool CWinApp::PreprocessMessage(MSG* pMsg)
{
    return PreTranslateWindowTree(m_pMainWnd ? m_pMainWnd->m_hWnd : nullptr, pMsg);
}
inline bool CWinApp::PumpMessage()
{
    if (const int result = GetMessageW(&m_msgCur, nullptr, 0, 0); result <= 0)
    {
        if (result < 0)
        {
            m_msgCur = {};
            m_msgCur.message = WM_QUIT;
            m_msgCur.wParam = static_cast<WPARAM>(-1);
        }
        return false;
    }
    if (!PreprocessMessage(&m_msgCur))
    {
        TranslateMessage(&m_msgCur);
        DispatchMessageW(&m_msgCur);
    }
    return true;
}
inline int CWinApp::Run()
{
    bool bIdle = true;
    LONG lIdleCount = 0;
    for (;;)
    {
        while (bIdle && !PeekMessageW(&m_msgCur, nullptr, 0, 0, PM_NOREMOVE))
        {
            if (!OnIdle(lIdleCount++)) bIdle = false;
        }
        do
        {
            if (!PumpMessage()) return ExitInstance();
            if (IsIdleMessage(&m_msgCur)) { bIdle = true; lIdleCount = 0; }
        } while (PeekMessageW(&m_msgCur, nullptr, 0, 0, PM_NOREMOVE));
    }
}

// ---- Application globals -------------------------------------------------------
inline CWnd* GetMainWindow() { return g_pApp ? g_pApp->m_pMainWnd : nullptr; }
inline HWND GetMainWindowHandle() noexcept
{
    const CWnd* mainWindow = GetMainWindow();
    return mainWindow != nullptr ? mainWindow->Handle() : nullptr;
}
inline HINSTANCE GetAppInstance() { return GetModuleHandleW(nullptr); }

// -----------------------------------------------------------------------------
//  Frame / control-bar / splitter constants
// -----------------------------------------------------------------------------
inline constexpr UINT WDS_PANE_ID_BASE = 0xE900;
inline constexpr UINT WDS_TOOLBAR_ID = 0xE81B;
inline constexpr UINT WDS_STATUS_BAR_ID = 0xE801;

class CSplitterWnd;

// -----------------------------------------------------------------------------
//  CFrameWnd
// -----------------------------------------------------------------------------
class CFrameWnd : public MessageTarget<CFrameWnd, CWnd>
{
public:
    ~CFrameWnd() override
    {
        if (m_hAccelTable != nullptr) DestroyAcceleratorTable(m_hAccelTable);
    }

    bool RouteCommand(const UINT nID, const int nCode, void* pExtra, const bool execute = true) override
    {
        if (const HWND focus = ::GetFocus(); focus != m_hWnd && IsChild(focus))
            for (HWND window = focus; window != nullptr && window != m_hWnd; window = ::GetParent(window))
                if (CWnd* target = FindAttached(window); target != nullptr &&
                    target->RouteCommand(nID, nCode, pExtra, execute)) return true;

        if (CWnd::RouteCommand(nID, nCode, pExtra, execute)) return true;
        if (CCmdTarget* target = GetCommandTarget();
            target != nullptr && target->RouteCommand(nID, nCode, pExtra, execute)) return true;
        return g_pApp != nullptr && g_pApp->RouteCommand(nID, nCode, pExtra, execute);
    }

    virtual bool CreateFromResource(UINT nIDResource);
    virtual void UpdateLayout();
    void SetDocumentTitle(const std::wstring_view docName = {})
    {
        const std::wstring prefix = m_strTitle.empty() ? L"WinDirStat" : m_strTitle;
        const std::wstring t = !docName.empty() ? prefix + L" - " + std::wstring(docName) : prefix;
        if (m_hWnd) SetWindowTextW(m_hWnd, t.c_str());
    }

    void SetTitle(const std::wstring_view title) { m_strTitle = title; SetText(m_strTitle); }

    void SetTopBar(CWnd* bar) { m_topBar = bar; UpdateLayout(); }
    void SetBottomBar(CWnd* bar) { m_bottomBar = bar; UpdateLayout(); }
    void UpdateMenuCommands(MenuRef menu, bool bSysMenu = false);

    static std::span<const RouteEntry> Routes();

protected:
    virtual bool OnCreateClient() { return true; }
    virtual CCmdTarget* GetCommandTarget() const { return nullptr; }

    bool PreCreateWindow(CREATESTRUCT& cs) override;
    void PostNcDestroy() override
    {
        if (CWinApp* pApp = g_pApp; pApp != nullptr && pApp->m_pMainWnd == this) pApp->m_pMainWnd = nullptr;
    }
    bool PreprocessMessage(MSG* pMsg) override;

    int OnCreate(const LPCREATESTRUCT lpcs)
    {
        if (CWnd::OnCreate(lpcs) == -1) return -1;
        return OnCreateClient() ? 0 : -1;
    }
    void OnSize(const UINT nType, const int cx, const int cy) { UpdateLayout(); CWnd::OnSize(nType, cx, cy); }
    void OnDestroy()
    {
        CWnd::OnDestroy();
        if (const CWinApp* pApp = g_pApp; pApp != nullptr && pApp->m_pMainWnd == this)
            PostQuitMessage(0);
    }
    void OnInitMenuPopup(MenuRef menu, UINT /*nIndex*/, const bool bSysMenu) { UpdateMenuCommands(menu, bSysMenu); }

    // Default handling for the standard ID_VIEW_TOOLBAR / ID_VIEW_STATUS_BAR menu commands.
    void OnToggleViewBar(UINT nID);
    void OnUpdateViewBarMenu(CCmdUI* pCmdUI) const;

private:
    HACCEL m_hAccelTable = nullptr;
    std::wstring m_strTitle;
    CWnd* m_topBar = nullptr;
    CWnd* m_bottomBar = nullptr;
};

inline bool CFrameWnd::PreprocessMessage(MSG* pMsg)
{
    if (m_hAccelTable && ::TranslateAccelerator(m_hWnd, m_hAccelTable, pMsg))
        return true;
    return CWnd::PreprocessMessage(pMsg);
}

inline bool CFrameWnd::PreCreateWindow(CREATESTRUCT& cs)
{
    if (cs.lpszName) m_strTitle = cs.lpszName;
    if (cs.lpszClass == nullptr)
        cs.lpszClass = RegisterWindowClass(CS_DBLCLKS, LoadCursorW(nullptr, IDC_ARROW),
            reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1));
    return true;
}

inline bool CFrameWnd::CreateFromResource(const UINT nIDResource)
{
    struct FrameResourceScope final
    {
        ~FrameResourceScope()
        {
            if (hMenu != nullptr && IsMenu(hMenu)) DestroyMenu(hMenu);
            if (hAccel != nullptr) DestroyAcceleratorTable(hAccel);
        }

        HMENU hMenu;
        HACCEL hAccel;
    } resources{
        LoadMenuW(GetAppInstance(), MAKEINTRESOURCEW(nIDResource)),
        LoadAcceleratorsW(GetAppInstance(), MAKEINTRESOURCEW(nIDResource))
    };

    const auto className = RegisterWindowClass(CS_DBLCLKS, LoadCursorW(nullptr, IDC_ARROW),
        reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1), LoadIconW(GetAppInstance(), MAKEINTRESOURCEW(nIDResource)));
    if (const std::wstring strTitle = LoadResourceString(nIDResource);
        !CreateEx(0, className, strTitle.empty() ? nullptr : strTitle.c_str(),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, resources.hMenu))
    {
        return false;
    }

    m_hAccelTable = resources.hAccel;
    resources.hAccel = nullptr;
    resources.hMenu = nullptr;
    UpdateLayout();
    return true;
}

inline void CFrameWnd::UpdateLayout()
{
    if (!IsWindow(m_hWnd)) return;
    const CRect rc = GetClientRect();
    int top = rc.top, bottom = rc.bottom;
    if (m_topBar != nullptr && IsWindow(m_topBar->m_hWnd) && m_topBar->IsWindowVisible())
    {
        const CSize size = m_topBar->PreferredSize();
        m_topBar->SetWindowPos(nullptr, rc.left, top, rc.Width(), size.cy, SWP_NOZORDER | SWP_NOACTIVATE);
        top += size.cy;
    }
    if (m_bottomBar != nullptr && IsWindow(m_bottomBar->m_hWnd) && m_bottomBar->IsWindowVisible())
    {
        const CSize size = m_bottomBar->PreferredSize();
        m_bottomBar->SetWindowPos(nullptr, rc.left, bottom - size.cy, rc.Width(), size.cy, SWP_NOZORDER | SWP_NOACTIVATE);
        bottom -= size.cy;
    }
    if (const auto client = GetDlgItem(WDS_PANE_ID_BASE); client != nullptr)
        client.SetWindowPos(nullptr, rc.left, top, rc.Width(), bottom - top, SWP_NOZORDER | SWP_NOACTIVATE);
}
