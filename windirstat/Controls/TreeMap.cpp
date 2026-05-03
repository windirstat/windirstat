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

#include "pch.h"
#include "TreeMap.h"
#include "Item.h"

static constexpr COLORREF BGR(auto b, auto g, auto r)
{
    return static_cast<BYTE>(b) | static_cast<BYTE>(g) << 8 | static_cast<BYTE>(r) << 16;
}

// Define the "brightness" of an RBG value as (r+b+g)/3/255.
// The EqualizeColors() method creates a palette with colors
// all having the same brightness of 0.6
// Later in RenderCushion() this number is used again to
// scale the colors.

static constexpr double PALETTE_BRIGHTNESS = 0.6;

using Surface = std::array<double, 4>;

struct DrawStateInfo
{
    Surface surface{};
    CRect rc{};
    CItem* item = nullptr;
    double ridgeHeight = 0.0;
    bool asRoot = false;
};

struct LayoutScratch
{
    std::vector<double> childWidth;
    std::vector<double> rows;
    std::vector<int> childrenPerRow;
};

struct PreparedColor
{
    COLORREF color = RGB(0, 0, 0);
    double brightness = 0.0;
};

constexpr UINT EXTENSION_TEXT_FLAGS = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
constexpr int EXTENSION_TEXT_PADDING = 4;
const std::array<CPoint, 8> EXTENSION_SHADOW_OFFSETS = {
    CPoint(-1, -1),
    CPoint(0, -1),
    CPoint(1, -1),
    CPoint(-1, 0),
    CPoint(1, 0),
    CPoint(-1, 1),
    CPoint(0, 1),
    CPoint(1, 1)
};

[[nodiscard]] PreparedColor PrepareRenderColor(const DWORD color, const double brightness)
{
    const COLORREF baseColor = static_cast<COLORREF>(color);
    PreparedColor prepared{ baseColor, brightness };

    if ((color & CTreeMap::COLORFLAG_MASK) == 0)
    {
        return prepared;
    }

    const DWORD flags = color & CTreeMap::COLORFLAG_MASK;
    prepared.color = CColorSpace::MakeBrightColor(baseColor, PALETTE_BRIGHTNESS);

    if ((flags & CTreeMap::COLORFLAG_DARKER) != 0)
    {
        prepared.brightness *= 0.66;
    }
    else
    {
        prepared.brightness = std::min<double>(prepared.brightness * 1.2, 1.0);
    }

    return prepared;
}

[[nodiscard]] COLORREF MakeBitmapColor(const COLORREF color, const double brightness)
{
    const double factor = brightness / PALETTE_BRIGHTNESS;

    int red = static_cast<int>(GetRValue(color) * factor);
    int green = static_cast<int>(GetGValue(color) * factor);
    int blue = static_cast<int>(GetBValue(color) * factor);

    CColorSpace::NormalizeColor(red, green, blue);
    return BGR(blue, green, red);
}

[[nodiscard]] bool PrepareRenderArea(CDC* const pdc, CRect& rc, const bool drawOuterFrame)
{
    if (rc.Width() <= 0 || rc.Height() <= 0)
    {
        return false;
    }

    if (drawOuterFrame)
    {
        // We shrink the rectangle here, too.
        // If we didn't do this, the layout of the treemap would
        // change, when grid is switched on and off.
        CPen pen(PS_SOLID, 1, DarkMode::WdsSysColor(COLOR_3DSHADOW));
        CSelectObject sopen(pdc, &pen);
        pdc->MoveTo(rc.right - 1, rc.top);
        pdc->LineTo(rc.right - 1, rc.bottom);
        pdc->MoveTo(rc.left, rc.bottom - 1);
        pdc->LineTo(rc.right, rc.bottom - 1);
    }

    rc.right--;
    rc.bottom--;

    return rc.Width() > 0 && rc.Height() > 0;
}

void BlitBitmap(CDC* const pdc, const CRect& rc, const std::vector<COLORREF>& bitmapBits)
{
    CDC dcTreeView;
    dcTreeView.CreateCompatibleDC(pdc);

    CBitmap bmp;
    bmp.CreateBitmap(rc.Width(), rc.Height(), 1, 32, bitmapBits.data());

    CSelectObject sobmp(&dcTreeView, &bmp);
    pdc->BitBlt(rc.left, rc.top, rc.Width(), rc.Height(), &dcTreeView, 0, 0, SRCCOPY);
}

[[nodiscard]] bool CanDrawExtensionLabel(const CRect& rc, const CSize& textSize)
{
    return textSize.cx + EXTENSION_TEXT_PADDING <= rc.Width()
        && textSize.cy + EXTENSION_TEXT_PADDING <= rc.Height();
}

void DrawShadowedExtensionText(CDC* const pdc, const std::wstring_view text, const CRect& rc)
{
    const int savedDc = pdc->SaveDC();
    if (savedDc != 0)
    {
        pdc->IntersectClipRect(rc);
    }

    {
        CSetTextColor soShadowTextColor(pdc, RGB(0, 0, 0));
        for (const CPoint& offset : EXTENSION_SHADOW_OFFSETS)
        {
            CRect shadowRc = rc;
            shadowRc.OffsetRect(offset);
            pdc->DrawText(text.data(), static_cast<int>(text.size()), &shadowRc, EXTENSION_TEXT_FLAGS);
        }
    }

    {
        CSetTextColor soTextColor(pdc, RGB(255, 255, 255));
        CRect textRc = rc;
        pdc->DrawText(text.data(), static_cast<int>(text.size()), &textRc, EXTENSION_TEXT_FLAGS);
    }

    if (savedDc != 0)
    {
        pdc->RestoreDC(savedDc);
    }
}

/////////////////////////////////////////////////////////////////////////////


void CTreeMap::GetDefaultPalette(std::vector<COLORREF>& palette)
{
    palette.resize(std::size(DefaultCushionColors));
    std::ranges::transform(DefaultCushionColors, palette.begin(),
        [](const COLORREF color) { return CColorSpace::MakeBrightColor(color, PALETTE_BRIGHTNESS); });
}

CTreeMap::Options CTreeMap::GetDefaults()
{
    return DefaultOptions;
}

CTreeMap::CTreeMap()
{
    SetOptions(&DefaultOptions);
}

void CTreeMap::SetOptions(const Options* options)
{
    ASSERT(options != nullptr);
    m_options = *options;

    // Derive normalized vector here for performance
    const double lx = m_options.lightSourceX; // negative = left
    const double ly = m_options.lightSourceY; // negative = top
    constexpr double lz = 10;

    const double len = sqrt(lx * lx + ly * ly + lz * lz);
    m_lx = lx / len;
    m_ly = ly / len;
    m_lz = lz / len;
}

CTreeMap::Options CTreeMap::GetOptions() const
{
    return m_options;
}

#ifdef _DEBUG
void CTreeMap::RecurseCheckTree(const CItem* item)
{
    if (item->TmiIsLeaf())
    {
        ASSERT(item->TmiGetChildCount() == 0);
    }
    else
    {
        ULONGLONG sum = 0;
        ULONGLONG last = static_cast<ULONGLONG>(-1);
        for (const int i : std::views::iota(0, item->TmiGetChildCount()))
        {
            const CItem* child = item->TmiGetChild(i);
            const ULONGLONG size = child->TmiGetSize();
            ASSERT(size <= last);
            sum += size;
            last = size;
            RecurseCheckTree(child);
        }
        ASSERT(sum == item->TmiGetSize());
    }
}
#endif

void CTreeMap::DrawTreeMap(CDC* pdc, CRect rc, CItem* root, const Options* options)
{
    ASSERT(pdc != nullptr);
    ASSERT(root != nullptr);
    if (pdc == nullptr || root == nullptr)
    {
        return;
    }

#ifdef _DEBUG
    RecurseCheckTree(root);
#endif // _DEBUG

    if (options != nullptr)
    {
        SetOptions(options);
    }

    if (!PrepareRenderArea(pdc, rc, !m_options.grid))
    {
        return;
    }

    m_renderArea = rc;

    if (root->TmiGetSize() == 0)
    {
        pdc->FillSolidRect(rc, RGB(0, 0, 0));
        return;
    }

    const int renderWidth = rc.Width();
    const int renderHeight = rc.Height();
    const size_t pixelCount = static_cast<size_t>(renderWidth) * static_cast<size_t>(renderHeight);
    std::vector<COLORREF> bitmapBits(pixelCount, MakeBitmapColor(m_options.gridColor, PALETTE_BRIGHTNESS));

    const int gridWidth = m_options.grid ? 1 : 0;
    const bool cushionShading = IsCushionShading();

    LayoutScratch layoutScratch;
    std::vector<DrawStateInfo> stack;
    stack.reserve(128);
    stack.push_back({ {}, CRect(0, 0, renderWidth, renderHeight), root, m_options.height, true });

    auto pushChildState = [this, &stack](CItem* child, const CRect& childRect, const DrawStateInfo& parentState)
    {
        stack.push_back({ parentState.surface, childRect, child,
            parentState.ridgeHeight * m_options.scaleFactor, false });
    };

    auto pushKDirStatChildren = [this, &layoutScratch, &pushChildState](CItem* item, const DrawStateInfo& state)
    {
        const bool horizontalRows =
            KDirStat_ArrangeChildren(item, layoutScratch.childWidth, layoutScratch.rows, layoutScratch.childrenPerRow);

        const double rowOrigin = horizontalRows ? state.rc.top : state.rc.left;
        const int rowExtent = horizontalRows ? state.rc.Height() : state.rc.Width();
        const int columnExtent = horizontalRows ? state.rc.Width() : state.rc.Height();

        double top = rowOrigin;
        size_t childIndex = 0;
        for (const size_t rowIndex : std::views::iota(0u, layoutScratch.rows.size()))
        {
            const double fBottom = top + layoutScratch.rows[rowIndex] * rowExtent;
            int bottom = static_cast<int>(fBottom);
            if (rowIndex + 1 == layoutScratch.rows.size())
            {
                bottom = horizontalRows ? state.rc.bottom : state.rc.right;
            }

            double left = horizontalRows ? state.rc.left : state.rc.top;
            for (int childInRow = 0; childInRow < layoutScratch.childrenPerRow[rowIndex]; ++childInRow, ++childIndex)
            {
                CItem* child = item->TmiGetChild(static_cast<int>(childIndex));
                const double fRight = left + layoutScratch.childWidth[childIndex] * columnExtent;
                int right = static_cast<int>(fRight);

                const bool lastChild = childInRow + 1 == layoutScratch.childrenPerRow[rowIndex]
                    || (childIndex + 1 < layoutScratch.childWidth.size() && layoutScratch.childWidth[childIndex + 1] == 0.0);

                if (lastChild)
                {
                    right = horizontalRows ? state.rc.right : state.rc.bottom;
                }

                CRect rcChild;
                if (horizontalRows)
                {
                    rcChild = CRect(static_cast<int>(left), static_cast<int>(top), right, bottom);
                }
                else
                {
                    rcChild = CRect(static_cast<int>(top), static_cast<int>(left), bottom, right);
                }

                pushChildState(child, rcChild, state);
                left = fRight;
            }

            top = fBottom;
        }
    };

    auto pushSequoiaViewChildren = [this, &pushChildState](CItem* item, const DrawStateInfo& state)
    {
        CRect remaining = state.rc;
        ULONGLONG remainingSize = item->TmiGetSize();
        ASSERT(remainingSize > 0);

        const int maxChild = item->TmiGetChildCount();
        if (maxChild == 0)
        {
            return;
        }

        const double sizePerSquarePixel = static_cast<double>(remainingSize) / remaining.Width() / remaining.Height();
        int head = 0;

        while (head < maxChild)
        {
            ASSERT(remaining.Width() > 0);
            ASSERT(remaining.Height() > 0);

            const bool horizontal = remaining.Width() >= remaining.Height();
            const int rowThickness = horizontal ? remaining.Height() : remaining.Width();
            const double hh = rowThickness * rowThickness * sizePerSquarePixel;
            ASSERT(hh > 0);

            const int rowBegin = head;
            int rowEnd = head;

            double worst = DBL_MAX;
            const ULONGLONG rmax = item->TmiGetChild(rowBegin)->TmiGetSize();
            ULONGLONG sum = 0;

            while (rowEnd < maxChild)
            {
                const ULONGLONG childSize = item->TmiGetChild(rowEnd)->TmiGetSize();
                if (childSize == 0)
                {
                    rowEnd = maxChild;
                    break;
                }

                const double nextSum = static_cast<double>(sum) + childSize;
                const double ss = nextSum * nextSum;
                const double nextWorst = max(hh * rmax / ss, ss / hh / childSize);

                if (nextWorst > worst)
                {
                    break;
                }

                sum += childSize;
                ++rowEnd;
                worst = nextWorst;
            }

            if (sum == 0)
            {
                for (const int i : std::views::iota(head, maxChild))
                {
                    item->TmiGetChild(i)->TmiSetRectangle(CRect(-1, -1, -1, -1));
                }
                break;
            }

            const int remainingExtent = horizontal ? remaining.Width() : remaining.Height();
            int rowWidth = remainingExtent;
            if (sum < remainingSize)
            {
                rowWidth = static_cast<int>(static_cast<double>(sum) / remainingSize * remainingExtent);
                rowWidth = std::clamp(rowWidth, 1, remainingExtent);
            }

            CRect rcRow = remaining;
            if (horizontal)
            {
                rcRow.right = rcRow.left + rowWidth;
            }
            else
            {
                rcRow.bottom = rcRow.top + rowWidth;
            }

            double fBegin = horizontal ? rcRow.top : rcRow.left;
            for (const int i : std::views::iota(rowBegin, rowEnd))
            {
                const ULONGLONG childSize = item->TmiGetChild(i)->TmiGetSize();
                const double fraction = static_cast<double>(childSize) / sum;
                const double fEnd = fBegin + fraction * (horizontal ? rcRow.Height() : rcRow.Width());
                int end = static_cast<int>(fEnd);

                const bool lastChild = i + 1 == rowEnd
                    || (i + 1 < item->TmiGetChildCount() && item->TmiGetChild(i + 1)->TmiGetSize() == 0);

                if (lastChild)
                {
                    end = horizontal ? rcRow.bottom : rcRow.right;
                }

                CRect rcChild;
                if (horizontal)
                {
                    rcChild = CRect(rcRow.left, static_cast<int>(fBegin), rcRow.right, end);
                }
                else
                {
                    rcChild = CRect(static_cast<int>(fBegin), rcRow.top, end, rcRow.bottom);
                }

                pushChildState(item->TmiGetChild(i), rcChild, state);
                fBegin = fEnd;
            }

            if (horizontal)
            {
                remaining.left += rowWidth;
            }
            else
            {
                remaining.top += rowWidth;
            }

            remainingSize -= sum;
            head += rowEnd - rowBegin;

            if (remaining.Width() <= 0 || remaining.Height() <= 0)
            {
                for (const int i : std::views::iota(head, maxChild))
                {
                    item->TmiGetChild(i)->TmiSetRectangle(CRect(-1, -1, -1, -1));
                }
                break;
            }
        }
    };

    while (!stack.empty())
    {
        DrawStateInfo state = stack.back();
        stack.pop_back();

        CItem* const item = state.item;
        item->TmiSetRectangle(state.rc);

        if (state.rc.Width() <= gridWidth || state.rc.Height() <= gridWidth)
        {
            continue;
        }

        if (cushionShading && !state.asRoot)
        {
            AddRidge(state.rc, state.surface, state.ridgeHeight);
        }

        if (item->TmiIsLeaf())
        {
            RenderLeaf(bitmapBits, item, state.surface);
            continue;
        }

        switch (m_options.style)
        {
        case KDirStatStyle:
            pushKDirStatChildren(item, state);
            break;

        case SequoiaViewStyle:
        default:
            pushSequoiaViewChildren(item, state);
            break;
        }
    }

    BlitBitmap(pdc, rc, bitmapBits);

    if (m_options.showExtensions)
    {
        DrawExtensionLabels(pdc, root, rc.TopLeft());
    }
}


CItem* CTreeMap::FindItemByPoint(CItem* item, const CPoint point)
{
    ASSERT(item != nullptr);
    if (item == nullptr)
    {
        return nullptr;
    }

    if (!item->TmiGetRectangle().PtInRect(point))
    {
        return nullptr;
    }

    const int gridWidth = m_options.grid ? 1 : 0;
    CItem* current = item;

    while (true)
    {
        const CRect& rc = current->TmiGetRectangle();
        ASSERT(rc.PtInRect(point));

        if (rc.Width() <= gridWidth || rc.Height() <= gridWidth || current->TmiIsLeaf())
        {
            return current;
        }

        ASSERT(current->TmiGetSize() > 0);
        ASSERT(current->TmiGetChildCount() > 0);

        CItem* next = nullptr;
        for (const int i : std::views::iota(0, current->TmiGetChildCount()))
        {
            CItem* child = current->TmiGetChild(i);
            ASSERT(child->TmiGetSize() > 0);

#ifdef _DEBUG
            const CRect rcChild(child->TmiGetRectangle());
            ASSERT(rcChild.right >= rcChild.left);
            ASSERT(rcChild.bottom >= rcChild.top);
            ASSERT(rcChild.left >= rc.left);
            ASSERT(rcChild.right <= rc.right);
            ASSERT(rcChild.top >= rc.top);
            ASSERT(rcChild.bottom <= rc.bottom);
#endif

            if (child->TmiGetRectangle().PtInRect(point))
            {
                next = child;
                break;
            }
        }

        if (next == nullptr)
        {
            return current;
        }

        current = next;
    }
}


void CTreeMap::DrawColorPreview(CDC* pdc, const CRect& rc, const COLORREF color, const Options* options)
{
    ASSERT(pdc != nullptr);
    if (pdc == nullptr || rc.Width() <= 0 || rc.Height() <= 0)
    {
        return;
    }

    if (options != nullptr)
    {
        SetOptions(options);
    }

    Surface surface{};
    AddRidge(rc, surface, m_options.height * m_options.scaleFactor);

    m_renderArea = rc;

    std::vector<COLORREF> bitmapBits(static_cast<size_t>(rc.Width()) * static_cast<size_t>(rc.Height()));
    RenderRectangle(bitmapBits, CRect(0, 0, rc.Width(), rc.Height()), surface, color);

    BlitBitmap(pdc, rc, bitmapBits);

    if (m_options.grid)
    {
        CPen pen(PS_SOLID, 1, m_options.gridColor);
        CSelectObject sopen(pdc, &pen);
        CSelectStockObject sobrush(pdc, NULL_BRUSH);
        pdc->Rectangle(rc);
    }
}


void CTreeMap::RenderLeaf(std::vector<COLORREF>& bitmap, const CItem* item, const std::array<double, 4>& surface) const
{
    CRect rc = item->TmiGetRectangle();

    if (m_options.grid)
    {
        rc.top++;
        rc.left++;
        if (rc.Width() <= 0 || rc.Height() <= 0)
        {
            return;
        }
    }

    RenderRectangle(bitmap, rc, surface, item->TmiGetGraphColor());
}

void CTreeMap::RenderRectangle(std::vector<COLORREF>& bitmap, const CRect& rc, const std::array<double, 4>& surface, DWORD color) const
{
    if (rc.Width() <= 0 || rc.Height() <= 0)
    {
        return;
    }

    const PreparedColor prepared = PrepareRenderColor(color, m_options.brightness);

    if (IsCushionShading())
    {
        DrawCushion(bitmap, rc, surface, prepared.color, prepared.brightness);
    }
    else
    {
        DrawSolidRect(bitmap, rc, prepared.color, prepared.brightness);
    }
}


// Helper functions for KDirStat style
bool CTreeMap::KDirStat_ArrangeChildren(
    const CItem* parent,
    std::vector<double>& childWidth,
    std::vector<double>& rows,
    std::vector<int>& childrenPerRow
) const
{
    ASSERT(!parent->TmiIsLeaf());

    const int childCount = parent->TmiGetChildCount();
    childWidth.clear();
    rows.clear();
    childrenPerRow.clear();

    childWidth.reserve(childCount);
    rows.reserve(childCount);
    childrenPerRow.reserve(childCount);

    if (childCount == 0)
    {
        return true;
    }

    if (parent->TmiGetSize() == 0)
    {
        rows.emplace_back(1.0);
        childrenPerRow.emplace_back(childCount);
        childWidth.resize(childCount, 1.0 / childCount);
        return true;
    }

    const CRect& parentRect = parent->TmiGetRectangle();
    const bool horizontalRows = parentRect.Width() >= parentRect.Height();

    double width = 1.0;
    if (horizontalRows)
    {
        if (parentRect.Height() > 0)
        {
            width = static_cast<double>(parentRect.Width()) / parentRect.Height();
        }
    }
    else if (parentRect.Width() > 0)
    {
        width = static_cast<double>(parentRect.Height()) / parentRect.Width();
    }

    childWidth.resize(childCount);
    for (int nextChild = 0, childrenUsed = 0; nextChild < childCount; nextChild += childrenUsed)
    {
        rows.emplace_back(KDirStat_CalculateNextRow(parent, nextChild, width, childrenUsed, childWidth));
        childrenPerRow.emplace_back(childrenUsed);
    }

    return horizontalRows;
}


double CTreeMap::KDirStat_CalculateNextRow(
    const CItem* parent,
    const int nextChild,
    const double width,
    int& childrenUsed,
    std::vector<double>& childWidth
) const
{
    static constexpr double s_minProportion = 0.4;
    ASSERT(s_minProportion < 1.);

    ASSERT(nextChild < parent->TmiGetChildCount());
    ASSERT(width >= 1.0);

    const double mySize = static_cast<double>(parent->TmiGetSize());
    ASSERT(mySize > 0);
    ULONGLONG sizeUsed = 0;
    double rowHeight = 0;

    int i = nextChild;
    const auto childCount = parent->TmiGetChildCount();
    for (; i < childCount; i++)
    {
        const ULONGLONG childSize = parent->TmiGetChild(i)->TmiGetSize();
        if (childSize == 0)
        {
            ASSERT(i > nextChild); // first child has size > 0
            break;
        }

        sizeUsed += childSize;
        const double virtualRowHeight = static_cast<double>(sizeUsed) / mySize;
        ASSERT(virtualRowHeight > 0);
        ASSERT(virtualRowHeight <= 1);

        // Rectangle(mySize)    = width * 1.0
        // Rectangle(childSize) = childWidth * virtualRowHeight
        // Rectangle(childSize) = childSize / mySize * width / virtualRowHeight;

        const double childWidth_ = childSize / mySize * width / virtualRowHeight;

        if (childWidth_ / virtualRowHeight < s_minProportion)
        {
            ASSERT(i > nextChild); // because width >= 1 and _minProportion < 1.
            break;
        }
        rowHeight = virtualRowHeight;
    }
    ASSERT(i > nextChild);

    // Now i-1 is the last child used
    // and rowHeight is the height of the row.

    // We add the rest of the children, if their size is 0.
    while (i < childCount && parent->TmiGetChild(i)->TmiGetSize() == 0)
    {
        i++;
    }

    childrenUsed = i - nextChild;

    // Now as we know the rowHeight, we compute the widths of our children
    for (const int j : std::views::iota(0, childrenUsed))
    {
        // Rectangle(1.0 * 1.0) = mySize
        const double rowSize = mySize * rowHeight;
        const double childSize = static_cast<double>(parent->TmiGetChild(nextChild + j)->TmiGetSize());
        const double cw = childSize / rowSize;
        ASSERT(cw >= 0);
        childWidth[nextChild + j] = cw;
    }

    return rowHeight;
}

bool CTreeMap::IsCushionShading() const
{
    return m_options.ambientLight < 1.0
        && m_options.height > 0.0
        && m_options.scaleFactor > 0.0;
}

void CTreeMap::DrawSolidRect(std::vector<COLORREF>& bitmap, const CRect& rc, const COLORREF col, const double brightness) const
{
    const COLORREF pixelColor = MakeBitmapColor(col, brightness);
    const int stride = m_renderArea.Width();

    for (const int iy : std::views::iota(rc.top, rc.bottom))
    {
        const auto rowStart = bitmap.begin() + (static_cast<ptrdiff_t>(iy) * stride) + rc.left;
        std::fill_n(rowStart, rc.Width(), pixelColor);
    }
}


void CTreeMap::DrawCushion(std::vector<COLORREF>& bitmap, const CRect& rc, const std::array<double, 4>& surface, const COLORREF col, const double brightness) const
{
    const double Ia = m_options.ambientLight;
    const double Is = 1 - Ia;
    const double brightnessFactor = brightness / PALETTE_BRIGHTNESS;

    const double colR = GetRValue(col);
    const double colG = GetGValue(col);
    const double colB = GetBValue(col);
    const int stride = m_renderArea.Width();

    for (const int iy : std::views::iota(rc.top, rc.bottom))
    {
        const double ny = -(2 * surface[1] * (iy + 0.5) + surface[3]);
        COLORREF* const row = bitmap.data() + static_cast<size_t>(iy) * static_cast<size_t>(stride);

        for (const int ix : std::views::iota(rc.left, rc.right))
        {
            const double nx = -(2 * surface[0] * (ix + 0.5) + surface[2]);
            double cosa = (nx * m_lx + ny * m_ly + m_lz) / sqrt(nx * nx + ny * ny + 1.0);
            cosa = std::min<double>(cosa, 1.0);

            double pixel = Is * cosa;
            pixel = std::max<double>(pixel, 0.0);
            pixel += Ia;
            ASSERT(pixel <= 1.0);

            // Now, pixel is the brightness of the pixel, 0...1.0.
            // Apply contrast.
            // Not implemented.
            // Costs performance and nearly the same effect can be
            // made width the m_options->ambientLight parameter.
            // pixel = pow(pixel, m_options->contrast);

            pixel *= brightnessFactor;

            int red = static_cast<int>(colR * pixel);
            int green = static_cast<int>(colG * pixel);
            int blue = static_cast<int>(colB * pixel);

            CColorSpace::NormalizeColor(red, green, blue);
            row[ix] = BGR(blue, green, red);
        }
    }
}


void CTreeMap::AddRidge(const CRect& rc, std::array<double, 4>& surface, const double h)
{
    const int width = rc.Width();
    const int height = rc.Height();

    ASSERT(width > 0 && height > 0);

    const double h4 = 4 * h;

    const double wf = h4 / width;
    surface[2] += wf * (rc.right + rc.left);
    surface[0] -= wf;

    const double hf = h4 / height;
    surface[3] += hf * (rc.bottom + rc.top);
    surface[1] -= hf;
}

void CTreeMap::DrawExtensionLabels(CDC* pdc, CItem* root, const CPoint& offset) const
{
    ASSERT(pdc != nullptr);
    ASSERT(root != nullptr);
    if (pdc == nullptr || root == nullptr)
    {
        return;
    }

    CSelectStockObject soFont(pdc, DEFAULT_GUI_FONT);
    CSetBkMode soBkMode(pdc, TRANSPARENT);

    std::vector<const CItem*> stack;
    stack.reserve(128);
    stack.push_back(root);

    while (!stack.empty())
    {
        const CItem* item = stack.back();
        stack.pop_back();

        if (item->TmiIsLeaf())
        {
            CRect rc = item->TmiGetRectangle();
            rc.OffsetRect(offset);

            const std::wstring ext = item->GetExtension();
            if (ext.empty())
            {
                continue;
            }

            CSize textSize;
            ::GetTextExtentPoint32(pdc->GetSafeHdc(), ext.c_str(), static_cast<int>(ext.size()), &textSize);
            if (!CanDrawExtensionLabel(rc, textSize))
            {
                continue;
            }

            DrawShadowedExtensionText(pdc, ext, rc);
            continue;
        }

        for (const int i : std::views::iota(0, item->TmiGetChildCount()))
        {
            const CItem* child = item->TmiGetChild(i);
            if (child->TmiGetSize() > 0)
            {
                stack.push_back(child);
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CTreeMapPreview, CStatic)
    ON_WM_PAINT()
END_MESSAGE_MAP()

CTreeMapPreview::CTreeMapPreview()
{
    m_root = nullptr;
    BuildDemoData();
}

CTreeMapPreview::~CTreeMapPreview()
{
    delete m_root;
}

void CTreeMapPreview::SetOptions(const CTreeMap::Options* options)
{
    m_treeMap.SetOptions(options);
    Invalidate();
}

void CTreeMapPreview::BuildDemoData()
{
    [[msvc::noinline_calls]]
    {
        CTreeMap::GetDefaultPalette(m_colors);
        int col = -1;

        auto createLeaf = [&](const int size, const COLORREF color) -> CItem*
        {
            const auto item = new CItem(IT_FILE | ITF_PREVIEW, L"");
            item->SetSizePhysical(size);
            item->SetSizeLogical(size);
            item->SetIndex(color);
            return item;
        };

        auto createContainer = [&](std::vector<CItem*>& children) -> CItem*
        {
            std::ranges::sort(children, [](CItem* a, CItem* b) { return a->GetSizePhysical() > b->GetSizePhysical(); });
            const auto item = new CItem(IT_DIRECTORY, L"");
            for (auto* child : children)
            {
                item->AddChild(child);
            }
            return item;
        };

        constexpr auto c4Items = 30;
        std::vector<CItem*> c4;
        c4.reserve(c4Items);
        COLORREF color = GetNextColor(col);
        for (const int i : std::views::iota(0, c4Items))
        {
            c4.emplace_back(createLeaf(1 + 100 * i, color));
        }

        constexpr auto c0Items = 8;
        std::vector<CItem*> c0;
        c0.reserve(c0Items);
        for (const int i : std::views::iota(0, c0Items))
        {
            c0.emplace_back(createLeaf(500 + 600 * i, GetNextColor(col)));
        }

        constexpr auto c1Items = 10;
        std::vector<CItem*> c1;
        c1.reserve(c1Items);
        color = GetNextColor(col);
        for (const int i : std::views::iota(0, c1Items))
        {
            c1.emplace_back(createLeaf(1 + 200 * i, color));
        }
        c0.emplace_back(createContainer(c1));

        constexpr auto c2Items = 160;
        std::vector<CItem*> c2;
        c2.reserve(c2Items);
        color = GetNextColor(col);
        for (const int i : std::views::iota(0, c2Items))
        {
            c2.emplace_back(createLeaf(1 + i, color));
        }

        std::vector<CItem*> c3;
        c3.reserve(5);
        c3.emplace_back(createLeaf(10000, GetNextColor(col)));
        c3.emplace_back(createContainer(c4));
        c3.emplace_back(createContainer(c2));
        c3.emplace_back(createLeaf(6000, GetNextColor(col)));
        c3.emplace_back(createLeaf(1500, GetNextColor(col)));

        std::vector<CItem*> c10;
        c10.reserve(2);
        c10.emplace_back(createContainer(c0));
        c10.emplace_back(createContainer(c3));

        m_root = createContainer(c10);
    }
}

COLORREF CTreeMapPreview::GetNextColor(int& i) const
{
    i++;
    i %= m_colors.size();
    return m_colors[i];
}

void CTreeMapPreview::OnPaint()
{
    CPaintDC dc(this);
    const CRect rc = ClientRectOf(this);
    m_treeMap.DrawTreeMap(&dc, rc, m_root);
}
