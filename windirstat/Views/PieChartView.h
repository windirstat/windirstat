// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "GraphView.h"
#include "PieChart.h"

// Drill-down view for the 3D pie chart of the zoomed item's children.
class CPieChartView final : public CGraphView
{
public:
    CPieChartView() = default;
    ~CPieChartView() override = default;

    // Returns the directory drilled into and goes back to following the zoom item.
    CItem* ReleaseRoot() noexcept { return std::exchange(m_pieRoot, nullptr); }

protected:
    const wchar_t* GetWindowClassName() const override
    {
        return L"WinDirStatPieChartClass";
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

    void UpdateSliceToolTip(const CItem* item);

    CItem* m_pieRoot = nullptr;   // directory selected elsewhere; null follows the zoom item
    CPieChart m_pie;
    CToolTipCtrl m_toolTip;
    std::wstring m_toolTipText;
    std::vector<const CItem*> m_extensionOutlineItems;
    std::vector<const CItem*> m_selectionOutlineItems;
    std::wstring m_cachedHighlightExtension;
    ULONGLONG m_hitRemainderSize = 0;
    bool m_hoveringRemainder = false;
    bool m_cachedHighlightUnregistered = false;
    bool m_extensionOutlineItemsValid = false;

    void ClearExtensionHighlightCache();
};
