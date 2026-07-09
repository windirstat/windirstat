// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
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

//
// CFlameGraph. Creates a flame graph (icicle plot) visualization.
// In this layout, the root spans the full width at the top,
// each depth level occupies a horizontal strip, and items are
// sized proportionally to their size relative to their parent.
//
class CFlameGraph final
{
public:
    static constexpr int ROW_HEIGHT = 18;
    static constexpr COLORREF BACKGROUND_COLOR = RGB(15, 15, 15);

    CFlameGraph() = default;

    // Create and draw a flame graph
    void DrawFlameGraph(CDC* pdc, CRect rc, CItem* root);

    // In the resulting layout, find the item below a given coordinate.
    CItem* FindItemByPoint(CItem* item, CPoint point) const;

    // Compute the maximum depth of the tree
    static int ComputeMaxDepth(CItem* item, int depth = 0);

    // Active / hover item for visual state
    void SetActiveItem(const CItem* item) { m_activeItem = item; }
    void SetHoverItem(const CItem* item) { m_hoverItem = item; }
    const CItem* GetActiveItem() const { return m_activeItem; }
    const CItem* GetHoverItem() const { return m_hoverItem; }
    const std::vector<CItem*>& GetBreadcrumbs() const { return m_breadcrumbs; }
    int GetBreadcrumbHeight() const { return m_breadcrumbHeight; }

protected:
    // Walk the tree assigning rectangles (icicle layout)
    void LayoutItem(CItem* item, const CRect& rc, int depth, int rowHeight, bool isRoot);

    // Render all items (fill and label)
    void RenderAll(CDC* pdc, CItem* root, const CPoint& offset) const;

    // Check if test is a descendant of ancestor
    static bool IsDescendantOf(const CItem* test, const CItem* ancestor);

    CRect m_renderArea;
    const CItem* m_activeItem = nullptr;
    const CItem* m_hoverItem = nullptr;
    std::vector<CItem*> m_breadcrumbs;
    int m_breadcrumbHeight = 0;
};
