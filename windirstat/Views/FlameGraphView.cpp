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
#include "FileTreeView.h"
#include "FlameGraphView.h"

IMPLEMENT_DYNCREATE(CFlameGraphView, CWinDirStatPane)

BEGIN_MESSAGE_MAP(CFlameGraphView, CWinDirStatPane)
    ON_WM_SIZE()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_LBUTTONDOWN()
    ON_WM_MBUTTONDOWN()
    ON_WM_SETFOCUS()
    ON_WM_CONTEXTMENU()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_MOUSEWHEEL()
    ON_WM_VSCROLL()
END_MESSAGE_MAP()

void CFlameGraphView::SuspendRecalculationDrawing(const bool suspend)
{
    if (suspend == m_drawingSuspended) return;

    if (suspend)
    {
        // Refresh scans can remove items immediately after this call. Drop all
        // cached item pointers and geometry before the model starts mutating.
        m_wheelDeltaRemainder = 0;
        m_forceScrollBarVisible = false;
        Inactivate();
    }
    m_drawingSuspended = suspend;
    if (!suspend)
    {
        Invalidate();
    }
}

bool CFlameGraphView::IsShowTreeMap() const
{
    return m_showTreeMap;
}

void CFlameGraphView::ShowTreeMap(const bool show)
{
    m_showTreeMap = show;
}

BOOL CFlameGraphView::PreCreateWindow(CREATESTRUCT& cs)
{
    CWinDirStatPane::PreCreateWindow(cs);

    WNDCLASS wc;
    if (!::GetClassInfo(AfxGetInstanceHandle(), L"WinDirStatFlameGraphClass", &wc))
    {
        ::GetClassInfo(AfxGetInstanceHandle(), cs.lpszClass, &wc);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"WinDirStatFlameGraphClass";
        ::RegisterClass(&wc);
    }

    cs.lpszClass = wc.lpszClassName;
    return TRUE;
}

void CFlameGraphView::DrawEmptyView()
{
    CClientDC dc(this);
    DrawEmptyView(&dc);
}

void CFlameGraphView::DrawEmptyView(CDC* pDC)
{
    constexpr COLORREF emptyBg = RGB(15, 15, 15);

    Inactivate();

    const CRect rc = ClientRectOf(this);
    if (m_dimmed.m_hObject == nullptr)
    {
        pDC->FillSolidRect(rc, emptyBg);

        const int cols = 8;
        const int rows = 5;
        const int cellW = rc.Width() / cols;
        const int cellH = rc.Height() / rows;

        for (int r = 0; r < rows; r++)
        {
            const int y = rc.top + r * cellH;
            const int height = (r == rows - 1) ? rc.bottom - y : cellH;

            CRect labelRc(rc.left, y, rc.left + 30, y + height);
            if (labelRc.Height() >= 10)
            {
                pDC->FillSolidRect(labelRc, RGB(30, 30, 30));
            }

            int x = rc.left + 30;
            const int colsThisRow = cols - r;
            for (int c = 0; c < colsThisRow; c++)
            {
                const int shade = 40 + r * 30 + c * 10;
                const int w = (c == colsThisRow - 1) ? rc.right - x : cellW;

                CRect tile(x, y, x + w, y + height);
                if (tile.Width() > 0 && tile.Height() > 0)
                {
                    pDC->FillSolidRect(tile, RGB(shade, shade, shade));
                }
                x += w;
            }
        }
    }
    else
    {
        CDC dcmem;
        dcmem.CreateCompatibleDC(pDC);
        CSelectObject sobmp(&dcmem, &m_dimmed);
        pDC->BitBlt(rc.left, rc.top, m_dimmedSize.cx, m_dimmedSize.cy, &dcmem, 0, 0, SRCCOPY);

        if (rc.Width() > m_dimmedSize.cx)
        {
            CRect r = rc;
            r.left = r.left + m_dimmedSize.cx;
            pDC->FillSolidRect(r, emptyBg);
        }

        if (rc.Height() > m_dimmedSize.cy)
        {
            CRect r = rc;
            r.top = r.top + m_dimmedSize.cy;
            pDC->FillSolidRect(r, emptyBg);
        }
    }
}

void CFlameGraphView::OnDraw(CDC* pDC)
{
    const CItem* root = CWinDirStatModel::Get()->GetRootItem();
    if (root == nullptr || !root->IsDone() || m_drawingSuspended || !m_showTreeMap)
    {
        DrawEmptyView(pDC);
        return;
    }

    CRect rc = ClientRectOf(this);
    ASSERT(m_size == rc.Size());
    ASSERT(rc.TopLeft() == CPoint(0, 0));
    if (rc.IsRectEmpty()) return;

    const CItem* zoomItem = CWinDirStatModel::Get()->GetZoomItem();
    const int rowHeight = ComputeRowHeight(pDC);
    if (rowHeight != m_rowHeight)
    {
        ClearHover();
        if (m_scrollPos > 0)
        {
            m_scrollPos = ::MulDiv(m_scrollPos, rowHeight, m_rowHeight);
        }
        m_rowHeight = rowHeight;
        DiscardBase(true);
    }

    int fullHeight = m_fullHeight;
    if (fullHeight == 0)
    {
        fullHeight = ComputeFlameFullHeight(rc.Width());
    }

    // A standard WS_VSCROLL bar changes the client width when SetScrollInfo
    // shows or hides it, and Windows delivers the resulting WM_SIZE
    // synchronously. Let that geometry settle before allocating the cached
    // bitmap or computing horizontal layout. Keep fullHeight local because
    // OnSize invalidates the member cache during that synchronous callback.
    bool scrollInfoCurrent = false;
    for (int pass = 0; pass < 2; pass++)
    {
        const int maxScroll = std::max(0, fullHeight - rc.Height());
        m_scrollPos = std::clamp(m_scrollPos, 0, maxScroll);
        UpdateScrollBar(fullHeight, rc.Height());
        scrollInfoCurrent = true;

        const CRect updatedRc = ClientRectOf(this);
        if (updatedRc == rc) break;
        const bool scrollBarAppeared = updatedRc.Width() < rc.Width();
        rc = updatedRc;
        scrollInfoCurrent = false;
        if (rc.IsRectEmpty())
        {
            m_fullHeight = fullHeight;
            return;
        }

        // Visible depth depends on pixel width, so a scrollbar transition must
        // recompute height for the width that will actually be laid out.
        fullHeight = ComputeFlameFullHeight(rc.Width());

        // A narrow layout can lose the deep one-pixel branch that made the
        // scrollbar necessary at the wider width. Keep a disabled scrollbar
        // in that boundary case; otherwise hide/show would oscillate forever.
        if (scrollBarAppeared && fullHeight <= rc.Height())
        {
            m_forceScrollBarVisible = true;
            break;
        }
    }

    if (!scrollInfoCurrent)
    {
        const int maxScroll = std::max(0, fullHeight - rc.Height());
        m_scrollPos = std::clamp(m_scrollPos, 0, maxScroll);
        UpdateScrollBar(fullHeight, rc.Height());
        ASSERT(ClientRectOf(this) == rc);
    }

    const CSize size = rc.Size();
    if (m_size != size)
    {
        // OnSize normally keeps this synchronized. This fallback also ensures
        // a cache can never survive an unexpected non-client geometry change.
        ClearHover();
        DiscardBase(false);
        m_size = size;
    }
    m_fullHeight = fullHeight;

    CDC dcmem;
    if (!dcmem.CreateCompatibleDC(pDC))
    {
        ClearHover();
        DiscardBase(false);
        pDC->FillSolidRect(rc, CFlameGraph::BACKGROUND_COLOR);
        return;
    }

    if (!IsDrawn())
    {
        CWaitCursor wc;
        if (!m_bitmap.CreateCompatibleBitmap(pDC, size.cx, size.cy))
        {
            ClearHover();
            DiscardBase(false);
            pDC->FillSolidRect(rc, CFlameGraph::BACKGROUND_COLOR);
            return;
        }

        CSelectObject sobmp(&dcmem, &m_bitmap);
        dcmem.FillSolidRect(rc, CFlameGraph::BACKGROUND_COLOR);

        // Layout coordinates cover the full graph; the cached bitmap contains
        // only the currently visible client-sized viewport.
        const CPoint oldViewportOrg = dcmem.SetViewportOrg(0, -m_scrollPos);
        m_flameGraph.DrawFlameGraph(&dcmem, CRect(0, 0, size.cx, m_fullHeight),
            zoomItem, m_rowHeight);
        dcmem.SetViewportOrg(oldViewportOrg);

        m_dimmed.DeleteObject();
        m_dimmedSize = { 0, 0 };
    }

    {
        CSelectObject sobmp(&dcmem, &m_bitmap);
        pDC->BitBlt(0, 0, size.cx, size.cy, &dcmem, 0, 0, SRCCOPY);
    }

    if (m_hoverItem != nullptr)
    {
        m_flameGraph.DrawHoverItem(pDC, m_hoverItem, CPoint(0, -m_scrollPos));
    }

    // Persistent selection overlays take precedence over transient hover.
    DrawHighlights(pDC);
}

void CFlameGraphView::DrawHighlights(CDC* pdc)
{
    switch (CMainFrame::Get()->GetLogicalFocus())
    {
    case LF_DUPELIST:
    case LF_TOPLIST:
    case LF_FILETREE:
    case LF_SEARCHLIST:
        DrawSelection(pdc);
        break;
    case LF_EXTLIST:
        DrawHighlightExtension(pdc);
        break;
    case LF_NONE:
        break;
    }
}

static bool IsUnregisteredLeaf(const CItem* item, const std::unordered_set<std::wstring>& set)
{
    return item->IsTypeOrFlag(IT_FILE) && set.contains(item->GetExtension());
}

void CFlameGraphView::DrawHighlightExtension(CDC* pdc)
{
    CWaitCursor wc;

    CPen pen(PS_SOLID, 1, COptions::TreeMapHighlightColor);
    CSelectObject sopen(pdc, &pen);
    CSelectStockObject sobrush(pdc, NULL_BRUSH);

    const CWinDirStatModel* model = CWinDirStatModel::Get();
    const std::wstring& highlightExt = model->GetHighlightExtension();
    const bool unregistered = model->IsHighlightUnregistered();
    const auto& highlightExtensions = model->GetHighlightExtensions();
    CRect rcClip;
    if (pdc->GetClipBox(&rcClip) == ERROR)
    {
        rcClip = ClientRectOf(this);
    }

    m_flameGraph.VisitItemsIntersecting(rcClip, CPoint(0, -m_scrollPos),
        [&](const CItem* item, const CRect& itemRectangle)
    {
        if (item->TmiIsLeaf()
            && (unregistered
                ? IsUnregisteredLeaf(item, highlightExtensions)
                : item->HasExtension(highlightExt)))
        {
            CRect rc = itemRectangle;
            RenderHighlightRectangle(pdc, rc);
        }
    });
}

void CFlameGraphView::DrawSelection(CDC* pdc) const
{
    CSelectStockObject sobrush(pdc, NULL_BRUSH);

    CPen pen(PS_SOLID, 1, COptions::TreeMapHighlightColor);
    CSelectObject sopen(pdc, &pen);

    const auto& items = CWinDirStatModel::Get()->GetAllSelected();
    for (const auto& item : items)
    {
        const auto itemToSelect = item->IsTypeOrFlag(ITF_HARDLINK) ? item->FindHardlinksIndexItem() : item;
        HighlightSelectedItem(pdc, itemToSelect, items.size() == 1);
    }
}

void CFlameGraphView::HighlightSelectedItem(CDC* pdc, const CItem* item, const bool single) const
{
    CRect rc;
    if (!m_flameGraph.TryGetItemRectangle(item, rc))
    {
        return;
    }
    rc.OffsetRect(0, -m_scrollPos);

    if (single)
    {
        CRect rcClient = ClientRectOf(this);

        if (rcClient.left < rc.left) rc.left--;
        if (rcClient.top < rc.top) rc.top--;
        if (rc.right < rcClient.right) rc.right++;
        if (rc.bottom < rcClient.bottom) rc.bottom++;
    }

    CRect visible;
    if (rc.Width() <= 0 || rc.Height() <= 0 || !visible.IntersectRect(rc, ClientRectOf(this)))
    {
        return;
    }

    RenderHighlightRectangle(pdc, rc);
}

void CFlameGraphView::RenderHighlightRectangle(CDC* pdc, CRect& rc) const
{
    ASSERT(rc.Width() >= 0);
    ASSERT(rc.Height() >= 0);

    if (rc.Width() >= 7 && rc.Height() >= 7)
    {
        pdc->Rectangle(rc);
        rc.DeflateRect(1, 1);
        pdc->Rectangle(rc);
        rc.DeflateRect(1, 1);
        pdc->Rectangle(rc);
    }
    else
    {
        pdc->FillSolidRect(rc, COptions::TreeMapHighlightColor);
    }
}

CItem* CFlameGraphView::ResolveItemAtPoint(CPoint point, bool isScreenCoords)
{
    const CItem* root = CWinDirStatModel::Get()->GetRootItem();
    if (root == nullptr || !root->IsDone())
    {
        return nullptr;
    }

    CPoint pointClicked = point;
    if (isScreenCoords) ScreenToClient(&pointClicked);

    if (!ClientRectOf(this).PtInRect(pointClicked))
    {
        return nullptr;
    }

    pointClicked.y += m_scrollPos;

    return m_flameGraph.FindItemByPoint(CWinDirStatModel::Get()->GetZoomItem(), pointClicked);
}

void CFlameGraphView::ClearHover()
{
    SetHoverItem(nullptr);

    if (!m_paneTextOverride.empty() || m_paneSizeOverride != 0)
    {
        m_paneTextOverride.clear();
        m_paneSizeOverride = 0;
        if (CMainFrame::Get() != nullptr)
        {
            CMainFrame::Get()->UpdatePaneText();
        }
    }
}

void CFlameGraphView::SetHoverItem(const CItem* item)
{
    if (item == m_hoverItem) return;

    InvalidateItem(m_hoverItem);
    m_hoverItem = item;
    InvalidateItem(m_hoverItem);
}

void CFlameGraphView::InvalidateItem(const CItem* item)
{
    if (item == nullptr) return;

    CRect rc;
    if (!m_flameGraph.TryGetItemRectangle(item, rc)) return;

    rc.OffsetRect(0, -m_scrollPos);
    rc.InflateRect(1, 1);
    CRect visible;
    if (visible.IntersectRect(rc, ClientRectOf(this)))
    {
        InvalidateRect(visible, FALSE);
    }
}

void CFlameGraphView::DiscardBase(const bool invalidateFullHeight)
{
    m_bitmap.DeleteObject();
    m_flameGraph.ClearLayout();
    if (invalidateFullHeight)
    {
        m_fullHeight = 0;
        m_forceScrollBarVisible = false;
    }
}

void CFlameGraphView::UpdateScrollBar(const int fullHeight, const int pageHeight)
{
    SCROLLINFO si{
        .cbSize = sizeof(SCROLLINFO),
        .fMask = SIF_RANGE | SIF_PAGE | SIF_POS,
    };
    si.nMin = 0;
    si.nMax = std::max(0, fullHeight - 1);
    si.nPage = static_cast<UINT>(std::max(0, pageHeight));
    si.nPos = m_scrollPos;
    if (m_forceScrollBarVisible) si.fMask |= SIF_DISABLENOSCROLL;

    m_updatingScrollBar = true;
    SetScrollInfo(SB_VERT, &si, TRUE);
    if (!m_forceScrollBarVisible && fullHeight <= pageHeight)
    {
        // SIF_DISABLENOSCROLL is sticky for a standard window scrollbar:
        // a later ordinary SetScrollInfo does not remove the disabled bar.
        // Explicitly hide it when a resize/model change leaves forcing mode.
        ShowScrollBar(SB_VERT, FALSE);
    }
    m_updatingScrollBar = false;
}

bool CFlameGraphView::EnsureFullHeightForInput()
{
    const auto* model = CWinDirStatModel::Get();
    if (m_drawingSuspended || !m_showTreeMap || model == nullptr || !model->IsRootDone())
    {
        return false;
    }
    if (m_fullHeight > 0) return true;

    // Paint messages have lower priority than queued input. Recompute here so
    // the first wheel or scrollbar action after a resize/model update is not
    // discarded merely because OnDraw has not rebuilt the cached height yet.
    m_fullHeight = ComputeFlameFullHeight(m_size.cx);
    return m_fullHeight > 0;
}

int CFlameGraphView::ComputeRowHeight(CDC* pDC) const
{
    int rowHeight = DpiRest(CFlameGraph::ROW_HEIGHT, this);

    CSelectStockObject soFont(pDC, DEFAULT_GUI_FONT);
    TEXTMETRIC tm{};
    if (pDC->GetTextMetrics(&tm))
    {
        // Breadcrumbs have one scaled fill inset and one text inset on each edge.
        const int verticalPadding = DpiRest(1, this) * 4;
        rowHeight = std::max(rowHeight, static_cast<int>(tm.tmHeight) + verticalPadding);
    }

    return std::max(1, rowHeight);
}

int CFlameGraphView::ComputeFlameFullHeight(const int width) const
{
    const CItem* zoomItem = CWinDirStatModel::Get()->GetZoomItem();
    const CItem* rootItem = CWinDirStatModel::Get()->GetRootItem();

    int breadcrumbRows = 0;
    if (zoomItem != rootItem)
    {
        for (const CItem* p = zoomItem->GetParent(); p != nullptr; p = p->GetParent())
        {
            breadcrumbRows++;
            if (p == rootItem) break;
        }
    }

    const int maxDepth = CFlameGraph::ComputeVisibleMaxDepth(
        zoomItem, std::max(0, width), 0);
    return (breadcrumbRows + maxDepth + 1) * m_rowHeight;
}

void CFlameGraphView::OnSize(const UINT nType, const int cx, const int cy)
{
    CWinDirStatPane::OnSize(nType, cx, cy);
    const CSize sz(cx, cy);
    if (sz != m_size)
    {
        if (!m_updatingScrollBar) m_forceScrollBarVisible = false;
        Inactivate();
        m_size = sz;
        if (!m_drawingSuspended) Invalidate();
    }
}

void CFlameGraphView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    if (auto* item = ResolveItemAtPoint(point)) DrillDown(item);
    CWinDirStatPane::OnLButtonDblClk(nFlags, point);
}

void CFlameGraphView::OnLButtonDown(const UINT nFlags, const CPoint point)
{
    if (auto* item = ResolveItemAtPoint(point))
    {
        CWinDirStatModel::Get()->ClearReselectChildStack();
        NotifyOtherPanes(MODEL_CHANGE_SELECTION_ACTION, item);
    }
    CWinDirStatPane::OnLButtonDown(nFlags, point);
}

void CFlameGraphView::DrillDown(CItem* item)
{
    ClearHover();

    auto* model = CWinDirStatModel::Get();

    CItem* target;
    if (item == model->GetZoomItem())
    {
        target = model->GetRootItem();
    }
    else
    {
        target = item->IsTypeOrFlag(IT_FILE) ? item->GetParent() : item;
    }

    if (target != model->GetZoomItem())
    {
        m_scrollPos = 0;
        model->SetZoomItem(target);
    }
}

void CFlameGraphView::OnMButtonDown(UINT nFlags, CPoint point)
{
    if (CWinDirStatModel::Get()->IsZoomed())
    {
        m_scrollPos = 0;
        CWinDirStatModel::Get()->SetZoomItem(CWinDirStatModel::Get()->GetRootItem());
    }
    else if (ResolveItemAtPoint(point))
    {
        AfxGetMainWnd()->SendMessage(WM_COMMAND, ID_TREEMAP_ZOOMRESET);
    }

    CWinDirStatPane::OnMButtonDown(nFlags, point);
}

bool CFlameGraphView::IsDrawn() const
{
    return m_bitmap.m_hObject != nullptr;
}

void CFlameGraphView::Inactivate()
{
    ClearHover();

    if (m_bitmap.m_hObject != nullptr)
    {
        m_dimmed.DeleteObject();
        m_dimmed.Attach(m_bitmap.Detach());
        m_dimmedSize = m_size;

        CClientDC dc(this);
        CDC dcmem;
        dcmem.CreateCompatibleDC(&dc);
        CSelectObject sobmp(&dcmem, &m_dimmed);

        constexpr BLENDFUNCTION blendFunc{
            .BlendOp = AC_SRC_OVER, .BlendFlags = 0,
            .SourceConstantAlpha = 175, .AlphaFormat = 0 };
        dcmem.FillSolidRect(CRect(0, 0, m_dimmedSize.cx, m_dimmedSize.cy), RGB(0, 0, 0));
        dcmem.AlphaBlend(0, 0, m_dimmedSize.cx, m_dimmedSize.cy, &dc,
            0, 0, m_dimmedSize.cx, m_dimmedSize.cy, blendFunc);
    }

    m_flameGraph.ClearLayout();
    m_fullHeight = 0;
}

void CFlameGraphView::EmptyView()
{
    ClearHover();
    m_bitmap.DeleteObject();
    m_dimmed.DeleteObject();
    m_dimmedSize = { 0, 0 };
    m_flameGraph.ClearLayout();
    m_fullHeight = 0;
    m_scrollPos = 0;
    m_wheelDeltaRemainder = 0;
    m_forceScrollBarVisible = false;
    UpdateScrollBar(m_fullHeight, m_size.cy);
}

void CFlameGraphView::OnSetFocus(CWnd* /*pOldWnd*/)
{
    CMainFrame::Get()->GetFileTreeView()->SetFocus();
}

void CFlameGraphView::OnUpdate(CWnd* sender, const MODEL_CHANGE change, CItem* item)
{
    if (!CWinDirStatModel::Get()->IsRootDone())
    {
        Inactivate();
    }

    switch (change)
    {
    case MODEL_CHANGE_NEW_ROOT:
        {
            EmptyView();
            CWinDirStatPane::OnUpdate(sender, change, item);
            CMainFrame::Get()->UpdatePaneText();
        }
        break;

    case MODEL_CHANGE_SELECTION_ACTION:
    case MODEL_CHANGE_SELECTION_REFRESH:
    case MODEL_CHANGE_SELECTION_STYLE:
    case MODEL_CHANGE_EXTENSION_SELECTION:
        {
            CWinDirStatPane::OnUpdate(sender, change, item);
            CMainFrame::Get()->UpdatePaneText();
        }
        break;

    case MODEL_CHANGE_TREEMAP_STYLE:
        {
            ClearHover();
            DiscardBase(false);
            CWinDirStatPane::OnUpdate(sender, change, item);
        }
        break;

    case MODEL_CHANGE_ZOOM:
        {
            ClearHover();
            m_scrollPos = 0;
            m_wheelDeltaRemainder = 0;
            DiscardBase(true);
            CWinDirStatPane::OnUpdate(sender, change, item);
        }
        break;

    case MODEL_CHANGE_NONE:
        {
            ClearHover();
            DiscardBase(true);
            CWinDirStatPane::OnUpdate(sender, change, item);
        }
        break;

    default:
        break;
    }
}

HoverInfo CFlameGraphView::GetHoverInfo() const
{
    CPoint point;
    GetCursorPos(&point);
    ScreenToClient(&point);

    if (const CRect rc = ClientRectOf(this); !rc.PtInRect(point))
    {
        return {};
    }

    return { m_paneTextOverride, m_paneSizeOverride };
}

void CFlameGraphView::OnContextMenu(CWnd* /*pWnd*/, const CPoint point)
{
    static constexpr std::array<UINT, 7> persistentCommands{
        ID_TREEMAP_ZOOMIN,
        ID_TREEMAP_ZOOMOUT,
        ID_TREEMAP_SELECT_PARENT,
        ID_TREEMAP_RESELECT_CHILD,
        ID_VIEW_GROUP_TYPES,
        ID_TREEMAP_SHOW_EXTENSIONS,
        ID_TREEMAP_LOGICAL_SIZE,
    };
    ShowGraphContextMenu(ResolveItemAtPoint(point, true), point, persistentCommands);
}

void CFlameGraphView::OnMouseMove(UINT /*nFlags*/, const CPoint point)
{
    if (!m_trackingMouse)
    {
        TRACKMOUSEEVENT tme{
            .cbSize = sizeof(TRACKMOUSEEVENT),
            .dwFlags = TME_LEAVE,
            .hwndTrack = m_hWnd,
        };
        m_trackingMouse = ::TrackMouseEvent(&tme) != FALSE;
    }

    auto* item = ResolveItemAtPoint(point);
    if (item == nullptr)
    {
        ClearHover();
        return;
    }
    if (item == m_hoverItem) return;

    SetHoverItem(item);
    m_paneTextOverride = item->GetPath();
    m_paneSizeOverride = item->GetSizeLogical();
    CMainFrame::Get()->UpdatePaneText();
}

void CFlameGraphView::OnMouseLeave()
{
    m_trackingMouse = false;
    ClearHover();
}

BOOL CFlameGraphView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if (!CMainFrame::Get())
        return CWinDirStatPane::OnMouseWheel(nFlags, zDelta, pt);
    if (!EnsureFullHeightForInput()) return TRUE;

    const int totalDelta = m_wheelDeltaRemainder + static_cast<int>(zDelta);
    const int clicks = totalDelta / WHEEL_DELTA;
    m_wheelDeltaRemainder = totalDelta % WHEEL_DELTA;
    if (clicks == 0) return TRUE;

    const int oldPos = m_scrollPos;
    const int requested = m_scrollPos - clicks * m_rowHeight * 3;
    const int maxScroll = std::max(0, m_fullHeight - static_cast<int>(m_size.cy));
    m_scrollPos = std::clamp(requested, 0, maxScroll);
    if (m_scrollPos != oldPos)
    {
        ClearHover();
        // Scrolling changes only the cached viewport. Reuse logical geometry
        // for hit testing and the repaint that rebuilds the bitmap.
        m_bitmap.DeleteObject();
        UpdateScrollBar(m_fullHeight, m_size.cy);
        Invalidate(FALSE);
    }
    return TRUE;
}

void CFlameGraphView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* /*pScrollBar*/)
{
    // A direct scrollbar action starts a new input sequence; do not carry a
    // fractional high-resolution wheel delta into a later wheel gesture.
    m_wheelDeltaRemainder = 0;
    if (nSBCode == SB_ENDSCROLL) return;
    if (!EnsureFullHeightForInput()) return;

    const int cy = static_cast<int>(m_size.cy);
    const int oldPos = m_scrollPos;

    switch (nSBCode)
    {
    case SB_LINEUP:        m_scrollPos -= m_rowHeight; break;
    case SB_LINEDOWN:      m_scrollPos += m_rowHeight; break;
    case SB_PAGEUP:        m_scrollPos -= cy; break;
    case SB_PAGEDOWN:      m_scrollPos += cy; break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
        {
            SCROLLINFO si{
                .cbSize = sizeof(SCROLLINFO),
                .fMask = SIF_TRACKPOS,
            };
            m_scrollPos = GetScrollInfo(SB_VERT, &si) ? si.nTrackPos : static_cast<int>(nPos);
        }
        break;
    case SB_TOP:           m_scrollPos = 0; break;
    case SB_BOTTOM:        m_scrollPos = INT_MAX; break;
    }

    const int maxScroll = std::max(0, m_fullHeight - cy);
    m_scrollPos = std::clamp(m_scrollPos, 0, maxScroll);

    if (m_scrollPos != oldPos)
    {
        ClearHover();
        // Reuse logical geometry for hit testing and the newly scrolled paint.
        m_bitmap.DeleteObject();
        UpdateScrollBar(m_fullHeight, m_size.cy);
        Invalidate(FALSE);
    }
}
