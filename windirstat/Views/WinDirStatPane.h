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

class CItem;
enum MODEL_CHANGE : std::uint8_t;

struct HoverInfo
{
    std::wstring path;
    ULONGLONG size = 0;
    bool IsEmpty() const { return path.empty(); }
};

//
// CWinDirStatPane. A plain child window base for splitter/tab panes.
// It supplies the small subset of pane behavior this UI uses.
//
class CWinDirStatPane : public CWnd
{
protected:
    DECLARE_DYNAMIC(CWinDirStatPane)

    CWinDirStatPane() = default;
    ~CWinDirStatPane() override = default;

    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    void PostNcDestroy() override;

    virtual void OnDraw(CDC* pDC);

public:
    virtual void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item);
    virtual HoverInfo GetHoverInfo() const { return {}; }
    virtual void SuspendRecalculationDrawing(bool /*suspend*/) {};

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg int OnMouseActivate(CWnd* pDesktopWnd, UINT nHitTest, UINT message);
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

    void NotifyOtherPanes(MODEL_CHANGE change = MODEL_CHANGE_NONE, CItem* item = nullptr);
};
