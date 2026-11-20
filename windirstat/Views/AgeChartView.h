// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "GraphView.h"
#include "AgeChart.h"

// Bar chart of the file ages below the selected folder (or the zoomed item when
// nothing is selected). It is redrawn whenever the selection moves to another folder.
class CAgeChartView final : public CGraphView
{
public:
    CAgeChartView() = default;
    ~CAgeChartView() override = default;

protected:
    const wchar_t* GetWindowClassName() const override
    {
        return L"WinDirStatAgeChartClass";
    }
    void DrawEmptyPlaceholder(CDC* pDC, const CRect& rect) override;
    void RenderVisualization(CDC* pDC, CRect rect) override;
    void DrawHighlightExtension(CDC*) override {}
    void DrawSelection(CDC*) override {}
    void DrawHover(CDC*) override {}
    CItem* FindItemAtPoint(CPoint) override { return nullptr; }
    void ClearVisualizationLayout() override { m_shownItem = nullptr; }
    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;

    CItem* GetTargetItem() const;

    CAgeChart m_chart;
    const CItem* m_shownItem = nullptr;
};
