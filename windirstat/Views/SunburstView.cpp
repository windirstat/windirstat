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
    const int margin = DpiRest(8, this);
    const int radius = std::max(0,
        std::min(rc.Width(), rc.Height()) / 2 - margin);
    if (radius < DpiRest(8, this)) return;

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
    const int separatorWidth = std::max(1, DpiRest(1, this));
    for (std::size_t ring = 0; ring < shades.size(); ++ring)
    {
        const int outer = radius - static_cast<int>(ring) * ringWidth;
        if (outer <= 0) break;
        const int inner = std::max(0, outer - ringWidth + separatorWidth);
        Gdiplus::GraphicsPath path(Gdiplus::FillModeAlternate);
        path.AddEllipse(center.x - outer, center.y - outer, outer * 2, outer * 2);
        if (inner > 0)
            path.AddEllipse(center.x - inner, center.y - inner, inner * 2, inner * 2);
        Gdiplus::SolidBrush brush(shades[ring]);
        graphics.FillPath(&brush, &path);
    }
    Gdiplus::Pen separator(Gdiplus::Color(255, 24, 24, 24),
        static_cast<Gdiplus::REAL>(separatorWidth));
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
    // A base render may have rebuilt geometry because of DPI or sizing, even
    // when the view deliberately retained the logical layout cache.
    ClearExtensionHighlightCache();
}

void CSunburstView::DrawHighlightExtension(CDC* pDC)
{
    const CWinDirStatModel* model = CWinDirStatModel::Get();
    const std::wstring extension = model->GetHighlightExtension();
    const bool unregistered = model->IsHighlightUnregistered();
    if (!m_extensionOutlineItemsValid || extension != m_cachedHighlightExtension
        || unregistered != m_cachedHighlightUnregistered)
    {
        m_extensionOutlineItems.clear();
        m_sunburst.VisitItems([&](const CItem* item)
        {
            if (IsExtensionHighlighted(item)) m_extensionOutlineItems.push_back(item);
        });
        m_cachedHighlightExtension = extension;
        m_cachedHighlightUnregistered = unregistered;
        m_extensionOutlineItemsValid = true;
    }

    m_sunburst.DrawOutlineItems(pDC, m_extensionOutlineItems,
        COptions::TreeMapHighlightColor, 2.0f);
}

void CSunburstView::DrawSelection(CDC* pDC)
{
    const auto& items = CWinDirStatModel::Get()->GetAllSelected();
    m_selectionOutlineItems.clear();
    m_selectionOutlineItems.reserve(items.size());
    for (const CItem* item : items)
        m_selectionOutlineItems.push_back(GetDisplayItem(item));
    m_sunburst.DrawOutlineItems(pDC, m_selectionOutlineItems,
        COptions::TreeMapHighlightColor, items.size() == 1 ? 3.0f : 2.0f);
}

CItem* CSunburstView::FindItemAtPoint(const CPoint point)
{
    return m_sunburst.FindItemByPoint(point, &m_hitRemainderSize);
}

bool CSunburstView::UpdateHoverDetails(const CItem* item, const bool itemChanged)
{
    if (item == nullptr) m_hitRemainderSize = 0;
    const bool hoveringRemainder = m_hitRemainderSize != 0;
    if (!itemChanged && hoveringRemainder == m_hoveringRemainder) return false;
    m_hoveringRemainder = hoveringRemainder;
    std::wstring path;
    ULONGLONG size = 0;
    if (item != nullptr)
    {
        path = item->GetPath();
        size = item->TmiGetSize();
        if (m_hitRemainderSize != 0)
        {
            if (!path.empty() && path.back() != L'\\') path.push_back(L'\\');
            path.push_back(L'\u2026');
            size = m_hitRemainderSize;
        }
    }
    if (path == m_paneTextOverride && size == m_paneSizeOverride) return false;
    m_paneTextOverride = std::move(path);
    m_paneSizeOverride = size;
    return true;
}

void CSunburstView::ClearVisualizationLayout()
{
    m_sunburst.ClearLayout();
    m_hitRemainderSize = 0;
    m_hoveringRemainder = false;
    m_selectionOutlineItems.clear();
    ClearExtensionHighlightCache();
}

void CSunburstView::OnRenderCacheTrimmed()
{
    m_sunburst.TrimMemory();
    decltype(m_extensionOutlineItems){}.swap(m_extensionOutlineItems);
    decltype(m_selectionOutlineItems){}.swap(m_selectionOutlineItems);
    m_cachedHighlightExtension.clear();
    m_cachedHighlightExtension.shrink_to_fit();
    m_extensionOutlineItemsValid = false;
}

void CSunburstView::ClearExtensionHighlightCache()
{
    m_extensionOutlineItems.clear();
    m_cachedHighlightExtension.clear();
    m_cachedHighlightUnregistered = false;
    m_extensionOutlineItemsValid = false;
}

void CSunburstView::OnUpdate(CWnd* sender, const MODEL_CHANGE change, CItem* item)
{
    if (change == MODEL_CHANGE_EXTENSION_SELECTION) ClearExtensionHighlightCache();
    CGraphView::OnUpdate(sender, change, item);
}

bool CSunburstView::CanReuseVisualizationLayout(const MODEL_CHANGE change) const
{
    return change == MODEL_CHANGE_TREEMAP_STYLE;
}
