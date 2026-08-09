// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
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

// Stable splitter pane that owns the renderer-specific visualization windows.
class CVisualizationPane final : public MessageTarget<CVisualizationPane, CWinDirStatPane>
{
public:
    CVisualizationPane() = default;
    ~CVisualizationPane() override = default;

    bool PreCreateWindow(CREATESTRUCT& cs) override;
    void OnDraw(CDC* pDC) override;

GraphPane GetActivePaneType() const { return m_activePane; }
    void SelectPane(GraphPane pane);
    void ShowVisualization(bool show);
    bool IsVisualizationShown() const { return m_showVisualization; }
    CGraphView* GetActiveView() const;

    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;
    HoverInfo GetHoverInfo() const override;
    void SuspendRecalculationDrawing(bool suspend) override;

protected:
    std::array<CGraphView*, 3> m_views{};
    GraphPane m_activePane = GraphPane::TreeMap;
    bool m_showVisualization = true;
    unsigned int m_drawingSuspensionCount = 0;

public:
    static std::span<const RouteEntry> Routes();

protected:
    int OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnSetFocus(CWnd* pOldWnd);
    void OnSize(UINT nType, int cx, int cy);
};

inline std::span<const RouteEntry> CVisualizationPane::Routes()
{
    using ThisClass = CVisualizationPane;
    static constexpr std::array entries
    {
        Route::Window<&ThisClass::OnCreate>(WM_CREATE),
        Route::Window<&ThisClass::OnSetFocus>(WM_SETFOCUS),
        Route::Window<&ThisClass::OnSize>(WM_SIZE),
    };
    return entries;
}
