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
#include "SelectObject.h"
#include "SmartPointer.h"

//
// CIconHandler. Handles all shell information lookup.
//
class CIconHandler final
{
    static constexpr UINT WDS_SHGFI_DEFAULTS = SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON | SHGFI_ICON | SHGFI_ADDOVERLAYS | SHGFI_SYSICONINDEX | SHGFI_OVERLAYINDEX;
    static constexpr auto MAX_ICON_THREADS = 4;

    std::mutex m_cachedIconMutex;
    std::unordered_map<int, HICON> m_cachedIcons;

public:
    CIconHandler() = default;
    ~CIconHandler();

    using IconLookup = std::tuple<CWdsListItem*, CWdsListControl*,
        std::wstring, DWORD, HICON*, std::wstring*>;

    void Initialize();
    void DoAsyncShellInfoLookup(IconLookup&& lookupInfo);
    void DrawIcon(const CDC* hdc, HICON image, const CPoint& pt, const CSize& sz);
    void ClearAsyncShellInfoQueue();
    void StopAsyncShellInfoQueue();

    HICON FetchShellIcon(const std::wstring& path, UINT flags = 0, DWORD attr = FILE_ATTRIBUTE_NORMAL, std::wstring* psTypeName = nullptr);
    static HICON IconFromFontChar(WCHAR ch, COLORREF textColor, bool bold = false, LPCWSTR fontName = wds::strFontCambriaMath, int iconSize = 0);

    BlockingQueue<IconLookup> m_lookupQueue = BlockingQueue<IconLookup>(false);

    HICON m_freeSpaceImage = nullptr;    // <Free Space>
    HICON m_unknownImage = nullptr;      // <Unknown>
    HICON m_hardlinksImage = nullptr;    // <Hardlinks>
    HICON m_dupesImage = nullptr;        // <Duplicates>
    HICON m_searchImage = nullptr;       // <Search>
    HICON m_largestImage = nullptr;      // <Largest>
    HICON m_emptyImage = nullptr;        // For items whose icon cannot be found
    HICON m_defaultFileImage = nullptr;  // Generic file icon while loading
    HICON m_defaultFolderImage = nullptr;// Generic folder icon while loading
    HICON m_junctionImage = nullptr;     // For normal junctions
    HICON m_symlinkImage = nullptr;      // For symbolic links
    HICON m_junctionProtected = nullptr; // For protected junctions
    HICON m_mountPointImage = nullptr;   // Mount point icon
    HICON m_myComputerImage = nullptr;   // My computer icon

    // Trivial getters
    HICON GetMyComputerImage() const { return m_myComputerImage; }
    HICON GetMountPointImage() const { return m_mountPointImage; }
    HICON GetJunctionImage() const { return m_junctionImage; }
    HICON GetSymbolicLinkImage() const { return m_symlinkImage; }
    HICON GetJunctionProtectedImage() const { return m_junctionProtected; }
    HICON GetFreeSpaceImage() const { return m_freeSpaceImage; }
    HICON GetUnknownImage() const { return m_unknownImage; }
    HICON GetEmptyImage() const { return m_emptyImage; }
    HICON GetHardlinksImage() const { return m_hardlinksImage; }
    HICON GetDupesImage() const { return m_dupesImage; }
    HICON GetSearchImage() const { return m_searchImage; }
    HICON GetLargestImage() const { return m_largestImage; }
};

namespace Icons
{
    using namespace Gdiplus;

    inline Color C(BYTE r, BYTE g, BYTE b, BYTE a = 255) { return Color(a, r, g, b); }
    inline Color Neutral() { return C(140, 140, 140); }

    inline void PaintDocument(Graphics& g)
    {
        Pen outlinePen(Neutral(), 4);
        SolidBrush lineBrush(Neutral());
        PointF body[] = { {8, 6}, {44, 6}, {56, 18}, {56, 58}, {8, 58} };
        g.DrawPolygon(&outlinePen, body, 5);
        g.DrawLine(&outlinePen, 44, 6, 44, 18);
        g.DrawLine(&outlinePen, 44, 18, 56, 18);
    }

    inline void PaintBin(Graphics& g, Color body, Color bar)
    {
        SolidBrush bodyBrush(body), barBrush(bar);
        g.FillRectangle(&bodyBrush, 24, 6,  16, 6);
        g.FillRectangle(&bodyBrush,  6, 12, 52, 6);
        g.FillRectangle(&bodyBrush, 12, 18,  4, 40);
        g.FillRectangle(&bodyBrush, 48, 18,  4, 40);
        g.FillRectangle(&bodyBrush, 12, 54, 40, 4);
        for (int i = 0; i < 3; ++i)
            g.FillRectangle(&barBrush, 23 + i * 8, 26, 3, 24);
    }
    inline void PaintDelete(Graphics& g) { PaintBin(g, C(204, 0, 0), C(204, 40, 40)); }
    inline void PaintDeleteBin(Graphics& g) { PaintBin(g, Neutral(), Neutral()); }

    inline void PaintExplorerSelect(Graphics& g)
    {
        PaintDocument(g);
        SolidBrush cursorBrush(C(0, 102, 204));
        PointF cursor[] = {
            {21, 14}, {21, 44}, {29, 36},
            {37, 48}, {43, 44}, {35, 32}, {45, 32}
        };
        g.FillPolygon(&cursorBrush, cursor, 7);
    }

    inline void PaintOpenInConsole(Graphics& g)
    {
        SolidBrush grayBrush(Neutral());
        Pen framePen(Neutral(), 4);
        Pen chevronPen(Neutral(), 4);
        chevronPen.SetStartCap(LineCapRound);
        chevronPen.SetEndCap(LineCapRound);
        chevronPen.SetLineJoin(LineJoinRound);
        Pen promptPen(C(0, 102, 204), 10);
        promptPen.SetStartCap(LineCapRound);
        promptPen.SetEndCap(LineCapRound);
        g.DrawRectangle(&framePen, 6, 8, 52, 48);
        g.FillRectangle(&grayBrush, 6, 17, 52, 2);
        PointF chevronPts[] = { {14, 26}, {22, 34}, {14, 42} };
        g.DrawLines(&chevronPen, chevronPts, 3);
        g.DrawLine(&chevronPen, 26, 42, 38, 42);
    }

    inline void PaintOpenSelected(Graphics& g)
    {
        PaintDocument(g);
        SolidBrush greenBrush(C(40, 140, 50));
        PointF triangle[] = { {20, 18}, {44, 32}, {20, 46} };
        g.FillPolygon(&greenBrush, triangle, 3);
    }

    inline void PaintRefreshSelected(Graphics& g)
    {
        PaintDocument(g);
        Pen arcPen(C(0, 156, 221), 4);
        arcPen.SetStartCap(LineCapRound);
        arcPen.SetEndCap(LineCapRound);
        g.DrawArc(&arcPen, Rect(18, 20, 27, 27), -45, 270);

        SolidBrush brush(C(0, 156, 221));
        Point arrow[] = { {17,34},{20,24},{16,19},{26,20},{27,30},{23,27},{22,34} };
        g.FillPolygon(&brush, arrow, static_cast<INT>(std::size(arrow)));
    }

    inline void PaintProperties(Graphics& g)
    {
        PaintDocument(g);
        SolidBrush blueBrush(C(0, 102, 204));
        g.FillEllipse(&blueBrush, 26, 15, 8, 8);
        g.FillRectangle(&blueBrush, 27, 27, 6, 23);
    }

    inline void PaintEditCopyClipboard(Graphics& g)
    {
        SolidBrush goldBrush(C(200, 140, 30)), goldLightBrush(C(230, 180, 80)),
            clipBrush(C(120, 90, 20)), grayBrush(C(180, 180, 180));
        Pen outlinePen(Neutral(), 3);
        g.FillRectangle(&goldBrush, 4, 10, 36, 50);
        g.FillRectangle(&clipBrush, 12, 2, 20, 10);
        g.FillRectangle(&goldLightBrush, 16, 5, 12, 4);
        SolidBrush whiteBrush(C(255, 255, 255));
        g.FillRectangle(&whiteBrush, 28, 24, 32, 36);
        g.DrawRectangle(&outlinePen, 28, 24, 32, 36);
        for (int i = 0; i < 3; ++i)
            g.FillRectangle(&grayBrush, 33, 32 + i * 7, 22, 3);
    }

    inline void PaintFileSelect(Graphics& g)
    {
        SolidBrush folderBrush(C(210, 160, 40));
        g.FillRectangle(&folderBrush,  2,  4, 30,  8); // tab
        g.FillRectangle(&folderBrush,  2, 12, 60, 48); // body

        // Bullseye centered in the folder body (~x=32, y=36)
        SolidBrush ringRed(C(200, 30, 30));
        SolidBrush ringWhite(C(240, 240, 240));
        SolidBrush ringRed2(C(200, 30, 30));
        g.FillEllipse(&ringRed,   14, 18, 36, 36); // outer red  (r=18)
        g.FillEllipse(&ringWhite, 21, 25, 22, 22); // white ring (r=11)
        g.FillEllipse(&ringRed2,  27, 31, 10, 10); // inner red  (r=5)
    }

    inline void PaintFilter(Graphics& g, bool active = false)
    {
        SolidBrush darkBrush(Neutral());
        Pen outlinePen(Neutral(), 5);
        outlinePen.SetLineJoin(LineJoinMiter);
        g.FillRectangle(&darkBrush, 4, 8, 56, 6);
        PointF funnelOutline[] = { {8, 16}, {56, 16}, {40, 34},
                                   {40, 58}, {24, 58}, {24, 34} };
        g.DrawPolygon(&outlinePen, funnelOutline, 6);

        if (!active) return;
        Pen asteriskPen(C(255, 140, 0), 5);
        asteriskPen.SetStartCap(LineCapRound);
        asteriskPen.SetEndCap(LineCapRound);
        g.DrawLine(&asteriskPen, 50, 42, 50, 62); // vertical
        g.DrawLine(&asteriskPen, 38, 47, 62, 57); // diagonal top-left to bottom-right
        g.DrawLine(&asteriskPen, 62, 47, 38, 57); // diagonal top-right to bottom-left
    }

    inline void PaintMagnifier(Graphics& g, bool plus)
    {
        SolidBrush blueBrush(C(0, 102, 204));
        Pen rimPen(Neutral(), 6);
        Pen handlePen(Neutral(), 10);
        handlePen.SetEndCap(LineCapRound);
        g.DrawEllipse(&rimPen, 6, 6, 40, 40);
        g.DrawLine(&handlePen, 39, 39, 58, 58);
        g.FillRectangle(&blueBrush, 14, 23, 24, 6);
        if (plus) g.FillRectangle(&blueBrush, 23, 14, 6, 24);
    }

    template <auto Painter>
    HBITMAP Make(int width, int height)
    {
        Bitmap bitmap(width, height, PixelFormat32bppPARGB);
        Graphics g(&bitmap);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.ScaleTransform(static_cast<REAL>(width) / 64,
            static_cast<REAL>(height) / 64);
        Painter(g);
        HBITMAP hbitmap = nullptr;
        bitmap.GetHBITMAP(Color(0, 0, 0, 0), &hbitmap);
        return hbitmap;
    }

    inline HBITMAP CreateGlyphBitmap(WCHAR ch, COLORREF color, int size)
    {
        // Render with premultiplied alpha so the bitmap is transparent in menus
        // when used with MIIM_BITMAP / SetMenuItemInfo.
        SmartPointer<HICON> hIcon(DestroyIcon, CIconHandler::IconFromFontChar(ch, color, true, wds::strFontSegoeUISymbol, size));
        if (hIcon == nullptr) return nullptr;

        // Create a 32bpp PARGB DIB section to hold the icon with transparency
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = size;
        bmi.bmiHeader.biHeight      = -size; // top-down
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        CDC dc;
        dc.CreateCompatibleDC(nullptr);
        CBitmap bm;
        bm.Attach(CreateDIBSection(dc.GetSafeHdc(), &bmi, DIB_RGB_COLORS, &bits, nullptr, 0));
        CSelectObject sobm(&dc, &bm);
        DrawIconEx(dc.GetSafeHdc(), 0, 0, hIcon, size, size, 0, nullptr, DI_NORMAL);
        return static_cast<HBITMAP>(bm.Detach());
    }
}
