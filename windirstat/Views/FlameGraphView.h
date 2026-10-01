// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

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
    void DrawHover(CDC* pdc) override;
    void HighlightSelectedItem(CDC* pdc, const CItem* item, bool single, bool hover = false) const;

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
    void OnVScroll(UINT nSBCode, UINT nPos, WindowRef scrollBar);
};

inline std::span<const RouteEntry> CFlameGraphView::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnMouseWheel>(WM_MOUSEWHEEL),
        Route::Window<&OnVScroll>(WM_VSCROLL),
    };
    return entries;
}
