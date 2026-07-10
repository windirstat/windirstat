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

static constexpr COLORREF ApplyHoverColor(const COLORREF color) noexcept
{
    return RGB(
        std::min(255, GetRValue(color) + 80),
        std::max<int>(GetGValue(color), GetBValue(color)),
        std::min(255, GetBValue(color) + 60));
}

int CFlameGraph::GetDrawableChildCount(const CItem* item)
{
    const auto& children = item->GetChildren();
    const auto firstEmpty = std::ranges::partition_point(children,
        [](const CItem* child) { return child->TmiGetSize() != 0; });
    return static_cast<int>(std::ranges::distance(children.begin(), firstEmpty));
}

int CFlameGraph::ComputeVisibleMaxDepth(const CItem* item, const int width, const int depth)
{
    if (item == nullptr || width <= 0)
    {
        return depth;
    }

    struct PendingItem
    {
        const CItem* item;
        int width;
        int depth;
    };

    int maxDepth = depth;
    std::vector<PendingItem> pending;
    pending.reserve(128);
    pending.push_back({ item, width, depth });

    std::vector<ChildSpan> childSpans;
    while (!pending.empty())
    {
        const auto current = pending.back();
        pending.pop_back();
        maxDepth = std::max(maxDepth, current.depth);

        if (current.item->TmiIsLeaf() || current.item->TmiGetSize() == 0)
        {
            continue;
        }

        ComputeChildSpans(current.item, 0, current.width, childSpans);
        for (const ChildSpan& span : childSpans | std::views::reverse)
        {
            pending.push_back({ span.item, span.right - span.left, current.depth + 1 });
        }
    }
    return maxDepth;
}

void CFlameGraph::DrawFlameGraph(CDC* pdc, CRect rc, const CItem* root, const int rowHeight)
{
    if (pdc == nullptr || root == nullptr || rc.Width() <= 0 || rc.Height() <= 0)
    {
        ClearLayout();
        return;
    }

    const int effectiveRowHeight = std::max(1, rowHeight);
    if (m_layoutRoot != root || m_renderArea != rc || m_rowHeight != effectiveRowHeight)
    {
        BuildLayout(root, rc, effectiveRowHeight);
    }

    CSelectStockObject soFont(pdc, DEFAULT_GUI_FONT);
    CSetBkMode soBkMode(pdc, TRANSPARENT);

    // Always paint the background, including for a valid root whose aggregate
    // size is zero.
    pdc->FillSolidRect(rc, BACKGROUND_COLOR);
    RenderLayout(pdc);
}

void CFlameGraph::BuildLayout(const CItem* root, const CRect rc, const int rowHeight)
{
    ClearLayout();

    m_layoutRoot = root;
    m_renderArea = rc;
    m_rowHeight = rowHeight;
    m_minLabelWidth = ScaleMetric(MIN_LABEL_WIDTH, m_rowHeight);
    m_minLabelHeight = ScaleMetric(MIN_LABEL_HEIGHT, m_rowHeight);
    m_itemInset = ScaleMetric(1, m_rowHeight);
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
    const int breadcrumbHeight = static_cast<int>(m_breadcrumbs.size()) * m_rowHeight;

    // Record breadcrumb ancestors at the top when zoomed.
    for (std::size_t i = 0; i < m_breadcrumbs.size(); i++)
    {
        CItem* ancestor = m_breadcrumbs[i];
        const int y = rc.top + static_cast<int>(i) * m_rowHeight;
        AddLayoutEntry(ancestor, CRect(rc.left, y, rc.right, y + m_rowHeight),
            static_cast<int>(i), true);
    }

    // A zero-sized root has no graph frames, but its breadcrumb layout remains
    // available for rendering and hit testing.
    if (root->TmiGetSize() == 0)
    {
        return;
    }

    // Adjust main layout rect to start below breadcrumbs
    CRect mainRc = rc;
    mainRc.top += breadcrumbHeight;
    if (mainRc.Height() < m_rowHeight) return;

    LayoutItem(root,
        CRect(mainRc.left, mainRc.top, mainRc.right, mainRc.top + m_rowHeight), 0);
}

void CFlameGraph::ClearLayout()
{
    m_layoutRoot = nullptr;
    m_renderArea.SetRectEmpty();
    m_layout.clear();
    m_laidOutChildren.clear();
    m_rows.clear();
    m_breadcrumbs.clear();
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

        LayoutEntry& layout = AddLayoutEntry(current.item, current.rectangle,
            current.depth, false);

        if (current.item->TmiIsLeaf() || current.item->TmiGetSize() == 0)
        {
            continue;
        }

        const LONG childRowTop = current.rectangle.top + m_rowHeight;
        if (childRowTop >= m_renderArea.bottom)
        {
            continue;
        }
        const LONG childRowBottom = std::min<LONG>(childRowTop + m_rowHeight,
            m_renderArea.bottom);

        ComputeChildSpans(current.item, current.rectangle.left, current.rectangle.right,
            childSpans);
        if (childSpans.empty())
        {
            continue;
        }

        layout.firstChild = m_laidOutChildren.size();
        layout.childCount = childSpans.size();
        for (const ChildSpan& span : childSpans)
        {
            m_laidOutChildren.push_back({ span.item, span.right });
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

void CFlameGraph::RenderLayout(CDC* pdc) const
{
    CRect clip;
    if (pdc->GetClipBox(&clip) == ERROR) clip = m_renderArea;

    VisitRowItems(clip, CPoint(0, 0), [this, pdc](const RowItem& entry,
        const CRect& rectangle)
    {
        if (entry.breadcrumb)
        {
            RenderBreadcrumb(pdc, entry.item, rectangle, entry.depth, false);
        }
        else
        {
            RenderItem(pdc, entry.item, rectangle, entry.depth, false);
        }
    });
}

void CFlameGraph::RenderItem(CDC* pdc, const CItem* item, const CRect& rectangle,
    const int depth, const bool hover) const
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

    if (hover)
    {
        drawColor = ApplyHoverColor(drawColor);
    }

    pdc->FillSolidRect(rc, drawColor);

    if (item->IsTypeOrFlag(IT_DIRECTORY)
        && rc.Width() > m_borderThreshold && rc.Height() > m_borderThreshold)
    {
        CBrush borderBrush(DimColor(drawColor, 0.6f));
        pdc->FrameRect(&rc, &borderBrush);
    }

    if (!item->IsTypeOrFlag(IT_FREESPACE, IT_UNKNOWN))
    {
        RenderLabel(pdc, item, rc, drawColor);
    }
}

void CFlameGraph::RenderBreadcrumb(CDC* pdc, const CItem* item, const CRect& rectangle,
    const int depth, const bool hover) const
{
    CRect rc = rectangle;
    if (rc.Width() <= 0 || rc.Height() <= 0) return;

    CRect fillRc = rc;
    fillRc.DeflateRect(m_itemInset, m_itemInset);
    COLORREF color = DimColor(GetFlameDepthColor(depth), 0.35f);
    if (hover)
    {
        color = ApplyHoverColor(color);
    }
    pdc->FillSolidRect(fillRc, color);

    RenderLabel(pdc, item, fillRc, color);

    CPen separator(PS_SOLID, 1, RGB(80, 80, 90));
    CSelectObject soPen(pdc, &separator);
    pdc->MoveTo(rc.left, rc.bottom - 1);
    pdc->LineTo(rc.right, rc.bottom - 1);
}

void CFlameGraph::RenderLabel(CDC* pdc, const CItem* item, const CRect& rc,
    const COLORREF color) const
{
    if (rc.Width() < m_minLabelWidth || rc.Height() < m_minLabelHeight) return;

    const auto name = item->GetNameView();
    if (name.empty()) return;

    const double brightness = CColorSpace::GetColorBrightness(color);
    pdc->SetTextColor(brightness > 0.5 ? RGB(0, 0, 0) : RGB(255, 255, 255));
    CRect textRc = rc;
    textRc.DeflateRect(m_textInsetX, m_textInsetY);
    pdc->DrawText(name.data(), static_cast<int>(name.size()), &textRc,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
}

void CFlameGraph::DrawHoverItem(CDC* pdc, const CItem* item, const CPoint offset) const
{
    if (pdc == nullptr || item == nullptr) return;

    const auto found = m_layout.find(item);
    if (found == m_layout.end()) return;

    CSelectStockObject soFont(pdc, DEFAULT_GUI_FONT);
    CSetBkMode soBkMode(pdc, TRANSPARENT);
    CRect rectangle = found->second.rectangle;
    rectangle.OffsetRect(offset);
    if (found->second.breadcrumb)
    {
        RenderBreadcrumb(pdc, item, rectangle, found->second.depth, true);
    }
    else
    {
        RenderItem(pdc, item, rectangle, found->second.depth, true);
    }
}

CItem* CFlameGraph::FindItemByPoint(CItem* item, CPoint point) const
{
    if (item == nullptr || !m_renderArea.PtInRect(point)) return nullptr;

    const LONG breadcrumbRow = (point.y - m_renderArea.top) / m_rowHeight;
    if (breadcrumbRow < std::ssize(m_breadcrumbs))
    {
        return m_breadcrumbs[static_cast<std::size_t>(breadcrumbRow)];
    }

    const auto rootFound = m_layout.find(item);
    if (rootFound == m_layout.end() || rootFound->second.breadcrumb) return nullptr;
    const CRect& rootRc = rootFound->second.rectangle;

    // Determine which depth row the click falls in based on Y coordinate
    const int targetDepth = (point.y - rootRc.top) / m_rowHeight;
    if (targetDepth < 0) return nullptr;

    // Walk down from the starting item to the target depth
    CItem* current = item;
    int depth = 0;

    while (current != nullptr && depth < targetDepth)
    {
        if (current->TmiIsLeaf() || current->TmiGetSize() == 0)
        {
            return nullptr;
        }

        const auto currentFound = m_layout.find(current);
        if (currentFound == m_layout.end() || currentFound->second.childCount == 0)
        {
            return nullptr;
        }

        const std::size_t firstIndex = currentFound->second.firstChild;
        const std::size_t childCount = currentFound->second.childCount;
        if (firstIndex > m_laidOutChildren.size()
            || childCount > m_laidOutChildren.size() - firstIndex)
        {
            return nullptr;
        }

        const auto first = m_laidOutChildren.begin() + static_cast<std::ptrdiff_t>(firstIndex);
        const auto last = first + static_cast<std::ptrdiff_t>(childCount);
        const auto childPosition = std::ranges::upper_bound(first, last, point.x,
            std::ranges::less{}, &LaidOutChild::right);
        if (childPosition == last) return nullptr;

        CItem* next = childPosition->item;
        const auto childFound = m_layout.find(next);
        if (childFound == m_layout.end()) return nullptr;

        const CRect& childRectangle = childFound->second.rectangle;
        if (childFound->second.breadcrumb
            || childFound->second.depth != currentFound->second.depth + 1
            || point.x < childRectangle.left || point.x >= childRectangle.right)
        {
            return nullptr;
        }

        current = next;
        depth++;
    }

    const auto currentFound = m_layout.find(current);
    return currentFound != m_layout.end() && currentFound->second.rectangle.PtInRect(point)
        ? current
        : nullptr;
}
