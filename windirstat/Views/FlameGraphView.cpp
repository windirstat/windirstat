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
    ON_WM_MOUSEWHEEL()
    ON_WM_VSCROLL()
END_MESSAGE_MAP()

void CFlameGraphView::SuspendRecalculationDrawing(const bool suspend)
{
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

        // Draw a simple flame graph-like placeholder with demo rectangles
        const int cols = 8;
        const int rows = 5;
        const int cellW = rc.Width() / cols;
        const int cellH = rc.Height() / rows;

        for (int r = 0; r < rows; r++)
        {
            const int y = rc.top + r * cellH;
            const int height = (r == rows - 1) ? rc.bottom - y : cellH;

            // Draw row label
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

    CDC dcmem;
    dcmem.CreateCompatibleDC(pDC);

    const CItem* zoomItem = CWinDirStatModel::Get()->GetZoomItem();
    const int fullHeight = ComputeFlameFullHeight();

    // Clamp scroll position
    const int maxScroll = std::max(0, fullHeight - static_cast<int>(m_size.cy));
    m_scrollPos = std::clamp(m_scrollPos, 0, maxScroll);

    // Update scroll bar
    SCROLLINFO si = { sizeof(SCROLLINFO), SIF_RANGE | SIF_PAGE | SIF_POS };
    si.nMin = 0;
    si.nMax = fullHeight - 1;
    si.nPage = m_size.cy;
    si.nPos = m_scrollPos;
    SetScrollInfo(SB_VERT, &si, TRUE);

    // Render flame graph at full height into a temporary buffer
    CBitmap fullBmp;
    fullBmp.CreateCompatibleBitmap(pDC, m_size.cx, fullHeight);
    CSelectObject sobmp(&dcmem, &fullBmp);

    m_flameGraph.DrawFlameGraph(&dcmem, CRect(0, 0, m_size.cx, fullHeight), const_cast<CItem*>(zoomItem));

    // BitBlt visible portion to screen with scroll offset
    const int copyHeight = std::min(static_cast<int>(m_size.cy), fullHeight - m_scrollPos);
    if (copyHeight > 0)
    {
        pDC->BitBlt(0, 0, m_size.cx, copyHeight, &dcmem, 0, m_scrollPos, SRCCOPY);
    }

    // Fill area below the flame graph content with background
    if (m_size.cy > copyHeight)
    {
        pDC->FillSolidRect(CRect(0, copyHeight, m_size.cx, m_size.cy), CFlameGraph::BACKGROUND_COLOR);
    }

}

void CFlameGraphView::DrawZoomFrame(CDC* pdc, CRect& rc) const
{
    CRect r = rc;
    r.bottom = r.top + ZoomFrameWidth;
    pdc->FillSolidRect(r, CWinDirStatModel::Get()->GetZoomColor());

    r = rc;
    r.top = r.bottom - ZoomFrameWidth;
    pdc->FillSolidRect(r, CWinDirStatModel::Get()->GetZoomColor());

    r = rc;
    r.right = r.left + ZoomFrameWidth;
    pdc->FillSolidRect(r, CWinDirStatModel::Get()->GetZoomColor());

    r = rc;
    r.left = r.right - ZoomFrameWidth;
    pdc->FillSolidRect(r, CWinDirStatModel::Get()->GetZoomColor());

    rc.DeflateRect(ZoomFrameWidth, ZoomFrameWidth);
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
    const bool isZoomed = model->IsZoomed();

    std::vector<const CItem*> stack;
    stack.reserve(128);
    stack.push_back(model->GetZoomItem());

    while (!stack.empty())
    {
        const CItem* item = stack.back();
        stack.pop_back();

        CRect rc(item->TmiGetRectangle());
        if (isZoomed)
        {
            rc.OffsetRect(ZoomFrameWidth, ZoomFrameWidth);
        }

        if (rc.Width() <= 0 || rc.Height() <= 0)
        {
            continue;
        }

        if (item->TmiIsLeaf())
        {
            if (unregistered ? IsUnregisteredLeaf(item, highlightExtensions) : item->HasExtension(highlightExt))
            {
                RenderHighlightRectangle(pdc, rc);
            }
        }
        else for (const auto& child : item->GetChildren())
        {
            if (child->TmiGetSize() == 0) break;
            if (child->TmiGetRectangle().left == -1) break;
            stack.push_back(child);
        }
    }
}

void CFlameGraphView::DrawSelection(CDC* pdc) const
{
    CSelectStockObject sobrush(pdc, NULL_BRUSH);

    CPen pen(PS_SOLID, 1, COptions::TreeMapHighlightColor);
    CSelectObject sopen(pdc, &pen);

    const auto& items = CWinDirStatModel::Get()->GetAllSelected();
    for (const auto& item : items)
    {
        if (!CWinDirStatModel::Get()->GetZoomItem()->IsAncestorOf(item)) continue;

        const auto itemToSelect = item->IsTypeOrFlag(ITF_HARDLINK) ? item->FindHardlinksIndexItem() : item;
        HighlightSelectedItem(pdc, itemToSelect, items.size() == 1);
    }
}

void CFlameGraphView::HighlightSelectedItem(CDC* pdc, const CItem* item, const bool single) const
{
    CRect rc(item->TmiGetRectangle());

    if (CWinDirStatModel::Get()->IsZoomed())
    {
        rc.OffsetRect(ZoomFrameWidth, ZoomFrameWidth);
    }

    if (single)
    {
        CRect rcClient = ClientRectOf(this);

        if (rcClient.left < rc.left) rc.left--;
        if (rcClient.top < rc.top) rc.top--;
        if (rc.right < rcClient.right) rc.right++;
        if (rc.bottom < rcClient.bottom) rc.bottom++;
    }

    if (rc.Width() <= 0 || rc.Height() <= 0)
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

    if (CWinDirStatModel::Get()->IsZoomed())
    {
        pointClicked.Offset(-ZoomFrameWidth, -ZoomFrameWidth);
    }

    // Account for scroll offset: the layout is at full height,
    // and the screen shows a portion starting at m_scrollPos
    pointClicked.y += m_scrollPos;

    return static_cast<CItem*>(m_flameGraph.FindItemByPoint(
        CWinDirStatModel::Get()->GetZoomItem(), pointClicked));
}

int CFlameGraphView::ComputeFlameFullHeight() const
{
    const CItem* zoomItem = CWinDirStatModel::Get()->GetZoomItem();
    const CItem* rootItem = CWinDirStatModel::Get()->GetRootItem();

    // Count breadcrumb rows (ancestors of zoom item up to root)
    int breadcrumbRows = 0;
    if (zoomItem != rootItem)
    {
        for (const CItem* p = zoomItem->GetParent(); p != nullptr; p = p->GetParent())
        {
            breadcrumbRows++;
            if (p == rootItem) break;
        }
    }

    const int maxDepth = CFlameGraph::ComputeMaxDepth(const_cast<CItem*>(zoomItem), 0);
    return (breadcrumbRows + std::max(1, maxDepth)) * CFlameGraph::ROW_HEIGHT;
}

void CFlameGraphView::OnSize(const UINT nType, const int cx, const int cy)
{
    CWinDirStatPane::OnSize(nType, cx, cy);
    const CSize sz(cx, cy);
    if (sz != m_size)
    {
        Inactivate();
        m_size = sz;
        if (!m_drawingSuspended) Invalidate();
    }
}

void CFlameGraphView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    // Treat double-click as a regular click in flame graph mode
    OnLButtonDown(nFlags, point);
}

void CFlameGraphView::OnLButtonDown(const UINT nFlags, const CPoint point)
{
    m_hoverItem = nullptr;
    if (auto* item = ResolveItemAtPoint(point))
    {
        m_flameGraph.SetHoverItem(nullptr);

        // Check if clicked item is a breadcrumb ancestor
        bool isBreadcrumb = false;
        for (const auto* b : m_flameGraph.GetBreadcrumbs())
        {
            if (b == item) { isBreadcrumb = true; break; }
        }

        // Update tree view selection before changing zoom
        CWinDirStatModel::Get()->ClearReselectChildStack();
        NotifyOtherPanes(MODEL_CHANGE_SELECTION_ACTION, item);

        if (isBreadcrumb)
        {
            // Navigate back to breadcrumb ancestor
            m_scrollPos = 0;
            CWinDirStatModel::Get()->SetZoomItem(item);
        }
        else if (item == CWinDirStatModel::Get()->GetZoomItem() || item == CWinDirStatModel::Get()->GetRootItem())
        {
            // Clicking the current zoom item or root → reset to full tree
            m_scrollPos = 0;
            CWinDirStatModel::Get()->SetZoomItem(CWinDirStatModel::Get()->GetRootItem());
        }
        else
        {
            // Zoom into clicked item
            m_scrollPos = 0;
            if (item->IsTypeOrFlag(IT_FILE))
            {
                CWinDirStatModel::Get()->SetZoomItem(item->GetParent());
            }
            else
            {
                CWinDirStatModel::Get()->SetZoomItem(item);
            }
        }
    }
    CWinDirStatPane::OnLButtonDown(nFlags, point);
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
    if (m_bitmap.m_hObject == nullptr) return;

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

void CFlameGraphView::EmptyView()
{
    if (m_bitmap.m_hObject != nullptr)
    {
        m_bitmap.DeleteObject();
    }

    if (m_dimmed.m_hObject != nullptr)
    {
        m_dimmed.DeleteObject();
    }
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
    case MODEL_CHANGE_ZOOM:
        {
            m_scrollPos = 0;
            Inactivate();
            CWinDirStatPane::OnUpdate(sender, change, item);
        }
        break;

    case MODEL_CHANGE_NONE:
        {
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
    static constexpr struct {
        UINT id;
        bool isPersistent;
    } contextMenuPersistent[] = {
        { ID_TREEMAP_ZOOMIN,             true  },
        { ID_TREEMAP_ZOOMOUT,            true  },
        { ID_TREEMAP_SELECT_PARENT,      true  },
        { ID_TREEMAP_RESELECT_CHILD,     true  },
        { ID_VIEW_GROUP_TYPES,           true  },
        { ID_TREEMAP_SHOW_EXTENSIONS,    true  },
        { ID_TREEMAP_LOGICAL_SIZE,       true  },
        { ID_EDIT_COPY_CLIPBOARD,        false },
        { ID_CLEANUP_EXPLORER_SELECT,    false },
        { ID_CLEANUP_OPEN_IN_CONSOLE,    false },
        { ID_CLEANUP_OPEN_IN_PWSH,       false },
        { ID_POPUP_CANCEL,               false }
    };

    [[msvc::flatten]] static constexpr auto IsContextMenuPersistent = [](UINT id) -> bool {
        return std::ranges::any_of(contextMenuPersistent, [id](const auto& cmd) {
            return cmd.id == id && cmd.isPersistent;
        });
    };

    auto* clickedItem = ResolveItemAtPoint(point, true);
    if (clickedItem == nullptr) return;

    if (!std::ranges::any_of(CWinDirStatModel::Get()->GetAllSelected(), [&](auto* s)
        { return s == clickedItem || s->IsAncestorOf(clickedItem); }))
    {
        CWinDirStatModel::Get()->ClearReselectChildStack();
        NotifyOtherPanes(MODEL_CHANGE_SELECTION_ACTION, clickedItem);
    }

    if (CMenu menu; menu.LoadMenu(IDR_POPUP_MAP))
    {
        Localization::UpdateMenu(menu);
        if (CMenu* sub = menu.GetSubMenu(0))
        {
            UINT cmdId = 0;
            do {
                cmdId = sub->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD,
                    point.x, point.y, AfxGetMainWnd());
                if (cmdId > 0) AfxGetMainWnd()->SendMessage(WM_COMMAND, cmdId);
            } while (cmdId > 0 && IsContextMenuPersistent(cmdId));
        }
    }
}

void CFlameGraphView::OnMouseMove(UINT /*nFlags*/, const CPoint point)
{
    auto* item = ResolveItemAtPoint(point);
    if (item != nullptr)
    {
        m_paneTextOverride = item->GetPath();
        m_paneSizeOverride = item->GetSizeLogical();
        CMainFrame::Get()->UpdatePaneText();
    }
    if (item != m_flameGraph.GetHoverItem())
    {
        m_flameGraph.SetHoverItem(item);
        Invalidate();
    }
}

BOOL CFlameGraphView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if (!CMainFrame::Get())
        return CWinDirStatPane::OnMouseWheel(nFlags, zDelta, pt);

    const int clicks = zDelta / WHEEL_DELTA;
    if (clicks == 0) return TRUE;
    m_scrollPos -= clicks * CFlameGraph::ROW_HEIGHT * 3;
    const int maxScroll = ComputeFlameFullHeight() - static_cast<int>(m_size.cy);
    m_scrollPos = std::clamp(m_scrollPos, 0, std::max(0, maxScroll));
    SetScrollPos(SB_VERT, m_scrollPos, TRUE);
    Invalidate();
    return TRUE;
}

void CFlameGraphView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* /*pScrollBar*/)
{
    const int cy = static_cast<int>(m_size.cy);
    const int oldPos = m_scrollPos;

    switch (nSBCode)
    {
    case SB_LINEUP:        m_scrollPos -= CFlameGraph::ROW_HEIGHT; break;
    case SB_LINEDOWN:      m_scrollPos += CFlameGraph::ROW_HEIGHT; break;
    case SB_PAGEUP:        m_scrollPos -= cy; break;
    case SB_PAGEDOWN:      m_scrollPos += cy; break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: m_scrollPos = static_cast<int>(nPos); break;
    case SB_TOP:           m_scrollPos = 0; break;
    case SB_BOTTOM:        m_scrollPos = INT_MAX; break;
    }

    // Clamp to valid range
    const int maxScroll = ComputeFlameFullHeight() - cy;
    m_scrollPos = std::clamp(m_scrollPos, 0, std::max(0, maxScroll));

    if (m_scrollPos != oldPos)
    {
        SetScrollPos(SB_VERT, m_scrollPos, TRUE);
        Invalidate();
    }
}
