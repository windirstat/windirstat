// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

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
    void OnFontSizeChanged(int oldPercent, int newPercent) override;
    void OnHScroll(UINT nSBCode, UINT nPos, WindowRef pScrollBar);
    void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    void OnLButtonDown(UINT nFlags, CPoint point);
    void OnMouseMove(UINT nFlags, CPoint point);
    bool OnMouseWheel(UINT nFlags, short zDelta, CPoint point);
    bool OnSetCursor(WindowRef pWnd, UINT nHitTest, UINT message);
    void OnTtnGetDispInfo(NMHDR* pNMHDR, LRESULT* pResult);
    void OnVScroll(UINT nSBCode, UINT nPos, WindowRef pScrollBar);
};

inline std::span<const RouteEntry> CFileTreeControl::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnHScroll>(WM_HSCROLL),
        Route::Window<&OnKeyDown>(WM_KEYDOWN),
        Route::Window<&OnLButtonDown>(WM_LBUTTONDOWN),
        Route::Window<&OnMouseMove>(WM_MOUSEMOVE),
        Route::Window<&OnMouseWheel>(WM_MOUSEWHEEL),
        Route::Window<&OnSetCursor>(WM_SETCURSOR),
        Route::Window<&OnVScroll>(WM_VSCROLL),
        Route::Notify<&OnTtnGetDispInfo>(TTN_GETDISPINFOW, PortionToolTipId),
    };
    return entries;
}
