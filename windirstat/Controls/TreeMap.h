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

#pragma once

#include "pch.h"
#include "Item.h"
#include "TreeMapLayout.h"

//
// CColorSpace. Helper class for manipulating colors. Static members only.
//
class CColorSpace final
{
public:
    static constexpr double GraphPaletteBrightness = 0.6;
    static constexpr DWORD GraphColorDarker = 0x01000000;
    static constexpr DWORD GraphColorLighter = 0x02000000;
    static constexpr DWORD GraphColorMask = GraphColorDarker | GraphColorLighter;

    static constexpr COLORREF DimColor(const COLORREF color, float factor = 0.9f) noexcept
    {
        factor = std::clamp(factor, 0.0f, 1.0f);
        return RGB(static_cast<BYTE>(GetRValue(color) * factor),
            static_cast<BYTE>(GetGValue(color) * factor),
            static_cast<BYTE>(GetBValue(color) * factor));
    }

    // Returns the arithmetic brightness used by MakeBrightColor.
    static constexpr double GetColorBrightness(const COLORREF color)
    {
        const unsigned int intensity = GetRValue(color) + GetGValue(color) + GetBValue(color);
        return intensity / 255.0 / 3.0;
    }

    // Returns the WCAG relative luminance of an sRGB color (0.0 .. 1.0).
    // This tracks perceived brightness and should be used for contrast choices.
    static double GetRelativeLuminance(const COLORREF color)
    {
        const auto toLinear = [](const BYTE component)
        {
            const double srgb = component / 255.0;
            return srgb <= 0.04045
                ? srgb / 12.92
                : std::pow((srgb + 0.055) / 1.055, 2.4);
        };

        return 0.2126 * toLinear(GetRValue(color))
            + 0.7152 * toLinear(GetGValue(color))
            + 0.0722 * toLinear(GetBValue(color));
    }

    // Gives a color a defined brightness.
    static constexpr COLORREF MakeBrightColor(COLORREF color, double brightness)
    {
        ASSERT(brightness >= 0.0);
        ASSERT(brightness <= 1.0);

        double dred = (GetRValue(color) & 0xFF) / 255.0;
        double dgreen = (GetGValue(color) & 0xFF) / 255.0;
        double dblue = (GetBValue(color) & 0xFF) / 255.0;

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

    static constexpr COLORREF ApplyGraphColorFlags(const DWORD rawColor)
    {
        const DWORD flags = rawColor & GraphColorMask;
        COLORREF color = rawColor & 0x00FFFFFF;
        if (flags != GraphColorDarker && flags != GraphColorLighter) return color;

        color = MakeBrightColor(color, GraphPaletteBrightness);
        if (flags == GraphColorDarker) return DimColor(color, 0.66f);
        return RGB(std::min(255, GetRValue(color) + 60),
            std::min(255, GetGValue(color) + 60),
            std::min(255, GetBValue(color) + 60));
    }

    // Swaps values above 255 to the other two values
    static constexpr void NormalizeColor(int& red, int& green, int& blue)
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

protected:
    // Helper function for NormalizeColor()
    static constexpr void DistributeFirst(int& first, int& second, int& third)
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
};

//
// CTreeMap. Can create a treemap using rows, squarified, Hilbert, or Moore layouts.
//
// This class is fairly reusable.
//
class CTreeMap final
{
public:
    // Geometry produced by the most recent layout. This is deliberately kept
    // outside CItem so hidden/pruned descendants can never expose rectangles
    // left over from a previous render generation.
    struct VisibleItem
    {
        CItem* item = nullptr;
        CRect rectangle;
        int depth = 0;
    };

    // One of these flags can be added to the COLORREF returned
    // by TmiGetGraphColor(). Used for <Free space> (darker)
    // and <Unknown> (brighter).
    //
    static constexpr DWORD COLORFLAG_DARKER  = CColorSpace::GraphColorDarker;
    static constexpr DWORD COLORFLAG_LIGHTER = CColorSpace::GraphColorLighter;
    static constexpr DWORD COLORFLAG_MASK    = CColorSpace::GraphColorMask;

    //
    // Collection of all treemap options.
    //
    struct Options
    {
        TreeMapLayout::Style style; // Child layout algorithm
        bool grid;           // Whether to draw grid lines
        bool showExtensions; // Whether to show file extensions in treemap
        bool showFolderFrames; // Whether to draw folder borders and headers
        int folderFramesDrawThreshold; // Minimum folder rectangle edge to draw frames
        COLORREF gridColor;  // Color of grid lines
        double brightness;   // 0..1.0   (default = 0.84)
        double height;       // >= 0.0    (default = 0.40)    Factor "H"
        double scaleFactor;  // 0..1.0   (default = 0.90)    Factor "F"
        double ambientLight; // 0..1.0   (default = 0.15)    Factor "Ia"
        double lightSourceX; // -4.0..+4.0 (default = -1.0), negative = left
        double lightSourceY; // -4.0..+4.0 (default = -1.0), negative = top

        constexpr int GetBrightnessPercent() const { return RoundDouble(brightness * 100); }
        constexpr int GetHeightPercent() const { return RoundDouble(height * 100); }
        constexpr int GetScaleFactorPercent() const { return RoundDouble(scaleFactor * 100); }
        constexpr int GetAmbientLightPercent() const { return RoundDouble(ambientLight * 100); }
        constexpr int GetLightSourceXPercent() const { return RoundDouble(lightSourceX * 100); }
        constexpr int GetLightSourceYPercent() const { return RoundDouble(lightSourceY * 100); }
        CPoint GetLightSourcePoint() const { return { GetLightSourceXPercent(), GetLightSourceYPercent() }; }

        constexpr void SetBrightnessPercent(int n) { brightness = n / 100.0; }
        constexpr void SetHeightPercent(int n) { height = n / 100.0; }
        constexpr void SetScaleFactorPercent(int n) { scaleFactor = n / 100.0; }
        constexpr void SetAmbientLightPercent(int n) { ambientLight = n / 100.0; }
        constexpr void SetLightSourceXPercent(int n) { lightSourceX = n / 100.0; }
        constexpr void SetLightSourceYPercent(int n) { lightSourceY = n / 100.0; }
        void SetLightSourcePoint(CPoint pt) { SetLightSourceXPercent(pt.x); SetLightSourceYPercent(pt.y); }

        static constexpr int RoundDouble(double d) { return static_cast<int>(d + (d < 0.0 ? -0.5 : 0.5)); }
    };

    // Get a good palette of 18 colors
    static void GetDefaultPalette(std::vector<COLORREF>& palette);

    // Build the small demo tree used by treemap previews.
    [[nodiscard]] static std::unique_ptr<CItem> BuildDemoTree();

    // Good values
    static Options GetDefaults();

    // Construct the treemap generator and register the callback interface.
    CTreeMap();

    // Alter the options
    void SetOptions(const Options* options);
    Options GetOptions() const;

#ifdef _DEBUG
    // DEBUG function
    void RecurseCheckTree(const CItem *item);
#endif // _DEBUG

    // Create and draw a treemap
    void DrawTreeMap(CDC* pdc, CRect rc, CItem* root, const Options* options = nullptr);

    // In the resulting treemap, find the item below a given coordinate.
    // Return value can be nullptr, iff point is outside root rect.
    CItem* FindItemByPoint(CItem* item, CPoint point) const;

    // Access and clear only geometry from the most recent treemap render.
    [[nodiscard]] bool HasValidLayout(const CItem* root) const;
    [[nodiscard]] bool TryGetItemRectangle(const CItem* item, CRect& rectangle) const;
    [[nodiscard]] std::span<const VisibleItem> GetVisibleItems() const { return m_visibleItems; }
    void ClearLayout();
    void TrimMemory();

    // Draws a sample rectangle in the given style (for color legend)
    void DrawColorPreview(CDC* pdc, const CRect& rc, COLORREF color, const Options* options = nullptr);

protected:

    struct BitmapView
    {
        COLORREF* bits;
        std::size_t stride;
    };

    // Returns true, if height and scaleFactor are > 0 and ambientLight is < 1.0
    bool IsCushionShading() const;

    // Leaves space for grid and then calls RenderRectangle()
    void RenderLeaf(BitmapView bitmap, const CItem* item,
        const CRect& rectangle, const std::array<double, 4>& surface) const;

    // Either calls DrawCushion() or DrawSolidRect()
    void RenderRectangle(BitmapView bitmap, const CRect& rc, const std::array<double, 4>& surface, DWORD color) const;

    // Renders cushion pixels.
    void DrawCushion(BitmapView bitmap, const CRect& rc, const std::array<double, 4>& surface, COLORREF col, double brightness) const;

    // Fills solid pixels.
    void DrawSolidRect(BitmapView bitmap, const CRect& rc, COLORREF col, double brightness) const;

    // Adds a new ridge to surface
    static void AddRidge(const CRect& rc, std::array<double, 4>& surface, double h);

    // Draws file extension/filename labels on leaf items
    void DrawTreeMapLabels(CDC* pdc, const CPoint& offset) const;

    void AddVisibleItem(CItem* item, const CRect& rectangle, int depth);
    void BuildHitTestIndex();

    // Default tree map options
    static constexpr Options DefaultOptions = {
        .style = TreeMapLayout::Style::Rows,
        .grid = false,
        .showExtensions = false,
        .showFolderFrames = false,
        .folderFramesDrawThreshold = 5,
        .gridColor = RGB(0, 0, 0),
        .brightness = 0.88,
        .height = 0.38,
        .scaleFactor = 0.91,
        .ambientLight = 0.13,
        .lightSourceX = -1.0,
        .lightSourceY = -1.0
    };

    // Standard palette for WinDirStat
    static constexpr COLORREF DefaultCushionColors[] = {
        RGB(  0,   0, 255),  // Blue
        RGB(255,   0,   0),  // Red
        RGB(  0, 255,   0),  // Green
        RGB(255, 255,   0),  // Yellow
        RGB(  0, 255, 255),  // Cyan
        RGB(255,   0, 255),  // Magenta
        RGB(255, 170,   0),  // Orange
        RGB(  0,  85, 255),  // Dodger Blue
        RGB(255,   0,  85),  // Hot Pink
        RGB( 85, 255,   0),  // Lime Green
        RGB(170,   0, 255),  // Violet
        RGB(  0, 255,  85),  // Spring Green
        RGB(255,   0, 170),  // Deep Pink
        RGB(  0, 170, 255),  // Sky Blue
        RGB(255,  85,   0),  // Orange Red
        RGB(  0, 255, 170),  // Aquamarine
        RGB( 85,   0, 255),  // Indigo
        RGB(255, 255, 255),  // White
    };

    static constexpr int HitTestCellSize = 16;
    const CItem* m_layoutRoot = nullptr;
    CRect m_layoutArea;
    int m_hitTestColumns = 0;
    int m_hitTestRows = 0;
    std::vector<VisibleItem> m_visibleItems;
    std::unordered_map<const CItem*, std::size_t> m_itemToVisibleIndex;
    // Compressed per-cell candidate lists avoid one heap allocation for every
    // spatial bucket while keeping mouse hit-testing bounded to a 16px cell.
    std::vector<std::size_t> m_hitTestCellOffsets;
    std::vector<std::size_t> m_hitTestEntries;

    // Reused for DCs that cannot expose a compatible top-down DIB.
    std::vector<COLORREF> m_bitmapBits;

    Options m_options; // Current options
    double m_lx = 0.0; // Derived parameters
    double m_ly = 0.0;
    double m_lz = 0.0;
};

//
// CTreeMapPreview. A child window, which demonstrates the options
// with an own little demo tree.
//
class CTreeMapPreview final : public CStatic
{
public:
    CTreeMapPreview();
    ~CTreeMapPreview() override;
    void SetOptions(const CTreeMap::Options* options);

protected:
    void BuildDemoData();

    CItem* m_root;                  // Demo tree
    CTreeMap m_treeMap;             // Our treemap creator

    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
};
