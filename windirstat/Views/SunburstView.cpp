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

#include "pch.h"
#include "SunburstView.h"

#include <numbers>

IMPLEMENT_DYNCREATE(CSunburstView, CGraphView)

BEGIN_MESSAGE_MAP(CSunburstView, CGraphView)
END_MESSAGE_MAP()

void CSunburstView::DrawEmptyPlaceholder(CDC* pDC, const CRect& rc)
{
    const int radius = std::max(0, std::min(rc.Width(), rc.Height()) / 2 - 8);
    if (radius < 8) return;

    const CPoint center = rc.CenterPoint();
    Gdiplus::Graphics graphics(pDC->GetSafeHdc());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    const std::array<Gdiplus::Color, 4> shades{
        Gdiplus::Color(255, 58, 61, 68),
        Gdiplus::Color(255, 72, 76, 84),
        Gdiplus::Color(255, 50, 53, 59),
        Gdiplus::Color(255, 84, 88, 97),
    };
    const int ringWidth = (radius + static_cast<int>(shades.size()) - 1)
        / static_cast<int>(shades.size());
    for (std::size_t ring = 0; ring < shades.size(); ++ring)
    {
        const int outer = radius - static_cast<int>(ring) * ringWidth;
        if (outer <= 0) break;
        const int inner = std::max(0, outer - ringWidth + 1);
        Gdiplus::GraphicsPath path(Gdiplus::FillModeAlternate);
        path.AddEllipse(center.x - outer, center.y - outer, outer * 2, outer * 2);
        if (inner > 0)
            path.AddEllipse(center.x - inner, center.y - inner, inner * 2, inner * 2);
        Gdiplus::SolidBrush brush(shades[ring]);
        graphics.FillPath(&brush, &path);
    }
    Gdiplus::Pen separator(Gdiplus::Color(255, 24, 24, 24), 1.0f);
    for (int i = 0; i < 12; i++)
    {
        const double angle = i * std::numbers::pi_v<double> / 6.0;
        const Gdiplus::Point end(
            center.x + static_cast<int>(std::cos(angle) * radius),
            center.y + static_cast<int>(std::sin(angle) * radius));
        graphics.DrawLine(&separator, Gdiplus::Point(center.x, center.y), end);
    }
}

void CSunburstView::RenderVisualization(CDC* pDC, const CRect rect)
{
    m_sunburst.DrawSunburst(pDC, rect,
        CWinDirStatModel::Get()->GetZoomItem(), COptions::TreeMapMaxDepth);
}

void CSunburstView::DrawHoverOverlay(CDC* pDC)
{
    if (m_hoverItem != nullptr) m_sunburst.DrawHoverItem(pDC, m_hoverItem);
}

void CSunburstView::DrawHighlightExtension(CDC* pDC)
{
    m_outlineItems.clear();

    m_sunburst.VisitItems([&](const CItem* item)
    {
        if (IsExtensionHighlighted(item)) m_outlineItems.push_back(item);
    });
    m_sunburst.DrawOutlineItems(pDC, m_outlineItems,
        COptions::TreeMapHighlightColor, 2.0f);
}

void CSunburstView::DrawSelection(CDC* pDC)
{
    const auto& items = CWinDirStatModel::Get()->GetAllSelected();
    m_outlineItems.clear();
    m_outlineItems.reserve(items.size());
    for (const CItem* item : items) m_outlineItems.push_back(GetDisplayItem(item));
    m_sunburst.DrawOutlineItems(pDC, m_outlineItems, COptions::TreeMapHighlightColor,
        items.size() == 1 ? 3.0f : 2.0f);
}

CItem* CSunburstView::FindItemAtPoint(const CPoint point)
{
    return m_sunburst.FindItemByPoint(point);
}

void CSunburstView::ClearVisualizationLayout()
{
    m_sunburst.ClearLayout();
    m_outlineItems.clear();
}

void CSunburstView::OnHoverItemChanged(const CItem* /*oldItem*/, const CItem* /*newItem*/)
{
    if (GetSafeHwnd()) Invalidate(FALSE);
}
