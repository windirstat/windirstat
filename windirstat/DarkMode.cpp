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
#include "SelectObject.h"

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
    reinterpret_cast<LPVOID>(GetProcAddress(LoadLibraryW(L"uxtheme.dll"), MAKEINTRESOURCEA(135))));

static DWORD(WINAPI* AllowDarkModeForWindow)(HWND hwnd, bool allow) = reinterpret_cast<decltype(AllowDarkModeForWindow)>(
    reinterpret_cast<LPVOID>(GetProcAddress(LoadLibraryW(L"uxtheme.dll"), MAKEINTRESOURCEA(133))));

static LONG(WINAPI* RtlGetVersion)(LPOSVERSIONINFOEXW) = reinterpret_cast<decltype(RtlGetVersion)>(
    reinterpret_cast<LPVOID>(GetProcAddress(GetModuleHandle(L"ntdll.dll"), "RtlGetVersion")));

bool DarkMode::s_darkModeEnabled = false;
static std::array<COLORREF, 50> OriginalColors;
static std::array<COLORREF, 50> DarkModeColors;

void DarkMode::SetAppDarkMode() noexcept
{
    // Determine if dark mode should be set based on settings
    s_darkModeEnabled = COptions::DarkMode == 1;

    if (COptions::DarkMode == 2)
    {
        // Check Windows dark mode setting
        if (CRegKey key; key.Open(HKEY_CURRENT_USER, wds::strThemesKey, KEY_READ) == ERROR_SUCCESS)
        {
            DWORD darkSetting = 0;
            key.QueryDWORDValue(L"AppsUseLightTheme", darkSetting);
            s_darkModeEnabled = (darkSetting == 0);
        }
    }

    // Validate this version of Windows supports dark mode
    OSVERSIONINFOEXW verInfo { .dwOSVersionInfoSize  = sizeof(verInfo) };
    RtlGetVersion(&verInfo);
    s_darkModeEnabled &= verInfo.dwMajorVersion >= 10 && verInfo.dwBuildNumber >= 17763;

    // Disable if functions are not accessible
    s_darkModeEnabled &= SetPreferredAppMode != nullptr;
    s_darkModeEnabled &= AllowDarkModeForWindow != nullptr;

    // Signal this app can support dark mode
    if (s_darkModeEnabled) SetPreferredAppMode(ForceDark);

    // Record initial system colors
    for (const auto i : std::views::iota(0u, OriginalColors.size()))
    {
        OriginalColors[i] = GetSysColor(i);
    }

    // Setup dark mode colors
    DarkModeColors = OriginalColors;
    DarkModeColors[CTLCOLOR_DLG] = RGB(40, 40, 40);
    DarkModeColors[CTLCOLOR_STATIC] = RGB(40, 40, 40);
    DarkModeColors[CTLCOLOR_EDIT] = RGB(32, 32, 32);
    DarkModeColors[CTLCOLOR_LISTBOX] = RGB(32, 32, 32);
    DarkModeColors[COLOR_3DHIGHLIGHT] = RGB(70, 70, 70);
    DarkModeColors[COLOR_3DLIGHT] = RGB(60, 60, 60);
    DarkModeColors[COLOR_3DSHADOW] = RGB(20, 20, 20);
    DarkModeColors[COLOR_BACKGROUND] = RGB(25, 25, 25);
    DarkModeColors[COLOR_BTNFACE] = RGB(45, 45, 45);
    DarkModeColors[COLOR_BTNTEXT] = RGB(220, 220, 220);
    DarkModeColors[COLOR_GRAYTEXT] = RGB(120, 120, 120);
    DarkModeColors[COLOR_HIGHLIGHT] = RGB(0, 120, 215);
    DarkModeColors[COLOR_HIGHLIGHTTEXT] = RGB(255, 255, 255);
    DarkModeColors[COLOR_MENU] = RGB(35, 35, 35);
    DarkModeColors[COLOR_MENUBAR] = RGB(30, 30, 30);
    DarkModeColors[COLOR_WINDOW] = RGB(32, 32, 32);
    DarkModeColors[COLOR_WINDOWFRAME] = RGB(50, 50, 50);
    DarkModeColors[COLOR_WINDOWTEXT] = RGB(220, 220, 220);

    // Update colors
    SetupGlobalColors();
}

void DarkMode::SetupGlobalColors() noexcept
{
    // No need to continue if dark mode is not enabled
    if (!s_darkModeEnabled) return;

    // Update global colors
    GetGlobalData()->clrBarFace = WdsSysColor(COLOR_MENUBAR);
    GetGlobalData()->clrBarShadow = WdsSysColor(COLOR_MENUBAR);
    GetGlobalData()->clrBtnText = WdsSysColor(COLOR_BTNTEXT);
    GetGlobalData()->clrBtnFace = WdsSysColor(COLOR_BTNFACE);
    GetGlobalData()->clrBtnHilite = WdsSysColor(COLOR_BTNHILIGHT);
    GetGlobalData()->clrBtnShadow = WdsSysColor(COLOR_BTNSHADOW);
    GetGlobalData()->brBarFace.DeleteObject();
    GetGlobalData()->brBarFace.CreateSolidBrush(GetGlobalData()->clrBarFace);
    GetGlobalData()->brBtnFace.DeleteObject();
    GetGlobalData()->brBtnFace.CreateSolidBrush(GetGlobalData()->clrBtnFace);
}

COLORREF DarkMode::WdsSysColor(const DWORD index)
{
    return s_darkModeEnabled ? DarkModeColors[index] : OriginalColors[index];
}

bool DarkMode::IsDarkModeActive() noexcept
{
    return s_darkModeEnabled;
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
    DWORD buildVal = std::wcstoul(buildString.data(), nullptr, 10);
    return buildVal > 26200 || ((buildVal == 26100 || buildVal == 26200) && ubr >= 7462);
}

void DarkMode::AdjustControls(const HWND hWnd)
{
    if (!s_darkModeEnabled) return;

    auto ProcessWindow = [](const HWND hWnd, const LPARAM) -> BOOL
    {
        std::array<WCHAR, MAX_CLASS_NAME> classNameBuffer;
        const int length = GetClassName(hWnd, classNameBuffer.data(), static_cast<int>(classNameBuffer.size()));
        const std::wstring className(classNameBuffer.data(), length);

        // Control whether the window is allowed for dark mode
        AllowDarkModeForWindow(hWnd, s_darkModeEnabled);

        // Set toplevel theme
        const BOOL dark = s_darkModeEnabled;
        DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

        if (className == WC_BUTTON)
        {
            if (const auto style = GetWindowLong(hWnd, GWL_STYLE) & BS_TYPEMASK;
                style == BS_PUSHBUTTON || style == BS_DEFPUSHBUTTON ||
                (EnhancedDarkModeSupport() && (style == BS_CHECKBOX || style == BS_AUTOCHECKBOX)))
            {
                SetWindowTheme(hWnd, L"DarkMode_Explorer", nullptr);
            }
            else
            {
                // Disable theme for other buttons to force classic rendering
                SetWindowTheme(hWnd, L"", L"");
            }
        }
        else if (className == WC_HEADER)
        {
            SetWindowTheme(hWnd, L"DarkMode_ItemsView", nullptr);
        }
        else if (className == WC_COMBOBOX)
        {
            SetWindowTheme(hWnd, L"DarkMode_CFD", nullptr);
        }
        else
        {
            SetWindowTheme(hWnd, L"DarkMode_Explorer", nullptr);
        }
        
        return TRUE;
    };

    ProcessWindow(hWnd, NULL);
    EnumChildWindows(hWnd, ProcessWindow, NULL);

    SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

HBRUSH DarkMode::OnCtlColor(CDC* pDC, UINT nCtlColor)
{
    if (s_darkModeEnabled &&
        (nCtlColor == CTLCOLOR_DLG || nCtlColor == CTLCOLOR_STATIC ||
         nCtlColor == CTLCOLOR_EDIT || nCtlColor == CTLCOLOR_LISTBOX))
    {
        pDC->SetTextColor(WdsSysColor(COLOR_WINDOWTEXT));
        pDC->SetBkColor(WdsSysColor(CTLCOLOR_DLG));
        pDC->SetBkMode(nCtlColor == CTLCOLOR_STATIC ? TRANSPARENT : OPAQUE);
        return GetDialogBackgroundBrush();
    }

    return nullptr;
}

HBRUSH DarkMode::GetDialogBackgroundBrush()
{
    static HBRUSH darkBrush = CreateSolidBrush(DarkModeColors[COLOR_WINDOW]);
    return s_darkModeEnabled ? darkBrush : GetSysColorBrush(COLOR_WINDOW);
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

    CRect rcClient;
    wnd.GetClientRect(&rcClient);
    wnd.ClientToScreen(&rcClient);

    CRect rcWindow;
    wnd.GetWindowRect(&rcWindow);
    rcClient.OffsetRect(-rcWindow.left, -rcWindow.top);

    // the rcBar is offset by the window rect
    CRect lineToPaint = rcClient;
    lineToPaint.bottom = lineToPaint.top;
    lineToPaint.top--;

    CWindowDC dc(&wnd);
    dc.FillSolidRect(&lineToPaint, WdsSysColor(COLOR_MENUBAR));
}

LRESULT DarkMode::HandleMenuMessage(const UINT message, const WPARAM wParam, const LPARAM lParam, const HWND hWnd)
{
    if (!s_darkModeEnabled || lParam == NULL) return DefWindowProc(hWnd, message, wParam, lParam);

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
        UAHMENU* pUDM = reinterpret_cast<UAHMENU*>(lParam);
        MENUBARINFO mbi{};
        mbi.cbSize = sizeof(MENUBARINFO);
        GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi);

        RECT rcWindow{};
        ::GetWindowRect(hWnd, &rcWindow);

        // the rcBar is offset by the window rect
        OffsetRect(&mbi.rcBar, -rcWindow.left, -rcWindow.top);
        mbi.rcBar.top -= 1;

        CDC::FromHandle(pUDM->hdc)->FillSolidRect(&mbi.rcBar, WdsSysColor(COLOR_MENUBAR));
    }
    else if (message == WM_UAHDRAWMENUITEM)
    {
        UAHDRAWMENUITEM* pUDMI = reinterpret_cast<UAHDRAWMENUITEM*>(lParam);

        std::array<WCHAR, 256> menuString = { L'\0' };
        MENUITEMINFO mii{ .cbSize = sizeof(MENUITEMINFO), .fMask = MIIM_STRING,
            .dwTypeData = menuString.data(), .cch = static_cast<UINT>(menuString.size() - 1) };
        GetMenuItemInfo(pUDMI->um.hmenu, pUDMI->iPosition, TRUE, &mii);

        // Use structured bindings and lambda for state determination
        auto [txtId, bgId] = [&]() -> std::pair<int, int>
        {
            const auto itemState = pUDMI->dis.itemState;

            if (itemState & ODS_SELECTED)
                return { MBI_PUSHED, MBI_PUSHED };
            if (itemState & ODS_HOTLIGHT)
                return { (itemState & ODS_INACTIVE) ? MBI_DISABLEDHOT : MBI_HOT, MBI_HOT };
            if (itemState & (ODS_GRAYED | ODS_DISABLED | ODS_INACTIVE))
                return { MBI_DISABLED, MBI_DISABLED };

            return { MBI_NORMAL, MBI_NORMAL };
        }();

        DWORD dwFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
        if (pUDMI->dis.itemState & ODS_NOACCEL)
        {
            dwFlags |= DT_HIDEPREFIX;
        }

        const COLORREF bgColor =
            (bgId == MBI_PUSHED || bgId == MBI_DISABLEDPUSHED) ? WdsSysColor(COLOR_MENU) :
            (bgId == MBI_HOT || bgId == MBI_DISABLEDHOT) ? WdsSysColor(COLOR_MENU) : WdsSysColor(COLOR_MENUBAR);

        CDC::FromHandle(pUDMI->um.hdc)->FillSolidRect(&pUDMI->dis.rcItem, bgColor);

        const COLORREF textColor =
            (txtId == MBI_DISABLED || txtId == MBI_DISABLEDHOT || txtId == MBI_DISABLEDPUSHED) ?
            WdsSysColor(COLOR_GRAYTEXT) : WdsSysColor(COLOR_BTNTEXT);

        DTTOPTS dttopts{ .dwSize = sizeof(DTTOPTS) };
        dttopts.dwFlags = DTT_TEXTCOLOR;
        dttopts.crText = textColor;

        SmartPointer<HTHEME> menuTheme(CloseThemeData, OpenThemeData(hWnd, VSCLASS_MENU));
        DrawThemeTextEx(menuTheme, pUDMI->um.hdc, MENU_BARITEM, txtId,
            menuString.data(), mii.cch, dwFlags, &pUDMI->dis.rcItem, &dttopts);
    }

    return 1;
}

void DarkMode::LightenBitmap(CBitmap* pBitmap, const bool invert)
{
    if (!s_darkModeEnabled) return;
    BITMAP bm;
    pBitmap->GetBitmap(&bm);
    CDC memDC;
    memDC.CreateCompatibleDC(nullptr);
    CSelectObject sobmp(&memDC, pBitmap);
    BITMAPINFO bmi = { {sizeof(BITMAPINFOHEADER), bm.bmWidth, -bm.bmHeight, 1, 32, BI_RGB} };
    const auto pixels = std::make_unique_for_overwrite<BYTE[]>(bm.bmWidth * bm.bmHeight * 4);
    if (!GetDIBits(memDC, *pBitmap, 0, bm.bmHeight, pixels.get(), &bmi, DIB_RGB_COLORS)) return;

    if (invert)
    {
        // Invert all color channels (BGR)
        for (int i = 0; i < bm.bmWidth * bm.bmHeight * 4; i += 4)
            std::ranges::transform(pixels.get() + i, pixels.get() + i + 3,
                pixels.get() + i, [](const BYTE b) { return static_cast<BYTE>(255 - b); });
    }
    else
    {
        // Gamma lookup table
        std::array<BYTE, 256> lut;
        std::ranges::transform(std::views::iota(0, static_cast<int>(lut.size())), lut.begin(),
            [](const int i) { return static_cast<BYTE>(std::pow(i / 255.0f, 0.5f) * 255.0f); });

        // Apply to all color channels (BGR)
        for (int i = 0; i < bm.bmWidth * bm.bmHeight * 4; i += 4)
            std::ranges::transform(pixels.get() + i, pixels.get() + i + 3,
                pixels.get() + i, [&lut](const BYTE b) { return lut[b]; });
    }

    SetDIBits(memDC, *pBitmap, 0, bm.bmHeight, pixels.get(), &bmi, DIB_RGB_COLORS);
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
    CPen pen(PS_DOT, 1, RGB(120, 120, 120));
    CSelectObject sopen(pdc, &pen);
    CSelectStockObject sobrush(pdc, NULL_BRUSH);
    CSetBkMode sbm(pdc, TRANSPARENT);
    pdc->Rectangle(rc);
}

// Implement runtime class information for CDarkModeVisualManager
IMPLEMENT_DYNCREATE(CDarkModeVisualManager, CMFCVisualManagerWindows)

void CDarkModeVisualManager::GetTabFrameColors(const CMFCBaseTabCtrl* pTabWnd,
    COLORREF& clrDark, COLORREF& clrBlack, COLORREF& clrHighlight, COLORREF& clrFace,
    COLORREF& clrDarkShadow, COLORREF& clrLight, CBrush*& pbrFace, CBrush*& pbrBlack)
{
    CMFCVisualManagerWindows::GetTabFrameColors(pTabWnd, clrDark, clrBlack,
        clrHighlight, clrFace, clrDarkShadow, clrLight, pbrFace, pbrBlack);

    clrBlack = DarkMode::WdsSysColor(COLOR_BTNHILIGHT); // Flat Style - Tab Border
    clrFace = DarkMode::WdsSysColor(COLOR_WINDOW); // Flat Style Tab Border Sides
    clrHighlight = DarkMode::WdsSysColor(COLOR_WINDOW); // Flat Style Tab Border Top

    static CBrush brFaceDark(clrFace); // Flat Style - Tab No Focus Background
    static CBrush brBlackDark(COLORREF{ 0 }); // Flat Style - Tab Control Border

    pbrFace = &brFaceDark;
    pbrBlack = &brBlackDark;
}

void CDarkModeVisualManager::OnFillBarBackground(CDC* pDC, CBasePane* pBar, CRect rectClient, CRect rectClip, BOOL bNCArea)
{
    UNREFERENCED_PARAMETER(pBar);
    UNREFERENCED_PARAMETER(bNCArea);
    UNREFERENCED_PARAMETER(rectClip);

    pDC->FillSolidRect(rectClient, DarkMode::WdsSysColor(COLOR_MENUBAR));
}

void CDarkModeVisualManager::OnDrawSeparator(CDC* pDC, CBasePane* pBar, CRect rect, BOOL bIsHoriz)
{
    UNREFERENCED_PARAMETER(pDC);
    UNREFERENCED_PARAMETER(pBar);
    UNREFERENCED_PARAMETER(rect);
    UNREFERENCED_PARAMETER(bIsHoriz);
}

void CDarkModeVisualManager::OnDrawStatusBarPaneBorder(CDC* pDC, CMFCStatusBar* pBar, CRect rectPane, UINT uiID, UINT nStyle)
{
    UNREFERENCED_PARAMETER(pBar);
    UNREFERENCED_PARAMETER(uiID);
    UNREFERENCED_PARAMETER(nStyle);

    pDC->FillSolidRect(rectPane.left, rectPane.top, rectPane.Width(), 1, DarkMode::WdsSysColor(COLOR_WINDOWFRAME));
    pDC->FillSolidRect(rectPane.right - 1, rectPane.top, 1, rectPane.Height(), DarkMode::WdsSysColor(COLOR_WINDOWFRAME));
}

void CDarkModeVisualManager::OnFillSplitterBackground(CDC* pDC, CSplitterWndEx* /*pSplitterWnd*/, CRect rect)
{
    pDC->FillSolidRect(rect, DarkMode::WdsSysColor(COLOR_WINDOWFRAME));
}

void CDarkModeVisualManager::OnUpdateSystemColors()
{
    CMFCVisualManagerWindows::OnUpdateSystemColors();
    DarkMode::SetupGlobalColors();
}
