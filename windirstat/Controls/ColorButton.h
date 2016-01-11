// colorbutton.h - Declaration of CColorButton
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2016 WinDirStat team (windirstat.info)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#ifndef __WDS_COLORBUTTON_H__
#define __WDS_COLORBUTTON_H__
#pragma once

#define COLBN_CHANGED   0x87    // this is a value, I hope, that is nowhere used as notification code.

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
class CColorButton: public CButton
{
public:
    COLORREF GetColor();
    void SetColor(COLORREF color);

private:
    // The color preview is an own little child window of the button.
    class CPreview: public CWnd
    {
    public:
        CPreview();
        COLORREF GetColor();
        void SetColor(COLORREF color);

    private:
        COLORREF m_color;

        DECLARE_MESSAGE_MAP()
        afx_msg void OnPaint();
    public:
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

#endif // __WDS_COLORBUTTON_H__
