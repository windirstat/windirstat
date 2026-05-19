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
#include "IconHandler.h"
#include "HelpersInterface.h"

CIconHandler::~CIconHandler()
{
    m_lookupQueue.SuspendExecution();
    m_lookupQueue.CancelExecution();
}

void CIconHandler::Initialize()
{
    static std::once_flag s_once;
    std::call_once(s_once, [this]
    {
        m_junctionImage = Icons::IconFromFontChar(L'⤷', RGB(0x3A, 0x3A, 0xFF), true);
        m_symlinkImage = Icons::IconFromFontChar(L'⤷', RGB(0x3A, 0xFF, 0x3A), true);
        m_junctionProtected = Icons::IconFromFontChar(L'⤷', RGB(0xFF, 0x3A, 0x3A), true);
        m_freeSpaceImage = Icons::IconFromFontChar(L'▢', RGB(0x3A, 0xCC, 0x3A), true);
        m_emptyImage = Icons::IconFromFontChar(L'▢', DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
        m_hardlinksImage = Icons::IconFromFontChar(L'⧉', DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
        m_dupesImage = Icons::IconFromFontChar(L'⧈', DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
        m_searchImage = Icons::IconFromFontChar(L'⊙', DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
        m_largestImage = Icons::IconFromFontChar(L'⋙', DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
        m_unknownImage = Icons::IconFromFontChar(L'?', RGB(0xCC,0xB8,0x66), true);
        m_defaultFileImage = FetchShellIcon(GetSysDirectory() + L"\\~", 0, FILE_ATTRIBUTE_NORMAL);
        m_defaultFolderImage = FetchShellIcon(GetSysDirectory() + L"\\~", 0, FILE_ATTRIBUTE_DIRECTORY);

        // Cache icon for boot drive
        std::wstring drive(MAX_PATH, wds::chrNull);
        drive.resize(min(wcslen(L"C:\\"), GetWindowsDirectory(drive.data(), MAX_PATH)));
        m_mountPointImage = FetchShellIcon(drive, 0, FILE_ATTRIBUTE_REPARSE_POINT);

        // Cache icon for my computer
        m_myComputerImage = FetchShellIcon(std::to_wstring(CSIDL_DRIVES), SHGFI_PIDL);

        // Use two threads for asynchronous icon lookup
        m_lookupQueue.StartThreads(MAX_ICON_THREADS, [this]
        {
            if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) return;
            for (auto itemOpt = m_lookupQueue.Pop(); itemOpt.has_value(); itemOpt = m_lookupQueue.Pop())
            {
                // Fetch item from queue
                auto& [item, control, path, attr, icon, desc] = itemOpt.value();

                // Query the icon from the system
                std::wstring descTmp;
                const HICON iconTmp = FetchShellIcon(
                    path, 0, attr, desc != nullptr ? &descTmp : nullptr);

                // Join the UI thread and see if the item still exists
                // since it could have been deleted since originally
                // requested
                CMainFrame::Get()->InvokeInMessageThread([&]
                {
                    const auto i = control->FindListItem(item);
                    if (i == -1 || !item->IsVisible()) return;

                    *icon = iconTmp;
                    if (desc != nullptr) *desc = descTmp;
                    control->RedrawItems(i, i);
                });
            }
            CoUninitialize();
        });
    });
}

void CIconHandler::DoAsyncShellInfoLookup(IconLookup&& lookupInfo)
{
    const auto& [item, control, path, attr, icon, desc] = lookupInfo;

    // set default icon while loading
    if (*icon == nullptr)
    {
        *icon = (attr & FILE_ATTRIBUTE_DIRECTORY) ? m_defaultFolderImage : m_defaultFileImage;
    }

    // queue for lookup
    m_lookupQueue.PushIfNotQueued(std::move(lookupInfo));
}

void CIconHandler::ClearAsyncShellInfoQueue()
{
    ProcessMessagesUntilSignaled([this]
    {
        m_lookupQueue.SuspendExecution(true);
        m_lookupQueue.ResumeExecution();
    });
}

void CIconHandler::StopAsyncShellInfoQueue()
{
    ProcessMessagesUntilSignaled([this]
    {
        m_lookupQueue.SuspendExecution();
        m_lookupQueue.CancelExecution();
    });
}

void CIconHandler::DrawIcon(const CDC* hdc, const HICON image, const CPoint & pt, const CSize& sz)
{
    DrawIconEx(*hdc, pt.x, pt.y, image, sz.cx, sz.cy, 0, nullptr, DI_NORMAL);
}

// Returns the icon handle
HICON CIconHandler::FetchShellIcon(const std::wstring & path, UINT flags, const DWORD attr, std::wstring* psTypeName)
{
    flags |= WDS_SHGFI_DEFAULTS;

    // Also retrieve the file type description
    if (psTypeName != nullptr && !path.empty()) flags |= SHGFI_TYPENAME;

    SHFILEINFO sfi{};
    bool success = false;

    if (flags & SHGFI_PIDL)
    {
        // Assume folder id numeric encoded as string
        SmartPointer pidl(CoTaskMemFree, static_cast<LPITEMIDLIST>(nullptr));
        if (SUCCEEDED(SHGetSpecialFolderLocation(nullptr, std::stoi(path), &pidl)))
        {
            success = std::bit_cast<HIMAGELIST>(::SHGetFileInfo(
                static_cast<LPCWSTR>(static_cast<LPVOID>(pidl)), attr, &sfi,
                sizeof(sfi), flags)) != nullptr;
        }
    }
    else
    {
        success = std::bit_cast<HIMAGELIST>(::SHGetFileInfo(path.c_str(),
            attr, &sfi, sizeof(sfi), flags)) != nullptr;
    }

    if (!success)
    {
        VTRACE(L"SHGetFileInfo() failed: {}", path);
        return GetEmptyImage();
    }

    if (psTypeName != nullptr)
    {
        *psTypeName = path.empty() ?
            Localization::Lookup(IDS_EXTENSION_MISSING) : sfi.szTypeName;
    }

    std::scoped_lock lock(m_cachedIconMutex);
    if (const auto it = m_cachedIcons.find(sfi.iIcon); it != m_cachedIcons.end())
    {
        if (sfi.hIcon != nullptr) DestroyIcon(sfi.hIcon);
        return it->second;
    }

    m_cachedIcons[sfi.iIcon] = sfi.hIcon;
    return sfi.hIcon;
}

namespace Icons
{
    using namespace Gdiplus;

    static void PaintDocument(Graphics& g)
    {
        Pen outlinePen(Neutral(), 4);
        SolidBrush lineBrush(Neutral());
        Point body[] = { {8, 6}, {44, 6}, {56, 18}, {56, 58}, {8, 58} };
        g.DrawPolygon(&outlinePen, body, 5);
        g.DrawLine(&outlinePen, 44, 6, 44, 18);
        g.DrawLine(&outlinePen, 44, 18, 56, 18);
    }

    static void PaintBin(Graphics& g, Color body, Color bar)
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

    void PaintDelete(Graphics& g)    { PaintBin(g, C(204, 0, 0), C(204, 40, 40)); }
    void PaintDeleteBin(Graphics& g) { PaintBin(g, Neutral(), Neutral()); }

    void PaintExplorerSelect(Graphics& g)
    {
        PaintDocument(g);
        SolidBrush cursorBrush(C(0, 102, 204));
        Point cursor[] = {
            {21, 14}, {21, 44}, {29, 36},
            {37, 48}, {43, 44}, {35, 32}, {45, 32}
        };
        g.FillPolygon(&cursorBrush, cursor, 7);
    }

    void PaintOpenInConsole(Graphics& g)
    {
        SolidBrush grayBrush(Neutral());
        Pen framePen(Neutral(), 4);
        Pen chevronPen(Neutral(), 4);
        chevronPen.SetStartCap(LineCapRound);
        chevronPen.SetEndCap(LineCapRound);
        chevronPen.SetLineJoin(LineJoinRound);
        g.DrawRectangle(&framePen, 6, 8, 52, 48);
        g.FillRectangle(&grayBrush, 6, 17, 52, 2);
        Point chevronPts[] = { {14, 26}, {22, 34}, {14, 42} };
        g.DrawLines(&chevronPen, chevronPts, 3);
        g.DrawLine(&chevronPen, 26, 42, 38, 42);
    }

    void PaintOpenSelected(Graphics& g)
    {
        PaintDocument(g);
        SolidBrush greenBrush(C(40, 140, 50));
        Point triangle[] = { {20, 18}, {44, 32}, {20, 46} };
        g.FillPolygon(&greenBrush, triangle, 3);
    }

    void PaintRefreshSelected(Graphics& g)
    {
        PaintDocument(g);
        Pen arcPen(C(0, 156, 221), 4);
        arcPen.SetStartCap(LineCapRound);
        arcPen.SetEndCap(LineCapRound);
        g.DrawArc(&arcPen, Rect(18, 20, 27, 27), -45, 270);
        SolidBrush brush(C(0, 156, 221));
        Point arrow[] = { {26,20},{24,30},{16,22} };
        g.FillPolygon(&brush, arrow, static_cast<INT>(std::size(arrow)));
    }

    void PaintProperties(Graphics& g)
    {
        PaintDocument(g);
        SolidBrush blueBrush(C(0, 102, 204));
        g.FillEllipse(&blueBrush, 26, 15, 8, 8);
        g.FillRectangle(&blueBrush, 27, 27, 6, 23);
    }

    void PaintEditCopyClipboard(Graphics& g)
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

    void PaintFileSelect(Graphics& g)
    {
        SolidBrush folderBrush(C(210, 160, 40));
        g.FillRectangle(&folderBrush,  2,  4, 30,  8);
        g.FillRectangle(&folderBrush,  2, 12, 60, 48);
        SolidBrush ringRed(C(200, 30, 30)), ringWhite(C(240, 240, 240)), ringRed2(C(200, 30, 30));
        g.FillEllipse(&ringRed,   14, 18, 36, 36);
        g.FillEllipse(&ringWhite, 21, 25, 22, 22);
        g.FillEllipse(&ringRed2,  27, 31, 10, 10);
    }

    void PaintFilter(Graphics& g, bool active)
    {
        Point funnelShape[] = { {8, 16}, {56, 16}, {40, 34}, {40, 58}, {24, 58}, {24, 34} };
        if (active)
        {
            SolidBrush activeBrush(C(255, 140, 0));
            g.FillPolygon(&activeBrush, funnelShape, 6);
        }
        SolidBrush darkBrush(Neutral());
        Pen outlinePen(Neutral(), 5);
        outlinePen.SetLineJoin(LineJoinMiter);
        g.FillRectangle(&darkBrush, 4, 8, 56, 6);
        g.DrawPolygon(&outlinePen, funnelShape, 6);
    }

    void PaintHelp(Graphics& g)
    {
        Color blue = C(100, 149, 237);
        Pen pen(blue, 10);
        pen.SetStartCap(LineCapRound);
        pen.SetEndCap(LineCapRound);
        g.DrawArc(&pen, Rect(18, 8, 28, 28), 180, 270);
        SolidBrush brush(blue);
        g.FillEllipse(&brush, 27, 48, 10, 10);
    }

    void PaintPause(Graphics& g)
    {
        SolidBrush amberBrush(C(200, 160, 0));
        g.FillRectangle(&amberBrush, 5, 5, 19, 54);
        g.FillRectangle(&amberBrush, 39, 5, 19, 54);
    }

    void PaintMagnifier(Graphics& g, bool plus)
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

    void PaintGear(Graphics& g)
    {
        SolidBrush gearBrush(Neutral());
        const Point primaryTooth[] = { {-3, -27}, {3, -27}, {6, -17}, {-6, -17} };

        GraphicsState state = g.Save();
        g.TranslateTransform(32, 32);

        for (int i = 0; i < 8; ++i)
        {
            g.FillPolygon(&gearBrush, primaryTooth, 4);
            g.RotateTransform(45);
        }
        g.Restore(state);

        Pen ringPen(Neutral(), 8);
        g.DrawEllipse(&ringPen, 15, 15, 34, 34);

        Pen bearingPen(Neutral(), 1);
        g.DrawEllipse(&bearingPen, 23, 23, 18, 18);
    }

    void PaintCharacter(Graphics& g, WCHAR ch, COLORREF clr, bool bold, LPCWSTR fontName)
    {
        const WCHAR text[]{ ch, L'\0' };
        FontFamily fontFamily(fontName);
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        format.SetFormatFlags(StringFormatFlagsNoClip);

        GraphicsPath path;
        path.AddString(text, 1, &fontFamily,
            bold ? FontStyleBold : FontStyleRegular, 56,
            Rect(0, 0, 64, 64), &format);

        Rect bounds;
        path.GetBounds(&bounds);
        if (bounds.Width <= 0 || bounds.Height <= 0) return;

        const REAL scale = min(56.0f / bounds.Width, 56.0f / bounds.Height);
        Matrix m;
        m.Scale(scale, scale);
        path.Transform(&m);
        path.GetBounds(&bounds);
        m.Reset();
        m.Translate((64.0f - bounds.Width) / 2.0f - bounds.X,
                    (64.0f - bounds.Height) / 2.0f - bounds.Y);
        path.Transform(&m);

        SolidBrush brush(Color(255, GetRValue(clr), GetGValue(clr), GetBValue(clr)));
        g.FillPath(&brush, &path);
    }

    std::function<void(Graphics&)> Char(WCHAR ch, COLORREF clr)
    {
        return [=](Graphics& g) { PaintCharacter(g, ch, clr); };
    }

    HICON IconFromFontChar(WCHAR ch, COLORREF clr, bool bold, LPCWSTR fontName, int iconSize)
    {
        const int size = iconSize > 0 ? iconSize : GetSystemMetrics(SM_CXSMICON);
        return MakeIcon(size, [=](Graphics& g) { PaintCharacter(g, ch, clr, bold, fontName); });
    }
}

HBITMAP Icons::MakeBitmap(const int size, const std::function<void(Graphics&)>& painter)
{
    // Render directly at the target size in the canonical 64-unit space.
    // GDI+'s native antialiasing at the destination resolution produces
    // crisper edges than super-sampling and bicubic downsampling, which
    // compounds two AA passes and introduces ringing on thin strokes.
    Bitmap renderBitmap(size, size, PixelFormat32bppPARGB);
    Graphics g(&renderBitmap);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHalf);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    const REAL scale = static_cast<REAL>(size) / 64.0f;
    g.ScaleTransform(scale, scale);
    painter(g);

    HBITMAP hbm = nullptr;
    renderBitmap.GetHBITMAP(Color(0, 0, 0, 0), &hbm);
    return hbm;
}

HICON Icons::MakeIcon(const int size, const std::function<void(Graphics&)>& painter)
{
    SmartPointer color(DeleteObject, MakeBitmap(size, painter));
    if (color == nullptr) return nullptr;

    CBitmap mask;
    mask.CreateBitmap(size, size, 1, 1, nullptr);

    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmColor = color;
    ii.hbmMask = static_cast<HBITMAP>(mask.m_hObject);
    return CreateIconIndirect(&ii);
}
