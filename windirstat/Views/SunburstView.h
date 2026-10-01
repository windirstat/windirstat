// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "GraphView.h"
#include "Sunburst.h"

// Drill-down view for the multi-layer Sunburst chart.
class CSunburstView final : public CGraphView
{
public:
    CSunburstView() = default;
    ~CSunburstView() override = default;

protected:
    const wchar_t* GetWindowClassName() const override
    {
        return L"WinDirStatSunburstClass";
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

    CSunburst m_sunburst;
    std::vector<const CItem*> m_extensionOutlineItems;
    std::vector<const CItem*> m_selectionOutlineItems;
    std::wstring m_cachedHighlightExtension;
    ULONGLONG m_hitRemainderSize = 0;
    bool m_hoveringRemainder = false;
    bool m_cachedHighlightUnregistered = false;
    bool m_extensionOutlineItemsValid = false;

    void ClearExtensionHighlightCache();

};
