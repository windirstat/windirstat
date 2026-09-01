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

//
// CXySlider. A two-dimensional slider.
//
class CXySlider final : public MessageTarget<CXySlider, CStatic>
{
public:
    static constexpr UINT XYSLIDER_CHANGED = 0x88; // Notification code to parent
    static constexpr UINT XY_SETPOS = WM_USER + 100; // lparam = POINT *
    static constexpr UINT XY_GETPOS = WM_USER + 101; // lparam = POINT *

    CXySlider() = default;

    void GetRange(CSize& range) const { range = m_externalRange; }
    void SetRange(const CSize & range) { m_externalRange = range; }

    CPoint GetPos() const { return m_externalPos; }
    void SetPos(CPoint pt);

protected:
    void Initialize();
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

    bool m_gripperHighlight = false;

public:
    static std::span<const RouteEntry> Routes();

protected:
    UINT OnGetDlgCode() { return DLGC_WANTARROWS; }
    LRESULT OnNcHitTest(CPoint) { return HTCLIENT; }
    void OnSetFocus(CWnd* pOldWnd);
    void OnKillFocus(CWnd* pNewWnd);
    void OnPaint();
    void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    void OnLButtonDown(UINT nFlags, CPoint point);
    void OnLButtonDblClk(UINT nFlags, CPoint point);
    LRESULT OnSetPos(WPARAM, LPARAM lparam);
    LRESULT OnGetPos(WPARAM, LPARAM lparam) const;
};

inline std::span<const RouteEntry> CXySlider::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnGetDlgCode>(WM_GETDLGCODE),
        Route::Window<&OnNcHitTest>(WM_NCHITTEST),
        Route::Window<&OnSetFocus>(WM_SETFOCUS),
        Route::Window<&OnKillFocus>(WM_KILLFOCUS),
        Route::Window<&OnPaint>(WM_PAINT),
        Route::Window<&OnKeyDown>(WM_KEYDOWN),
        Route::Window<&OnLButtonDown>(WM_LBUTTONDOWN),
        Route::Window<&OnLButtonDblClk>(WM_LBUTTONDBLCLK),
        Route::Window<&OnSetPos>(XY_SETPOS),
        Route::Window<&OnGetPos>(XY_GETPOS),
    };
    return entries;
}
