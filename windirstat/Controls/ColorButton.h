// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"

constexpr auto COLBN_CHANGED = 0x87;

//
// CColorButton. A Pushbutton which allows the user to choose a color and
// shows this color on its surface.
//
// In the resource editor, the button should be set to "right align text",
// as the color will be shown in the left third.
//
// When the user chooses a color, the parent is notified via WM_NOTIFY
// and the notification code COLBN_CHANGED.
//
class CColorButton final : public MessageTarget<CColorButton, CButton>
{
public:
    COLORREF GetColor() const { return m_preview.GetColor(); }
    void SetColor(const COLORREF color) { m_preview.SetColor(color); }

private:
    // The color preview is the button's own little child window.
    class CPreview final : public MessageTarget<CPreview, CWnd>
    {
    public:
        CPreview() = default;
        COLORREF GetColor() const { return m_color; }
        void SetColor(COLORREF color);

    private:
        COLORREF m_color = 0;

    public:
        static std::span<const RouteEntry> Routes();

    protected:
        void OnPaint();
        void OnLButtonDown(UINT nFlags, CPoint point) const;
    };

    CPreview m_preview;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnPaint();
    void OnDestroy();
    void OnBnClicked();
    void OnEnable(bool bEnable);
};

inline std::span<const RouteEntry> CColorButton::CPreview::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnPaint>(WM_PAINT),
        Route::Window<&OnLButtonDown>(WM_LBUTTONDOWN),
    };
    return entries;
}

inline std::span<const RouteEntry> CColorButton::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnPaint>(WM_PAINT),
        Route::Window<&OnDestroy>(WM_DESTROY),
        Route::ReflectControl<&OnBnClicked>(BN_CLICKED),
        Route::Window<&OnEnable>(WM_ENABLE),
    };
    return entries;
}
