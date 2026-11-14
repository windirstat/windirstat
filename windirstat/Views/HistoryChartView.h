// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "GraphView.h"
#include "HistoryChart.h"

// Line chart of the stored scan history (size, allocated size and file count) of the
// selected folder (or the zoomed item when nothing is selected). It is redrawn whenever
// the selection moves to another folder.
class CHistoryChartView final : public CGraphView
{
public:
    CHistoryChartView() = default;
    ~CHistoryChartView() override = default;

protected:
    const wchar_t* GetWindowClassName() const override
    {
        return L"WinDirStatHistoryChartClass";
    }
    void DrawEmptyPlaceholder(CDC* pDC, const CRect& rect) override;
    void RenderVisualization(CDC* pDC, CRect rect) override;
    void DrawHighlightExtension(CDC*) override {}
    void DrawSelection(CDC*) override {}
    void DrawHover(CDC*) override {}
    CItem* FindItemAtPoint(CPoint) override { return nullptr; }
    void ClearVisualizationLayout() override { m_shownItem = nullptr; }
    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;

    void OnContextMenu(WindowRef pWnd, CPoint point) override;
    void OnLButtonDown(UINT nFlags, CPoint point) override;
    void OnLButtonDblClk(UINT nFlags, CPoint point) override;
    void OnMouseMove(UINT nFlags, CPoint point) override;
    bool OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) override;
    void Redraw() { Inactivate(); Invalidate(false); }
    CItem* GetTargetItem() const;

    void CopyChart(const CItem* item, const std::wstring& origin);
    void ExportChart(const CItem* item, const std::wstring& origin);
    void PrintChart(const CItem* item, const std::wstring& origin);

    CHistoryChart m_chart;
    const CItem* m_shownItem = nullptr;
    bool m_panning = false;
    bool m_dragThumb = false;
    int m_panX = 0;
};
