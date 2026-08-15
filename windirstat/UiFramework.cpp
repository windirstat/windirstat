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

static int ReadWindowsTextScalePercent() noexcept
{
    DWORD percent = 100;
    if (CRegKey key; key.Open(HKEY_CURRENT_USER, wds::strAccessibilityKey, KEY_READ) == ERROR_SUCCESS)
        key.QueryDWORDValue(L"TextScaleFactor", percent);
    return static_cast<int>(std::clamp<DWORD>(percent, 100, 225));
}

int ResolveTextScalePercent(const int configuredPercent) noexcept
{
    return configuredPercent != 0 ? std::clamp(configuredPercent, 100, 200) :
        std::min(ReadWindowsTextScalePercent(), 200);
}

HFONT GetAppFont(const HWND window)
{
    const int dpi = GetWindowDpi(window);
    const int percent = GetFontSizePercent();
    const std::pair key(dpi, percent);
    static std::map<std::pair<int, int>, CFont> fonts;
    if (const auto found = fonts.find(key); found != fonts.end()) return found->second;

    NONCLIENTMETRICSW metrics{ .cbSize = sizeof(metrics) };
    using SystemParametersInfoForDpiFn = BOOL(WINAPI*)(UINT, UINT, PVOID, UINT, UINT);
    static const auto systemParametersInfoForDpi = reinterpret_cast<SystemParametersInfoForDpiFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SystemParametersInfoForDpi"));
    const bool dpiAdjusted = systemParametersInfoForDpi != nullptr &&
        systemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi);
    const bool systemMetrics = dpiAdjusted ||
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
    if (!systemMetrics)
    {
        LOGFONTW fallback{};
        GetObjectW(GetStockObject(DEFAULT_GUI_FONT), sizeof(fallback), &fallback);
        metrics.lfMessageFont = fallback;
    }
    else if (!dpiAdjusted)
    {
        const int systemDpi = GetWindowDpi(nullptr);
        metrics.lfMessageFont.lfHeight = MulDiv(metrics.lfMessageFont.lfHeight, dpi, systemDpi);
        metrics.lfMessageFont.lfWidth = MulDiv(metrics.lfMessageFont.lfWidth, dpi, systemDpi);
    }

    const int sourcePercent = systemMetrics ? ReadWindowsTextScalePercent() : 100;
    metrics.lfMessageFont.lfHeight = MulDiv(metrics.lfMessageFont.lfHeight, percent, sourcePercent);
    metrics.lfMessageFont.lfWidth = MulDiv(metrics.lfMessageFont.lfWidth, percent, sourcePercent);
    return fonts.try_emplace(key, metrics.lfMessageFont).first->second;
}

static BOOL CALLBACK SetAppFontCallback(const HWND window, LPARAM) noexcept
{
    try
    {
        SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(GetAppFont(window)), false);
    }
    catch (...) {}
    return true;
}

static BOOL CALLBACK NotifyFontSizeChangedCallback(const HWND window, const LPARAM oldPercent) noexcept
{
    try
    {
        if (CWnd* attached = CWnd::FindAttached(window))
            attached->OnFontSizeChanged(static_cast<int>(oldPercent), GetFontSizePercent());
    }
    catch (...) {}
    return true;
}

void ApplyAppFont(const HWND window, const int oldPercent)
{
    if (!IsWindow(window)) return;
    SetAppFontCallback(window, 0);
    EnumChildWindows(window, SetAppFontCallback, 0);
    if (oldPercent == 0) return;
    NotifyFontSizeChangedCallback(window, oldPercent);
    EnumChildWindows(window, NotifyFontSizeChangedCallback, oldPercent);
}

void InitializeDialogFontAndSize(const HWND dialog)
{
    if (!IsWindow(dialog)) return;

    const int percent = GetFontSizePercent();
    if (percent != 100)
    {
        std::vector<std::pair<HWND, CRect>> controls;
        for (HWND control = GetWindow(dialog, GW_CHILD); control != nullptr;
            control = GetWindow(control, GW_HWNDNEXT))
        {
            CRect rect(control);
            MapWindowPoints(nullptr, dialog, reinterpret_cast<LPPOINT>(&rect), 2);
            controls.emplace_back(control, rect);
        }

        const CRect windowRect(dialog);
        CRect clientRect;
        GetClientRect(dialog, &clientRect);
        const bool childDialog = (GetWindowLongPtrW(dialog, GWL_STYLE) & WS_CHILD) != 0;
        const int width = childDialog ? MulDiv(windowRect.Width(), percent, 100) :
            windowRect.Width() + MulDiv(clientRect.Width(), percent, 100) - clientRect.Width();
        const int height = childDialog ? MulDiv(windowRect.Height(), percent, 100) :
            windowRect.Height() + MulDiv(clientRect.Height(), percent, 100) - clientRect.Height();
        SetWindowPos(dialog, nullptr, 0, 0, width, height,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

        HDWP positions = BeginDeferWindowPos(static_cast<int>(controls.size()));
        for (const auto& [control, rect] : controls)
        {
            positions = DeferWindowPos(positions, control, nullptr,
                MulDiv(rect.left, percent, 100), MulDiv(rect.top, percent, 100),
                MulDiv(rect.Width(), percent, 100), MulDiv(rect.Height(), percent, 100),
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        if (positions != nullptr) EndDeferWindowPos(positions);
    }

    ApplyAppFont(dialog);
}

void CDC::DrawTreeConnector(const CRect& nodeRect, const COLORREF background, const bool toTop,
    const bool toBottom, const bool toRight, const bool showPlus, const bool showMinus)
{
    const int centerX = nodeRect.left + nodeRect.Width() / 2;
    const int centerY = nodeRect.top + nodeRect.Height() / 2;
    const COLORREF lineColor = DarkMode::IsDarkModeActive() ? RGB(160, 160, 160) : RGB(96, 96, 96);
    const LOGBRUSH connectorBrush{ BS_SOLID, lineColor, 0 };
    const CPen connectorPen(PS_GEOMETRIC | PS_DOT, 1, &connectorBrush);
    GdiObjectSelection selectConnector(this, &connectorPen);

    if (toBottom && toTop) MoveTo(centerX, nodeRect.top), LineTo(centerX, nodeRect.bottom);
    else if (toBottom) MoveTo(centerX, centerY), LineTo(centerX, nodeRect.bottom);
    else if (toTop) MoveTo(centerX, nodeRect.top), LineTo(centerX, centerY);
    if (toRight) MoveTo(centerX + 1, centerY), LineTo(nodeRect.right, centerY);
    if (!showPlus && !showMinus) return;

    const int boxSize = nodeRect.Height() / 2 | 1;
    const int halfBox = boxSize / 2;
    const CRect box(centerX - halfBox, centerY - halfBox,
        centerX - halfBox + boxSize, centerY - halfBox + boxSize);
    const LOGBRUSH boxBrush{ BS_SOLID, lineColor, 0 };
    const CPen boxPen(PS_GEOMETRIC | PS_ENDCAP_FLAT, 1, &boxBrush);
    const CBrush backgroundBrush(background);
    GdiObjectSelection selectBoxPen(this, &boxPen);
    GdiObjectSelection selectBackground(this, &backgroundBrush);
    RoundRect(box, CPoint(2, 2));

    const int margin = nodeRect.Height() / 8;
    MoveTo(box.left + margin, centerY), LineTo(box.right - margin, centerY);
    if (showPlus) MoveTo(centerX, box.top + margin), LineTo(centerX, box.bottom - margin);
}

void CWinApp::RunTaskWithUiUpdates(const std::function<void()>& task)
{
    CWnd* const mainWindow = GetMainWindow();
    if (mainWindow == nullptr || GetWindowThreadProcessId(mainWindow->m_hWnd, nullptr) != GetCurrentThreadId())
    {
        task();
        return;
    }

    static const UINT taskCompleteMessage = RegisterWindowMessageW(L"WinDirStatTaskComplete");
    std::jthread([mainWindow, task]
    {
        task();
        mainWindow->PostMessage(taskCompleteMessage);
    }).detach();

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0))
    {
        if (message.message == taskCompleteMessage) break;
        if (message.message >= WM_MOUSEFIRST && message.message <= WM_MOUSELAST) continue;
        if (message.message >= WM_KEYFIRST && message.message <= WM_KEYLAST) continue;
        if (message.message == WM_NCLBUTTONDOWN || message.message == WM_NCLBUTTONUP) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
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
//  CMenu
// -----------------------------------------------------------------------------
static TemporaryHandleCache<HMENU, CMenu>& MenuHandleCache()
{
    thread_local TemporaryHandleCache<HMENU, CMenu> cache;
    return cache;
}

CMenu::~CMenu()
{
    if (m_hMenu != nullptr && m_bAutoDestroy) DestroyMenu(m_hMenu);
}

CMenu::CMenu(CMenu&& other) noexcept
    : m_hMenu(std::exchange(other.m_hMenu, nullptr)),
    m_bAutoDestroy(std::exchange(other.m_bAutoDestroy, false))
{
}

CMenu& CMenu::operator=(CMenu&& other) noexcept
{
    if (this == &other) return *this;
    if (m_hMenu != nullptr && m_bAutoDestroy) DestroyMenu(m_hMenu);
    m_hMenu = std::exchange(other.m_hMenu, nullptr);
    m_bAutoDestroy = std::exchange(other.m_bAutoDestroy, false);
    return *this;
}

CMenu::operator bool() const noexcept
{
    return m_hMenu != nullptr;
}

HMENU CMenu::Detach() noexcept
{
    const HMENU menu = m_hMenu;
    m_hMenu = nullptr;
    m_bAutoDestroy = false;
    return menu;
}

CMenu CMenu::CreatePopup() noexcept
{
    return CMenu(CreatePopupMenu());
}

CMenu CMenu::LoadResource(const UINT id) noexcept
{
    return CMenu(LoadMenuW(GetAppInstance(), MAKEINTRESOURCEW(id)));
}

CMenu* CMenu::FromHandle(const HMENU menu)
{
    if (menu == nullptr) return nullptr;
    return MenuHandleCache().Get(menu,
        [](CMenu& object, const HMENU handle)
        {
            object.m_hMenu = handle;
            object.m_bAutoDestroy = false;
        });
}

int CMenu::ItemCount() const noexcept
{
    return GetMenuItemCount(m_hMenu);
}

UINT CMenu::ItemIdAt(const int pos) const noexcept
{
    return GetMenuItemID(m_hMenu, pos);
}

UINT CMenu::ItemState(const UINT id, const UINT flags) const noexcept
{
    return GetMenuState(m_hMenu, id, flags);
}

std::wstring CMenu::ItemTextAt(const UINT pos) const
{
    const int length = GetMenuStringW(m_hMenu, pos, nullptr, 0, MF_BYPOSITION);
    std::wstring buffer(static_cast<size_t>(length) + 1, L'\0');
    const int copied = GetMenuStringW(m_hMenu, pos, buffer.data(), length + 1, MF_BYPOSITION);
    buffer.resize(static_cast<size_t>(copied));
    return buffer;
}

CMenu* CMenu::SubmenuAt(const int pos) const
{
    return FromHandle(GetSubMenu(m_hMenu, pos));
}

bool CMenu::Append(const UINT flags, const UINT_PTR id, const LPCWSTR psz) noexcept
{
    return AppendMenuW(m_hMenu, flags, id, psz);
}

bool CMenu::Modify(const UINT pos, const UINT flags, const UINT_PTR id, const LPCWSTR psz) noexcept
{
    return ModifyMenuW(m_hMenu, pos, flags, id, psz);
}

bool CMenu::Remove(const UINT pos, const UINT flags) noexcept
{
    return DeleteMenu(m_hMenu, pos, flags);
}

UINT CMenu::EnableItem(const UINT id, const UINT flags) noexcept
{
    return EnableMenuItem(m_hMenu, id, flags);
}

UINT CMenu::CheckItem(const UINT id, const UINT flags) noexcept
{
    return CheckMenuItem(m_hMenu, id, flags);
}

bool CMenu::SetDefaultItem(const UINT item) noexcept
{
    return SetMenuDefaultItem(m_hMenu, item, false);
}

bool CMenu::GetItemInfo(const UINT item, MENUITEMINFOW* info, const ItemLookup lookup) const
{
    return GetMenuItemInfoW(m_hMenu, item, lookup == ItemLookup::Position, info);
}

bool CMenu::SetItemInfo(const UINT item, const MENUITEMINFOW* info, const ItemLookup lookup)
{
    return SetMenuItemInfoW(m_hMenu, item, lookup == ItemLookup::Position, info);
}

UINT CMenu::ShowPopup(const UINT flags, const int x, const int y, CWnd* pWnd) const
{
    return TrackPopupMenu(m_hMenu, flags, x, y, 0, pWnd != nullptr ? pWnd->m_hWnd : nullptr, nullptr);
}

UINT CMenu::ShowPopupEx(const UINT flags, const int x, const int y, CWnd* pWnd,
    const LPTPMPARAMS lptpm) const
{
    return TrackPopupMenuEx(m_hMenu, flags, x, y, pWnd != nullptr ? pWnd->m_hWnd : nullptr, lptpm);
}

void CMenu::SetItemEnabled(const int item, const bool enable, const ItemLookup lookup)
{
    if (item < 0) return;
    const UINT flags = lookup == ItemLookup::Command ? MF_BYCOMMAND : MF_BYPOSITION;
    EnableItem(static_cast<UINT>(item), flags | (enable ? MF_ENABLED : MF_DISABLED | MF_GRAYED));
}

bool CMenu::IsItemEnabled(const UINT item, const ItemLookup lookup) const noexcept
{
    const UINT flags = lookup == ItemLookup::Command ? MF_BYCOMMAND : MF_BYPOSITION;
    return (ItemState(item, flags) & (MF_DISABLED | MF_GRAYED)) == 0;
}

CMenu::CMenu(const HMENU menu) noexcept
    : m_hMenu(menu),
    m_bAutoDestroy(menu != nullptr)
{
}

CMenu* CWnd::GetMenu() const
{
    return CMenu::FromHandle(::GetMenu(m_hWnd));
}

void ClearTemporaryHandleCaches()
{
    WindowHandleCache().Clear();
    MenuHandleCache().Clear();
}

// -----------------------------------------------------------------------------
//  CCmdUI implementation
// -----------------------------------------------------------------------------
void CCmdUI::Enable(const bool bOn)
{
    m_bEnableChanged = true;
    if (m_pMenu != nullptr)
        m_pMenu->EnableItem(m_nIndex, MF_BYPOSITION | (bOn ? MF_ENABLED : MF_DISABLED | MF_GRAYED));
}

void CCmdUI::SetCheck(const int nCheck)
{
    if (m_pMenu != nullptr)
        m_pMenu->CheckItem(m_nIndex, MF_BYPOSITION | (nCheck ? MF_CHECKED : MF_UNCHECKED));
}

void CCmdUI::SetRadio(const bool bOn)
{
    SetCheck(bOn ? 1 : 0);
    if (m_pMenu == nullptr) return;

    MENUITEMINFOW info{ .cbSize = sizeof(info), .fMask = MIIM_FTYPE | MIIM_CHECKMARKS };
    if (!m_pMenu->GetItemInfo(m_nIndex, &info)) return;

    info.fType |= MFT_RADIOCHECK;
    info.hbmpChecked = nullptr;
    info.hbmpUnchecked = nullptr;
    m_pMenu->SetItemInfo(m_nIndex, &info);
}

void CCmdUI::SetText(const LPCWSTR lpszText)
{
    if (m_pMenu == nullptr || lpszText == nullptr) return;
    const UINT state = m_pMenu->ItemState(m_nIndex, MF_BYPOSITION);
    m_pMenu->Modify(m_nIndex, MF_BYPOSITION | MF_STRING | (state & ~(MF_BITMAP | MF_OWNERDRAW)),
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

void CFrameWnd::UpdateMenuCommands(CMenu* pMenu, const bool bSysMenu)
{
    if (bSysMenu || pMenu == nullptr || pMenu->m_hMenu == nullptr) return;

    CCmdUI state;
    state.m_pMenu = pMenu;
    for (int i = 0; i < pMenu->ItemCount(); ++i)
    {
        const UINT id = pMenu->ItemIdAt(i);
        if (id == 0 || id == static_cast<UINT>(-1)) continue;   // separator / popup
        state.m_nIndex = static_cast<UINT>(i);
        state.m_nID = id;
        state.Update(this, true);
    }
}

// -----------------------------------------------------------------------------
//  Class-owned theme drawing
// -----------------------------------------------------------------------------
void CSplitterWnd::DrawBackground(CDC& dc, CRect rect) const
{
    const bool dark = DarkMode::IsDarkModeActive();
    const COLORREF paneFace = DarkMode::SystemColor(dark ? COLOR_MENUBAR : COLOR_BTNFACE);
    COLORREF paneEdge = paneFace;
    if (!dark)
    {
        paneEdge = GetSysColor(COLOR_3DSHADOW);
        const SmartPointer toolbarTheme(CloseThemeData, OpenThemeData(m_hWnd, VSCLASS_TOOLBAR));
        if (toolbarTheme.IsValid())
            GetThemeColor(toolbarTheme, TP_BUTTON, 0, TMT_EDGESHADOWCOLOR, &paneEdge);
    }

    dc.FillSolidRect(rect, DarkMode::SystemColor(dark ? COLOR_WINDOWFRAME : COLOR_BTNFACE));
    for (int row = 0; row < RowCount(); ++row)
    {
        for (int column = 0; column < ColumnCount(); ++column)
        {
            const CWnd* pane = PaneAt(row, column);
            if (pane == nullptr || !IsWindow(pane->m_hWnd) || pane->IsSplitterWindow())
                continue;

            CRect paneRect = WindowRectInClient(pane->Handle());
            paneRect.Inflate(PaneBorderSize, PaneBorderSize);
            dc.Draw3dRect(paneRect, paneEdge, paneEdge);
            paneRect.Deflate(1, 1);
            dc.Draw3dRect(paneRect, paneFace, paneFace);
        }
    }
}

void CSplitterWnd::OnPaint()
{
    CPaintDC dc(this);
    const CRect rect = ClientRect();
    DrawBackground(dc, rect);
    if (m_bTrackerVisible) DrawTrackerRect(dc, m_rectTracker);
}

void CToolBar::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult) const
{
    switch (auto* customDraw = reinterpret_cast<LPNMTBCUSTOMDRAW>(pNMHDR); customDraw->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
    {
        const CRect rect = ClientRect();
        auto dc = CDC::Borrow(customDraw->nmcd.hdc);
        const COLORREF background = DarkMode::IsDarkModeActive() ?
            DarkMode::SystemColor(COLOR_MENUBAR) : GetSysColor(COLOR_WINDOW);
        dc.FillSolidRect(rect, background);
        *pResult = CDRF_NOTIFYITEMDRAW;
        break;
    }
    case CDDS_ITEMPREPAINT:
        customDraw->clrText = DarkMode::SystemColor(COLOR_BTNTEXT);
        *pResult = TBCDRF_USECDCOLORS | CDRF_DODEFAULT;
        break;
    default:
        *pResult = CDRF_DODEFAULT;
        break;
    }
}

void CStatusBar::DrawPaneBorder(CDC& dc, const CRect rect)
{
    const bool dark = DarkMode::IsDarkModeActive();
    const COLORREF border = dark ? DarkMode::SystemColor(COLOR_WINDOWFRAME) : GetSysColor(COLOR_3DSHADOW);
    if (dark) dc.FillSolidRect(rect.left, rect.top, rect.Width(), 1, border);
    dc.FillSolidRect(rect.right - 1, rect.top, 1, rect.Height(), border);
}

void CStatusBar::OnPaint()
{
    CPaintDC paintDC(this);
    CBufferedDC dc(paintDC, this);
    const CRect rcClient = ClientRect();
    const COLORREF barFace = DarkMode::SystemColor(
        DarkMode::IsDarkModeActive() ? COLOR_MENUBAR : COLOR_BTNFACE);
    dc.FillSolidRect(rcClient, barFace);
    GdiObjectSelection selectFont(&dc, GetAppFont(m_hWnd));
    dc.SetBkMode(TRANSPARENT);

    const auto rects = LayoutPanes();
    for (size_t i = 0; i < m_panes.size(); ++i)
    {
        const Pane& pane = m_panes[i];
        CRect rect = rects[i];
        const COLORREF background = m_background != CLR_NONE ? m_background : barFace;
        dc.FillSolidRect(rect, background);
        DrawPaneBorder(dc, rect);
        CRect textRect = rect;
        textRect.Deflate(4, 0, 4, 0);
        dc.SetTextColor(DarkMode::SystemColor(COLOR_BTNTEXT));
        dc.DrawText(pane.text.c_str(), static_cast<int>(pane.text.size()), &textRect,
            DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
}

void CTabControl::SetLocation(const Location loc)
{
    m_location = loc;
    if (!IsWindow(m_hWnd)) return;

    ModifyStyle(m_location == Location::Bottom ? 0 : TCS_BOTTOM,
        m_location == Location::Bottom ? TCS_BOTTOM : 0, SWP_FRAMECHANGED);
    LayoutPanes();
    Invalidate(false);
}

void CTabControl::SetContentBackgroundColor(const COLORREF color)
{
    m_paneBackgroundColor = color;
    if (IsWindow(m_hWnd)) Invalidate(false);
}

std::wstring_view CTabControl::TabLabel(const int index) const
{
    return index >= 0 && index < TabCount() ? m_tabs[index].label : std::wstring_view();
}

void CTabControl::SetTabLabel(const int i, const std::wstring_view label)
{
    if (i < 0 || i >= TabCount()) return;

    m_tabs[i].label = label;
    if (const int native = NativeIndexFromLogical(i); native >= 0)
    {
        TCITEMW item{};
        item.mask = TCIF_TEXT | TCIF_PARAM;
        item.pszText = const_cast<LPWSTR>(m_tabs[i].label.c_str());
        item.lParam = static_cast<LPARAM>(i);
        SendNativeMessage(TCM_SETITEMW, static_cast<WPARAM>(native), &item);
    }
    if (IsWindow(m_hWnd)) Invalidate(false);
}

bool CTabControl::Create(const RECT& rect, CWnd* pParentWnd, const UINT nID, const bool focusTabStrip)
{
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_TAB_CLASSES };
    ::InitCommonControlsEx(&icc);
    m_focusTabStrip = focusTabStrip;
    DWORD tabStyle = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
        TCS_TABS | TCS_SINGLELINE | TCS_RAGGEDRIGHT;
    if (m_focusTabStrip)
        tabStyle |= WS_TABSTOP;
    if (m_location == Location::Bottom)
        tabStyle |= TCS_BOTTOM;
    if (!CreateEx(0, WC_TABCONTROLW, nullptr, tabStyle, rect, pParentWnd, nID)) return false;
    SendNativeMessage(TCM_SETUNICODEFORMAT, true, 0);
    return true;
}

int CTabControl::AddTab(CWnd* window, const std::wstring_view label)
{
    m_tabs.push_back({ window, std::wstring(label) });
    if (m_activeTab < 0) m_activeTab = static_cast<int>(m_tabs.size()) - 1;
    RebuildNativeTabs();
    LayoutPanes();
    Invalidate(false);
    return static_cast<int>(m_tabs.size()) - 1;
}

bool CTabControl::PreprocessMessage(MSG* pMsg)
{
    if (pMsg != nullptr && pMsg->hwnd == m_hWnd &&
        (pMsg->message == WM_KEYDOWN || pMsg->message == WM_SYSKEYDOWN ||
            pMsg->message == WM_CHAR || pMsg->message == WM_SYSCHAR) &&
        !IsPlainTabTraversal(*pMsg))
    {
        if (ForwardKeyboardMessageToActiveTab(*pMsg)) return true;
    }

    return CWnd::PreprocessMessage(pMsg);
}

void CTabControl::SetTabVisible(const int i, const bool show)
{
    if (i < 0 || i >= TabCount()) return;
    if (m_tabs[i].visible == show) return;

    const int previousActiveTab = m_activeTab;
    const bool moveFocusToActiveTab = previousActiveTab == i &&
        ShouldMoveFocusOnTabActivation(previousActiveTab);
    int activeTab = previousActiveTab;
    if (!show && previousActiveTab == i)
    {
        activeTab = -1;
        for (int k = 0; k < TabCount(); ++k)
        {
            if (k != i && m_tabs[k].visible) { activeTab = k; break; }
        }
    }
    else if (show && previousActiveTab < 0) activeTab = i;

    if (activeTab != previousActiveTab)
    {
        if (const CWnd* p = GetParent())
            ::SendMessageW(p->m_hWnd, WM_WDS_TAB_CHANGING, static_cast<WPARAM>(activeTab), 0);
    }

    m_tabs[i].visible = show;
    m_activeTab = activeTab;
    RebuildNativeTabs();
    LayoutPanes();
    if (moveFocusToActiveTab) FocusActiveTabWindow();
    if (m_activeTab != previousActiveTab) NotifyParentOfTabChange(m_activeTab);
    Invalidate(false);
}

LRESULT CTabControl::OnNcHitTest(const CPoint point)
{
    const CPoint clientPt = ToClient(point);
    if (TabStripRect().Contains(clientPt))
        return HTCLIENT;
    return CallDefaultHandler();
}

void CTabControl::OnSize(UINT, int, int)
{
    CallDefaultHandler(); LayoutPanes(); Invalidate(false);
}

void CTabControl::OnFontSizeChanged(int, int)
{
    RebuildNativeTabs();
    LayoutPanes();
    Invalidate(false);
}

void CTabControl::OnLButtonDown(UINT, const CPoint point)
{
    const CRect rcStrip = TabStripRect();
    UpdatePaintedTabRects(rcStrip, m_location == Location::Bottom, UsesLabelOnlyTabs());

    int logical = -1;
    if (m_activeTab >= 0 && m_activeTab < TabCount() && m_tabs[m_activeTab].paintedRect.Contains(point))
    {
        logical = m_activeTab;
    }
    else
    {
        for (int native = static_cast<int>(m_visibleToLogical.size()) - 1; native >= 0; --native)
        {
            const int candidate = m_visibleToLogical[static_cast<size_t>(native)];
            if (candidate < 0 || candidate >= TabCount() || candidate == m_activeTab) continue;
            if (m_tabs[candidate].paintedRect.Contains(point)) { logical = candidate; break; }
        }
    }

    m_handledPaintedTabMouseDown = logical >= 0;
    if (logical >= 0 && logical != m_activeTab) ActivateTab(logical, true);
    if (!m_handledPaintedTabMouseDown)
        CallDefaultHandler();
    if (m_focusTabStrip) SetFocus();
    else RedirectFocusAwayFromTabControl();
}

void CTabControl::OnLButtonUp(UINT, CPoint)
{
    if (!m_handledPaintedTabMouseDown)
        CallDefaultHandler();
    m_handledPaintedTabMouseDown = false;
    RedirectFocusAwayFromTabControl();
}

void CTabControl::OnSetFocus(CWnd*)
{
    if (m_focusTabStrip) CallDefaultHandler();
    else RedirectFocusAwayFromTabControl();
    Invalidate(false);
}

void CTabControl::OnKillFocus(CWnd*)
{
    if (m_focusTabStrip) CallDefaultHandler();
    Invalidate(false);
}

void CTabControl::OnKeyDown(const UINT nChar, const UINT nRepCnt, const UINT nFlags)
{
    MSG msg{};
    msg.hwnd = m_hWnd;
    msg.message = WM_KEYDOWN;
    msg.wParam = nChar;
    msg.lParam = MAKELPARAM(nRepCnt, nFlags);
    if (!IsPlainTabTraversal(msg) && ForwardKeyboardMessageToActiveTab(msg)) return;
    CallDefaultHandler();
}

void CTabControl::OnNativeSelChange(NMHDR*, LRESULT* pResult)
{
    const int native = static_cast<int>(SendNativeMessage(TCM_GETCURSEL));
    if (native >= 0 && std::cmp_less(native, m_visibleToLogical.size()))
    {
        ActivateTab(m_visibleToLogical[static_cast<size_t>(native)], false);
    }
    if (pResult) *pResult = 0;
}

void CTabControl::OnPaint()
{
    CPaintDC paintDC(this);
    CBufferedDC dc(paintDC, this);

    const CRect rcClient = ClientRect();
    const bool darkMode = DarkMode::IsDarkModeActive();
    const COLORREF tabBorder = GetSysColor(COLOR_3DSHADOW);
    const COLORREF tabPane = darkMode ? DarkMode::SystemColor(COLOR_WINDOW) : GetSysColor(COLOR_3DHILIGHT);
    const COLORREF tabStrip = darkMode ? DarkMode::SystemColor(COLOR_WINDOW) : GetSysColor(COLOR_3DFACE);
    const COLORREF buttonFace = DarkMode::SystemColor(COLOR_BTNFACE);

    const int tabH = TabStripHeight();
    const bool bottomTabs = m_location == Location::Bottom;
    const bool labelOnlyTabs = UsesLabelOnlyTabs();
    const bool dark = IsDarkColor(tabStrip) || IsDarkColor(tabPane);
    const COLORREF paneBackground = m_paneBackgroundColor != CLR_NONE ? m_paneBackgroundColor : tabPane;
    const COLORREF controlBackground = labelOnlyTabs ? buttonFace : paneBackground;
    dc.FillSolidRect(rcClient, controlBackground);

    CRect rcStrip = bottomTabs ?
        CRect(rcClient.left, std::max(rcClient.top, rcClient.bottom - tabH), rcClient.right, rcClient.bottom) :
        CRect(rcClient.left, rcClient.top, rcClient.right, std::min(rcClient.bottom, rcClient.top + tabH));
    const COLORREF stripBg = dark ? tabStrip
                                  : (labelOnlyTabs ? buttonFace : tabStrip);
    const COLORREF stripBorder = dark ? RGB(95, 95, 95) : tabBorder;
    const COLORREF activeTabBg = dark ? RGB(245, 245, 245)
                                      : (labelOnlyTabs ? buttonFace : RGB(255, 255, 255));
    const COLORREF inactiveTabBg = dark ? RGB(31, 31, 31)
                                        : BlendColor(tabStrip, RGB(0, 0, 0), 0.05);
    const COLORREF activeText = dark ? RGB(0, 0, 0) : DarkMode::SystemColor(COLOR_WINDOWTEXT);
    const COLORREF inactiveText = dark ? RGB(235, 235, 235) : DarkMode::SystemColor(COLOR_WINDOWTEXT);
    dc.FillSolidRect(rcStrip, stripBg);
    const int paneEdge = bottomTabs ? rcStrip.top : rcStrip.bottom - 1;
    dc.FillSolidRect(rcStrip.left, paneEdge, rcStrip.Width(), 1, stripBorder);

    HFONT font = GetFont();
    if (font == nullptr) font = GetAppFont(m_hWnd);
    GdiObjectSelection selectFont(&dc, font);
    CFont boldFont(font, FW_BOLD);
    dc.SetBkMode(TRANSPARENT);

    struct TabPaintInfo
    {
        int logical = -1;
        CRect rect;
    };

    TabPaintInfo activeTab;
    bool hasActiveTab = false;

    UpdatePaintedTabRects(rcStrip, bottomTabs, labelOnlyTabs);

    const int dpi = GetWindowDpi(m_hWnd);
    const auto scale = [dpi](const int value) { return MulDiv(value, dpi, 96); };
    const int textInsetX = scale(5);
    const int textInsetY = scale(1);
    const int minSlant = scale(4);
    const int maxSlant = scale(8);
    const bool drawFocus = m_focusTabStrip && ::GetFocus() == m_hWnd &&
        (SendMessage(WM_QUERYUISTATE) & UISF_HIDEFOCUS) == 0;
    CBrush activeBrush(activeTabBg);
    CBrush inactiveBrush(inactiveTabBg);
    CPen borderPen(PS_SOLID, 1, stripBorder);

    auto drawTab = [&](const TabPaintInfo& tab, const bool active)
        {
            if (tab.logical < 0 || tab.logical >= TabCount()) return;
            const CRect rcTab = tab.rect;
            const int slant = std::clamp(rcTab.Width() / 5, minSlant, maxSlant);
            POINT points[4] = {};
            if (bottomTabs)
            {
                points[0] = { rcTab.left, rcTab.top };
                points[1] = { rcTab.left + slant, rcTab.bottom - 1 };
                points[2] = { rcTab.right - slant, rcTab.bottom - 1 };
                points[3] = { rcTab.right, rcTab.top };
            }
            else
            {
                points[0] = { rcTab.left, rcTab.bottom - 1 };
                points[1] = { rcTab.left + slant, rcTab.top };
                points[2] = { rcTab.right - slant, rcTab.top };
                points[3] = { rcTab.right, rcTab.bottom - 1 };
            }

            const CBrush& fillBrush = active ? activeBrush : inactiveBrush;
            {
                GdiObjectSelection selectBrush(&dc, &fillBrush);
                StockObjectSelection selectFillPen(&dc, NULL_PEN);
                dc.Polygon(points, 4);
            }

            {
                GdiObjectSelection selectPen(&dc, &borderPen);
                if (active)
                {
                    dc.MoveTo(points[0]);
                    dc.LineTo(points[1]);
                    dc.LineTo(points[2]);
                    dc.LineTo(points[3]);
                }
                else
                {
                    const POINT outline[5] = { points[0], points[1], points[2], points[3], points[0] };
                    dc.Polyline(outline, 5);
                }
            }

            dc.SetTextColor(active ? activeText : inactiveText);
            const HGDIOBJ textFont = active && boldFont ? boldFont.Handle() : font;
            GdiObjectSelection selectTextFont(&dc, textFont);
            CRect rcText = rcTab;
            rcText.Deflate(textInsetX, textInsetY, textInsetX, textInsetY);
            const std::wstring& text = m_tabs[tab.logical].label;
            const int oldBk = dc.SetBkMode(TRANSPARENT);
            const CSize textExtent = dc.GetTextExtent(text.c_str(), static_cast<int>(text.size()));
            const auto textMetrics = dc.TextMetrics();

            const int x = static_cast<int>(rcText.left) +
                std::max<int>(0, (static_cast<int>(rcText.Width()) - textExtent.cx) / 2);
            const int y = static_cast<int>(rcText.top) +
                std::max<int>(0, (static_cast<int>(rcText.Height()) -
                    (textMetrics ? textMetrics->tmHeight : 0)) / 2);
            ExtTextOutW(dc, x, y, ETO_CLIPPED, &rcText, text.c_str(),
                static_cast<UINT>(text.size()), nullptr);
            dc.SetBkMode(oldBk);
            if (active && drawFocus) dc.DrawFocusRect(&rcText);
        };

    for (int native = 0; std::cmp_less(native, m_visibleToLogical.size()); ++native)
    {
        const int logical = m_visibleToLogical[static_cast<size_t>(native)];
        if (logical < 0 || logical >= TabCount()) continue;

        const CRect rcTab = m_tabs[logical].paintedRect;
        if (rcTab.IsEmpty()) continue;

        if (logical == m_activeTab)
        {
            activeTab.logical = logical;
            activeTab.rect = rcTab;
            hasActiveTab = true;
            continue;
        }

        drawTab({ logical, rcTab }, false);
    }

    if (hasActiveTab)
    {
        drawTab(activeTab, true);
    }
}

std::span<const RouteEntry> CTabControl::Routes()
{
    using ThisClass = CTabControl;
    static constexpr std::array entries
    {
        Route::Window<&ThisClass::OnNcHitTest>(WM_NCHITTEST),
        Route::Window<&ThisClass::OnSize>(WM_SIZE),
        Route::Window<&ThisClass::OnEraseBkgnd>(WM_ERASEBKGND),
        Route::Window<&ThisClass::OnLButtonDown>(WM_LBUTTONDOWN),
        Route::Window<&ThisClass::OnLButtonUp>(WM_LBUTTONUP),
        Route::Window<&ThisClass::OnKeyDown>(WM_KEYDOWN),
        Route::Window<&ThisClass::OnSetFocus>(WM_SETFOCUS),
        Route::Window<&ThisClass::OnKillFocus>(WM_KILLFOCUS),
        Route::Window<&ThisClass::OnPaint>(WM_PAINT),
        Route::ReflectNotify<&ThisClass::OnNativeSelChange>(TCN_SELCHANGE),
    };
    return entries;
}

bool CTabControl::IsPlainTabTraversal(const MSG& msg)
{
    if (msg.message != WM_KEYDOWN || msg.wParam != VK_TAB) return false;
    return !IsKeyDown(VK_CONTROL) && !IsKeyDown(VK_MENU);
}

bool CTabControl::IsDarkColor(const COLORREF color)
{
    const int luminance = GetRValue(color) * 299 + GetGValue(color) * 587 + GetBValue(color) * 114;
    return luminance < 128000;
}

int CTabControl::NativeIndexFromLogical(const int logical) const
{
    for (int i = 0; std::cmp_less(i, m_visibleToLogical.size()); ++i)
    {
        if (m_visibleToLogical[static_cast<size_t>(i)] == logical) return i;
    }
    return -1;
}

void CTabControl::RebuildNativeTabs()
{
    if (!IsWindow(m_hWnd)) return;

    UpdateNativePadding();
    SendNativeMessage(TCM_DELETEALLITEMS);
    m_visibleToLogical.clear();
    m_visibleToLogical.reserve(m_tabs.size());

    int native = 0;
    for (int logical = 0; logical < TabCount(); ++logical)
    {
        if (!m_tabs[logical].visible)
        {
            m_tabs[logical].paintedRect.Clear();
            continue;
        }

        TCITEMW item{};
        item.mask = TCIF_TEXT | TCIF_PARAM;
        item.pszText = const_cast<LPWSTR>(m_tabs[logical].label.c_str());
        item.lParam = static_cast<LPARAM>(logical);
        SendNativeMessage(TCM_INSERTITEMW, static_cast<WPARAM>(native), &item);
        m_visibleToLogical.push_back(logical);
        ++native;
    }

    SyncNativeSelection();
}

void CTabControl::UpdateNativePadding()
{
    if (!IsWindow(m_hWnd)) return;

    const bool labelOnlyTabs = UsesLabelOnlyTabs();
    SendNativeMessage(TCM_SETPADDING, 0, MAKELPARAM(
        ::ScaleForDpi(labelOnlyTabs ? 8 : 12, m_hWnd),
        ::ScaleForDpi(labelOnlyTabs ? 2 : 3, m_hWnd)));
}

void CTabControl::SyncNativeSelection()
{
    if (!IsWindow(m_hWnd)) return;
    if (const int native = NativeIndexFromLogical(m_activeTab); native >= 0)
        SendNativeMessage(TCM_SETCURSEL, static_cast<WPARAM>(native));
}

bool CTabControl::ActivateTab(const int i, const bool syncNative)
{
    if (i < 0 || i >= TabCount() || !m_tabs[i].visible) return false;
    if (i == m_activeTab) return true;

    const int previousActiveTab = m_activeTab;
    const bool moveFocusToActiveTab = ShouldMoveFocusOnTabActivation(previousActiveTab);

    if (const CWnd* p = GetParent())
        ::SendMessageW(p->m_hWnd, WM_WDS_TAB_CHANGING, static_cast<WPARAM>(i), 0);

    m_activeTab = i;
    if (syncNative) SyncNativeSelection();
    LayoutPanes();
    if (moveFocusToActiveTab) FocusActiveTabWindow();

    NotifyParentOfTabChange(i);
    Invalidate(false);
    return true;
}

void CTabControl::NotifyParentOfTabChange(const int activeTab) const
{
    if (const CWnd* p = GetParent())
        ::SendMessageW(p->m_hWnd, WM_WDS_TAB_CHANGED, static_cast<WPARAM>(activeTab), 0);
}

bool CTabControl::ShouldMoveFocusOnTabActivation(const int previousActiveTab) const
{
    const HWND focus = ::GetFocus();
    if (focus == nullptr) return false;
    if (focus == m_hWnd || ::IsChild(m_hWnd, focus)) return true;

    const CWnd* previous = TabWindow(previousActiveTab);
    return previous != nullptr && IsWindow(previous->m_hWnd) &&
        (focus == previous->m_hWnd || ::IsChild(previous->m_hWnd, focus));
}

bool CTabControl::ForwardKeyboardMessageToActiveTab(const MSG& msg)
{
    CWnd* p = TabWindow(m_activeTab);
    if (p == nullptr || !IsWindow(p->m_hWnd) || !::IsWindowVisible(p->m_hWnd) || !::IsWindowEnabled(p->m_hWnd)) return false;

    p->SetFocus();
    HWND target = ::GetFocus();
    if (target == nullptr || (target != p->m_hWnd && !::IsChild(p->m_hWnd, target))) target = p->m_hWnd;
    ::SendMessageW(target, msg.message, msg.wParam, msg.lParam);
    return true;
}

bool CTabControl::FocusActiveTabWindow()
{
    CWnd* p = TabWindow(m_activeTab);
    if (p == nullptr || !IsWindow(p->m_hWnd) || !::IsWindowVisible(p->m_hWnd) || !::IsWindowEnabled(p->m_hWnd)) return false;

    p->SetFocus();
    const HWND focus = ::GetFocus();
    return focus == p->m_hWnd || (focus != nullptr && ::IsChild(p->m_hWnd, focus));
}

bool CTabControl::RedirectFocusAwayFromTabControl()
{
    if (m_focusTabStrip) return false;
    if (FocusActiveTabWindow()) return true;

    if (const HWND parent = ::GetParent(m_hWnd);
        parent != nullptr && IsWindow(parent) && ::IsWindowVisible(parent) && ::IsWindowEnabled(parent))
    {
        ::SetFocus(parent);
        return ::GetFocus() != m_hWnd;
    }
    return false;
}

bool CTabControl::GetNativeItemRect(const int native, CRect& rc) const
{
    RECT r{};
    if (SendNativeMessage(TCM_GETITEMRECT, static_cast<WPARAM>(native), &r) == 0) return false;
    rc = r;
    return true;
}

CRect CTabControl::TabStripRect() const
{
    const CRect rcClient = ClientRect();
    const int tabH = TabStripHeight();
    return (m_location == Location::Bottom) ?
        CRect(rcClient.left, std::max(rcClient.top, rcClient.bottom - tabH), rcClient.right, rcClient.bottom) :
        CRect(rcClient.left, rcClient.top, rcClient.right, std::min(rcClient.bottom, rcClient.top + tabH));
}

void CTabControl::UpdatePaintedTabRects(const CRect& rcStrip, const bool bottomTabs, const bool labelOnlyTabs)
{
    for (TabInfo& tab : m_tabs)
        tab.paintedRect.Clear();

    const int dpi = GetWindowDpi(m_hWnd);
    const auto scale = [dpi](const int value) { return MulDiv(value, dpi, 96); };
    const int tabVisualHeight = std::clamp(rcStrip.Height() - scale(4), 0, ::ScaleForDpi(18, m_hWnd));
    const int leftInset = scale(labelOnlyTabs ? 2 : 3);
    const int overlap = scale(labelOnlyTabs ? 1 : 2);
    const int rightExpansion = scale(labelOnlyTabs ? 2 : 4);

    for (int native = 0; std::cmp_less(native, m_visibleToLogical.size()); ++native)
    {
        const int logical = m_visibleToLogical[static_cast<size_t>(native)];
        if (logical < 0 || logical >= TabCount()) continue;

        CRect rcTab;
        if (!GetNativeItemRect(native, rcTab)) continue;

        if (bottomTabs)
        {
            rcTab.top = rcStrip.top;
            rcTab.bottom = std::min(rcStrip.bottom, rcTab.top + tabVisualHeight);
        }
        else
        {
            rcTab.bottom = rcStrip.bottom;
            rcTab.top = std::max(rcStrip.top, rcTab.bottom - tabVisualHeight);
        }
        rcTab.left = std::max(rcStrip.left + leftInset, rcTab.left - (native == 0 ? 0 : overlap));
        rcTab.right += rightExpansion;
        m_tabs[logical].paintedRect = rcTab;
    }
}

bool CTabControl::UsesLabelOnlyTabs() const
{
    return !std::ranges::any_of(m_tabs, [](const TabInfo& tab)
        {
            return tab.visible && tab.window != nullptr;
        });
}

void CTabControl::LayoutPanes()
{
    if (!IsWindow(m_hWnd)) return;
    const CRect rc = ClientRect();
    const int tabH = TabStripHeight();
    CRect rcPane = (m_location == Location::Bottom) ?
        CRect(rc.left, rc.top, rc.right, std::max(rc.top, rc.bottom - tabH)) :
        CRect(rc.left, rc.top + tabH, rc.right, rc.bottom);
    for (int i = 0; i < TabCount(); ++i)
    {
        CWnd* p = m_tabs[i].window;
        if (p == nullptr || !IsWindow(p->m_hWnd)) continue;
        if (i == m_activeTab) { p->MoveWindow(rcPane); p->ShowWindow(SW_SHOW); }
        else p->ShowWindow(SW_HIDE);
    }
}

// -----------------------------------------------------------------------------
//  Cold CWnd and native-dialog helpers
// -----------------------------------------------------------------------------
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

bool CWnd::CopyTextToClipboard(const std::wstring_view text) const
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

void CWnd::CenterWindow(CWnd* pAlternate)
{
    HWND hParent = pAlternate ? pAlternate->m_hWnd : ::GetParent(m_hWnd);
    if (hParent == nullptr) hParent = ::GetDesktopWindow();
    RECT rcParent{}, rcWnd{};
    if (!::GetWindowRect(hParent, &rcParent) || !::GetWindowRect(m_hWnd, &rcWnd)) return;
    const CSize parent = CRect(rcParent).Size(), window = CRect(rcWnd).Size();
    const int64_t dx = static_cast<int64_t>(parent.cx) - window.cx, dy = static_cast<int64_t>(parent.cy) - window.cy;
    const int x = static_cast<int>(std::clamp<int64_t>(static_cast<int64_t>(rcParent.left) + dx / 2, INT_MIN, INT_MAX));
    const int y = static_cast<int>(std::clamp<int64_t>(static_cast<int64_t>(rcParent.top) + dy / 2, INT_MIN, INT_MAX));
    ::SetWindowPos(m_hWnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

std::optional<std::wstring> CDialog::PickFile(const FilePickerMode mode, std::wstring filter)
{
    std::ranges::replace(filter, L'|', L'\0');
    filter.push_back(L'\0');
    std::wstring file(4096, L'\0');
    OPENFILENAMEW dialog{ .lStructSize = sizeof(OPENFILENAMEW) };
    dialog.hwndOwner = GetDialogOwner();
    dialog.lpstrFilter = filter.c_str();
    dialog.lpstrFile = file.data();
    dialog.nMaxFile = static_cast<DWORD>(file.size());
    dialog.lpstrDefExt = L"csv";
    dialog.Flags = OFN_EXPLORER | OFN_DONTADDTORECENT |
        (mode == FilePickerMode::Open ? OFN_PATHMUSTEXIST : 0);
    if (!(mode == FilePickerMode::Open ? GetOpenFileNameW(&dialog) : GetSaveFileNameW(&dialog)))
        return std::nullopt;
    return file.c_str();
}

std::optional<std::wstring> CDialog::PickFolder(CWnd* parent)
{
    CComPtr<IFileOpenDialog> dialog;
    if (FAILED(dialog.CoCreateInstance(CLSID_FileOpenDialog)) ||
        FAILED(dialog->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_DONTADDTORECENT)) ||
        FAILED(dialog->SetTitle(L"WinDirStat")) || FAILED(dialog->Show(GetDialogOwner(parent)))) return std::nullopt;

    CComPtr<IShellItem> result;
    CComHeapPtr<wchar_t> path;
    if (FAILED(dialog->GetResult(&result)) || FAILED(result->GetDisplayName(SIGDN_FILESYSPATH, &path)) || path == nullptr)
        return std::nullopt;
    return std::wstring(path);
}

std::optional<COLORREF> CDialog::PickColor(const COLORREF initial)
{
    static COLORREF custom[16]{};
    CHOOSECOLORW dialog{ .lStructSize = sizeof(CHOOSECOLORW) };
    dialog.hwndOwner = GetDialogOwner();
    dialog.rgbResult = initial;
    dialog.lpCustColors = custom;
    dialog.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR;
    if (!ChooseColorW(&dialog)) return std::nullopt;
    return dialog.rgbResult;
}

// -----------------------------------------------------------------------------
//  Cold toolbar and property-sheet setup
// -----------------------------------------------------------------------------
int CToolBar::AddImage(const CBitmap& bmp)
{
    if (m_imageList == nullptr || m_disabledImageList == nullptr) RecreateImageLists();
    if (m_imageList == nullptr || m_disabledImageList == nullptr || bmp.Handle() == nullptr) return -1;
    if (ImageList_GetImageCount(m_imageList) != ImageList_GetImageCount(m_disabledImageList)) return -1;

    const auto hbmOriginal = static_cast<HBITMAP>(bmp.m_hObject);
    const CBitmap scaled([&]() -> HBITMAP
        {
            BITMAP bitmap{};
            if (GetObjectW(hbmOriginal, sizeof(BITMAP), &bitmap) == 0 || m_imageSize <= 0) return nullptr;

            const int64_t sourceHeight = bitmap.bmHeight < 0 ?
                -static_cast<int64_t>(bitmap.bmHeight) : bitmap.bmHeight;
            if (bitmap.bmWidth <= 0 || bitmap.bmWidth > INT_MAX / 4 ||
                sourceHeight <= 0 || sourceHeight > INT_MAX)
                return nullptr;

            const int sourceWidth = bitmap.bmWidth;
            const int sourceHeightInt = static_cast<int>(sourceHeight);
            if (sourceWidth == m_imageSize && sourceHeightInt == m_imageSize) return nullptr;

            const uint64_t sourceBytes =
                static_cast<uint64_t>(sourceWidth) * static_cast<uint64_t>(sourceHeightInt) * 4;
            if (sourceBytes > SIZE_MAX) return nullptr;

            std::vector<BYTE> sourcePixels(static_cast<size_t>(sourceBytes));
            BITMAPINFO bitmapInfo{};
            bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bitmapInfo.bmiHeader.biWidth = sourceWidth;
            bitmapInfo.bmiHeader.biHeight = -sourceHeightInt;
            bitmapInfo.bmiHeader.biPlanes = 1;
            bitmapInfo.bmiHeader.biBitCount = 32;
            bitmapInfo.bmiHeader.biCompression = BI_RGB;

            const CClientDC dc(nullptr);
            if (dc.Handle() == nullptr ||
                GetDIBits(dc, hbmOriginal, 0, static_cast<UINT>(sourceHeightInt), sourcePixels.data(),
                    &bitmapInfo, DIB_RGB_COLORS) != sourceHeightInt)
                return nullptr;

            Gdiplus::Bitmap source(sourceWidth, sourceHeightInt, sourceWidth * 4,
                PixelFormat32bppPARGB, sourcePixels.data());
            if (source.GetLastStatus() != Gdiplus::Ok) return nullptr;

            Gdiplus::Bitmap destination(m_imageSize, m_imageSize, PixelFormat32bppPARGB);
            Gdiplus::Graphics graphics(&destination);
            graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
            graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
            graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
            if (graphics.DrawImage(&source, 0, 0, m_imageSize, m_imageSize) != Gdiplus::Ok) return nullptr;

            HBITMAP result = nullptr;
            if (destination.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &result) == Gdiplus::Ok) return result;
            if (result != nullptr) DeleteObject(result);
            return nullptr;
        }());
    const HBITMAP hbmToAdd = scaled.Handle() != nullptr ?
        static_cast<HBITMAP>(scaled.m_hObject) : hbmOriginal;
    const int index = ImageList_Add(m_imageList, hbmToAdd, nullptr);
    if (index < 0) return -1;

    const CBitmap disabled([&]() -> HBITMAP
        {
            BITMAP bitmap{};
            if (GetObjectW(hbmToAdd, sizeof(BITMAP), &bitmap) == 0 || bitmap.bmWidth <= 0) return nullptr;

            const int64_t heightValue = bitmap.bmHeight < 0 ?
                -static_cast<int64_t>(bitmap.bmHeight) : bitmap.bmHeight;
            if (heightValue <= 0 || heightValue > INT_MAX) return nullptr;

            const int width = bitmap.bmWidth;
            const int height = static_cast<int>(heightValue);
            const uint64_t pixelCount = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
            if (pixelCount > SIZE_MAX / 4) return nullptr;

            BITMAPINFO bitmapInfo{};
            bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bitmapInfo.bmiHeader.biWidth = width;
            bitmapInfo.bmiHeader.biHeight = -height;
            bitmapInfo.bmiHeader.biPlanes = 1;
            bitmapInfo.bmiHeader.biBitCount = 32;
            bitmapInfo.bmiHeader.biCompression = BI_RGB;

            void* newBits = nullptr;
            CBitmap result(static_cast<HBITMAP>(
                CreateDIBSection(nullptr, &bitmapInfo, DIB_RGB_COLORS, &newBits, nullptr, 0)));
            if (!result || newBits == nullptr) return nullptr;

            const CClientDC dc(nullptr);
            if (dc.Handle() == nullptr ||
                GetDIBits(dc, hbmToAdd, 0, static_cast<UINT>(height), newBits,
                    &bitmapInfo, DIB_RGB_COLORS) != height)
                return nullptr;

            const COLORREF disabledText = GetSysColor(COLOR_GRAYTEXT);
            const BYTE disabledR = GetRValue(disabledText);
            const BYTE disabledG = GetGValue(disabledText);
            const BYTE disabledB = GetBValue(disabledText);
            auto* pixels = static_cast<RGBQUAD*>(newBits);
            const bool sourceHasAlpha = bitmap.bmPlanes == 1 && bitmap.bmBitsPixel == 32 &&
                std::any_of(pixels, pixels + pixelCount,
                    [](const RGBQUAD& pixel) { return pixel.rgbReserved != 0; });

            for (size_t i = 0; i < static_cast<size_t>(pixelCount); ++i)
            {
                RGBQUAD& pixel = pixels[i];
                const BYTE alpha = sourceHasAlpha ? pixel.rgbReserved : 0xff;

                // Grayscale using NTSC weights
                const BYTE gray = static_cast<BYTE>(
                    (pixel.rgbRed * 299 + pixel.rgbGreen * 587 + pixel.rgbBlue * 114) / 1000);
                pixel.rgbBlue = static_cast<BYTE>((disabledB * 3 + gray) / 4);
                pixel.rgbGreen = static_cast<BYTE>((disabledG * 3 + gray) / 4);
                pixel.rgbRed = static_cast<BYTE>((disabledR * 3 + gray) / 4);
                pixel.rgbReserved = static_cast<BYTE>((static_cast<int>(alpha) * 3) / 4);
            }

            return static_cast<HBITMAP>(result.Detach());
        }());
    const HBITMAP hbmDisabled = disabled.Handle() != nullptr ?
        static_cast<HBITMAP>(disabled.m_hObject) : hbmToAdd;
    const int disabledIndex = ImageList_Add(m_disabledImageList, hbmDisabled, nullptr);
    if (disabledIndex != index)
    {
        ImageList_Remove(m_imageList, index);
        if (disabledIndex >= 0) ImageList_Remove(m_disabledImageList, disabledIndex);
        return -1;
    }
    return index;
}

bool CPropertyPage::Create(CWnd* parent)
{
    if (m_hWnd != nullptr) return false;
    return CreateDialogParamW(GetAppInstance(), MAKEINTRESOURCEW(m_nIDTemplate), parent->m_hWnd,
        FrameworkDialogProc, reinterpret_cast<LPARAM>(static_cast<CWnd*>(this))) != nullptr;
}

void CPropertyPage::SetModified(const bool bChanged)
{
    m_bModified = bChanged;
    if (auto* sheet = static_cast<CPropertySheet*>(GetParent())) sheet->UpdateApplyButton();
}

bool CPropertySheet::SelectPage(const int i)
{
    if (i < 0 || i >= PageCount()) return false;
    if (m_tab.Handle() == nullptr)
    {
        m_pendingActivePage = i;
        return true;
    }

    if (!ActivatePage(i)) return false;
    SyncTabSelection(i);
    return true;
}

void CPropertySheet::UpdateApplyButton()
{
    if (IsWindow(m_applyButton))
        ::EnableWindow(m_applyButton, std::ranges::any_of(m_pages,
            [](const auto& page) { return page->m_bModified; }));
}

bool CPropertySheet::PreprocessMessage(MSG* pMsg)
{
    if (pMsg == nullptr || pMsg->message != WM_KEYDOWN) return CWnd::PreprocessMessage(pMsg);
    if (pMsg->wParam == VK_ESCAPE)
    {
        RequestModalExit(IDCANCEL);
        return true;
    }

    if (!IsKeyDown(VK_CONTROL) ||
        (pMsg->wParam != VK_TAB && pMsg->wParam != VK_PRIOR && pMsg->wParam != VK_NEXT))
        return CWnd::PreprocessMessage(pMsg);

    const int pageCount = PageCount();
    const int active = ActivePageIndex();
    if (pageCount <= 0 || active < 0 || active >= pageCount) return CWnd::PreprocessMessage(pMsg);
    if (pageCount == 1) return true;

    const HWND activePage = m_pages[active]->Handle();
    const HWND focus = ::GetFocus();
    const bool moveFocus = focus != nullptr && activePage != nullptr &&
        (focus == activePage || ::IsChild(activePage, focus));
    const bool previous = pMsg->wParam == VK_PRIOR ||
        (pMsg->wParam == VK_TAB && IsKeyDown(VK_SHIFT));
    const int next = (active + pageCount + (previous ? -1 : 1)) % pageCount;
    if (!SelectPage(next)) return CWnd::PreprocessMessage(pMsg);

    if (moveFocus)
    {
        const HWND nextPage = m_pages[next]->Handle();
        const HWND firstChild = ::GetWindow(nextPage, GW_CHILD);
        const HWND lastTab = firstChild != nullptr ? GetNextDlgTabItem(nextPage, firstChild, true) : nullptr;
        const HWND firstTab = lastTab != nullptr ? GetNextDlgTabItem(nextPage, lastTab, false) : nullptr;
        const bool validTab = firstTab != nullptr && ::IsChild(nextPage, firstTab) && ::IsWindowVisible(firstTab) &&
            ::IsWindowEnabled(firstTab) && (GetWindowLongPtrW(firstTab, GWL_STYLE) & WS_TABSTOP) != 0;
        if (validTab)
            ::SendMessageW(nextPage, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(firstTab), true);
        else
            ::SetFocus(nextPage);
    }
    return true;
}

LRESULT CPropertySheet::OnTabChanged(const WPARAM w, LPARAM)
{
    if (m_syncingTabSelection) return 0;
    if (const int previous = m_currentPage; !ActivatePage(static_cast<int>(w)) && previous >= 0)
        SyncTabSelection(previous);
    return 0;
}

std::span<const RouteEntry> CPropertySheet::Routes()
{
    using ThisClass = CPropertySheet;
    static constexpr std::array entries
    {
        Route::Window<&ThisClass::OnTabChanged>(WM_WDS_TAB_CHANGED),
        Route::Window<&ThisClass::OnClose>(WM_CLOSE),
    };
    return entries;
}

bool CPropertySheet::OnCommand(const WPARAM wParam, const LPARAM lParam)
{
    const UINT id = LOWORD(wParam);
    if (id == IDOK)
    {
        ApplyPages();
        RequestModalExit(IDOK);
        return true;
    }
    if (id == IDCANCEL) { RequestModalExit(IDCANCEL); return true; }
    if (id == ID_APPLY_NOW)
    {
        ApplyPages();
        return true;
    }
    return CWnd::OnCommand(wParam, lParam);
}

bool CPropertySheet::EnsurePageCreated(const int i)
{
    if (i < 0 || i >= PageCount()) return false;

    CPropertyPage* page = m_pages[i].get();
    if (page->Handle() != nullptr) return true;

    if (!page->Create(this)) return false;
    page->ModifyStyle(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_DISABLED, WS_CHILD, SWP_FRAMECHANGED);
    page->ModifyStyleEx(0, WS_EX_CONTROLPARENT);

    if (!m_pageRect.IsEmpty())
    {
        page->SetWindowPos(nullptr, m_pageRect.left, m_pageRect.top,
            m_pageRect.Width(), m_pageRect.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
    }

    page->ShowWindow(SW_HIDE);
    return true;
}

bool CPropertySheet::ActivatePage(const int active)
{
    if (!EnsurePageCreated(active)) return false;
    if (m_currentPage == active) return true;

    for (int i = 0; i < PageCount(); ++i)
        if (m_pages[i]->Handle())
        {
            const bool isActive = (i == active);
            m_pages[i]->EnableWindow(isActive);
            m_pages[i]->ShowWindow(isActive ? SW_SHOW : SW_HIDE);
        }

    // Dialog traversal follows sibling Z-order, so the active page belongs immediately after the tab strip.
    m_pages[active]->SetWindowPos(&m_tab, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    m_currentPage = active;
    return true;
}

void CPropertySheet::SyncTabSelection(const int active)
{
    if (m_tab.ActiveTab() == active) return;
    m_syncingTabSelection = true;
    m_tab.SelectTab(active);
    m_syncingTabSelection = false;
}

void CPropertySheet::ApplyPages()
{
    for (const auto& page : m_pages)
    {
        if (page->Handle() == nullptr) continue;
        page->OnOK();
        page->m_bModified = false;
    }
    UpdateApplyButton();
}

bool CPropertySheet::OnEraseBkgnd(CDC* pDC) const
{
    const CRect rc = ClientRect();
    pDC->FillSolidRect(rc, DarkMode::SystemColor(COLOR_BTNFACE));
    return true;
}

bool CPropertySheet::OnInitDialog()
{
    const int margin = ::ScaleForDpi(10, m_hWnd);
    const int tabH = ::ScaleForDpi(22, m_hWnd);
    const int tabGap = ::ScaleForDpi(4, m_hWnd);
    const int btnW = ::ScaleForDpi(86, m_hWnd);
    const int btnH = ::ScaleForDpi(26, m_hWnd);
    const int btnGap = ::ScaleForDpi(8, m_hWnd);
    const int gap = ::ScaleForDpi(8, m_hWnd);

    // Read tab labels from the dialog templates, then create only one page for
    // initial sizing.  Initializing every property page here makes the settings
    // dialog feel sluggish compared with MFC's lazy property-page creation.
    int maxW = 100, maxH = 100;
    std::vector<std::wstring> captions;
    captions.reserve(m_pages.size());
    for (const auto& page : m_pages)
    {
        captions.push_back([templateId = page->m_nIDTemplate]() -> std::wstring
            {
                const HINSTANCE instance = GetAppInstance();
                const HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(templateId), RT_DIALOG);
                if (resource == nullptr) return {};

                const DWORD resourceSize = SizeofResource(instance, resource);
                const HGLOBAL global = LoadResource(instance, resource);
                const auto* begin = global != nullptr ? static_cast<const WORD*>(LockResource(global)) : nullptr;
                if (begin == nullptr || resourceSize < 2 * sizeof(WORD)) return {};

                const size_t wordCount = resourceSize / sizeof(WORD);
                const size_t headerWords = begin[0] == 1 && begin[1] == 0xFFFF ? 13 : 9;
                if (wordCount < headerWords) return {};

                const WORD* const end = begin + wordCount;
                const auto skipResourceOrString = [end](const WORD* value)
                    {
                        if (value >= end) return static_cast<const WORD*>(nullptr);
                        if (*value == 0) return value + 1;
                        if (*value == 0xFFFF) return end - value >= 2 ? value + 2 : nullptr;
                        while (value < end && *value != 0) ++value;
                        return value < end ? value + 1 : nullptr;
                    };

                const WORD* value = skipResourceOrString(begin + headerWords); // menu
                if (value == nullptr) return {};
                value = skipResourceOrString(value); // window class
                if (value == nullptr || value >= end || *value == 0 || *value == 0xFFFF) return {};

                const WORD* const start = value;
                while (value < end && *value != 0) ++value;
                if (value >= end) return {};
                return { reinterpret_cast<const wchar_t*>(start), static_cast<size_t>(value - start) };
            }());
    }

    for (int i = 0; i < PageCount(); ++i)
    {
        if (!EnsurePageCreated(i)) continue;

        const CRect rcPage = m_pages[i]->WindowRect();
        maxW = std::max<int>(maxW, rcPage.Width());
        maxH = std::max<int>(maxH, rcPage.Height());
        break;
    }

    const int clientW = margin + maxW + margin;
    const int clientH = margin + tabH + tabGap + maxH + gap + btnH + margin;

    // Resize the sheet to fit
    RECT rcWin{ 0, 0, clientW, clientH };
    AdjustWindowRectEx(&rcWin, static_cast<DWORD>(GetStyle()), false,
        static_cast<DWORD>(GetWindowLongPtrW(m_hWnd, GWL_EXSTYLE)));
    SetWindowPos(nullptr, 0, 0, rcWin.right - rcWin.left, rcWin.bottom - rcWin.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    // Tab strip (label-only)
    m_tab.SetLocation(CTabControl::Location::Top);
    m_tab.Create(CRect(margin, margin, margin + maxW, margin + tabH), this, 0xCAFE, true);
    for (auto& cap : captions) m_tab.AddTab(nullptr, cap);

    // Position pages
    const int pageY = margin + tabH + tabGap;
    m_pageRect = CRect(margin, pageY, margin + maxW, pageY + maxH);
    for (const auto& page : m_pages)
        if (page->Handle()) page->SetWindowPos(nullptr, m_pageRect.left, m_pageRect.top,
            m_pageRect.Width(), m_pageRect.Height(), SWP_NOZORDER | SWP_NOACTIVATE);

    // Buttons (bottom-right: OK, Cancel, Apply)
    const int btnY = pageY + maxH + gap;
    int bx = margin + maxW - 3 * btnW - 2 * btnGap;
    const auto createButton = [this, btnY, btnW, btnH](const int x, const UINT id, const LPCWSTR text, const DWORD style)
        {
            const HWND button = CreateWindowExW(0, WC_BUTTONW, text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
                x, btnY, btnW, btnH, m_hWnd, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)),
                GetAppInstance(), nullptr);
            ::SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(GetAppFont(button)), true);
            return button;
        };
    createButton(bx, IDOK, L"IDS_GENERIC_OK", BS_DEFPUSHBUTTON);
    bx += btnW + btnGap;
    createButton(bx, IDCANCEL, L"IDS_GENERIC_CANCEL", BS_PUSHBUTTON);
    bx += btnW + btnGap;
    m_applyButton = createButton(bx, ID_APPLY_NOW, L"IDS_GENERIC_APPLY", BS_PUSHBUTTON);

    if (PageCount() > 0) SelectPage(std::clamp(m_pendingActivePage, 0, PageCount() - 1));
    UpdateApplyButton();
    return true;
}

INT_PTR CPropertySheet::ShowModal()
{
    const HWND hOwner = GetDialogOwner();
    if (const LPCWSTR cls = RegisterWindowClass(0, LoadCursorW(nullptr, IDC_ARROW),
            reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1));
        !CreateEx(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, cls, m_caption.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, hOwner, nullptr))
        return -1;

    if (!OnInitDialog())
    {
        if (IsWindow(m_hWnd)) DestroyWindow();
        ClearTemporaryHandleCaches();
        return -1;
    }

    CenterWindow(FromHandle(hOwner));
    ShowWindow(SW_SHOW);
    if (m_tab.TabCount() > 0) m_tab.SetFocus();
    UpdateWindow();
    const bool ownerWasEnabled = hOwner != nullptr && ::IsWindowEnabled(hOwner);
    struct OwnerEnableScope final
    {
        HWND hWnd; bool enabled; ~OwnerEnableScope() { if (enabled && IsWindow(hWnd)) ::EnableWindow(hWnd, true); }
    } ownerEnableScope{ hOwner, ownerWasEnabled };
    if (ownerWasEnabled) ::EnableWindow(hOwner, false);

    m_modalResult = -1;
    bool repostQuit = false;
    int quitCode = 0;
    MSG msg{};
    while (m_modalResult == -1 && IsWindow(m_hWnd))
    {
        if (const int result = GetMessageW(&msg, nullptr, 0, 0); result <= 0)
        {
            if (result == 0)
            {
                repostQuit = true;
                quitCode = static_cast<int>(msg.wParam);
            }
            m_modalResult = IDCANCEL;
            break;
        }
        if (msg.hwnd != nullptr && (msg.hwnd == m_hWnd || ::IsChild(m_hWnd, msg.hwnd)) &&
            PreTranslateWindowTree(m_hWnd, &msg)) continue;
        if (!IsDialogMessageW(m_hWnd, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }

    if (m_modalResult == -1) m_modalResult = IDCANCEL;
    if (ownerWasEnabled && IsWindow(hOwner))
    {
        ::EnableWindow(hOwner, true);
        ::SetForegroundWindow(hOwner);
    }
    if (IsWindow(m_hWnd)) DestroyWindow();
    ClearTemporaryHandleCaches();
    if (repostQuit) PostQuitMessage(quitCode);
    return m_modalResult;
}
