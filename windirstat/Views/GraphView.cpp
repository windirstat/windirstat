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
    if (!show)
    {
        TrimRenderCache();
    }
    else if (GetSafeHwnd())
    {
        Invalidate(FALSE);
    }
}

void CGraphView::TrimRenderCache()
{
    ResetInputState();
    ClearHover();
    m_bitmap.DeleteObject();
    m_dimmed.DeleteObject();
    m_dimmedSize = { 0, 0 };
    ClearVisualizationLayout();
    OnRenderCacheTrimmed();
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

bool CGraphView::CreateRenderBitmap(CDC* pDC, const CSize size)
{
    return m_bitmap.CreateCompatibleBitmap(pDC, size.cx, size.cy) != FALSE;
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
        if (m_dimmed.m_hObject != nullptr && m_dimmedSize == rect.Size())
        {
            // Rendering is synchronous, so the dimmed bitmap can be recycled
            // instead of briefly retaining two full-window device bitmaps.
            m_bitmap.Attach(m_dimmed.Detach());
            m_dimmedSize = { 0, 0 };
        }
        else
        {
            // Do not retain a stale full-window bitmap while allocating its
            // differently sized replacement.
            m_dimmed.DeleteObject();
            m_dimmedSize = { 0, 0 };
            if (!CreateRenderBitmap(pDC, rect.Size()))
            {
                DiscardRenderCache();
                pDC->FillSolidRect(rect, BackgroundColor);
                return;
            }
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

void CGraphView::Inactivate(const bool clearLayout)
{
    ClearHover();
    if (clearLayout) ClearVisualizationLayout();
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

void CGraphView::DiscardRenderCache(const bool clearLayout)
{
    ClearHover();
    m_bitmap.DeleteObject();
    if (clearLayout) ClearVisualizationLayout();
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
    if (!CWinDirStatModel::Get()->IsRootDone())
    {
        if (IsWindowVisible()) Inactivate();
        else TrimRenderCache();
    }

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

void CGraphView::OnVisualizationChanged(const MODEL_CHANGE change)
{
    ResetInputState();
    if (!IsWindowVisible())
    {
        TrimRenderCache();
        return;
    }
    Inactivate(!CanReuseVisualizationLayout(change));
}

void CGraphView::ResetInputState()
{
    if (m_trackingMouse && GetSafeHwnd())
    {
        TRACKMOUSEEVENT trackMouseEvent{
            .cbSize = sizeof(TRACKMOUSEEVENT),
            .dwFlags = TME_CANCEL | TME_LEAVE,
            .hwndTrack = m_hWnd,
        };
        ::TrackMouseEvent(&trackMouseEvent);
    }
    m_trackingMouse = false;
    m_navigationWheelDeltaRemainder = 0;
    m_zoomWheelDeltaRemainder = 0;
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
    m_hoverItem = nullptr;
    if (UpdateHoverDetails(nullptr, true) && CMainFrame::Get() != nullptr)
        CMainFrame::Get()->UpdatePaneText();
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
    const bool itemChanged = item != m_hoverItem;
    if (itemChanged) m_hoverItem = item;
    const bool detailsChanged = UpdateHoverDetails(item, itemChanged);
    if (detailsChanged)
    {
        CMainFrame::Get()->UpdatePaneText();
    }
}

bool CGraphView::UpdateHoverDetails(const CItem* item, const bool itemChanged)
{
    if (!itemChanged) return false;
    const std::wstring path = item == nullptr ? std::wstring{} : item->GetPath();
    const ULONGLONG size = item == nullptr ? 0 : item->TmiGetSize();
    if (path == m_paneTextOverride && size == m_paneSizeOverride) return false;

    m_paneTextOverride = path;
    m_paneSizeOverride = size;
    return true;
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

    const bool zoomCommand = (nFlags & MK_CONTROL) != 0;
    int& remainder = zoomCommand
        ? m_zoomWheelDeltaRemainder : m_navigationWheelDeltaRemainder;
    const int totalDelta = remainder + static_cast<int>(zDelta);
    const int clicks = totalDelta / WHEEL_DELTA;
    remainder = totalDelta % WHEEL_DELTA;
    if (clicks == 0) return TRUE;

    const UINT commandUp = (nFlags & MK_CONTROL)
        ? ID_TREEMAP_ZOOMIN : ID_TREEMAP_SELECT_PARENT;
    const UINT commandDown = (nFlags & MK_CONTROL)
        ? ID_TREEMAP_ZOOMOUT : ID_TREEMAP_RESELECT_CHILD;
    const UINT command = clicks > 0 ? commandUp : commandDown;
    const auto currentItem = [zoomCommand]() -> const CItem*
    {
        return zoomCommand
            ? CWinDirStatModel::Get()->GetZoomItem()
            : CFileTreeControl::Get()->GetFirstSelectedItem<CItem>();
    };
    for (int i = 0; i < std::abs(clicks); i++)
    {
        const CItem* before = currentItem();
        frame->SendMessage(WM_COMMAND, command);
        if (currentItem() == before) break;
    }
    return TRUE;
}
