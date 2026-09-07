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

class CItem;
enum MODEL_CHANGE : std::uint8_t;

struct HoverInfo
{
    std::wstring_view path;
    ULONGLONG size = 0;
};

//
// CWinDirStatPane. A plain child window base for splitter/tab panes.
// It supplies the small subset of pane behavior this UI uses.
//
class CWinDirStatPane : public MessageTarget<CWinDirStatPane, CWnd>
{
public:
    CWinDirStatPane() = default;
    ~CWinDirStatPane() override = default;

    void PostNcDestroy() override;

    virtual void OnDraw(CDC* pDC);

    virtual void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item);
    virtual HoverInfo GetHoverInfo() const { return {}; }
    virtual void SuspendRecalculationDrawing(bool /*suspend*/) {}

static std::span<const RouteEntry> Routes();

protected:
    int OnCreate(LPCREATESTRUCT) { return 0; }
    int OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message) override;
    void OnPaint();
    bool OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

    void NotifyOtherPanes(MODEL_CHANGE change = MODEL_CHANGE_NONE, CItem* item = nullptr);
    void ShowGraphContextMenu(CItem* clickedItem, CPoint point,
        std::span<const UINT> persistentCommands);
};

inline std::span<const RouteEntry> CWinDirStatPane::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnCreate>(WM_CREATE),
        Route::Window<&OnMouseActivate>(WM_MOUSEACTIVATE),
        Route::Window<&OnPaint>(WM_PAINT),
    };
    return entries;
}
