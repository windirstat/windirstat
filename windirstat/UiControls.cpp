// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"

// -----------------------------------------------------------------------------
//  Class-owned theme drawing
// -----------------------------------------------------------------------------
void CSplitterWnd::DrawBackground(CDC& dc, const CRect& rect) const
{
    const COLORREF paneFace = DarkMode::SystemColor(DarkMode::IsDarkModeActive() ? COLOR_MENUBAR : COLOR_BTNFACE);
    COLORREF paneEdge = paneFace;
    if (!DarkMode::IsDarkModeActive())
    {
        paneEdge = GetSysColor(COLOR_3DSHADOW);
        const SmartPointer toolbarTheme(CloseThemeData, OpenThemeData(m_hWnd, VSCLASS_TOOLBAR));
        if (toolbarTheme.IsValid())
            GetThemeColor(toolbarTheme, TP_BUTTON, 0, TMT_EDGESHADOWCOLOR, &paneEdge);
    }

    dc.FillSolidRect(rect, DarkMode::SystemColor(DarkMode::IsDarkModeActive() ? COLOR_WINDOWFRAME : COLOR_BTNFACE));
    for (const auto [row, column] : std::views::cartesian_product(
        std::views::iota(0, GetRowCount()),
        std::views::iota(0, GetColumnCount())))
    {
        const CWnd* pane = GetPane(row, column);
        if (pane == nullptr || !IsWindow(pane->m_hWnd) || pane->IsSplitterWindow())
            continue;

        CRect paneRect = GetChildWindowRect(pane->Handle());
        paneRect.Inflate(1, 1);
        dc.Draw3dRect(paneRect, paneEdge, paneEdge);
    }
}

void CSplitterWnd::OnPaint()
{
    CPaintDC dc(this);
    const CRect rect = GetClientRect();
    DrawBackground(dc, rect);
}

HBRUSH CToolBar::OnCtlColor(CDC* dc, WindowRef control, const UINT type)
{
    const HBRUSH brush = DarkMode::OnCtlColor(dc, type);
    return brush != nullptr ? brush : CWnd::OnCtlColor(dc, control, type);
}

void CToolBar::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult) const
{
    switch (auto* customDraw = reinterpret_cast<LPNMTBCUSTOMDRAW>(pNMHDR); customDraw->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
    {
        const CRect rect = GetClientRect();
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

void CStatusBar::DrawPaneBorder(CDC& dc, const CRect& rect)
{
    const COLORREF border = DarkMode::IsDarkModeActive()
        ? DarkMode::SystemColor(COLOR_WINDOWFRAME) : GetSysColor(COLOR_3DSHADOW);
    if (DarkMode::IsDarkModeActive()) dc.FillSolidRect(rect.left, rect.top, rect.Width(), 1, border);
    dc.FillSolidRect(rect.right - 1, rect.top, 1, rect.Height(), border);
}

void CStatusBar::OnPaint()
{
    CPaintDC paintDC(this);
    CBufferedDC dc(paintDC, this);
    const CRect rcClient = GetClientRect();
    const COLORREF barFace = DarkMode::SystemColor(
        DarkMode::IsDarkModeActive() ? COLOR_MENUBAR : COLOR_BTNFACE);
    dc.FillSolidRect(rcClient, barFace);
    GdiObjectSelection selectFont(&dc, GetAppFont(m_hWnd));
    dc.SetBkMode(TRANSPARENT);

    const auto rects = LayoutPanes();
    for (const auto& [pane, rectLayout] : std::views::zip(m_panes, rects))
    {
        CRect rect = rectLayout;
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

std::wstring_view CTabControl::GetTabLabel(const int index) const
{
    return index >= 0 && index < GetTabCount() ? m_tabs[index].label : std::wstring_view();
}

void CTabControl::SetTabLabel(const int i, const std::wstring_view label)
{
    if (i < 0 || i >= GetTabCount()) return;

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
    if (i < 0 || i >= GetTabCount()) return;
    if (m_tabs[i].visible == show) return;

    const int previousActiveTab = m_activeTab;
    const bool moveFocusToActiveTab = previousActiveTab == i &&
        ShouldMoveFocusOnTabActivation(previousActiveTab);
    int activeTab = previousActiveTab;
    if (!show && previousActiveTab == i)
    {
        activeTab = -1;
        for (const auto [k, tab] : std::views::enumerate(m_tabs))
        {
            if (static_cast<int>(k) != i && tab.visible) { activeTab = static_cast<int>(k); break; }
        }
    }
    else if (show && previousActiveTab < 0) activeTab = i;

    if (activeTab != previousActiveTab)
    {
        if (const auto p = GetParent())
            ::SendMessageW(p.Handle(), WM_WDS_TAB_CHANGING, static_cast<WPARAM>(activeTab), 0);
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
    if (m_activeTab >= 0 && m_activeTab < GetTabCount() && m_tabs[m_activeTab].paintedRect.Contains(point))
    {
        logical = m_activeTab;
    }
    else
    {
        for (int native = static_cast<int>(m_visibleToLogical.size()) - 1; native >= 0; --native)
        {
            const int candidate = m_visibleToLogical[static_cast<size_t>(native)];
            if (candidate < 0 || candidate >= GetTabCount() || candidate == m_activeTab) continue;
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

void CTabControl::OnSetFocus(WindowRef)
{
    if (m_focusTabStrip) CallDefaultHandler();
    else RedirectFocusAwayFromTabControl();
    Invalidate(false);
}

void CTabControl::OnKillFocus(WindowRef)
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

void CTabControl::OnTabSelectionChanged(NMHDR*, LRESULT* pResult)
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

    const CRect rcClient = GetClientRect();
    const COLORREF tabBorder = GetSysColor(COLOR_3DSHADOW);
    const COLORREF tabPane = DarkMode::IsDarkModeActive()
        ? DarkMode::SystemColor(COLOR_WINDOW) : GetSysColor(COLOR_3DHILIGHT);
    const COLORREF tabStrip = DarkMode::IsDarkModeActive()
        ? DarkMode::SystemColor(COLOR_WINDOW) : GetSysColor(COLOR_3DFACE);
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
    const COLORREF activeTabBg = dark ? RGB(190, 190, 190)
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
    const bool drawFocus = m_focusTabStrip && HasFocus() &&
        (SendMessage(WM_QUERYUISTATE) & UISF_HIDEFOCUS) == 0;
    CBrush activeBrush(activeTabBg);
    CBrush inactiveBrush(inactiveTabBg);
    CPen borderPen(PS_SOLID, 1, stripBorder);
    CPen focusPen(PS_DOT, 1, activeText);

    auto drawTab = [&](const TabPaintInfo& tab, const bool active)
        {
            if (tab.logical < 0 || tab.logical >= GetTabCount()) return;
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
                GdiObjectSelection selectPen(&dc, active && drawFocus ? &focusPen : &borderPen);
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
            const CSize textExtent = dc.GetTextExtent(text);
            const auto textMetrics = dc.GetTextMetrics();

            const int x = static_cast<int>(rcText.left) +
                std::max<int>(0, (static_cast<int>(rcText.Width()) - textExtent.cx) / 2);
            const int y = static_cast<int>(rcText.top) +
                std::max<int>(0, (static_cast<int>(rcText.Height()) -
                    (textMetrics ? textMetrics->tmHeight : 0)) / 2);
            ExtTextOutW(dc, x, y, ETO_CLIPPED, &rcText, text.c_str(),
                static_cast<UINT>(text.size()), nullptr);
            dc.SetBkMode(oldBk);
        };

    for (const int logical : m_visibleToLogical)
    {
        if (logical < 0 || logical >= GetTabCount()) continue;

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
    static constexpr std::array entries
    {
        Route::Window<&OnNcHitTest>(WM_NCHITTEST),
        Route::Window<&OnSize>(WM_SIZE),
        Route::Window<&OnEraseBkgnd>(WM_ERASEBKGND),
        Route::Window<&OnLButtonDown>(WM_LBUTTONDOWN),
        Route::Window<&OnLButtonUp>(WM_LBUTTONUP),
        Route::Window<&OnKeyDown>(WM_KEYDOWN),
        Route::Window<&OnSetFocus>(WM_SETFOCUS),
        Route::Window<&OnKillFocus>(WM_KILLFOCUS),
        Route::Window<&OnPaint>(WM_PAINT),
        Route::ReflectNotify<&OnTabSelectionChanged>(TCN_SELCHANGE),
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
    const auto it = std::ranges::find(m_visibleToLogical, logical);
    return it != m_visibleToLogical.end() ?
        static_cast<int>(std::ranges::distance(m_visibleToLogical.begin(), it)) : -1;
}

void CTabControl::RebuildNativeTabs()
{
    if (!IsWindow(m_hWnd)) return;

    UpdateNativePadding();
    SendNativeMessage(TCM_DELETEALLITEMS);
    m_visibleToLogical.clear();
    m_visibleToLogical.reserve(m_tabs.size());

    int native = 0;
    for (const auto [logical, tab] : std::views::enumerate(m_tabs))
    {
        if (!tab.visible)
        {
            tab.paintedRect.Clear();
            continue;
        }

        TCITEMW item{};
        item.mask = TCIF_TEXT | TCIF_PARAM;
        item.pszText = const_cast<LPWSTR>(tab.label.c_str());
        item.lParam = static_cast<LPARAM>(logical);
        SendNativeMessage(TCM_INSERTITEMW, static_cast<WPARAM>(native), &item);
        m_visibleToLogical.push_back(static_cast<int>(logical));
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
    if (i < 0 || i >= GetTabCount() || !m_tabs[i].visible) return false;
    if (i == m_activeTab) return true;

    const int previousActiveTab = m_activeTab;
    const bool moveFocusToActiveTab = ShouldMoveFocusOnTabActivation(previousActiveTab);

    if (const auto p = GetParent())
        ::SendMessageW(p.Handle(), WM_WDS_TAB_CHANGING, static_cast<WPARAM>(i), 0);

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
    if (const auto p = GetParent())
        ::SendMessageW(p.Handle(), WM_WDS_TAB_CHANGED, static_cast<WPARAM>(activeTab), 0);
}

bool CTabControl::ShouldMoveFocusOnTabActivation(const int previousActiveTab) const
{
    const HWND focus = ::GetFocus();
    if (focus == nullptr) return false;
    if (focus == m_hWnd || IsChild(focus)) return true;

    const CWnd* previous = GetTabWindow(previousActiveTab);
    return previous != nullptr && IsWindow(previous->m_hWnd) &&
        (focus == previous->m_hWnd || previous->IsChild(focus));
}

bool CTabControl::ForwardKeyboardMessageToActiveTab(const MSG& msg)
{
    CWnd* p = GetTabWindow(m_activeTab);
    if (p == nullptr || !IsWindow(p->m_hWnd) || !::IsWindowVisible(p->m_hWnd) || !::IsWindowEnabled(p->m_hWnd)) return false;

    p->SetFocus();
    HWND target = ::GetFocus();
    if (target == nullptr || (target != p->m_hWnd && !p->IsChild(target))) target = p->m_hWnd;
    ::SendMessageW(target, msg.message, msg.wParam, msg.lParam);
    return true;
}

bool CTabControl::FocusActiveTabWindow()
{
    CWnd* p = GetTabWindow(m_activeTab);
    if (p == nullptr || !IsWindow(p->m_hWnd) || !::IsWindowVisible(p->m_hWnd) || !::IsWindowEnabled(p->m_hWnd)) return false;

    p->SetFocus();
    const HWND focus = ::GetFocus();
    return focus == p->m_hWnd || p->IsChild(focus);
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
    const CRect rcClient = GetClientRect();
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

    for (const auto [native, logical] : std::views::enumerate(m_visibleToLogical))
    {
        if (logical < 0 || logical >= GetTabCount()) continue;

        CRect rcTab;
        if (!GetNativeItemRect(static_cast<int>(native), rcTab)) continue;

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
    const CRect rc = GetClientRect();
    const int tabH = TabStripHeight();
    CRect rcPane = (m_location == Location::Bottom) ?
        CRect(rc.left, rc.top, rc.right, std::max(rc.top, rc.bottom - tabH)) :
        CRect(rc.left, rc.top + tabH, rc.right, rc.bottom);
    for (const auto [i, tab] : std::views::enumerate(m_tabs))
    {
        CWnd* p = tab.window;
        if (p == nullptr || !IsWindow(p->m_hWnd)) continue;
        if (static_cast<int>(i) == m_activeTab) { p->MoveWindow(rcPane); p->ShowWindow(SW_SHOW); }
        else p->ShowWindow(SW_HIDE);
    }
}

// -----------------------------------------------------------------------------
//  Cold CWnd and native-dialog helpers
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
            const std::span pixelSpan(pixels, static_cast<size_t>(pixelCount));
            const bool sourceHasAlpha = bitmap.bmPlanes == 1 && bitmap.bmBitsPixel == 32 &&
                std::ranges::any_of(pixelSpan,
                    [](const RGBQUAD& pixel) { return pixel.rgbReserved != 0; });

            for (RGBQUAD& pixel : pixelSpan)
            {
                const BYTE alpha = sourceHasAlpha ? pixel.rgbReserved : 0xFF;

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

void CTabControl::OnAppearanceChanged()
{
    SetContentBackgroundColor(DarkMode::IsDarkModeActive() ? DarkMode::Color(DarkMode::ColorRole::Window) :
        CLR_NONE);
}
