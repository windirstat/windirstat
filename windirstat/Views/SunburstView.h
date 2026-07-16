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
#include "Sunburst.h"

// Drill-down view for the multi-layer Sunburst chart.
class CSunburstView final : public CGraphView
{
protected:
    DECLARE_DYNCREATE(CSunburstView)

public:
    CSunburstView() = default;
    ~CSunburstView() override = default;

protected:
    [[nodiscard]] const wchar_t* GetWindowClassName() const override
    {
        return L"WinDirStatSunburstClass";
    }
    void DrawEmptyPlaceholder(CDC* pDC, const CRect& rect) override;
    void RenderVisualization(CDC* pDC, CRect rect) override;
    void DrawHoverOverlay(CDC* pDC) override;
    void DrawHighlightExtension(CDC* pDC) override;
    void DrawSelection(CDC* pDC) override;
    [[nodiscard]] CItem* FindItemAtPoint(CPoint point) override;
    void ClearVisualizationLayout() override;
    void OnHoverItemChanged(const CItem* oldItem, const CItem* newItem) override;

    CSunburst m_sunburst;
    std::vector<const CItem*> m_outlineItems;

    DECLARE_MESSAGE_MAP()
};
