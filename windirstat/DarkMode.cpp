// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")

using PreferredAppMode = enum : DWORD
{
    Default = 0,
    AllowDark = 1,
    ForceDark = 2,
    ForceLight = 3
};

static DWORD(WINAPI* SetPreferredAppMode)(PreferredAppMode word) = reinterpret_cast<decltype(SetPreferredAppMode)>(
    reinterpret_cast<LPVOID>(GetProcAddress(LoadLibrary(L"uxtheme.dll"), MAKEINTRESOURCEA(135))));

static DWORD(WINAPI* AllowDarkModeForWindow)(HWND hwnd, bool allow) = reinterpret_cast<decltype(AllowDarkModeForWindow)>(
    reinterpret_cast<LPVOID>(GetProcAddress(LoadLibrary(L"uxtheme.dll"), MAKEINTRESOURCEA(133))));

static LONG(WINAPI* RtlGetVersion)(LPOSVERSIONINFOEXW) = reinterpret_cast<decltype(RtlGetVersion)>(
    reinterpret_cast<LPVOID>(GetProcAddress(GetModuleHandle(L"ntdll.dll"), "RtlGetVersion")));

bool DarkMode::s_darkModeSupported = false;
bool DarkMode::s_darkModeEnabled = false;
bool DarkMode::s_highContrastEnabled = false;

bool DarkMode::SetAppDarkMode() noexcept
{
    // Determine if dark mode should be set based on settings
    bool darkMode = COptions::DarkMode == DM_ENABLED;

    if (COptions::DarkMode == DM_USE_WINDOWS)
    {
        // Check Windows dark mode setting
        if (CRegKey key; key.Open(HKEY_CURRENT_USER, wds::strThemesKey, KEY_READ) == ERROR_SUCCESS)
        {
            DWORD lightTheme = 1;
            key.QueryDWORDValue(L"AppsUseLightTheme", lightTheme);
            darkMode = lightTheme == 0;
        }
    }

    HIGHCONTRASTW contrast{ .cbSize = sizeof(contrast) };
    const bool highContrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0)
        && (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;

    // Validate this version of Windows supports dark mode
    OSVERSIONINFOEXW version{ .dwOSVersionInfoSize = sizeof(version) };
    const bool supported = RtlGetVersion != nullptr && RtlGetVersion(&version) == 0
        && version.dwMajorVersion >= 10 && version.dwBuildNumber >= 17763
        && SetPreferredAppMode != nullptr && AllowDarkModeForWindow != nullptr;
    s_darkModeSupported = supported;
    darkMode = darkMode && supported && !highContrast;
    const bool changed = s_darkModeEnabled != darkMode || s_highContrastEnabled != highContrast;
    s_darkModeEnabled = darkMode;
    s_highContrastEnabled = highContrast;

    // Signal this app can support dark mode
    if (supported)
    {
        if (version.dwBuildNumber < 18362)
        {
            using AllowDarkModeForApp = bool(WINAPI*)(bool);
            reinterpret_cast<AllowDarkModeForApp>(reinterpret_cast<LPVOID>(SetPreferredAppMode))(darkMode);
        }
        else SetPreferredAppMode(highContrast ? Default : darkMode ? ForceDark : ForceLight);
    }
    return changed;
}

COLORREF DarkMode::Color(const ColorRole role)
{
    if (!s_darkModeEnabled) return GetSysColor(std::to_underlying(role));
    switch (role)
    {
    case ColorRole::Edge: return RGB(70, 70, 70);
    case ColorRole::Light: return RGB(60, 60, 60);
    case ColorRole::Shadow: return RGB(20, 20, 20);
    case ColorRole::Desktop: return RGB(25, 25, 25);
    case ColorRole::Control: return RGB(45, 45, 45);
    case ColorRole::ControlText:
    case ColorRole::MenuText:
    case ColorRole::WindowText: return RGB(220, 220, 220);
    case ColorRole::DisabledText: return RGB(120, 120, 120);
    case ColorRole::Selection: return RGB(0, 120, 215);
    case ColorRole::SelectionText: return RGB(255, 255, 255);
    case ColorRole::Menu: return RGB(44, 44, 44);
    case ColorRole::MenuBar: return RGB(30, 30, 30);
    case ColorRole::Window: return RGB(32, 32, 32);
    case ColorRole::Frame: return RGB(50, 50, 50);
    }
    return GetSysColor(std::to_underlying(role));
}

COLORREF DarkMode::SystemColor(const DWORD index)
{
    return Color(static_cast<ColorRole>(index));
}

bool DarkMode::EnhancedDarkModeSupport()
{
    CRegKey key;
    if (key.Open(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        KEY_READ) != ERROR_SUCCESS) return false;

    // Fetch Windows build and revision information
    DWORD ubr = 0;
    std::array<WCHAR, 16> buildString;
    DWORD buildLen = static_cast<DWORD>(buildString.size());
    if (key.QueryStringValue(L"CurrentBuild", buildString.data(), &buildLen) != ERROR_SUCCESS ||
        key.QueryDWORDValue(L"UBR", ubr) != ERROR_SUCCESS) return false;

    // Support for dark mode checkboxes and other controls added in KB5072033
    const DWORD buildVal = std::wcstoul(buildString.data(), nullptr, 10);
    return buildVal > 26200 || ((buildVal == 26100 || buildVal == 26200) && ubr >= 7462);
}

void DarkMode::AdjustControls(const HWND hWnd)
{
    if (!IsWindow(hWnd)) return;

    auto ProcessWindow = [](const HWND hWnd, const LPARAM) -> BOOL
    {
        std::array<WCHAR, MAX_CLASS_NAME> classNameBuffer{};
        const int length = GetClassName(hWnd, classNameBuffer.data(), static_cast<int>(classNameBuffer.size()));
        const std::wstring_view className(classNameBuffer.data(), length);

        // Control whether the window is allowed for dark mode
        if (s_darkModeSupported) AllowDarkModeForWindow(hWnd, s_darkModeEnabled);

        // Set toplevel theme
        if ((GetWindowLongPtrW(hWnd, GWL_STYLE) & WS_CHILD) == 0)
        {
            const BOOL dark = s_darkModeEnabled;
            if (DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark)) == E_INVALIDARG)
            {
                // Fallback for older operating systems
                DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE - 1, &dark, sizeof(dark));
            }
        }

        if (!s_darkModeEnabled) SetWindowTheme(hWnd, nullptr, nullptr);
        else if (className == WC_BUTTON)
        {
            static const bool enhancedButtons = EnhancedDarkModeSupport();
            if (const auto style = GetWindowLong(hWnd, GWL_STYLE) & BS_TYPEMASK;
                style == BS_PUSHBUTTON || style == BS_DEFPUSHBUTTON ||
                style == BS_SPLITBUTTON || style == BS_DEFSPLITBUTTON ||
                (enhancedButtons && (style == BS_CHECKBOX || style == BS_AUTOCHECKBOX)))
            {
                SetWindowTheme(hWnd, L"DarkMode_Explorer", nullptr);
            }
            else
            {
                // Disable theme for other buttons to force classic rendering
                SetWindowTheme(hWnd, L"", L"");
            }
        }
        else if (className == WC_HEADER) SetWindowTheme(hWnd, L"DarkMode_ItemsView", nullptr);
        else if (className == WC_COMBOBOX) SetWindowTheme(hWnd, L"DarkMode_CFD", nullptr);
        else SetWindowTheme(hWnd, L"DarkMode_Explorer", nullptr);

        if (className == WC_LISTVIEW)
        {
            SendMessageW(hWnd, LVM_SETBKCOLOR, 0, Color(ColorRole::Window));
            SendMessageW(hWnd, LVM_SETTEXTBKCOLOR, 0, Color(ColorRole::Window));
            SendMessageW(hWnd, LVM_SETTEXTCOLOR, 0, Color(ColorRole::WindowText));
        }
        else if (className == MSFTEDIT_CLASS)
        {
            CHARFORMAT2W format{};
            format.cbSize = sizeof(format);
            format.dwMask = CFM_COLOR;
            format.crTextColor = Color(ColorRole::WindowText);
            SendMessageW(hWnd, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
            SendMessageW(hWnd, EM_SETBKGNDCOLOR, 0, Color(ColorRole::Window));
        }
        return TRUE;
    };

    ProcessWindow(hWnd, 0);
    EnumChildWindows(hWnd, ProcessWindow, 0);

    SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
}

HBRUSH DarkMode::OnCtlColor(CDC* pDC, const UINT nCtlColor)
{
    if (s_darkModeEnabled &&
        (nCtlColor == CTLCOLOR_DLG || nCtlColor == CTLCOLOR_STATIC ||
         nCtlColor == CTLCOLOR_EDIT || nCtlColor == CTLCOLOR_LISTBOX))
    {
        pDC->SetTextColor(Color(ColorRole::WindowText));
        pDC->SetBkColor(Color(ColorRole::Window));
        pDC->SetBkMode(nCtlColor == CTLCOLOR_STATIC ? TRANSPARENT : OPAQUE);
        return GetDialogBackgroundBrush();
    }

    return nullptr;
}

HBRUSH DarkMode::GetDialogBackgroundBrush()
{
    if (!s_darkModeEnabled) return GetSysColorBrush(COLOR_WINDOW);
    static const CBrush darkBrush(Color(ColorRole::Window));
    return darkBrush;
}

void DarkMode::DrawMenuClientArea(CWnd& wnd)
{
    if (!s_darkModeEnabled) return;

    MENUBARINFO mbi{};
    mbi.cbSize = sizeof(MENUBARINFO);
    if (!GetMenuBarInfo(wnd, OBJID_MENU, 0, &mbi))
    {
        return;
    }

    CRect rcClient = wnd.ToScreen(wnd.GetClientRect());

    const CRect rcWindow(wnd.Handle());
    rcClient.Offset(-rcWindow.left, -rcWindow.top);

    // the rcBar is offset by the window rect
    CRect lineToPaint = rcClient;
    lineToPaint.bottom = lineToPaint.top;
    lineToPaint.top--;

    CWindowDC dc(&wnd);
    dc.FillSolidRect(lineToPaint, SystemColor(COLOR_MENUBAR));
}

LRESULT DarkMode::HandleMenuMessage(const UINT message, const WPARAM wParam, const LPARAM lParam, const HWND hWnd)
{
    if (!s_darkModeEnabled || lParam == 0) return DefWindowProc(hWnd, message, wParam, lParam);

    using UAHMENU = struct UAHMENU
    {
        HMENU hmenu;
        HDC hdc;
        DWORD dwFlags;
    };

    using UAHDRAWMENUITEM = struct UAHDRAWMENUITEM
    {
        DRAWITEMSTRUCT dis;
        UAHMENU um;
        int iPosition; // Abbreviated structure
    };

    if (message == WM_UAHDRAWMENU)
    {
        const UAHMENU* pUDM = std::bit_cast<UAHMENU*>(lParam);
        MENUBARINFO mbi{};
        mbi.cbSize = sizeof(MENUBARINFO);
        GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi);

        const CRect rcWindow(hWnd);

        // the rcBar is offset by the window rect
        OffsetRect(&mbi.rcBar, -rcWindow.left, -rcWindow.top);
        mbi.rcBar.top -= 1;

        auto dc = CDC::Borrow(pUDM->hdc);
        dc.FillSolidRect(mbi.rcBar, SystemColor(COLOR_MENUBAR));
    }
    else if (message == WM_UAHDRAWMENUITEM)
    {
        UAHDRAWMENUITEM* pUDMI = std::bit_cast<UAHDRAWMENUITEM*>(lParam);

        std::array<WCHAR, 256> menuString = { L'\0' };
        MENUITEMINFO mii{ .cbSize = sizeof(MENUITEMINFO), .fMask = MIIM_STRING | MIIM_FTYPE,
            .dwTypeData = menuString.data(), .cch = static_cast<UINT>(menuString.size() - 1) };
        GetMenuItemInfoW(pUDMI->um.hmenu, pUDMI->iPosition, true, &mii);
        if ((mii.fType & MFT_OWNERDRAW) != 0) return DefWindowProc(hWnd, message, wParam, lParam);

        // Use structured bindings and lambda for state determination
        auto [txtId, bgId] = [&]() -> std::pair<int, int>
        {
            if (const auto itemState = pUDMI->dis.itemState; itemState & ODS_SELECTED)
                return { MBI_PUSHED, MBI_PUSHED };
            else if (itemState & ODS_HOTLIGHT)
                return { (itemState & ODS_INACTIVE) ? MBI_DISABLEDHOT : MBI_HOT, MBI_HOT };
            else if (itemState & (ODS_GRAYED | ODS_DISABLED | ODS_INACTIVE))
                return { MBI_DISABLED, MBI_DISABLED };

            return { MBI_NORMAL, MBI_NORMAL };
        }();

        DWORD dwFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
        if (pUDMI->dis.itemState & ODS_NOACCEL)
        {
            dwFlags |= DT_HIDEPREFIX;
        }

        const COLORREF bgColor =
            (bgId == MBI_PUSHED || bgId == MBI_DISABLEDPUSHED) ? SystemColor(COLOR_MENU) :
            (bgId == MBI_HOT || bgId == MBI_DISABLEDHOT) ? SystemColor(COLOR_MENU) : SystemColor(COLOR_MENUBAR);

        auto dc = CDC::Borrow(pUDMI->um.hdc);
        dc.FillSolidRect(pUDMI->dis.rcItem, bgColor);

        const COLORREF textColor =
            (txtId == MBI_DISABLED || txtId == MBI_DISABLEDHOT || txtId == MBI_DISABLEDPUSHED) ?
            SystemColor(COLOR_GRAYTEXT) : SystemColor(COLOR_BTNTEXT);

        DTTOPTS dttopts{ .dwSize = sizeof(DTTOPTS) };
        dttopts.dwFlags = DTT_TEXTCOLOR;
        dttopts.crText = textColor;

        const SmartPointer menuTheme(CloseThemeData, OpenThemeData(hWnd, VSCLASS_MENU));
        DrawThemeTextEx(menuTheme, pUDMI->um.hdc, MENU_BARITEM, txtId,
            menuString.data(), mii.cch, dwFlags, &pUDMI->dis.rcItem, &dttopts);
    }

    return 1;
}

void DarkMode::LightenBitmap(CBitmap& bitmap, const bool invert)
{
    if (!s_darkModeEnabled) return;
    const auto bitmapInfo = bitmap.Info();
    if (!bitmapInfo) return;
    const BITMAP& bm = *bitmapInfo;
    if (bm.bmWidth <= 0 || bm.bmHeight <= 0) return;

    const auto width = static_cast<std::size_t>(bm.bmWidth);
    const auto height = static_cast<std::size_t>(bm.bmHeight);
    if (width > (std::numeric_limits<std::size_t>::max)() / height / 4) return;

    const std::size_t byteCount = width * height * 4;
    CDC memDC(nullptr);
    GdiObjectSelection sobmp(&memDC, &bitmap);
    BITMAPINFO bmi = { {sizeof(BITMAPINFOHEADER), bm.bmWidth, -bm.bmHeight, 1, 32, BI_RGB} };
    const auto pixels = std::make_unique_for_overwrite<BYTE[]>(byteCount);
    if (!GetDIBits(memDC, bitmap, 0, bm.bmHeight, pixels.get(), &bmi, DIB_RGB_COLORS)) return;

    if (invert)
    {
        // Invert all color channels (BGR)
        for (std::size_t i = 0; i < byteCount; i += 4)
            std::ranges::transform(pixels.get() + i, pixels.get() + i + 3,
                pixels.get() + i, [](const BYTE b) { return static_cast<BYTE>(255 - b); });
    }
    else
    {
        // Gamma lookup table
        std::array<BYTE, 256> lut;
        for (const auto [i, val] : std::views::enumerate(lut))
        {
            val = static_cast<BYTE>(std::pow(i / 255.0f, 0.5f) * 255.0f);
        }

        // Apply to all color channels (BGR)
        for (std::size_t i = 0; i < byteCount; i += 4)
            std::ranges::transform(pixels.get() + i, pixels.get() + i + 3,
                pixels.get() + i, [&lut](const BYTE b) { return lut[b]; });
    }

    SetDIBits(memDC, bitmap, 0, bm.bmHeight, pixels.get(), &bmi, DIB_RGB_COLORS);
}

void DarkMode::DrawFocusRect(CDC* pdc, const CRect& rc)
{
    if (!s_darkModeEnabled)
    {
        pdc->DrawFocusRect(rc);
        return;
    }

    // In dark mode, draw a dotted rectangle with a visible light color
    // Standard DrawFocusRect uses XOR which is nearly invisible on dark backgrounds
    const CPen pen(PS_DOT, 1, RGB(120, 120, 120));
    GdiObjectSelection sopen(pdc, &pen);
    StockObjectSelection sobrush(pdc, NULL_BRUSH);
    ScopedBkMode sbm(pdc, TRANSPARENT);
    pdc->Rectangle(rc);
}
