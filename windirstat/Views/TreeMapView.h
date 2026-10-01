// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "GraphView.h"
#include "TreeMap.h"

class CWinDirStatModel;
class CItem;

//
// CTreeMapView. The treemap window.
//
class CTreeMapView final : public CGraphView
{
public:
    CTreeMapView() = default;

    ~CTreeMapView() override = default;

    const wchar_t* GetWindowClassName() const override
    {
        return L"WinDirStatTreeMapClass";
    }
    void DrawEmptyPlaceholder(CDC* pDC, const CRect& rect) override;
    bool CreateRenderBitmap(CDC* pDC, CSize size) override;
    void RenderVisualization(CDC* pDC, CRect rect) override;

    void DrawZoomFrame(CDC* pdc, CRect& rc) const;
    void DrawHighlightExtension(CDC* pdc) override;
    void DrawSelection(CDC* pdc) override;
    void DrawHover(CDC* pdc) override;

    void HighlightSelectedItem(CDC* pdc, const CItem* item, bool single, bool hover = false) const;
    CItem* FindItemAtPoint(CPoint point) override;
    bool HasValidLayout() const override;
    void ClearVisualizationLayout() override;
    void OnRenderCacheTrimmed() override;
    void DrillDown(CItem* item) override;
    std::span<const UINT> GetPersistentContextCommands() const override;

    static constexpr int ZoomFrameWidth = 4;

    CTreeMap m_treeMap;               // Treemap generator

};
