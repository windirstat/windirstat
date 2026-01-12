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
#include "TreeMap.h"

class CDirStatDoc;
class CItem;

//
// CTreeMapView. The treemap window.
//
class CTreeMapView final : public CView
{
protected:
    CTreeMapView() = default;
    DECLARE_DYNCREATE(CTreeMapView)

    ~CTreeMapView() override = default;

    void SuspendRecalculationDrawing(bool suspend);
    bool IsShowTreeMap() const;
    void ShowTreeMap(bool show);
    void DrawEmptyView();
    std::tuple<std::wstring, ULONGLONG> GetTreeMapHoverInfo();

protected:
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override;
    void OnDraw(CDC* pDC) override;
    bool IsDrawn() const;
    void Inactivate();
    void EmptyView();
    void DrawEmptyView(CDC* pDC);

    void DrawZoomFrame(CDC* pdc, CRect& rc) const;
    void DrawHighlights(CDC* pdc);

    void DrawHighlightExtension(CDC* pdc);
    void RecurseHighlightExtension(CDC* pdc, const CItem* item);

    void DrawSelection(CDC* pdc) const;

    void HighlightSelectedItem(CDC* pdc, const CItem* item, bool single) const;
    void RenderHighlightRectangle(CDC* pdc, CRect& rc) const;

    static constexpr int ZoomFrameWidth = 4;

    std::wstring m_paneTextOverride;  // Populated with the last hovered item for a period of time
    ULONGLONG m_paneSizeOverride = 0; // Size of the last hovered item for display in the pane text
    bool m_drawingSuspended = false;  // True while the user is resizing the window.
    bool m_showTreeMap = true;        // False, if the user switched off the treemap (by F9).
    CSize m_size{ 0, 0 };             // Current size of view
    CTreeMap m_treeMap;               // Treemap generator
    CBitmap m_bitmap;                 // Cached view. If m_hObject is NULL, the view must be recalculated.
    CSize m_dimmedSize{ 0,0 };        // Size of bitmap m_dimmed
    CBitmap m_dimmed;                 // Dimmed view. Used during refresh to avoid the ooops-effect.

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
};

