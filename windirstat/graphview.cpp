// graphview.cpp: Implementation of CGraphView
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2004 Bernhard Seifert
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
// Author: bseifert@users.sourceforge.net, bseifert@daccord.net
//
// Last modified: $Date$

#include "stdafx.h"
#include "windirstat.h"
#include "mainframe.h"
#include "dirstatdoc.h"
#include "dirstatview.h"
#include "item.h"

#include ".\graphview.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


IMPLEMENT_DYNCREATE(CGraphView, CView)

BEGIN_MESSAGE_MAP(CGraphView, CView)
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_SETFOCUS()
	ON_WM_CONTEXTMENU()
	ON_WM_MOUSEMOVE()
	ON_WM_DESTROY()
	ON_WM_TIMER()
	ON_COMMAND(ID_POPUP_CANCEL, OnPopupCancel)
END_MESSAGE_MAP()


CGraphView::CGraphView()
{
	m_recalculationSuspended= false;
	m_showTreemap= true;
	m_size.cx= m_size.cy= 0;
	m_dimmedSize.cx= m_dimmedSize.cy= 0;
	m_timer= 0;
}

CGraphView::~CGraphView()
{
}

void CGraphView::TreemapDrawingCallback()
{
	GetApp()->PeriodicalUpdateRamUsage();
}

void CGraphView::SuspendRecalculation(bool suspend)
{
	m_recalculationSuspended= suspend;
	if (!suspend)
		Invalidate();
}

bool CGraphView::IsShowTreemap()
{
	return m_showTreemap;
}

void CGraphView::ShowTreemap(bool show)
{
	m_showTreemap= show;
}

BOOL CGraphView::PreCreateWindow(CREATESTRUCT& cs)
{
	// We don't want a background brush
	VERIFY(CView::PreCreateWindow(cs)); // this registeres a wndclass
	
	WNDCLASS wc;
	VERIFY(GetClassInfo(AfxGetInstanceHandle(), cs.lpszClass, &wc));
	wc.hbrBackground= NULL;
	wc.lpszClassName= _T("windirstat_graphview_class");
	cs.lpszClass= (LPCTSTR)RegisterClass(&wc);
	
	return true;
}

void CGraphView::OnInitialUpdate()
{
	CView::OnInitialUpdate();
}

void CGraphView::DrawEmptyView()
{
	CClientDC dc(this);
	DrawEmptyView(&dc);
}

void CGraphView::DrawEmptyView(CDC *pDC)
{
	const COLORREF gray= RGB(160, 160, 160);

	Inactivate();

	CRect rc;
	GetClientRect(rc);

	if (m_dimmed.m_hObject == NULL)
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
			CRect r= rc;
			r.left= r.left + m_dimmedSize.cx;
			pDC->FillSolidRect(r, gray);
		}

		if (rc.Height() > m_dimmedSize.cy)
		{
			CRect r= rc;
			r.top= r.top + m_dimmedSize.cy;
			pDC->FillSolidRect(r, gray);
		}
	}
}

void CGraphView::OnDraw(CDC* pDC)
{
	CItem *root= GetDocument()->GetRootItem();
	if (root != NULL && root->IsDone())
	{
		if (m_recalculationSuspended || !m_showTreemap)
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
					DrawZoomFrame(&dcmem, rc);

				m_treemap.DrawTreemap(&dcmem, rc, GetDocument()->GetZoomItem(), GetOptions()->GetTreemapOptions());

				// Cause OnIdle() to be called once.
				PostAppMessage(GetCurrentThreadId(), WM_NULL, 0, 0);
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

void CGraphView::DrawZoomFrame(CDC *pdc, CRect& rc)
{
	const int w = 4;
	CRect r;
	
	r= rc;
	r.bottom= r.top + w;
	pdc->FillSolidRect(r, GetDocument()->GetZoomColor());

	r= rc;
	r.top= r.bottom - w;
	pdc->FillSolidRect(r, GetDocument()->GetZoomColor());

	r= rc;
	r.right= r.left + w;
	pdc->FillSolidRect(r, GetDocument()->GetZoomColor());

	r= rc;
	r.left= r.right - w;
	pdc->FillSolidRect(r, GetDocument()->GetZoomColor());

	rc.DeflateRect(w, w);
}

void CGraphView::DrawHighlights(CDC *pdc)
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

void CGraphView::DrawHighlightExtension(CDC *pdc)
{
	CWaitCursor wc;

	CPen pen(PS_SOLID, 1, GetOptions()->GetTreemapHighlightColor());
	CSelectObject sopen(pdc, &pen);
	CSelectStockObject sobrush(pdc, NULL_BRUSH);
	RecurseHighlightExtension(pdc, GetDocument()->GetZoomItem());
}

void CGraphView::RecurseHighlightExtension(CDC *pdc, const CItem *item)
{
	CRect rc = item->TmiGetRectangle();
	if (rc.Width() <= 0 || rc.Height() <= 0)
		return;

	GetApp()->PeriodicalUpdateRamUsage();

	if (item->TmiIsLeaf())
	{
		if (item->GetType() == IT_FILE 
		&& item->GetExtension().CompareNoCase(GetDocument()->GetHighlightExtension()) == 0)
		{
			RenderHighlightRectangle(pdc, rc);
		}
	}
	else
	{
		for (int i=0; i < item->TmiGetChildrenCount(); i++)
		{
			const CItem *child= item->GetChild(i);
			if (child->TmiGetSize() == 0)
				break;
			if (child->TmiGetRectangle().left == -1)
				break;
			RecurseHighlightExtension(pdc, child);
		}
	}
}

void CGraphView::DrawSelection(CDC *pdc)
{
	const CItem *item= GetDocument()->GetSelection();
	if (item == NULL)
		return;

	CRect rcClient;
	GetClientRect(rcClient);

	CRect rc= item->TmiGetRectangle();
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

	CSelectStockObject sobrush(pdc, NULL_BRUSH);

	CPen pen(PS_SOLID, 1, GetOptions()->GetTreemapHighlightColor());
	CSelectObject sopen(pdc, &pen);

	RenderHighlightRectangle(pdc, rc);
}

// A pen and the null brush must be selected.
//
void CGraphView::RenderHighlightRectangle(CDC *pdc, CRect& rc)
{
	ASSERT(rc.Width() >= 0);
	ASSERT(rc.Height() >= 0);

	// The documentation of CDC::Rectangle() says that the width
	// and height must be greater than 2. Experiment says that
	// it must be greater than 1. We follow the documentation.

	if (rc.Width() >= 7 && rc.Height() >= 7)
	{
		pdc->Rectangle(rc);		// w = 7
		rc.DeflateRect(1, 1);
		pdc->Rectangle(rc);		// w = 5
		rc.DeflateRect(1, 1);
		pdc->Rectangle(rc);		// w = 3
	}
	else
	{
		pdc->FillSolidRect(rc, GetOptions()->GetTreemapHighlightColor());
	}
}

#ifdef _DEBUG
void CGraphView::AssertValid() const
{
	CView::AssertValid();
}

void CGraphView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CDirstatDoc* CGraphView::GetDocument() // Nicht-Debugversion ist inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CDirstatDoc)));
	return (CDirstatDoc*)m_pDocument;
}
#endif //_DEBUG



void CGraphView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	CSize sz(cx, cy);
	if (sz != m_size)
	{
		Inactivate();
		m_size= sz;
	}
}

void CGraphView::OnLButtonDown(UINT nFlags, CPoint point)
{
	CItem *root= GetDocument()->GetRootItem();
	if (root != NULL && root->IsDone() && IsDrawn())
	{
		const CItem *item= (const CItem *)m_treemap.FindItemByPoint(GetDocument()->GetZoomItem(), point);
		if (item == NULL)
			return;

		GetDocument()->SetSelection(item);
		GetDocument()->UpdateAllViews(NULL, HINT_SHOWNEWSELECTION);
	}
	CView::OnLButtonDown(nFlags, point);
}

bool CGraphView::IsDrawn()
{
	return m_bitmap.m_hObject != NULL;
}

void CGraphView::Inactivate()
{
	if (m_bitmap.m_hObject != NULL)
	{
		// Move the old bitmap to m_dimmed
		m_dimmed.DeleteObject();
		m_dimmed.Attach(m_bitmap.Detach());
		m_dimmedSize= m_size;

		// Dimm m_inactive
		CClientDC dc(this);
		CDC dcmem;
		dcmem.CreateCompatibleDC(&dc);
		CSelectObject sobmp(&dcmem, &m_dimmed);
		for (int x=0; x < m_dimmedSize.cx; x+=2)
		for (int y=0; y < m_dimmedSize.cy; y+=2)
		{
			dcmem.SetPixel(x, y, RGB(100,100,100));
		}
	}
}

void CGraphView::EmptyView()
{
	if (m_bitmap.m_hObject != NULL)
		m_bitmap.DeleteObject();

	if (m_dimmed.m_hObject != NULL)
		m_dimmed.DeleteObject();
}

void CGraphView::OnSetFocus(CWnd* /*pOldWnd*/)
{
	GetMainFrame()->GetDirstatView()->SetFocus();
}

void CGraphView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	if (!GetDocument()->IsRootDone())
		Inactivate();

	switch (lHint)
	{
	case HINT_NEWROOT:
		EmptyView();
		CView::OnUpdate(pSender, lHint, pHint);
		break;

	case HINT_SELECTIONCHANGED:
	case HINT_SHOWNEWSELECTION:
	case HINT_SELECTIONSTYLECHANGED:
	case HINT_EXTENSIONSELECTIONCHANGED:
		CView::OnUpdate(pSender, lHint, pHint);
		break;

	case HINT_ZOOMCHANGED:
		Inactivate();
		CView::OnUpdate(pSender, lHint, pHint);
		break;

	case HINT_REDRAWWINDOW:
		RedrawWindow();
		break;

	case HINT_TREEMAPSTYLECHANGED:
		Inactivate();
		CView::OnUpdate(pSender, lHint, pHint);
		break;

	case 0:
		CView::OnUpdate(pSender, lHint, pHint);
		break;

	default:
		break;
	}
}

void CGraphView::OnContextMenu(CWnd* /*pWnd*/, CPoint ptscreen)
{
	CItem *root= GetDocument()->GetRootItem();
	if (root != NULL && root->IsDone())
	{
		CMenu menu;
		menu.LoadMenu(IDR_POPUPGRAPH);
		CMenu *sub= menu.GetSubMenu(0);
		sub->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, ptscreen.x, ptscreen.y, AfxGetMainWnd());
	}
}

void CGraphView::OnMouseMove(UINT /*nFlags*/, CPoint point)
{
	CItem *root= GetDocument()->GetRootItem();
	if (root != NULL && root->IsDone() && IsDrawn())
	{
		const CItem *item= (const CItem *)m_treemap.FindItemByPoint(GetDocument()->GetZoomItem(), point);
		if (item != NULL)
			GetMainFrame()->SetMessageText(item->GetPath());

	}
	if (m_timer == 0)
		m_timer= SetTimer(4711, 100, NULL);
}

void CGraphView::OnDestroy()
{
	if (m_timer != NULL)
		KillTimer(m_timer);
	m_timer= 0;

	CView::OnDestroy();
}

void CGraphView::OnTimer(UINT /*nIDEvent*/)
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
		m_timer= 0;
	}
}

void CGraphView::OnPopupCancel()
{
}

// $Log$
// Revision 1.6  2004/11/09 22:31:59  assarbad
// - Removed obsolete code lines
//
// Revision 1.5  2004/11/05 16:53:07  assarbad
// Added Date and History tag where appropriate.
//
