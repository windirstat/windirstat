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
#include "FlameGraph.h"
#include "TreeMap.h"

static constexpr int MIN_LABEL_WIDTH = 40;
static constexpr int MIN_LABEL_HEIGHT = 14;
static constexpr double PALETTE_BRIGHTNESS = 0.6;

static int ScaleMetric(const int value, const int rowHeight) noexcept
{
    return std::max(1, ::MulDiv(value, rowHeight, CFlameGraph::ROW_HEIGHT));
}

static constexpr COLORREF DimColor(COLORREF rgb, float factor) noexcept
{
    factor = std::clamp(factor, 0.0f, 1.0f);
    return RGB(
        static_cast<BYTE>(GetRValue(rgb) * factor),
        static_cast<BYTE>(GetGValue(rgb) * factor),
        static_cast<BYTE>(GetBValue(rgb) * factor)
    );
}

static COLORREF GetFlameDepthColor(int depth) noexcept
{
    static constexpr std::array<COLORREF, 7> palette = {
        RGB(240, 128, 128),
        RGB(244, 200, 120),
        RGB(250, 250, 160),
        RGB(160, 240, 160),
        RGB(160, 240, 240),
        RGB(160, 160, 240),
        RGB(240, 160, 240)
    };
    return (depth <= 0) ? RGB(200, 200, 200) : palette[static_cast<std::size_t>(depth - 1) % palette.size()];
}

static COLORREF GetContrastingTextColor(const COLORREF color) noexcept
{
    const double luminance = CColorSpace::GetRelativeLuminance(color);
    const double blackContrast = (luminance + 0.05) / 0.05;
    const double whiteContrast = 1.05 / (luminance + 0.05);
    return blackContrast >= whiteContrast ? RGB(0, 0, 0) : RGB(255, 255, 255);
}

int CFlameGraph::GetDrawableChildCount(const CItem* item)
{
    const auto& children = item->GetChildren();
    const auto firstEmpty = std::ranges::partition_point(children,
        [](const CItem* child) { return child->TmiGetSize() != 0; });
    return static_cast<int>(std::ranges::distance(children.begin(), firstEmpty));
}

int CFlameGraph::PrepareLayout(const CItem* root, const int width, const int rowHeight)
{
    if (root == nullptr || width <= 0)
    {
        ClearLayout();
        return 0;
    }

    const int effectiveRowHeight = std::max(1, rowHeight);
    if (m_layoutRoot != root || m_renderArea.Width() != width
        || m_rowHeight != effectiveRowHeight)
    {
        BuildLayout(root, width, effectiveRowHeight);
    }
    return m_renderArea.Height();
}

void CFlameGraph::DrawFlameGraph(CDC* pdc) const
{
    if (pdc == nullptr || m_layoutRoot == nullptr || m_renderArea.IsRectEmpty()) return;

    CSelectStockObject soFont(pdc, DEFAULT_GUI_FONT);
    CSetBkMode soBkMode(pdc, TRANSPARENT);
    RenderLayout(pdc, false);
}

void CFlameGraph::DrawBreadcrumbs(CDC* pdc) const
{
    if (pdc == nullptr || m_breadcrumbs.empty()) return;

    CSelectStockObject soFont(pdc, DEFAULT_GUI_FONT);
    CSetBkMode soBkMode(pdc, TRANSPARENT);
    RenderLayout(pdc, true);
}

void CFlameGraph::BuildLayout(const CItem* root, const int width, const int rowHeight)
{
    ClearLayout();

    m_layoutRoot = root;
    m_renderArea.SetRect(0, 0, width, 0);
    m_rowHeight = rowHeight;
    m_minLabelWidth = ScaleMetric(MIN_LABEL_WIDTH, m_rowHeight);
    m_minLabelHeight = ScaleMetric(MIN_LABEL_HEIGHT, m_rowHeight);
    m_separatorThickness = ScaleMetric(1, m_rowHeight);
    m_textInsetX = ScaleMetric(3, m_rowHeight);
    m_textInsetY = ScaleMetric(1, m_rowHeight);
    m_borderThreshold = ScaleMetric(4, m_rowHeight);

    // Collect breadcrumb ancestors when zoomed (root is not the model root)
    const CItem* modelRoot = CWinDirStatModel::Get()->GetRootItem();
    if (root != modelRoot)
    {
        for (CItem* p = root->GetParent(); p != nullptr; p = p->GetParent())
        {
            m_breadcrumbs.push_back(p);
            if (p == modelRoot) break;
        }
        std::ranges::reverse(m_breadcrumbs);
    }
    LayoutBreadcrumbs(width);

    // Keep a zero-sized root interactive as well. This also makes breadcrumb-
    // only zoom states valid for hover and navigation while a scan is settling.
    const int graphTop = GetBreadcrumbHeight();
    LayoutItem(root, CRect(0, graphTop, width, graphTop + m_rowHeight), 0);

    if (!m_rows.empty())
    {
        m_renderArea.bottom = m_rows.back().front().rectangle.bottom;
    }
}

void CFlameGraph::LayoutBreadcrumbs(const int width)
{
    if (m_breadcrumbs.empty() || width <= 0) return;

    // One compact sticky row replaces the old full-width row per ancestor.
    // In the pathological case of more ancestors than horizontal pixels, keep
    // the nearest ancestors because they are the useful drill-up targets.
    const std::size_t visibleCount = std::min<std::size_t>(m_breadcrumbs.size(),
        static_cast<std::size_t>(width));
    const std::size_t firstVisible = m_breadcrumbs.size() - visibleCount;

    std::size_t totalWeight = 0;
    for (std::size_t i = firstVisible; i < m_breadcrumbs.size(); i++)
    {
        const std::size_t nameLength = m_breadcrumbs[i]->GetNameView(true).size();
        totalWeight += std::clamp<std::size_t>(nameLength + 2, 4, 32);
    }

    LONG left = 0;
    std::size_t cumulativeWeight = 0;
    for (std::size_t i = 0; i < visibleCount; i++)
    {
        CItem* ancestor = m_breadcrumbs[firstVisible + i];
        cumulativeWeight += std::clamp<std::size_t>(
            ancestor->GetNameView(true).size() + 2, 4, 32);

        const LONG remaining = static_cast<LONG>(visibleCount - i - 1);
        LONG right = width;
        if (remaining > 0)
        {
            right = static_cast<LONG>(std::llround(
                static_cast<long double>(cumulativeWeight) * width / totalWeight));
            right = std::clamp<LONG>(right, left + 1, width - remaining);
        }

        AddLayoutEntry(ancestor, CRect(left, 0, right, m_rowHeight),
            static_cast<int>(firstVisible + i), true);
        left = right;
    }
}

void CFlameGraph::ClearLayout()
{
    m_layoutRoot = nullptr;
    m_renderArea.SetRectEmpty();
    m_layout.clear();
    m_rows.clear();
    m_breadcrumbs.clear();
}

void CFlameGraph::TrimMemory()
{
    ClearLayout();
    decltype(m_layout){}.swap(m_layout);
    decltype(m_rows){}.swap(m_rows);
    decltype(m_breadcrumbs){}.swap(m_breadcrumbs);
}

bool CFlameGraph::IsBreadcrumb(const CItem* item) const
{
    const auto found = m_layout.find(item);
    return found != m_layout.end() && found->second.breadcrumb;
}

int CFlameGraph::GetBreadcrumbHeight() const
{
    return m_breadcrumbs.empty() ? 0 : m_rowHeight;
}

bool CFlameGraph::TryGetItemRectangle(const CItem* item, CRect& rectangle) const
{
    const auto found = m_layout.find(item);
    if (found == m_layout.end()) return false;

    rectangle = found->second.rectangle;
    return true;
}

CFlameGraph::LayoutEntry& CFlameGraph::AddLayoutEntry(const CItem* item,
    const CRect rectangle, const int depth, const bool breadcrumb)
{
    const auto [found, inserted] = m_layout.emplace(item,
        LayoutEntry{ rectangle, depth, breadcrumb });
    ASSERT(inserted);
    if (!inserted) return found->second;

    const LONG relativeTop = rectangle.top - m_renderArea.top;
    ASSERT(relativeTop >= 0);
    if (relativeTop >= 0)
    {
        const auto rowIndex = static_cast<std::size_t>(relativeTop / m_rowHeight);
        if (m_rows.size() <= rowIndex) m_rows.resize(rowIndex + 1);

        auto& row = m_rows[rowIndex];
        ASSERT(row.empty() || row.back().rectangle.right <= rectangle.left);
        row.push_back({ item, rectangle, depth, breadcrumb });
    }
    return found->second;
}

void CFlameGraph::ComputeChildSpans(const CItem* item, const LONG left, const LONG right,
    std::vector<ChildSpan>& spans)
{
    spans.clear();
    if (item == nullptr || right <= left || item->TmiIsLeaf() || item->TmiGetSize() == 0)
    {
        return;
    }

    const ULONGLONG totalSize = item->TmiGetSize();

    // Child collections are size-sorted, so the first zero-sized child ends
    // the drawable range. The last child absorbs harmless aggregate mismatch.
    const int childCount = GetDrawableChildCount(item);
    if (childCount == 0) return;

    spans.reserve(std::min<std::size_t>(static_cast<std::size_t>(right - left),
        static_cast<std::size_t>(childCount)));

    LONG x = left;
    ULONGLONG cumulativeSize = 0;
    for (int i = 0; i < childCount && x < right; i++)
    {
        CItem* child = item->TmiGetChild(i);
        const ULONGLONG childSize = child->TmiGetSize();

        // Saturate at the parent total, both to avoid addition overflow and to
        // tolerate a transient aggregate mismatch while the model is updating.
        if (cumulativeSize >= totalSize || childSize >= totalSize - cumulativeSize)
        {
            cumulativeSize = totalSize;
        }
        else
        {
            cumulativeSize += childSize;
        }

        LONG nextX = right;
        if (i + 1 < childCount && cumulativeSize < totalSize)
        {
            const long double scaledWidth = static_cast<long double>(cumulativeSize)
                * static_cast<long double>(right - left) / static_cast<long double>(totalSize);
            nextX = left + static_cast<LONG>(std::llround(scaledWidth));
        }
        nextX = std::clamp<LONG>(nextX, x, right);

        // A subpixel child can legitimately have no rectangle. Cumulative
        // boundaries preserve its fraction for later siblings instead of
        // incorrectly assigning the entire remaining row to that child.
        if (nextX > x)
        {
            spans.push_back({ child, x, nextX });
        }
        x = nextX;
    }
}

void CFlameGraph::LayoutItem(const CItem* item, const CRect& rc, const int depth)
{
    struct PendingItem
    {
        const CItem* item;
        CRect rectangle;
        int depth;
    };

    std::vector<PendingItem> pending;
    pending.reserve(128);
    pending.push_back({ item, rc, depth });

    std::vector<ChildSpan> childSpans;
    while (!pending.empty())
    {
        const auto current = pending.back();
        pending.pop_back();
        if (current.item == nullptr || current.rectangle.Width() <= 0
            || current.rectangle.Height() <= 0)
        {
            continue;
        }

        AddLayoutEntry(current.item, current.rectangle, current.depth, false);

        if (current.item->TmiIsLeaf() || current.item->TmiGetSize() == 0)
        {
            continue;
        }

        if (current.rectangle.bottom > LONG_MAX - m_rowHeight)
        {
            continue;
        }
        const LONG childRowTop = current.rectangle.bottom;
        const LONG childRowBottom = childRowTop + m_rowHeight;

        ComputeChildSpans(current.item, current.rectangle.left, current.rectangle.right,
            childSpans);
        if (childSpans.empty())
        {
            continue;
        }

        // Reverse insertion preserves the recursive pre-order traversal when
        // entries are popped from the explicit stack.
        for (const ChildSpan& span : childSpans | std::views::reverse)
        {
            pending.push_back({ span.item,
                CRect(span.left, childRowTop, span.right, childRowBottom),
                current.depth + 1 });
        }
    }
}

void CFlameGraph::RenderLayout(CDC* pdc, const bool breadcrumbs) const
{
    CRect clip;
    if (pdc->GetClipBox(&clip) == ERROR) clip = m_renderArea;

    VisitRowItems(clip, CPoint(0, 0), [this, pdc, breadcrumbs](const RowItem& entry,
        const CRect& rectangle)
    {
        if (entry.breadcrumb != breadcrumbs) return;
        if (entry.breadcrumb)
        {
            RenderBreadcrumb(pdc, entry.item, rectangle, entry.depth);
        }
        else
        {
            RenderItem(pdc, entry.item, rectangle, entry.depth);
        }
    });
}

void CFlameGraph::RenderItem(CDC* pdc, const CItem* item, const CRect& rectangle,
    const int depth) const
{
    CRect rc = rectangle;
    if (rc.Width() <= 0 || rc.Height() <= 0) return;

    const DWORD rawColor = item->TmiGetGraphColor();
    const DWORD colorFlags = rawColor & CTreeMap::COLORFLAG_MASK;
    COLORREF drawColor = rawColor & 0x00FFFFFF;

    if (drawColor == RGB(0, 0, 0) && item->IsTypeOrFlag(IT_DIRECTORY)
        && !item->IsTypeOrFlag(IT_FREESPACE, IT_UNKNOWN))
    {
        drawColor = GetFlameDepthColor(depth);
    }
    else if (colorFlags == CTreeMap::COLORFLAG_DARKER)
    {
        drawColor = CColorSpace::MakeBrightColor(drawColor, PALETTE_BRIGHTNESS);
        drawColor = DimColor(drawColor, 0.66f);
    }
    else if (colorFlags == CTreeMap::COLORFLAG_LIGHTER)
    {
        drawColor = CColorSpace::MakeBrightColor(drawColor, PALETTE_BRIGHTNESS);
        drawColor = RGB(
            std::min(255, GetRValue(drawColor) + 60),
            std::min(255, GetGValue(drawColor) + 60),
            std::min(255, GetBValue(drawColor) + 60));
    }

    pdc->FillSolidRect(rc, drawColor);

    if (item->IsTypeOrFlag(IT_DIRECTORY)
        && rc.Width() > m_borderThreshold && rc.Height() > m_borderThreshold)
    {
        // Reuse the process-wide DC brush instead of constructing a GDI brush
        // for every directory tile.
        ::SetDCBrushColor(pdc->GetSafeHdc(), DimColor(drawColor, 0.6f));
        ::FrameRect(pdc->GetSafeHdc(), &rc,
            static_cast<HBRUSH>(::GetStockObject(DC_BRUSH)));
    }

    if (!item->IsTypeOrFlag(IT_FREESPACE, IT_UNKNOWN))
    {
        RenderLabel(pdc, item, rc, drawColor);
    }

    // Each item owns its right and bottom separators. Solid strips avoid a GDI
    // pen allocation per tile and remain crisp at every scaled row height.
    const int separator = std::min({ m_separatorThickness, rc.Width(), rc.Height() });
    if (separator > 0 && rc.Width() > separator && rc.Height() > separator)
    {
        pdc->FillSolidRect(CRect(rc.right - separator, rc.top,
            rc.right, rc.bottom), BACKGROUND_COLOR);
        pdc->FillSolidRect(CRect(rc.left, rc.bottom - separator,
            rc.right, rc.bottom), BACKGROUND_COLOR);
    }
}

void CFlameGraph::RenderBreadcrumb(CDC* pdc, const CItem* item, const CRect& rectangle,
    const int depth) const
{
    CRect rc = rectangle;
    if (rc.Width() <= 0 || rc.Height() <= 0) return;

    const int separator = std::min({ m_separatorThickness, rc.Width(), rc.Height() });
    CRect fillRc = rc;
    fillRc.right -= separator;
    fillRc.bottom -= separator;
    COLORREF color = DimColor(GetFlameDepthColor(depth), 0.35f);
    if (!fillRc.IsRectEmpty())
    {
        pdc->FillSolidRect(fillRc, color);
        RenderLabel(pdc, item, fillRc, color);
    }

    if (separator > 0)
    {
        static constexpr COLORREF separatorColor = RGB(80, 80, 90);
        pdc->FillSolidRect(CRect(rc.right - separator, rc.top,
            rc.right, rc.bottom), separatorColor);
        pdc->FillSolidRect(CRect(rc.left, rc.bottom - separator,
            rc.right, rc.bottom), separatorColor);
    }
}

void CFlameGraph::RenderLabel(CDC* pdc, const CItem* item, const CRect& rc,
    const COLORREF color) const
{
    if (rc.Width() < m_minLabelWidth || rc.Height() < m_minLabelHeight) return;

    const auto name = item->GetNameView(true);
    if (name.empty()) return;

    pdc->SetTextColor(GetContrastingTextColor(color));
    CRect textRc = rc;
    textRc.DeflateRect(m_textInsetX, m_textInsetY);
    pdc->DrawText(name.data(), static_cast<int>(name.size()), &textRc,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
}

CItem* CFlameGraph::FindItemByPoint(CItem* item, CPoint point) const
{
    if (item == nullptr || item != m_layoutRoot || !m_renderArea.PtInRect(point)
        || m_rows.empty()) return nullptr;

    const auto rowIndex = static_cast<std::size_t>(
        (point.y - m_renderArea.top) / m_rowHeight);
    if (rowIndex >= m_rows.size()) return nullptr;

    const auto& row = m_rows[rowIndex];
    const auto found = std::ranges::upper_bound(row, point.x,
        std::ranges::less{}, [](const RowItem& entry) {
            return entry.rectangle.right;
        });
    return found != row.end() && found->rectangle.PtInRect(point)
        ? const_cast<CItem*>(found->item)
        : nullptr;
}
