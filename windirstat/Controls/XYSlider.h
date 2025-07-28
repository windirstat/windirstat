// XYSlider.h - Declaration of CXySlider
//
// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
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

//
// CXySlider. A two-dimensional slider.
//
class CXySlider final : public CStatic
{
    DECLARE_DYNAMIC(CXySlider)

    static constexpr UINT XYSLIDER_CHANGED = 0x88; // Notification code to parent
    static constexpr UINT XY_SETPOS = WM_USER + 100; // lparam = POINT *
    static constexpr UINT XY_GETPOS = WM_USER + 101; // lparam = POINT *

    CXySlider() = default;

    void GetRange(CSize& range) const;
    void SetRange(const CSize & range);

    CPoint GetPos() const;
    void SetPos(CPoint pt);

protected:
    void Initialize();
    void CalcSizes();
    CRect GetGripperRect() const;
    void CheckMinMax(LONG& val, int minVal, int maxVal) const;
    void InternToExtern();
    void ExternToIntern();
    void NotifyParent() const;

    void PaintBackground(CDC* pdc);
    void PaintGripper(CDC* pdc) const;
    void DoMoveBy(int cx, int cy);
    void DoDrag(const CPoint & point);
    void DoPage(const CPoint & point);
    void HighlightGripper(bool on);

    bool m_Inited = false;

    // These are in external scale
    CSize m_ExternalRange{ 100, 100 };
    CPoint m_ExternalPos{ 0, 0 };

    // These are in pixels
    CSize m_Range;
    CPoint m_Pos{ 0, 0 }; // relative to m_Zero

    // Constants (in pixels)
    CRect m_RcAll;
    CRect m_RcInner;
    CPoint m_Zero{ 0, 0 };
    CSize m_Radius;
    CSize m_GripperRadius;

    UINT_PTR m_Timer = 0;
    bool m_GripperHighlight = false;

    DECLARE_MESSAGE_MAP()
    afx_msg UINT OnGetDlgCode();
    afx_msg LRESULT OnNcHitTest(CPoint point);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnPaint();
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg LRESULT OnSetPos(WPARAM, LPARAM lparam);
    afx_msg LRESULT OnGetPos(WPARAM, LPARAM lparam);
};

void AFXAPI DDX_XySlider(CDataExchange* pDX, int nIDC, CPoint& value);
