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
#include "LayoutPopup.h"
#include "DarkMode.h"
#include "HelpersInterface.h"
#include "MainFrame.h"

// Pane viewType: 0=AllFiles  1=FileTypes  2=TreeMap  -1=absent; x,y,w,h are fractions of card area
const CLayoutPopup::LayoutDef CLayoutPopup::LAYOUTS[LAYOUT_COUNT] =
{
    // 00: [AF|FT] top half / TM bottom half
    { { {0, 0.00f, 0.00f, 0.58f, 0.50f},
        {1, 0.58f, 0.00f, 0.42f, 0.50f},
        {2, 0.00f, 0.50f, 1.00f, 0.50f} }, LT_ROWS_SUB_COLS, 0 },

    // 01: AF(40) | FT(20) | TM(40) columns
    { { {0, 0.00f, 0.00f, 0.40f, 1.00f},
        {1, 0.40f, 0.00f, 0.20f, 1.00f},
        {2, 0.60f, 0.00f, 0.40f, 1.00f} }, LT_COLS_THREE, 2 },

    // 02: TM(40) | AF(40) | FT(20) columns
    { { {2, 0.00f, 0.00f, 0.40f, 1.00f},
        {0, 0.40f, 0.00f, 0.40f, 1.00f},
        {1, 0.80f, 0.00f, 0.20f, 1.00f} }, LT_COLS_THREE, 1 },

    // 03: TM top half / [AF|FT] bottom half
    { { {2, 0.00f, 0.00f, 1.00f, 0.50f},
        {0, 0.00f, 0.50f, 0.58f, 0.50f},
        {1, 0.58f, 0.50f, 0.42f, 0.50f} }, LT_ROWS_SUB_COLS, 1 },

    // 04: AF(40) | TM(40) | FT(20) columns
    { { {0, 0.00f, 0.00f, 0.40f, 1.00f},
        {2, 0.40f, 0.00f, 0.40f, 1.00f},
        {1, 0.80f, 0.00f, 0.20f, 1.00f} }, LT_COLS_THREE, 0 },

    // 05: TM(40) | FT(20) | AF(40) columns
    { { {2, 0.00f, 0.00f, 0.40f, 1.00f},
        {1, 0.40f, 0.00f, 0.20f, 1.00f},
        {0, 0.60f, 0.00f, 0.40f, 1.00f} }, LT_COLS_THREE, 3 },

    // 06: left=[AF(top)/TM(bot)], right=FT
    { { {0, 0.00f, 0.00f, 0.60f, 0.50f},
        {2, 0.00f, 0.50f, 0.60f, 0.50f},
        {1, 0.60f, 0.00f, 0.40f, 1.00f} }, LT_COLS_SUB_ROWS, 1 },

    // 07: left=[AF(top)/FT(bot)], right=TM
    { { {0, 0.00f, 0.00f, 0.50f, 0.50f},
        {1, 0.00f, 0.50f, 0.50f, 0.50f},
        {2, 0.50f, 0.00f, 0.50f, 1.00f} }, LT_COLS_TM_FULL, 3 },

    // 08: left=TM, right=[AF(top)/FT(bot)]
    { { {2, 0.00f, 0.00f, 0.50f, 1.00f},
        {0, 0.50f, 0.00f, 0.50f, 0.50f},
        {1, 0.50f, 0.50f, 0.50f, 0.50f} }, LT_COLS_TM_FULL, 1 },

    // 09: left=[TM(top)/AF(bot)], right=FT
    { { {2, 0.00f, 0.00f, 0.60f, 0.50f},
        {0, 0.00f, 0.50f, 0.60f, 0.50f},
        {1, 0.60f, 0.00f, 0.40f, 1.00f} }, LT_COLS_SUB_ROWS, 0 },

    // 10: left=[FT(top)/AF(bot)], right=TM
    { { {1, 0.00f, 0.00f, 0.50f, 0.50f},
        {0, 0.00f, 0.50f, 0.50f, 0.50f},
        {2, 0.50f, 0.00f, 0.50f, 1.00f} }, LT_COLS_TM_FULL, 2 },

    // 11: left=TM, right=[FT(top)/AF(bot)]
    { { {2, 0.00f, 0.00f, 0.50f, 1.00f},
        {1, 0.50f, 0.00f, 0.50f, 0.50f},
        {0, 0.50f, 0.50f, 0.50f, 0.50f} }, LT_COLS_TM_FULL, 0 },
};

int CLayoutPopup::LayoutIndex(int topology, int permutation)
{
    for (int i = 0; i < LAYOUT_COUNT; ++i)
        if (LAYOUTS[i].topology == topology && LAYOUTS[i].permutation == permutation)
            return i;
    return 0;
}

int CLayoutPopup::CurrentLayoutIndex()
{
    return LayoutIndex(COptions::LayoutTopology, COptions::LayoutPermutation);
}

BEGIN_MESSAGE_MAP(CLayoutPopup, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_KEYDOWN()
    ON_WM_KILLFOCUS()
    ON_WM_ACTIVATEAPP()
    ON_WM_CAPTURECHANGED()
    ON_MESSAGE(WM_MOUSELEAVE, &CLayoutPopup::OnMouseLeave)
END_MESSAGE_MAP()

BOOL CLayoutPopup::Create(CWnd* parent)
{
    static ATOM s_atom = 0;
    if (s_atom == 0)
    {
        const WNDCLASSEX wc = {
            .cbSize        = sizeof(WNDCLASSEX),
            .style         = CS_DROPSHADOW,
            .lpfnWndProc   = ::DefWindowProc,
            .hInstance     = AfxGetInstanceHandle(),
            .hCursor       = LoadCursor(nullptr, IDC_ARROW),
            .hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1),
            .lpszClassName = L"WdsLayoutPopup",
        };
        s_atom = ::RegisterClassEx(&wc);
        if (s_atom == 0) return FALSE;
    }

    return CWnd::CreateEx(
        WS_EX_TOOLWINDOW,
        L"WdsLayoutPopup",
        nullptr,
        WS_POPUP | WS_BORDER,
        0, 0, 1, 1,
        parent->GetSafeHwnd(),
        nullptr);
}

void CLayoutPopup::ShowAtButton(const CRect& buttonScreenRect)
{
    m_selectedLayout = CurrentLayoutIndex();
    m_hoveredLayout  = -1;

    const int cw = DpiRest(CARD_W_BASE, this);
    const int ch = DpiRest(CARD_H_BASE, this);
    const int gp = DpiRest(GAP_BASE, this);
    const int mg = DpiRest(MARGIN_BASE, this);

    const int popupW = COLS * cw + (COLS - 1) * gp + 2 * mg;
    const int popupH = ROWS * ch + (ROWS - 1) * gp + 2 * mg;

    // Default: anchor below the button, left-aligned
    CPoint origin(buttonScreenRect.left, buttonScreenRect.bottom + DpiRest(2, this));

    // Fit to work area
    MONITORINFO mi{ sizeof(mi) };
    HMONITOR hMon = MonitorFromPoint(origin, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfo(hMon, &mi);
    const CRect work = mi.rcWork;

    if (origin.x + popupW > work.right)
        origin.x = work.right - popupW;
    if (origin.x < work.left)
        origin.x = work.left;
    if (origin.y + popupH > work.bottom)
        origin.y = buttonScreenRect.top - popupH - DpiRest(2, this);
    if (origin.y < work.top)
        origin.y = work.top;

    SetWindowPos(nullptr, origin.x, origin.y, popupW, popupH,
        SWP_NOZORDER | SWP_SHOWWINDOW);
    SetFocus();
    SetCapture();
    Invalidate();
}

void CLayoutPopup::DismissPopup(bool cancel, bool resetPositions)
{
    ShowWindow(SW_HIDE);

    if (::GetCapture() == GetSafeHwnd())
        ReleaseCapture();

    if (::GetFocus() == GetSafeHwnd())
        if (CWnd* parent = GetParent())
            parent->SetFocus();

    if (!cancel)
        CMainFrame::Get()->RebuildLayout(resetPositions);
}

CRect CLayoutPopup::CardRect(int idx) const
{
    const int cw = DpiRest(CARD_W_BASE, this);
    const int ch = DpiRest(CARD_H_BASE, this);
    const int gp = DpiRest(GAP_BASE, this);
    const int mg = DpiRest(MARGIN_BASE, this);

    const int col = idx % COLS;
    const int row = idx / COLS;
    const int x   = mg + col * (cw + gp);
    const int y   = mg + row * (ch + gp);

    return CRect(x, y, x + cw, y + ch);
}

int CLayoutPopup::CardAtPoint(CPoint pt) const
{
    for (int i = 0; i < LAYOUT_COUNT; ++i)
        if (CardRect(i).PtInRect(pt))
            return i;
    return -1;
}

static void DrawPaneHeader(CDC& dc, const CRect& r, int rowH, COLORREF hdr, COLORREF sep)
{
    dc.FillSolidRect(r.left, r.top,            r.Width(), rowH + 1, hdr);
    dc.FillSolidRect(r.left, r.top + 1 + rowH, r.Width(), 1,    sep);
}

static COLORREF GetPaneHeaderBackground()
{
    return DarkMode::WdsSysColor(COLOR_BTNFACE);
}

static COLORREF GetPaneHeaderSeparator()
{
    return DarkMode::WdsSysColor(DarkMode::IsDarkModeActive() ? COLOR_3DHIGHLIGHT : COLOR_3DSHADOW);
}

static COLORREF GetPaneHeaderTextColor()
{
    return DarkMode::IsDarkModeActive() ? RGB(176, 176, 176) : RGB(88, 88, 88);
}

static void DrawHeaderDivider(CDC& dc, const CRect& r, const int rowH, const int x, const COLORREF color)
{
    if (x <= r.left || x >= r.right) return;
    dc.FillSolidRect(x, r.top + 1, 1, rowH, color);
}

static void DrawHeaderText(CDC& dc, CRect r, const int seed = 0)
{
    r.DeflateRect(2, 0);
    if (r.Width() <= 2 || r.Height() <= 0) return;

    constexpr int strokeH = 1;
    const int y = r.top + (r.Height() - strokeH) / 2;
    const int width = std::max(2, r.Width() * (55 + (seed % 3) * 10) / 100);
    dc.FillSolidRect(r.left, y, std::min(width, r.Width()), strokeH, GetPaneHeaderTextColor());
}

static void DrawMiniText(CDC& dc, CRect r, int seed = 0)
{
    if (r.Width() <= 2 || r.Height() <= 0) return;

    const COLORREF color = DarkMode::IsDarkModeActive() ? RGB(204, 204, 204) : RGB(52, 52, 52);
    constexpr int strokeH = 1;
    const int y = r.top + (r.Height() - strokeH) / 2;
    const int first = std::max(2, r.Width() - (seed % 4));
    dc.FillSolidRect(r.left, y, first, strokeH, color);
}

static COLORREF GetFileTreeColor(const int index)
{
    const int count = std::clamp(static_cast<int>(COptions::FileTreeColorCount), 1, TREELISTCOLORCOUNT);
    return COptions::FileTreeColors[index % count];
}

void CLayoutPopup::DrawAllFilesPane(CDC& dc, CRect r) const
{
    const bool dk   = DarkMode::IsDarkModeActive();
    const COLORREF bg    = dk ? RGB(26, 24, 22)    : RGB(255, 254, 249);
    const COLORREF alt   = dk ? RGB(34, 32, 28)    : RGB(247, 245, 240);
    const COLORREF hdrBg = GetPaneHeaderBackground();
    const COLORREF sep   = GetPaneHeaderSeparator();
    const COLORREF folio = RGB(228, 178, 36);

    const int rowH  = DpiRest(7, this);
    const int iSz   = std::max(3, rowH - 2);
    const int indU  = std::max(2, r.Width() / 9);
    const int pColX = r.left + r.Width() * 54 / 100;

    DrawPaneHeader(dc, r, rowH, hdrBg, sep);
    DrawHeaderText(dc, CRect(r.left, r.top, pColX, r.top + rowH), 0);
    DrawHeaderDivider(dc, r, rowH, pColX, sep);
    DrawHeaderText(dc, CRect(pColX, r.top, r.right, r.top + rowH), 1);

    struct Row { int ind; float pf; };
    static constexpr Row rows[] = {
        {0, 0.95f}, {1, 0.37f}, {1, 0.29f}, {1, 0.16f},
        {2, 0.10f}, {2, 0.07f}, {1, 0.04f}, {1, 0.02f},
    };

    for (int i = 0; i < 8; ++i)
    {
        const int y0 = r.top + 2 + rowH + i * rowH;
        if (y0 >= r.bottom) break;
        const int y1 = std::min(y0 + rowH, static_cast<int>(r.bottom));

        CRect rowR(r.left, y0, r.right, y1);
        dc.FillSolidRect(&rowR, i % 2 ? alt : bg);

        const int ix = r.left + 2 + rows[i].ind * indU;
        const int iy = y0 + (rowH - iSz + 1) / 2;

        if (ix + iSz < pColX - 2)
        {
            CRect ico(ix, iy, ix + iSz, iy + iSz);
            dc.FillSolidRect(&ico, folio);
        }

        const int tx  = ix + iSz + 2;
        const int txW = (pColX - tx - 2) * 2 / 3;
        if (tx < pColX - 4 && txW > 2)
        {
            CRect txt(tx, iy, std::min(tx + txW, pColX - 2), iy + std::max(2, iSz - 1));
            DrawMiniText(dc, txt, i);
        }

        const int pw = static_cast<int>((r.right - pColX - 2) * rows[i].pf);
        if (pw > 0)
        {
            CRect bar(pColX, iy, pColX + pw, iy + std::max(2, iSz - 1));
            dc.FillSolidRect(&bar, GetFileTreeColor(i));
        }
    }
}

void CLayoutPopup::DrawFileTypesPane(CDC& dc, CRect r) const
{
    const bool dk   = DarkMode::IsDarkModeActive();
    const COLORREF bg    = dk ? RGB(22, 24, 26)    : RGB(249, 252, 255);
    const COLORREF alt   = dk ? RGB(28, 30, 36)    : RGB(241, 245, 252);
    const COLORREF hdrBg = GetPaneHeaderBackground();
    const COLORREF sep   = GetPaneHeaderSeparator();

    static constexpr COLORREF iconClr[] = {
        RGB( 65, 115, 220),
        RGB(185, 130,  40),
        RGB( 45, 165,  80),
        RGB( 55, 125, 200),
        RGB(140, 140, 170),
        RGB(190,  65,  65),
        RGB( 45, 165, 165),
        RGB(155,  65, 195),
    };

    const int rowH    = DpiRest(7, this);
    const int iSz     = std::max(3, rowH - 2);
    const int extColW = r.Width() * 28 / 100;
    const int numColX = r.left + r.Width() * 68 / 100;

    DrawPaneHeader(dc, r, rowH, hdrBg, sep);
    DrawHeaderText(dc, CRect(r.left, r.top, r.left + extColW, r.top + rowH), 0);
    DrawHeaderDivider(dc, r, rowH, r.left + extColW, sep);
    DrawHeaderText(dc, CRect(r.left + extColW, r.top, numColX, r.top + rowH), 1);
    DrawHeaderDivider(dc, r, rowH, numColX, sep);
    DrawHeaderText(dc, CRect(numColX, r.top, r.right, r.top + rowH), 2);

    for (int i = 0; i < 8; ++i)
    {
        const int y0 = r.top + 2 + rowH + i * rowH;
        if (y0 >= r.bottom) break;
        const int y1 = std::min(y0 + rowH, static_cast<int>(r.bottom));

        CRect rowR(r.left, y0, r.right, y1);
        dc.FillSolidRect(&rowR, i % 2 ? alt : bg);

        const int iy = y0 + (rowH - iSz + 1) / 2;

        CRect ico(r.left + 2, iy, r.left + 2 + iSz, iy + iSz);
        if (ico.right < numColX - 2)
            dc.FillSolidRect(&ico, iconClr[i % 8]);

        const int extX = ico.right + 2;
        const int extW = r.left + extColW - extX;
        if (extX < numColX - 4 && extW > 2)
        {
            CRect extR(extX, iy, extX + extW, iy + std::max(2, iSz - 1));
            DrawMiniText(dc, extR, i);
        }

        const int descX = r.left + extColW + 2;
        const int descW = (numColX - descX - 2) * 3 / 4;
        if (descX < numColX - 4 && descW > 2)
        {
            CRect descR(descX, iy, descX + descW, iy + std::max(2, iSz - 1));
            DrawMiniText(dc, descR, i + 1);
        }

        const int numW = (r.right - numColX - 2) * 2 / 3;
        if (numW > 2)
        {
            CRect numR(numColX + 2, iy, numColX + 2 + numW, iy + std::max(2, iSz - 1));
            DrawMiniText(dc, numR, i + 2);
        }
    }
}

void CLayoutPopup::DrawTreeMapPane(CDC& dc, CRect r, int /*cardIdx*/) const
{
    static const std::unique_ptr<CItem> demoRoot = CTreeMap::BuildDemoTree();
    CTreeMap treeMap;
    treeMap.DrawTreeMap(&dc, r, demoRoot.get(), &COptions::TreeMapOptions);
}

void CLayoutPopup::PaintCard(CDC& dc, int idx) const
{
    const bool  dark     = DarkMode::IsDarkModeActive();
    const bool  selected = (idx == m_selectedLayout);
    const bool  hovered  = (idx == m_hoveredLayout);

    const CRect card = CardRect(idx);

    // Card background
    const COLORREF cardBg = dark ? RGB(38, 38, 38) : RGB(248, 248, 248);
    dc.FillSolidRect(&card, cardBg);

    // Draw each pane in the card
    const LayoutDef& ld = LAYOUTS[idx];
    CRect inner = card;
    inner.DeflateRect(3, 3);

    const int pg = DpiRest(4, this);  // gap inset applied to each pane edge

    for (int p = 0; p < 3; ++p)
    {
        const Pane& pn = ld.panes[p];
        if (pn.viewType < 0) continue;

        CRect pr(
            inner.left + static_cast<int>(inner.Width()  * pn.x) + pg,
            inner.top  + static_cast<int>(inner.Height() * pn.y) + pg,
            inner.left + static_cast<int>(inner.Width()  * (pn.x + pn.w)) - pg,
            inner.top  + static_cast<int>(inner.Height() * (pn.y + pn.h)) - pg
        );
        if (pr.Width() < 2 || pr.Height() < 2) continue;

        switch (pn.viewType)
        {
        case 0: DrawAllFilesPane (dc, pr);       break;
        case 1: DrawFileTypesPane(dc, pr);       break;
        case 2: DrawTreeMapPane  (dc, pr, idx);  break;
        }
    }

    const int borderWidth = (selected || hovered) ? 2 : 1;
    const COLORREF borderColor = selected ? (dark ? RGB(100, 170, 255) : RGB(0, 102, 204))
                               : hovered  ? (dark ? RGB(80,  145, 220) : RGB(60, 140, 220))
                                          : (dark ? RGB(72,   72,  72) : RGB(195, 195, 195));

    CPen pen(PS_SOLID, borderWidth, borderColor);
    CPen* oldPen = dc.SelectObject(&pen);
    CBrush* oldBrush = static_cast<CBrush*>(dc.SelectStockObject(NULL_BRUSH));
    const int adj = borderWidth - 1;
    dc.Rectangle(card.left + adj, card.top + adj,
                 card.right - adj, card.bottom - adj);
    dc.SelectObject(oldPen);
    dc.SelectObject(oldBrush);

    if (selected)
    {
        const int r = DpiRest(5, this), cx = card.right - r - DpiRest(3, this), cy = card.top + r + DpiRest(3, this);
        const int saved = dc.SaveDC();
        CBrush b(borderColor); dc.SelectObject(&b);
        CPen p(PS_SOLID, 1, borderColor); dc.SelectObject(&p);
        dc.Ellipse(cx - r, cy - r, cx + r, cy + r);
        dc.RestoreDC(saved);
    }
}

BOOL CLayoutPopup::OnEraseBkgnd(CDC* pDC)
{
    CRect rc;
    GetClientRect(&rc);
    const COLORREF bg = DarkMode::IsDarkModeActive() ? RGB(28, 28, 28) : RGB(240, 240, 240);
    pDC->FillSolidRect(&rc, bg);
    return TRUE;
}

void CLayoutPopup::OnPaint()
{
    CPaintDC dc(this);
    for (int i = 0; i < LAYOUT_COUNT; ++i)
        PaintCard(dc, i);
}

void CLayoutPopup::OnMouseMove(UINT /*nFlags*/, CPoint point)
{
    const int hit = CardAtPoint(point);
    if (hit != m_hoveredLayout)
    {
        m_hoveredLayout = hit;
        Invalidate(FALSE);
    }

    // Track mouse so WM_MOUSELEAVE fires when cursor leaves the window
    TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, GetSafeHwnd(), 0 };
    TrackMouseEvent(&tme);
}

LRESULT CLayoutPopup::OnMouseLeave(WPARAM, LPARAM)
{
    if (m_hoveredLayout != -1)
    {
        m_hoveredLayout = -1;
        Invalidate(FALSE);
    }
    return 0;
}

void CLayoutPopup::OnLButtonDown(UINT /*nFlags*/, CPoint point)
{
    if (CardAtPoint(point) < 0)
        DismissPopup(true);
}

void CLayoutPopup::OnLButtonUp(UINT /*nFlags*/, CPoint point)
{
    const int hit = CardAtPoint(point);
    if (hit < 0)
    {
        DismissPopup(true);
        return;
    }

    m_selectedLayout = hit;
    COptions::LayoutTopology    = LAYOUTS[hit].topology;
    COptions::LayoutPermutation = LAYOUTS[hit].permutation;
    DismissPopup(false, true);
}

void CLayoutPopup::OnKeyDown(UINT nChar, UINT /*nRepCnt*/, UINT /*nFlags*/)
{
    if (nChar == VK_ESCAPE) DismissPopup(true);
}

void CLayoutPopup::OnKillFocus(CWnd* /*pNewWnd*/)
{
    if (IsWindowVisible()) DismissPopup(true);
}

void CLayoutPopup::OnActivateApp(BOOL bActive, DWORD /*dwThreadID*/)
{
    if (!bActive && IsWindowVisible()) DismissPopup(true);
}

void CLayoutPopup::OnCaptureChanged(CWnd* pWnd)
{
    if (pWnd != this && IsWindowVisible())
        DismissPopup(true);
}
