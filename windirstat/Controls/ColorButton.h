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
class CColorButton final : public CButton
{
public:
    COLORREF GetColor() const;
    void SetColor(COLORREF color);

private:
    // The color preview is an own little child window of the button.
    class CPreview final : public CWnd
    {
    public:
        CPreview();
        COLORREF GetColor() const;
        void SetColor(COLORREF color);

    private:
        COLORREF m_color;

        DECLARE_MESSAGE_MAP()
        afx_msg void OnPaint();
        afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    };

    CPreview m_preview;

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg void OnDestroy();
    afx_msg void OnBnClicked();
    afx_msg void OnEnable(BOOL bEnable);
};
