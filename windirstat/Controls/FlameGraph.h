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
    // Logical row height at 96 DPI. The view scales this before drawing.
    static constexpr int ROW_HEIGHT = 18;
    static constexpr COLORREF BACKGROUND_COLOR = RGB(15, 15, 15);

    // Create and draw a flame graph. Logical geometry is reused when the root,
    // render rectangle, and DPI-scaled row height have not changed.
    void DrawFlameGraph(CDC* pdc, CRect rc, const CItem* root, int rowHeight);

    // In the resulting layout, find the item below a given coordinate.
    CItem* FindItemByPoint(CItem* item, CPoint point) const;

    // Access the most recently computed flame-graph layout without using the
    // rectangle cache owned by the treemap projection.
    bool TryGetItemRectangle(const CItem* item, CRect& rectangle) const;
    void ClearLayout();

    // Redraw one laid-out item with the hover treatment. The offset
    // translates logical layout coordinates to the destination DC.
    void DrawHoverItem(CDC* pdc, const CItem* item, CPoint offset) const;

    // Compute the maximum depth that can actually receive pixels at the
    // supplied width. This uses the same cumulative rounding as layout.
    static int ComputeVisibleMaxDepth(const CItem* item, int width, int depth = 0);

    // Visit only laid-out items intersecting a destination-space clip. The
    // callback receives the item and its full destination rectangle. Iteration
    // is allocation-free and uses row and horizontal indexes.
    template<typename Visitor>
        requires std::invocable<Visitor&, const CItem*, const CRect&>
    void VisitItemsIntersecting(const CRect clip, const CPoint offset, Visitor&& visitor) const
    {
        VisitRowItems(clip, offset, [&visitor](const auto& entry, const CRect& rectangle)
        {
            std::invoke(visitor, entry.item, rectangle);
        });
    }

private:
    struct LayoutEntry
    {
        CRect rectangle;
        int depth = 0;
        bool breadcrumb = false;
        std::size_t firstChild = 0;
        std::size_t childCount = 0;
    };

    struct ChildSpan
    {
        CItem* item = nullptr;
        LONG left = 0;
        LONG right = 0;
    };

    struct LaidOutChild
    {
        CItem* item = nullptr;
        LONG right = 0;
    };

    struct RowItem
    {
        const CItem* item = nullptr;
        CRect rectangle;
        int depth = 0;
        bool breadcrumb = false;
    };

    // Build the logical layout independently from viewport rendering.
    void BuildLayout(const CItem* root, CRect rc, int rowHeight);
    void LayoutItem(const CItem* item, const CRect& rc, int depth);
    LayoutEntry& AddLayoutEntry(const CItem* item, CRect rectangle, int depth,
        bool breadcrumb);

    // Allocate positive-width child spans with cumulative rounded boundaries.
    static int GetDrawableChildCount(const CItem* item);
    static void ComputeChildSpans(const CItem* item, LONG left, LONG right,
        std::vector<ChildSpan>& spans);

    void RenderLayout(CDC* pdc) const;
    void RenderItem(CDC* pdc, const CItem* item, const CRect& rectangle,
        int depth, bool hover) const;
    void RenderBreadcrumb(CDC* pdc, const CItem* item, const CRect& rectangle,
        int depth, bool hover) const;
    void RenderLabel(CDC* pdc, const CItem* item, const CRect& rc,
        COLORREF color) const;

    template<typename Visitor>
        requires std::invocable<Visitor&, const RowItem&, const CRect&>
    void VisitRowItems(CRect clip, const CPoint offset, Visitor&& visitor) const
    {
        clip.OffsetRect(-offset.x, -offset.y);
        CRect logicalClip;
        if (!logicalClip.IntersectRect(clip, m_renderArea) || m_rows.empty()) return;

        const auto firstRow = static_cast<std::size_t>(
            (logicalClip.top - m_renderArea.top) / m_rowHeight);
        const auto lastRow = std::min(m_rows.size() - 1, static_cast<std::size_t>(
            (logicalClip.bottom - 1 - m_renderArea.top) / m_rowHeight));

        for (std::size_t rowIndex = firstRow; rowIndex <= lastRow; rowIndex++)
        {
            const auto& row = m_rows[rowIndex];
            auto entry = std::ranges::upper_bound(row, logicalClip.left,
                std::ranges::less{}, [](const RowItem& item) {
                    return item.rectangle.right;
                });
            for (; entry != row.end() && entry->rectangle.left < logicalClip.right; ++entry)
            {
                CRect rectangle = entry->rectangle;
                rectangle.OffsetRect(offset);
                std::invoke(visitor, *entry, rectangle);
            }
        }
    }

    const CItem* m_layoutRoot = nullptr;
    CRect m_renderArea;
    int m_rowHeight = ROW_HEIGHT;
    int m_minLabelWidth = 40;
    int m_minLabelHeight = 14;
    int m_itemInset = 1;
    int m_textInsetX = 3;
    int m_textInsetY = 1;
    int m_borderThreshold = 4;
    std::unordered_map<const CItem*, LayoutEntry> m_layout;
    std::vector<LaidOutChild> m_laidOutChildren;
    std::vector<std::vector<RowItem>> m_rows;
    std::vector<CItem*> m_breadcrumbs;
};
