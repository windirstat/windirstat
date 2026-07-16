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
#include "GraphView.h"
#include "FlameGraph.h"

class CWinDirStatModel;
class CItem;

//
// CFlameGraphView. The flame graph (icicle plot) window.
// A standalone pane that parallels CTreeMapView.
//
class CFlameGraphView final : public CGraphView
{
protected:
    DECLARE_DYNCREATE(CFlameGraphView)

public:
    CFlameGraphView() = default;

    ~CFlameGraphView() override = default;

protected:
    [[nodiscard]] const wchar_t* GetWindowClassName() const override
    {
        return L"WinDirStatFlameGraphClass";
    }
    void DrawEmptyPlaceholder(CDC* pDC, const CRect& rect) override;
    [[nodiscard]] bool PrepareDrawing(CDC* pDC, CRect& rect) override;
    void RenderVisualization(CDC* pDC, CRect rect) override;
    void DrawHoverOverlay(CDC* pDC) override;

    void DrawHighlightExtension(CDC* pdc) override;
    void DrawSelection(CDC* pdc) override;
    void HighlightSelectedItem(CDC* pdc, const CItem* item, bool single) const;

    [[nodiscard]] CItem* FindItemAtPoint(CPoint point) override;
    [[nodiscard]] bool HasValidLayout() const override;
    void ClearVisualizationLayout() override;
    void OnViewEmptied() override;
    void OnSuspending() override;
    void OnBeforeSizeChanged() override;
    void OnInputStateReset() override;
    void OnHoverItemChanged(const CItem* oldItem, const CItem* newItem) override;
    void OnVisualizationChanged(MODEL_CHANGE change) override;

    void InvalidateItem(const CItem* item);
    void DiscardBase(bool invalidateFullHeight);
    void UpdateScrollBar(int fullHeight, int pageHeight);
    bool EnsureFullHeightForInput();
    int ComputeRowHeight(CDC* pDC) const;
    int ComputeFlameFullHeight(int width) const;

    bool m_updatingScrollBar = false;
    bool m_forceScrollBarVisible = false;
    int m_rowHeight = CFlameGraph::ROW_HEIGHT;
    int m_scrollPos = 0;
    int m_scrollWheelDeltaRemainder = 0;
    int m_fullHeight = 0;
    CFlameGraph m_flameGraph;

    DECLARE_MESSAGE_MAP()
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
};
