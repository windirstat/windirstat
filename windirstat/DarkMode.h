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

class DarkMode final
{
public:
    // window messages related to menu bar drawing
    static constexpr auto WM_UAHDRAWMENU = 0x0091;
    static constexpr auto WM_UAHDRAWMENUITEM = 0x0092;

    // Check if dark mode is supported on this system
    static bool IsDarkModeActive() noexcept { return s_darkModeEnabled; }
    static bool EnhancedDarkModeSupport();
    static COLORREF SystemColor(DWORD index);

    // Menu rendering functions
    static void DrawMenuClientArea(CWnd& wnd);

    // Unified message handler for menu drawing messages
    static LRESULT HandleMenuMessage(UINT message, WPARAM wParam, LPARAM lParam, HWND hWnd);

    // Enhanced dialog support functions
    static HBRUSH GetDialogBackgroundBrush();
    static void AdjustControls(HWND hWnd);
    static HBRUSH OnCtlColor(CDC* pDC, UINT nCtlColor);
    static void SetAppDarkMode() noexcept;
    static void LightenBitmap(CBitmap* pBitmap, bool invert = false);
    static void DrawFocusRect(CDC* pdc, const CRect& rc);

private:
    static bool s_darkModeEnabled;
};

//
// CTabCtrlHelper. Used to set up tab control properties.
//
class CTabCtrlHelper final
{
public:
    static void SetupTabControl(CTabControl& tab)
    {
        tab.SetContentBackgroundColor(DarkMode::IsDarkModeActive() ? DarkMode::SystemColor(COLOR_WINDOWTEXT) : CLR_NONE);
    }
};
