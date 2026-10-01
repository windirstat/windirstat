// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "GraphView.h"

// Stable splitter pane that owns the renderer-specific visualization windows.
class CVisualizationPane final : public MessageTarget<CVisualizationPane, CWinDirStatPane>
{
public:
    CVisualizationPane() = default;
    ~CVisualizationPane() override;

    bool PreCreateWindow(CREATESTRUCT& cs) override;
    void OnDraw(CDC* pDC) override;

GraphPane GetActivePaneType() const { return m_activePane; }
    void SelectPane(GraphPane pane);
    void ShowVisualization(bool show);
    bool IsVisualizationShown() const { return m_showVisualization; }
    CGraphView* GetActiveView() const { return m_views[std::to_underlying(m_activePane)].get(); }

    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;
    HoverInfo GetHoverInfo() const override;
    void SuspendRecalculationDrawing(bool suspend) override;

protected:
    std::array<std::unique_ptr<CGraphView>, 3> m_views{};
    GraphPane m_activePane = GraphPane::TreeMap;
    bool m_showVisualization = true;
    unsigned int m_drawingSuspensionCount = 0;

public:
    static std::span<const RouteEntry> Routes();

protected:
    int OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnSetFocus(WindowRef pOldWnd);
    void OnSize(UINT nType, int cx, int cy);
};

inline std::span<const RouteEntry> CVisualizationPane::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnCreate>(WM_CREATE),
        Route::Window<&OnSetFocus>(WM_SETFOCUS),
        Route::Window<&OnSize>(WM_SIZE),
    };
    return entries;
}
