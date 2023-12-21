// graphview.h - Declaration of CGraphView (the Treemap view)
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

#pragma once

#include "treemap.h"

class CDirstatDoc;
class CItem;


//
// CGraphView. The treemap window.
//
class CGraphView : public CView, public CTreemap::Callback
{
protected:
    CGraphView();
    DECLARE_DYNCREATE(CGraphView)

public:
    ~CGraphView() override = default;

    // CTreemap::Callback
    void TreemapDrawingCallback() override;

    CDirstatDoc* GetDocument() const;
    void SuspendRecalculation(bool suspend);
    bool IsShowTreemap() const;
    void ShowTreemap(bool show);
    void DrawEmptyView();

protected:
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    void OnInitialUpdate() override;
    void OnDraw(CDC* pDC) override;
    bool IsDrawn();
    void Inactivate();
    void EmptyView();
    void DrawEmptyView(CDC* pDC);

    void DrawZoomFrame(CDC* pdc, CRect& rc);
    void DrawHighlights(CDC* pdc);

    void DrawHighlightExtension(CDC* pdc);
    void RecurseHighlightExtension(CDC* pdc, const CItem* item);

    void DrawSelection(CDC* pdc);

    void HighlightSelectedItem(CDC* pdc, const CItem* item, bool single);
    void RenderHighlightRectangle(CDC* pdc, CRect& rc);

    bool m_recalculationSuspended; // True while the user is resizing the window.
    bool m_showTreemap;            // False, if the user switched off the treemap (by F9).
    CSize m_size;                  // Current size of view
    CTreemap m_treemap;            // Treemap generator
    CBitmap m_bitmap;              // Cached view. If m_hObject is NULL, the view must be recalculated.
    CSize m_dimmedSize;            // Size of bitmap m_dimmed
    CBitmap m_dimmed;              // Dimmed view. Used during refresh to avoid the ooops-effect.
    UINT_PTR m_timer;              // We need a timer to realize when the mouse left our window.

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override;
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnDestroy();
    afx_msg void OnTimer(UINT_PTR nIDEvent);

public:
#ifdef _DEBUG
    void AssertValid() const override;
    void Dump(CDumpContext& dc) const override;
#endif
    afx_msg void OnPopupCancel();
};

#ifndef _DEBUG  // Debugversion in graphview.cpp
inline CDirstatDoc* CGraphView::GetDocument() const
{
    return reinterpret_cast<CDirstatDoc*>(m_pDocument);
}
#endif
