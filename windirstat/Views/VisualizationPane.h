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
class CVisualizationPane final : public CWinDirStatPane
{
protected:
    DECLARE_DYNCREATE(CVisualizationPane)

    CVisualizationPane() = default;
    ~CVisualizationPane() override = default;

    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    void OnDraw(CDC* pDC) override;

public:
    [[nodiscard]] GraphPane GetActivePaneType() const { return m_activePane; }
    void SelectPane(GraphPane pane);
    void ShowVisualization(bool show);
    [[nodiscard]] bool IsVisualizationShown() const { return m_showVisualization; }
    [[nodiscard]] CGraphView* GetActiveView() const;

    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;
    HoverInfo GetHoverInfo() const override;
    void SuspendRecalculationDrawing(bool suspend) override;

protected:
    std::array<CGraphView*, 3> m_views{};
    GraphPane m_activePane = GraphPane::TreeMap;
    bool m_showVisualization = true;
    unsigned int m_drawingSuspensionCount = 0;

    DECLARE_MESSAGE_MAP()
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnSize(UINT nType, int cx, int cy);
};
