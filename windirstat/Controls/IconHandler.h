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
    static HICON IconFromFontChar(WCHAR ch, COLORREF textColor, bool bold = false, LPCWSTR fontName = L"Cambria Math", int iconSize = 0);

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

    // A neutral gray that reads against both light AND dark toolbar backgrounds.
    inline Color Neutral() { return C(140, 140, 140); }

    inline void PaintBin(Graphics& g, Color body, Color bar)
    {
        SolidBrush bodyBrush(body), barBrush(bar);
        g.FillRectangle(&bodyBrush, 6.0f, 1.5f, 4.0f, 1.5f);
        g.FillRectangle(&bodyBrush, 1.5f, 3.0f, 13.0f, 1.5f);
        g.FillRectangle(&bodyBrush, 3.0f, 4.5f, 1.0f, 10.0f);
        g.FillRectangle(&bodyBrush, 12.0f, 4.5f, 1.0f, 10.0f);
        g.FillRectangle(&bodyBrush, 3.0f, 13.5f, 10.0f, 1.0f);
        for (int i = 0; i < 3; ++i)
            g.FillRectangle(&barBrush, 5.625f + i * 2.0f, 6.5f, 0.75f, 6.0f);
    }
    inline void PaintDelete(Graphics& g) { PaintBin(g, C(204, 0, 0), C(204, 80, 80)); }
    inline void PaintDeleteBin(Graphics& g) { PaintBin(g, Neutral(), Neutral()); }

    inline void PaintExplorerSelect(Graphics& g)
    {
        SolidBrush grayBrush(Neutral()), blueBrush(C(0, 102, 204));
        PointF dots[] = { {1.0f, 3.0f}, {4.0f, 1.0f}, {6.5f, 3.0f},
                          {1.0f, 6.0f}, {4.0f, 6.5f} };
        for (auto& p : dots) g.FillRectangle(&grayBrush, p.X, p.Y, 1.5f, 1.5f);
        PointF arrow[] = { {6.0f, 7.0f}, {13.5f, 11.5f}, {10.0f, 11.5f},
                           {12.0f, 15.5f}, {10.0f, 15.5f}, {8.0f, 12.0f}, {6.0f, 14.0f} };
        g.FillPolygon(&blueBrush, arrow, 7);
    }

    inline void PaintOpenInConsole(Graphics& g)
    {
        SolidBrush grayBrush(Neutral());
        Pen framePen(Neutral(), 1.0f);
        Pen chevronPen(Neutral(), 1.1f);
        chevronPen.SetStartCap(LineCapRound);
        chevronPen.SetEndCap(LineCapRound);
        chevronPen.SetLineJoin(LineJoinRound);
        Pen promptPen(C(0, 102, 204), 2.5f);
        promptPen.SetStartCap(LineCapRound);
        promptPen.SetEndCap(LineCapRound);
        g.DrawRectangle(&framePen, 1.5f, 2.0f, 13.0f, 12.0f);
        g.FillRectangle(&grayBrush, 1.5f, 4.25f, 13.0f, 0.5f);
        PointF chevronPts[] = { {3.5f, 6.5f}, {5.5f, 8.5f}, {3.5f, 10.5f} };
        g.DrawLines(&chevronPen, chevronPts, 3);
        g.DrawLine(&chevronPen, 6.5f, 10.5f, 9.5f, 10.5f);
    }

    inline void PaintDocument(Graphics& g)
    {
        Pen outlinePen(Neutral(), 1.0f);
        SolidBrush lineBrush(Neutral());
        PointF body[] = { {2.0f, 1.5f}, {11.0f, 1.5f}, {14.0f, 4.5f},
                          {14.0f, 14.5f}, {2.0f, 14.5f} };
        g.DrawPolygon(&outlinePen, body, 5);
        g.DrawLine(&outlinePen, 11.0f, 1.5f, 11.0f, 4.5f);
        g.DrawLine(&outlinePen, 11.0f, 4.5f, 14.0f, 4.5f);
    }

    inline void PaintOpenSelected(Graphics& g)
    {
        PaintDocument(g);
        SolidBrush greenBrush(C(40, 140, 50));
        PointF triangle[] = { {6.0f, 4.5f}, {12.0f, 8.0f}, {6.0f, 11.5f} };
        g.FillPolygon(&greenBrush, triangle, 3);
    }

    inline void PaintRefreshSelected(Graphics& g)
    {
        PaintDocument(g);
        SolidBrush blueBrush(C(0, 102, 204));
        Pen arcPen(C(0, 102, 204), 1.75f);
        arcPen.SetStartCap(LineCapRound);
        g.DrawArc(&arcPen, 5.0f, 5.0f, 6.0f, 6.0f, 330.0f, 300.0f);
        PointF arrowHead[] = { {7.5f, 3.5f}, {10.5f, 5.0f}, {7.5f, 6.5f} };
        g.FillPolygon(&blueBrush, arrowHead, 3);
    }

    inline void PaintProperties(Graphics& g)
    {
        PaintDocument(g);
        SolidBrush blueBrush(C(0, 102, 204));
        g.FillEllipse(&blueBrush, 6.5f, 3.75f, 2.0f, 2.0f);
        g.FillRectangle(&blueBrush, 6.75f, 6.75f, 1.5f, 5.75f);
    }

    inline void PaintEditCopyClipboard(Graphics& g)
    {
        SolidBrush goldBrush(C(200, 140, 30)), goldLightBrush(C(230, 180, 80)),
            clipBrush(C(120, 90, 20)), grayBrush(C(180, 180, 180));
        Pen outlinePen(Neutral(), 0.75f);
        g.FillRectangle(&goldBrush, 1.0f, 2.5f, 9.0f, 12.5f);
        g.FillRectangle(&clipBrush, 3.0f, 0.5f, 5.0f, 2.5f);
        g.FillRectangle(&goldLightBrush, 4.0f, 1.25f, 3.0f, 0.9f);
        SolidBrush whiteBrush(C(255, 255, 255));
        g.FillRectangle(&whiteBrush, 7.0f, 6.0f, 8.0f, 9.0f);
        g.DrawRectangle(&outlinePen, 7.0f, 6.0f, 8.0f, 9.0f);
        for (int i = 0; i < 3; ++i)
            g.FillRectangle(&grayBrush, 8.25f, 8.0f + i * 1.75f, 5.5f, 0.7f);
    }

    inline void PaintFileSelect(Graphics& g)
    {
        SolidBrush folderBrush(C(180, 130, 20));
        Pen magnifierPen(C(0, 102, 204), 1.5f);
        magnifierPen.SetEndCap(LineCapRound);
        g.FillRectangle(&folderBrush, 1.0f, 2.5f, 6.0f, 1.5f);
        g.FillRectangle(&folderBrush, 1.0f, 4.0f, 14.0f, 1.0f);
        g.FillRectangle(&folderBrush, 1.0f, 12.0f, 14.0f, 1.0f);
        g.FillRectangle(&folderBrush, 1.0f, 4.0f, 1.0f, 9.0f);
        g.FillRectangle(&folderBrush, 14.0f, 4.0f, 1.0f, 9.0f);
        g.DrawEllipse(&magnifierPen, 8.0f, 7.0f, 5.5f, 5.5f);
        g.DrawLine(&magnifierPen, 13.0f, 12.0f, 14.75f, 14.75f);
    }

    inline void PaintFilter(Graphics& g)
    {
        SolidBrush darkBrush(Neutral());
        Pen outlinePen(Neutral(), 1.2f);
        outlinePen.SetLineJoin(LineJoinMiter);
        g.FillRectangle(&darkBrush, 1.0f, 2.5f, 14.0f, 1.5f);
        PointF funnelOutline[] = { {2.0f, 4.5f}, {14.0f, 4.5f}, {10.0f, 9.0f},
                                   {10.0f, 15.0f}, {6.0f, 15.0f}, {6.0f, 9.0f} };
        g.DrawPolygon(&outlinePen, funnelOutline, 6);
    }

    inline void PaintMagnifier(Graphics& g, bool plus)
    {
        SolidBrush blueBrush(C(0, 102, 204));
        Pen rimPen(Neutral(), 1.5f);
        Pen handlePen(Neutral(), 2.5f);
        handlePen.SetEndCap(LineCapRound);
        g.DrawEllipse(&rimPen, 1.5f, 1.5f, 10.0f, 10.0f);
        g.DrawLine(&handlePen, 9.75f, 9.75f, 14.5f, 14.5f);
        g.FillRectangle(&blueBrush, 3.5f, 5.7f, 6.0f, 1.6f);
        if (plus) g.FillRectangle(&blueBrush, 5.7f, 3.5f, 1.6f, 6.0f);
    }

    template <auto Painter>
    HBITMAP Make(int width, int height)
    {
        Bitmap bitmap(width, height, PixelFormat32bppPARGB);
        Graphics g(&bitmap);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHalf);
        g.ScaleTransform(static_cast<REAL>(width) / 16.0f,
            static_cast<REAL>(height) / 16.0f);
        Painter(g);
        HBITMAP hbitmap = nullptr;
        bitmap.GetHBITMAP(Color(0, 0, 0, 0), &hbitmap);
        return hbitmap;
    }

    inline HBITMAP CreateGlyphBitmap(WCHAR ch, COLORREF color, int size)
    {
        // Render with premultiplied alpha so the bitmap is transparent in menus
        // when used with MIIM_BITMAP / SetMenuItemInfo.
        SmartPointer<HICON> hIcon(DestroyIcon, CIconHandler::IconFromFontChar(ch, color, true, L"Segoe UI Symbol", size));
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
