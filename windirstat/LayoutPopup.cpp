// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "LayoutPopup.h"
#include "DarkMode.h"
#include "MainFrame.h"

#include <algorithm>

// Splitter structure, default positions, and proportions used by the preview cards.
const std::array<CLayoutPopup::LayoutDef, LayoutCount> CLayoutPopup::LAYOUTS = []
{
    using enum CSplitterWnd::Orientation;
    return std::to_array<LayoutDef>({
        // [AF|FT] above visualization
        { Horizontal, Vertical, 0, { VT_VISUALIZATION, VT_ALLFILES, VT_FILETYPES }, 0.5, 0.75, 0.5f, 0.58f },
        // AF | [FT|visualization]
        { Vertical, Vertical, 1, { VT_ALLFILES, VT_FILETYPES, VT_VISUALIZATION }, 0.4, 1.0 / 3.0, 0.4f, 1.0f / 3.0f },
        // [visualization|AF] | FT
        { Vertical, Vertical, 0, { VT_FILETYPES, VT_VISUALIZATION, VT_ALLFILES }, 0.8, 0.5, 0.8f, 0.5f },
        // Visualization above [AF|FT]
        { Horizontal, Vertical, 1, { VT_VISUALIZATION, VT_ALLFILES, VT_FILETYPES }, 0.5, 0.75, 0.5f, 0.58f },
        // [AF|visualization] | FT
        { Vertical, Vertical, 0, { VT_FILETYPES, VT_ALLFILES, VT_VISUALIZATION }, 0.8, 0.5, 0.8f, 0.5f },
        // Visualization | [FT|AF]
        { Vertical, Vertical, 1, { VT_VISUALIZATION, VT_FILETYPES, VT_ALLFILES }, 0.4, 1.0 / 3.0, 0.4f, 1.0f / 3.0f },
        // [AF/visualization] | FT
        { Vertical, Horizontal, 0, { VT_FILETYPES, VT_ALLFILES, VT_VISUALIZATION }, 0.75, 0.5, 0.6f, 0.5f },
        // [AF/FT] | visualization
        { Vertical, Horizontal, 0, { VT_VISUALIZATION, VT_ALLFILES, VT_FILETYPES }, 0.5, 0.5, 0.5f, 0.5f },
        // Visualization | [AF/FT]
        { Vertical, Horizontal, 1, { VT_VISUALIZATION, VT_ALLFILES, VT_FILETYPES }, 0.5, 0.5, 0.5f, 0.5f },
        // [visualization/AF] | FT
        { Vertical, Horizontal, 0, { VT_FILETYPES, VT_VISUALIZATION, VT_ALLFILES }, 0.75, 0.5, 0.6f, 0.5f },
        // [FT/AF] | visualization
        { Vertical, Horizontal, 0, { VT_VISUALIZATION, VT_FILETYPES, VT_ALLFILES }, 0.5, 0.5, 0.5f, 0.5f },
        // Visualization | [FT/AF]
        { Vertical, Horizontal, 1, { VT_VISUALIZATION, VT_FILETYPES, VT_ALLFILES }, 0.5, 0.5, 0.5f, 0.5f },
    });
}();

std::array<CLayoutPopup::Pane, 3> CLayoutPopup::LayoutDef::PreviewPanes() const
{
    // Split the preview with the same pane placement as the live windows.
    const auto split = [](const Pane& bounds, const CSplitterWnd::Orientation axis, const float ratio)
    {
        std::array<Pane, 2> panes{ bounds, bounds };
        if (axis == CSplitterWnd::Orientation::Vertical)
        {
            panes[0].w *= ratio;
            panes[1].x += panes[0].w;
            panes[1].w -= panes[0].w;
        }
        else
        {
            panes[0].h *= ratio;
            panes[1].y += panes[0].h;
            panes[1].h -= panes[0].h;
        }
        return panes;
    };
    auto main = split({ views[0], 0, 0, 1, 1 }, orientation, previewRatio);
    auto sub = split(main[subPane], subOrientation, previewSubRatio);
    sub[0].viewType = views[1];
    sub[1].viewType = views[2];
    return { main[1 - subPane], sub[0], sub[1] };
}

bool CLayoutPopup::Create(CWnd* parent)
{
    static ATOM s_atom = 0;
    if (s_atom == 0)
    {
        const WNDCLASSEX wc = {
            .cbSize        = sizeof(WNDCLASSEX),
            .style         = CS_DROPSHADOW,
            .lpfnWndProc   = ::DefWindowProc,
            .hInstance     = GetAppInstance(),
            .hCursor       = LoadCursor(nullptr, IDC_ARROW),
            .hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1),
            .lpszClassName = L"WdsLayoutPopup",
        };
        s_atom = ::RegisterClassEx(&wc);
        if (s_atom == 0) return false;
    }

    return CreateEx(
        WS_EX_TOOLWINDOW,
        L"WdsLayoutPopup",
        nullptr,
        WS_POPUP | WS_BORDER,
        0, 0, 1, 1,
        parent->Handle(),
        nullptr);
}

void CLayoutPopup::ShowAtButton(const CRect& buttonScreenRect)
{
    m_selectedLayout = COptions::WindowLayout;
    m_hoveredLayout  = -1;

    const CRect lastCard = CardRect(COLS * ROWS - 1);
    const int margin = ScaleForDpi(MARGIN_BASE);
    const int popupW = lastCard.right + margin;
    const int popupH = lastCard.bottom + margin;

    // Default: anchor below the button, left-aligned
    CPoint origin(buttonScreenRect.left, buttonScreenRect.bottom + ScaleForDpi(2));

    // Fit to work area
    MONITORINFO mi{ .cbSize = sizeof(mi) };
    const HMONITOR hMon = MonitorFromPoint(origin, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfo(hMon, &mi);
    const CRect work = mi.rcWork;

    if (origin.x + popupW > work.right)
        origin.x = work.right - popupW;
    origin.x = std::max(origin.x, work.left);
    if (origin.y + popupH > work.bottom)
        origin.y = buttonScreenRect.top - popupH - ScaleForDpi(2);
    origin.y = std::max(origin.y, work.top);

    SetWindowPos(nullptr, origin.x, origin.y, popupW, popupH,
        SWP_NOZORDER | SWP_SHOWWINDOW);
    SetFocus();
    SetCapture();
    Invalidate();
}

void CLayoutPopup::DismissPopup(const bool cancel, const bool resetPositions)
{
    ShowWindow(SW_HIDE);

    if (HasCapture())
        ReleaseCapture();

    if (HasFocus())
        if (auto parent = GetParent())
            parent.SetFocus();

    if (!cancel)
        CMainFrame::Get()->RebuildLayout(resetPositions);
}

CRect CLayoutPopup::CardRect(const int idx) const
{
    const int cw = ScaleForDpi(CARD_W_BASE);
    const int ch = ScaleForDpi(CARD_H_BASE);
    const int gp = ScaleForDpi(GAP_BASE);
    const int mg = ScaleForDpi(MARGIN_BASE);

    const int col = idx % COLS;
    const int row = idx / COLS;
    const int x   = mg + col * (cw + gp);
    const int y   = mg + row * (ch + gp);

    return { x, y, x + cw, y + ch };
}

int CLayoutPopup::CardAtPoint(const CPoint pt) const
{
    const auto range = std::views::iota(0, LayoutCount);
    const auto it = std::ranges::find_if(range,
        [&](const int i) { return CardRect(i).Contains(pt); });
    return it != range.end() ? *it : -1;
}

static void DrawPaneHeader(CDC& dc, const CRect& r, const int rowH, std::initializer_list<int> columnEnds)
{
    const bool dark = DarkMode::IsDarkModeActive();
    const COLORREF separator = DarkMode::SystemColor(dark ? COLOR_3DHIGHLIGHT : COLOR_3DSHADOW);
    dc.FillSolidRect(r.left, r.top, r.Width(), rowH + 1, DarkMode::SystemColor(COLOR_BTNFACE));
    dc.FillSolidRect(r.left, r.top + rowH + 1, r.Width(), 1, separator);

    // Draw each column label between its inset edges and divide neighboring columns.
    int left = r.left;
    for (const auto [index, right] : std::views::enumerate(columnEnds))
    {
        const int width = right - left - 4;
        if (width > 2 && rowH > 0)
        {
            const int stroke = std::max(2, width * (55 + static_cast<int>(index) * 10) / 100);
            dc.FillSolidRect(left + 2, r.top + (rowH - 1) / 2, stroke, 1, dark ? RGB(176, 176, 176) : RGB(88, 88, 88));
        }
        if (right > r.left && right < r.right) dc.FillSolidRect(right, r.top + 1, 1, rowH, separator);
        left = right;
    }
}

void CLayoutPopup::DrawListPane(CDC& dc, const CRect r, const LAYOUT_VIEW_TYPE viewType) const
{
    struct FileRow { int indent; float fraction; };
    static constexpr std::array fileRows = {
        FileRow{0, 0.95f}, FileRow{1, 0.37f}, FileRow{1, 0.29f}, FileRow{1, 0.16f},
        FileRow{2, 0.10f}, FileRow{2, 0.07f}, FileRow{1, 0.04f}, FileRow{1, 0.02f},
    };
    static constexpr std::array typeColors = {
        RGB(65, 115, 220), RGB(185, 130, 40), RGB(45, 165, 80), RGB(55, 125, 200),
        RGB(140, 140, 170), RGB(190, 65, 65), RGB(45, 165, 165), RGB(155, 65, 195),
    };

    // The file tree has a percentage column. The file-type list has a description and a count.
    const bool files = viewType == VT_ALLFILES;
    const bool dark = DarkMode::IsDarkModeActive();
    const int rowH = ScaleForDpi(7);
    const int icon = std::max(3, rowH - 2);
    const int textH = std::max(2, icon - 1);
    const int firstColumnEnd = r.left + r.Width() * (files ? 54 : 28) / 100;
    const int countColumn = r.left + r.Width() * 68 / 100;
    COLORREF bg, alt;
    if (files)
    {
        bg = dark ? RGB(26, 24, 22) : RGB(255, 254, 249);
        alt = dark ? RGB(34, 32, 28) : RGB(247, 245, 240);
        DrawPaneHeader(dc, r, rowH, { firstColumnEnd, r.right });
    }
    else
    {
        bg = dark ? RGB(22, 24, 26) : RGB(249, 252, 255);
        alt = dark ? RGB(28, 30, 36) : RGB(241, 245, 252);
        DrawPaneHeader(dc, r, rowH, { firstColumnEnd, countColumn, r.right });
    }

    // Paint the visible sample rows with the same spacing and alternating backgrounds in both lists.
    const int count = static_cast<int>(files ? fileRows.size() : typeColors.size());
    for (int i = 0, top = r.top + rowH + 2; i < count && top < r.bottom; ++i, top += rowH)
    {
        dc.FillSolidRect(r.left, top, r.Width(), std::min<int>(rowH, r.bottom - top), i % 2 ? alt : bg);
        const int y = top + (rowH - icon + 1) / 2;
        const auto drawText = [&](const int x, const int width, const int seed)
        {
            if (width > 2) dc.FillSolidRect(x, y + (textH - 1) / 2, std::max(2, width - seed % 4), 1,
                dark ? RGB(204, 204, 204) : RGB(52, 52, 52));
        };

        if (files)
        {
            const auto [indent, fraction] = fileRows[i];
            const int x = r.left + 2 + indent * std::max(2, r.Width() / 9);
            if (x + icon < firstColumnEnd - 2) dc.FillSolidRect(x, y, icon, icon, RGB(228, 178, 36));
            const int textX = x + icon + 2;
            drawText(textX, (firstColumnEnd - textX - 2) * 2 / 3, i);
            const int width = static_cast<int>((r.right - firstColumnEnd - 2) * fraction);
            const int colors = std::clamp(static_cast<int>(COptions::FileTreeColorCount), 1, TREELISTCOLORCOUNT);
            if (width > 0) dc.FillSolidRect(firstColumnEnd, y, width, textH, COptions::FileTreeColors[i % colors]);
        }
        else
        {
            const int x = r.left + 2;
            if (x + icon < countColumn - 2) dc.FillSolidRect(x, y, icon, icon, typeColors[i]);
            const int textX = x + icon + 2;
            if (textX < countColumn - 4) drawText(textX, firstColumnEnd - textX, i);
            const int descriptionX = firstColumnEnd + 2;
            drawText(descriptionX, (countColumn - descriptionX - 2) * 3 / 4, i + 1);
            drawText(countColumn + 2, (r.right - countColumn - 2) * 2 / 3, i + 2);
        }
    }
}

void CLayoutPopup::DrawVisualizationPane(CDC& dc, const CRect r) const
{
    static const std::unique_ptr<CItem> demoRoot = CTreeMap::BuildDemoTree();
    CTreeMap treeMap;
    treeMap.DrawTreeMap(dc.Handle(), r, demoRoot.get(), &COptions::TreeMapOptions);
}

void CLayoutPopup::PaintCard(CDC& dc, const int idx) const
{
    const bool  selected = (idx == m_selectedLayout);
    const bool  hovered  = (idx == m_hoveredLayout);

    const CRect card = CardRect(idx);

    // Card background
    const COLORREF cardBg = DarkMode::IsDarkModeActive() ? RGB(38, 38, 38) : RGB(248, 248, 248);
    dc.FillSolidRect(card, cardBg);

    // Draw each pane in the card
    const LayoutDef& ld = LAYOUTS[idx];
    CRect inner = card;
    inner.Deflate(3, 3);

    const int pg = ScaleForDpi(4);  // gap inset applied to each pane edge

    for (auto [viewType, x, y, w, h] : ld.PreviewPanes())
    {
        CRect pr(
            inner.left + static_cast<int>(inner.Width()  * x) + pg,
            inner.top  + static_cast<int>(inner.Height() * y) + pg,
            inner.left + static_cast<int>(inner.Width()  * (x + w)) - pg,
            inner.top  + static_cast<int>(inner.Height() * (y + h)) - pg
        );
        if (pr.Width() < 2 || pr.Height() < 2) continue;

        switch (viewType)
        {
        case VT_ALLFILES:
        case VT_FILETYPES: DrawListPane(dc, pr, viewType); break;
        case VT_VISUALIZATION: DrawVisualizationPane(dc, pr); break;
        }
    }

    const int borderWidth = (selected || hovered) ? 2 : 1;
    const COLORREF borderColor = selected ? (DarkMode::IsDarkModeActive() ? RGB(100, 170, 255) : RGB(0, 102, 204))
                               : hovered  ? (DarkMode::IsDarkModeActive() ? RGB(80,  145, 220) : RGB(60, 140, 220))
                                          : (DarkMode::IsDarkModeActive() ? RGB(72,   72,  72) : RGB(195, 195, 195));

    const CPen pen(PS_SOLID, borderWidth, borderColor);
    GdiObjectSelection selectPen(&dc, &pen);
    StockObjectSelection selectBrush(&dc, NULL_BRUSH);
    const int adj = borderWidth - 1;
    dc.Rectangle(card.left + adj, card.top + adj,
                 card.right - adj, card.bottom - adj);

    if (selected)
    {
        const int r = ScaleForDpi(5), cx = card.right - r - ScaleForDpi(3), cy = card.top + r + ScaleForDpi(3);
        const CBrush brush(borderColor);
        const CPen selectionPen(PS_SOLID, 1, borderColor);
        GdiObjectSelection selectionBrush(&dc, &brush);
        GdiObjectSelection selectionPenScope(&dc, &selectionPen);
        dc.Ellipse(cx - r, cy - r, cx + r, cy + r);
    }
}

bool CLayoutPopup::OnEraseBkgnd(CDC* pDC) const
{
    const CRect rc = GetClientRect();
    const COLORREF bg = DarkMode::IsDarkModeActive() ? RGB(28, 28, 28) : RGB(240, 240, 240);
    pDC->FillSolidRect(rc, bg);
    return true;
}

void CLayoutPopup::OnPaint()
{
    CPaintDC dc(this);
    for (const int i : std::views::iota(0, LayoutCount))
        PaintCard(dc, i);
}

void CLayoutPopup::OnMouseMove(UINT /*nFlags*/, const CPoint point)
{
    const int hit = CardAtPoint(point);
    if (hit != m_hoveredLayout)
    {
        m_hoveredLayout = hit;
        Invalidate(false);
    }

    // Track mouse so WM_MOUSELEAVE fires when cursor leaves the window
    TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, Handle(), 0 };
    TrackMouseEvent(&tme);
}

LRESULT CLayoutPopup::OnMouseLeave(WPARAM, LPARAM)
{
    if (m_hoveredLayout != -1)
    {
        m_hoveredLayout = -1;
        Invalidate(false);
    }
    return 0;
}

void CLayoutPopup::OnLButtonDown(UINT /*nFlags*/, const CPoint point)
{
    if (CardAtPoint(point) < 0)
        DismissPopup(true);
}

void CLayoutPopup::OnLButtonUp(UINT /*nFlags*/, const CPoint point)
{
    const int hit = CardAtPoint(point);
    if (hit < 0)
    {
        DismissPopup(true);
        return;
    }

    m_selectedLayout = hit;
    COptions::WindowLayout = hit;
    DismissPopup(false, true);
}

void CLayoutPopup::OnKeyDown(const UINT nChar, UINT /*nRepCnt*/, UINT /*nFlags*/)
{
    if (nChar == VK_ESCAPE) DismissPopup(true);
}

void CLayoutPopup::OnKillFocus(WindowRef /*pNewWnd*/)
{
    if (IsWindowVisible()) DismissPopup(true);
}

void CLayoutPopup::OnActivateApp(const bool bActive, DWORD /*dwThreadID*/)
{
    if (!bActive && IsWindowVisible()) DismissPopup(true);
}

void CLayoutPopup::OnCaptureChanged(WindowRef pWnd)
{
    if (pWnd.Handle() != Handle() && IsWindowVisible())
        DismissPopup(true);
}
