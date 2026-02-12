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

struct CFilterGuard
{
    COleFilterOverride& f;
    explicit CFilterGuard(COleFilterOverride& x) : f(x) { f.SetDefaultHandler(false); }
    ~CFilterGuard() { f.SetDefaultHandler(true); }
};

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
        m_filterOverride.RegisterFilter();
        
        m_junctionImage = IconFromFontChar(L'⤷', RGB(0x3A, 0x3A, 0xFF), true);
        m_symlinkImage = IconFromFontChar(L'⤷', RGB(0x3A, 0xFF, 0x3A), true);
        m_junctionProtected = IconFromFontChar(L'⤷', RGB(0xFF, 0x3A, 0x3A), true);
        m_freeSpaceImage = IconFromFontChar(L'▢', RGB(0x3A, 0xCC, 0x3A), true);
        m_emptyImage = IconFromFontChar(L'▢', DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
        m_hardlinksImage = IconFromFontChar(L'⧉', DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
        m_dupesImage = IconFromFontChar(L'⧈', DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
        m_searchImage = IconFromFontChar(L'⊙', DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
        m_largestImage = IconFromFontChar(L'⋙', DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
        m_unknownImage = IconFromFontChar(L'?', RGB(0xCC,0xB8,0x66), true);
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
                    if (i == -1) return;

                    *icon = iconTmp;
                    if (desc != nullptr) *desc = descTmp;
                    control->RedrawItems(i, i);
                });
            }
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
    CFilterGuard guard(m_filterOverride);
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
        SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree);
        if (SUCCEEDED(SHGetSpecialFolderLocation(nullptr, std::stoi(path), &pidl)))
        {
            CFilterGuard guard(m_filterOverride);
            success = std::bit_cast<HIMAGELIST>(::SHGetFileInfo(
                static_cast<LPCWSTR>(static_cast<LPVOID>(pidl)), attr, &sfi,
                sizeof(sfi), flags)) != nullptr;
        }
    }
    else
    {
        CFilterGuard guard(m_filterOverride);
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

HICON CIconHandler::IconFromFontChar(const WCHAR ch, const COLORREF textColor, const bool bold, LPCWSTR fontName)
{
    const int ICON_SIZE = GetSystemMetrics(SM_CXSMICON);
    const int RENDER_SIZE = GetSystemMetrics(SM_CXSMICON) * 4;
    constexpr int PADDING = 2;

    // Lambda to create bitmap headers
    auto createBitmapHeader = [](int size) {
        BITMAPV5HEADER bi{};
        bi.bV5Size = sizeof(BITMAPV5HEADER);
        bi.bV5Width = size;
        bi.bV5Height = -size;
        bi.bV5Planes = 1;
        bi.bV5BitCount = 32;
        bi.bV5Compression = BI_RGB;
        bi.bV5AlphaMask = 0xFF000000;
        bi.bV5RedMask = 0x00FF0000;
        bi.bV5GreenMask = 0x0000FF00;
        bi.bV5BlueMask = 0x000000FF;
        return bi;
        };

    CClientDC screenDC(nullptr);
    CDC memDC;
    memDC.CreateCompatibleDC(&screenDC);

    const auto bi = createBitmapHeader(RENDER_SIZE);
    BYTE* pBits = nullptr;
    CBitmap renderBmp;
    renderBmp.Attach(CreateDIBSection(screenDC.m_hDC,
        reinterpret_cast<BITMAPINFO*>(const_cast<BITMAPV5HEADER*>(&bi)),
        DIB_RGB_COLORS, reinterpret_cast<void**>(&pBits), nullptr, 0));

    CSelectObject sobmp(&memDC, &renderBmp);

    // Clear and setup rendering
    memset(pBits, 0, RENDER_SIZE * RENDER_SIZE * 4);
    memDC.SetBkMode(TRANSPARENT);
    memDC.SetTextColor(RGB(255, 255, 255));
    memDC.SetGraphicsMode(GM_ADVANCED);

    // Create and select font
    CFont font;
    LOGFONT lf{};
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfHeight = -RENDER_SIZE + 8;
    lf.lfQuality = ANTIALIASED_QUALITY;
    wcscpy_s(lf.lfFaceName, fontName ? fontName : L"Segoe UI");
    font.CreateFontIndirect(&lf);

    CSelectObject sofont(&memDC, &font);

    // Measure and draw text
    const CSize textSize = memDC.GetTextExtent(&ch, 1);
    const int x = (RENDER_SIZE - textSize.cx) / 2;
    const int y = (RENDER_SIZE - textSize.cy) / 2;
    memDC.TextOut(x, y, &ch, 1);

    // Apply color with alpha
    const BYTE r = GetRValue(textColor);
    const BYTE g = GetGValue(textColor);
    const BYTE b = GetBValue(textColor);

    const int PIXEL_COUNT = RENDER_SIZE * RENDER_SIZE;
    for (const int i : std::views::iota(0, PIXEL_COUNT))
    {
        const BYTE alpha = pBits[i * 4 + 2]; // Red channel has intensity
        pBits[i * 4] = b;
        pBits[i * 4 + 1] = g;
        pBits[i * 4 + 2] = r;
        pBits[i * 4 + 3] = alpha;
    }

    // Find glyph bounds
    int minX = RENDER_SIZE, maxX = 0, minY = RENDER_SIZE, maxY = 0;

    for (const int py : std::views::iota(0, RENDER_SIZE))
        for (const int px : std::views::iota(0, RENDER_SIZE))
            if (pBits[(py * RENDER_SIZE + px) * 4 + 3] > 0)
            {
                minX = min(minX, px);
                maxX = max(maxX, px);
                minY = min(minY, py);
                maxY = max(maxY, py);
            }

    // Calculate scaling
    const int glyphWidth = maxX - minX + 1;
    const int glyphHeight = maxY - minY + 1;

    const float scale = min(
        static_cast<float>(ICON_SIZE - PADDING * 2) / glyphWidth,
        static_cast<float>(ICON_SIZE - PADDING * 2) / glyphHeight
    );

    const int scaledWidth = static_cast<int>(glyphWidth * scale);
    const int scaledHeight = static_cast<int>(glyphHeight * scale);

    // Create final bitmap
    const auto biFinal = createBitmapHeader(ICON_SIZE);
    BYTE* pFinalBits = nullptr;
    CBitmap finalBmp;
    finalBmp.Attach(CreateDIBSection(screenDC.m_hDC,
        reinterpret_cast<BITMAPINFO*>(const_cast<BITMAPV5HEADER*>(&biFinal)),
        DIB_RGB_COLORS, reinterpret_cast<void**>(&pFinalBits), nullptr, 0));

    CDC finalDC;
    finalDC.CreateCompatibleDC(&screenDC);
    CSelectObject sofinal(&finalDC, &finalBmp);

    finalDC.SetStretchBltMode(HALFTONE);
    finalDC.SetBrushOrg(0, 0);

    // Center and blend the glyph
    const int destX = (ICON_SIZE - scaledWidth) / 2;
    const int destY = (ICON_SIZE - scaledHeight) / 2;

    constexpr BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    finalDC.AlphaBlend(destX, destY, scaledWidth, scaledHeight,
        &memDC, minX, minY, glyphWidth, glyphHeight, blend);

    // Create icon
    CBitmap maskBmp;
    maskBmp.CreateBitmap(ICON_SIZE, ICON_SIZE, 1, 1, nullptr);

    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmColor = static_cast<HBITMAP>(finalBmp.m_hObject);
    ii.hbmMask = static_cast<HBITMAP>(maskBmp.m_hObject);
    const HICON hIcon = CreateIconIndirect(&ii);

    return hIcon;
}
