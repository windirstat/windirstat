// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"

static int ReadWindowsTextScalePercent() noexcept
{
    DWORD percent = 100;
    if (CRegKey key; key.Open(HKEY_CURRENT_USER, wds::strAccessibilityKey, KEY_READ) == ERROR_SUCCESS)
        key.QueryDWORDValue(L"TextScaleFactor", percent);
    return static_cast<int>(std::clamp<DWORD>(percent, 100, 225));
}

int ResolveTextScalePercent(const int configuredPercent) noexcept
{
    return configuredPercent != 0 ? std::clamp(configuredPercent, 100, 200) :
        std::min(ReadWindowsTextScalePercent(), 200);
}

static std::map<std::pair<int, int>, HFONT> s_appFonts;

HFONT GetAppFont(const HWND window)
{
    const int dpi = GetWindowDpi(window);
    std::array<WCHAR, MAX_CLASS_NAME> className{};
    if (window != nullptr) GetClassNameW(window, className.data(), static_cast<int>(className.size()));
    const int percent = wcscmp(className.data(), TOOLBARCLASSNAMEW) == 0 ? GetToolBarSizePercent() : GetFontSizePercent();
    const std::pair key(dpi, percent);
    if (const auto found = s_appFonts.find(key); found != s_appFonts.end()) return found->second;

    NONCLIENTMETRICSW metrics{ .cbSize = sizeof(metrics) };
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
        GetObjectW(GetStockObject(DEFAULT_GUI_FONT), sizeof(LOGFONTW), &metrics.lfMessageFont);

    // System font sizes can retain a different text scale after RDP reconnects. Use a stable 9-point baseline.
    metrics.lfMessageFont.lfHeight = -MulDiv(9 * percent, dpi, 72 * 100);
    metrics.lfMessageFont.lfWidth = 0;
    // Keep fonts alive for controls that still use them, and reuse identical metrics after a settings broadcast.
    static std::map<std::array<BYTE, sizeof(LOGFONTW)>, CFont> fonts;
    const auto fontKey = std::bit_cast<std::array<BYTE, sizeof(LOGFONTW)>>(metrics.lfMessageFont);
    return s_appFonts.emplace(key, fonts.try_emplace(fontKey, metrics.lfMessageFont).first->second).first->second;
}

struct ScaledMenuItem final
{
    HWND owner;
    HMENU menu;
    UINT position;
    ULONG_PTR data;
    HBITMAP bitmap;
    bool menuBar;
    UINT type;
    UINT addedType;
};

static std::map<ULONG_PTR, std::unique_ptr<ScaledMenuItem>> s_scaledMenuItems;
static std::map<HMENU, HBRUSH> s_scaledMenuBackgrounds;

static ScaledMenuItem* FindScaledMenuItem(const ULONG_PTR data)
{
    const auto found = s_scaledMenuItems.find(data);
    return found != s_scaledMenuItems.end() ? found->second.get() : nullptr;
}

static void RestoreScaledMenu(const HMENU menu)
{
    if (const auto found = s_scaledMenuBackgrounds.find(menu); found != s_scaledMenuBackgrounds.end())
    {
        MENUINFO info{ .cbSize = sizeof(info), .fMask = MIM_BACKGROUND, .hbrBack = found->second };
        SetMenuInfo(menu, &info);
        s_scaledMenuBackgrounds.erase(found);
    }
    for (int position = 0; position < GetMenuItemCount(menu); ++position)
    {
        MENUITEMINFOW info{ .cbSize = sizeof(info), .fMask = MIIM_FTYPE | MIIM_DATA | MIIM_BITMAP };
        if (!GetMenuItemInfoW(menu, position, true, &info)) continue;
        const auto* item = FindScaledMenuItem(info.dwItemData);
        if (item == nullptr) continue;
        info.fType &= ~item->addedType;
        info.dwItemData = item->data;
        info.hbmpItem = item->bitmap;
        SetMenuItemInfoW(menu, position, true, &info);
    }
    std::erase_if(s_scaledMenuItems, [menu](const auto& entry) { return entry.second->menu == menu; });
}

static void PrepareScaledMenu(const HWND owner, const HMENU menu, const bool menuBar)
{
    RestoreScaledMenu(menu);
    if (!menuBar && owner == GetMainWindowHandle())
    {
        // Shell popups also contain separators and submenu headers without command IDs.
        for (int position = 0; position < GetMenuItemCount(menu); ++position)
        {
            MENUITEMINFOW info{ .cbSize = sizeof(info), .fMask = MIIM_ID };
            if (GetMenuItemInfoW(menu, position, true, &info) &&
                info.wID >= CONTENT_MENU_MINCMD && info.wID <= CONTENT_MENU_MAXCMD) return;
        }
    }
    bool converted = false;
    for (int position = 0; position < GetMenuItemCount(menu); ++position)
    {
        MENUITEMINFOW info{ .cbSize = sizeof(info), .fMask = MIIM_FTYPE | MIIM_DATA | MIIM_BITMAP };
        if (!GetMenuItemInfoW(menu, position, true, &info) || (info.fType & MFT_OWNERDRAW) != 0 ||
            (info.hbmpItem != nullptr && GetObjectType(info.hbmpItem) != OBJ_BITMAP)) continue;
        const UINT type = MFT_OWNERDRAW | (Localization::IsRightToLeft() ? MFT_RIGHTORDER : 0);
        auto item = std::make_unique<ScaledMenuItem>(owner, menu, position,
            info.dwItemData, info.hbmpItem, menuBar, type, type & ~info.fType);
        info.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_BITMAP;
        info.fType |= type;
        info.dwItemData = reinterpret_cast<ULONG_PTR>(item.get());
        info.hbmpItem = menuBar ? HBMMENU_CALLBACK : nullptr;
        s_scaledMenuItems.emplace(info.dwItemData, std::move(item));
        if (!SetMenuItemInfoW(menu, position, true, &info)) s_scaledMenuItems.erase(info.dwItemData);
        else converted = true;
    }
    MENUINFO info{ .cbSize = sizeof(info), .fMask = MIM_BACKGROUND };
    if (!converted || !GetMenuInfo(menu, &info)) return;
    s_scaledMenuBackgrounds.emplace(menu, info.hbrBack);
    static std::map<COLORREF, CBrush> brushes;
    const COLORREF background = DarkMode::SystemColor(menuBar ? COLOR_MENUBAR : COLOR_MENU);
    info.hbrBack = brushes.try_emplace(background, background).first->second;
    if (!SetMenuInfo(menu, &info)) s_scaledMenuBackgrounds.erase(menu);
}

static CRect RenderScaledMenuItem(const ScaledMenuItem& item, MEASUREITEMSTRUCT* measure, DRAWITEMSTRUCT* draw)
{
    MENUITEMINFOW info{ .cbSize = sizeof(info), .fMask = MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU };
    if (!GetMenuItemInfoW(item.menu, item.position, true, &info)) return {};
    const bool separator = (info.fType & MFT_SEPARATOR) != 0;
    const auto text = MenuRef(item.menu).GetItemText(item.position);
    const auto tab = text.find(L'\t');
    const std::wstring_view caption(text.data(), tab == std::wstring::npos ? text.size() : tab);
    const std::wstring_view shortcut = tab == std::wstring::npos ? L"" : std::wstring_view(text).substr(tab + 1);
    const CClientDC ownerDC(WindowRef(item.owner));
    const HDC dc = draw != nullptr ? draw->hDC : ownerDC.Handle();
    const ScopedDcState restore(dc);
    const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(item.owner, GWL_EXSTYLE));
    const bool rtl = (exStyle & WS_EX_LAYOUTRTL) != 0 || (info.fType & MFT_RIGHTORDER) != 0;
    const UINT captionFlags = DT_SINGLELINE | ((rtl || (exStyle & WS_EX_RTLREADING) != 0) ? DT_RTLREADING : 0);
    SetLayout(dc, 0);
    LOGFONTW font{};
    GetObjectW(GetAppFont(item.owner), sizeof(font), &font);
    if ((info.fState & MFS_DEFAULT) != 0) font.lfWeight = FW_BOLD;
    const CFont menuFont(font);
    const GdiObjectSelection selectFont(dc, &menuFont);
    CRect captionSize, shortcutSize;
    DrawTextW(dc, caption.data(), static_cast<int>(caption.size()), &captionSize, captionFlags | DT_CALCRECT);
    DrawTextW(dc, shortcut.data(), static_cast<int>(shortcut.size()), &shortcutSize,
        DT_SINGLELINE | DT_CALCRECT | DT_NOPREFIX);
    const int gutter = ScaleForDpi(16, item.owner) + ScaleForScreenDpi(4, item.owner);
    const int padding = ScaleForScreenDpi(2, item.owner);
    if (measure != nullptr)
    {
        measure->itemWidth = captionSize.Width() + (item.menuBar ? padding * 4 : gutter * 2) +
            (shortcut.empty() ? 0 : ScaleForScreenDpi(12, item.owner) + shortcutSize.Width());
        measure->itemHeight = (separator ? 0 : std::max(captionSize.Height(), shortcutSize.Height())) + padding * 2;
        return {};
    }

    const bool selected = (draw->itemState & (ODS_SELECTED | ODS_HOTLIGHT)) != 0;
    const bool disabled = (draw->itemState & (ODS_DISABLED | ODS_GRAYED | ODS_INACTIVE)) != 0;
    const COLORREF background = DarkMode::SystemColor(selected ? COLOR_3DLIGHT :
        item.menuBar ? COLOR_MENUBAR : COLOR_MENU);
    const COLORREF foreground = DarkMode::SystemColor(disabled ? COLOR_GRAYTEXT : COLOR_MENUTEXT);
    const CBrush brush(DarkMode::SystemColor(item.menuBar ? COLOR_MENUBAR : COLOR_MENU));
    FillRect(dc, &draw->rcItem, brush);
    CRect rect(draw->rcItem);
    if (separator)
    {
        const CBrush line(DarkMode::SystemColor(COLOR_3DLIGHT));
        rect.Deflate(ScaleForScreenDpi(8, item.owner), 0);
        rect.top += rect.Height() / 2;
        rect.bottom = rect.top + 1;
        FillRect(dc, &rect, line);
        return {};
    }
    if (selected)
    {
        CRect highlight(rect);
        highlight.Deflate(ScaleForScreenDpi(2, item.owner), ScaleForScreenDpi(1, item.owner));
        const CBrush highlightBrush(background);
        const GdiObjectSelection selectBrush(dc, &highlightBrush);
        const StockObjectSelection selectPen(dc, NULL_PEN);
        const int corner = ScaleForScreenDpi(8, item.owner);
        RoundRect(dc, highlight.left, highlight.top, highlight.right, highlight.bottom, corner, corner);
    }

    SetTextColor(dc, foreground);
    SetBkColor(dc, background);
    SetBkMode(dc, TRANSPARENT);
    const auto drawMark = [&](const UINT state, const bool right)
    {
        const int size = ScaleForDpi(13, item.owner);
        auto target = CDC::Borrow(dc);
        CDC memory(&target);
        CBitmap bitmap(size, size, 1, 1, nullptr);
        const GdiObjectSelection selectBitmap(&memory, &bitmap);
        CRect mark(0, 0, size, size);
        DrawFrameControl(memory, &mark, DFC_MENU, state);
        BitBlt(dc, right ? rect.right - gutter + (gutter - size) / 2 : rect.left + (gutter - size) / 2,
            rect.top + (rect.Height() - size) / 2, size, size, memory, 0, 0, SRCCOPY);
    };
    if (item.bitmap != nullptr)
    {
        BITMAP bitmap{};
        GetObjectW(item.bitmap, sizeof(bitmap), &bitmap);
        const int width = MulDiv(bitmap.bmWidth, GetFontSizePercent(), 100);
        const int height = MulDiv(bitmap.bmHeight, GetFontSizePercent(), 100);
        const int x = rtl ? rect.right - gutter + (gutter - width) / 2 : rect.left + (gutter - width) / 2;
        const int y = rect.top + (rect.Height() - height) / 2;
        auto target = CDC::Borrow(dc);
        CDC memory(&target);
        const GdiObjectSelection selectBitmap(&memory, item.bitmap);
        if (bitmap.bmBitsPixel == 32)
            AlphaBlend(dc, x, y, width, height, memory, 0, 0, bitmap.bmWidth, bitmap.bmHeight,
                BLENDFUNCTION{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA });
        else StretchBlt(dc, x, y, width, height, memory, 0, 0, bitmap.bmWidth, bitmap.bmHeight, SRCCOPY);
    }
    else if (!item.menuBar && (draw->itemState & ODS_CHECKED) != 0)
        drawMark((info.fType & MFT_RADIOCHECK) != 0 ? DFCS_MENUBULLET : DFCS_MENUCHECK, rtl);
    CRect arrow;
    if (!item.menuBar && info.hSubMenu != nullptr)
    {
        drawMark(rtl ? DFCS_MENUARROWRIGHT : DFCS_MENUARROW, !rtl);
        arrow = rect;
        if (rtl) arrow.right = arrow.left + gutter;
        else arrow.left = arrow.right - gutter;
    }
    rect.Deflate(item.menuBar ? padding * 2 : gutter, 0);
    const UINT flags = captionFlags | DT_VCENTER | ((draw->itemState & ODS_NOACCEL) != 0 ? DT_HIDEPREFIX : 0);
    DrawTextW(dc, caption.data(), static_cast<int>(caption.size()), &rect,
        flags | (item.menuBar ? DT_CENTER : rtl ? DT_RIGHT : DT_LEFT));
    if (!shortcut.empty()) DrawTextW(dc, shortcut.data(), static_cast<int>(shortcut.size()),
        &rect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | (rtl ? DT_LEFT : DT_RIGHT));
    return arrow;
}

static void RecolorScaledMenuScrollArrows(const HWND window, const HDC dc)
{
    MENUBARINFO menu{ .cbSize = sizeof(menu) };
    WINDOWINFO info{ .cbSize = sizeof(info) };
    if (dc == nullptr || !DarkMode::IsDarkModeActive() ||
        !GetMenuBarInfo(window, OBJID_CLIENT, 0, &menu) || !s_scaledMenuBackgrounds.contains(menu.hMenu) ||
        !GetWindowInfo(window, &info)) return;

    const int left = info.rcClient.left - info.rcWindow.left;
    const int right = info.rcClient.right - info.rcWindow.left;
    const int border = static_cast<int>(info.cyWindowBorders);
    const std::array bands
    {
        CRect(left, border, right, info.rcClient.top - info.rcWindow.top),
        CRect(left, info.rcClient.bottom - info.rcWindow.top, right,
            info.rcWindow.bottom - info.rcWindow.top - border)
    };
    const ScopedDcState restore(dc);
    SetLayout(dc, 0);
    auto target = CDC::Borrow(dc);
    CDC memory(&target);
    if (!memory) return;
    const COLORREF nativeText = GetSysColor(COLOR_MENUTEXT);
    const COLORREF appText = DarkMode::SystemColor(COLOR_MENUTEXT);
    const DWORD source = RGB(GetBValue(nativeText), GetGValue(nativeText), GetRValue(nativeText));
    const DWORD replacement = RGB(GetBValue(appText), GetGValue(appText), GetRValue(appText));
    for (const auto& band : bands)
    {
        if (band.IsEmpty()) continue;
        BITMAPINFO bitmapInfo{ .bmiHeader = { sizeof(BITMAPINFOHEADER), band.Width(), -band.Height(), 1, 32, BI_RGB } };
        DWORD* pixels = nullptr;
        CBitmap bitmap(CreateDIBSection(dc, &bitmapInfo, DIB_RGB_COLORS,
            reinterpret_cast<void**>(&pixels), nullptr, 0));
        if (!bitmap || pixels == nullptr) continue;
        const GdiObjectSelection selectBitmap(&memory, &bitmap);
        if (!BitBlt(memory, 0, 0, band.Width(), band.Height(),
            dc, band.left, band.top, SRCCOPY) || !GdiFlush()) continue;
        // Recolor only native text pixels, preserving arrow geometry and disabled glyphs.
        for (int pixel = 0; pixel < band.Width() * band.Height(); ++pixel)
            if ((pixels[pixel] & 0xFFFFFF) == source) pixels[pixel] = (pixels[pixel] & 0xFF000000) | replacement;
        BitBlt(dc, band.left, band.top, band.Width(), band.Height(), memory, 0, 0, SRCCOPY);
    }
}

static LRESULT CALLBACK ScaledMenuPopupSubclass(const HWND window, const UINT message,
    const WPARAM wParam, const LPARAM lParam, const UINT_PTR id, DWORD_PTR)
{
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, ScaledMenuPopupSubclass, id);
    const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
    if (message == WM_PRINT && (lParam & PRF_NONCLIENT) != 0)
        RecolorScaledMenuScrollArrows(window, reinterpret_cast<HDC>(wParam));
    else if (message == WM_NCPAINT || message == WM_PAINT || message == WM_TIMER || message == WM_NCMOUSEMOVE)
    {
        const CWindowDC dc{ WindowRef(window) };
        RecolorScaledMenuScrollArrows(window, dc);
    }
    return result;
}

static LRESULT CALLBACK ScaledMenuSubclass(const HWND window, const UINT message,
    const WPARAM wParam, const LPARAM lParam, const UINT_PTR id, DWORD_PTR)
{
    if (message == WM_MEASUREITEM || message == WM_DRAWITEM)
    {
        auto* measure = message == WM_MEASUREITEM ? reinterpret_cast<MEASUREITEMSTRUCT*>(lParam) : nullptr;
        auto* draw = message == WM_DRAWITEM ? reinterpret_cast<DRAWITEMSTRUCT*>(lParam) : nullptr;
        if (lParam != 0 && (measure != nullptr ? measure->CtlType : draw->CtlType) == ODT_MENU)
            if (const auto* item = FindScaledMenuItem(measure != nullptr ? measure->itemData : draw->itemData))
            {
                const CRect arrow = RenderScaledMenuItem(*item, measure, draw);
                if (!arrow.IsEmpty())
                {
                    // User32 draws its own unscaled arrow after WM_DRAWITEM, so exclude only that gutter.
                    const DWORD layout = SetLayout(draw->hDC, 0);
                    ExcludeClipRect(draw->hDC, arrow.left, arrow.top, arrow.right, arrow.bottom);
                    SetLayout(draw->hDC, layout);
                }
                return true;
            }
    }
    if (message == WM_MENUCHAR)
    {
        const auto menu = reinterpret_cast<HMENU>(lParam);
        const WCHAR key = LOWORD(wParam);
        std::vector<UINT> matches;
        int selected = -1;
        for (int position = 0; position < GetMenuItemCount(menu); ++position)
        {
            MENUITEMINFOW info{ .cbSize = sizeof(info), .fMask = MIIM_DATA | MIIM_STATE };
            if (!GetMenuItemInfoW(menu, position, true, &info)) continue;
            if ((info.fState & MFS_HILITE) != 0) selected = position;
            if (FindScaledMenuItem(info.dwItemData) == nullptr || (info.fState & MFS_DISABLED) != 0) continue;
            const auto text = MenuRef(menu).GetItemText(position);
            for (size_t i = 0; i + 1 < text.size(); ++i)
                if (text[i] == L'&' && text[++i] != L'&' &&
                    CompareStringOrdinal(text.data() + i, 1, &key, 1, true) == CSTR_EQUAL)
                {
                    matches.push_back(position);
                    break;
                }
        }
        if (!matches.empty())
        {
            const auto next = std::ranges::find_if(matches, [selected](const UINT position) {
                return static_cast<int>(position) > selected;
            });
            return MAKELRESULT(next != matches.end() ? *next : matches.front(),
                matches.size() == 1 ? MNC_EXECUTE : MNC_SELECT);
        }
    }
    if (message == WM_NCDESTROY)
    {
        std::vector<HMENU> menus;
        for (const auto& [_, item] : s_scaledMenuItems)
            if (item->owner == window) menus.push_back(item->menu);
        for (const auto menu : menus) RestoreScaledMenu(menu);
        RemoveWindowSubclass(window, ScaledMenuSubclass, id);
    }
    const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
    if (message == WM_INITMENUPOPUP && HIWORD(lParam) == 0)
        PrepareScaledMenu(window, reinterpret_cast<HMENU>(wParam), false);
    if (message == WM_UNINITMENUPOPUP) RestoreScaledMenu(reinterpret_cast<HMENU>(wParam));
    if (message == WM_ENTERIDLE && wParam == MSGF_MENU)
    {
        const HWND popup = reinterpret_cast<HWND>(lParam);
        MENUBARINFO menu{ .cbSize = sizeof(menu) };
        if (GetMenuBarInfo(popup, OBJID_CLIENT, 0, &menu) && s_scaledMenuBackgrounds.contains(menu.hMenu))
        {
            const DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUNDSMALL;
            DwmSetWindowAttribute(popup, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
            const COLORREF border = DarkMode::SystemColor(COLOR_3DLIGHT);
            DwmSetWindowAttribute(popup, DWMWA_BORDER_COLOR, &border, sizeof(border));
            SetWindowSubclass(popup, ScaledMenuPopupSubclass, 1, 0);
            const CWindowDC dc{ WindowRef(popup) };
            RecolorScaledMenuScrollArrows(popup, dc);
        }
    }
    return result;
}

void ApplyMenuFont(const HWND window)
{
    const HMENU menu = GetMenu(window);
    if (menu == nullptr) return;
    SetWindowSubclass(window, ScaledMenuSubclass, 1, 0);
    PrepareScaledMenu(window, menu, true);
    DrawMenuBar(window);
    SetWindowPos(window, nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static void DrawScaledButton(const HWND window, const HDC dc)
{
    const ScopedDcState restore(dc);
    SetLayout(dc, 0);
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
    const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE));
    const bool radio = (style & BS_TYPEMASK) == BS_RADIOBUTTON || (style & BS_TYPEMASK) == BS_AUTORADIOBUTTON;
    const bool enabled = IsWindowEnabled(window);
    const UINT state = static_cast<UINT>(SendMessageW(window, BM_GETSTATE, 0, 0));
    const int check = static_cast<int>(SendMessageW(window, BM_GETCHECK, 0, 0));
    const int part = radio ? BP_RADIOBUTTON : BP_CHECKBOX;
    const int themeState = (check == BST_CHECKED ? 4 : check == BST_INDETERMINATE ? 8 : 0) +
        (!enabled ? 4 : (state & BST_PUSHED) != 0 ? 3 : (state & BST_HOT) != 0 ? 2 : 1);
    const SmartPointer theme(CloseThemeData, OpenThemeData(window, VSCLASS_BUTTON));
    SIZE nativeSize{ ScaleForScreenDpi(13, window), ScaleForScreenDpi(13, window) };
    if (theme.IsValid()) GetThemePartSize(theme, dc, part, themeState, nullptr, TS_TRUE, &nativeSize);
    const int glyphWidth = MulDiv(nativeSize.cx, GetFontSizePercent(), 100);
    const int glyphHeight = MulDiv(nativeSize.cy, GetFontSizePercent(), 100);
    CRect client;
    GetClientRect(window, &client);
    const bool rtl = (exStyle & WS_EX_LAYOUTRTL) != 0;
    const bool rightGlyph = ((style & BS_LEFTTEXT) != 0) != rtl;
    CRect glyph(rightGlyph ? client.right - glyphWidth : client.left, client.top, 0, 0);
    glyph.top += (style & BS_VCENTER) == BS_TOP ? 0 : (style & BS_VCENTER) == BS_BOTTOM
        ? client.Height() - glyphHeight : (client.Height() - glyphHeight) / 2;
    glyph.right = glyph.left + glyphWidth;
    glyph.bottom = glyph.top + glyphHeight;

    HBRUSH background = reinterpret_cast<HBRUSH>(SendMessageW(GetParent(window), WM_CTLCOLORSTATIC,
        reinterpret_cast<WPARAM>(dc), reinterpret_cast<LPARAM>(window)));
    if (background == nullptr) background = GetSysColorBrush(COLOR_BTNFACE);
    FillRect(dc, &client, background);
    auto target = CDC::Borrow(dc);
    CDC memory(&target);
    CBitmap bitmap(&target, glyphWidth, glyphHeight);
    if (memory && bitmap)
    {
        const GdiObjectSelection selectBitmap(&memory, &bitmap);
        CRect nativeRect(0, 0, glyphWidth, glyphHeight);
        FillRect(memory, &nativeRect, background);
        // Use geometry to avoid stretching theme bitmaps; keep native high-contrast rendering.
        HIGHCONTRASTW contrast{ .cbSize = sizeof(contrast) };
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
        if ((contrast.dwFlags & HCF_HIGHCONTRASTON) == 0)
        {
            const bool pressed = enabled && (state & BST_PUSHED) != 0;
            const bool hot = enabled && (state & BST_HOT) != 0;
            const auto color = [](const int index, const BYTE alpha = 255)
            {
                const COLORREF value = DarkMode::SystemColor(index);
                return Gdiplus::Color(alpha, GetRValue(value), GetGValue(value), GetBValue(value));
            };
            Gdiplus::Graphics graphics(memory);
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            const float stroke = std::max(1.0f, glyphWidth / 15.0f), inset = stroke / 2.0f;
            const float width = glyphWidth - stroke, height = glyphHeight - stroke;
            Gdiplus::SolidBrush fill(color(COLOR_HIGHLIGHT, pressed ? 208 : hot ? 232 : 255));
            Gdiplus::SolidBrush mark(color(enabled ? COLOR_HIGHLIGHTTEXT : COLOR_GRAYTEXT));
            Gdiplus::Pen border(color(!enabled ? COLOR_GRAYTEXT :
                check != BST_UNCHECKED || hot || pressed ? COLOR_HIGHLIGHT : COLOR_WINDOWTEXT), stroke);
            Gdiplus::Pen tick(color(enabled ? COLOR_HIGHLIGHTTEXT : COLOR_GRAYTEXT),
                std::max(1.5f, glyphWidth / 11.0f));
            tick.SetStartCap(Gdiplus::LineCapRound);
            tick.SetEndCap(Gdiplus::LineCapRound);
            tick.SetLineJoin(Gdiplus::LineJoinRound);
            if (radio)
            {
                if (enabled && (check != BST_UNCHECKED || pressed))
                    graphics.FillEllipse(&fill, inset, inset, width, height);
                graphics.DrawEllipse(&border, inset, inset, width, height);
                if (check != BST_UNCHECKED) graphics.FillEllipse(&mark,
                    glyphWidth * .3f, glyphHeight * .3f, glyphWidth * .4f, glyphHeight * .4f);
            }
            else
            {
                const float diameter = glyphWidth / 4.0f;
                Gdiplus::GraphicsPath outline;
                outline.AddArc(inset, inset, diameter, diameter, 180, 90);
                outline.AddArc(inset + width - diameter, inset, diameter, diameter, 270, 90);
                outline.AddArc(inset + width - diameter, inset + height - diameter, diameter, diameter, 0, 90);
                outline.AddArc(inset, inset + height - diameter, diameter, diameter, 90, 90);
                outline.CloseFigure();
                if (enabled && (check != BST_UNCHECKED || pressed)) graphics.FillPath(&fill, &outline);
                graphics.DrawPath(&border, &outline);
                if (check == BST_INDETERMINATE) graphics.FillRectangle(&mark,
                    glyphWidth * .25f, glyphHeight * .45f, glyphWidth * .5f, glyphHeight * .1f);
                else if (check == BST_CHECKED)
                {
                    const Gdiplus::PointF points[] = { {glyphWidth * .23f, glyphHeight * .52f},
                        {glyphWidth * .43f, glyphHeight * .72f}, {glyphWidth * .78f, glyphHeight * .28f} };
                    graphics.DrawLines(&tick, points, _countof(points));
                }
            }
        }
        else if (!theme.IsValid() ||
            FAILED(DrawThemeBackground(theme, memory, part, themeState, &nativeRect, nullptr)))
        {
            const UINT flags = (radio ? DFCS_BUTTONRADIO :
                check == BST_INDETERMINATE ? DFCS_BUTTON3STATE : DFCS_BUTTONCHECK) |
                (check != BST_UNCHECKED ? DFCS_CHECKED : 0) | (!enabled ? DFCS_INACTIVE : 0) |
                ((state & BST_PUSHED) != 0 ? DFCS_PUSHED : 0);
            DrawFrameControl(memory, &nativeRect, DFC_BUTTON, flags);
        }
        BitBlt(dc, glyph.left, glyph.top, glyphWidth, glyphHeight, memory, 0, 0, SRCCOPY);
    }

    const GdiObjectSelection selectFont(dc, reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0)));
    SetBkMode(dc, TRANSPARENT);
    if (!enabled) SetTextColor(dc, DarkMode::SystemColor(COLOR_GRAYTEXT));
    std::wstring text(static_cast<size_t>(GetWindowTextLengthW(window)) + 1, L'\0');
    text.resize(GetWindowTextW(window, text.data(), static_cast<int>(text.size())));
    CRect textRect = client;
    if (rightGlyph) textRect.right = glyph.left - ScaleForDpi(3, window);
    else textRect.left = glyph.right + ScaleForDpi(3, window);
    const UINT uiState = static_cast<UINT>(SendMessageW(window, WM_QUERYUISTATE, 0, 0));
    const UINT alignment = (style & BS_CENTER) == BS_CENTER ? DT_CENTER :
        ((style & BS_CENTER) == BS_RIGHT || rtl) ? DT_RIGHT : DT_LEFT;
    const UINT flags = alignment | ((style & BS_MULTILINE) != 0 ? DT_WORDBREAK : DT_SINGLELINE) |
        ((uiState & UISF_HIDEACCEL) != 0 ? DT_HIDEPREFIX : 0) |
        ((exStyle & (WS_EX_RTLREADING | WS_EX_LAYOUTRTL)) != 0 ? DT_RTLREADING : 0);
    CRect measured = textRect;
    DrawTextW(dc, text.data(), static_cast<int>(text.size()), &measured, flags | DT_CALCRECT);
    textRect.top += (style & BS_VCENTER) == BS_TOP ? 0 : (style & BS_VCENTER) == BS_BOTTOM
        ? client.Height() - measured.Height() : (client.Height() - measured.Height()) / 2;
    textRect.bottom = textRect.top + measured.Height();
    DrawTextW(dc, text.data(), static_cast<int>(text.size()), &textRect, flags);
    if ((state & BST_FOCUS) != 0 && (uiState & UISF_HIDEFOCUS) == 0)
    {
        const int width = std::min(measured.Width(), textRect.Width());
        if (alignment == DT_RIGHT) textRect.left = textRect.right - width;
        else if (alignment == DT_CENTER) textRect.left += (textRect.Width() - width) / 2;
        textRect.right = textRect.left + width;
        textRect.Inflate(1, 1);
        DrawFocusRect(dc, &textRect);
    }
}

static LRESULT CALLBACK ScaledButtonSubclass(const HWND window, const UINT message,
    const WPARAM wParam, const LPARAM lParam, const UINT_PTR id, DWORD_PTR)
{
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, ScaledButtonSubclass, id);
    if (GetFontSizePercent() == 100 || (GetWindowLongPtrW(window, GWL_STYLE) & BS_PUSHLIKE) != 0)
        return DefSubclassProc(window, message, wParam, lParam);

    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT)
    {
        CPaintDC paintDC{ WindowRef(window) };
        const ScopedDcState restore(paintDC);
        SetLayout(paintDC, 0);
        CBufferedDC dc(paintDC, WindowRef(window));
        DrawScaledButton(window, dc);
        return 0;
    }
    if (message == WM_PRINTCLIENT)
    {
        DrawScaledButton(window, reinterpret_cast<HDC>(wParam));
        return 0;
    }
    const bool mouse = message == WM_MOUSEMOVE || message == WM_MOUSELEAVE;
    const LRESULT state = mouse ? SendMessageW(window, BM_GETSTATE, 0, 0) : 0;
    const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
    if (message == BM_SETCHECK || message == BM_SETSTATE || message == WM_ENABLE ||
        message == WM_SETFOCUS || message == WM_KILLFOCUS || message == WM_UPDATEUISTATE ||
        (mouse && state != SendMessageW(window, BM_GETSTATE, 0, 0)))
        InvalidateRect(window, nullptr, false);
    return result;
}

static BOOL CALLBACK SetAppFontCallback(const HWND window, LPARAM) noexcept
{
    try
    {
        SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(GetAppFont(window)), false);
        std::array<WCHAR, MAX_CLASS_NAME> className{};
        GetClassNameW(window, className.data(), static_cast<int>(className.size()));
        constexpr std::array buttonTypes{ BS_CHECKBOX, BS_AUTOCHECKBOX, BS_RADIOBUTTON,
            BS_AUTORADIOBUTTON, BS_3STATE, BS_AUTO3STATE };
        if (wcscmp(className.data(), WC_BUTTONW) == 0 && std::ranges::contains(buttonTypes,
            GetWindowLongPtrW(window, GWL_STYLE) & BS_TYPEMASK))
            SetWindowSubclass(window, ScaledButtonSubclass, 1, 0);
    }
    catch (...) {}
    return true;
}

static BOOL CALLBACK RefreshToolTipFontCallback(const HWND window, LPARAM) noexcept
{
    std::array<WCHAR, MAX_CLASS_NAME> className{};
    GetClassNameW(window, className.data(), static_cast<int>(className.size()));
    if (wcscmp(className.data(), TOOLTIPS_CLASSW) != 0) return true;

    SendMessageW(window, TTM_POP, 0, 0);
    SetAppFontCallback(window, 0);
    return true;
}

static BOOL CALLBACK NotifyFontSizeChangedCallback(const HWND window, const LPARAM oldPercent) noexcept
{
    try
    {
        if (CWnd* attached = CWnd::FindAttached(window))
            attached->OnFontSizeChanged(static_cast<int>(oldPercent), GetFontSizePercent());
    }
    catch (...) {}
    return true;
}

void ApplyAppFont(const HWND window, const int oldPercent)
{
    if (!IsWindow(window)) return;
    if (oldPercent != 0) s_appFonts.clear();
    ApplyMenuFont(window);
    SetAppFontCallback(window, 0);
    EnumChildWindows(window, SetAppFontCallback, 0);
    if (oldPercent == 0) return;
    EnumThreadWindows(GetWindowThreadProcessId(window, nullptr), RefreshToolTipFontCallback, 0);
    NotifyFontSizeChangedCallback(window, oldPercent);
    EnumChildWindows(window, NotifyFontSizeChangedCallback, oldPercent);
}

void InitializeDialogFontAndSize(const HWND dialog)
{
    if (!IsWindow(dialog)) return;

    const int percent = GetFontSizePercent();
    if (percent != 100)
    {
        std::vector<std::pair<HWND, CRect>> controls;
        for (HWND control = GetWindow(dialog, GW_CHILD); control != nullptr;
            control = GetWindow(control, GW_HWNDNEXT))
        {
            CRect rect(control);
            MapWindowPoints(nullptr, dialog, reinterpret_cast<LPPOINT>(&rect), 2);
            controls.emplace_back(control, rect);
            std::array<WCHAR, MAX_CLASS_NAME> className{};
            GetClassNameW(control, className.data(), static_cast<int>(className.size()));
            if (wcscmp(className.data(), TRACKBAR_CLASSW) == 0)
            {
                const int length = static_cast<int>(SendMessageW(control, TBM_GETTHUMBLENGTH, 0, 0));
                SetWindowLongPtrW(control, GWL_STYLE, GetWindowLongPtrW(control, GWL_STYLE) | TBS_FIXEDLENGTH);
                SendMessageW(control, TBM_SETTHUMBLENGTH, MulDiv(length, percent, 100), 0);
            }
        }

        const CRect windowRect(dialog);
        CRect clientRect;
        GetClientRect(dialog, &clientRect);
        const bool childDialog = (GetWindowLongPtrW(dialog, GWL_STYLE) & WS_CHILD) != 0;
        const int width = childDialog ? MulDiv(windowRect.Width(), percent, 100) :
            windowRect.Width() + MulDiv(clientRect.Width(), percent, 100) - clientRect.Width();
        const int height = childDialog ? MulDiv(windowRect.Height(), percent, 100) :
            windowRect.Height() + MulDiv(clientRect.Height(), percent, 100) - clientRect.Height();
        SetWindowPos(dialog, nullptr, 0, 0, width, height,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

        HDWP positions = BeginDeferWindowPos(static_cast<int>(controls.size()));
        for (const auto& [control, rect] : controls)
        {
            positions = DeferWindowPos(positions, control, nullptr,
                MulDiv(rect.left, percent, 100), MulDiv(rect.top, percent, 100),
                MulDiv(rect.Width(), percent, 100), MulDiv(rect.Height(), percent, 100),
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        if (positions != nullptr) EndDeferWindowPos(positions);
    }

    ApplyAppFont(dialog);
}

void CButton::SetTextOffset(const CPoint& offset)
{
    m_textOffset = offset;
    if (m_hWnd != nullptr) Invalidate();
}

std::span<const RouteEntry> CButton::Routes()
{
    static constexpr std::array entries
    {
        Route::ReflectNotify<&OnCustomDraw>(NM_CUSTOMDRAW),
    };
    return entries;
}

bool CButton::OnCustomDraw(UINT, NMHDR* header, LRESULT* result)
{
    const auto* customDraw = reinterpret_cast<NMCUSTOMDRAW*>(header);
    if (customDraw->dwDrawStage == CDDS_POSTPAINT)
    {
        if (m_textDrawDc == nullptr || m_textDrawDc != customDraw->hdc) return false;
        SetViewportOrgEx(std::exchange(m_textDrawDc, nullptr),
            m_textDrawOrigin.x, m_textDrawOrigin.y, nullptr);
        *result = CDRF_DODEFAULT;
        return true;
    }

    // Shift the caption after the background is drawn, then restore before the native focus rectangle is drawn.
    if (customDraw->dwDrawStage != CDDS_PREPAINT || m_textOffset == CPoint() || m_textDrawDc != nullptr ||
        !OffsetViewportOrgEx(customDraw->hdc, ScaleForDpi(m_textOffset.x), ScaleForDpi(m_textOffset.y), &m_textDrawOrigin))
        return false;

    m_textDrawDc = customDraw->hdc;
    *result = CDRF_NOTIFYPOSTPAINT;
    return true;
}

std::span<const RouteEntry> CSliderCtrl::Routes()
{
    static constexpr std::array entries
    {
        Route::ReflectNotify<&OnCustomDraw>(NM_CUSTOMDRAW),
    };
    return entries;
}

bool CSliderCtrl::OnCustomDraw(UINT, NMHDR* header, LRESULT* result)
{
    if (!DarkMode::IsDarkModeActive() || !IsWindowEnabled()) return false;

    const auto* draw = reinterpret_cast<NMCUSTOMDRAW*>(header);
    if (draw->dwDrawStage == CDDS_PREPAINT)
    {
        *result = CDRF_NOTIFYITEMDRAW;
        return true;
    }
    if (draw->dwDrawStage != CDDS_ITEMPREPAINT || draw->dwItemSpec != TBCD_THUMB ||
        (draw->uItemState & (CDIS_SELECTED | CDIS_DISABLED)) != 0) return false;

    // Native trackbars do not report CDIS_HOT for the thumb.
    const CRect rect(draw->rc);
    const auto cursor = GetClientCursorPos();
    if (!cursor || !rect.Contains(*cursor)) return false;

    const CBrush brush(DarkMode::Color(DarkMode::ColorRole::ControlText));
    const GdiObjectSelection selectBrush(draw->hdc, &brush);
    const StockObjectSelection selectPen(draw->hdc, NULL_PEN);
    const int corner = ScaleForDpi(4);
    RoundRect(draw->hdc, rect.left, rect.top, rect.right, rect.bottom, corner, corner);
    *result = CDRF_SKIPDEFAULT;
    return true;
}

void CDC::DrawTreeExpander(const CRect& nodeRect, const bool expanded)
{
    Gdiplus::Graphics graphics(m_hDC);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    const BYTE shade = DarkMode::IsDarkModeActive() ? 180 : 80;
    Gdiplus::Pen pen(Gdiplus::Color(255, shade, shade, shade), std::max(1.0f, nodeRect.Height() / 15.0f));
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);

    const float centerX = nodeRect.left + nodeRect.Width() / 2.0f;
    const float centerY = nodeRect.top + nodeRect.Height() / 2.0f;
    const float size = std::max(2.0f, nodeRect.Height() / 5.0f);
    if (expanded)
    {
        const Gdiplus::PointF points[] = { {centerX - size, centerY - size / 2.0f},
            {centerX, centerY + size / 2.0f}, {centerX + size, centerY - size / 2.0f} };
        graphics.DrawLines(&pen, points, _countof(points));
    }
    else
    {
        const Gdiplus::PointF points[] = { {centerX - size / 2.0f, centerY - size},
            {centerX + size / 2.0f, centerY}, {centerX - size / 2.0f, centerY + size} };
        graphics.DrawLines(&pen, points, _countof(points));
    }
}

// -----------------------------------------------------------------------------
//  CMenu
// -----------------------------------------------------------------------------
CMenu& CMenu::operator=(CMenu&& other) noexcept
{
    if (this != &other)
    {
        if (m_hMenu != nullptr) DestroyMenu(m_hMenu);
        m_hMenu = other.Detach();
    }
    return *this;
}

CMenu CMenu::CreatePopup() noexcept
{
    return CMenu(CreatePopupMenu());
}

CMenu CMenu::LoadResource(const UINT id) noexcept
{
    return CMenu(LoadMenuW(GetAppInstance(), MAKEINTRESOURCEW(id)));
}

int MenuRef::GetItemCount() const noexcept
{
    return GetMenuItemCount(m_hMenu);
}

UINT MenuRef::GetItemId(const int pos) const noexcept
{
    return GetMenuItemID(m_hMenu, pos);
}

UINT MenuRef::GetItemState(const UINT id, const UINT flags) const noexcept
{
    return GetMenuState(m_hMenu, id, flags);
}

std::wstring MenuRef::GetItemText(const UINT pos) const
{
    MENUITEMINFOW info{ .cbSize = sizeof(info), .fMask = MIIM_STRING };
    if (!GetMenuItemInfoW(m_hMenu, pos, true, &info)) return {};
    std::wstring buffer(info.cch + 1, L'\0');
    info.dwTypeData = buffer.data();
    ++info.cch;
    if (!GetMenuItemInfoW(m_hMenu, pos, true, &info)) return {};
    buffer.resize(info.cch);
    return buffer;
}

MenuRef MenuRef::GetSubMenu(const int pos) const
{
    return MenuRef(::GetSubMenu(m_hMenu, pos));
}

bool MenuRef::Append(const UINT flags, const UINT_PTR id, const LPCWSTR psz) const noexcept
{
    return AppendMenuW(m_hMenu, flags, id, psz);
}

bool MenuRef::Modify(const UINT pos, const UINT flags, const UINT_PTR id, const LPCWSTR psz) const noexcept
{
    MENUITEMINFOW info{ .cbSize = sizeof(info), .fMask = MIIM_DATA };
    GetMenuItemInfoW(m_hMenu, pos, (flags & MF_BYPOSITION) != 0, &info);
    const auto* item = FindScaledMenuItem(info.dwItemData);
    if (!ModifyMenuW(m_hMenu, pos, flags, id, psz)) return false;
    if (item == nullptr) return true;
    info.fMask = MIIM_FTYPE | MIIM_BITMAP;
    GetMenuItemInfoW(item->menu, item->position, true, &info);
    info.fMask |= MIIM_DATA;
    info.fType |= item->type;
    info.dwItemData = reinterpret_cast<ULONG_PTR>(item);
    info.hbmpItem = item->menuBar ? HBMMENU_CALLBACK : nullptr;
    return SetMenuItemInfoW(item->menu, item->position, true, &info);
}

bool MenuRef::Remove(const UINT pos, const UINT flags) const noexcept
{
    return DeleteMenu(m_hMenu, pos, flags);
}

UINT MenuRef::EnableItem(const UINT id, const UINT flags) const noexcept
{
    return EnableMenuItem(m_hMenu, id, flags);
}

bool MenuRef::SetDefaultItem(const UINT item) const noexcept
{
    return SetMenuDefaultItem(m_hMenu, item, false);
}

bool MenuRef::GetItemInfo(const UINT item, MENUITEMINFOW* info, const ItemLookup lookup) const
{
    if (!GetMenuItemInfoW(m_hMenu, item, lookup == ItemLookup::Position, info)) return false;
    if ((info->fMask & MIIM_DATA) != 0)
        if (const auto* scaled = FindScaledMenuItem(info->dwItemData)) info->dwItemData = scaled->data;
    return true;
}

bool MenuRef::SetItemInfo(const UINT item, const MENUITEMINFOW* info, const ItemLookup lookup) const
{
    MENUITEMINFOW previous{ .cbSize = sizeof(previous), .fMask = MIIM_DATA };
    GetMenuItemInfoW(m_hMenu, item, lookup == ItemLookup::Position, &previous);
    auto update = *info;
    if (auto* scaled = FindScaledMenuItem(previous.dwItemData))
    {
        if ((update.fMask & MIIM_DATA) != 0)
        {
            scaled->data = update.dwItemData;
            update.dwItemData = previous.dwItemData;
        }
        if ((update.fMask & MIIM_BITMAP) != 0)
        {
            scaled->bitmap = update.hbmpItem;
            update.hbmpItem = scaled->menuBar ? HBMMENU_CALLBACK : nullptr;
        }
        if ((update.fMask & MIIM_FTYPE) != 0) update.fType |= scaled->type;
    }
    return SetMenuItemInfoW(m_hMenu, item, lookup == ItemLookup::Position, &update);
}

UINT MenuRef::ShowPopup(const UINT flags, const int x, const int y, WindowRef window,
    const LPTPMPARAMS lptpm) const
{
    const HWND owner = window.Handle();
    if (owner != nullptr) SetWindowSubclass(owner, ScaledMenuSubclass, 1, 0);
    return TrackPopupMenuEx(m_hMenu, flags | (Localization::IsRightToLeft() ? TPM_LAYOUTRTL : 0),
        x, y, owner, lptpm);
}

void MenuRef::SetItemEnabled(const int item, const bool enable, const ItemLookup lookup) const
{
    if (item < 0) return;
    const UINT flags = lookup == ItemLookup::Command ? MF_BYCOMMAND : MF_BYPOSITION;
    EnableItem(static_cast<UINT>(item), flags | (enable ? MF_ENABLED : MF_DISABLED | MF_GRAYED));
}

bool MenuRef::IsItemEnabled(const UINT item, const ItemLookup lookup) const noexcept
{
    const UINT flags = lookup == ItemLookup::Command ? MF_BYCOMMAND : MF_BYPOSITION;
    return (GetItemState(item, flags) & (MF_DISABLED | MF_GRAYED)) == 0;
}

UINT MenuRef::CheckItem(const UINT id, const UINT flags) const noexcept
{
    return CheckMenuItem(m_hMenu, id, flags);
}

MenuRef WindowRef::GetMenu() const
{
    return MenuRef(::GetMenu(m_hWnd));
}

UINT MenuRef::ShowPopup(const UINT flags, const POINT pt, WindowRef window, LPTPMPARAMS lptpm) const
{
    return ShowPopup(flags, pt.x, pt.y, window, lptpm);
}

void NotifyAppearanceChanged(const HWND window)
{
    if (CWnd* attached = CWnd::FindAttached(window)) attached->OnAppearanceChanged();
    EnumChildWindows(window, [](const HWND child, LPARAM) -> BOOL
        {
            if (CWnd* attached = CWnd::FindAttached(child)) attached->OnAppearanceChanged();
            return true;
        }, 0);
}
