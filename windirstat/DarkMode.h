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
    static bool IsDarkModeActive() noexcept;
    static COLORREF WdsSysColor(DWORD index);

    // Menu rendering functions
    static void DrawMenuClientArea(CWnd& wnd);
    
    // Unified message handler for menu drawing messages
    static LRESULT HandleMenuMessage(UINT message, WPARAM wParam, LPARAM lParam, HWND hWnd);

    // Enhanced dialog support functions
    static HBRUSH GetDialogBackgroundBrush();
    static void AdjustControls(HWND hWnd);
    static HBRUSH OnCtlColor(CDC* pDC, UINT nCtlColor);
    static void SetAppDarkMode() noexcept;
    static void SetupGlobalColors() noexcept;
    static HICON LightenIcon(HICON hIcon, bool invert = false);
    static void LightenBitmap(CBitmap* pBitmap, bool invert = false);

private:

    static bool _darkModeEnabled;
};

//
// CTabCtrlHelper. Used to set up tab control properties.
//
class CTabCtrlHelper final : public CMFCTabCtrl
{
public:
    static void SetupTabControl(CMFCTabCtrl& tab)
    {
        auto& helper = reinterpret_cast<CTabCtrlHelper&>(tab);

        helper.ModifyTabStyle(DarkMode::IsDarkModeActive() ? STYLE_FLAT : STYLE_3D_VS2005);
        helper.EnableTabSwap(FALSE);
        helper.SetDrawFrame(FALSE);
        helper.SetScrollButtons();

        // Forcibly hide tabs
        if (IsWindow(helper.m_btnScrollFirst)) helper.m_btnScrollFirst.ShowWindow(SW_HIDE);
        if (IsWindow(helper.m_btnScrollLast)) helper.m_btnScrollLast.ShowWindow(SW_HIDE);
        if (IsWindow(helper.m_btnScrollLeft)) helper.m_btnScrollLeft.ShowWindow(SW_HIDE);
        if (IsWindow(helper.m_btnScrollRight)) helper.m_btnScrollRight.ShowWindow(SW_HIDE);
        helper.m_bScroll = FALSE;

        // Dark mode tab have a black background so set text to be white
        if (DarkMode::IsDarkModeActive())
        {
            helper.SetActiveTabColor(DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
            helper.SetTabBorderSize(1);
        }
    }
};

//
// CDarkModeVisualManager. A visual manager tweak for dark mode support
//
class CDarkModeVisualManager final : public CMFCVisualManagerWindows7
{
    DECLARE_DYNCREATE(CDarkModeVisualManager)

protected:

    void GetTabFrameColors(const CMFCBaseTabCtrl* pTabWnd,
        COLORREF& clrDark, COLORREF& clrBlack, COLORREF& clrHighlight, COLORREF& clrFace,
        COLORREF& clrDarkShadow, COLORREF& clrLight, CBrush*& pbrFace, CBrush*& pbrBlack) override;
    void OnFillBarBackground(CDC* pDC, CBasePane* pBar, CRect rectClient, CRect rectClip, BOOL bNCArea) override;
    void OnDrawSeparator(CDC* pDC, CBasePane* pBar, CRect rect, BOOL bIsHoriz) override;
    void OnDrawStatusBarPaneBorder(CDC* pDC, CMFCStatusBar* pBar, CRect rectPane, UINT uiID, UINT nStyle) override;
    void OnUpdateSystemColors() override;
};
