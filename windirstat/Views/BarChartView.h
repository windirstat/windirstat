// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "GraphView.h"
#include "BarChart.h"

// Drill-down view for the vertical bar chart of the zoomed item's children.
class CBarChartView final : public CGraphView
{
public:
    CBarChartView() = default;
    ~CBarChartView() override = default;

protected:
    const wchar_t* GetWindowClassName() const override
    {
        return L"WinDirStatBarChartClass";
    }
    void DrawEmptyPlaceholder(CDC* pDC, const CRect& rect) override;
    void RenderVisualization(CDC* pDC, CRect rect) override;
    void DrawHighlightExtension(CDC* pDC) override;
    void DrawSelection(CDC* pDC) override;
    void DrawHover(CDC* pDC) override;
    CItem* FindItemAtPoint(CPoint point) override;
    void ClearVisualizationLayout() override;
    void OnRenderCacheTrimmed() override;
    bool UpdateHoverDetails(const CItem* item, bool itemChanged) override;
    bool CanReuseVisualizationLayout(MODEL_CHANGE change) const override;
    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;

    void UpdateBarToolTip(const CItem* item);
    void ClearExtensionHighlightCache();

    CItem* m_barRoot = nullptr;   // directory selected elsewhere; null follows the zoom item
    CBarChart m_chart;
    CToolTipCtrl m_toolTip;
    std::wstring m_toolTipText;
    std::vector<const CItem*> m_extensionOutlineItems;
    std::vector<const CItem*> m_selectionOutlineItems;
    std::wstring m_cachedHighlightExtension;
    bool m_cachedHighlightUnregistered = false;
    bool m_extensionOutlineItemsValid = false;
};
