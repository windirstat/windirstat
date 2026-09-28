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
