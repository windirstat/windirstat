// GraphView.cpp - Implementation of CGraphView
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "stdafx.h"
#include "WinDirStat.h"
#include "MainFrame.h"
#include "DirStatDoc.h"
#include "DirStatView.h"
#include "Item.h"
#include "SelectObject.h"

#include "GraphView.h"

IMPLEMENT_DYNCREATE(CGraphView, CView)

BEGIN_MESSAGE_MAP(CGraphView, CView)
    ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
    ON_WM_SETFOCUS()
    ON_WM_CONTEXTMENU()
    ON_WM_MOUSEMOVE()
    ON_WM_DESTROY()
    ON_WM_TIMER()
END_MESSAGE_MAP()

CGraphView::CGraphView()
{
    m_recalculationDrawingSuspended = false;
    m_showTreemap            = true;
    m_size.cx                = m_size.cy       = 0;
    m_dimmedSize.cx          = m_dimmedSize.cy = 0;
    m_timer                  = 0;
}

void CGraphView::SuspendRecalculationDrawing(bool suspend)
{
    m_recalculationDrawingSuspended = suspend;
    if (!suspend)
    {
        Invalidate();
    }
}

bool CGraphView::IsShowTreemap() const
{
    return m_showTreemap;
}

void CGraphView::ShowTreemap(bool show)
{
    m_showTreemap = show;
}

BOOL CGraphView::PreCreateWindow(CREATESTRUCT& cs)
{
    // We don't want a background brush
    VERIFY(CView::PreCreateWindow(cs)); // this registers a wndclass

    WNDCLASS wc;
    VERIFY(GetClassInfo(AfxGetInstanceHandle(), cs.lpszClass, &wc));
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"windirstat_graphview_class-{E0BE4F6F-3904-4c99-A3D4-2F11DE629740}";
    cs.lpszClass     = (LPCWSTR)::RegisterClass(&wc);

    return true;
}

void CGraphView::DrawEmptyView()
{
    CClientDC dc(this);
    DrawEmptyView(&dc);
}

void CGraphView::DrawEmptyView(CDC* pDC)
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

void CGraphView::OnDraw(CDC* pDC)
{
    const CItem* root = GetDocument()->GetRootItem();
    if (root != nullptr && root->IsDone())
    {
        if (m_recalculationDrawingSuspended || !m_showTreemap)
        {
            // TODO: draw something interesting, e.g. outline of the first level.
            DrawEmptyView(pDC);
        }
        else
        {
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

                if (GetDocument()->IsZoomed())
                {
                    DrawZoomFrame(&dcmem, rc);
                }

                m_treemap.DrawTreemap(&dcmem, rc, GetDocument()->GetZoomItem(), &COptions::TreemapOptions);

                // Cause OnIdle() to be called once.
                ::PostThreadMessage(::GetCurrentThreadId(), WM_NULL, 0, 0);
            }

            CSelectObject sobmp2(&dcmem, &m_bitmap);

            pDC->BitBlt(0, 0, m_size.cx, m_size.cy, &dcmem, 0, 0, SRCCOPY);

            DrawHighlights(pDC);
        }
    }
    else
    {
        DrawEmptyView(pDC);
    }
}

void CGraphView::DrawZoomFrame(CDC* pdc, CRect& rc)
{
    constexpr int w = 4;

    CRect r  = rc;
    r.bottom = r.top + w;
    pdc->FillSolidRect(r, GetDocument()->GetZoomColor());

    r     = rc;
    r.top = r.bottom - w;
    pdc->FillSolidRect(r, GetDocument()->GetZoomColor());

    r       = rc;
    r.right = r.left + w;
    pdc->FillSolidRect(r, GetDocument()->GetZoomColor());

    r      = rc;
    r.left = r.right - w;
    pdc->FillSolidRect(r, GetDocument()->GetZoomColor());

    rc.DeflateRect(w, w);
}

void CGraphView::DrawHighlights(CDC* pdc)
{
    switch (GetMainFrame()->GetLogicalFocus())
    {
    case LF_DIRECTORYLIST:
        DrawSelection(pdc);
        break;
    case LF_EXTENSIONLIST:
        DrawHighlightExtension(pdc);
        break;
    }
}

void CGraphView::DrawHighlightExtension(CDC* pdc)
{
    CWaitCursor wc;

    CPen pen(PS_SOLID, 1, COptions::TreeMapHighlightColor);
    CSelectObject sopen(pdc, &pen);
    CSelectStockObject sobrush(pdc, NULL_BRUSH);
    RecurseHighlightExtension(pdc, GetDocument()->GetZoomItem());
}

void CGraphView::RecurseHighlightExtension(CDC* pdc, const CItem* item)
{
    CRect rc(item->TmiGetRectangle());
    if (rc.Width() <= 0 || rc.Height() <= 0)
    {
        return;
    }

    if (item->TmiIsLeaf())
    {
        if (item->IsType(IT_FILE) && item->GetExtension().CompareNoCase(GetDocument()->GetHighlightExtension()) == 0)
        {
            RenderHighlightRectangle(pdc, rc);
        }
    }
    else
    {
        for (const auto& child : item->GetChildren())
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
}

void CGraphView::DrawSelection(CDC* pdc)
{
    CSelectStockObject sobrush(pdc, NULL_BRUSH);

    CPen pen(PS_SOLID, 1, COptions::TreeMapHighlightColor);
    CSelectObject sopen(pdc, &pen);

    const auto& items = CTreeListControl::GetTheTreeListControl()->GetAllSelected<CItem>();
    for (const auto& item : items)
    {
        HighlightSelectedItem(pdc, item, items.size() == 1);
    }
}

// A pen and the null brush must be selected.
// Draws the highlight rectangle of item. If single, the rectangle is slightly
// bigger than the item rect, else it fits inside.
//
void CGraphView::HighlightSelectedItem(CDC* pdc, const CItem* item, bool single)
{
    CRect rc(item->TmiGetRectangle());

    if (single)
    {
        CRect rcClient;
        GetClientRect(rcClient);

        if (m_treemap.GetOptions().grid)
        {
            rc.right++;
            rc.bottom++;
        }

        if (rcClient.left < rc.left)
            rc.left--;
        if (rcClient.top < rc.top)
            rc.top--;
        if (rc.right < rcClient.right)
            rc.right++;
        if (rc.bottom < rcClient.bottom)
            rc.bottom++;
    }

    if (rc.Width() <= 0 || rc.Height() <= 0)
    {
        return;
    }

    RenderHighlightRectangle(pdc, rc);
}

// A pen and the null brush must be selected.
//
void CGraphView::RenderHighlightRectangle(CDC* pdc, CRect& rc)
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

void CGraphView::OnSize(UINT nType, int cx, int cy)
{
    CView::OnSize(nType, cx, cy);
    const CSize sz(cx, cy);
    if (sz != m_size)
    {
        Inactivate();
        m_size = sz;
    }
}

void CGraphView::OnLButtonDown(UINT nFlags, CPoint point)
{
    const CItem* root = GetDocument()->GetRootItem();
    if (root != nullptr && root->IsDone() && IsDrawn())
    {
        const auto item = static_cast<CItem*>(m_treemap.FindItemByPoint(GetDocument()->GetZoomItem(), point));
        if (item == nullptr)
        {
            return;
        }

        GetDocument()->UpdateAllViews(this, HINT_SELECTIONACTION, reinterpret_cast<CObject*>(item));
    }
    CView::OnLButtonDown(nFlags, point);
}

bool CGraphView::IsDrawn() const
{
    return m_bitmap.m_hObject != nullptr;
}

void CGraphView::Inactivate()
{
    if (m_bitmap.m_hObject != nullptr)
    {
        // Move the old bitmap to m_dimmed
        m_dimmed.DeleteObject();
        m_dimmed.Attach(m_bitmap.Detach());
        m_dimmedSize = m_size;

        // Dim m_inactive
        CClientDC dc(this);
        CDC dcmem;
        dcmem.CreateCompatibleDC(&dc);
        CSelectObject sobmp(&dcmem, &m_dimmed);
        for (int x = 0; x < m_dimmedSize.cx; x += 2)
            for (int y = 0; y < m_dimmedSize.cy; y += 2)
            {
                dcmem.SetPixel(x, y, RGB(100, 100, 100));
            }
    }
}

void CGraphView::EmptyView()
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

void CGraphView::OnSetFocus(CWnd* /*pOldWnd*/)
{
    GetMainFrame()->GetDirStatView()->SetFocus();
}

void CGraphView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
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
        }
        break;

    case HINT_SELECTIONACTION:
    case HINT_SELECTIONREFRESH:
    case HINT_SELECTIONSTYLECHANGED:
    case HINT_EXTENSIONSELECTIONCHANGED:
        {
            CView::OnUpdate(pSender, lHint, pHint);
        }
        break;

    case HINT_ZOOMCHANGED:
        {
            Inactivate();
            CView::OnUpdate(pSender, lHint, pHint);
        }
        break;

    case HINT_TREEMAPSTYLECHANGED:
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

void CGraphView::OnContextMenu(CWnd* /*pWnd*/, CPoint ptscreen)
{
    const CItem* root = GetDocument()->GetRootItem();
    if (root != nullptr && root->IsDone())
    {
        CMenu menu;
        menu.LoadMenu(IDR_POPUPGRAPH);
        CMenu* sub = menu.GetSubMenu(0);
        sub->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, ptscreen.x, ptscreen.y, AfxGetMainWnd());
    }
}

void CGraphView::OnMouseMove(UINT /*nFlags*/, CPoint point)
{
    if (GetDocument()->IsRootDone() && IsDrawn())
    {
        const auto item = static_cast<const CItem*>(m_treemap.FindItemByPoint(GetDocument()->GetZoomItem(), point));
        if (item != nullptr)
        {
            GetMainFrame()->SetMessageText(item->GetPath());
        }
    }
    if (m_timer == 0)
    {
        m_timer = SetTimer(ID_WDS_CONTROL, 100, nullptr);
    }
}

void CGraphView::OnDestroy()
{
    if (m_timer != NULL)
    {
        KillTimer(m_timer);
    }
    m_timer = 0;

    CView::OnDestroy();
}

void CGraphView::OnTimer(UINT_PTR /*nIDEvent*/)
{
    CPoint point;
    GetCursorPos(&point);
    ScreenToClient(&point);

    CRect rc;
    GetClientRect(rc);

    if (!rc.PtInRect(point))
    {
        GetMainFrame()->SetSelectionMessageText();
        KillTimer(m_timer);
        m_timer = 0;
    }
}
