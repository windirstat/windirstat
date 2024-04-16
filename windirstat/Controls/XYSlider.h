// XYSlider.h - Declaration of CXySlider
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
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

#pragma once

#define XYSLIDER_CHANGED    0x88    // this is a value, I hope, that is nowhere used as notification code.

//
// CXySlider. A two-dimensional slider.
//
class CXySlider final : public CStatic
{
    DECLARE_DYNAMIC(CXySlider)

    static const UINT XY_SETPOS; // lparam = POINT *
    static const UINT XY_GETPOS; // lparam = POINT *

    CXySlider() = default;

    void GetRange(CSize& range) const;
    void SetRange(CSize range);

    CPoint GetPos() const;
    void SetPos(CPoint pt);

    // "Line size" is always 1 Pixel
    // "Page size" is always 10 Pixel

protected:
    void Initialize();
    void CalcSizes();
    CRect GetGripperRect() const;
    void CheckMinMax(LONG& val, int min, int max);
    void InternToExtern();
    void ExternToIntern();
    void NotifyParent();

    void PaintBackground(CDC* pdc);
    // void PaintValues(CDC *pdc); This is too noisy
    void PaintGripper(CDC* pdc);
    void DoMoveBy(int cx, int cy);
    void DoDrag(CPoint point);
    void DoPage(CPoint point);
    void HighlightGripper(bool on);
    void InstallTimer();
    void RemoveTimer();

    bool m_inited = false;

    // These are in external scale
    CSize m_externalRange{ 100, 100 };
    CPoint m_externalPos{ 0, 0 };

    // These are in pixels
    CSize m_range;
    CPoint m_pos{ 0, 0 }; // relative to m_zero

    // Constants (in pixels)
    CRect m_rcAll;
    CRect m_rcInner;
    CPoint m_zero{ 0, 0 };
    CSize m_radius;
    CSize m_gripperRadius;

    UINT_PTR m_timer = 0;
    bool m_gripperHighlight = false;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnDestroy();
    afx_msg UINT OnGetDlgCode();
    afx_msg LRESULT OnNcHitTest(CPoint point);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnPaint();
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg LRESULT OnSetPos(WPARAM, LPARAM lparam);
    afx_msg LRESULT OnGetPos(WPARAM, LPARAM lparam);
};

void AFXAPI DDX_XySlider(CDataExchange* pDX, int nIDC, CPoint& value);
