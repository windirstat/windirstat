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
#include "FlameGraphView.h"

IMPLEMENT_DYNCREATE(CFlameGraphView, CGraphView)

BEGIN_MESSAGE_MAP(CFlameGraphView, CGraphView)
    ON_WM_MOUSEWHEEL()
    ON_WM_VSCROLL()
END_MESSAGE_MAP()

void CFlameGraphView::DrawEmptyPlaceholder(CDC* pDC, const CRect& rc)
{
    const int cols = 8;
    const int rows = 5;
    const int cellW = rc.Width() / cols;
    const int cellH = rc.Height() / rows;

    for (int r = 0; r < rows; r++)
    {
        const int y = rc.top + r * cellH;
        const int height = (r == rows - 1) ? rc.bottom - y : cellH;

        CRect labelRc(rc.left, y, rc.left + 30, y + height);
        if (labelRc.Height() >= 10) pDC->FillSolidRect(labelRc, RGB(30, 30, 30));

        int x = rc.left + 30;
        const int colsThisRow = cols - r;
        for (int c = 0; c < colsThisRow; c++)
        {
            const int shade = 40 + r * 30 + c * 10;
            const int w = (c == colsThisRow - 1) ? rc.right - x : cellW;

            CRect tile(x, y, x + w, y + height);
            if (tile.Width() > 0 && tile.Height() > 0)
                pDC->FillSolidRect(tile, RGB(shade, shade, shade));
            x += w;
        }
    }
}

bool CFlameGraphView::PrepareDrawing(CDC* pDC, CRect& rc)
{
    ASSERT(m_size == rc.Size());
    ASSERT(rc.TopLeft() == CPoint(0, 0));
    if (rc.IsRectEmpty()) return false;

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

    // Preparing geometry also computes the exact visible depth, avoiding the
    // former full hierarchy walk here followed by a second walk while drawing.
    int fullHeight = ComputeFlameFullHeight(rc.Width());

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
            return false;
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
        fullHeight = ComputeFlameFullHeight(rc.Width());
    }
    m_fullHeight = fullHeight;
    return true;
}

void CFlameGraphView::RenderVisualization(CDC* pDC, const CRect rect)
{
    RenderViewport(pDC, rect);
}

void CFlameGraphView::RenderViewport(CDC* pDC, CRect clip)
{
    CRect client = ClientRectOf(this);
    if (!clip.IntersectRect(clip, client)) return;

    const int savedState = pDC->SaveDC();
    pDC->IntersectClipRect(clip);
    pDC->FillSolidRect(clip, BackgroundColor);

    const int breadcrumbHeight = std::min(m_flameGraph.GetBreadcrumbHeight(),
        client.Height());
    CRect graphClip = clip;
    graphClip.top = std::max<LONG>(graphClip.top, breadcrumbHeight);
    if (!graphClip.IsRectEmpty())
    {
        const int graphState = pDC->SaveDC();
        pDC->IntersectClipRect(graphClip);

        // Layout coordinates cover the full graph; the cached bitmap contains
        // only the visible client-sized viewport below the sticky header.
        pDC->SetViewportOrg(0, -m_scrollPos);
        m_flameGraph.DrawFlameGraph(pDC);
        pDC->RestoreDC(graphState);
    }

    CRect breadcrumbClip(0, 0, client.Width(), breadcrumbHeight);
    if (breadcrumbHeight > 0 && breadcrumbClip.IntersectRect(breadcrumbClip, clip))
    {
        const int breadcrumbState = pDC->SaveDC();
        pDC->IntersectClipRect(breadcrumbClip);
        m_flameGraph.DrawBreadcrumbs(pDC);
        pDC->RestoreDC(breadcrumbState);
    }
    pDC->RestoreDC(savedState);
}

void CFlameGraphView::DrawHighlightExtension(CDC* pdc)
{
    CWaitCursor wc;

    const CRect client = ClientRectOf(this);
    const int savedState = pdc->SaveDC();
    pdc->IntersectClipRect(CRect(0, m_flameGraph.GetBreadcrumbHeight(),
        client.Width(), client.Height()));

    CPen pen(PS_SOLID, 1, COptions::TreeMapHighlightColor);
    CSelectObject sopen(pdc, &pen);
    CSelectStockObject sobrush(pdc, NULL_BRUSH);

    CRect rcClip;
    if (pdc->GetClipBox(&rcClip) == ERROR)
    {
        rcClip = ClientRectOf(this);
    }

    m_flameGraph.VisitItemsIntersecting(rcClip, CPoint(0, -m_scrollPos),
        [&](const CItem* item, const CRect& itemRectangle)
    {
        if (IsExtensionHighlighted(item))
        {
            CRect rc = itemRectangle;
            RenderHighlightRectangle(pdc, rc);
        }
    });
    pdc->RestoreDC(savedState);
}

void CFlameGraphView::DrawSelection(CDC* pdc)
{
    CSelectStockObject sobrush(pdc, NULL_BRUSH);

    CPen pen(PS_SOLID, 1, COptions::TreeMapHighlightColor);
    CSelectObject sopen(pdc, &pen);

    const auto& items = CWinDirStatModel::Get()->GetAllSelected();
    for (const CItem* item : items)
    {
        HighlightSelectedItem(pdc, GetDisplayItem(item), items.size() == 1);
    }
}

void CFlameGraphView::HighlightSelectedItem(CDC* pdc, const CItem* item, const bool single) const
{
    CRect rc;
    if (!m_flameGraph.TryGetItemRectangle(item, rc))
    {
        return;
    }
    const bool breadcrumb = m_flameGraph.IsBreadcrumb(item);
    rc.OffsetRect(0, breadcrumb ? 0 : -m_scrollPos);

    if (single)
    {
        CRect rcClient = ClientRectOf(this);

        if (rcClient.left < rc.left) rc.left--;
        if (rcClient.top < rc.top) rc.top--;
        if (rc.right < rcClient.right) rc.right++;
        if (rc.bottom < rcClient.bottom) rc.bottom++;
    }

    CRect clip = ClientRectOf(this);
    if (!breadcrumb)
        clip.top = std::max<LONG>(clip.top, m_flameGraph.GetBreadcrumbHeight());
    CRect visible;
    if (rc.Width() <= 0 || rc.Height() <= 0 || !visible.IntersectRect(rc, clip))
    {
        return;
    }

    const int savedState = pdc->SaveDC();
    pdc->IntersectClipRect(visible);
    RenderHighlightRectangle(pdc, rc);
    pdc->RestoreDC(savedState);
}

CItem* CFlameGraphView::FindItemAtPoint(CPoint point)
{
    if (point.y >= m_flameGraph.GetBreadcrumbHeight()) point.y += m_scrollPos;

    return m_flameGraph.FindItemByPoint(CWinDirStatModel::Get()->GetZoomItem(), point);
}

bool CFlameGraphView::HasValidLayout() const
{
    CRect rectangle;
    return m_flameGraph.TryGetItemRectangle(
        CWinDirStatModel::Get()->GetZoomItem(), rectangle);
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

bool CFlameGraphView::ScrollCachedViewport(const int oldPosition)
{
    if (!IsDrawn() || !HasValidLayout() || m_size.cx <= 0 || m_size.cy <= 0)
    {
        return false;
    }

    const int delta = m_scrollPos - oldPosition;
    if (delta == 0) return true;

    const int fixedTop = std::min(m_flameGraph.GetBreadcrumbHeight(),
        static_cast<int>(m_size.cy));
    const int scrollableHeight = static_cast<int>(m_size.cy) - fixedTop;
    if (scrollableHeight <= 0) return true;

    CClientDC windowDc(this);
    CDC memoryDc;
    if (!memoryDc.CreateCompatibleDC(&windowDc)) return false;
    CSelectObject selectBitmap(&memoryDc, &m_bitmap);

    const int distance = std::abs(delta);
    if (distance >= scrollableHeight)
    {
        RenderViewport(&memoryDc, CRect(0, fixedTop, m_size.cx, m_size.cy));
        return true;
    }

    const CRect scrollRectangle(0, fixedTop, m_size.cx, m_size.cy);
    if (!::ScrollDC(memoryDc.GetSafeHdc(), 0, -delta, &scrollRectangle,
        &scrollRectangle, nullptr, nullptr))
    {
        RenderViewport(&memoryDc, scrollRectangle);
        return true;
    }

    CRect exposed;
    if (delta > 0)
    {
        // Scrolling toward deeper rows moves retained pixels upward.
        exposed.SetRect(0, m_size.cy - distance, m_size.cx, m_size.cy);
    }
    else
    {
        exposed.SetRect(0, fixedTop, m_size.cx, fixedTop + distance);
    }

    RenderViewport(&memoryDc, exposed);
    return true;
}

void CFlameGraphView::SetScrollPosition(const int position)
{
    const int maxScroll = std::max(0, m_fullHeight - static_cast<int>(m_size.cy));
    const int newPosition = std::clamp(position, 0, maxScroll);
    if (newPosition == m_scrollPos) return;

    // The status-pane item beneath the cursor becomes stale when the logical
    // viewport moves, so clear it before updating the scroll position.
    ClearHover();
    const int oldPosition = m_scrollPos;
    m_scrollPos = newPosition;
    if (!ScrollCachedViewport(oldPosition)) m_bitmap.DeleteObject();

    UpdateScrollBar(m_fullHeight, m_size.cy);
    Invalidate(FALSE);
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
    if (m_drawingSuspended || !IsWindowVisible() || model == nullptr || !model->IsRootDone())
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

int CFlameGraphView::ComputeFlameFullHeight(const int width)
{
    const CItem* zoomItem = CWinDirStatModel::Get()->GetZoomItem();
    return m_flameGraph.PrepareLayout(zoomItem, std::max(0, width), m_rowHeight);
}

void CFlameGraphView::OnBeforeSizeChanged()
{
    if (!m_updatingScrollBar) m_forceScrollBarVisible = false;
}

void CFlameGraphView::ClearVisualizationLayout()
{
    m_flameGraph.ClearLayout();
    m_fullHeight = 0;
}

void CFlameGraphView::OnViewEmptied()
{
    m_scrollPos = 0;
    m_forceScrollBarVisible = false;
    UpdateScrollBar(m_fullHeight, m_size.cy);
}

void CFlameGraphView::OnSuspending()
{
    m_forceScrollBarVisible = false;
}

void CFlameGraphView::OnInputStateReset()
{
    m_scrollWheelDeltaRemainder = 0;
}

void CFlameGraphView::OnRenderCacheTrimmed()
{
    m_flameGraph.TrimMemory();
    m_fullHeight = 0;
}

bool CFlameGraphView::CanReuseVisualizationLayout(const MODEL_CHANGE change) const
{
    // Palette/style changes alter pixels but not size-proportional geometry.
    return change == MODEL_CHANGE_TREEMAP_STYLE;
}

void CFlameGraphView::OnVisualizationChanged(const MODEL_CHANGE change)
{
    m_forceScrollBarVisible = false;
    if (change == MODEL_CHANGE_ZOOM)
    {
        m_scrollPos = 0;
    }
    CGraphView::OnVisualizationChanged(change);
}

BOOL CFlameGraphView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if (!CMainFrame::Get())
        return CWinDirStatPane::OnMouseWheel(nFlags, zDelta, pt);
    if (nFlags & MK_CONTROL)
        return CGraphView::OnMouseWheel(nFlags, zDelta, pt);
    if (!EnsureFullHeightForInput()) return TRUE;

    const int totalDelta = m_scrollWheelDeltaRemainder + static_cast<int>(zDelta);
    const int clicks = totalDelta / WHEEL_DELTA;
    m_scrollWheelDeltaRemainder = totalDelta % WHEEL_DELTA;
    if (clicks == 0) return TRUE;

    const int requested = m_scrollPos - clicks * m_rowHeight * 3;
    SetScrollPosition(requested);
    return TRUE;
}

void CFlameGraphView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* /*pScrollBar*/)
{
    // A direct scrollbar action starts a new input sequence; do not carry a
    // fractional high-resolution wheel delta into a later wheel gesture.
    m_scrollWheelDeltaRemainder = 0;
    if (nSBCode == SB_ENDSCROLL) return;
    if (!EnsureFullHeightForInput()) return;

    const int cy = static_cast<int>(m_size.cy);
    int requested = m_scrollPos;

    switch (nSBCode)
    {
    case SB_LINEUP:        requested -= m_rowHeight; break;
    case SB_LINEDOWN:      requested += m_rowHeight; break;
    case SB_PAGEUP:        requested -= cy; break;
    case SB_PAGEDOWN:      requested += cy; break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
        {
            SCROLLINFO si{
                .cbSize = sizeof(SCROLLINFO),
                .fMask = SIF_TRACKPOS,
            };
            requested = GetScrollInfo(SB_VERT, &si)
                ? si.nTrackPos : static_cast<int>(nPos);
        }
        break;
    case SB_TOP:           requested = 0; break;
    case SB_BOTTOM:        requested = INT_MAX; break;
    }
    SetScrollPosition(requested);
}
