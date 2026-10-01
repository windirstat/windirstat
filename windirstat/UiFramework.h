// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "UiPanes.h"

// -----------------------------------------------------------------------------
//  Application command and resource IDs
// -----------------------------------------------------------------------------
#include "resource.h"

inline constexpr UINT ID_SEPARATOR = 0;

// CWinApp's routes are defined here because ID_APP_EXIT must be visible first.
// It is inline because this header is included by multiple translation units.
inline std::span<const RouteEntry> CWinApp::Routes()
{
    static constexpr std::array entries
    {
        Route::Command<&OnAppExit>(ID_APP_EXIT),
    };
    return entries;
}

// CFrameWnd's routes handle ID_VIEW_TOOLBAR and ID_VIEW_STATUS_BAR.
// It is inline for the same reason as CWinApp's table above.
inline std::span<const RouteEntry> CFrameWnd::Routes()
{
    static constexpr std::array entries
    {
        Route::Command<&OnToggleViewBar>(ID_VIEW_TOOLBAR, ID_VIEW_STATUS_BAR),
        Route::Update<&OnUpdateViewBarMenu>(ID_VIEW_TOOLBAR, ID_VIEW_STATUS_BAR),
    };
    return entries;
}

inline void CFrameWnd::OnToggleViewBar(const UINT nID)
{
    CWnd* bar = nID == ID_VIEW_TOOLBAR ? m_topBar : nID == ID_VIEW_STATUS_BAR ? m_bottomBar : nullptr;
    if (bar == nullptr) return;
    bar->ShowWindow(bar->IsWindowVisible() ? SW_HIDE : SW_SHOW);
    UpdateLayout();
}

inline void CFrameWnd::OnUpdateViewBarMenu(CCmdUI* pCmdUI) const
{
    const CWnd* bar = pCmdUI->m_nID == ID_VIEW_TOOLBAR ? m_topBar :
        pCmdUI->m_nID == ID_VIEW_STATUS_BAR ? m_bottomBar : nullptr;
    pCmdUI->Enable(bar != nullptr);
    if (bar != nullptr) pCmdUI->SetCheck(bar->IsWindowVisible() ? 1 : 0);
}
