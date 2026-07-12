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

#pragma once

#include "pch.h"
#include "WinDirStatPane.h"
#include "FlameGraph.h"

class CWinDirStatModel;
class CItem;

//
// CFlameGraphView. The flame graph (icicle plot) window.
// A standalone pane that parallels CTreeMapView.
//
class CFlameGraphView final : public CWinDirStatPane
{
protected:
    DECLARE_DYNCREATE(CFlameGraphView)

public:
    CFlameGraphView() = default;

    ~CFlameGraphView() override = default;

    void SuspendRecalculationDrawing(bool suspend) override;
    bool IsShowTreeMap() const;
    void ShowTreeMap(bool show);
    void DrawEmptyView();
    HoverInfo GetHoverInfo() const override;

protected:
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;
    void OnDraw(CDC* pDC) override;
    bool IsDrawn() const;
    void Inactivate();
    void EmptyView();
    void DrawEmptyView(CDC* pDC);

    void DrawHighlights(CDC* pdc);
    void DrawHighlightExtension(CDC* pdc);
    void DrawSelection(CDC* pdc) const;
    void HighlightSelectedItem(CDC* pdc, const CItem* item, bool single) const;
    void RenderHighlightRectangle(CDC* pdc, CRect& rc) const;

    CItem* ResolveItemAtPoint(CPoint point, bool isScreenCoords = false);
    void DrillDown(CItem* item);
    void ClearHover();
    void SetHoverItem(const CItem* item);
    void InvalidateItem(const CItem* item);
    void DiscardBase(bool invalidateFullHeight);
    void UpdateScrollBar(int fullHeight, int pageHeight);
    bool EnsureFullHeightForInput();
    int ComputeRowHeight(CDC* pDC) const;
    int ComputeFlameFullHeight(int width) const;

    std::wstring m_paneTextOverride;
    ULONGLONG m_paneSizeOverride = 0;
    bool m_drawingSuspended = false;
    bool m_showTreeMap = true;
    bool m_trackingMouse = false;
    bool m_updatingScrollBar = false;
    bool m_forceScrollBarVisible = false;
    const CItem* m_hoverItem = nullptr;
    int m_rowHeight = CFlameGraph::ROW_HEIGHT;
    int m_scrollPos = 0;
    int m_wheelDeltaRemainder = 0;
    int m_fullHeight = 0;
    CSize m_size{ 0, 0 };
    CFlameGraph m_flameGraph;
    CBitmap m_bitmap;
    CSize m_dimmedSize{ 0, 0 };
    CBitmap m_dimmed;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
};
