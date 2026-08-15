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
class CFlameGraphView final : public MessageTarget<CFlameGraphView, CGraphView>
{
public:
    CFlameGraphView() = default;

    ~CFlameGraphView() override = default;

protected:
    const wchar_t* GetWindowClassName() const override
    {
        return L"WinDirStatFlameGraphClass";
    }
    void DrawEmptyPlaceholder(CDC* pDC, const CRect& rect) override;
    bool PrepareDrawing(CDC* pDC, CRect& rect) override;
    void RenderVisualization(CDC* pDC, CRect rect) override;

    void DrawHighlightExtension(CDC* pdc) override;
    void DrawSelection(CDC* pdc) override;
    void HighlightSelectedItem(CDC* pdc, const CItem* item, bool single) const;

    CItem* FindItemAtPoint(CPoint point) override;
    bool HasValidLayout() const override;
    void ClearVisualizationLayout() override;
    void OnViewEmptied() override;
    void OnSuspending() override { m_forceScrollBarVisible = false; }
    void OnBeforeSizeChanged() override { if (!m_updatingScrollBar) m_forceScrollBarVisible = false; }
    void OnInputStateReset() override { m_scrollWheelDeltaRemainder = 0; }
    void OnRenderCacheTrimmed() override;
    bool CanReuseVisualizationLayout(MODEL_CHANGE change) const override;
    void OnVisualizationChanged(MODEL_CHANGE change) override;

    void DiscardBase(bool invalidateFullHeight);
    void RenderViewport(CDC* pDC, CRect clip) const;
    bool ScrollCachedViewport(int oldPosition);
    void SetScrollPosition(int position);
    void UpdateScrollBar(int fullHeight, int pageHeight);
    bool EnsureFullHeightForInput();
    int ComputeRowHeight(CDC* pDC) const;
    int ComputeFlameFullHeight(int width);

    bool m_updatingScrollBar = false;
    bool m_forceScrollBarVisible = false;
    int m_rowHeight = CFlameGraph::ROW_HEIGHT;
    int m_scrollPos = 0;
    int m_scrollWheelDeltaRemainder = 0;
    int m_fullHeight = 0;
    CFlameGraph m_flameGraph;

public:
    static std::span<const RouteEntry> Routes();

protected:
    bool OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    void OnVScroll(UINT nSBCode, UINT nPos, CWnd* scrollBar);
};

inline std::span<const RouteEntry> CFlameGraphView::Routes()
{
    using ThisClass = CFlameGraphView;
    static constexpr std::array entries
    {
        Route::Window<&ThisClass::OnMouseWheel>(WM_MOUSEWHEEL),
        Route::Window<&ThisClass::OnVScroll>(WM_VSCROLL),
    };
    return entries;
}
