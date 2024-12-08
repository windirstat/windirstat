// TreeMap.cpp - Implementation of CColorSpace, CTreeMap and CTreeMapPreview
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "stdafx.h"
#include "SelectObject.h"
#include "TreeMap.h"

#pragma strict_gs_check(off)

#include <vector>

constexpr COLORREF BGR(auto b, auto g, auto r)
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
    const COLORREF crIndividualIntensitySum = RGB_GET_RVALUE(color) + RGB_GET_GVALUE(color) + RGB_GET_BVALUE(color);
    return crIndividualIntensitySum / 255.0 / 3.0;
}

COLORREF CColorSpace::MakeBrightColor(const COLORREF color, const double brightness)
{
    ASSERT(brightness >= 0.0);
    ASSERT(brightness <= 1.0);

    double dred   = (RGB_GET_RVALUE(color) & 0xFF) / 255.0;
    double dgreen = (RGB_GET_GVALUE(color) & 0xFF) / 255.0;
    double dblue  = (RGB_GET_BVALUE(color) & 0xFF) / 255.0;

    const double f = 3.0 * brightness / (dred + dgreen + dblue);
    dred *= f;
    dgreen *= f;
    dblue *= f;

    int red   = static_cast<int>(dred * 255);
    int green = static_cast<int>(dgreen * 255);
    int blue  = static_cast<int>(dblue * 255);

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
    EqualizeColors(DefaultCushionColors, _countof(DefaultCushionColors), palette);
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
    static constexpr double lz = 10;

    const double len = sqrt(lx * lx + ly * ly + lz * lz);
    m_Lx             = lx / len;
    m_Ly             = ly / len;
    m_Lz             = lz / len;
}

CTreeMap::Options CTreeMap::GetOptions() const
{
    return m_Options;
}

#ifdef _DEBUG
void CTreeMap::RecurseCheckTree(const Item *item)
{
    if(item->TmiIsLeaf())
    {
        ASSERT(item->TmiGetChildCount() == 0);
    }
    else
    {
        ULONGLONG sum = 0;
        ULONGLONG last = static_cast<ULONGLONG>(-1);
        for(int i = 0; i < item->TmiGetChildCount(); i++)
        {
            Item *child = item->TmiGetChild(i);
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

    if (m_Options.grid)
    {
        pdc->FillSolidRect(rc, m_Options.gridColor);
    }
    else
    {
        // We shrink the rectangle here, too.
        // If we didn't do this, the layout of the treemap would
        // change, when grid is switched on and off.
        CPen pen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
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

        // Recursively draw the tree graph
        constexpr double surface[4] = {0, 0, 0, 0};
        const CRect baserc({ 0,0 }, rc.Size());
        RecurseDrawGraph(bitmapBits, root, baserc, true, surface, m_Options.height, 0);

        // Fill the bitmap with the array
        VERIFY(bmp.CreateBitmap(rc.Width(), rc.Height(), 1, 32, bitmapBits.data()));

        // Render bitmap to the temporary CDC
        dcTreeView.SelectObject(&bmp);

        // And lastly, draw the temporary CDC to the real one
        VERIFY(pdc->BitBlt(rc.TopLeft().x, rc.TopLeft().y, rc.Width(), rc.Height(), &dcTreeView, 0, 0, SRCCOPY));

        // Free memory
        VERIFY(bmp.DeleteObject());
        VERIFY(dcTreeView.DeleteDC());

#ifdef STRONGDEBUG  // slow, but finds bugs!
#ifdef _DEBUG
        for(int x = rc.left; x < rc.right - m_Options.grid; x++)
        {
            for(int y = rc.top; y < rc.bottom - m_Options.grid; y++)
            {
                ASSERT(FindItemByPoint(root, CPoint(x, y)) != NULL);
            }
        }
#endif
#endif
    }
    else
    {
        pdc->FillSolidRect(rc, RGB(0, 0, 0));
    }
}

void CTreeMap::DrawTreeMapDoubleBuffered(CDC* pdc, const CRect& rc, Item* root, const Options* options)
{
    if (options != nullptr)
    {
        SetOptions(options);
    }

    if (rc.Width() <= 0 || rc.Height() <= 0)
    {
        return;
    }

    CDC dc;
    VERIFY(dc.CreateCompatibleDC(pdc));

    CBitmap bmp;
    VERIFY(bmp.CreateCompatibleBitmap(pdc, rc.Width(), rc.Height()));

    CSelectObject sobmp(&dc, &bmp);

    const CRect rect(CPoint(0, 0), rc.Size());

    DrawTreeMap(&dc, rect, root);

    VERIFY(pdc->BitBlt(rc.left, rc.top, rc.Width(), rc.Height(), &dc, 0, 0, SRCCOPY));
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
                for(i++; i < item->TmiGetChildCount(); i++)
                {
                    child = item->TmiGetChild(i);

                    if(child->TmiGetSize() == 0)
                    {
                        break;
                    }

                    rcChild = child->TmiGetRectangle();
                    if(rcChild.left == -1)
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

    double surface[4] = {0, 0, 0, 0};
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

void CTreeMap::RecurseDrawGraph(std::vector<COLORREF>& bitmap, Item* item, const CRect& rc,
    const bool asroot, const double* psurface, const double h, const DWORD flags)
{
    ASSERT(rc.Width() >= 0);
    ASSERT(rc.Height() >= 0);

    ASSERT(item->TmiGetSize() > 0);

    item->TmiSetRectangle(rc);

    const int gridWidth = m_Options.grid ? 1 : 0;

    if (rc.Width() <= gridWidth || rc.Height() <= gridWidth)
    {
        return;
    }

    double surface[4] = {0, 0, 0, 0};
    if (IsCushionShading())
    {
        std::copy_n(psurface, _countof(surface), surface);

        if (!asroot)
        {
            AddRidge(rc, surface, h);
        }
    }

    if (item->TmiIsLeaf())
    {
        RenderLeaf(bitmap, item, surface);
    }
    else
    {
        ASSERT(item->TmiGetChildCount() > 0);
        ASSERT(item->TmiGetSize() > 0);

        if (m_Options.style == KDirStatStyle)
        {
            KDirStat_DrawChildren(bitmap, item, surface, h, flags);
        }
        else
        {
            SequoiaView_DrawChildren(bitmap, item, surface, h, flags);
        }
    }
}

// I learned this squarification style from the KDirStat executable.
// It's the most complex one here but also the clearest, imho.
//
void CTreeMap::KDirStat_DrawChildren(std::vector<COLORREF>& bitmap, const Item* parent, const double* surface, const double h, DWORD /*flags*/)
{
    ASSERT(parent->TmiGetChildCount() > 0);

    const CRect& rc = parent->TmiGetRectangle();

    std::vector<double> rows;     // Our rectangle is divided into rows, each of which gets this height (fraction of total height).
    std::vector<int> childrenPerRow; // childrenPerRow[i] = # of children in rows[i]

    std::vector<double> childWidth; // Widths of the children (fraction of row width).
    childWidth.resize(parent->TmiGetChildCount());

    const bool horizontalRows = KDirStat_ArrangeChildren(parent, childWidth, rows, childrenPerRow);

    const int width  = horizontalRows ? rc.Width() : rc.Height();
    const int height = horizontalRows ? rc.Height() : rc.Width();
    ASSERT(width >= 0);
    ASSERT(height >= 0);

    int c = 0;
    double top = horizontalRows ? rc.top : rc.left;
    for (std::size_t row = 0; row < rows.size(); row++)
    {
        const double fBottom = top + rows[row] * height;
        int bottom = static_cast<int>(fBottom);
        if (row == rows.size() - 1)
        {
            bottom = horizontalRows ? rc.bottom : rc.right;
        }
        double left = horizontalRows ? rc.left : rc.top;
        for (int i = 0; i < childrenPerRow[row]; i++, c++)
        {
            Item* child = parent->TmiGetChild(c);
            ASSERT(childWidth[c] >= 0);
            const double fRight = left + childWidth[c] * width;
            int right           = static_cast<int>(fRight);

            const bool lastChild = i == childrenPerRow[row] - 1 || childWidth[c + 1] == 0;

            if (lastChild)
            {
                right = horizontalRows ? rc.right : rc.bottom;
            }

            CRect rcChild;
            if (horizontalRows)
            {
                rcChild.left   = static_cast<int>(left);
                rcChild.right  = right;
                rcChild.top    = static_cast<int>(top);
                rcChild.bottom = bottom;
            }
            else
            {
                rcChild.left   = static_cast<int>(top);
                rcChild.right  = bottom;
                rcChild.top    = static_cast<int>(left);
                rcChild.bottom = right;
            }

#ifdef _DEBUG
            if(rcChild.Width() > 0 && rcChild.Height() > 0)
            {
                CRect test;
                test.IntersectRect(parent->TmiGetRectangle(), rcChild);
                ASSERT(test == rcChild);
            }
#endif

            RecurseDrawGraph(bitmap, child, rcChild, false, surface, h * m_Options.scaleFactor, 0);

            if (lastChild)
            {
                i++;
                c++;

                if (i < childrenPerRow[row])
                {
                    parent->TmiGetChild(c)->TmiSetRectangle(CRect(-1, -1, -1, -1));
                }

                c += childrenPerRow[row] - i;
                break;
            }

            left = fRight;
        }
        // This asserts due to rounding error: ASSERT(left == (horizontalRows ? rc.right : rc.bottom));
        top = fBottom;
    }
    // This asserts due to rounding error: ASSERT(top == (horizontalRows ? rc.bottom : rc.right));
}

// return: whether the rows are horizontal.
//
bool CTreeMap::KDirStat_ArrangeChildren(
    const Item* parent,
    std::vector<double>& childWidth,
    std::vector<double>& rows,
    std::vector<int>& childrenPerRow
) const
{
    ASSERT(!parent->TmiIsLeaf());
    ASSERT(parent->TmiGetChildCount() > 0);

    const auto childCount = parent->TmiGetChildCount();
    if (parent->TmiGetSize() == 0)
    {
        rows.emplace_back(1.0);
        childrenPerRow.emplace_back(childCount);
        std::ranges::fill(childWidth, 1.0 / childCount);
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

    for (int nextChild = 0; nextChild < childCount;)
    {
        int childrenUsed = 0;
        rows.emplace_back(KDirStat_CalculateNextRow(parent, nextChild, width, childrenUsed, childWidth));
        childrenPerRow.emplace_back(childrenUsed);
        nextChild += childrenUsed;
    }

    return horizontalRows;
}

double CTreeMap::KDirStat_CalculateNextRow(const Item* parent, const int nextChild, const double width, int& childrenUsed, std::vector<double>& childWidth) const
{
    static constexpr double _minProportion = 0.4;
    ASSERT(_minProportion < 1.);

    ASSERT(nextChild < parent->TmiGetChildCount());
    ASSERT(width >= 1.0);

    const double mySize = static_cast<double>(parent->TmiGetSize());
    ASSERT(mySize > 0);
    ULONGLONG sizeUsed = 0;
    double rowHeight   = 0;

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
        const double virtualRowHeight = sizeUsed / mySize;
        ASSERT(virtualRowHeight > 0);
        ASSERT(virtualRowHeight <= 1);

        // Rectangle(mySize)    = width * 1.0
        // Rectangle(childSize) = childWidth * virtualRowHeight
        // Rectangle(childSize) = childSize / mySize * width

        const double childWidth_ = childSize / mySize * width / virtualRowHeight;

        if (childWidth_ / virtualRowHeight < _minProportion)
        {
            ASSERT(i > nextChild); // because width >= 1 and _minProportion < 1.
            // For the first child we have:
            // childWidth / rowHeight
            // = childSize / mySize * width / rowHeight / rowHeight
            // = childSize * width / sizeUsed / sizeUsed * mySize
            // > childSize * mySize / sizeUsed / sizeUsed
            // > childSize * childSize / childSize / childSize
            // = 1 > _minProportion.
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
    for (i = 0; i < childrenUsed; i++)
    {
        // Rectangle(1.0 * 1.0) = mySize
        const double rowSize   = mySize * rowHeight;
        const double childSize = static_cast<double>(parent->TmiGetChild(nextChild + i)->TmiGetSize());
        const double cw        = childSize / rowSize;
        ASSERT(cw >= 0);
        childWidth[nextChild + i] = cw;
    }

    return rowHeight;
}

// The classical squarification method.
//
void CTreeMap::SequoiaView_DrawChildren(std::vector<COLORREF>& bitmap, const Item* parent, const double* surface, const double h, DWORD /*flags*/)
{
    // Rest rectangle to fill
    CRect remaining(parent->TmiGetRectangle());

    ASSERT(remaining.Width() > 0);
    ASSERT(remaining.Height() > 0);

    // Size of rest rectangle
    ULONGLONG remainingSize = parent->TmiGetSize();
    ASSERT(remainingSize > 0);

    // Scale factor
    const double sizePerSquarePixel = static_cast<double>(parent->TmiGetSize()) / remaining.Width() / remaining.Height();

    // At least one child left
    for (int head = 0, maxChild = parent->TmiGetChildCount(); head < maxChild;)
    {
        ASSERT(remaining.Width() > 0);
        ASSERT(remaining.Height() > 0);

        // How we divide the remaining rectangle
        const bool horizontal = remaining.Width() >= remaining.Height();

        // Height of the new row
        const int height = horizontal ? remaining.Height() : remaining.Width();

        // Square of height in size scale for ratio formula
        const double hh = height * height * sizePerSquarePixel;
        ASSERT(hh > 0);

        // Row will be made up of child(rowBegin)...child(rowEnd - 1)
        const int rowBegin = head;
        int rowEnd         = head;

        // Worst ratio so far
        double worst = DBL_MAX;

        // Maximum size of children in row
        const ULONGLONG rmax = parent->TmiGetChild(rowBegin)->TmiGetSize();

        // Sum of sizes of children in row
        ULONGLONG sum = 0;

        // This condition will hold at least once.
        while (rowEnd < maxChild)
        {
            // We check a virtual row made up of child(rowBegin)...child(rowEnd) here.

            // Minimum size of child in virtual row
            const ULONGLONG rmin = parent->TmiGetChild(rowEnd)->TmiGetSize();

            // If sizes of the rest of the children is zero, we add all of them
            if (rmin == 0)
            {
                rowEnd = maxChild;
                break;
            }

            // Calculate the worst ratio in virtual row.
            // Formula taken from the "Squarified Treemaps" paper.
            // (https://www.win.tue.nl/~vanwijk/)

            const double ss     = (static_cast<double>(sum) + rmin) * (static_cast<double>(sum) + rmin);
            const double ratio1 = hh * rmax / ss;
            const double ratio2 = ss / hh / rmin;

            const double nextWorst = max(ratio1, ratio2);

            // Will the ratio get worse?
            if (nextWorst > worst)
            {
                // Yes. Don't take the virtual row, but the
                // real row (child(rowBegin)..child(rowEnd - 1))
                // made so far.
                break;
            }

            // Here we have decided to add child(rowEnd) to the row.
            sum += rmin;
            rowEnd++;

            worst = nextWorst;
        }

        // Row will be made up of child(rowBegin)...child(rowEnd - 1).
        // sum is the size of the row.

        // As the size of parent is greater than zero, the size of
        // the first child must have been greater than zero, too.
        ASSERT(sum > 0);

        // Width of row
        int width = horizontal ? remaining.Width() : remaining.Height();
        ASSERT(width > 0);

        if (sum < remainingSize)
            width = static_cast<int>(static_cast<double>(sum) / remainingSize * width);
        // else: use up the whole width
        // width may be 0 here.

        // Build the rectangles of children.
        CRect rc;
        double fBegin;
        if (horizontal)
        {
            rc.left  = remaining.left;
            rc.right = remaining.left + width;
            fBegin   = remaining.top;
        }
        else
        {
            rc.top    = remaining.top;
            rc.bottom = remaining.top + width;
            fBegin    = remaining.left;
        }

        // Now put the children into their places
        for (int i = rowBegin; i < rowEnd; i++)
        {
            const int begin       = static_cast<int>(fBegin);
            const double fraction = static_cast<double>(parent->TmiGetChild(i)->TmiGetSize()) / sum;
            const double fEnd     = fBegin + fraction * height;
            int end               = static_cast<int>(fEnd);

            const bool lastChild = i == rowEnd - 1 || parent->TmiGetChild(i + 1)->TmiGetSize() == 0;

            if (lastChild)
            {
                // Use up the whole height
                end = horizontal ? remaining.top + height : remaining.left + height;
            }

            if (horizontal)
            {
                rc.top    = begin;
                rc.bottom = end;
            }
            else
            {
                rc.left  = begin;
                rc.right = end;
            }

            ASSERT(rc.left <= rc.right);
            ASSERT(rc.top <= rc.bottom);

            ASSERT(rc.left >= remaining.left);
            ASSERT(rc.right <= remaining.right);
            ASSERT(rc.top >= remaining.top);
            ASSERT(rc.bottom <= remaining.bottom);

            RecurseDrawGraph(bitmap, parent->TmiGetChild(i), rc, false, surface, h * m_Options.scaleFactor, 0);

            if (lastChild)
                break;

            fBegin = fEnd;
        }

        // Put the next row into the rest of the rectangle
        if (horizontal)
        {
            remaining.left += width;
        }
        else
        {
            remaining.top += width;
        }

        remainingSize -= sum;

        ASSERT(remaining.left <= remaining.right);
        ASSERT(remaining.top <= remaining.bottom);

        ASSERT(remainingSize >= 0);

        head += rowEnd - rowBegin;

        if (remaining.Width() <= 0 || remaining.Height() <= 0)
        {
            if (head < maxChild)
            {
                parent->TmiGetChild(head)->TmiSetRectangle(CRect(-1, -1, -1, -1));
            }

            break;
        }
    }
    ASSERT(remainingSize == 0);
    ASSERT(remaining.left == remaining.right || remaining.top == remaining.bottom);
}

bool CTreeMap::IsCushionShading() const
{
    return m_Options.ambientLight < 1.0
    && m_Options.height > 0.0
    && m_Options.scaleFactor > 0.0;
}

void CTreeMap::RenderLeaf(std::vector<COLORREF>& bitmap, const Item* item, const double* surface) const
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

void CTreeMap::RenderRectangle(std::vector<COLORREF>& bitmap, const CRect& rc, const double* surface, DWORD color) const
{
    double brightness = m_Options.brightness;

    if ((color & COLORFLAG_MASK) != 0)
    {
        const DWORD flags = color & COLORFLAG_MASK;
        color             = CColorSpace::MakeBrightColor(color, PALETTE_BRIGHTNESS);

        if ((flags & COLORFLAG_DARKER) != 0)
        {
            brightness *= 0.66;
        }
        else
        {
            brightness *= 1.2;
            if (brightness > 1.0)
            {
                brightness = 1.0;
            }
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

void CTreeMap::DrawSolidRect(std::vector<COLORREF>& bitmap, const CRect& rc, const COLORREF col, const double brightness) const
{
    int red   = RGB_GET_RVALUE(col);
    int green = RGB_GET_GVALUE(col);
    int blue  = RGB_GET_BVALUE(col);

    const double factor = brightness / PALETTE_BRIGHTNESS;

    red   = static_cast<int>(red * factor);
    green = static_cast<int>(green * factor);
    blue  = static_cast<int>(blue * factor);

    CColorSpace::NormalizeColor(red, green, blue);

    for (int iy = rc.top; iy < rc.bottom; iy++)
    {
        for (int ix = rc.left; ix < rc.right; ix++)
        {
            bitmap[ix + iy * m_RenderArea.Width()] = BGR(blue, green, red);
        }
    }
}

void CTreeMap::DrawCushion(std::vector<COLORREF>& bitmap, const CRect& rc, const double* surface, const COLORREF col, const double brightness) const
{
    // Cushion parameters
    const double Ia = m_Options.ambientLight;

    // Derived parameters
    const double Is = 1 - Ia; // shading

    const double colR = RGB_GET_RVALUE(col);
    const double colG = RGB_GET_GVALUE(col);
    const double colB = RGB_GET_BVALUE(col);

    for (int iy = rc.top; iy < rc.bottom; iy++)
        for (int ix = rc.left; ix < rc.right; ix++)
        {
            const double nx = -(2 * surface[0] * (ix + 0.5) + surface[2]);
            const double ny = -(2 * surface[1] * (iy + 0.5) + surface[3]);
            double cosa     = (nx * m_Lx + ny * m_Ly + m_Lz) / sqrt(nx * nx + ny * ny + 1.0);
            if (cosa > 1.0)
            {
                cosa = 1.0;
            }

            double pixel = Is * cosa;
            if (pixel < 0)
            {
                pixel = 0;
            }

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
            int red   = static_cast<int>(colR * pixel);
            int green = static_cast<int>(colG * pixel);
            int blue  = static_cast<int>(colB * pixel);

            CColorSpace::NormalizeColor(red, green, blue);

            // ... and set!
            bitmap[ix + iy * m_RenderArea.Width()] = BGR(blue, green, red);
        }
}

void CTreeMap::AddRidge(const CRect& rc, double* surface, const double h)
{
    const int width  = rc.Width();
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

    std::vector<CItem*> c4;
    COLORREF color = GetNextColor(col);
    for (int i = 0; i < 30; i++)
    {
        c4.emplace_back(new CItem(1 + 100 * i, color));
    }

    std::vector<CItem*> c0;
    for (int i = 0; i < 8; i++)
    {
        c0.emplace_back(new CItem(500 + 600 * i, GetNextColor(col)));
    }

    std::vector<CItem*> c1;
    color = GetNextColor(col);
    for (int i = 0; i < 10; i++)
    {
        c1.emplace_back(new CItem(1 + 200 * i, color));
    }
    c0.emplace_back(new CItem(c1));

    std::vector<CItem*> c2;
    color = GetNextColor(col);
    for (int i = 0; i < 160; i++)
    {
        c2.emplace_back(new CItem(1 + i, color));
    }

    std::vector<CItem*> c3;
    c3.emplace_back(new CItem(10000, GetNextColor(col)));
    c3.emplace_back(new CItem(c4));
    c3.emplace_back(new CItem(c2));
    c3.emplace_back(new CItem(6000, GetNextColor(col)));
    c3.emplace_back(new CItem(1500, GetNextColor(col)));

    std::vector<CItem*> c10;
    c10.emplace_back(new CItem(c0));
    c10.emplace_back(new CItem(c3));

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
    m_TreeMap.DrawTreeMapDoubleBuffered(&dc, rc, m_Root);
}
