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
    m_DrawingSuspended = suspend;
    if (!suspend)
    {
        Invalidate();
    }
}

bool CTreeMapView::IsShowTreeMap() const
{
    return m_ShowTreeMap;
}

void CTreeMapView::ShowTreeMap(const bool show)
{
    m_ShowTreeMap = show;
}

BOOL CTreeMapView::PreCreateWindow(CREATESTRUCT& cs)
{
    VERIFY(CView::PreCreateWindow(cs)); // this registers a wndclass

    WNDCLASS wc;
    VERIFY(GetClassInfo(AfxGetInstanceHandle(), cs.lpszClass, &wc));
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

    if (m_Dimmed.m_hObject == nullptr)
    {
        pDC->FillSolidRect(rc, gray);
    }
    else
    {
        CDC dcmem;
        dcmem.CreateCompatibleDC(pDC);
        CSelectObject sobmp(&dcmem, &m_Dimmed);
        pDC->BitBlt(rc.left, rc.top, m_DimmedSize.cx, m_DimmedSize.cy, &dcmem, 0, 0, SRCCOPY);

        if (rc.Width() > m_DimmedSize.cx)
        {
            CRect r = rc;
            r.left  = r.left + m_DimmedSize.cx;
            pDC->FillSolidRect(r, gray);
        }

        if (rc.Height() > m_DimmedSize.cy)
        {
            CRect r = rc;
            r.top   = r.top + m_DimmedSize.cy;
            pDC->FillSolidRect(r, gray);
        }
    }
}

void CTreeMapView::OnDraw(CDC * pDC)
{
    const CItem* root = GetDocument()->GetRootItem();
    if (root == nullptr || !root->IsDone() || m_DrawingSuspended || !m_ShowTreeMap)
    {
        DrawEmptyView(pDC);
        return;
    }

    CRect rc;
    GetClientRect(rc);
    ASSERT(m_Size == rc.Size());
    ASSERT(rc.TopLeft() == CPoint(0, 0));

    CDC dcmem;
    dcmem.CreateCompatibleDC(pDC);

    if (!IsDrawn())
    {
        CWaitCursor wc;

        m_Bitmap.CreateCompatibleBitmap(pDC, m_Size.cx, m_Size.cy);

        CSelectObject sobmp(&dcmem, &m_Bitmap);

        if (GetDocument()->IsZoomed())
        {
            DrawZoomFrame(&dcmem, rc);
        }

        m_TreeMap.DrawTreeMap(&dcmem, rc, GetDocument()->GetZoomItem(), &COptions::TreeMapOptions);
    }

    CSelectObject sobmp2(&dcmem, &m_Bitmap);

    pDC->BitBlt(0, 0, m_Size.cx, m_Size.cy, &dcmem, 0, 0, SRCCOPY);

    DrawHighlights(pDC);
}

void CTreeMapView::DrawZoomFrame(CDC* pdc, CRect& rc) const
{
    CRect r  = rc;
    r.bottom = r.top + ZoomFrameWidth;
    pdc->FillSolidRect(r, GetDocument()->GetZoomColor());

    r = rc;
    r.top = r.bottom - ZoomFrameWidth;
    pdc->FillSolidRect(r, GetDocument()->GetZoomColor());

    r = rc;
    r.right = r.left + ZoomFrameWidth;
    pdc->FillSolidRect(r, GetDocument()->GetZoomColor());

    r = rc;
    r.left = r.right - ZoomFrameWidth;
    pdc->FillSolidRect(r, GetDocument()->GetZoomColor());

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
    RecurseHighlightExtension(pdc, GetDocument()->GetZoomItem());
}

void CTreeMapView::RecurseHighlightExtension(CDC* pdc, const CItem* item)
{
    CRect rc(item->TmiGetRectangle());
    if (GetDocument()->IsZoomed())
    {
        rc.OffsetRect(ZoomFrameWidth, ZoomFrameWidth);
    }

    if (rc.Width() <= 0 || rc.Height() <= 0)
    {
        return;
    }

    if (item->TmiIsLeaf())
    {
        if (item->IsTypeOrFlag(IT_FILE) && _wcsicmp(item->GetExtension().c_str(), GetDocument()->GetHighlightExtension().c_str()) == 0)
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
        if (!GetDocument()->GetZoomItem()->IsAncestorOf(item)) continue;
        
        HighlightSelectedItem(pdc, item, items.size() == 1);
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
    if (GetDocument()->IsZoomed())
    {
        rc.OffsetRect(ZoomFrameWidth, ZoomFrameWidth);
    }

    if (single)
    {
        CRect rcClient;
        GetClientRect(rcClient);

        if (m_TreeMap.GetOptions().grid)
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
    if (sz != m_Size)
    {
        Inactivate();
        m_Size = sz;
    }
}

void CTreeMapView::OnLButtonDown(const UINT nFlags, const CPoint point)
{
    // Offset the click point if zoomed
    CPoint pointClicked = point;
    if (GetDocument()->IsZoomed())
    {
        pointClicked.Offset(-1 * ZoomFrameWidth, -1 * ZoomFrameWidth);
    }

    const CItem* root = GetDocument()->GetRootItem();
    if (root != nullptr && root->IsDone() && IsDrawn())
    {
        const auto item = static_cast<CItem*>(m_TreeMap.FindItemByPoint(GetDocument()->GetZoomItem(), pointClicked));
        if (item == nullptr)
        {
            return;
        }

        GetDocument()->UpdateAllViews(this, HINT_SELECTIONACTION, reinterpret_cast<CObject*>(item));
    }
    CView::OnLButtonDown(nFlags, point);
}

bool CTreeMapView::IsDrawn() const
{
    return m_Bitmap.m_hObject != nullptr;
}

void CTreeMapView::Inactivate()
{
    if (m_Bitmap.m_hObject == nullptr) return;

    // Move the old bitmap to m_Dimmed for later dimmed display
    m_Dimmed.DeleteObject();
    m_Dimmed.Attach(m_Bitmap.Detach());
    m_DimmedSize = m_Size;
    
    // Dim m_Dimmed contents to indicate inactive/refresh state
    CClientDC dc(this);
    CDC dcmem;
    dcmem.CreateCompatibleDC(&dc);
    CSelectObject sobmp(&dcmem, &m_Dimmed);

    // Apply the dimming overlay
    BLENDFUNCTION blendFunc;
    blendFunc.BlendOp = AC_SRC_OVER;
    blendFunc.BlendFlags = 0;
    blendFunc.SourceConstantAlpha = 175;
    blendFunc.AlphaFormat = 0;
    dcmem.FillSolidRect(CRect(0, 0, m_DimmedSize.cx, m_DimmedSize.cy), RGB(0, 0, 0));
    dcmem.AlphaBlend(0, 0, m_DimmedSize.cx, m_DimmedSize.cy, &dc,
        0, 0, m_DimmedSize.cx, m_DimmedSize.cy, blendFunc);
}

void CTreeMapView::EmptyView()
{
    if (m_Bitmap.m_hObject != nullptr)
    {
        m_Bitmap.DeleteObject();
    }

    if (m_Dimmed.m_hObject != nullptr)
    {
        m_Dimmed.DeleteObject();
    }
}

void CTreeMapView::OnSetFocus(CWnd* /*pOldWnd*/)
{
    CMainFrame::Get()->GetFileTreeView()->SetFocus();
}

void CTreeMapView::OnUpdate(CView* pSender, const LPARAM lHint, CObject* pHint)
{
    if (!GetDocument()->IsRootDone())
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

std::wstring CTreeMapView::GetTreeMapHoverPath()
{
    CPoint point;
    GetCursorPos(&point);
    ScreenToClient(&point);

    CRect rc;
    GetClientRect(rc);

    if (!rc.PtInRect(point))
    {
        m_PaneTextOverride = {};
    }

    return m_PaneTextOverride;
}

void CTreeMapView::OnContextMenu(CWnd* /*pWnd*/, const CPoint point)
{
    const CItem* root = GetDocument()->GetRootItem();
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
    if (GetDocument()->IsRootDone() && IsDrawn())
    {
        const auto item = static_cast<const CItem*>(m_TreeMap.FindItemByPoint(GetDocument()->GetZoomItem(), point));
        if (item != nullptr)
        {
            m_PaneTextOverride = item->GetPath();
            CMainFrame::Get()->UpdatePaneText();
        }
    }
}
