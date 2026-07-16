// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
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
#include "GraphView.h"

IMPLEMENT_DYNAMIC(CGraphView, CWinDirStatPane)

BEGIN_MESSAGE_MAP(CGraphView, CWinDirStatPane)
    ON_WM_SIZE()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_LBUTTONDOWN()
    ON_WM_MBUTTONDOWN()
    ON_WM_SETFOCUS()
    ON_WM_CONTEXTMENU()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

void CGraphView::SuspendRecalculationDrawing(const bool suspend)
{
    if (suspend == m_drawingSuspended) return;

    if (suspend)
    {
        // Refresh scans can remove items immediately after this call. Drop all
        // cached item pointers and renderer geometry before mutation starts.
        ResetInputState();
        OnSuspending();
        Inactivate();
    }

    m_drawingSuspended = suspend;
    if (!suspend) Invalidate();
}

BOOL CGraphView::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CWinDirStatPane::PreCreateWindow(cs)) return FALSE;

    const wchar_t* className = GetWindowClassName();
    WNDCLASSW wc{};
    if (!::GetClassInfoW(AfxGetInstanceHandle(), className, &wc))
    {
        if (!::GetClassInfoW(AfxGetInstanceHandle(), cs.lpszClass, &wc)) return FALSE;
        wc.hbrBackground = nullptr;
        wc.lpszClassName = className;
        if (::RegisterClassW(&wc) == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return FALSE;
    }

    cs.lpszClass = className;
    return TRUE;
}

void CGraphView::DrawEmptyView()
{
    CClientDC dc(this);
    PaintEmptyView(&dc);
}

void CGraphView::ShowTreeMap(const bool show)
{
    // This method is also called when a previously hidden graph style becomes
    // active again. Its cached hover may no longer be under the cursor.
    ClearHover();
    ResetInputState();
    m_showTreeMap = show;
    if (GetSafeHwnd()) Invalidate(FALSE);
}

void CGraphView::PaintEmptyView(CDC* pDC)
{
    Inactivate();
    if (DrawDimmedView(pDC)) return;

    const CRect rect = ClientRectOf(this);
    pDC->FillSolidRect(rect, BackgroundColor);
    DrawEmptyPlaceholder(pDC, rect);
}

bool CGraphView::IsReadyToDraw() const
{
    const CItem* root = CWinDirStatModel::Get()->GetRootItem();
    return root != nullptr && root->IsDone() && !m_drawingSuspended && m_showTreeMap;
}

bool CGraphView::PrepareDrawing(CDC* /*pDC*/, CRect& rect)
{
    ASSERT(m_size == rect.Size());
    ASSERT(rect.TopLeft() == CPoint(0, 0));
    return !rect.IsRectEmpty();
}

void CGraphView::OnDraw(CDC* pDC)
{
    if (!IsReadyToDraw())
    {
        PaintEmptyView(pDC);
        return;
    }

    CRect rect = ClientRectOf(this);
    if (!PrepareDrawing(pDC, rect)) return;

    CDC memoryDc;
    if (!memoryDc.CreateCompatibleDC(pDC))
    {
        DiscardRenderCache();
        pDC->FillSolidRect(rect, BackgroundColor);
        return;
    }

    if (!IsDrawn())
    {
        CWaitCursor waitCursor;
        if (!m_bitmap.CreateCompatibleBitmap(pDC, rect.Width(), rect.Height()))
        {
            DiscardRenderCache();
            pDC->FillSolidRect(rect, BackgroundColor);
            return;
        }

        CSelectObject selectBitmap(&memoryDc, &m_bitmap);
        RenderVisualization(&memoryDc, rect);
        m_dimmed.DeleteObject();
        m_dimmedSize = { 0, 0 };
    }

    {
        CSelectObject selectBitmap(&memoryDc, &m_bitmap);
        pDC->BitBlt(rect.left, rect.top, rect.Width(), rect.Height(),
            &memoryDc, 0, 0, SRCCOPY);
    }

    DrawHoverOverlay(pDC);
    DrawHighlights(pDC);
}

bool CGraphView::DrawDimmedView(CDC* pDC)
{
    if (m_dimmed.m_hObject == nullptr) return false;

    const CRect clientRect = ClientRectOf(this);
    CDC memoryDc;
    if (!memoryDc.CreateCompatibleDC(pDC))
    {
        pDC->FillSolidRect(clientRect, BackgroundColor);
        return true;
    }

    CSelectObject selectBitmap(&memoryDc, &m_dimmed);
    pDC->BitBlt(clientRect.left, clientRect.top, m_dimmedSize.cx, m_dimmedSize.cy,
        &memoryDc, 0, 0, SRCCOPY);

    if (clientRect.Width() > m_dimmedSize.cx)
    {
        CRect fill = clientRect;
        fill.left += m_dimmedSize.cx;
        pDC->FillSolidRect(fill, BackgroundColor);
    }
    if (clientRect.Height() > m_dimmedSize.cy)
    {
        CRect fill = clientRect;
        fill.top += m_dimmedSize.cy;
        pDC->FillSolidRect(fill, BackgroundColor);
    }
    return true;
}

void CGraphView::DrawHighlights(CDC* pDC)
{
    switch (CMainFrame::Get()->GetLogicalFocus())
    {
    case LF_DUPELIST:
    case LF_TOPLIST:
    case LF_FILETREE:
    case LF_SEARCHLIST:
        DrawSelection(pDC);
        break;
    case LF_EXTLIST:
        DrawHighlightExtension(pDC);
        break;
    case LF_NONE:
        break;
    }
}

bool CGraphView::IsExtensionHighlighted(const CItem* item)
{
    const CWinDirStatModel* model = CWinDirStatModel::Get();
    if (!item->TmiIsLeaf()) return false;

    if (model->IsHighlightUnregistered())
    {
        return item->IsTypeOrFlag(IT_FILE)
            && model->GetHighlightExtensions().contains(item->GetExtension());
    }
    return item->HasExtension(model->GetHighlightExtension());
}

const CItem* CGraphView::GetDisplayItem(const CItem* item)
{
    return item->IsTypeOrFlag(ITF_HARDLINK) ? item->FindHardlinksIndexItem() : item;
}

void CGraphView::RenderHighlightRectangle(CDC* pDC, CRect& rect)
{
    ASSERT(rect.Width() >= 0);
    ASSERT(rect.Height() >= 0);

    if (rect.Width() >= 7 && rect.Height() >= 7)
    {
        pDC->Rectangle(rect);
        rect.DeflateRect(1, 1);
        pDC->Rectangle(rect);
        rect.DeflateRect(1, 1);
        pDC->Rectangle(rect);
    }
    else
    {
        pDC->FillSolidRect(rect, COptions::TreeMapHighlightColor);
    }
}

CItem* CGraphView::ResolveItemAtPoint(CPoint point, const bool isScreenCoords)
{
    const CItem* root = CWinDirStatModel::Get()->GetRootItem();
    if (root == nullptr || !root->IsDone() || !HasValidLayout()) return nullptr;

    if (isScreenCoords) ScreenToClient(&point);
    if (!ClientRectOf(this).PtInRect(point)) return nullptr;
    return FindItemAtPoint(point);
}

void CGraphView::OnSize(const UINT nType, const int cx, const int cy)
{
    CWinDirStatPane::OnSize(nType, cx, cy);
    const CSize size(cx, cy);
    if (size == m_size) return;

    OnBeforeSizeChanged();
    ResetInputState();
    Inactivate();
    m_size = size;
    if (!m_drawingSuspended) Invalidate();
}

void CGraphView::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    if (CItem* item = ResolveItemAtPoint(point)) DrillDown(item);
    CWinDirStatPane::OnLButtonDblClk(nFlags, point);
}

void CGraphView::OnLButtonDown(const UINT nFlags, const CPoint point)
{
    if (CItem* item = ResolveItemAtPoint(point))
    {
        CWinDirStatModel::Get()->ClearReselectChildStack();
        NotifyOtherPanes(MODEL_CHANGE_SELECTION_ACTION, item);
    }
    CWinDirStatPane::OnLButtonDown(nFlags, point);
}

void CGraphView::DrillDown(CItem* item)
{
    ClearHover();
    CWinDirStatModel* model = CWinDirStatModel::Get();
    CItem* target = item == model->GetZoomItem()
        ? model->GetRootItem()
        : (item->IsTypeOrFlag(IT_FILE) ? item->GetParent() : item);

    if (target != nullptr && target != model->GetZoomItem()) model->SetZoomItem(target);
}

void CGraphView::OnMButtonDown(UINT nFlags, CPoint point)
{
    ResetZoom(point);
    CWinDirStatPane::OnMButtonDown(nFlags, point);
}

void CGraphView::ResetZoom(const CPoint point)
{
    if (ResolveItemAtPoint(point))
        AfxGetMainWnd()->SendMessage(WM_COMMAND, ID_TREEMAP_ZOOMRESET);
}

void CGraphView::Inactivate()
{
    ClearHover();
    ClearVisualizationLayout();
    if (m_bitmap.m_hObject == nullptr) return;

    m_dimmed.DeleteObject();
    m_dimmed.Attach(m_bitmap.Detach());
    m_dimmedSize = m_size;

    CClientDC dc(this);
    CDC memoryDc;
    if (!memoryDc.CreateCompatibleDC(&dc)) return;
    CSelectObject selectBitmap(&memoryDc, &m_dimmed);
    constexpr BLENDFUNCTION blendFunction{
        .BlendOp = AC_SRC_OVER,
        .BlendFlags = 0,
        .SourceConstantAlpha = 175,
        .AlphaFormat = 0,
    };
    memoryDc.FillSolidRect(CRect(0, 0, m_dimmedSize.cx, m_dimmedSize.cy), RGB(0, 0, 0));
    memoryDc.AlphaBlend(0, 0, m_dimmedSize.cx, m_dimmedSize.cy, &dc,
        0, 0, m_dimmedSize.cx, m_dimmedSize.cy, blendFunction);
}

void CGraphView::DiscardRenderCache()
{
    ClearHover();
    m_bitmap.DeleteObject();
    ClearVisualizationLayout();
}

void CGraphView::EmptyView()
{
    ClearHover();
    ResetInputState();
    ClearVisualizationLayout();
    m_bitmap.DeleteObject();
    m_dimmed.DeleteObject();
    m_dimmedSize = { 0, 0 };
    OnViewEmptied();
}

void CGraphView::OnSetFocus(CWnd* /*pOldWnd*/)
{
    CMainFrame::Get()->GetFileTreeView()->SetFocus();
}

void CGraphView::OnUpdate(CWnd* sender, const MODEL_CHANGE change, CItem* item)
{
    if (!CWinDirStatModel::Get()->IsRootDone()) Inactivate();

    switch (change)
    {
    case MODEL_CHANGE_NEW_ROOT:
        EmptyView();
        CWinDirStatPane::OnUpdate(sender, change, item);
        CMainFrame::Get()->UpdatePaneText();
        break;

    case MODEL_CHANGE_SELECTION_ACTION:
    case MODEL_CHANGE_SELECTION_REFRESH:
    case MODEL_CHANGE_SELECTION_STYLE:
    case MODEL_CHANGE_EXTENSION_SELECTION:
        CWinDirStatPane::OnUpdate(sender, change, item);
        CMainFrame::Get()->UpdatePaneText();
        break;

    case MODEL_CHANGE_TREEMAP_STYLE:
    case MODEL_CHANGE_ZOOM:
    case MODEL_CHANGE_NONE:
    case MODEL_CHANGE_SIZE_MODE:
        OnVisualizationChanged(change);
        CWinDirStatPane::OnUpdate(sender, change, item);
        break;

    default:
        break;
    }
}

void CGraphView::OnVisualizationChanged(MODEL_CHANGE /*change*/)
{
    ResetInputState();
    Inactivate();
}

void CGraphView::ResetInputState()
{
    m_wheelDeltaRemainder = 0;
    OnInputStateReset();
}

HoverInfo CGraphView::GetHoverInfo() const
{
    if (!m_showTreeMap || !IsWindowVisible()) return {};

    CPoint point;
    GetCursorPos(&point);
    ScreenToClient(&point);
    if (!ClientRectOf(this).PtInRect(point)) return {};
    return { m_paneTextOverride, m_paneSizeOverride };
}

void CGraphView::ClearHover()
{
    SetHoverItem(nullptr);
    if (m_paneTextOverride.empty() && m_paneSizeOverride == 0) return;

    m_paneTextOverride.clear();
    m_paneSizeOverride = 0;
    if (CMainFrame::Get() != nullptr) CMainFrame::Get()->UpdatePaneText();
}

void CGraphView::SetHoverItem(const CItem* item)
{
    if (item == m_hoverItem) return;
    const CItem* oldItem = m_hoverItem;
    m_hoverItem = item;
    OnHoverItemChanged(oldItem, item);
}

std::span<const UINT> CGraphView::GetPersistentContextCommands() const
{
    static constexpr std::array<UINT, 8> commands{
        ID_TREEMAP_ZOOMIN,
        ID_TREEMAP_ZOOMOUT,
        ID_TREEMAP_SELECT_PARENT,
        ID_TREEMAP_RESELECT_CHILD,
        ID_VIEW_GROUP_TYPES,
        ID_TREEMAP_SHOW_EXTENSIONS,
        ID_TREEMAP_LOGICAL_SIZE,
        ID_TREEMAP_PHYSICAL_SIZE,
    };
    return commands;
}

void CGraphView::OnContextMenu(CWnd* /*pWnd*/, const CPoint point)
{
    ShowGraphContextMenu(ResolveItemAtPoint(point, true), point,
        GetPersistentContextCommands());
}

void CGraphView::OnMouseMove(UINT /*nFlags*/, const CPoint point)
{
    if (!m_trackingMouse)
    {
        TRACKMOUSEEVENT trackMouseEvent{
            .cbSize = sizeof(TRACKMOUSEEVENT),
            .dwFlags = TME_LEAVE,
            .hwndTrack = m_hWnd,
        };
        m_trackingMouse = ::TrackMouseEvent(&trackMouseEvent) != FALSE;
    }

    CItem* item = ResolveItemAtPoint(point);
    if (item == m_hoverItem) return;
    SetHoverItem(item);

    if (item == nullptr)
    {
        m_paneTextOverride.clear();
        m_paneSizeOverride = 0;
    }
    else
    {
        m_paneTextOverride = item->GetPath();
        m_paneSizeOverride = item->TmiGetSize();
    }
    CMainFrame::Get()->UpdatePaneText();
}

void CGraphView::OnMouseLeave()
{
    m_trackingMouse = false;
    ClearHover();
}

BOOL CGraphView::OnMouseWheel(const UINT nFlags, const short zDelta, const CPoint pt)
{
    CMainFrame* frame = CMainFrame::Get();
    if (frame == nullptr) return CWinDirStatPane::OnMouseWheel(nFlags, zDelta, pt);

    const int totalDelta = m_wheelDeltaRemainder + static_cast<int>(zDelta);
    const int clicks = totalDelta / WHEEL_DELTA;
    m_wheelDeltaRemainder = totalDelta % WHEEL_DELTA;
    if (clicks == 0) return TRUE;

    const UINT commandUp = (nFlags & MK_CONTROL)
        ? ID_TREEMAP_ZOOMIN : ID_TREEMAP_SELECT_PARENT;
    const UINT commandDown = (nFlags & MK_CONTROL)
        ? ID_TREEMAP_ZOOMOUT : ID_TREEMAP_RESELECT_CHILD;
    const UINT command = clicks > 0 ? commandUp : commandDown;
    for (int i = 0; i < std::abs(clicks); i++) frame->SendMessage(WM_COMMAND, command);
    return TRUE;
}
