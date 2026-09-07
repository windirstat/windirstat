// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
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
#include "WinDirStatPane.h"

// Shared window, interaction, and bitmap-cache lifecycle for the disk-usage
// visualizations. Derived classes supply only their renderer-specific drawing
// and geometry operations.
class CGraphView : public MessageTarget<CGraphView, CWinDirStatPane>
{
public:
    CGraphView() = default;
    ~CGraphView() override = default;

    static constexpr COLORREF BackgroundColor = RGB(15, 15, 15);

    void SuspendRecalculationDrawing(bool suspend) final;
    void TrimRenderCache();
    void DrawEmptyView();
    HoverInfo GetHoverInfo() const final;

protected:
    bool PreCreateWindow(CREATESTRUCT& cs) final;
    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;
    void OnDraw(CDC* pDC) final;

    virtual const wchar_t* GetWindowClassName() const = 0;
    virtual void DrawEmptyPlaceholder(CDC* pDC, const CRect& rect) = 0;
    virtual bool CreateRenderBitmap(CDC* pDC, CSize size);
    virtual bool PrepareDrawing(CDC* pDC, CRect& rect);
    virtual void RenderVisualization(CDC* pDC, CRect rect) = 0;
    virtual void DrawSelection(CDC* pDC) = 0;
    virtual void DrawHighlightExtension(CDC* pDC) = 0;
    virtual CItem* FindItemAtPoint(CPoint point) = 0;
    virtual bool HasValidLayout() const { return IsDrawn(); }

    virtual void ClearVisualizationLayout() {}
    virtual void OnViewEmptied() {}
    virtual void OnSuspending() {}
    virtual void OnBeforeSizeChanged() {}
    virtual void OnInputStateReset() {}
    virtual void OnRenderCacheTrimmed() {}
    virtual bool UpdateHoverDetails(const CItem* item, bool itemChanged);
    virtual bool CanReuseVisualizationLayout(MODEL_CHANGE /*change*/) const
    {
        return false;
    }
    virtual void OnVisualizationChanged(MODEL_CHANGE change);
    virtual void DrillDown(CItem* item);
    void ResetZoom(CPoint point);
    virtual std::span<const UINT> GetPersistentContextCommands() const;

    bool IsReadyToDraw() const;
    bool IsDrawn() const { return m_bitmap.m_hObject != nullptr; }
    void DrawHighlights(CDC* pDC);
    void Inactivate(bool clearLayout = true);
    void EmptyView();

    CItem* ResolveItemAtPoint(CPoint point, bool isScreenCoords = false);
    void ClearHover();
    static bool IsExtensionHighlighted(const CItem* item);
    static const CItem* GetDisplayItem(const CItem* item);
    static void RenderHighlightRectangle(CDC* pDC, CRect& rect);

private:
    void PaintEmptyView(CDC* pDC);
    bool DrawDimmedView(CDC* pDC);
    void DiscardRenderCache(bool clearLayout = true);
    void ResetInputState();

protected:
    std::wstring m_paneTextOverride;
    ULONGLONG m_paneSizeOverride = 0;
    bool m_drawingSuspended = false;
    bool m_trackingMouse = false;
    int m_navigationWheelDeltaRemainder = 0;
    int m_zoomWheelDeltaRemainder = 0;
    const CItem* m_hoverItem = nullptr;
    CSize m_size{ 0, 0 };
    CBitmap m_bitmap;
    CSize m_dimmedSize{ 0, 0 };
    CBitmap m_dimmed;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnSize(UINT nType, int cx, int cy);
    void OnLButtonDblClk(UINT nFlags, CPoint point);
    void OnLButtonDown(UINT nFlags, CPoint point);
    void OnMButtonDown(UINT nFlags, CPoint point);
    void OnSetFocus(CWnd* pOldWnd);
    void OnContextMenu(CWnd* pWnd, CPoint point);
    void OnMouseMove(UINT nFlags, CPoint point);
    void OnMouseLeave();
    bool OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    void OnFontSizeChanged(int, int) override { TrimRenderCache(); Invalidate(false); }
};

inline std::span<const RouteEntry> CGraphView::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnSize>(WM_SIZE),
        Route::Window<&OnLButtonDblClk>(WM_LBUTTONDBLCLK),
        Route::Window<&OnLButtonDown>(WM_LBUTTONDOWN),
        Route::Window<&OnMButtonDown>(WM_MBUTTONDOWN),
        Route::Window<&OnSetFocus>(WM_SETFOCUS),
        Route::Window<&OnContextMenu>(WM_CONTEXTMENU),
        Route::Window<&OnMouseMove>(WM_MOUSEMOVE),
        Route::Window<&OnMouseLeave>(WM_MOUSELEAVE),
        Route::Window<&OnMouseWheel>(WM_MOUSEWHEEL),
    };
    return entries;
}
