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
class CGraphView : public CWinDirStatPane
{
protected:
    DECLARE_DYNAMIC(CGraphView)

    CGraphView() = default;
    ~CGraphView() override = default;

public:
    static constexpr COLORREF BackgroundColor = RGB(15, 15, 15);

    void SuspendRecalculationDrawing(bool suspend) final;
    void TrimRenderCache();
    void DrawEmptyView();
    HoverInfo GetHoverInfo() const final;

protected:
    BOOL PreCreateWindow(CREATESTRUCT& cs) final;
    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;
    void OnDraw(CDC* pDC) final;

    [[nodiscard]] virtual const wchar_t* GetWindowClassName() const = 0;
    virtual void DrawEmptyPlaceholder(CDC* pDC, const CRect& rect) = 0;
    [[nodiscard]] virtual bool CreateRenderBitmap(CDC* pDC, CSize size);
    [[nodiscard]] virtual bool PrepareDrawing(CDC* pDC, CRect& rect);
    virtual void RenderVisualization(CDC* pDC, CRect rect) = 0;
    virtual void DrawSelection(CDC* pDC) = 0;
    virtual void DrawHighlightExtension(CDC* pDC) = 0;
    [[nodiscard]] virtual CItem* FindItemAtPoint(CPoint point) = 0;
    [[nodiscard]] virtual bool HasValidLayout() const { return IsDrawn(); }

    virtual void ClearVisualizationLayout() {}
    virtual void OnViewEmptied() {}
    virtual void OnSuspending() {}
    virtual void OnBeforeSizeChanged() {}
    virtual void OnInputStateReset() {}
    virtual void OnRenderCacheTrimmed() {}
    virtual bool UpdateHoverDetails(const CItem* item, bool itemChanged);
    [[nodiscard]] virtual bool CanReuseVisualizationLayout(MODEL_CHANGE /*change*/) const
    {
        return false;
    }
    virtual void OnVisualizationChanged(MODEL_CHANGE change);
    virtual void DrillDown(CItem* item);
    void ResetZoom(CPoint point);
    [[nodiscard]] virtual std::span<const UINT> GetPersistentContextCommands() const;

    [[nodiscard]] bool IsReadyToDraw() const;
    [[nodiscard]] bool IsDrawn() const { return m_bitmap.m_hObject != nullptr; }
    void DrawHighlights(CDC* pDC);
    void Inactivate(bool clearLayout = true);
    void EmptyView();

    [[nodiscard]] CItem* ResolveItemAtPoint(CPoint point, bool isScreenCoords = false);
    void ClearHover();
    [[nodiscard]] static bool IsExtensionHighlighted(const CItem* item);
    [[nodiscard]] static const CItem* GetDisplayItem(const CItem* item);
    static void RenderHighlightRectangle(CDC* pDC, CRect& rect);

private:
    void PaintEmptyView(CDC* pDC);
    [[nodiscard]] bool DrawDimmedView(CDC* pDC);
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

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
};
