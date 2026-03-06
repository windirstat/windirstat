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

    if (!m_options.grid)
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

    m_renderArea = rc;

    if (root->TmiGetSize() == 0)
    {
        pdc->FillSolidRect(rc, RGB(0, 0, 0));
        return;
    }

    // Create a temporary CDC that represents only the tree map
    CDC dcTreeView;
    dcTreeView.CreateCompatibleDC(pdc);

    // This bitmap will be blitted onto the temporary DC
    CBitmap bmp;

    // That bitmap in turn will be created from this array
    std::vector<COLORREF> bitmapBits;
    bitmapBits.resize(static_cast<size_t>(rc.Width()) * static_cast<size_t>(rc.Height()));
    COLORREF bgColor = m_options.grid ? m_options.gridColor : RGB(0, 0, 0);
    DrawSolidRect(bitmapBits, CRect(CPoint(), rc.Size()), bgColor, PALETTE_BRIGHTNESS);

    using DrawState = struct DrawState
    {
        std::array<double, 4> surface{};
        CRect rc{};
        CItem* item = nullptr;
        int depth = 0;
        double h = 0.0;
        bool asroot = false;

        DrawState(CItem* item_, const CRect rc_, int depth_, const bool asroot_,
            const std::array<double, 4>& surface_, const double h_)
            : rc(rc_), item(item_), depth(depth_), asroot(asroot_), surface(surface_), h(h_) {
        }
    };

    // Defined at top level to prevent reallocation
    std::vector<double> childWidth;
    std::vector<double> rows;
    std::vector<int> childrenPerRow;

    struct FolderHeader { CRect rc; std::wstring name; COLORREF color; bool isHeaderBar; };
    std::vector<FolderHeader> headersToDraw;
    std::vector<CRect> gridRects;
    std::vector<CRect> folderInnerRects;
    struct HeaderSeparatorLine { CPoint p1; CPoint p2; };
    std::vector<HeaderSeparatorLine> headerSeparators;

    bool showHeaders = m_options.showHeaders;
    std::vector<DrawState> stack;
    stack.emplace_back(root, CRect(0, 0, rc.Width(), rc.Height()), 0, true,
        std::array<double, 4>{}, m_options.height);

    const COLORREF fileTreeColors[] = {
        COptions::FileTreeColor0.Obj(),
        COptions::FileTreeColor1.Obj(),
        COptions::FileTreeColor2.Obj(),
        COptions::FileTreeColor3.Obj(),
        COptions::FileTreeColor4.Obj(),
        COptions::FileTreeColor5.Obj(),
        COptions::FileTreeColor6.Obj(),
        COptions::FileTreeColor7.Obj()
    };
    const int dpiY = pdc->GetDeviceCaps(LOGPIXELSY);
    auto ScaleY = [dpiY](int y) { return MulDiv(y, dpiY, 96); };

    // Main loop
    while (!stack.empty())
    {
        DrawState state = stack.back();
        stack.pop_back();
        CItem* item = state.item;

        if (state.rc.Width() <= 0 || state.rc.Height() <= 0)
        {
            item->TmiSetRectangle(CRect(-1, -1, -1, -1));
            for (int i = 0; i < item->TmiGetChildCount(); ++i)
            {
                item->TmiGetChild(i)->TmiSetRectangle(CRect(-1, -1, -1, -1));
            }
            continue; // Skip to the next item without processing children
        }

        item->TmiSetRectangle(state.rc);

        // Calculate cushion shading geometry. This modifies state.surface,
        // which is then passed to children. Without this, the surface remains
        // flat (0,0,0,0) and shading doesn't work.
        AddRidge(state.rc, state.surface, state.h);

        COLORREF currentColor;
        // Folders always use depth-based coloring (extension colors are only for files)
        if (item->TmiIsLeaf())
        {
            // Use extension-based coloring only for files (original behavior)
            currentColor = item->TmiGetGraphColor();
        }
        else
        {
            // Use depth-based coloring for folders or when DepthColor scheme is selected
            if (state.depth == 0)
            {
                currentColor = RGB(200, 200, 200);
            }
            else
            {
                currentColor = fileTreeColors[(state.depth - 1) % 8];
            }
        }
        double area = (double)state.rc.Width() * state.rc.Height();

        // The 'gridMinimumArea' option acts as a threshold for aggregation.
        // If set to 0, no aggregation occurs (pure WinDirStat style).
        // If set to N, any item with an area less than or equal to N pixels
        // will be rendered as a solid block (SpaceMonger-like aggregation).
        bool forceLeaf = (m_options.gridMinimumArea > 0) && (area <= m_options.gridMinimumArea);

        if (item->TmiIsLeaf() || forceLeaf)
        {
            if (item->TmiIsLeaf()) {
                // Render actual file with cushion shading effect.
                RenderLeaf(bitmapBits, item, state.surface);
            }
            else {
                // Aggregated folder: render as a solid block with depth-based color.
                DrawSolidRect(bitmapBits, state.rc, currentColor, PALETTE_BRIGHTNESS);
            }

            // Add to grid rects only if the element exceeds the minimum area threshold.
            // This prevents drawing borders around very small aggregated items.
            if (m_options.grid && area > m_options.gridMinimumArea)
            {
                gridRects.push_back(state.rc);
            }

            // Queue text label for rendering if item is large enough to be readable.
            if (state.rc.Width() > 60 && state.rc.Height() > 20)
                headersToDraw.push_back({ state.rc, item->GetName(), currentColor, false });

            // Mark children as undrawn to prevent rendering them as separate items.
            if (forceLeaf) {
                for (int i = 0; i < item->TmiGetChildCount(); ++i)
                    item->TmiGetChild(i)->TmiSetRectangle(CRect(-1, -1, -1, -1));
            }
            continue;
        }

        // Fill the entire folder rectangle first. This ensures that border areas
        // created by DeflateRect and margins will have the folder's color rather than
        // appearing as black gaps.
        DrawSolidRect(bitmapBits, state.rc, currentColor, PALETTE_BRIGHTNESS);

        // Draw folder outline only if the folder is large enough.
        if (m_options.grid && area > m_options.gridMinimumArea)
        {
            gridRects.push_back(state.rc);
        }

        int headerHeight = ScaleY(18);

        // Render folder header bar and content if enabled and space allows
        if (showHeaders && state.rc.Height() > headerHeight + 10 && state.rc.Width() > 30)
        {
            CRect headerRc = state.rc;
            headerRc.bottom = headerRc.top + headerHeight;
            // No need to fill the header bar again; the folder rectangle was already filled above.

            // Draw separator line between header and content
            headerSeparators.push_back({
                CPoint(state.rc.left, state.rc.top + headerHeight),
                CPoint(state.rc.right, state.rc.top + headerHeight)
                });

            // Queue header for later text rendering
            headersToDraw.push_back({ headerRc, item->GetName(), currentColor, true });

            // Reduce the space available for child items to create a colored frame border.
            // A 3-pixel margin on left, right, and bottom will remain visible in the folder's color.
            state.rc.top += headerHeight;
            state.rc.DeflateRect(3, 0, 3, 3);

            // Draw inner frame border when headers are enabled.
            if (m_options.grid) folderInnerRects.push_back(state.rc);
        }

        if (m_options.style == KDirStatStyle) [[msvc::flatten]]
        {
            // Reset vectors for next run
            childWidth.clear();
            rows.clear();
            childrenPerRow.clear();

            // KDirStat style preparation
            const bool horizontalRows = KDirStat_ArrangeChildren(item, childWidth, rows, childrenPerRow);
            const double horizontalTop = horizontalRows ? state.rc.top : state.rc.left;

            // Now process the children
            const int width = horizontalRows ? state.rc.Width() : state.rc.Height();
            const int height = horizontalRows ? state.rc.Height() : state.rc.Width();

            double top = horizontalTop;
            size_t c = 0;
            for (const size_t row : std::views::iota(0u, rows.size()))
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
                    CItem* child = item->TmiGetChild(static_cast<int>(c));
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
                    stack.emplace_back(child, rcChild, state.depth + 1, false, state.surface,
                        state.h * m_options.scaleFactor);

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

                for (const int i : std::views::iota(rowBegin, rowEnd))
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
                        stack.emplace_back(item->TmiGetChild(i), rcChild, state.depth + 1, false, state.surface, state.h * m_options.scaleFactor);
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
    bmp.CreateBitmap(rc.Width(), rc.Height(), 1, 32, bitmapBits.data());
    {
        CSelectObject sobmp(&dcTreeView, &bmp);
        pdc->BitBlt(rc.TopLeft().x, rc.TopLeft().y, rc.Width(), rc.Height(), &dcTreeView, 0, 0, SRCCOPY);
    }

    pdc->SetBkMode(TRANSPARENT);
    CFont font;
    font.CreatePointFont(85, L"Segoe UI Bold");
    CSelectObject sofont(pdc, &font);
    CPoint offset = rc.TopLeft();

    // Render text labels (only in flat mode, not when cushion shading is enabled)
    // Text is shown for headers and large items when cushion shading is disabled.
    bool isCushion = IsCushionShading();

    for (auto& h : headersToDraw) {
        // Skip text rendering for file bodies when cushion shading is active.
        // Only header bars will show text.
        if (isCushion && !h.isHeaderBar) {
            continue;
        }

        pdc->SetTextColor(RGB(0, 0, 0));
        CRect textRc = h.rc;
        textRc.OffsetRect(offset);
        textRc.DeflateRect(4, 1);

        pdc->DrawText(h.name.c_str(), (int)h.name.length(), &textRc,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    if (m_options.grid)
    {
        CPen gridPen(PS_SOLID, 1, RGB(0, 0, 0));
        CSelectObject sogrid(pdc, &gridPen);

        pdc->MoveTo(m_renderArea.left, m_renderArea.bottom);
        pdc->LineTo(m_renderArea.left, m_renderArea.top);
        pdc->LineTo(m_renderArea.right, m_renderArea.top);

        for (const auto& rect : gridRects)
        {
            CRect r = rect;
            r.OffsetRect(offset);
            pdc->MoveTo(r.left, r.bottom);
            pdc->LineTo(r.right, r.bottom);
            pdc->LineTo(r.right, r.top);
        }

        if (showHeaders)
        {
            for (const auto& line : headerSeparators)
            {
                pdc->MoveTo(line.p1 + offset);
                pdc->LineTo(line.p2 + offset);
            }

            CSelectStockObject sobrush(pdc, NULL_BRUSH);
            for (const auto& iRect : folderInnerRects)
            {
                CRect r = iRect;
                r.OffsetRect(offset);
                pdc->Rectangle(&r);
            }
        }
    }
}

CItem* CTreeMap::FindItemByPoint(CItem* item, const CPoint point)
{
    ASSERT(item != nullptr);
    const CRect& rc = item->TmiGetRectangle();

    if (!rc.PtInRect(point))
    {
        return nullptr;
    }

    const int gridWidth = m_options.grid ? 1 : 0;

    // Return the item if it's a leaf, too small to subdivide, or an aggregated folder
    // that wasn't subdivided due to the minimum area threshold.
    if (rc.Width() <= gridWidth || rc.Height() <= gridWidth || item->TmiIsLeaf())
    {
        return item;
    }
    else
    {
        CItem* ret = nullptr;
        for (const int i : std::views::iota(0, item->TmiGetChildCount()))
        {
            CItem* child = item->TmiGetChild(i);
            const CRect& rcChild = child->TmiGetRectangle();

            // Skip children that were not rendered (uninitialized rectangle),
            // indicating they were aggregated due to the minimum area threshold.
            if (rcChild.left == -1 || !rcChild.PtInRect(point))
                continue;

            ret = FindItemByPoint(child, point);
            if (ret) break;
        }

        // If no child claimed the point (because they weren't rendered),
        // return the folder itself.
        return ret ? ret : item;
    }
}

void CTreeMap::DrawColorPreview(CDC* pdc, const CRect& rc, const COLORREF color, const Options* options)
{
    if (options != nullptr)
    {
        SetOptions(options);
    }

    std::array<double, 4> surface{};
    AddRidge(rc, surface, m_options.height * m_options.scaleFactor);

    m_renderArea = rc;

    // Create a temporary CDC that represents only the tree map
    CDC dcTreeView;
    dcTreeView.CreateCompatibleDC(pdc);

    // This bitmap will be blitted onto the temporary DC
    CBitmap bmp;

    // That bitmap in turn will be created from this array
    std::vector<COLORREF> bitmapBits;
    bitmapBits.resize(static_cast<size_t>(rc.Width()) * static_cast<size_t>(rc.Height()));

    // Recursively draw the tree graph
    RenderRectangle(bitmapBits, CRect(0, 0, rc.Width(), rc.Height()), surface, color);

    // Fill the bitmap with the array
    bmp.CreateBitmap(rc.Width(), rc.Height(), 1, 32, bitmapBits.data());
    {
        CSelectObject sobmp(&dcTreeView, &bmp);

        // Render bitmap to the temporary CDC
        pdc->BitBlt(rc.TopLeft().x, rc.TopLeft().y, rc.Width(), rc.Height(), &dcTreeView, 0, 0, SRCCOPY);
    }

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
    double brightness = m_options.brightness;

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
    const CItem* parent,
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
    const double factor = brightness / PALETTE_BRIGHTNESS;

    auto red = static_cast<int>(GetRValue(col) * factor);
    auto green = static_cast<int>(GetGValue(col) * factor);
    auto blue = static_cast<int>(GetBValue(col) * factor);

    CColorSpace::NormalizeColor(red, green, blue);

    for (const int iy : std::views::iota(rc.top, rc.bottom))
    {
        const auto rowStart = bitmap.begin() + (iy * m_renderArea.Width()) + rc.left;
        std::fill_n(rowStart, rc.Width(), BGR(blue, green, red));
    }
}

void CTreeMap::DrawCushion(std::vector<COLORREF>& bitmap, const CRect& rc, const std::array<double, 4>& surface, const COLORREF col, const double brightness) const
{
    // Cushion parameters
    const double Ia = m_options.ambientLight;

    // Derived parameters
    const double Is = 1 - Ia; // shading

    const double colR = GetRValue(col);
    const double colG = GetGValue(col);
    const double colB = GetBValue(col);

    for (const int iy : std::views::iota(rc.top, rc.bottom))
        for (const int ix : std::views::iota(rc.left, rc.right))
    {
        const double nx = -(2 * surface[0] * (ix + 0.5) + surface[2]);
        const double ny = -(2 * surface[1] * (iy + 0.5) + surface[3]);
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

        // Apply "brightness"
        pixel *= brightness / PALETTE_BRIGHTNESS;

        // Make color value
        int red = static_cast<int>(colR * pixel);
        int green = static_cast<int>(colG * pixel);
        int blue = static_cast<int>(colB * pixel);

        CColorSpace::NormalizeColor(red, green, blue);

        // ... and set!
        bitmap[ix + iy * m_renderArea.Width()] = BGR(blue, green, red);
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
