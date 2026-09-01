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
#include "WinDirStatPane.h"
#include "ExtensionListControl.h"

//
// CExtensionView. The upper right view, which shows the extensions and their
// cushion colors.
//
class CExtensionView final : public MessageTarget<CExtensionView, CWinDirStatPane>
{
public:
    CExtensionView();

    ~CExtensionView() override = default;

    void SysColorChanged();
    bool IsShowTypes() const { return m_showTypes; }
    void ShowTypes(bool show);

    void SetHighlightExtension(const std::wstring& ext, bool unregistered = false);

    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;
    void SetSelection();

    bool m_showTypes = true; // Whether this view shall be shown (F8 option)
    CExtensionListControl m_extensionListControl; // The list control

static std::span<const RouteEntry> Routes();

protected:
    int OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnSize(UINT nType, int cx, int cy);
    void OnSetFocus(CWnd* pOldWnd);
    bool OnEraseBkgnd(CDC*) { return true; }
};

inline std::span<const RouteEntry> CExtensionView::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnCreate>(WM_CREATE),
        Route::Window<&OnEraseBkgnd>(WM_ERASEBKGND),
        Route::Window<&OnSize>(WM_SIZE),
        Route::Window<&OnSetFocus>(WM_SETFOCUS),
    };
    return entries;
}
