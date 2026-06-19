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
#include "Options.h"

// CLayoutPopup - 2x4 grid of layout cards, anchored to a toolbar button.
class CLayoutPopup final : public CWnd
{
public:
    static constexpr int LAYOUT_COUNT = 8;

    struct Pane
    {
        int   viewType; // 0=AllFiles, 1=FileTypes, 2=TreeMap; -1=unused
        float x, y, w, h;
    };

    struct LayoutDef
    {
        Pane panes[3];
        int  topology;
        int  permutation;
    };

    static const LayoutDef LAYOUTS[LAYOUT_COUNT];

    static int LayoutIndex(int topology, int permutation);
    static int CurrentLayoutIndex();
    BOOL Create(CWnd* parent);
    void ShowAtButton(const CRect& buttonScreenRect);
    void DismissPopup(bool cancel = false, bool resetPositions = false);

protected:
    int m_selectedLayout = 0;  // currently active layout index
    int m_hoveredLayout  = -1; // -1 = none

    static constexpr int CARD_W_BASE = 192;
    static constexpr int CARD_H_BASE = CARD_W_BASE * 9 / 16;
    static constexpr int GAP_BASE    =  12;
    static constexpr int MARGIN_BASE =  14;
    static constexpr int COLS        =   2;
    static constexpr int ROWS        =   4;

    CRect CardRect(int idx) const;
    int   CardAtPoint(CPoint pt) const;
    void PaintCard(CDC& dc, int idx) const;
    void DrawAllFilesPane(CDC& dc, CRect r) const;
    void DrawFileTypesPane(CDC& dc, CRect r) const;
    void DrawTreeMapPane(CDC& dc, CRect r, int cardIdx) const;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnActivateApp(BOOL bActive, DWORD dwThreadID);
    afx_msg void OnCaptureChanged(CWnd* pWnd);
    afx_msg LRESULT OnMouseLeave(WPARAM, LPARAM);
};
