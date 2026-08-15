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

constexpr auto COLBN_CHANGED = 0x87;

//
// CColorButton. A Pushbutton which allows to choose a color and
// shows this color on its surface.
//
// In the resource editor, the button should be set to "right align text",
// as the color will be shown in the left third.
//
// When the user chose a color, the parent is notified via WM_NOTIFY
// and the notification code COLBN_CHANGED.
//
class CColorButton final : public MessageTarget<CColorButton, CButton>
{
public:
    COLORREF GetColor() const { return m_preview.GetColor(); }
    void SetColor(const COLORREF color) { m_preview.SetColor(color); }

private:
    // The color preview is an own little child window of the button.
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
    using ThisClass = CPreview;
    static constexpr std::array entries
    {
        Route::Window<&ThisClass::OnPaint>(WM_PAINT),
        Route::Window<&ThisClass::OnLButtonDown>(WM_LBUTTONDOWN),
    };
    return entries;
}

inline std::span<const RouteEntry> CColorButton::Routes()
{
    using ThisClass = CColorButton;
    static constexpr std::array entries
    {
        Route::Window<&ThisClass::OnPaint>(WM_PAINT),
        Route::Window<&ThisClass::OnDestroy>(WM_DESTROY),
        Route::ReflectControl<&ThisClass::OnBnClicked>(BN_CLICKED),
        Route::Window<&ThisClass::OnEnable>(WM_ENABLE),
    };
    return entries;
}
