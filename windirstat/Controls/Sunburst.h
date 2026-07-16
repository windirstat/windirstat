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

// Multi-layer radial chart with size-proportional sectors and indexed hit testing.
class CSunburst final
{
public:
    static constexpr COLORREF BACKGROUND_COLOR = RGB(15, 15, 15);

    void DrawSunburst(CDC* pdc, CRect rc, CItem* root, int maxDepth);
    void DrawHoverItem(CDC* pdc, const CItem* item) const;
    void DrawOutlineItems(CDC* pdc, std::span<const CItem* const> items,
        COLORREF color, float width = 2.0f) const;

    [[nodiscard]] CItem* FindItemByPoint(CPoint point) const;
    void ClearLayout();

    template<typename Visitor>
        requires std::invocable<Visitor&, const CItem*>
    void VisitItems(Visitor&& visitor) const
    {
        for (const LayoutEntry& entry : m_entries)
        {
            std::invoke(visitor, entry.item);
        }
    }

private:
    struct LayoutEntry
    {
        CItem* item = nullptr;
        double startAngle = 0.0;
        double sweepAngle = 0.0;
        double innerRadius = 0.0;
        double outerRadius = 0.0;
        int depth = 0;
        bool visualLeaf = true;
    };

    struct RingEntry
    {
        CItem* item = nullptr;
        double startAngle = 0.0;
        double endAngle = 0.0;
    };

    void BuildLayout(CItem* root, const CRect& rc, int maxDepth);
    void RenderLayout(CDC* pdc) const;
    void RenderEntry(Gdiplus::Graphics& graphics, const LayoutEntry& entry, bool hover,
        Gdiplus::SolidBrush& brush, Gdiplus::Pen& separator) const;
    void RenderLabel(Gdiplus::Graphics& graphics,
        const Gdiplus::FontFamily* fontFamily, const LayoutEntry& entry,
        std::vector<Gdiplus::PointF>& points, std::vector<BYTE>& types,
        bool hover = false) const;
    void CreatePath(const LayoutEntry& entry, Gdiplus::GraphicsPath& path) const;
    [[nodiscard]] COLORREF GetItemColor(const LayoutEntry& entry, bool hover) const;

    [[nodiscard]] const LayoutEntry* FindLayoutEntry(const CItem* item) const;

    CItem* m_layoutRoot = nullptr;
    CRect m_renderArea;
    CPoint m_center;
    double m_outerRadius = 0.0;
    double m_centerRadius = 0.0;
    double m_ringWidth = 0.0;
    int m_maxDepth = 0;
    std::vector<LayoutEntry> m_entries;
    std::unordered_map<const CItem*, std::size_t> m_itemEntries;
    std::vector<std::vector<RingEntry>> m_rings;
};
