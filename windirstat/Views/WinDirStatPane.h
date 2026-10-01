// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

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

    virtual void OnDraw(CDC* pDC);

    virtual void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item);
    virtual HoverInfo GetHoverInfo() const { return {}; }
    virtual void SuspendRecalculationDrawing(bool /*suspend*/) {}

static std::span<const RouteEntry> Routes();

protected:
    int OnCreate(LPCREATESTRUCT) { return 0; }
    int OnMouseActivate(WindowRef pDesktopWnd, UINT nHitTest, UINT message) override;
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
