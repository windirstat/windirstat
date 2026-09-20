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
#include "TreeMapLayout.h"
#include "Item.h"

static constexpr COLORREF BGR(auto b, auto g, auto r)
{
    return static_cast<BYTE>(b) | static_cast<BYTE>(g) << 8 | static_cast<BYTE>(r) << 16;
}

// Define the "brightness" of an RGB value as (r+b+g)/3/255.
// The GetDefaultPalette() method creates a palette with colors
// all having the same brightness of 0.6.
// Later in DrawCushion() this number is used again to
// scale the colors.

using Surface = std::array<double, 4>;

struct DrawStateInfo
{
    Surface surface{};
    CRect rc{};
    CItem* item = nullptr;
    double ridgeHeight = 0.0;
    bool asRoot = false;
    int depth = 0;
    TreeMapLayout::State layoutState;
};

struct LayoutScratch
{
    std::vector<ULONGLONG> childWeights;
    std::vector<TreeMapLayout::ChildRegion> childRegions;
};

struct PreparedColor
{
    COLORREF color = RGB(0, 0, 0);
    double brightness = 0.0;
};

constexpr UINT EXTENSION_TEXT_FLAGS = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
constexpr int EXTENSION_TEXT_PADDING = 4;
const std::array EXTENSION_SHADOW_OFFSETS = {
    CPoint(0, -1),
    CPoint(-1, 0),
    CPoint(1, 0),
    CPoint(0, 1)
};

PreparedColor PrepareRenderColor(const DWORD color, const double brightness, const double saturation)
{
    const COLORREF baseColor = static_cast<COLORREF>(color);
    PreparedColor prepared{ .color = baseColor, .brightness = brightness };

    if (const DWORD flags = color & CTreeMap::COLORFLAG_MASK; flags != 0)
    {
        prepared.color = CColorSpace::MakeBrightColor(baseColor, CColorSpace::GraphPaletteBrightness);
        prepared.brightness = (flags & CTreeMap::COLORFLAG_DARKER) != 0
            ? prepared.brightness * 0.66 : std::min(prepared.brightness * 1.2, 1.0);
    }

    const auto gray = static_cast<BYTE>((GetRValue(prepared.color)
        + GetGValue(prepared.color) + GetBValue(prepared.color)) / 3);
    prepared.color = CColorSpace::BlendColor(prepared.color, RGB(gray, gray, gray),
        1.0 - std::clamp(saturation, 0.0, 1.0));
    return prepared;
}

COLORREF MakeBitmapColor(const COLORREF color, const double brightness)
{
    const double factor = brightness / CColorSpace::GraphPaletteBrightness;

    int red = static_cast<int>(GetRValue(color) * factor);
    int green = static_cast<int>(GetGValue(color) * factor);
    int blue = static_cast<int>(GetBValue(color) * factor);

    CColorSpace::NormalizeColor(red, green, blue);
    return BGR(blue, green, red);
}

COLORREF CFolderColors::GetColor(const CItem* item) const
{
    const COLORREF background = DarkMode::SystemColor(COLOR_WINDOW);
    if (item == nullptr || item->IsScanRoot())
        return CColorSpace::BlendColor(background, DarkMode::SystemColor(COLOR_WINDOWTEXT), 0.12);

    static constexpr std::array palette{
        RGB(79, 157, 232), RGB(71, 173, 128), RGB(230, 159, 0), RGB(213, 94, 0),
        RGB(168, 135, 224), RGB(86, 180, 233), RGB(204, 121, 167), RGB(230, 205, 70)
    };
    if (m_colors.empty()) m_colors.resize(ColorCacheSize);
    const auto cachedColor = [this](const CItem* folder) -> ColorInfo&
    {
        return m_colors[std::hash<const CItem*>{}(folder) % ColorCacheSize];
    };
    ColorInfo color = cachedColor(item);
    if (color.item != item)
    {
        int depth = 0;
        const CItem* ancestor = item;
        while (ancestor->GetParent() != nullptr && !ancestor->GetParent()->IsScanRoot())
        {
            ancestor = ancestor->GetParent();
            ++depth;
            color = cachedColor(ancestor);
            if (color.item == ancestor) break;
        }

        if (color.item != ancestor)
        {
            auto branch = m_branches.find(ancestor);
            if (branch == m_branches.end())
            {
                std::vector<const CItem*> siblings;
                for (const CItem* sibling : ancestor->GetParent()->GetChildren())
                    if (!sibling->TmiIsLeaf()) siblings.push_back(sibling);
                std::ranges::sort(siblings, [](const CItem* left, const CItem* right)
                {
                    if (left->GetSizePhysical() != right->GetSizePhysical())
                        return left->GetSizePhysical() > right->GetSizePhysical();
                    return left->GetNameView() < right->GetNameView();
                });
                unsigned int used = 0;
                for (const CItem* sibling : siblings)
                {
                    if (used == (1u << palette.size()) - 1) used = 0;
                    std::uint32_t hash = 2166136261u;
                    for (const wchar_t character : sibling->GetNameView()) hash = (hash ^ character) * 16777619u;
                    std::size_t index = hash % palette.size();
                    while ((used & (1u << index)) != 0) index = (index + 1) % palette.size();
                    used |= 1u << index;
                    m_branches.emplace(sibling, index);
                }
                branch = m_branches.find(ancestor);
            }
            color = { ancestor, static_cast<std::uint8_t>(branch->second), 0 };
            cachedColor(ancestor) = color;
        }

        const ColorInfo inherited = color;
        color = { item, inherited.index, static_cast<std::uint8_t>(std::min(inherited.depth + depth, 6)) };
        for (const CItem* descendant = item; descendant != ancestor; descendant = descendant->GetParent(), --depth)
            cachedColor(descendant) = { descendant, inherited.index,
                static_cast<std::uint8_t>(std::min(inherited.depth + depth, 6)) };
        cachedColor(item) = color;
    }
    return CColorSpace::BlendColor(palette[color.index], background, 0.22 + std::min<int>(color.depth, 6) * 0.06);
}

void FillSolidRect(HDC dc, const RECT& rc, const COLORREF color)
{
    ScopedBkColor background(dc, color);
    ExtTextOutW(dc, 0, 0, ETO_OPAQUE, &rc, nullptr, 0, nullptr);
}

bool PrepareRenderArea(HDC dc, CRect& rc, const bool drawOuterFrame)
{
    if (rc.Width() <= 0 || rc.Height() <= 0)
    {
        return false;
    }

    if (drawOuterFrame)
    {
        // We shrink the rectangle here, too.
        // If we didn't do this, the layout of the treemap would
        // change when grid is switched on and off.
        const CPen pen(PS_SOLID, 1, DarkMode::SystemColor(COLOR_3DSHADOW));
        GdiObjectSelection selectPen(dc, &pen);
        MoveToEx(dc, rc.right - 1, rc.top, nullptr);
        LineTo(dc, rc.right - 1, rc.bottom);
        MoveToEx(dc, rc.left, rc.bottom - 1, nullptr);
        LineTo(dc, rc.right, rc.bottom - 1);
    }

    rc.right--;
    rc.bottom--;

    return rc.Width() > 0 && rc.Height() > 0;
}

void BlitBitmap(HDC dc, const CRect& rc, const std::vector<COLORREF>& bitmapBits)
{
    // Use SetDIBitsToDevice for compatibility with Remote Desktop at <32-bit depth
    const BITMAPINFO bmi{ .bmiHeader = { .biSize = sizeof(BITMAPINFOHEADER),
        .biWidth = rc.Width(), .biHeight = -rc.Height(), .biPlanes = 1,
        .biBitCount = 32, .biCompression = BI_RGB } };
    SetDIBitsToDevice(dc, rc.left, rc.top, rc.Width(), rc.Height(),
        0, 0, 0, rc.Height(), bitmapBits.data(), &bmi, DIB_RGB_COLORS);
}

bool CanDrawExtensionLabel(const CRect& rc, const CSize& textSize)
{
    return textSize.cx + EXTENSION_TEXT_PADDING <= rc.Width()
        && textSize.cy + EXTENSION_TEXT_PADDING <= rc.Height();
}

void DrawShadowedExtensionText(HDC dc, const std::wstring_view text, const CRect& rc, const COLORREF background)
{
    ScopedDcState saveDc(dc);
    IntersectClipRect(dc, rc.left, rc.top, rc.right, rc.bottom);

    const bool contrastLabels = background != CLR_INVALID;
    COLORREF foreground = RGB(255, 255, 255);
    if (contrastLabels)
    {
        foreground = CColorSpace::GetContrastingColor(background);
    }

    if (!contrastLabels)
    {
        ScopedTextColor shadowTextColor(dc, RGB(0, 0, 0));
        for (const CPoint& offset : EXTENSION_SHADOW_OFFSETS)
        {
            CRect shadowRc = rc + offset;
            DrawTextW(dc, text.data(), static_cast<int>(text.size()), &shadowRc, EXTENSION_TEXT_FLAGS);
        }
    }

    {
        ScopedTextColor textColor(dc, foreground);
        CRect textRc = rc;
        DrawTextW(dc, text.data(), static_cast<int>(text.size()), &textRc, EXTENSION_TEXT_FLAGS);
    }
}

/////////////////////////////////////////////////////////////////////////////

void CTreeMap::GetDefaultPalette(std::vector<COLORREF>& palette)
{
    palette.assign_range(DefaultCushionColors | std::views::transform([](const COLORREF color) {
        return CColorSpace::MakeBrightColor(color, CColorSpace::GraphPaletteBrightness);
    }));
}

void CTreeMap::Options::SetAppearance(const Options& appearance)
{
    grid = appearance.grid;
    gridColor = appearance.gridColor;
    brightness = appearance.brightness;
    saturation = appearance.saturation;
    contrastLabels = appearance.contrastLabels;
    ambientLight = appearance.ambientLight;
    height = appearance.height;
    scaleFactor = appearance.scaleFactor;
    lightSourceX = appearance.lightSourceX;
    lightSourceY = appearance.lightSourceY;
}

CTreeMap::Options CTreeMap::GetPreset(const Preset preset)
{
    Options options = DefaultOptions;
    if (preset == Preset::Classic) return options;

    options.grid = true;
    options.gridColor = CLR_DEFAULT;
    options.brightness = 0.72;
    options.saturation = 0.65;
    options.height = 0.15;
    options.scaleFactor = 0.80;
    options.ambientLight = 0.70;
    options.contrastLabels = true;

    switch (preset)
    {
    case Preset::Flat:
        options.saturation = 0.85;
        options.height = 0.0;
        options.ambientLight = 1.0;
        break;
    case Preset::Pastel:
        options.gridColor = CLR_DEFAULT;
        options.brightness = 0.82;
        options.saturation = 0.40;
        options.height = 0.10;
        options.ambientLight = 0.85;
        break;
    case Preset::HighContrast:
        options.gridColor = RGB(0, 0, 0);
        options.brightness = 0.62;
        options.saturation = 1.0;
        options.height = 0.0;
        options.ambientLight = 1.0;
        break;
    default:
        break;
    }
    return options;
}

std::optional<CTreeMap::Preset> CTreeMap::GetMatchingPreset(const Options& options)
{
    const auto appearance = [](const Options& candidate)
    {
        return std::tuple(candidate.grid, candidate.grid ? candidate.gridColor : CLR_INVALID, candidate.contrastLabels,
            candidate.GetBrightnessPercent(), candidate.GetSaturationPercent(), candidate.GetAmbientLightPercent(),
            candidate.GetHeightPercent(), candidate.GetScaleFactorPercent(),
            candidate.GetLightSourceXPercent(), candidate.GetLightSourceYPercent());
    };
    const auto current = appearance(options);
    const int maxPreset = std::to_underlying(Preset::HighContrast);
    const auto presets = std::views::iota(0, maxPreset + 1);
    const auto it = std::ranges::find_if(presets, [&](const int preset) {
        return current == appearance(GetPreset(static_cast<Preset>(preset)));
    });
    return it != presets.end() ? std::optional(static_cast<Preset>(*it)) : std::nullopt;
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
            const auto item = CItem::Create(IT_FILE | ITF_PREVIEW, L"");
            item->SetSizePhysical(size);
            item->SetSizeLogical(size);
            item->SetIndex(color);
            return item;
        };

        auto createContainer = [](std::vector<CItem*>& children) -> CItem*
        {
            std::ranges::sort(children, [](CItem* a, CItem* b) { return a->GetSizePhysical() > b->GetSizePhysical(); });
            const auto item = CItem::Create(IT_DIRECTORY, L"");
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

COLORREF CTreeMap::GetFlatColor(const DWORD color, const Options& options)
{
    const PreparedColor prepared = PrepareRenderColor(color, options.brightness, options.saturation);
    const COLORREF pixel = MakeBitmapColor(prepared.color, prepared.brightness);
    return RGB(GetBValue(pixel), GetGValue(pixel), GetRValue(pixel));
}

COLORREF CTreeMap::GetGridColor(const Options& options)
{
    return options.gridColor == CLR_DEFAULT
        ? CColorSpace::BlendColor(DarkMode::SystemColor(COLOR_WINDOW),
            DarkMode::SystemColor(COLOR_WINDOWTEXT), 0.12)
        : options.gridColor;
}

CTreeMap::CTreeMap()
{
    const Options options = GetDefaults();
    SetOptions(&options);
}

void CTreeMap::SetOptions(const Options* options)
{
    assert(options != nullptr);
    m_options = *options;
    m_options.saturation = std::clamp(m_options.saturation, 0.0, 1.0);

    // Derive normalized vector here for performance
    const double lx = m_options.lightSourceX; // negative = left
    const double ly = m_options.lightSourceY; // negative = top
    constexpr double lz = 10;

    const double len = sqrt(lx * lx + ly * ly + lz * lz);
    m_lx = lx / len;
    m_ly = ly / len;
    m_lz = lz / len;
}

void CTreeMap::ClearLayout()
{
    m_folderColors.Clear();
    m_layoutRoot = nullptr;
    m_layoutArea.Clear();
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
    m_folderColors = {};
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
    assert(inserted);
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
    if (m_layoutArea.IsEmpty() || m_visibleItems.empty()) return;

    m_hitTestColumns = (m_layoutArea.Width() + HitTestCellSize - 1) / HitTestCellSize;
    m_hitTestRows = (m_layoutArea.Height() + HitTestCellSize - 1) / HitTestCellSize;
    const std::size_t cellCount = static_cast<std::size_t>(m_hitTestColumns)
        * static_cast<std::size_t>(m_hitTestRows);
    m_hitTestCellOffsets.assign(cellCount + 1, 0);

    const auto visitCells = [this](const CRect& rectangle, auto&& visitor)
    {
        CRect clipped;
        if (!clipped.Intersect(rectangle, m_layoutArea)) return;

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
    std::vector hasVisibleChildren(m_visibleItems.size(), false);
    for (const VisibleItem& child : m_visibleItems)
    {
        const CItem* const parent = child.item->GetParent();
        const auto parentFound = m_itemToVisibleIndex.find(parent);
        if (parentFound == m_itemToVisibleIndex.end()) continue;

        const std::size_t parentIndex = parentFound->second;
        if (hasVisibleChildren[parentIndex])
        {
            CRect combined;
            combined.Union(childBounds[parentIndex], child.rectangle);
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

    for (const auto [index, visibleItem] : std::views::enumerate(m_visibleItems))
    {
        const CRect outer = visibleItem.rectangle;
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

    for (const auto& [_, rectangle] : indexedRegions)
    {
        visitCells(rectangle, [this](const std::size_t cell)
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
    for (const auto& [visibleIndex, rectangle] : indexedRegions)
    {
        visitCells(rectangle,
            [&nextEntry, this, index = visibleIndex](const std::size_t cell)
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

void CTreeMap::RecurseCheckTree(const CItem* item)
{
    if (item->TmiIsLeaf())
    {
        assert(item->TmiGetChildCount() == 0);
    }
    else
    {
        ULONGLONG sum = 0;
        ULONGLONG last = static_cast<ULONGLONG>(-1);
        for (const CItem* child : item->GetChildren())
        {
            const ULONGLONG size = child->TmiGetSize();
            assert(size <= last);
            sum += size;
            last = size;
            RecurseCheckTree(child);
        }
        assert(sum == item->TmiGetSize());
    }
}
void CTreeMap::DrawTreeMap(HDC dc, CRect rc, CItem* root, const Options* options)
{
    ClearLayout();

    // Validate parameters and options
    assert(dc != nullptr && root != nullptr);
    if (dc == nullptr || root == nullptr)
    {
        // Parameter check fallback
        return;
    }

    if constexpr (IsDebugBuild) RecurseCheckTree(root);

    if (options != nullptr)
    {
        SetOptions(options);
    }

    if (root->TmiGetSize() == 0)
    {
        FillSolidRect(dc, rc, DarkMode::SystemColor(COLOR_WINDOW));
    }

    if (!PrepareRenderArea(dc, rc, !m_options.grid))
    {
        return;
    }

    GdiObjectSelection selectFont(dc, GetAppFont());

    TEXTMETRIC tm{};
    GetTextMetricsW(dc, &tm);
    const int headerHeight = tm.tmHeight + 2;
    const int accentWidth = std::max(2, MulDiv(GetDeviceCaps(dc, LOGPIXELSX), GetFontSizePercent(),
        USER_DEFAULT_SCREEN_DPI * 50));

    const int renderWidth = rc.Width();
    const int renderHeight = rc.Height();
    m_layoutRoot = root;
    m_layoutArea = CRect(0, 0, renderWidth, renderHeight);

    if (root->TmiGetSize() == 0)
    {
        AddVisibleItem(root, m_layoutArea, 0);
        BuildHitTestIndex();
        return;
    }

    const size_t pixelCount = static_cast<size_t>(renderWidth) * static_cast<size_t>(renderHeight);
    BitmapView bitmap{};
    bool rendersIntoDc = false;
    DIBSECTION dibSection{};
    const HGDIOBJ selectedBitmap = GetCurrentObject(dc, OBJ_BITMAP);
    if (selectedBitmap != nullptr
        && ::GetObject(selectedBitmap, sizeof(dibSection), &dibSection) == sizeof(dibSection)
        && dibSection.dsBm.bmBits != nullptr && dibSection.dsBm.bmBitsPixel == 32
        && dibSection.dsBmih.biHeight < 0 && rc.left >= 0 && rc.top >= 0
        && rc.right <= dibSection.dsBm.bmWidth && rc.bottom <= dibSection.dsBm.bmHeight)
    {
        GdiFlush();
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
        m_options.grid ? GetGridColor(m_options) : DarkMode::SystemColor(COLOR_WINDOW),
        CColorSpace::GraphPaletteBrightness);

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
        std::wstring label;
        bool showHeader;
    };
    std::vector<FolderDrawInfo> foldersToDraw;
    foldersToDraw.reserve(128);

    auto pushChildState = [this, &stack](CItem* child, const CRect& childRect,
        const DrawStateInfo& parentState,
        const TreeMapLayout::State layoutState)
    {
        stack.push_back({ parentState.surface, childRect, child,
            parentState.ridgeHeight * m_options.scaleFactor, false,
            parentState.depth + 1, layoutState });
    };

    auto pushChildren = [this, &layoutScratch, &pushChildState](
        CItem* item, const DrawStateInfo& state)
    {
        const auto& children = item->GetChildren();
        layoutScratch.childWeights.resize(children.size());
        for (const auto [i, child] : std::views::enumerate(children))
            layoutScratch.childWeights[i] = child->TmiGetSize();

        TreeMapLayout::ArrangeChildren({
            .style = m_options.style,
            .bounds = state.rc,
            .parentWeight = item->TmiGetSize(),
            .weights = layoutScratch.childWeights,
            .state = state.layoutState,
        }, layoutScratch.childRegions);

        for (const auto [child, childRegion] : std::views::zip(children, layoutScratch.childRegions))
        {
            if (childRegion.bounds.IsEmpty()) continue;
            pushChildState(child, childRegion.bounds, state, childRegion.state);
        }
    };

    // Main layout loop
    while (!stack.empty())
    {
        DrawStateInfo state = stack.back();
        stack.pop_back();

        CItem* item = state.item;
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
            std::wstring label;
            while (item->IsTypeOrFlag(IT_DIRECTORY))
            {
                const auto& children = item->GetChildren();
                if (children.empty() || !children.front()->IsTypeOrFlag(IT_DIRECTORY)
                    || children.front()->TmiIsLeaf() || children.front()->TmiGetSize() != item->TmiGetSize()
                    || std::ranges::any_of(children | std::views::drop(1),
                        [](const CItem* child) { return child->TmiGetSize() != 0; })) break;
                if (label.empty()) label = item->GetNameView(true);
                item = children.front();
                label += L'\\';
                label += item->GetNameView(true);
                ++state.depth;
                state.ridgeHeight *= m_options.scaleFactor;
                if (cushionShading) AddRidge(state.rc, state.surface, state.ridgeHeight);
                AddVisibleItem(item, state.rc, state.depth);
            }
            const std::wstring_view name = item->GetNameView(true);
            const int textWidth = state.rc.Width() - accentWidth - 8;
            CSize nameSize;
            GetTextExtentPoint32W(dc, name.data(), static_cast<int>(name.size()), &nameSize);
            const bool showHeader = state.rc.Height() > headerHeight && nameSize.cx <= textWidth;

            foldersToDraw.push_back({ item, state.rc, std::move(label), showHeader });
            state.rc.left += 1;
            state.rc.right -= 1;
            state.rc.bottom -= 1;
            state.rc.top += showHeader ? headerHeight : 1;

            if (state.rc.Width() <= gridWidth || state.rc.Height() <= gridWidth)
            {
                continue;
            }
        }

        pushChildren(item, state);
    }

    BuildHitTestIndex();

    if (!rendersIntoDc) BlitBitmap(dc, rc, m_bitmapBits);

    // Render directory frames and labels
    if (m_options.showFolderFrames)
    {
        ScopedBkMode backgroundMode(dc, TRANSPARENT);
        const CPoint rcOffset = rc.TopLeft();
        const COLORREF background = DarkMode::SystemColor(COLOR_WINDOW);
        const COLORREF headerColor = CColorSpace::BlendColor(background, DarkMode::SystemColor(COLOR_WINDOWTEXT), 0.07);
        const ScopedTextColor textColor(dc, CColorSpace::GetContrastingColor(headerColor));
        const auto borderBrush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));

        for (const auto& folder : foldersToDraw)
        {
            CRect rcFolder = folder.rc + rcOffset;

            if (rcFolder.Width() > 2 && rcFolder.Height() > 2)
            {
                const COLORREF branchColor = m_folderColors.GetColor(folder.item);
                SetDCBrushColor(dc, CColorSpace::BlendColor(branchColor, background, 0.65));
                FrameRect(dc, &rcFolder, borderBrush);

                if (folder.showHeader)
                {
                    CRect rcHeader(rcFolder.left + 1, rcFolder.top + 1, rcFolder.right - 1, rcFolder.top + headerHeight);
                    FillSolidRect(dc, rcHeader, headerColor);
                    FillSolidRect(dc, CRect(rcHeader.left, rcHeader.top,
                        std::min(rcHeader.right, rcHeader.left + accentWidth), rcHeader.bottom), branchColor);

                    CRect rcText(rcHeader.left + accentWidth + 3, rcHeader.top, rcHeader.right - 3, rcHeader.bottom);
                    const std::wstring_view label = folder.label.empty()
                        ? folder.item->GetNameView(true) : folder.label;
                    DrawTextW(dc, label.data(), static_cast<int>(label.size()), &rcText,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_PATH_ELLIPSIS);
                }
            }
        }
    }

    if (m_options.showExtensions)
    {
        DrawTreeMapLabels(dc, rc.TopLeft(), bitmap);
    }
}

CItem* CTreeMap::FindItemByPoint(CItem* item, const CPoint point) const
{
    if (item == nullptr || !m_layoutArea.Contains(point)
        || m_hitTestColumns <= 0 || m_hitTestRows <= 0) return nullptr;

    const auto start = m_itemToVisibleIndex.find(item);
    if (start == m_itemToVisibleIndex.end()
        || !m_visibleItems[start->second].rectangle.Contains(point)) return nullptr;

    const int column = (point.x - m_layoutArea.left) / HitTestCellSize;
    const int row = (point.y - m_layoutArea.top) / HitTestCellSize;
    if (column < 0 || column >= m_hitTestColumns || row < 0 || row >= m_hitTestRows)
        return nullptr;

    CItem* bestItem = item;
    int bestDepth = m_visibleItems[start->second].depth;
    const std::size_t cell = static_cast<std::size_t>(row) * m_hitTestColumns + column;
    const std::size_t begin = m_hitTestCellOffsets[cell];
    const std::size_t end = m_hitTestCellOffsets[cell + 1];
    const std::span entries = std::span(m_hitTestEntries).subspan(begin, end - begin);
    for (const std::size_t candidateIndex : entries)
    {
        const VisibleItem& candidate = m_visibleItems[candidateIndex];
        if (candidate.depth > bestDepth && candidate.rectangle.Contains(point))
        {
            bestItem = candidate.item;
            bestDepth = candidate.depth;
        }
    }

    return bestItem;
}

void CTreeMap::DrawColorPreview(HDC dc, const CRect& rc, const COLORREF color, const Options* options)
{
    assert(dc != nullptr);
    if (dc == nullptr || rc.Width() <= 0 || rc.Height() <= 0)
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

    if (ScopedDcState saveDc(dc); true)
    {
        const CRgn region(rc, 3, 3);
        ExtSelectClipRgn(dc, region, RGN_AND);
        BlitBitmap(dc, rc, m_bitmapBits);
    }

    if (m_options.grid)
    {
        SetDCPenColor(dc, GetGridColor(m_options));
        StockObjectSelection selectPen(dc, DC_PEN);
        StockObjectSelection selectBrush(dc, NULL_BRUSH);
        RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, 3, 3);
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

void CTreeMap::RenderRectangle(const BitmapView bitmap, const CRect& rc, const std::array<double, 4>& surface, const DWORD color) const
{
    if (rc.Width() <= 0 || rc.Height() <= 0)
    {
        return;
    }

    const PreparedColor prepared = PrepareRenderColor(color, m_options.brightness, m_options.saturation);

    if (IsCushionShading())
    {
        DrawCushion(bitmap, rc, surface, prepared.color, prepared.brightness);
    }
    else
    {
        DrawSolidRect(bitmap, rc, prepared.color, prepared.brightness);
    }
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
    const double brightnessFactor = brightness / CColorSpace::GraphPaletteBrightness;

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
            assert(pixel <= 1.0);

            // Now, pixel is the brightness of the pixel, 0...1.0.
            // Apply contrast.
            // Not implemented.
            // Costs performance and nearly the same effect can be
            // made with the m_options->ambientLight parameter.
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

    assert(width > 0 && height > 0);

    const double h4 = 4 * h;

    const double wf = h4 / width;
    surface[2] += wf * (rc.right + rc.left);
    surface[0] -= wf;

    const double hf = h4 / height;
    surface[3] += hf * (rc.bottom + rc.top);
    surface[1] -= hf;
}

void CTreeMap::DrawTreeMapLabels(HDC dc, const CPoint& offset, const BitmapView bitmap) const
{
    assert(dc != nullptr);
    if (dc == nullptr) return;
    if (m_options.contrastLabels) GdiFlush();

    GdiObjectSelection selectFont(dc, GetAppFont());
    ScopedBkMode backgroundMode(dc, TRANSPARENT);

    std::unordered_map<std::wstring, CSize> textExtentCache;

    for (const VisibleItem& visible : m_visibleItems)
    {
        const CItem* item = visible.item;
        if (!item->TmiIsLeaf()) continue;

        const CRect rc = visible.rectangle + offset;

        // Fast size check to avoid string copies, lowercasing, and caching for tiny cushions
        if (rc.Height() < 16 || rc.Width() < 16) continue;

        std::wstring label = m_options.showExtensions ? item->GetExtension() : L"";

        if (label.empty()) continue;

        auto [cacheIt, inserted] = textExtentCache.try_emplace(label);
        if (inserted)
        {
            GetTextExtentPoint32W(dc, label.c_str(),
                static_cast<int>(label.size()), &cacheIt->second);
        }
        if (!CanDrawExtensionLabel(rc, cacheIt->second)) continue;

        COLORREF background = CLR_INVALID;
        if (m_options.contrastLabels)
        {
            const CPoint center = visible.rectangle.Center();
            const COLORREF pixel = bitmap.bits[static_cast<size_t>(center.y) * bitmap.stride + center.x];
            background = RGB(GetBValue(pixel), GetGValue(pixel), GetRValue(pixel));
        }
        DrawShadowedExtensionText(dc, label, rc, background);
    }
}

/////////////////////////////////////////////////////////////////////////////

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
    const CPaintDC dc(this);
    const CRect rc = GetClientRect();
    m_treeMap.DrawTreeMap(dc.Handle(), rc, m_root);
}
