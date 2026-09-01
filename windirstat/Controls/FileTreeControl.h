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

#include "TreeListControl.h"

class CFileTreeControl final : public MessageTarget<CFileTreeControl, CTreeListControl>
{
public:
    CFileTreeControl();
    ~CFileTreeControl() override { m_singleton = nullptr; }
    bool CreateExtended(DWORD dwExStyle, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID) override;
    bool GetAscendingDefault(int column) override;
    static CFileTreeControl* Get() { return m_singleton; }

protected:
    bool GetPortionToolTip(CPoint point, CRect& rect, std::wstring& text) const;
    void ClearPortionToolTip();
    void SelectFirstItemByType(ITEMTYPE itemType);
    inline static CFileTreeControl* m_singleton = nullptr;
    static constexpr UINT PortionToolTipId = 1;
    CToolTipCtrl m_toolTip;
    CRect m_portionToolTipRect;
    std::wstring m_portionToolTipText;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnHScroll(UINT nSBCode, UINT nPos, CWnd* pScrollBar);
    void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    void OnLButtonDown(UINT nFlags, CPoint point);
    void OnMouseMove(UINT nFlags, CPoint point);
    bool OnMouseWheel(UINT nFlags, short zDelta, CPoint point);
    bool OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    void OnTtnGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult);
    void OnVScroll(UINT nSBCode, UINT nPos, CWnd* pScrollBar);
};

inline std::span<const RouteEntry> CFileTreeControl::Routes()
{
    using ThisClass = CFileTreeControl;
    static constexpr std::array entries
    {
        Route::Window<&ThisClass::OnHScroll>(WM_HSCROLL),
        Route::Window<&ThisClass::OnKeyDown>(WM_KEYDOWN),
        Route::Window<&ThisClass::OnLButtonDown>(WM_LBUTTONDOWN),
        Route::Window<&ThisClass::OnMouseMove>(WM_MOUSEMOVE),
        Route::Window<&ThisClass::OnMouseWheel>(WM_MOUSEWHEEL),
        Route::Window<&ThisClass::OnSetCursor>(WM_SETCURSOR),
        Route::Window<&ThisClass::OnVScroll>(WM_VSCROLL),
        Route::Notify<&ThisClass::OnTtnGetDispInfo>(TTN_GETDISPINFOW, PortionToolTipId),
    };
    return entries;
}
