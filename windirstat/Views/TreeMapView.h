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
#include "GraphView.h"
#include "TreeMap.h"

class CWinDirStatModel;
class CItem;

//
// CTreeMapView. The treemap window.
//
class CTreeMapView final : public CGraphView
{
public:
    CTreeMapView() = default;

    ~CTreeMapView() override = default;

    const wchar_t* GetWindowClassName() const override
    {
        return L"WinDirStatTreeMapClass";
    }
    void DrawEmptyPlaceholder(CDC* pDC, const CRect& rect) override;
    bool CreateRenderBitmap(CDC* pDC, CSize size) override;
    void RenderVisualization(CDC* pDC, CRect rect) override;

    void DrawZoomFrame(CDC* pdc, CRect& rc) const;
    void DrawHighlightExtension(CDC* pdc) override;
    void DrawSelection(CDC* pdc) override;

    void HighlightSelectedItem(CDC* pdc, const CItem* item, bool single) const;
    CItem* FindItemAtPoint(CPoint point) override;
    bool HasValidLayout() const override;
    void ClearVisualizationLayout() override;
    void OnRenderCacheTrimmed() override;
    void DrillDown(CItem* item) override;
    std::span<const UINT> GetPersistentContextCommands() const override;

    static constexpr int ZoomFrameWidth = 4;

    CTreeMap m_treeMap;               // Treemap generator

};
