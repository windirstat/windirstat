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

#include "stdafx.h"
#include "SelectObject.h"
#include "TreeMap.h"
#include "DarkMode.h"

#include <algorithm>
#include <array>
#include <memory>
#include <vector>
#include <stack>

static constexpr COLORREF BGR(auto b, auto g, auto r)
{
    return static_cast<BYTE>(b) | static_cast<BYTE>(g) << 8 | static_cast<BYTE>(r) << 16;
}

// I define the "brightness" of an rgb value as (r+b+g)/3/255.
// The EqualizeColors() method creates a palette with colors
// all having the same brightness of 0.6
// Later in RenderCushion() this number is used again to
// scale the colors.

static constexpr double PALETTE_BRIGHTNESS = 0.6;

/////////////////////////////////////////////////////////////////////////////

double CColorSpace::GetColorBrightness(const COLORREF color)
{
    const unsigned int crIndividualIntensitySum = RGB_GET_RVALUE(color) + RGB_GET_GVALUE(color) + RGB_GET_BVALUE(color);
    return crIndividualIntensitySum / 255.0 / 3.0;
}

COLORREF CColorSpace::MakeBrightColor(const COLORREF color, const double brightness)
{
    ASSERT(brightness >= 0.0);
    ASSERT(brightness <= 1.0);

    double dred = (RGB_GET_RVALUE(color) & 0xFF) / 255.0;
    double dgreen = (RGB_GET_GVALUE(color) & 0xFF) / 255.0;
    double dblue = (RGB_GET_BVALUE(color) & 0xFF) / 255.0;

    const double f = 3.0 * brightness / (dred + dgreen + dblue);
    dred *= f;
    dgreen *= f;
    dblue *= f;

    int red = static_cast<int>(dred * 255);
    int green = static_cast<int>(dgreen * 255);
    int blue = static_cast<int>(dblue * 255);

    NormalizeColor(red, green, blue);

    return RGB(red, green, blue);
}

void CColorSpace::NormalizeColor(int& red, int& green, int& blue)
{
    ASSERT(red + green + blue <= 3 * 255);

    if (red > 255)
    {
        DistributeFirst(red, green, blue);
    }
    else if (green > 255)
    {
        DistributeFirst(green, red, blue);
    }
    else if (blue > 255)
    {
        DistributeFirst(blue, red, green);
    }
}

void CColorSpace::DistributeFirst(int& first, int& second, int& third)
{
    const int h = (first - 255) / 2;
    first = 255;
    second += h;
    third += h;

    if (second > 255)
    {
        const int j = second - 255;
        second = 255;
        third += j;
        ASSERT(third <= 255);
    }
    else if (third > 255)
    {
        const int j = third - 255;
        third = 255;
        second += j;
        ASSERT(second <= 255);
    }
}

/////////////////////////////////////////////////////////////////////////////

void CTreeMap::GetDefaultPalette(std::vector<COLORREF>& palette)
{
    EqualizeColors(DefaultCushionColors, std::size(DefaultCushionColors), palette);
}

void CTreeMap::EqualizeColors(const COLORREF* colors, const int count, std::vector<COLORREF>& out)
{
    out.resize(count);

    for (int i = 0; i < count; i++)
    {
        out[i] = CColorSpace::MakeBrightColor(colors[i], PALETTE_BRIGHTNESS);
    }
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
    m_Options = *options;

    // Derive normalized vector here for performance
    const double lx = m_Options.lightSourceX; // negative = left
    const double ly = m_Options.lightSourceY; // negative = top
    constexpr double lz = 10;

    const double len = sqrt(lx * lx + ly * ly + lz * lz);
    m_Lx = lx / len;
    m_Ly = ly / len;
    m_Lz = lz / len;
}

CTreeMap::Options CTreeMap::GetOptions() const
{
    return m_Options;
}

#ifdef _DEBUG
void CTreeMap::RecurseCheckTree(const Item* item)
{
    if (item->TmiIsLeaf())
    {
        ASSERT(item->TmiGetChildCount() == 0);
    }
    else
    {
        ULONGLONG sum = 0;
        ULONGLONG last = static_cast<ULONGLONG>(-1);
        for (int i = 0; i < item->TmiGetChildCount(); i++)
        {
            Item* child = item->TmiGetChild(i);
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


void CTreeMap::DrawTreeMap(CDC* pdc, CRect rc, Item* root, const Options* options)
{
#ifdef _DEBUG
    RecurseCheckTree(root);
#endif // _DEBUG

    if (options != nullptr)
    {
        SetOptions(options);
    }

    if (rc.Width() <= 0 || rc.Height() <= 0)
    {
        return;
    }

    if (!m_Options.grid)
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

    if (rc.Width() <= 0 || rc.Height() <= 0)
    {
        return;
    }

    m_RenderArea = rc;

    if (root->TmiGetSize() > 0)
    {
        // Create a temporary CDC that represents only the tree map
        CDC dcTreeView;
        VERIFY(dcTreeView.CreateCompatibleDC(pdc));

        // This bitmap will be blitted onto the temporary DC
        CBitmap bmp;

        // That bitmap in turn will be created from this array
        std::vector<COLORREF> bitmapBits;
        bitmapBits.resize(static_cast<std::vector<COLORREF>::size_type>(rc.Width()) *
            static_cast<std::vector<COLORREF>::size_type>(rc.Height()));
        DrawSolidRect(bitmapBits, CRect(CPoint(), rc.Size()), m_Options.gridColor, PALETTE_BRIGHTNESS);

        using DrawState = struct
        {
            std::array<double, 4> surface;
            CRect rc;
            Item* item;
            double h;
            bool asroot;
        };

        // Initialize the stack with the root item
        DrawState initialState;
        initialState.item = root;
        initialState.rc = CRect(0, 0, rc.Width(), rc.Height());
        initialState.asroot = true;
        initialState.surface = { 0, 0, 0, 0 };
        initialState.h = m_Options.height;

        // Defined at top level to prevent reallocation
        std::vector<double> childWidth;
        std::vector<double> rows;
        std::vector<int> childrenPerRow;

        // Main loop
        const int gridWidth = m_Options.grid ? 1 : 0;
        std::stack<DrawState> stack({ initialState });
        while (!stack.empty())
        {
            DrawState state = stack.top();
            stack.pop();
            Item* item = state.item;

            // Process the current state
            item->TmiSetRectangle(state.rc);

            if (state.rc.Width() <= gridWidth || state.rc.Height() <= gridWidth)
            {
                continue;
            }

            if (IsCushionShading() && (!state.asroot))
            {
                AddRidge(state.rc, state.surface, state.h);
            }

            if (item->TmiIsLeaf())
            {
                // Leaf node, render it
                RenderLeaf(bitmapBits, item, state.surface);
                continue;
            }

            if (m_Options.style == KDirStatStyle)
            {
                // Reset vectors for next run
                childWidth.resize(0);
                rows.resize(0);
                childrenPerRow.resize(0);

                // KDirStat style preparation
                const bool horizontalRows = KDirStat_ArrangeChildren(item, childWidth, rows, childrenPerRow);
                const double horizontalTop = horizontalRows ? state.rc.top : state.rc.left;

                // Now process the children
                const int width = horizontalRows ? state.rc.Width() : state.rc.Height();
                const int height = horizontalRows ? state.rc.Height() : state.rc.Width();

                double top = horizontalTop;
                for (size_t row = 0, c = 0; row < rows.size(); row++)
                {
                    const double fBottom = top + rows[row] * height;
                    int bottom = static_cast<int>(fBottom);
                    if (row == rows.size() - 1)
                    {
                        bottom = horizontalRows ? state.rc.bottom : state.rc.right;
                    }
                    double left = horizontalRows ? state.rc.left : state.rc.top;
                    for (int i = 0; i < childrenPerRow[row]; i++, c++)
                    {
                        Item* child = item->TmiGetChild(static_cast<int>(c));
                        const double childWidth_ = childWidth[c];
                        const double fRight = left + childWidth_ * width;
                        int right = static_cast<int>(fRight);

                        const bool lastChild = i == childrenPerRow[row] - 1 ||
                            (c + 1 < childWidth.size() && childWidth[c + 1] == 0);

                        if (lastChild)
                        {
                            right = horizontalRows ? state.rc.right : state.rc.bottom;
                        }

                        CRect rcChild;
                        if (horizontalRows)
                        {
                            rcChild.left = static_cast<int>(left);
                            rcChild.right = right;
                            rcChild.top = static_cast<int>(top);
                            rcChild.bottom = bottom;
                        }
                        else
                        {
                            rcChild.left = static_cast<int>(top);
                            rcChild.right = bottom;
                            rcChild.top = static_cast<int>(left);
                            rcChild.bottom = right;
                        }

                        // Prepare child state and push onto the stack
                        DrawState childState;
                        childState.item = child;
                        childState.rc = rcChild;
                        childState.asroot = false;
                        childState.surface = state.surface;
                        childState.h = state.h * m_Options.scaleFactor;

                        stack.push(childState);

                        left = fRight;
                    }
                    top = fBottom;
                }
            }
            else
            {
                // SequoiaView style processing
                CRect remaining = state.rc;
                ULONGLONG remainingSize = item->TmiGetSize();
                ASSERT(remainingSize > 0);
                const double sizePerSquarePixel = static_cast<double>(remainingSize) /
                    remaining.Width() / remaining.Height();
                int head = 0;
                const int maxChild = item->TmiGetChildCount();

                while (head < maxChild)
                {
                    ASSERT(remaining.Width() > 0);
                    ASSERT(remaining.Height() > 0);

                    const bool horizontal = remaining.Width() >= remaining.Height();
                    const int height = horizontal ? remaining.Height() : remaining.Width();

                    const double hh = height * height * sizePerSquarePixel;
                    ASSERT(hh > 0);

                    int rowBegin = head;
                    int rowEnd = head;

                    double worst = DBL_MAX;
                    ULONGLONG rmax = item->TmiGetChild(rowBegin)->TmiGetSize();
                    ULONGLONG sum = 0;

                    while (rowEnd < maxChild)
                    {
                        ULONGLONG childSize = item->TmiGetChild(rowEnd)->TmiGetSize();

                        if (childSize == 0)
                        {
                            rowEnd = maxChild;
                            break;
                        }

                        const ULONGLONG rmin = childSize;
                        double ss = (static_cast<double>(sum) + rmin) * (static_cast<double>(sum) + rmin);
                        double ratio1 = hh * rmax / ss;
                        double ratio2 = ss / hh / rmin;
                        double nextWorst = max(ratio1, ratio2);

                        if (nextWorst > worst)
                        {
                            break;
                        }

                        sum += rmin;
                        rowEnd++;
                        worst = nextWorst;
                    }

                    // Now process the row from rowBegin to rowEnd - 1
                    int width = horizontal ? remaining.Width() : remaining.Height();

                    if (sum < remainingSize)
                        width = static_cast<int>(static_cast<double>(sum) / remainingSize * width);

                    CRect rcRow = remaining;

                    if (horizontal)
                    {
                        rcRow.right = rcRow.left + width;
                    }
                    else
                    {
                        rcRow.bottom = rcRow.top + width;
                    }

                    // Distribute the children in the row
                    double fBegin = horizontal ? rcRow.top : rcRow.left;

                    for (int i = rowBegin; i < rowEnd; i++)
                    {
                        ULONGLONG childSize = item->TmiGetChild(i)->TmiGetSize();
                        double fraction = static_cast<double>(childSize) / sum;
                        double fEnd = fBegin + fraction * (horizontal ? rcRow.Height() : rcRow.Width());
                        int end = static_cast<int>(fEnd);

                        bool lastChild = (i == rowEnd - 1) ||
                            (i + 1 < item->TmiGetChildCount() && item->TmiGetChild(i + 1)->TmiGetSize() == 0);

                        if (lastChild)
                        {
                            end = horizontal ? rcRow.bottom : rcRow.right;
                        }

                        CRect rcChild;
                        if (horizontal)
                        {
                            rcChild.left = rcRow.left;
                            rcChild.right = rcRow.right;
                            rcChild.top = static_cast<int>(fBegin);
                            rcChild.bottom = end;
                        }
                        else
                        {
                            rcChild.left = static_cast<int>(fBegin);
                            rcChild.right = end;
                            rcChild.top = rcRow.top;
                            rcChild.bottom = rcRow.bottom;
                        }

                        // Prepare child state and push onto the stack
                        if (childSize > 0)
                        {
                            DrawState childState;
                            childState.item = item->TmiGetChild(i);
                            childState.rc = rcChild;
                            childState.asroot = false;
                            childState.surface = state.surface;
                            childState.h = state.h * m_Options.scaleFactor;
                            stack.push(childState);
                        }

                        fBegin = fEnd;
                    }

                    // Adjust remaining rectangle
                    if (horizontal)
                    {
                        remaining.left += width;
                    }
                    else
                    {
                        remaining.top += width;
                    }

                    remainingSize -= sum;
                    head += rowEnd - rowBegin;

                    if (remaining.Width() <= 0 || remaining.Height() <= 0)
                    {
                        if (head < maxChild)
                        {
                            item->TmiGetChild(head)->TmiSetRectangle(CRect(-1, -1, -1, -1));
                        }
                        break;
                    }
                }
            }
        }

        // Fill the bitmap with the array data and render
        VERIFY(bmp.CreateBitmap(rc.Width(), rc.Height(), 1, 32, bitmapBits.data()));
        dcTreeView.SelectObject(&bmp);
        VERIFY(pdc->BitBlt(rc.TopLeft().x, rc.TopLeft().y, rc.Width(), rc.Height(), &dcTreeView, 0, 0, SRCCOPY));

        // Free memory
        VERIFY(bmp.DeleteObject());
        VERIFY(dcTreeView.DeleteDC());
    }
    else
    {
        pdc->FillSolidRect(rc, RGB(0, 0, 0));
    }
}


CTreeMap::Item* CTreeMap::FindItemByPoint(Item* item, const CPoint point)
{
    ASSERT(item != nullptr);
    const CRect& rc = item->TmiGetRectangle();

    if (!rc.PtInRect(point))
    {
        // The only case that this function returns NULL is that
        // point is not inside the rectangle of item.
        //
        // Take notice of
        // (a) the very right an bottom lines, which can be "grid" and
        //     are not covered by the root rectangle,
        // (b) the fact, that WM_MOUSEMOVEs can occur after WM_SIZE but
        //     before WM_PAINT.
        //
        return nullptr;
    }

    ASSERT(rc.PtInRect(point));

    Item* ret = nullptr;

    const int gridWidth = m_Options.grid ? 1 : 0;

    if (rc.Width() <= gridWidth ||
        rc.Height() <= gridWidth ||
        item->TmiIsLeaf())
    {
        ret = item;
    }
    else
    {
        ASSERT(item->TmiGetSize() > 0);
        ASSERT(item->TmiGetChildCount() > 0);

        const auto childCount = item->TmiGetChildCount();
        for (int i = 0; i < childCount; i++)
        {
            Item* child = item->TmiGetChild(i);

            ASSERT(child->TmiGetSize() > 0);

#ifdef _DEBUG
            CRect rcChild(child->TmiGetRectangle());
            ASSERT(rcChild.right >= rcChild.left);
            ASSERT(rcChild.bottom >= rcChild.top);
            ASSERT(rcChild.left >= rc.left);
            ASSERT(rcChild.right <= rc.right);
            ASSERT(rcChild.top >= rc.top);
            ASSERT(rcChild.bottom <= rc.bottom);
#endif
            if (child->TmiGetRectangle().PtInRect(point))
            {
                ret = FindItemByPoint(child, point);
                ASSERT(ret != nullptr);
#ifdef STRONGDEBUG
#ifdef _DEBUG
                for (i++; i < item->TmiGetChildCount(); i++)
                {
                    child = item->TmiGetChild(i);

                    if (child->TmiGetSize() == 0)
                    {
                        break;
                    }

                    rcChild = child->TmiGetRectangle();
                    if (rcChild.left == -1)
                    {
                        ASSERT(rcChild.top == -1);
                        ASSERT(rcChild.right == -1);
                        ASSERT(rcChild.bottom == -1);
                        break;
                    }

                    ASSERT(rcChild.right >= rcChild.left);
                    ASSERT(rcChild.bottom >= rcChild.top);
                    ASSERT(rcChild.left >= rc.left);
                    ASSERT(rcChild.right <= rc.right);
                    ASSERT(rcChild.top >= rc.top);
                    ASSERT(rcChild.bottom <= rc.bottom);
                }
#endif
#endif

                break;
            }
        }
    }

    ASSERT(ret != nullptr);

    if (ret == nullptr)
    {
        ret = item;
    }

    return ret;
}

void CTreeMap::DrawColorPreview(CDC* pdc, const CRect& rc, const COLORREF color, const Options* options)
{
    if (options != nullptr)
    {
        SetOptions(options);
    }

    std::array<double, 4> surface = { 0, 0, 0, 0 };
    AddRidge(rc, surface, m_Options.height * m_Options.scaleFactor);

    m_RenderArea = rc;

    // Create a temporary CDC that represents only the tree map
    CDC dcTreeView;
    VERIFY(dcTreeView.CreateCompatibleDC(pdc));

    // This bitmap will be blitted onto the temporary DC
    CBitmap bmp;

    // That bitmap in turn will be created from this array
    std::vector<COLORREF> bitmapBits;
    bitmapBits.resize(static_cast<std::vector<COLORREF>::size_type>(rc.Width()) *
        static_cast<std::vector<COLORREF>::size_type>(rc.Height()));

    // Recursively draw the tree graph
    RenderRectangle(bitmapBits, CRect(0, 0, rc.Width(), rc.Height()), surface, color);

    // Fill the bitmap with the array
    VERIFY(bmp.CreateBitmap(rc.Width(), rc.Height(), 1, 32, bitmapBits.data()));

    // Render bitmap to the temporary CDC
    dcTreeView.SelectObject(&bmp);

    // And lastly, draw the temporary CDC to the real one
    VERIFY(pdc->BitBlt(rc.TopLeft().x, rc.TopLeft().y, rc.Width(), rc.Height(), &dcTreeView, 0, 0, SRCCOPY));

    if (m_Options.grid)
    {
        CPen pen(PS_SOLID, 1, m_Options.gridColor);
        CSelectObject sopen(pdc, &pen);
        CSelectStockObject sobrush(pdc, NULL_BRUSH);
        VERIFY(pdc->Rectangle(rc));
    }

    // Free memory
    VERIFY(bmp.DeleteObject());
    VERIFY(dcTreeView.DeleteDC());
}

void CTreeMap::RenderLeaf(std::vector<COLORREF>& bitmap, const Item* item, const std::array<double, 4>& surface) const
{
    CRect rc = item->TmiGetRectangle();

    if (m_Options.grid)
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
    double brightness = m_Options.brightness;

    if ((color & COLORFLAG_MASK) != 0)
    {
        const DWORD flags = color & COLORFLAG_MASK;
        color = CColorSpace::MakeBrightColor(color, PALETTE_BRIGHTNESS);

        if ((flags & COLORFLAG_DARKER) != 0)
        {
            brightness *= 0.66;
        }
        else
        {
            brightness *= 1.2;
            brightness = std::min<double>(brightness, 1.0);
        }
    }

    if (IsCushionShading())
    {
        DrawCushion(bitmap, rc, surface, color, brightness);
    }
    else
    {
        DrawSolidRect(bitmap, rc, color, brightness);
    }
}

// Helper functions for KDirStat style
bool CTreeMap::KDirStat_ArrangeChildren(
    const Item* parent,
    std::vector<double>& childWidth,
    std::vector<double>& rows,
    std::vector<int>& childrenPerRow
) const
{
    ASSERT(!parent->TmiIsLeaf());

    const auto childCount = parent->TmiGetChildCount();
    if (parent->TmiGetSize() == 0)
    {
        rows.emplace_back(1.0);
        childrenPerRow.emplace_back(childCount);
        childWidth.resize(childCount, 1.0 / childCount);
        return true;
    }

    auto const& parentRect = parent->TmiGetRectangle();
    const bool horizontalRows = parentRect.Width() >= parentRect.Height();

    double width = 1.0;
    if (horizontalRows)
    {
        if (parentRect.Height() > 0)
        {
            width = static_cast<double>(parentRect.Width()) / parentRect.Height();
        }
    }
    else
    {
        if (parentRect.Width() > 0)
        {
            width = static_cast<double>(parentRect.Height()) / parentRect.Width();
        }
    }

    childWidth.resize(childCount);
    for (int nextChild = 0; nextChild < childCount;)
    {
        int childrenUsed = 0;
        rows.emplace_back(KDirStat_CalculateNextRow(parent, nextChild, width, childrenUsed, childWidth));
        childrenPerRow.emplace_back(childrenUsed);
        nextChild += childrenUsed;
    }

    return horizontalRows;
}

double CTreeMap::KDirStat_CalculateNextRow(
    const Item* parent,
    const int nextChild,
    const double width,
    int& childrenUsed,
    std::vector<double>& childWidth
) const
{
    static constexpr double _minProportion = 0.4;
    ASSERT(_minProportion < 1.);

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
        // Rectangle(childSize) = childSize / mySize * width

        const double childWidth_ = childSize / mySize * width / virtualRowHeight;

        if (childWidth_ / virtualRowHeight < _minProportion)
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

    // Now as we know the rowHeight, we compute the widths of our children.
    for (int j = 0; j < childrenUsed; j++)
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
    return m_Options.ambientLight < 1.0
        && m_Options.height > 0.0
        && m_Options.scaleFactor > 0.0;
}

void CTreeMap::DrawSolidRect(std::vector<COLORREF>& bitmap, const CRect& rc, const COLORREF col, const double brightness) const
{
    const double factor = brightness / PALETTE_BRIGHTNESS;

    auto red = static_cast<int>(RGB_GET_RVALUE(col) * factor);
    auto green = static_cast<int>(RGB_GET_GVALUE(col) * factor);
    auto blue = static_cast<int>(RGB_GET_BVALUE(col) * factor);

    CColorSpace::NormalizeColor(red, green, blue);

    for (int iy = rc.top; iy < rc.bottom; iy++)
    {
        const auto rowStart = bitmap.begin() + (iy * m_RenderArea.Width()) + rc.left;
        std::fill_n(rowStart, rc.Width(), BGR(blue, green, red));
    }
}

void CTreeMap::DrawCushion(std::vector<COLORREF>& bitmap, const CRect& rc, const std::array<double, 4>& surface, const COLORREF col, const double brightness) const
{
    // Cushion parameters
    const double Ia = m_Options.ambientLight;

    // Derived parameters
    const double Is = 1 - Ia; // shading

    const double colR = RGB_GET_RVALUE(col);
    const double colG = RGB_GET_GVALUE(col);
    const double colB = RGB_GET_BVALUE(col);

    for (int iy = rc.top; iy < rc.bottom; iy++) for (int ix = rc.left; ix < rc.right; ix++)
    {
        const double nx = -(2 * surface[0] * (ix + 0.5) + surface[2]);
        const double ny = -(2 * surface[1] * (iy + 0.5) + surface[3]);
        double cosa = (nx * m_Lx + ny * m_Ly + m_Lz) / sqrt(nx * nx + ny * ny + 1.0);
        cosa = std::min<double>(cosa, 1.0);

        double pixel = Is * cosa;
        pixel = std::max<double>(pixel, 0.0);

        pixel += Ia;
        ASSERT(pixel <= 1.0);

        // Now, pixel is the brightness of the pixel, 0...1.0.

        // Apply contrast.
        // Not implemented.
        // Costs performance and nearly the same effect can be
        // made width the m_Options->ambientLight parameter.
        // pixel = pow(pixel, m_Options->contrast);

        // Apply "brightness"
        pixel *= brightness / PALETTE_BRIGHTNESS;

        // Make color value
        int red = static_cast<int>(colR * pixel);
        int green = static_cast<int>(colG * pixel);
        int blue = static_cast<int>(colB * pixel);

        CColorSpace::NormalizeColor(red, green, blue);

        // ... and set!
        bitmap[ix + iy * m_RenderArea.Width()] = BGR(blue, green, red);
    }
}

void CTreeMap::AddRidge(const CRect& rc, std::array<double,4> & surface, const double h)
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

/////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CTreeMapPreview, CStatic)
    ON_WM_PAINT()
END_MESSAGE_MAP()

CTreeMapPreview::CTreeMapPreview()
{
    m_Root = nullptr;
    BuildDemoData();
}

CTreeMapPreview::~CTreeMapPreview()
{
    delete m_Root;
}

void CTreeMapPreview::SetOptions(const CTreeMap::Options* options)
{
    m_TreeMap.SetOptions(options);
    Invalidate();
}

void CTreeMapPreview::BuildDemoData()
{
    CTreeMap::GetDefaultPalette(m_Colors);
    int col = -1;

    constexpr auto c4Items = 30;
    std::vector<CItem*> c4;
    c4.reserve(c4Items);
    COLORREF color = GetNextColor(col);
    for (int i = 0; i < c4Items; i++)
    {
        c4.emplace_back(new CItem(1 + 100 * i, color));
    }

    constexpr auto c0Items = 8;
    std::vector<CItem*> c0;
    c0.reserve(c0Items);
    for (int i = 0; i < c0Items; i++)
    {
        c0.emplace_back(new CItem(500 + 600 * i, GetNextColor(col)));
    }

    constexpr auto c1Items = 10;
    std::vector<CItem*> c1;
    c1.reserve(c1Items);
    color = GetNextColor(col);
    for (int i = 0; i < c1Items; i++)
    {
        c1.emplace_back(new CItem(1 + 200 * i, color));
    }
    c0.emplace_back(new CItem(c1));

    constexpr auto c2Items = 160;
    std::vector<CItem*> c2;
    c2.reserve(c2Items);
    color = GetNextColor(col);
    for (int i = 0; i < c2Items; i++)
    {
        c2.emplace_back(new CItem(1 + i, color));
    }

    const std::vector c3 =
    {
        new CItem(10000, GetNextColor(col)),
        new CItem(c4),
        new CItem(c2),
        new CItem(6000, GetNextColor(col)),
        new CItem(1500, GetNextColor(col))
    };

    const std::vector c10
    {
        new CItem(c0),
        new CItem(c3)
    };

    m_Root = new CItem(c10);
}

COLORREF CTreeMapPreview::GetNextColor(int& i) const
{
    i++;
    i %= m_Colors.size();
    return m_Colors[i];
}

void CTreeMapPreview::OnPaint()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(rc);
    m_TreeMap.DrawTreeMap(&dc, rc, m_Root);
}

