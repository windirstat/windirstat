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

static constexpr COLORREF DimColor(COLORREF rgb, float factor = 0.9f) noexcept
{
    factor = std::clamp(factor, 0.0f, 1.0f);
    return RGB(
        static_cast<BYTE>(GetRValue(rgb) * factor),
        static_cast<BYTE>(GetGValue(rgb) * factor),
        static_cast<BYTE>(GetBValue(rgb) * factor)
    );
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
    int depth = 0;
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
const std::array<CPoint, 4> EXTENSION_SHADOW_OFFSETS = {
    CPoint(0, -1),
    CPoint(-1, 0),
    CPoint(1, 0),
    CPoint(0, 1)
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

static COLORREF GetDepthColor(int depth) noexcept
{
    static constexpr std::array<COLORREF, 7> palette = {
        RGB(240, 128, 128), // Light Coral (Red)
        RGB(244, 200, 120), // Tan / Light Orange
        RGB(250, 250, 160), // Light Yellow
        RGB(160, 240, 160), // Light Green
        RGB(160, 240, 240), // Light Cyan
        RGB(160, 160, 240), // Light Blue
        RGB(240, 160, 240)  // Light Magenta
    };
    return (depth <= 0) ? RGB(200, 200, 200) : palette[static_cast<std::size_t>(depth - 1) % palette.size()];
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
    // Use SetDIBitsToDevice for compatibility with Remote Desktop at <32-bit depth
    const BITMAPINFO bmi{ .bmiHeader = { .biSize = sizeof(BITMAPINFOHEADER),
        .biWidth = rc.Width(), .biHeight = -rc.Height(), .biPlanes = 1,
        .biBitCount = 32, .biCompression = BI_RGB } };
    ::SetDIBitsToDevice(pdc->GetSafeHdc(), rc.left, rc.top, rc.Width(), rc.Height(),
        0, 0, 0, rc.Height(), bitmapBits.data(), &bmi, DIB_RGB_COLORS);
}

[[nodiscard]] bool CanDrawExtensionLabel(const CRect& rc, const CSize& textSize)
{
    return textSize.cx + EXTENSION_TEXT_PADDING <= rc.Width()
        && textSize.cy + EXTENSION_TEXT_PADDING <= rc.Height();
}

void DrawShadowedExtensionText(CDC* const pdc, const std::wstring_view text, const CRect& rc)
{
    CSaveDC saveDc(pdc);
    pdc->IntersectClipRect(rc);

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
}

/////////////////////////////////////////////////////////////////////////////

void CTreeMap::GetDefaultPalette(std::vector<COLORREF>& palette)
{
    palette.resize(std::size(DefaultCushionColors));
    std::ranges::transform(DefaultCushionColors, palette.begin(),
        [](const COLORREF color) { return CColorSpace::MakeBrightColor(color, PALETTE_BRIGHTNESS); });
}

std::unique_ptr<CItem> CTreeMap::BuildDemoTree()
{
    [[msvc::noinline_calls]]
    {
        std::vector<COLORREF> colors;
        GetDefaultPalette(colors);
        int colorIndex = -1;

        auto getNextColor = [&colors, &colorIndex]
        {
            ++colorIndex;
            colorIndex %= static_cast<int>(colors.size());
            return colors[colorIndex];
        };

        auto createLeaf = [](const int size, const COLORREF color) -> CItem*
        {
            const auto item = new CItem(IT_FILE | ITF_PREVIEW, L"");
            item->SetSizePhysical(size);
            item->SetSizeLogical(size);
            item->SetIndex(color);
            return item;
        };

        auto createContainer = [](std::vector<CItem*>& children) -> CItem*
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
        COLORREF color = getNextColor();
        for (const int i : std::views::iota(0, c4Items))
        {
            c4.emplace_back(createLeaf(1 + 100 * i, color));
        }

        constexpr auto c0Items = 8;
        std::vector<CItem*> c0;
        c0.reserve(c0Items);
        for (const int i : std::views::iota(0, c0Items))
        {
            c0.emplace_back(createLeaf(500 + 600 * i, getNextColor()));
        }

        constexpr auto c1Items = 10;
        std::vector<CItem*> c1;
        c1.reserve(c1Items);
        color = getNextColor();
        for (const int i : std::views::iota(0, c1Items))
        {
            c1.emplace_back(createLeaf(1 + 200 * i, color));
        }
        c0.emplace_back(createContainer(c1));

        constexpr auto c2Items = 160;
        std::vector<CItem*> c2;
        c2.reserve(c2Items);
        color = getNextColor();
        for (const int i : std::views::iota(0, c2Items))
        {
            c2.emplace_back(createLeaf(1 + i, color));
        }

        std::vector<CItem*> c3;
        c3.reserve(5);
        c3.emplace_back(createLeaf(10000, getNextColor()));
        c3.emplace_back(createContainer(c4));
        c3.emplace_back(createContainer(c2));
        c3.emplace_back(createLeaf(6000, getNextColor()));
        c3.emplace_back(createLeaf(1500, getNextColor()));

        std::vector<CItem*> c10;
        c10.reserve(2);
        c10.emplace_back(createContainer(c0));
        c10.emplace_back(createContainer(c3));

        return std::unique_ptr<CItem>(createContainer(c10));
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

void CTreeMap::ClearLayout()
{
    m_layoutRoot = nullptr;
    m_layoutArea.SetRectEmpty();
    m_hitTestColumns = 0;
    m_hitTestRows = 0;
    m_visibleItems.clear();
    m_itemToVisibleIndex.clear();
    m_hitTestCellOffsets.clear();
    m_hitTestEntries.clear();
}

void CTreeMap::TrimMemory()
{
    ClearLayout();
    decltype(m_visibleItems){}.swap(m_visibleItems);
    decltype(m_itemToVisibleIndex){}.swap(m_itemToVisibleIndex);
    decltype(m_hitTestCellOffsets){}.swap(m_hitTestCellOffsets);
    decltype(m_hitTestEntries){}.swap(m_hitTestEntries);
    decltype(m_bitmapBits){}.swap(m_bitmapBits);
}

void CTreeMap::AddVisibleItem(CItem* const item, const CRect& rectangle, const int depth)
{
    if (item == nullptr || rectangle.Width() <= 0 || rectangle.Height() <= 0) return;

    const std::size_t index = m_visibleItems.size();
    const auto [iterator, inserted] = m_itemToVisibleIndex.try_emplace(item, index);
    ASSERT(inserted);
    if (!inserted)
    {
        m_visibleItems[iterator->second] = { item, rectangle, depth };
        return;
    }

    m_visibleItems.push_back({ item, rectangle, depth });
}

void CTreeMap::BuildHitTestIndex()
{
    m_hitTestCellOffsets.clear();
    m_hitTestEntries.clear();
    m_hitTestColumns = 0;
    m_hitTestRows = 0;
    if (m_layoutArea.IsRectEmpty() || m_visibleItems.empty()) return;

    m_hitTestColumns = (m_layoutArea.Width() + HitTestCellSize - 1) / HitTestCellSize;
    m_hitTestRows = (m_layoutArea.Height() + HitTestCellSize - 1) / HitTestCellSize;
    const std::size_t cellCount = static_cast<std::size_t>(m_hitTestColumns)
        * static_cast<std::size_t>(m_hitTestRows);
    m_hitTestCellOffsets.assign(cellCount + 1, 0);

    const auto visitCells = [this](const CRect& rectangle, auto&& visitor)
    {
        CRect clipped;
        if (!clipped.IntersectRect(rectangle, m_layoutArea)) return;

        const int firstColumn = (clipped.left - m_layoutArea.left) / HitTestCellSize;
        const int lastColumn = (clipped.right - 1 - m_layoutArea.left) / HitTestCellSize;
        const int firstRow = (clipped.top - m_layoutArea.top) / HitTestCellSize;
        const int lastRow = (clipped.bottom - 1 - m_layoutArea.top) / HitTestCellSize;

        for (const int row : std::views::iota(firstRow, lastRow + 1))
        {
            for (const int column : std::views::iota(firstColumn, lastColumn + 1))
            {
                std::invoke(visitor,
                    static_cast<std::size_t>(row) * m_hitTestColumns + column);
            }
        }
    };

    struct IndexedRegion
    {
        std::size_t visibleIndex;
        CRect rectangle;
    };

    // Index only the area owned exclusively by a nonterminal item. In normal
    // layouts direct children tile one rectangular inset, so indexing every
    // ancestor's full rectangle would multiply storage by hierarchy depth.
    std::vector<CRect> childBounds(m_visibleItems.size());
    std::vector<ULONGLONG> childAreas(m_visibleItems.size(), 0);
    std::vector<bool> hasVisibleChildren(m_visibleItems.size(), false);
    for (const VisibleItem& child : m_visibleItems)
    {
        const CItem* const parent = child.item->GetParent();
        const auto parentFound = m_itemToVisibleIndex.find(parent);
        if (parentFound == m_itemToVisibleIndex.end()) continue;

        const std::size_t parentIndex = parentFound->second;
        if (hasVisibleChildren[parentIndex])
        {
            CRect combined;
            combined.UnionRect(childBounds[parentIndex], child.rectangle);
            childBounds[parentIndex] = combined;
        }
        else
        {
            childBounds[parentIndex] = child.rectangle;
            hasVisibleChildren[parentIndex] = true;
        }
        childAreas[parentIndex] += static_cast<ULONGLONG>(child.rectangle.Width())
            * static_cast<ULONGLONG>(child.rectangle.Height());
    }

    std::vector<IndexedRegion> indexedRegions;
    indexedRegions.reserve(m_visibleItems.size() * 2);
    const auto addRegion = [&indexedRegions](const std::size_t index, const CRect rectangle)
    {
        if (rectangle.Width() > 0 && rectangle.Height() > 0)
            indexedRegions.push_back({ index, rectangle });
    };

    for (const std::size_t index : std::views::iota(std::size_t{ 0 }, m_visibleItems.size()))
    {
        const CRect outer = m_visibleItems[index].rectangle;
        if (!hasVisibleChildren[index])
        {
            addRegion(index, outer);
            continue;
        }

        const CRect inner = childBounds[index];
        const bool contained = outer.left <= inner.left && inner.right <= outer.right
            && outer.top <= inner.top && inner.bottom <= outer.bottom;
        const ULONGLONG boundingArea = static_cast<ULONGLONG>(inner.Width())
            * static_cast<ULONGLONG>(inner.Height());
        if (!contained || childAreas[index] != boundingArea)
        {
            // Preserve hit correctness if release data violates the normal
            // rectangular child partition invariant.
            addRegion(index, outer);
            continue;
        }

        addRegion(index, CRect(outer.left, outer.top, outer.right, inner.top));
        addRegion(index, CRect(outer.left, inner.bottom, outer.right, outer.bottom));
        addRegion(index, CRect(outer.left, inner.top, inner.left, inner.bottom));
        addRegion(index, CRect(inner.right, inner.top, outer.right, inner.bottom));
    }

    for (const IndexedRegion& region : indexedRegions)
    {
        visitCells(region.rectangle, [this](const std::size_t cell)
        {
            ++m_hitTestCellOffsets[cell + 1];
        });
    }

    for (std::size_t cell = 1; cell < m_hitTestCellOffsets.size(); ++cell)
    {
        m_hitTestCellOffsets[cell] += m_hitTestCellOffsets[cell - 1];
    }

    std::vector<std::size_t> nextEntry = m_hitTestCellOffsets;
    m_hitTestEntries.resize(m_hitTestCellOffsets.back());
    for (const IndexedRegion& region : indexedRegions)
    {
        visitCells(region.rectangle,
            [&nextEntry, this, index = region.visibleIndex](const std::size_t cell)
        {
            m_hitTestEntries[nextEntry[cell]++] = index;
        });
    }
}

bool CTreeMap::HasValidLayout(const CItem* const root) const
{
    return root != nullptr && root == m_layoutRoot
        && m_itemToVisibleIndex.contains(root) && !m_hitTestCellOffsets.empty();
}

bool CTreeMap::TryGetItemRectangle(const CItem* const item, CRect& rectangle) const
{
    const auto found = m_itemToVisibleIndex.find(item);
    if (found == m_itemToVisibleIndex.end()) return false;

    rectangle = m_visibleItems[found->second].rectangle;
    return true;
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
    ClearLayout();

    // Validate parameters and options
    ASSERT(pdc != nullptr && root != nullptr);
    if (pdc == nullptr || root == nullptr)
    {
        // Parameter check fallback
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

    CSelectStockObject soFont(pdc, DEFAULT_GUI_FONT);

    TEXTMETRIC tm{};
    pdc->GetTextMetrics(&tm);
    const int headerHeight = tm.tmHeight + 2;

    const int renderWidth = rc.Width();
    const int renderHeight = rc.Height();
    m_layoutRoot = root;
    m_layoutArea = CRect(0, 0, renderWidth, renderHeight);

    if (root->TmiGetSize() == 0)
    {
        pdc->FillSolidRect(rc, RGB(0, 0, 0));
        AddVisibleItem(root, m_layoutArea, 0);
        BuildHitTestIndex();
        return;
    }

    const size_t pixelCount = static_cast<size_t>(renderWidth) * static_cast<size_t>(renderHeight);
    BitmapView bitmap{};
    bool rendersIntoDc = false;
    DIBSECTION dibSection{};
    const HGDIOBJ selectedBitmap = ::GetCurrentObject(pdc->GetSafeHdc(), OBJ_BITMAP);
    if (selectedBitmap != nullptr
        && ::GetObject(selectedBitmap, sizeof(dibSection), &dibSection) == sizeof(dibSection)
        && dibSection.dsBm.bmBits != nullptr && dibSection.dsBm.bmBitsPixel == 32
        && dibSection.dsBmih.biHeight < 0 && rc.left >= 0 && rc.top >= 0
        && rc.right <= dibSection.dsBm.bmWidth && rc.bottom <= dibSection.dsBm.bmHeight)
    {
        ::GdiFlush();
        bitmap.stride = static_cast<size_t>(dibSection.dsBm.bmWidthBytes) / sizeof(COLORREF);
        bitmap.bits = static_cast<COLORREF*>(dibSection.dsBm.bmBits)
            + static_cast<size_t>(rc.top) * bitmap.stride + rc.left;
        rendersIntoDc = true;
    }
    else
    {
        m_bitmapBits.resize(pixelCount);
        bitmap = { m_bitmapBits.data(), static_cast<size_t>(renderWidth) };
    }
    DrawSolidRect(bitmap, CRect(0, 0, renderWidth, renderHeight),
        m_options.gridColor, PALETTE_BRIGHTNESS);

    const int gridWidth = m_options.grid ? 1 : 0;
    const bool cushionShading = IsCushionShading();

    LayoutScratch layoutScratch;
    std::vector<DrawStateInfo> stack;
    stack.reserve(128);
    stack.push_back({ {}, CRect(0, 0, renderWidth, renderHeight), root, m_options.height, true, 0 });

    struct FolderDrawInfo
    {
        const CItem* item;
        CRect rc;
        int depth;
        bool showHeader;
    };
    std::vector<FolderDrawInfo> foldersToDraw;
    foldersToDraw.reserve(128);

    auto pushChildState = [this, &stack](CItem* child, const CRect& childRect, const DrawStateInfo& parentState)
    {
        stack.push_back({ parentState.surface, childRect, child,
            parentState.ridgeHeight * m_options.scaleFactor, false, parentState.depth + 1 });
    };

    // Lay out children using KDirStat style
    auto pushKDirStatChildren = [this, &layoutScratch, &pushChildState](CItem* item, const DrawStateInfo& state)
    {
        const bool horizontalRows =
            KDirStat_ArrangeChildren(item, state.rc, layoutScratch.childWidth,
                layoutScratch.rows, layoutScratch.childrenPerRow);

        const double rowOrigin = horizontalRows ? state.rc.top : state.rc.left;
        const int rowExtent = horizontalRows ? state.rc.Height() : state.rc.Width();
        const int columnExtent = horizontalRows ? state.rc.Width() : state.rc.Height();

        double top = rowOrigin;
        size_t childIndex = 0;
        for (const size_t rowIndex : std::views::iota(0u, layoutScratch.rows.size()))
        {
            const double fBottom = top + layoutScratch.rows[rowIndex] * rowExtent;
            const int bottom = (rowIndex + 1 == layoutScratch.rows.size())
                ? (horizontalRows ? state.rc.bottom : state.rc.right)
                : static_cast<int>(fBottom);

            double left = horizontalRows ? state.rc.left : state.rc.top;
            for (int childInRow = 0; childInRow < layoutScratch.childrenPerRow[rowIndex]; ++childInRow, ++childIndex)
            {
                CItem* child = item->TmiGetChild(static_cast<int>(childIndex));
                const double fRight = left + layoutScratch.childWidth[childIndex] * columnExtent;
                const bool lastChild = childInRow + 1 == layoutScratch.childrenPerRow[rowIndex]
                    || (childIndex + 1 < layoutScratch.childWidth.size() && layoutScratch.childWidth[childIndex + 1] == 0.0);
                const int right = lastChild
                    ? (horizontalRows ? state.rc.right : state.rc.bottom)
                    : static_cast<int>(fRight);

                const CRect rcChild = horizontalRows
                    ? CRect(static_cast<int>(left), static_cast<int>(top), right, bottom)
                    : CRect(static_cast<int>(top), static_cast<int>(left), bottom, right);

                pushChildState(child, rcChild, state);
                left = fRight;
            }

            top = fBottom;
        }
    };

    // Lay out children using SequoiaView style
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
            ASSERT(remaining.Width() > 0 && remaining.Height() > 0);

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
                const double nextWorst = std::max(hh * rmax / ss, ss / hh / childSize);

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
                break;
            }

            const int remainingExtent = horizontal ? remaining.Width() : remaining.Height();
            const int rowWidth = (sum < remainingSize)
                ? std::clamp(static_cast<int>(static_cast<double>(sum) / remainingSize * remainingExtent), 1, remainingExtent)
                : remainingExtent;

            CRect rcRow = remaining;
            if (horizontal) rcRow.right = rcRow.left + rowWidth;
            else rcRow.bottom = rcRow.top + rowWidth;

            double fBegin = horizontal ? rcRow.top : rcRow.left;
            for (const int i : std::views::iota(rowBegin, rowEnd))
            {
                const ULONGLONG childSize = item->TmiGetChild(i)->TmiGetSize();
                const double fraction = static_cast<double>(childSize) / sum;
                const double fEnd = fBegin + fraction * (horizontal ? rcRow.Height() : rcRow.Width());

                const bool lastChild = i + 1 == rowEnd
                    || (i + 1 < item->TmiGetChildCount() && item->TmiGetChild(i + 1)->TmiGetSize() == 0);
                const int end = lastChild
                    ? (horizontal ? rcRow.bottom : rcRow.right)
                    : static_cast<int>(fEnd);

                const CRect rcChild = horizontal
                    ? CRect(rcRow.left, static_cast<int>(fBegin), rcRow.right, end)
                    : CRect(static_cast<int>(fBegin), rcRow.top, end, rcRow.bottom);

                pushChildState(item->TmiGetChild(i), rcChild, state);
                fBegin = fEnd;
            }

            (horizontal ? remaining.left : remaining.top) += rowWidth;
            remainingSize -= sum;
            head += rowEnd - rowBegin;

            if (remaining.Width() <= 0 || remaining.Height() <= 0)
            {
                break;
            }
        }
    };

    // Main layout loop
    while (!stack.empty())
    {
        DrawStateInfo state = stack.back();
        stack.pop_back();

        CItem* const item = state.item;
        AddVisibleItem(item, state.rc, state.depth);

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
            RenderLeaf(bitmap, item, state.rc, state.surface);
            continue;
        }

        // Draw folder frames and headers if the rectangle is large enough
        if (m_options.showFolderFrames && !state.asRoot &&
            std::min(state.rc.Width(), state.rc.Height()) >= m_options.folderFramesDrawThreshold)
        {
            std::wstring_view name = item->GetNameView(true);
            const int textWidth = state.rc.Width() - 8;
            const bool showHeader = (state.rc.Height() > headerHeight) &&
                (pdc->GetTextExtent(name.data(), static_cast<int>(name.size())).cx <= textWidth);

            foldersToDraw.push_back({ item, state.rc, state.depth, showHeader });
            state.rc.left += 1;
            state.rc.right -= 1;
            state.rc.bottom -= 1;
            state.rc.top += showHeader ? headerHeight : 1;

            if (state.rc.Width() <= gridWidth || state.rc.Height() <= gridWidth)
            {
                continue;
            }
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

    BuildHitTestIndex();

    if (!rendersIntoDc) BlitBitmap(pdc, rc, m_bitmapBits);

    // Render directory frames and labels
    if (m_options.showFolderFrames)
    {
        CSetBkMode soBkMode(pdc, TRANSPARENT);
        const CPoint rcOffset = rc.TopLeft();

        for (const auto& folder : foldersToDraw)
        {
            CRect rcFolder = folder.rc;
            rcFolder.OffsetRect(rcOffset);

            if (rcFolder.Width() > 2 && rcFolder.Height() > 2)
            {
                CBrush borderBrush(DimColor(GetDepthColor(folder.depth)));
                pdc->FrameRect(&rcFolder, &borderBrush);

                if (folder.showHeader)
                {
                    CRect rcHeader(rcFolder.left + 1, rcFolder.top + 1, rcFolder.right - 1, rcFolder.top + headerHeight);
                    const COLORREF headerColor = GetDepthColor(folder.depth);
                    pdc->FillSolidRect(&rcHeader, headerColor);

                    CRect rcText(rcHeader.left + 3, rcHeader.top, rcHeader.right - 3, rcHeader.bottom);
                    std::wstring_view name = folder.item->GetNameView(true);
                    pdc->SetTextColor(RGB(0, 0, 0));
                    pdc->DrawText(name.data(), static_cast<int>(name.size()), &rcText,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                }
            }
        }
    }

    if (m_options.showExtensions)
    {
        DrawTreeMapLabels(pdc, rc.TopLeft());
    }
}

CItem* CTreeMap::FindItemByPoint(CItem* item, const CPoint point) const
{
    if (item == nullptr || !m_layoutArea.PtInRect(point)
        || m_hitTestColumns <= 0 || m_hitTestRows <= 0) return nullptr;

    const auto start = m_itemToVisibleIndex.find(item);
    if (start == m_itemToVisibleIndex.end()
        || !m_visibleItems[start->second].rectangle.PtInRect(point)) return nullptr;

    const int column = (point.x - m_layoutArea.left) / HitTestCellSize;
    const int row = (point.y - m_layoutArea.top) / HitTestCellSize;
    if (column < 0 || column >= m_hitTestColumns || row < 0 || row >= m_hitTestRows)
        return nullptr;

    CItem* bestItem = item;
    int bestDepth = m_visibleItems[start->second].depth;
    const std::size_t cell = static_cast<std::size_t>(row) * m_hitTestColumns + column;
    const std::size_t begin = m_hitTestCellOffsets[cell];
    const std::size_t end = m_hitTestCellOffsets[cell + 1];
    for (const std::size_t entry : std::views::iota(begin, end))
    {
        const std::size_t candidateIndex = m_hitTestEntries[entry];
        const VisibleItem& candidate = m_visibleItems[candidateIndex];
        if (candidate.depth > bestDepth && candidate.rectangle.PtInRect(point))
        {
            bestItem = candidate.item;
            bestDepth = candidate.depth;
        }
    }

    return bestItem;
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

    const CRect local(0, 0, rc.Width(), rc.Height());
    Surface surface{};
    AddRidge(local, surface, m_options.height * m_options.scaleFactor);

    m_bitmapBits.resize(static_cast<size_t>(rc.Width()) * static_cast<size_t>(rc.Height()));
    const BitmapView bitmap{ m_bitmapBits.data(), static_cast<size_t>(rc.Width()) };
    RenderRectangle(bitmap, local, surface, color);

    if (CSaveDC saveDc(pdc); true)
    {
        CRgn rgn;
        rgn.CreateRoundRectRgn(rc.left, rc.top, rc.right, rc.bottom, 3, 3);
        pdc->SelectClipRgn(&rgn, RGN_AND);
        BlitBitmap(pdc, rc, m_bitmapBits);
    }

    if (m_options.grid)
    {
        pdc->SetDCPenColor(m_options.gridColor);
        CSelectStockObject sp(pdc, DC_PEN);
        CSelectStockObject sb(pdc, NULL_BRUSH);
        pdc->RoundRect(rc, CPoint(3, 3));
    }
}

void CTreeMap::RenderLeaf(const BitmapView bitmap, const CItem* item,
    const CRect& rectangle, const std::array<double, 4>& surface) const
{
    CRect rc = rectangle;

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

void CTreeMap::RenderRectangle(const BitmapView bitmap, const CRect& rc, const std::array<double, 4>& surface, DWORD color) const
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
    const CRect& parentRect,
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
        ASSERT(virtualRowHeight > 0 && virtualRowHeight <= 1);

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

void CTreeMap::DrawSolidRect(const BitmapView bitmap, const CRect& rc, const COLORREF col, const double brightness) const
{
    const COLORREF pixelColor = MakeBitmapColor(col, brightness);
    const size_t stride = bitmap.stride;
    const size_t width = static_cast<size_t>(rc.Width());
    if (rc.left == 0 && width == stride)
    {
        std::fill_n(bitmap.bits + static_cast<size_t>(rc.top) * stride,
            static_cast<size_t>(rc.Height()) * stride, pixelColor);
        return;
    }

    for (int iy = rc.top; iy < rc.bottom; ++iy)
    {
        std::fill_n(bitmap.bits + static_cast<size_t>(iy) * stride + rc.left,
            width, pixelColor);
    }
}

void CTreeMap::DrawCushion(const BitmapView bitmap, const CRect& rc, const std::array<double, 4>& surface, const COLORREF col, const double brightness) const
{
    const double Ia = m_options.ambientLight;
    const double Is = 1 - Ia;
    const double brightnessFactor = brightness / PALETTE_BRIGHTNESS;

    const double colR = GetRValue(col);
    const double colG = GetGValue(col);
    const double colB = GetBValue(col);
    const size_t stride = bitmap.stride;
    const double nxStep = -2 * surface[0];

    const auto drawRow = [this, &bitmap, &rc, &surface, Ia, Is, brightnessFactor,
        colR, colG, colB, stride, nxStep](const int iy)
    {
        const double ny = -(2 * surface[1] * (iy + 0.5) + surface[3]);
        const double ny_ly_lz = ny * m_ly + m_lz;
        const double ny2_1 = ny * ny + 1.0;
        COLORREF* const row = bitmap.bits + static_cast<size_t>(iy) * stride;
        double nx = -(2 * surface[0] * (rc.left + 0.5) + surface[2]);

        for (int ix = rc.left; ix < rc.right; ++ix)
        {
            double cosa = (nx * m_lx + ny_ly_lz) / sqrt(nx * nx + ny2_1);
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
            nx += nxStep;
        }
    };

    const std::size_t pixelCount = static_cast<std::size_t>(rc.Width())
        * static_cast<std::size_t>(rc.Height());
    const auto rows = std::views::iota(rc.top, rc.bottom);
    if (pixelCount >= 512u * 1024u && rc.Width() >= 256 && rc.Height() >= 64)
    {
        // MSVC's parallel algorithms use a bounded shared scheduler. Restrict
        // dispatch to large cushions so normal layouts avoid scheduling cost.
        std::for_each(std::execution::par, rows.begin(), rows.end(), drawRow);
    }
    else
    {
        std::ranges::for_each(rows, drawRow);
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

void CTreeMap::DrawTreeMapLabels(CDC* pdc, const CPoint& offset) const
{
    ASSERT(pdc != nullptr);
    if (pdc == nullptr) return;

    CSelectStockObject soFont(pdc, DEFAULT_GUI_FONT);
    CSetBkMode soBkMode(pdc, TRANSPARENT);

    std::unordered_map<std::wstring, CSize> textExtentCache;

    for (const VisibleItem& visible : m_visibleItems)
    {
        const CItem* item = visible.item;
        if (!item->TmiIsLeaf()) continue;

        CRect rc = visible.rectangle;
        rc.OffsetRect(offset);

        // Fast size check to avoid string copies, lowercasing, and caching for tiny cushions
        if (rc.Height() < 16 || rc.Width() < 16) continue;

        std::wstring label = m_options.showExtensions ? item->GetExtension() : L"";

        if (label.empty()) continue;

        auto [cacheIt, inserted] = textExtentCache.try_emplace(label);
        if (inserted)
        {
            ::GetTextExtentPoint32(pdc->GetSafeHdc(), label.c_str(),
                static_cast<int>(label.size()), &cacheIt->second);
        }
        if (!CanDrawExtensionLabel(rc, cacheIt->second)) continue;

        DrawShadowedExtensionText(pdc, label, rc);
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
    m_root = CTreeMap::BuildDemoTree().release();
}

void CTreeMapPreview::OnPaint()
{
    CPaintDC dc(this);
    const CRect rc = ClientRectOf(this);
    m_treeMap.DrawTreeMap(&dc, rc, m_root);
}
