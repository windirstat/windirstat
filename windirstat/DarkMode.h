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

#include "stdafx.h"

#include <algorithm>

class DarkMode final
{
public:

    // window messages related to menu bar drawing
    static constexpr auto WM_UAHDRAWMENU = 0x0091;
    static constexpr auto WM_UAHDRAWMENUITEM = 0x0092;

    // describes the sizes of the menu bar or menu itemWM_NCACTIVATE
    using UAHMENUITEMMETRICS = union UAHMENUITEMMETRICS
    {
        struct {
            DWORD cx;
            DWORD cy;
        } rgsizeBar[2];
        struct {
            DWORD cx;
            DWORD cy;
        } rgsizePopup[4];
    };

    using UAHMENUPOPUPMETRICS = struct
    {
        DWORD rgcx[4];
        DWORD fUpdateMaxWidths : 2;
    };

    using UAHMENU = struct
    {
        HMENU hmenu;
        HDC hdc;
        DWORD dwFlags;
    };

    using UAHMENUITEM = struct
    {
        int iPosition;
        UAHMENUITEMMETRICS umim;
        UAHMENUPOPUPMETRICS umpm;
    };

    using UAHDRAWMENUITEM = struct
    {
        DRAWITEMSTRUCT dis;
        UAHMENU um;
        UAHMENUITEM umi;
    };

    // Check if dark mode is supported on this system
    static bool IsDarkModeActive() noexcept;
    static COLORREF WdsSysColor(DWORD index);

    // Menu rendering functions
    static void DrawMenuBar(HWND hWnd, const UAHMENU* pUDM);
    static void DrawMenuItem(HWND hWnd, UAHDRAWMENUITEM* pUDMI);
    static void DrawMenuClientArea(CWnd& wnd);

    // Enhanced dialog support functions
    static HBRUSH GetDialogBackgroundBrush();
    static void AdjustControls(HWND hWnd);
    static HBRUSH OnCtlColor(CDC* pDC, UINT nCtlColor);
    static void SetAppDarkMode() noexcept;
    static void SetupGlobalColors() noexcept;
    static void LightenBitmap(CBitmap* pBitmap);

private:

    static bool _darkModeEnabled;
};

class DarkModeTabCtrlHelper final : public CMFCTabCtrl
{
public:
    static void SetupDarkMode(CMFCTabCtrl& tab)
    {
        auto& helper = reinterpret_cast<DarkModeTabCtrlHelper&>(tab);

        if (DarkMode::IsDarkModeActive())
        {
            helper.ModifyTabStyle(STYLE_FLAT);
            helper.EnableTabSwap(FALSE);
            helper.SetDrawFrame(FALSE);
            helper.SetTabBorderSize(1);
            helper.SetActiveTabColor(DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
            helper.m_bScroll = FALSE;
        }
        else
        {
            helper.ModifyTabStyle(STYLE_3D_ONENOTE);
            helper.EnableTabSwap(FALSE);
            helper.SetDrawFrame(FALSE);
            helper.m_bScroll = FALSE;
        }
    }
};

class CDarkModeToolBar final : public CMFCToolBar
{
protected:

    void OnFillBackground(CDC* pDC) override
    {
        // Fill toolbar background with dark mode color
        CRect rect;
        GetClientRect(&rect);
        pDC->FillSolidRect(&rect, DarkMode::WdsSysColor(COLOR_MENUBAR));
    }
};

//
// CDarkModeStatusBar. A status bar control that supports dark mode.
//
class CDarkModeStatusBar final : public CMFCStatusBar
{
public:
    CDarkModeStatusBar() = default;

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnPaint();
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

    void OnDrawSeparator(CDC* pDC, CBasePane* pBar, CRect rect, BOOL bIsHoriz) override;
};
