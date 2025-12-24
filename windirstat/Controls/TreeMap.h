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

//
// CColorSpace. Helper class for manipulating colors. Static members only.
//
class CColorSpace final
{
public:
    // Returns the brightness of color. Brightness is a value between 0 and 1.0.
    static constexpr double GetColorBrightness(COLORREF color)
    {
        const unsigned int crIndividualIntensitySum = GetRValue(color) + GetGValue(color) + GetBValue(color);
        return crIndividualIntensitySum / 255.0 / 3.0;
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
// CTreeMap. Can create a treemap. Knows 3 squarification methods:
// KDirStat-like, SequoiaView-like and Simple.
//
// This class is fairly reusable.
//
class CTreeMap final
{
public:
    // One of these flags can be added to the COLORREF returned
    // by TmiGetGraphColor(). Used for <Free space> (darker)
    // and <Unknown> (brighter).
    //
    static constexpr DWORD COLORFLAG_DARKER  = 0x01000000;
    static constexpr DWORD COLORFLAG_LIGHTER = 0x02000000;
    static constexpr DWORD COLORFLAG_MASK    = 0x03000000;

    //
    // Item. Interface which must be supported by the tree items.
    // If you prefer to use the getHead()/getNext() pattern rather
    // than using an array for the children, you will have to
    // rewrite CTreeMap.
    //
    class Item
    {
    public:
        virtual ~Item() = default;
        virtual bool TmiIsLeaf() const = 0;
        virtual CRect TmiGetRectangle() const = 0;
        virtual void TmiSetRectangle(const CRect& rc) = 0;
        virtual COLORREF TmiGetGraphColor() const = 0;
        virtual int TmiGetChildCount() const = 0;
        virtual Item* TmiGetChild(int c) const = 0;
        virtual ULONGLONG TmiGetSize() const = 0;
    };

    //
    // Treemap squarification style.
    //
    enum STYLE : std::uint8_t
    {
        KDirStatStyle,   // Children are laid out in rows. Similar to the style used by KDirStat.
        SequoiaViewStyle // The classical squarification as described at https://www.win.tue.nl/~vanwijk/
    };

    //
    // Collection of all treemap options.
    //
    struct Options
    {
        STYLE style;         // Squarification method
        bool grid;           // Whether or not to draw grid lines
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

    // Good values
    static Options GetDefaults();

    // Construct the treemap generator and register the callback interface.
    CTreeMap();

    // Alter the options
    void SetOptions(const Options* options);
    Options GetOptions() const;

#ifdef _DEBUG
    // DEBUG function
    void RecurseCheckTree(const Item *item);
#endif // _DEBUG

    // Create and draw a treemap
    void DrawTreeMap(CDC* pdc, CRect rc, Item* root, const Options* options = nullptr);

    // In the resulting treemap, find the item below a given coordinate.
    // Return value can be NULL, iff point is outside root rect.
    Item* FindItemByPoint(Item* item, CPoint point);

    // Draws a sample rectangle in the given style (for color legend)
    void DrawColorPreview(CDC* pdc, const CRect& rc, COLORREF color, const Options* options = nullptr);

protected:

    // KDirStat-like squarification
    bool KDirStat_ArrangeChildren(const Item* parent, std::vector<double>& childWidth, std::vector<double>& rows, std::vector<int>& childrenPerRow) const;
    double KDirStat_CalculateNextRow(const Item* parent, int nextChild, double width, int& childrenUsed, std::vector<double>& childWidth) const;

    // Returns true, if height and scaleFactor are > 0 and ambientLight is < 1.0
    bool IsCushionShading() const;

    // Leaves space for grid and then calls RenderRectangle()
    void RenderLeaf(std::vector<COLORREF>& bitmap, const Item* item, const std::array<double, 4>& surface) const;

    // Either calls DrawCushion() or DrawSolidRect()
    void RenderRectangle(std::vector<COLORREF>& bitmap, const CRect& rc, const std::array<double, 4>& surface, DWORD color) const;

    // Draws the surface using SetPixel()
    void DrawCushion(std::vector<COLORREF>& bitmap, const CRect& rc, const std::array<double, 4>& surface, COLORREF col, double brightness) const;

    // Draws the surface using FillSolidRect()
    void DrawSolidRect(std::vector<COLORREF>& bitmap, const CRect& rc, COLORREF col, double brightness) const;

    // Adds a new ridge to surface
    static void AddRidge(const CRect& rc, std::array<double, 4>& surface, double h);

    // Default tree map options
    static constexpr Options DefaultOptions = {
        .style = KDirStatStyle,
        .grid = false,
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

    CRect m_RenderArea;

    Options m_Options; // Current options
    double m_Lx = 0.0; // Derived parameters
    double m_Ly = 0.0;
    double m_Lz = 0.0;
};

//
// CTreeMapPreview. A child window, which demonstrates the options
// with an own little demo tree.
//
class CTreeMapPreview final : public CStatic
{
    //
    // CItem. Element of the demo tree.
    //
    class CItem final : public CTreeMap::Item
    {
    public:
        CItem(const int size, const COLORREF color)
            : m_Size(size)
              , m_Color(color)
        {
        }

        CItem(const std::vector<CItem*>& children)
        {
            m_Size = 0;
            for (const auto & child : children)
            {
                m_Children.emplace_back(child);
                m_Size += static_cast<int>(child->TmiGetSize());
            }

            std::ranges::sort(m_Children, [](auto a, auto b) {return a->m_Size > b->m_Size; });
        }

        ~CItem() override
        {
            for (const auto & child : m_Children)
            {
                delete child;
            }
        }

        bool TmiIsLeaf() const override
        {
            return m_Children.empty();
        }

        CRect TmiGetRectangle() const override
        {
            return m_Rect;
        }

        void TmiSetRectangle(const CRect& rc) override
        {
            m_Rect = rc;
        }

        COLORREF TmiGetGraphColor() const override
        {
            return m_Color;
        }

        int TmiGetChildCount() const override
        {
            return static_cast<int>(m_Children.size());
        }

        Item* TmiGetChild(const int c) const override
        {
            return m_Children[c];
        }

        ULONGLONG TmiGetSize() const override
        {
            return m_Size;
        }

    private:
        std::vector<CItem*> m_Children; // Our children
        int m_Size = 0;                    // Our size (in fantasy units)
        COLORREF m_Color = CLR_INVALID;    // Our color
        CRect m_Rect;                      // Our Rectangle in the treemap
    };

public:
    CTreeMapPreview();
    ~CTreeMapPreview() override;
    void SetOptions(const CTreeMap::Options* options);

protected:
    void BuildDemoData();
    COLORREF GetNextColor(int& i) const;

    std::vector<COLORREF> m_Colors; // Our color palette
    CItem* m_Root;                  // Demo tree
    CTreeMap m_TreeMap;             // Our treemap creator

    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
};
