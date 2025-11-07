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

#include "stdafx.h"
#include "DarkMode.h"
#include "Options.h"
#include "Constants.h"

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <ranges>

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
    reinterpret_cast<LPVOID>(GetProcAddress(LoadLibraryW(L"ntdll.dll"), "RtlGetVersion")));


bool DarkMode::_darkModeEnabled = false;
static std::array<COLORREF, 50> OriginalColors;
static std::array<COLORREF, 50> DarkModeColors;

void DarkMode::SetAppDarkMode() noexcept
{
    // Determine if dark mode should based on settings
    _darkModeEnabled = COptions::DarkMode == 1;

    if (COptions::DarkMode == 2)
    {
        // Check Windows dark mode setting
        DWORD darkSetting;
        CRegKey key;
        key.Open(HKEY_CURRENT_USER, wds::strThemesKey, KEY_READ);
        key.QueryDWORDValue(L"AppsUseLightTheme", darkSetting);
        _darkModeEnabled = (darkSetting == 0);
    }

    // Validate this version of Windows supports dark mode
    OSVERSIONINFOEXW verInfo { .dwOSVersionInfoSize  = sizeof(verInfo) };
    RtlGetVersion(&verInfo);
    _darkModeEnabled &= verInfo.dwMajorVersion >= 10 && verInfo.dwBuildNumber >= 17763;

    // Disable if functions are not accessible
    _darkModeEnabled &= SetPreferredAppMode != nullptr;
    _darkModeEnabled &= AllowDarkModeForWindow != nullptr;

    // Signal this app can support dark mode
    if (_darkModeEnabled) SetPreferredAppMode(ForceDark);

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
}

void DarkMode::SetupGlobalColors() noexcept
{
    // No need to continue if dark mode is not enabled
    if (!_darkModeEnabled) return;

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
    return _darkModeEnabled ? DarkModeColors[index] : OriginalColors[index];
}

bool DarkMode::IsDarkModeActive() noexcept
{
    return _darkModeEnabled;
}

void DarkMode::AdjustControls(const HWND hWnd)
{
    if (!_darkModeEnabled) return;

    auto ProcessWindow = [](const HWND hWnd, const LPARAM) -> BOOL
    {
        std::array<WCHAR, MAX_CLASS_NAME> classNameBuffer;
        const int length = GetClassName(hWnd, classNameBuffer.data(), static_cast<int>(classNameBuffer.size()));
        const std::wstring className(classNameBuffer.data(), length);

        // Control whether is window is allowed for dark mode
        AllowDarkModeForWindow(hWnd, _darkModeEnabled);

        // Set toplevel theme
        const BOOL dark = _darkModeEnabled;
        DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

        if (className == WC_BUTTON)
        {
            const auto style = GetWindowLong(hWnd, GWL_STYLE) & BS_TYPEMASK;
            if (style == BS_PUSHBUTTON || style == BS_DEFPUSHBUTTON)
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
    if (_darkModeEnabled &&
        (nCtlColor == CTLCOLOR_DLG || nCtlColor == CTLCOLOR_STATIC ||
         nCtlColor == CTLCOLOR_EDIT || nCtlColor == CTLCOLOR_LISTBOX))
    {
        pDC->SetTextColor(WdsSysColor(COLOR_WINDOWTEXT));
        pDC->SetBkColor(WdsSysColor(CTLCOLOR_DLG));
        return GetDialogBackgroundBrush();
    }

    return nullptr;
}

HBRUSH DarkMode::GetDialogBackgroundBrush()
{
    static HBRUSH darkBrush = CreateSolidBrush(DarkModeColors[COLOR_WINDOW]);
    return _darkModeEnabled ? darkBrush : GetSysColorBrush(COLOR_WINDOW);
}

void DarkMode::DrawMenuBar(const HWND hWnd, const UAHMENU* pUDM)
{
    if (!_darkModeEnabled) return;

    MENUBARINFO mbi{};
    mbi.cbSize = sizeof(MENUBARINFO);
    GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi);

    RECT rcWindow{};
    GetWindowRect(hWnd, &rcWindow);

    // the rcBar is offset by the window rect
    OffsetRect(&mbi.rcBar, -rcWindow.left, -rcWindow.top);
    mbi.rcBar.top -= 1;

    const CBrush brush(WdsSysColor(COLOR_MENUBAR));
    FillRect(pUDM->hdc, &mbi.rcBar, brush);
}

void DarkMode::DrawMenuItem(const HWND hWnd, UAHDRAWMENUITEM* pUDMI)
{
    if (!_darkModeEnabled) return;

    std::array<WCHAR, 256> menuString = { L'\0' };
    MENUITEMINFO mii{ .cbSize = sizeof(MENUITEMINFO), .fMask = MIIM_STRING,
        .dwTypeData = menuString.data(), .cch = static_cast<UINT>(menuString.size() - 1)};
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

    const CBrush brush(bgColor);
    FillRect(pUDMI->um.hdc, &pUDMI->dis.rcItem, brush);

    const COLORREF textColor =
        (txtId == MBI_DISABLED || txtId == MBI_DISABLEDHOT || txtId == MBI_DISABLEDPUSHED) ?
        WdsSysColor(COLOR_GRAYTEXT) : WdsSysColor(COLOR_BTNTEXT);

    DTTOPTS dttopts{ .dwSize = sizeof(DTTOPTS) };
    dttopts.dwFlags = DTT_TEXTCOLOR;
    dttopts.crText = textColor;

    const HTHEME menuTheme = OpenThemeData(hWnd, VSCLASS_MENU);
    DrawThemeTextEx(menuTheme, pUDMI->um.hdc, MENU_BARITEM, txtId,
        menuString.data(), mii.cch, dwFlags, &pUDMI->dis.rcItem, &dttopts);
}

void DarkMode::DrawMenuClientArea(CWnd& wnd)
{
    if (!_darkModeEnabled) return;

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
    CBrush brush(WdsSysColor(COLOR_MENUBAR));
    dc.FillRect(&lineToPaint, &brush);
}

HICON DarkMode::LightenIcon(const HICON hIcon, const bool invert)
{
    if (!_darkModeEnabled) return hIcon;

    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo)) return hIcon;

    // Lighten the color bitmap
    if (CBitmap* pColorBitmap = CBitmap::FromHandle(iconInfo.hbmColor))
    {
        LightenBitmap(pColorBitmap, invert);
    }

    // Create new icon with lightened bitmap
    const HICON hNewIcon = CreateIconIndirect(&iconInfo);

    // Clean up
    if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
    if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);

    return hNewIcon ? hNewIcon : hIcon;
}
void DarkMode::LightenBitmap(CBitmap* pBitmap, const bool invert)
{
    if (!_darkModeEnabled) return;
    BITMAP bm;
    pBitmap->GetBitmap(&bm);
    CDC memDC;
    memDC.CreateCompatibleDC(nullptr);
    memDC.SelectObject(pBitmap);
    BITMAPINFO bmi = { {sizeof(BITMAPINFOHEADER), bm.bmWidth, -bm.bmHeight, 1, 32, BI_RGB} };
    const auto pixels = std::make_unique<BYTE[]>(bm.bmWidth * bm.bmHeight * 4);
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
        std::ranges::transform(std::views::iota(0, 256), lut.begin(),
            [](const int i) { return static_cast<BYTE>(std::pow(i / 255.0f, 0.5f) * 255.0f); });

        // Apply to all color channels (BGR)
        for (int i = 0; i < bm.bmWidth * bm.bmHeight * 4; i += 4)
            std::ranges::transform(pixels.get() + i, pixels.get() + i + 3,
                pixels.get() + i, [&lut](const BYTE b) { return lut[b]; });
    }

    SetDIBits(memDC, *pBitmap, 0, bm.bmHeight, pixels.get(), &bmi, DIB_RGB_COLORS);
}

BEGIN_MESSAGE_MAP(CDarkModeStatusBar, CMFCStatusBar)
    ON_WM_ERASEBKGND()
    ON_WM_PAINT()
END_MESSAGE_MAP()

BOOL CDarkModeStatusBar::OnEraseBkgnd(CDC* pDC)
{
    if (!DarkMode::IsDarkModeActive())
    {
        return CMFCStatusBar::OnEraseBkgnd(pDC);
    }

    CRect rect;
    GetClientRect(&rect);
    pDC->FillSolidRect(rect, DarkMode::WdsSysColor(COLOR_BACKGROUND));
    return TRUE;
}

void CDarkModeStatusBar::OnPaint()
{
    if (!DarkMode::IsDarkModeActive())
    {
        CMFCStatusBar::OnPaint();
        return;
    }

    CPaintDC dcPaint(this);
    CRect rectClient;
    GetClientRect(&rectClient);

    // Create memory DC with bitmap for double buffering
    CDC dcMem;
    dcMem.CreateCompatibleDC(&dcPaint);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dcPaint, rectClient.Width(), rectClient.Height());
    CBitmap* pOldBitmap = dcMem.SelectObject(&bmp);

    // Setup drawing context
    dcMem.FillSolidRect(rectClient, DarkMode::WdsSysColor(COLOR_BACKGROUND));
    dcMem.SetTextColor(DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
    dcMem.SetBkMode(TRANSPARENT);

    CFont* pOldFont = dcMem.SelectObject(GetFont());
    CPen penBorder(PS_SOLID, 1, DarkMode::WdsSysColor(COLOR_BACKGROUND));
    CPen* pOldPen = dcMem.SelectObject(&penBorder);

    // Draw top border
    dcMem.MoveTo(0, 0);
    dcMem.LineTo(rectClient.right, 0);

    // Draw each pane
    for (int i = 0; i < GetCount(); i++)
    {
        CRect rectPane;
        UINT nID, nStyle;
        int cxWidth;
        GetItemRect(i, &rectPane);
        GetPaneInfo(i, nID, nStyle, cxWidth);

        // Draw separator (skip first pane and stretch panes)
        if (i > 0 && !(nStyle & SBPS_STRETCH))
        {
            dcMem.MoveTo(rectPane.left - 1, rectPane.top + 2);
            dcMem.LineTo(rectPane.left - 1, rectPane.bottom - 2);
        }

        // Draw pane text
        CString strText;
        GetPaneText(i, strText);
        if (!strText.IsEmpty())
        {
            CRect rectText = rectPane;
            rectText.DeflateRect(4, 0);
            const UINT nFormat = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX |
                ((nStyle & SBPS_STRETCH) ? DT_LEFT : DT_CENTER);
            dcMem.DrawText(strText, rectText, nFormat);
        }
    }

    // Restore and blit to screen
    dcMem.SelectObject(pOldPen);
    dcMem.SelectObject(pOldFont);
    dcPaint.BitBlt(0, 0, rectClient.Width(), rectClient.Height(), &dcMem, 0, 0, SRCCOPY);
    dcMem.SelectObject(pOldBitmap);
}

// Implement runtime class information for CDarkModeVisualManager
IMPLEMENT_DYNCREATE(CDarkModeVisualManager, CMFCVisualManagerWindows7)

void CDarkModeVisualManager::GetTabFrameColors(const CMFCBaseTabCtrl* pTabWnd,
    COLORREF& clrDark, COLORREF& clrBlack, COLORREF& clrHighlight, COLORREF& clrFace,
    COLORREF& clrDarkShadow, COLORREF& clrLight, CBrush*& pbrFace, CBrush*& pbrBlack)
{
    CMFCVisualManagerWindows7::GetTabFrameColors(pTabWnd, clrDark, clrBlack,
        clrHighlight, clrFace, clrDarkShadow, clrLight, pbrFace, pbrBlack);

    clrBlack = DarkMode::WdsSysColor(COLOR_BTNHILIGHT); // Flat Style - Tab Border
    clrFace = DarkMode::WdsSysColor(COLOR_WINDOW); // Flat Style Tab Border Sides
    clrHighlight = DarkMode::WdsSysColor(COLOR_WINDOW); // Flat Style Tab Border Top

    static CBrush brFaceDark(clrFace); // Flat Style - Tab No Focus Background
    static CBrush brBlackDark(COLORREF{ 0 }); // Flat Style - Tab Control Border

    pbrFace = &brFaceDark;
    pbrBlack = &brBlackDark;
}

void CDarkModeVisualManager::OnDrawSeparator(CDC* pDC, CBasePane* pBar, CRect rect, BOOL bIsHoriz)
{
    UNREFERENCED_PARAMETER(pDC);
    UNREFERENCED_PARAMETER(pBar);
    UNREFERENCED_PARAMETER(rect);
    UNREFERENCED_PARAMETER(bIsHoriz);
}
