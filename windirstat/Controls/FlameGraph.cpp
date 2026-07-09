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
static constexpr int FLAME_ROW_HEIGHT = CFlameGraph::ROW_HEIGHT;
static constexpr COLORREF FLAME_BG = CFlameGraph::BACKGROUND_COLOR;
static constexpr double PALETTE_BRIGHTNESS = 0.6;

static COLORREF DimColorLocal(COLORREF rgb, float factor = 0.7f) noexcept
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

int CFlameGraph::ComputeMaxDepth(CItem* item, int depth)
{
    if (item == nullptr || item->TmiIsLeaf() || item->TmiGetSize() == 0)
    {
        return depth;
    }

    int maxDepth = depth + 1;
    for (int i = 0; i < item->TmiGetChildCount(); i++)
    {
        CItem* child = item->TmiGetChild(i);
        if (child->TmiGetSize() == 0) break;
        maxDepth = std::max(maxDepth, ComputeMaxDepth(child, depth + 1));
    }
    return maxDepth;
}

void CFlameGraph::DrawFlameGraph(CDC* pdc, CRect rc, CItem* root)
{
    if (pdc == nullptr || root == nullptr || rc.Width() <= 0 || rc.Height() <= 0)
    {
        return;
    }

    // Collect breadcrumb ancestors when zoomed (root is not the model root)
    m_breadcrumbs.clear();
    const CItem* modelRoot = CWinDirStatModel::Get()->GetRootItem();
    if (root != modelRoot)
    {
        for (CItem* p = root->GetParent(); p != nullptr; p = p->GetParent())
        {
            m_breadcrumbs.push_back(p);
            if (p == modelRoot) break;
        }
        std::reverse(m_breadcrumbs.begin(), m_breadcrumbs.end());
    }
    m_breadcrumbHeight = static_cast<int>(m_breadcrumbs.size()) * FLAME_ROW_HEIGHT;

    if (root->TmiGetSize() == 0)
    {
        pdc->FillSolidRect(rc, FLAME_BG);
        return;
    }

    CSelectStockObject soFont(pdc, DEFAULT_GUI_FONT);

    m_renderArea = rc;

    // Fill background
    pdc->FillSolidRect(rc, FLAME_BG);

    // Render breadcrumb ancestors at the top when zoomed
    if (!m_breadcrumbs.empty())
    {
        CSetBkMode soBkMode(pdc, TRANSPARENT);

        for (size_t i = 0; i < m_breadcrumbs.size(); i++)
        {
            CItem* ancestor = m_breadcrumbs[i];
            const int y = rc.top + static_cast<int>(i) * FLAME_ROW_HEIGHT;
            const CRect itemRc(rc.left, y, rc.right, y + FLAME_ROW_HEIGHT);
            ancestor->TmiSetRectangle(itemRc);

            CRect fillRc = itemRc;
            fillRc.DeflateRect(1, 1);
            COLORREF color = GetFlameDepthColor(static_cast<int>(i));
            color = DimColorLocal(color, 0.35f);
            pdc->FillSolidRect(fillRc, color);

            // Draw text label with contrast-aware color
            const std::wstring name = ancestor->GetName();
            if (!name.empty() && fillRc.Width() >= MIN_LABEL_WIDTH && fillRc.Height() >= MIN_LABEL_HEIGHT)
            {
                const double brightness = CColorSpace::GetColorBrightness(color);
                pdc->SetTextColor(brightness > 0.5 ? RGB(0, 0, 0) : RGB(255, 255, 255));
                CRect textRc = fillRc;
                textRc.DeflateRect(3, 1);
                pdc->DrawText(name.c_str(), static_cast<int>(name.size()), &textRc,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            }

            // Draw separator line
            CPen sepPen(PS_SOLID, 1, RGB(80, 80, 90));
            CSelectObject sopen(pdc, &sepPen);
            pdc->MoveTo(rc.left, itemRc.bottom - 1);
            pdc->LineTo(rc.right, itemRc.bottom - 1);
        }
    }

    // Adjust main layout rect to start below breadcrumbs
    const int layoutTop = rc.top + m_breadcrumbHeight;
    CRect mainRc = rc;
    mainRc.top = layoutTop;
    if (mainRc.Height() <= 0) return;

    const int maxDepth = std::min(std::max(ComputeMaxDepth(root, 0), 1), mainRc.Height() / FLAME_ROW_HEIGHT);
    const int rowHeight = FLAME_ROW_HEIGHT;
    if (maxDepth < 1 || mainRc.Height() < rowHeight) return;

    LayoutItem(root, CRect(mainRc.left, mainRc.top, mainRc.right, mainRc.top + rowHeight), 0, rowHeight, true);

    RenderAll(pdc, root, CPoint(0, 0));
}

void CFlameGraph::LayoutItem(CItem* item, const CRect& rc, int depth, int rowHeight, bool /*isRoot*/)
{
    if (item == nullptr || rc.Width() <= 0 || rc.Height() <= 0)
    {
        return;
    }

    item->TmiSetRectangle(rc);

    if (item->TmiIsLeaf() || item->TmiGetSize() == 0)
    {
        return;
    }

    const int childCount = item->TmiGetChildCount();
    if (childCount == 0) return;

    const ULONGLONG totalSize = item->TmiGetSize();
    const int nextDepth = depth + 1;
    const int childRowTop = rc.top + rowHeight;

    if (childRowTop >= m_renderArea.bottom)
    {
        // Clear children's rectangles so RenderAll doesn't render stale data from a previous layout
        for (int i = 0; i < item->TmiGetChildCount(); i++)
        {
            CItem* child = item->TmiGetChild(i);
            child->TmiSetRectangle(CRect(0, 0, 0, 0));
        }
        return;
    }

    const int childRowBottom = childRowTop + rowHeight;
    const int childRowHeight = childRowBottom - childRowTop;
    if (childRowHeight <= 0) return;

    int x = rc.left;
    for (int i = 0; i < childCount; i++)
    {
        CItem* child = item->TmiGetChild(i);
        const ULONGLONG childSize = child->TmiGetSize();
        if (childSize == 0)
        {
            // Clear stale rectangles for remaining unprocessed siblings
            for (int j = i; j < childCount; j++)
            {
                item->TmiGetChild(j)->TmiSetRectangle(CRect(0, 0, 0, 0));
            }
            break;
        }

        int childWidth = static_cast<int>(static_cast<double>(childSize) * rc.Width() / totalSize);

        if (i == childCount - 1 || childWidth <= 0)
        {
            childWidth = std::max<int>(1, rc.right - x);
        }

        childWidth = std::min<int>(childWidth, rc.right - x);
        if (childWidth <= 0)
        {
            // Clear stale rectangles for remaining unprocessed siblings
            for (int j = i; j < childCount; j++)
            {
                item->TmiGetChild(j)->TmiSetRectangle(CRect(0, 0, 0, 0));
            }
            break;
        }

        CRect childRect(x, childRowTop, x + childWidth, childRowBottom);

        LayoutItem(child, childRect, nextDepth, rowHeight, false);

        x += childWidth;
        if (x >= rc.right)
        {
            // Clear stale rectangles for remaining unprocessed siblings
            for (int j = i + 1; j < childCount; j++)
            {
                item->TmiGetChild(j)->TmiSetRectangle(CRect(0, 0, 0, 0));
            }
            break;
        }
    }
}

bool CFlameGraph::IsDescendantOf(const CItem* test, const CItem* ancestor)
{
    if (test == nullptr || ancestor == nullptr || test == ancestor) return false;
    for (const CItem* parent = test->GetParent(); parent != nullptr; parent = parent->GetParent())
    {
        if (parent == ancestor) return true;
    }
    return false;
}

void CFlameGraph::RenderAll(CDC* pdc, CItem* root, const CPoint& offset) const
{
    struct StackEntry { CItem* item; int depth; };
    std::vector<StackEntry> stack;
    stack.reserve(128);
    stack.push_back({root, 0});

    CSetBkMode soBkMode(pdc, TRANSPARENT);

    while (!stack.empty())
    {
        auto [item, depth] = stack.back();
        stack.pop_back();

        CRect rc = item->TmiGetRectangle();
        rc.OffsetRect(offset);
        rc.DeflateRect(0, 0);

        if (rc.Width() < 2 || rc.Height() < 2) continue;

        // Compute display color
        COLORREF drawColor;
        {
            const DWORD rawColor = item->TmiGetGraphColor();
            const DWORD colorFlags = rawColor & CTreeMap::COLORFLAG_MASK;
            drawColor = rawColor & 0x00FFFFFF;

            if (drawColor == RGB(0, 0, 0) && item->IsTypeOrFlag(IT_DIRECTORY)
                && !item->IsTypeOrFlag(IT_FREESPACE, IT_UNKNOWN))
            {
                // Directories with no extension color get a depth-based color
                drawColor = GetFlameDepthColor(depth);
            }
            else if (colorFlags == CTreeMap::COLORFLAG_DARKER)
            {
                drawColor = CColorSpace::MakeBrightColor(drawColor, PALETTE_BRIGHTNESS);
                drawColor = DimColorLocal(drawColor, 0.66f);
            }
            else if (colorFlags == CTreeMap::COLORFLAG_LIGHTER)
            {
                drawColor = CColorSpace::MakeBrightColor(drawColor, PALETTE_BRIGHTNESS);
                drawColor = RGB(
                    std::min(255, GetRValue(drawColor) + 60),
                    std::min(255, GetGValue(drawColor) + 60),
                    std::min(255, GetBValue(drawColor) + 60));
            }
        }

        // Apply visual state modifiers (active item, hover, dimmed chain)
        if (m_activeItem != nullptr)
        {
            if (item == m_activeItem)
            {
                // Active item gets a warm highlight
                drawColor = RGB(
                    std::min(255, static_cast<int>(GetRValue(drawColor)) + 60),
                    std::min(255, std::max(static_cast<int>(GetGValue(drawColor)), static_cast<int>(GetBValue(drawColor)))),
                    std::min(255, static_cast<int>(GetBValue(drawColor)) + 40));
            }
            else if (IsDescendantOf(m_activeItem, item))
            {
                // Ancestors of the active item are dimmed for back-navigation
                drawColor = DimColorLocal(drawColor, 0.4f);
            }
        }

        if (m_hoverItem != nullptr && item == m_hoverItem && item != m_activeItem)
        {
            // Hover item gets a bright highlight
            drawColor = RGB(
                std::min(255, static_cast<int>(GetRValue(drawColor)) + 80),
                std::min(255, std::max(static_cast<int>(GetGValue(drawColor)), static_cast<int>(GetBValue(drawColor)))),
                std::min(255, static_cast<int>(GetBValue(drawColor)) + 60));
        }

        // Fill rectangle
        pdc->FillSolidRect(rc, drawColor);

        // Draw border for directories
        if (item->IsTypeOrFlag(IT_DIRECTORY) && rc.Width() > 4 && rc.Height() > 4)
        {
            CBrush borderBrush(DimColorLocal(drawColor, 0.6f));
            CRect borderRc = rc;
            pdc->FrameRect(&borderRc, &borderBrush);
        }

        // Draw text label with contrast-aware color
        if (rc.Width() >= MIN_LABEL_WIDTH && rc.Height() >= MIN_LABEL_HEIGHT
            && !item->IsTypeOrFlag(IT_FREESPACE, IT_UNKNOWN))
        {
            const std::wstring name = item->GetName();
            if (!name.empty())
            {
                const double brightness = CColorSpace::GetColorBrightness(drawColor);
                pdc->SetTextColor(brightness > 0.5 ? RGB(0, 0, 0) : RGB(255, 255, 255));
                CRect textRc = rc;
                textRc.DeflateRect(3, 1);
                pdc->DrawText(name.c_str(), static_cast<int>(name.size()), &textRc,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            }
        }

        // Push children
        if (!item->TmiIsLeaf() && item->TmiGetSize() > 0)
        {
            const int childCount = item->TmiGetChildCount();
            for (int i = childCount - 1; i >= 0; i--)
            {
                CItem* child = item->TmiGetChild(i);
                if (child->TmiGetSize() == 0) continue;
                const CRect& childRc = child->TmiGetRectangle();
                if (childRc.Width() <= 0 || childRc.Height() <= 0) continue;
                stack.push_back({child, depth + 1});
            }
        }
    }
}

CItem* CFlameGraph::FindItemByPoint(CItem* item, CPoint point) const
{
    if (item == nullptr) return nullptr;

    // Check if the point is in the breadcrumb area (above the item's layout)
    if (!m_breadcrumbs.empty() && point.y < m_breadcrumbHeight)
    {
        const int row = point.y / FLAME_ROW_HEIGHT;
        if (row >= 0 && static_cast<size_t>(row) < m_breadcrumbs.size())
        {
            return m_breadcrumbs[row];
        }
        return nullptr;
    }

    const CRect& rootRc = item->TmiGetRectangle();
    if (rootRc.IsRectEmpty()) return nullptr;

    // Determine which depth row the click falls in based on Y coordinate
    const int targetDepth = (point.y - rootRc.top) / FLAME_ROW_HEIGHT;
    if (targetDepth < 0) return nullptr;

    // Walk down from the starting item to the target depth
    CItem* current = item;
    int depth = 0;

    while (current != nullptr && depth < targetDepth)
    {
        if (current->TmiIsLeaf() || current->TmiGetSize() == 0)
        {
            return current;
        }

        // Find the child whose X span contains the click point
        CItem* next = nullptr;
        for (int i = 0; i < current->TmiGetChildCount(); i++)
        {
            CItem* child = current->TmiGetChild(i);
            if (child->TmiGetSize() == 0) break;
            const CRect& childRc = child->TmiGetRectangle();
            if (point.x >= childRc.left && point.x < childRc.right)
            {
                next = child;
                break;
            }
        }

        if (next == nullptr) return current;
        current = next;
        depth++;
    }

    return current;
}
