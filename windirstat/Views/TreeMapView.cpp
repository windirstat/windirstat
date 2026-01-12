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
#include "SelectObject.h"
#include "TreeMapView.h"

IMPLEMENT_DYNCREATE(CTreeMapView, CView)

BEGIN_MESSAGE_MAP(CTreeMapView, CView)
    ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
    ON_WM_SETFOCUS()
    ON_WM_CONTEXTMENU()
    ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

void CTreeMapView::SuspendRecalculationDrawing(const bool suspend)
{
    m_drawingSuspended = suspend;
    if (!suspend)
    {
        Invalidate();
    }
}

bool CTreeMapView::IsShowTreeMap() const
{
    return m_showTreeMap;
}

void CTreeMapView::ShowTreeMap(const bool show)
{
    m_showTreeMap = show;
}

BOOL CTreeMapView::PreCreateWindow(CREATESTRUCT& cs)
{
    CView::PreCreateWindow(cs);

    WNDCLASS wc;
    GetClassInfo(AfxGetInstanceHandle(), cs.lpszClass, &wc);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"windirstatTreeMapClass";
    cs.lpszClass = reinterpret_cast<LPCWSTR>(::RegisterClass(&wc));

    return true;
}

void CTreeMapView::DrawEmptyView()
{
    CClientDC dc(this);
    DrawEmptyView(&dc);
}

void CTreeMapView::DrawEmptyView(CDC* pDC)
{
    constexpr COLORREF gray = RGB(160, 160, 160);

    Inactivate();

    CRect rc;
    GetClientRect(rc);

    if (m_dimmed.m_hObject == nullptr)
    {
        pDC->FillSolidRect(rc, gray);
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
            r.left  = r.left + m_dimmedSize.cx;
            pDC->FillSolidRect(r, gray);
        }

        if (rc.Height() > m_dimmedSize.cy)
        {
            CRect r = rc;
            r.top   = r.top + m_dimmedSize.cy;
            pDC->FillSolidRect(r, gray);
        }
    }
}

void CTreeMapView::OnDraw(CDC * pDC)
{
    const CItem* root = CDirStatDoc::Get()->GetRootItem();
    if (root == nullptr || !root->IsDone() || m_drawingSuspended || !m_showTreeMap)
    {
        DrawEmptyView(pDC);
        return;
    }

    CRect rc;
    GetClientRect(rc);
    ASSERT(m_size == rc.Size());
    ASSERT(rc.TopLeft() == CPoint(0, 0));

    CDC dcmem;
    dcmem.CreateCompatibleDC(pDC);

    if (!IsDrawn())
    {
        CWaitCursor wc;

        m_bitmap.CreateCompatibleBitmap(pDC, m_size.cx, m_size.cy);

        CSelectObject sobmp(&dcmem, &m_bitmap);

        if (CDirStatDoc::Get()->IsZoomed())
        {
            DrawZoomFrame(&dcmem, rc);
        }

        m_treeMap.DrawTreeMap(&dcmem, rc, CDirStatDoc::Get()->GetZoomItem(), &COptions::TreeMapOptions);
    }

    CSelectObject sobmp2(&dcmem, &m_bitmap);

    pDC->BitBlt(0, 0, m_size.cx, m_size.cy, &dcmem, 0, 0, SRCCOPY);

    DrawHighlights(pDC);
}

void CTreeMapView::DrawZoomFrame(CDC* pdc, CRect& rc) const
{
    CRect r  = rc;
    r.bottom = r.top + ZoomFrameWidth;
    pdc->FillSolidRect(r, CDirStatDoc::Get()->GetZoomColor());

    r = rc;
    r.top = r.bottom - ZoomFrameWidth;
    pdc->FillSolidRect(r, CDirStatDoc::Get()->GetZoomColor());

    r = rc;
    r.right = r.left + ZoomFrameWidth;
    pdc->FillSolidRect(r, CDirStatDoc::Get()->GetZoomColor());

    r = rc;
    r.left = r.right - ZoomFrameWidth;
    pdc->FillSolidRect(r, CDirStatDoc::Get()->GetZoomColor());

    rc.DeflateRect(ZoomFrameWidth, ZoomFrameWidth);
}

void CTreeMapView::DrawHighlights(CDC* pdc)
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

void CTreeMapView::DrawHighlightExtension(CDC* pdc)
{
    CWaitCursor wc;

    CPen pen(PS_SOLID, 1, COptions::TreeMapHighlightColor);
    CSelectObject sopen(pdc, &pen);
    CSelectStockObject sobrush(pdc, NULL_BRUSH);
    RecurseHighlightExtension(pdc, CDirStatDoc::Get()->GetZoomItem());
}

void CTreeMapView::RecurseHighlightExtension(CDC* pdc, const CItem* item)
{
    CRect rc(item->TmiGetRectangle());
    if (CDirStatDoc::Get()->IsZoomed())
    {
        rc.OffsetRect(ZoomFrameWidth, ZoomFrameWidth);
    }

    if (rc.Width() <= 0 || rc.Height() <= 0)
    {
        return;
    }

    if (item->TmiIsLeaf())
    {
        if (item->IsTypeOrFlag(IT_FILE) && _wcsicmp(item->GetExtension().c_str(), CDirStatDoc::Get()->GetHighlightExtension().c_str()) == 0)
        {
            RenderHighlightRectangle(pdc, rc);
        }
    }
    else for (const auto& child : item->GetChildren())
    {
        if (child->TmiGetSize() == 0)
        {
            break;
        }
        if (child->TmiGetRectangle().left == -1)
        {
            break;
        }
        RecurseHighlightExtension(pdc, child);
    }
}

void CTreeMapView::DrawSelection(CDC* pdc) const
{
    CSelectStockObject sobrush(pdc, NULL_BRUSH);

    CPen pen(PS_SOLID, 1, COptions::TreeMapHighlightColor);
    CSelectObject sopen(pdc, &pen);

    const auto& items = CDirStatDoc::Get()->GetAllSelected();
    for (const auto& item : items)
    {
        // Ignore if not a child of the current zoomed item
        if (!CDirStatDoc::Get()->GetZoomItem()->IsAncestorOf(item)) continue;

        auto itemToSelect = item->IsTypeOrFlag(ITF_HARDLINK) ? item->FindHardlinksIndexItem() : item;
        HighlightSelectedItem(pdc, itemToSelect, items.size() == 1);
    }
}

// A pen and the null brush must be selected.
// Draws the highlight rectangle of item. If single, the rectangle is slightly
// bigger than the item rect, else it fits inside.
//
void CTreeMapView::HighlightSelectedItem(CDC* pdc, const CItem* item, const bool single) const
{
    CRect rc(item->TmiGetRectangle());

    // Offset the display rectangle if zoomed
    if (CDirStatDoc::Get()->IsZoomed())
    {
        rc.OffsetRect(ZoomFrameWidth, ZoomFrameWidth);
    }

    if (single)
    {
        CRect rcClient;
        GetClientRect(rcClient);

        if (m_treeMap.GetOptions().grid)
        {
            rc.right++;
            rc.bottom++;
        }

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

// A pen and the null brush must be selected.
//
void CTreeMapView::RenderHighlightRectangle(CDC* pdc, CRect& rc) const
{
    ASSERT(rc.Width() >= 0);
    ASSERT(rc.Height() >= 0);

    // The documentation of CDC::Rectangle() says that the width
    // and height must be greater than 2. Experiment says that
    // it must be greater than 1. We follow the documentation.

    if (rc.Width() >= 7 && rc.Height() >= 7)
    {
        pdc->Rectangle(rc); // w = 7
        rc.DeflateRect(1, 1);
        pdc->Rectangle(rc); // w = 5
        rc.DeflateRect(1, 1);
        pdc->Rectangle(rc); // w = 3
    }
    else
    {
        pdc->FillSolidRect(rc, COptions::TreeMapHighlightColor);
    }
}

void CTreeMapView::OnSize(const UINT nType, const int cx, const int cy)
{
    CView::OnSize(nType, cx, cy);
    const CSize sz(cx, cy);
    if (sz != m_size)
    {
        Inactivate();
        m_size = sz;
    }
}

void CTreeMapView::OnLButtonDown(const UINT nFlags, const CPoint point)
{
    // Offset the click point if zoomed
    CPoint pointClicked = point;
    if (CDirStatDoc::Get()->IsZoomed())
    {
        pointClicked.Offset(-1 * ZoomFrameWidth, -1 * ZoomFrameWidth);
    }

    const CItem* root = CDirStatDoc::Get()->GetRootItem();
    if (root != nullptr && root->IsDone() && IsDrawn())
    {
        const auto item = static_cast<CItem*>(m_treeMap.FindItemByPoint(CDirStatDoc::Get()->GetZoomItem(), pointClicked));
        if (item == nullptr)
        {
            return;
        }

        CDirStatDoc::Get()->UpdateAllViews(this, HINT_SELECTIONACTION, item);
    }
    CView::OnLButtonDown(nFlags, point);
}

bool CTreeMapView::IsDrawn() const
{
    return m_bitmap.m_hObject != nullptr;
}

void CTreeMapView::Inactivate()
{
    if (m_bitmap.m_hObject == nullptr) return;

    // Move the old bitmap to m_dimmed for later dimmed display
    m_dimmed.DeleteObject();
    m_dimmed.Attach(m_bitmap.Detach());
    m_dimmedSize = m_size;
    
    // Dim m_dimmed contents to indicate inactive/refresh state
    CClientDC dc(this);
    CDC dcmem;
    dcmem.CreateCompatibleDC(&dc);
    CSelectObject sobmp(&dcmem, &m_dimmed);

    // Apply the dimming overlay
    BLENDFUNCTION blendFunc{};
    blendFunc.BlendOp = AC_SRC_OVER;
    blendFunc.BlendFlags = 0;
    blendFunc.SourceConstantAlpha = 175;
    blendFunc.AlphaFormat = 0;
    dcmem.FillSolidRect(CRect(0, 0, m_dimmedSize.cx, m_dimmedSize.cy), RGB(0, 0, 0));
    dcmem.AlphaBlend(0, 0, m_dimmedSize.cx, m_dimmedSize.cy, &dc,
        0, 0, m_dimmedSize.cx, m_dimmedSize.cy, blendFunc);
}

void CTreeMapView::EmptyView()
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

void CTreeMapView::OnSetFocus(CWnd* /*pOldWnd*/)
{
    CMainFrame::Get()->GetFileTreeView()->SetFocus();
}

void CTreeMapView::OnUpdate(CView* pSender, const LPARAM lHint, CObject* pHint)
{
    if (!CDirStatDoc::Get()->IsRootDone())
    {
        Inactivate();
    }

    switch (lHint)
    {
    case HINT_NEWROOT:
        {
            EmptyView();
            CView::OnUpdate(pSender, lHint, pHint);
            CMainFrame::Get()->UpdatePaneText();
        }
        break;

    case HINT_SELECTIONACTION:
    case HINT_SELECTIONREFRESH:
    case HINT_SELECTIONSTYLECHANGED:
    case HINT_EXTENSIONSELECTIONCHANGED:
        {
            CView::OnUpdate(pSender, lHint, pHint);
            CMainFrame::Get()->UpdatePaneText();
        }
        break;

    case HINT_TREEMAPSTYLECHANGED:
    case HINT_ZOOMCHANGED:
        {
            Inactivate();
            CView::OnUpdate(pSender, lHint, pHint);
        }
        break;

    case HINT_NULL:
        {
            CView::OnUpdate(pSender, lHint, pHint);
        }
        break;

    default:
        break;
    }
}

std::tuple<std::wstring, ULONGLONG>  CTreeMapView::GetTreeMapHoverInfo()
{
    CPoint point;
    GetCursorPos(&point);
    ScreenToClient(&point);

    CRect rc;
    GetClientRect(rc);

    if (!rc.PtInRect(point))
    {
        m_paneTextOverride = {};
        m_paneSizeOverride = 0;
    }

    return { m_paneTextOverride, m_paneSizeOverride };
}

void CTreeMapView::OnContextMenu(CWnd* /*pWnd*/, const CPoint point)
{
    const CItem* root = CDirStatDoc::Get()->GetRootItem();
    if (root != nullptr && root->IsDone())
    {
        CMenu menu;
        menu.LoadMenu(IDR_POPUP_MAP);
        Localization::UpdateMenu(menu);
        CMenu* sub = menu.GetSubMenu(0);
        sub->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, AfxGetMainWnd());
    }
}

void CTreeMapView::OnMouseMove(UINT /*nFlags*/, const CPoint point)
{
    if (CDirStatDoc::Get()->IsRootDone() && IsDrawn())
    {
        const auto item = static_cast<const CItem*>(m_treeMap.FindItemByPoint(CDirStatDoc::Get()->GetZoomItem(), point));
        if (item != nullptr)
        {
            m_paneTextOverride = item->GetPath();
            m_paneSizeOverride = item->GetSizeLogical();
            CMainFrame::Get()->UpdatePaneText();
        }
    }
}
