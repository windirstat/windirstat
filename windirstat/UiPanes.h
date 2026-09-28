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

#include "UiApplication.h"

// -----------------------------------------------------------------------------
//  CSplitterWnd (static splitter)
// -----------------------------------------------------------------------------

class CSplitterWnd : public MessageTarget<CSplitterWnd, CWnd>
{
public:
    static constexpr int PaneBorderSize = 2;
    static constexpr int SplitterSize = 7;

    ~CSplitterWnd() override { DestroyWindow(); }
    bool IsSplitterWindow() const noexcept override { return true; }

    bool CreateStatic(CWnd* pParentWnd, const int nRows, const int nCols,
        const DWORD dwStyle = WS_CHILD | WS_VISIBLE, const UINT nID = WDS_PANE_ID_BASE)
    {
        if ((nRows != 1 && nCols != 1) || nRows < 1 || nRows > 2 || nCols < 1 || nCols > 2)
            return false;

        m_rowSizes.assign(nRows, -1);
        m_columnSizes.assign(nCols, -1);
        const CRect rc(0, 0, 0, 0);
        const LPCWSTR cls = RegisterWindowClass(CS_DBLCLKS, LoadCursorW(nullptr, IDC_ARROW));
        return CreateEx(0, cls, nullptr, (dwStyle & ~WS_BORDER) | WS_CLIPCHILDREN, rc, pParentWnd, nID);
    }
    template<typename View>
    bool CreateView(const int row, const int col, const SIZE& sizeInit)
    {
        static_assert(std::is_base_of_v<CWnd, View>);
        if (!IsValidPane(row, col)) return false;
        auto view = std::make_unique<View>();
        const CRect rc(0, 0, sizeInit.cx, sizeInit.cy);
        if (!view->Create(nullptr, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            rc, this, PaneId(row, col)))
            return false;
        m_ownedPanes.push_back(std::move(view));
        return true;
    }
    CWnd* GetPane(const int row, const int col) const
    {
        return IsValidPane(row, col) ? FindAttached(::GetDlgItem(m_hWnd, PaneId(row, col))) : nullptr;
    }
    template<typename T> T* GetPane(const int row, const int col) const { return static_cast<T*>(GetPane(row, col)); }
    static constexpr int PaneId(const int row, const int col) noexcept { return WDS_PANE_ID_BASE + row * 16 + col; }
    int GetColumnCount() const { return static_cast<int>(m_columnSizes.size()); }
    int GetRowCount() const { return static_cast<int>(m_rowSizes.size()); }
    int GetRowSize(const int row) const { return IsValidRow(row) ? m_rowSizes[row] : 0; }
    void SetRowSize(const int row, const int size) { if (IsValidRow(row)) m_rowSizes[row] = size; }
    int GetColumnSize(const int column) const { return IsValidColumn(column) ? m_columnSizes[column] : 0; }
    void SetColumnSize(const int column, const int size) { if (IsValidColumn(column)) m_columnSizes[column] = size; }
    void ResetPanes() { m_rowSizes.clear(); m_columnSizes.clear(); }

    virtual void UpdateLayout();

    static std::span<const RouteEntry> Routes()
    {
        static constexpr std::array entries
        {
            Route::Window<&OnSize>(WM_SIZE),
            Route::Window<&OnEraseBkgnd>(WM_ERASEBKGND),
            Route::Window<&OnPaint>(WM_PAINT),
            Route::Window<&OnLButtonDown>(WM_LBUTTONDOWN),
            Route::Window<&OnLButtonUp>(WM_LBUTTONUP),
            Route::Window<&OnMouseMove>(WM_MOUSEMOVE),
            Route::Window<&OnCaptureChanged>(WM_CAPTURECHANGED),
            Route::Window<&OnCancelMode>(WM_CANCELMODE),
            Route::Window<&OnSetCursor>(WM_SETCURSOR),
        };
        return entries;
    }

protected:
    virtual void StopTracking(bool bAccept);

    // message handlers
    void OnSize(UINT, int, int) { UpdateLayout(); }
    bool OnEraseBkgnd(CDC*) { return true; }
    void OnPaint();
    void OnLButtonDown(UINT nFlags, CPoint pt);
    void OnLButtonUp(UINT nFlags, CPoint pt);
    void OnMouseMove(UINT nFlags, CPoint pt);
    void OnCaptureChanged(WindowRef pWnd);
    void OnCancelMode();
    bool OnSetCursor(WindowRef pWnd, UINT nHitTest, UINT message);

private:
    std::vector<std::unique_ptr<CWnd>> m_ownedPanes;
    bool IsValidRow(const int row) const { return row >= 0 && std::cmp_less(row, m_rowSizes.size()); }
    bool IsValidColumn(const int column) const
    {
        return column >= 0 && std::cmp_less(column, m_columnSizes.size());
    }
    bool IsValidPane(const int row, const int column) const { return IsValidRow(row) && IsValidColumn(column); }

    CRect WorkRect() const;
    CRect TrackerRect(int pos) const;
    void DrawBackground(CDC& dc, const CRect& rect) const;
    void DrawTrackerRect(CDC& dc, const CRect& rect) const;
    void DrawTracker();
    void FinishTracking(bool bAccept, bool releaseCapture);

    std::vector<int> m_rowSizes;
    std::vector<int> m_columnSizes;
    bool m_bTracking = false;
    bool m_bTrackingColumn = false;
    bool m_bTrackerVisible = false;
    int m_nTrackPos = 0;
    CRect m_rectTracker;
};

inline void CSplitterWnd::UpdateLayout()
{
    if (!IsWindow(m_hWnd) || m_rowSizes.empty() || m_columnSizes.empty()) return;
    CRect rcWork = GetClientRect();
    rcWork.Deflate(PaneBorderSize, PaneBorderSize);
    const auto movePane = [&](const int row, const int column, CRect rect)
    {
        CWnd* pane = GetPane(row, column);
        if (pane == nullptr || !IsWindow(pane->m_hWnd)) return;
        if (pane->IsSplitterWindow()) rect.Inflate(PaneBorderSize, PaneBorderSize);
        pane->MoveWindow(rect);
    };
    if (m_columnSizes.size() > 1)
    {
        const int total = rcWork.Width();
        constexpr int bar = SplitterSize;
        int w0 = m_columnSizes[0];
        if (w0 < 0) w0 = (total - bar) / 2;
        w0 = std::clamp(w0, 0, std::max(0, total - bar));
        const int w1 = std::max(0, total - bar - w0);
        m_columnSizes[0] = w0; m_columnSizes[1] = w1;
        movePane(0, 0, CRect(rcWork.left, rcWork.top, rcWork.left + w0, rcWork.bottom));
        movePane(0, 1, CRect(rcWork.left + w0 + bar, rcWork.top, rcWork.right, rcWork.bottom));
    }
    else if (m_rowSizes.size() > 1)
    {
        const int total = rcWork.Height();
        constexpr int bar = SplitterSize;
        int h0 = m_rowSizes[0];
        if (h0 < 0) h0 = (total - bar) / 2;
        h0 = std::clamp(h0, 0, std::max(0, total - bar));
        const int h1 = std::max(0, total - bar - h0);
        m_rowSizes[0] = h0; m_rowSizes[1] = h1;
        movePane(0, 0, CRect(rcWork.left, rcWork.top, rcWork.right, rcWork.top + h0));
        movePane(1, 0, CRect(rcWork.left, rcWork.top + h0 + bar, rcWork.right, rcWork.bottom));
    }
    else movePane(0, 0, rcWork);
    Invalidate(false);
}
inline CRect CSplitterWnd::WorkRect() const
{
    CRect rc = GetClientRect();
    rc.Deflate(PaneBorderSize, PaneBorderSize);
    return rc;
}

inline CRect CSplitterWnd::TrackerRect(const int pos) const
{
    const CRect rcWork = WorkRect();
    if (m_bTrackingColumn)
    {
        const int x = rcWork.left + std::clamp(pos, 0, std::max(0, rcWork.Width() - SplitterSize));
        return CRect(x, rcWork.top, x + SplitterSize, rcWork.bottom);
    }

    const int y = rcWork.top + std::clamp(pos, 0, std::max(0, rcWork.Height() - SplitterSize));
    return CRect(rcWork.left, y, rcWork.right, y + SplitterSize);
}

inline void CSplitterWnd::DrawTrackerRect(CDC& dc, const CRect& rect) const
{
    if (!rect.IsEmpty())
        dc.PatBlt(rect.left, rect.top, rect.Width(), rect.Height(), DSTINVERT);
}

inline void CSplitterWnd::DrawTracker()
{
    if (!IsWindow(m_hWnd) || m_rectTracker.IsEmpty())
        return;

    CClientDC dc(this);
    DrawTrackerRect(dc, m_rectTracker);
}

inline void CSplitterWnd::FinishTracking(const bool bAccept, const bool releaseCapture)
{
    if (!m_bTracking)
        return;

    const bool trackingColumn = m_bTrackingColumn;
    const int trackPos = m_nTrackPos;

    if (m_bTrackerVisible)
        DrawTracker();

    m_bTracking = false;
    m_bTrackerVisible = false;
    m_rectTracker.Clear();

    if (releaseCapture && HasCapture())
        ReleaseCapture();

    if (!bAccept)
    {
        Invalidate(false);
        return;
    }

    if (trackingColumn && m_columnSizes.size() > 1)
        m_columnSizes[0] = trackPos;
    else if (!trackingColumn && m_rowSizes.size() > 1)
        m_rowSizes[0] = trackPos;

    UpdateLayout();
}

inline void CSplitterWnd::StopTracking(const bool bAccept) { FinishTracking(bAccept, true); }
inline void CSplitterWnd::OnLButtonDown(UINT, const CPoint pt)
{
    if (m_bTracking)
        StopTracking(false);

    const CRect rcWork = WorkRect();
    bool onBar = false;
    bool trackingColumn = false;
    int trackPos = 0;

    if (m_columnSizes.size() > 1)
    {
        trackingColumn = true;
        trackPos = std::clamp(m_columnSizes[0], 0, std::max(0, rcWork.Width() - SplitterSize));
        const int x = rcWork.left + trackPos;
        onBar = (pt.x >= x && pt.x <= x + SplitterSize);
    }
    else if (m_rowSizes.size() > 1)
    {
        trackPos = std::clamp(m_rowSizes[0], 0, std::max(0, rcWork.Height() - SplitterSize));
        const int y = rcWork.top + trackPos;
        onBar = (pt.y >= y && pt.y <= y + SplitterSize);
    }

    if (!onBar)
        return;

    m_bTracking = true;
    m_bTrackingColumn = trackingColumn;
    m_nTrackPos = trackPos;
    m_rectTracker = TrackerRect(m_nTrackPos);
    m_bTrackerVisible = !m_rectTracker.IsEmpty();
    SetCapture();
    if (m_bTrackerVisible)
        DrawTracker();
}
inline void CSplitterWnd::OnMouseMove(UINT, const CPoint pt)
{
    if (!m_bTracking) return;

    const CRect rcWork = WorkRect();
    const int trackPos = m_bTrackingColumn ?
        std::clamp(static_cast<int>(pt.x - rcWork.left), 0, std::max(0, rcWork.Width() - SplitterSize)) :
        std::clamp(static_cast<int>(pt.y - rcWork.top), 0, std::max(0, rcWork.Height() - SplitterSize));
    if (trackPos == m_nTrackPos)
        return;

    if (m_bTrackerVisible)
        DrawTracker();

    m_nTrackPos = trackPos;
    m_rectTracker = TrackerRect(m_nTrackPos);
    m_bTrackerVisible = !m_rectTracker.IsEmpty();
    if (m_bTrackerVisible)
        DrawTracker();
}
inline void CSplitterWnd::OnLButtonUp(UINT, CPoint) { if (m_bTracking) StopTracking(true); }
inline void CSplitterWnd::OnCaptureChanged(WindowRef pWnd)
{
    if (m_bTracking && (pWnd == nullptr || pWnd.Handle() != m_hWnd))
        FinishTracking(false, false);
}
inline void CSplitterWnd::OnCancelMode() { StopTracking(false); }
inline bool CSplitterWnd::OnSetCursor(WindowRef, const UINT nHitTest, UINT)
{
    if (nHitTest == HTCLIENT)
    {
        const auto pt = GetClientCursorPos();
        if (!pt) return static_cast<bool>(CallDefaultHandler());
        const CRect rcWork = WorkRect();
        if (m_columnSizes.size() > 1)
        {
            const int x = rcWork.left + m_columnSizes[0];
            if (pt->x >= x && pt->x <= x + SplitterSize)
            {
                SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                return true;
            }
        }
        else if (m_rowSizes.size() > 1)
        {
            const int y = rcWork.top + m_rowSizes[0];
            if (pt->y >= y && pt->y <= y + SplitterSize)
            {
                SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
                return true;
            }
        }
    }
    return static_cast<bool>(CallDefaultHandler());
}

// -----------------------------------------------------------------------------
//  Theme helpers
// -----------------------------------------------------------------------------
inline constexpr UINT WM_WDS_TAB_CHANGING = WM_APP + 0x100;
inline constexpr UINT WM_WDS_TAB_CHANGED = WM_APP + 0x101;

inline COLORREF BlendColor(const COLORREF from, const COLORREF to, double amount)
{
    const auto ch = [amount](const BYTE a, const BYTE b) -> BYTE
        {
            return static_cast<BYTE>(std::clamp<int>(static_cast<int>(a + (b - a) * amount), 0, 255));
        };
    return RGB(ch(GetRValue(from), GetRValue(to)), ch(GetGValue(from), GetGValue(to)), ch(GetBValue(from), GetBValue(to)));
}

// -----------------------------------------------------------------------------
//  CToolBar
// -----------------------------------------------------------------------------
class CToolBarButton
{
public:
    CToolBarButton(const UINT id, int image, std::wstring text = {})
        : m_id(id), m_image(image), m_text(std::move(text)) {}
    CToolBarButton(const UINT id, std::wstring text) : m_id(id), m_text(std::move(text)) {}

    UINT m_id;
    std::optional<int> m_image;
    std::wstring m_text;
};

class CToolBar : public MessageTarget<CToolBar, CWnd>
{
public:
    ~CToolBar() override
    {
        if (m_imageList != nullptr) ImageList_Destroy(m_imageList);
        if (m_disabledImageList != nullptr) ImageList_Destroy(m_disabledImageList);
    }

    void SetMetrics(const SIZE& buttonSize, const int imageSize)
    {
        SetButtonSize(buttonSize);
        if (m_imageList != nullptr && !::ImageList_RemoveAll(m_imageList))
        {
            ImageList_Destroy(m_imageList);
            m_imageList = nullptr;
        }
        if (m_disabledImageList != nullptr && !::ImageList_RemoveAll(m_disabledImageList))
        {
            ImageList_Destroy(m_disabledImageList);
            m_disabledImageList = nullptr;
        }

        if (imageSize != m_imageSize)
        {
            m_imageSize = imageSize;
            RecreateImageLists();
        }
    }
    int AddImage(const CBitmap& bmp);

    bool Create(CFrameWnd* parent)
    {
        constexpr DWORD style = WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_LIST |
            CCS_NORESIZE | CCS_NOPARENTALIGN | CCS_NODIVIDER | TBSTYLE_TRANSPARENT;
        const CRect rect(0, 0, 0, 0);
        if (!CreateEx(0, TOOLBARCLASSNAMEW, nullptr, style, rect, parent, WDS_TOOLBAR_ID)) return false;
        SendNativeMessage(TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON));
        SendNativeMessage(TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_HIDECLIPPEDBUTTONS);
        SendNativeMessage(TB_SETIMAGELIST, 0, m_imageList);
        SendNativeMessage(TB_SETDISABLEDIMAGELIST, 0, m_disabledImageList);
        parent->SetTopBar(this);
        return true;
    }

    int GetButtonCount() const { return static_cast<int>(SendNativeMessage(TB_BUTTONCOUNT)); }
    void ClearButtons()
    {
        for (int i = GetButtonCount(); i-- > 0;) SendNativeMessage(TB_DELETEBUTTON, static_cast<WPARAM>(i));
        m_tips.clear();
    }
    void AddSeparator()
    {
        TBBUTTON b{};
        b.fsStyle = BTNS_SEP;
        b.iBitmap = MulDiv(m_imageSize, 8, 20);
        SendNativeMessage(TB_INSERTBUTTONW, GetButtonCount(), &b);
    }
    void AddButton(const CToolBarButton& btn)
    {
        TBBUTTON b{};
        b.iBitmap = btn.m_image.value_or(I_IMAGENONE);
        b.idCommand = static_cast<int>(btn.m_id);
        b.fsState = 0;
        b.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
        b.iString = -1;
        if (!btn.m_image) b.fsStyle |= BTNS_SHOWTEXT;
        if (!btn.m_text.empty())
        {
            std::wstring wstr = btn.m_text;
            wstr.push_back(L'\0');
            const LRESULT idx = SendNativeMessage(TB_ADDSTRINGW, 0, wstr.c_str());
            b.iString = (idx != -1) ? idx : -1;
            m_tips[btn.m_id] = btn.m_text;
        }
        SendNativeMessage(TB_INSERTBUTTONW, GetButtonCount(), &b);
    }
    void SetButtonSize(const SIZE& buttonSize) noexcept { m_buttonSize = buttonSize; }
    void UpdateLayout()
    {
        SendNativeMessage(TB_SETIMAGELIST, 0, m_imageList);
        SendNativeMessage(TB_SETDISABLEDIMAGELIST, 0, m_disabledImageList);
        SendNativeMessage(TB_SETBITMAPSIZE, 0, MAKELPARAM(m_imageSize, m_imageSize));
        SendNativeMessage(TB_SETBUTTONSIZE, 0, MAKELPARAM(m_buttonSize.cx, m_buttonSize.cy));
        SendNativeMessage(TB_AUTOSIZE);
        auto* frame = GetParent<CFrameWnd>();
        frame->UpdateLayout();
        const auto toolbar = this;
        const HWND hWnd = m_hWnd;
        g_idleCmdUiUpdate = [toolbar, hWnd]
            {
                if (FindAttached(hWnd) != toolbar) return;
                toolbar->OnUpdateCmdUI(toolbar->GetParent<CFrameWnd>());

            };
        OnUpdateCmdUI(frame);
        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    }
    int GetButtonIndex(const UINT id) const { return static_cast<int>(SendNativeMessage(TB_COMMANDTOINDEX, id)); }
    bool GetButtonRect(const int index, RECT& rect) const
    {
        return static_cast<bool>(SendNativeMessage(TB_GETITEMRECT, static_cast<WPARAM>(index),
            &rect));
    }
    bool SetButtonVisible(const int index, const bool visible)
    {
        TBBUTTON button{};
        if (index < 0 || index >= GetButtonCount() ||
            !SendNativeMessage(TB_GETBUTTON, static_cast<WPARAM>(index), &button))
            return false;

        const bool hidden = (button.fsState & TBSTATE_HIDDEN) != 0;
        if (hidden == !visible) return false;
        TBBUTTONINFOW info{ .cbSize = sizeof(info), .dwMask = TBIF_BYINDEX | TBIF_STATE,
            .fsState = static_cast<BYTE>((button.fsState & ~TBSTATE_HIDDEN) | (visible ? 0 : TBSTATE_HIDDEN)) };
        return SendNativeMessage(TB_SETBUTTONINFOW, index, &info) != FALSE;
    }
    CSize GetButtonSize() const
    {
        if (GetButtonCount() == 0) return m_buttonSize;
        const DWORD r = static_cast<DWORD>(SendNativeMessage(TB_GETBUTTONSIZE));
        const CSize sz(LOWORD(r), HIWORD(r));
        return sz.cx > 0 && sz.cy > 0 ? sz : m_buttonSize;
    }
    CSize PreferredSize() const override
    {
        const CRect rcParent = GetParent().GetClientRect();
        return CSize(rcParent.Width(), GetButtonSize().cy + ::ScaleForScreenDpi(2, m_hWnd));
    }

    void OnUpdateCmdUI(CFrameWnd* pTarget)
    {
        struct CToolBarCmdUI : CCmdUI
        {
            CToolBar* pBar = nullptr;
            void Enable(const bool bOn) override { pBar->SendNativeMessage(TB_ENABLEBUTTON, m_nID, MAKELONG(bOn, 0)); }
            void SetCheck(const int nCheck) override { pBar->SendNativeMessage(TB_CHECKBUTTON, m_nID, MAKELONG(nCheck != 0, 0)); }
            void SetRadio(const bool bOn) override { SetCheck(bOn); }
            void SetText(LPCWSTR) override {}
        };
        const int n = GetButtonCount();
        CToolBarCmdUI state; state.pBar = this;
        for (const int i : std::views::iota(0, n))
        {
            TBBUTTON b{}; SendNativeMessage(TB_GETBUTTON, static_cast<WPARAM>(i), &b);
            if (b.idCommand == 0 || (b.fsStyle & BTNS_SEP)) continue;
            state.m_nID = static_cast<UINT>(b.idCommand);
            state.m_nIndex = static_cast<UINT>(i);
            state.Update(pTarget, true);
        }
    }

    static std::span<const RouteEntry> Routes()
    {
        static constexpr std::array entries
        {
            Route::ReflectNotify<&OnCustomDraw>(NM_CUSTOMDRAW),
            Route::ReflectNotify<&OnGetInfoTip>(TBN_GETINFOTIPW),
            Route::Window<&OnCtlColor>(WM_CTLCOLOR),
        };
        return entries;
    }

private:
    HBRUSH OnCtlColor(CDC* dc, WindowRef control, UINT type);
    void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult) const;
    void OnGetInfoTip(NMHDR* pNMHDR, LRESULT* pResult)
    {
        const auto* tip = reinterpret_cast<LPNMTBGETINFOTIPW>(pNMHDR);
        tip->pszText[0] = L'\0';
        if (const auto it = m_tips.find(static_cast<UINT>(tip->iItem)); it != m_tips.end())
            wcsncpy_s(tip->pszText, tip->cchTextMax, it->second.c_str(), _TRUNCATE);
        *pResult = 0;
    }

    void RecreateImageLists()
    {
        if (m_imageList) ImageList_Destroy(m_imageList);
        if (m_disabledImageList) ImageList_Destroy(m_disabledImageList);
        m_imageList = nullptr;
        m_disabledImageList = nullptr;

        m_imageList = ImageList_Create(m_imageSize, m_imageSize, ILC_COLOR32, 0, 16);
        if (m_imageList) ImageList_SetBkColor(m_imageList, CLR_NONE);

        m_disabledImageList = ImageList_Create(m_imageSize, m_imageSize, ILC_COLOR32, 0, 16);
        if (m_disabledImageList) ImageList_SetBkColor(m_disabledImageList, CLR_NONE);
    }

    HIMAGELIST m_imageList = nullptr;
    HIMAGELIST m_disabledImageList = nullptr;
    int m_imageSize = 16;
    CSize m_buttonSize{ 23, 22 };
    std::map<UINT, std::wstring> m_tips;
};

// -----------------------------------------------------------------------------
//  CStatusBar (custom drawn)
// -----------------------------------------------------------------------------
class CStatusBar : public MessageTarget<CStatusBar, CWnd>
{
public:
    enum class PaneId : char { Idle, Size, Ram };

    bool Create(CFrameWnd* pParentWnd)
    {
        static constexpr wchar_t kCls[] = L"WdsStatusBar32";
        WNDCLASSEXW wc{ sizeof(wc) };
        wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = FrameworkWindowProc;
        wc.hInstance = GetAppInstance();
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kCls;
        RegisterClassExW(&wc);
        const CRect rc(0, 0, 0, 0);
        if (!CreateEx(0, kCls, nullptr, WS_CHILD | WS_VISIBLE, rc, pParentWnd, WDS_STATUS_BAR_ID)) return false;
        pParentWnd->SetBottomBar(this);
        return true;
    }
    void SetPaneText(const PaneId pane, const std::wstring_view text, const int width)
    {
        auto& target = m_panes[std::to_underlying(pane)];
        if (target.text == text && target.width == width) return;
        target.text.assign(text);
        target.width = width;
        Invalidate();
    }
    void SetBackgroundColor(const COLORREF color) { m_background = color; Invalidate(); }
    CRect GetPaneRect(const PaneId pane) const
    {
        return LayoutPanes()[std::to_underlying(pane)];
    }

    CSize PreferredSize() const override
    {
        const CRect rcParent = GetParent().GetClientRect();
        return CSize(rcParent.Width(), ::ScaleForDpi(22, m_hWnd));
    }

    static void DrawPaneBorder(CDC& dc, const CRect& rect);

    static std::span<const RouteEntry> Routes()
    {
        static constexpr std::array entries
        {
            Route::Window<&OnPaint>(WM_PAINT),
            Route::Window<&OnEraseBkgnd>(WM_ERASEBKGND),
            Route::Window<&OnSize>(WM_SIZE),
        };
        return entries;
    }

private:
    struct Pane { int width = 100; std::wstring text; };

    bool OnEraseBkgnd(CDC*) { return true; }
    void OnSize(UINT, int, int) { Invalidate(); }
    void OnPaint();

    std::array<CRect, 3> LayoutPanes() const
    {
        std::array<CRect, 3> rects{};
        if (!IsWindow(m_hWnd)) return rects;
        const CRect rc = GetClientRect();
        const int fixedTotal = m_panes[1].width + m_panes[2].width;
        const int stretchWidth = std::max(0, rc.Width() - fixedTotal);
        int x = rc.left;
        for (const auto [i, pane] : std::views::enumerate(m_panes))
        {
            const int w = i == 0 ? stretchWidth : pane.width;
            rects[i] = CRect(x, rc.top, x + w, rc.bottom);
            x += w;
        }
        rects.back().right = rc.right;   // last pane snaps to edge
        return rects;
    }
    std::array<Pane, 3> m_panes;
    COLORREF m_background = CLR_NONE;
};

// -----------------------------------------------------------------------------
//  CTabControl
// -----------------------------------------------------------------------------
class CTabControl : public MessageTarget<CTabControl, CWnd>
{
public:
    enum class Location : char { Bottom, Top };

    bool Create(const RECT& rect, CWnd* pParentWnd, UINT nID, bool focusTabStrip = false);
    int AddTab(CWnd* window, std::wstring_view label);
    bool SelectTab(const int i) { return ActivateTab(i, true); }
    void SetTabVisible(int i, bool show);

    void SetLocation(Location loc);
    void SetContentBackgroundColor(COLORREF color);
    void OnAppearanceChanged() override;
    int GetTabCount() const { return static_cast<int>(m_tabs.size()); }
    int GetActiveTab() const { return m_activeTab; }
    CWnd* GetTabWindow(const int index) const { return index >= 0 && index < GetTabCount() ? m_tabs[index].window : nullptr; }
    bool IsTabVisible(const int index) const { return index >= 0 && index < GetTabCount() && m_tabs[index].visible; }
    std::wstring_view GetTabLabel(int index) const;
    void SetTabLabel(int i, std::wstring_view label);

    bool PreprocessMessage(MSG* pMsg) override;
    static std::span<const RouteEntry> Routes();

protected:
    LRESULT OnNcHitTest(CPoint point);
    void OnFontSizeChanged(int oldPercent, int newPercent) override;
    void OnSize(UINT, int, int);
    bool OnEraseBkgnd(CDC*) { return true; }
    void OnLButtonDown(UINT, CPoint point);
    void OnLButtonUp(UINT, CPoint);
    void OnSetFocus(WindowRef);
    void OnKillFocus(WindowRef);
    void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    void OnTabSelectionChanged(NMHDR*, LRESULT* pResult);
    void OnPaint();

private:
    struct TabInfo { CWnd* window = nullptr; std::wstring label; bool visible = true; CRect paintedRect; };

    static bool IsPlainTabTraversal(const MSG& msg);
    static bool IsDarkColor(COLORREF color);
    int NativeIndexFromLogical(int logical) const;
    void RebuildNativeTabs();
    void UpdateNativePadding();
    void SyncNativeSelection();
    bool ActivateTab(int i, bool syncNative);
    void NotifyParentOfTabChange(int activeTab) const;
    bool ShouldMoveFocusOnTabActivation(int previousActiveTab) const;
    bool ForwardKeyboardMessageToActiveTab(const MSG& msg);
    bool FocusActiveTabWindow();
    bool RedirectFocusAwayFromTabControl();
    bool GetNativeItemRect(int native, CRect& rc) const;
    CRect TabStripRect() const;
    void UpdatePaintedTabRects(const CRect& rcStrip, bool bottomTabs, bool labelOnlyTabs);
    bool UsesLabelOnlyTabs() const;
    int TabStripHeight() const { return ::ScaleForDpi(22, m_hWnd); }
    void LayoutPanes();

    std::vector<TabInfo> m_tabs;
    std::vector<int> m_visibleToLogical;
    int m_activeTab = -1;
    Location m_location = Location::Bottom;
    COLORREF m_paneBackgroundColor = CLR_NONE;
    bool m_handledPaintedTabMouseDown = false;
    bool m_focusTabStrip = false;
};

// -----------------------------------------------------------------------------
//  CPropertyPage / CPropertySheet
// -----------------------------------------------------------------------------
class CPropertySheet;

class CPropertyPage : public CDialog
{
public:
    explicit CPropertyPage(const UINT templateId) : CDialog(templateId) {}

    bool Create(WindowRef parent);

    struct ValidationError { HWND control; std::wstring message; };
    virtual std::optional<ValidationError> PrepareApply() { return {}; }
    virtual void BeforeApply() {}
    virtual void CommitApply() { OnOK(); }
    virtual void AfterApply() {}
    void SetModified(bool bChanged = true);
    void OnOK() override {}        // a page does not end its own dialog
    void OnCancel() override {}

private:
    friend class CPropertySheet;
    bool m_bModified = false;
};

class CPropertySheet : public MessageTarget<CPropertySheet, CDialog>
{
public:
    explicit CPropertySheet(std::wstring caption) noexcept : MessageTarget(0), m_caption(std::move(caption)) {}

    template<typename Page, typename... Args>
        requires std::derived_from<Page, CPropertyPage>
    void AddPage(Args&&... args)
    {
        m_pages.push_back(std::make_unique<Page>(std::forward<Args>(args)...));
    }
    int GetPageCount() const { return static_cast<int>(m_pages.size()); }
    int GetActivePageIndex() const { return m_currentPage >= 0 ? m_currentPage : m_tab.GetActiveTab(); }
    bool SelectPage(int i);
    CTabControl& GetTabControl() { return m_tab; }

    void UpdateApplyButton();

    virtual INT_PTR ShowModal();

    static std::span<const RouteEntry> Routes();

protected:
    virtual bool OnInitDialog();
    virtual bool ConfirmApply(UINT) { return true; }
    virtual void OnApplied(UINT) {}
    bool PreprocessMessage(MSG* pMsg) override;
    bool OnEraseBkgnd(CDC* pDC) const;
    HBRUSH OnCtlColor(CDC* pDC, WindowRef pWnd, UINT nCtlColor);
    LRESULT OnRequestedPageChanged(WPARAM wParam, LPARAM lParam);
    void OnClose() { if (!m_applying) RequestModalExit(IDCANCEL); }
    void RequestModalExit(const int result) { CloseModal(result); }
    bool OnCommand(WPARAM wParam, LPARAM lParam) override;

private:
    bool EnsurePageCreated(int i);
    bool ActivatePage(int active);
    void SyncTabSelection(int active);
    bool ApplyPages(UINT command);

    std::vector<std::unique_ptr<CPropertyPage>> m_pages;
    CTabControl m_tab;
    HWND m_applyButton = nullptr;
    CRect m_pageRect;
    std::wstring m_caption;
    int m_currentPage = -1;
    int m_pendingActivePage = 0;
    bool m_syncingTabSelection = false;
    bool m_applying = false;
};
