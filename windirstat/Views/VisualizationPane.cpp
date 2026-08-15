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

#include "pch.h"
#include "VisualizationPane.h"
#include "TreeMapView.h"
#include "FlameGraphView.h"
#include "SunburstView.h"

static_assert(static_cast<std::size_t>(GraphPane::TreeMap) == 0
    && static_cast<std::size_t>(GraphPane::FlameGraph) == 1
    && static_cast<std::size_t>(GraphPane::Sunburst) == 2);

bool CVisualizationPane::PreCreateWindow(CREATESTRUCT& cs)
{
    cs.style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    return CWinDirStatPane::PreCreateWindow(cs);
}

int CVisualizationPane::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CWinDirStatPane::OnCreate(lpCreateStruct) == -1) return -1;

    struct ViewDefinition
    {
        CGraphView* (*create)();
        DWORD style;
    };
    const std::array definitions{
        ViewDefinition{ []() -> CGraphView* { return new CTreeMapView; }, WS_CHILD },
        ViewDefinition{ []() -> CGraphView* { return new CFlameGraphView; }, WS_CHILD | WS_VSCROLL },
        ViewDefinition{ []() -> CGraphView* { return new CSunburstView; }, WS_CHILD },
    };

    for (std::size_t index = 0; index < definitions.size(); ++index)
    {
        CGraphView* view = definitions[index].create();
        m_views[index] = view;
        if (!view->Create(nullptr, nullptr, definitions[index].style, CRect{}, this,
            static_cast<UINT>(WDS_PANE_ID_BASE + index)))
        {
            // Create invokes PostNcDestroy on failure, which deletes the view.
            m_views[index] = nullptr;
            return -1;
        }
    }

    m_activePane = DecodeGraphPane(COptions::GraphPaneStyle);
    m_showVisualization = COptions::ShowVisualization;
    ShowVisualization(m_showVisualization);
    return 0;
}

void CVisualizationPane::OnDraw(CDC* pDC)
{
    pDC->FillSolidRect(ClientRect(), CGraphView::BackgroundColor);
}

void CVisualizationPane::SelectPane(const GraphPane pane)
{
    CGraphView* previous = GetActiveView();
    if (m_activePane != pane)
    {
        if (previous != nullptr)
        {
            previous->ShowWindow(SW_HIDE);
            previous->TrimRenderCache();
        }
        m_activePane = pane;
    }

    CGraphView* active = GetActiveView();
    if (active == nullptr) return;

    if (!m_showVisualization)
    {
        active->ShowWindow(SW_HIDE);
        active->TrimRenderCache();
        return;
    }

    active->MoveWindow(ClientRect(), false);
    active->ShowWindow(SW_SHOW);
    active->Invalidate(false);
}

void CVisualizationPane::ShowVisualization(const bool show)
{
    m_showVisualization = show;
    CGraphView* active = GetActiveView();
    if (active == nullptr) return;

    if (!show)
    {
        active->ShowWindow(SW_HIDE);
        active->TrimRenderCache();
        return;
    }

    active->MoveWindow(ClientRect(), false);
    active->ShowWindow(SW_SHOW);
    active->Invalidate(false);
}

void CVisualizationPane::OnUpdate(CWnd* sender, const MODEL_CHANGE change, CItem* item)
{
    for (CGraphView* view : m_views)
    {
        if (view != nullptr && view != sender)
            static_cast<CWinDirStatPane*>(view)->OnUpdate(sender, change, item);
    }
}

HoverInfo CVisualizationPane::GetHoverInfo() const
{
    if (!m_showVisualization) return {};
    const CGraphView* active = GetActiveView();
    return active == nullptr ? HoverInfo{} : active->GetHoverInfo();
}

void CVisualizationPane::SuspendRecalculationDrawing(const bool suspend)
{
    // Scans and interactive window resizing can overlap. Keep their paired
    // suspension requests independent so ending one does not resume the views
    // while the other still owns a suspension.
    if (suspend)
    {
        if (m_drawingSuspensionCount++ != 0) return;
    }
    else
    {
        if (m_drawingSuspensionCount == 0)
        {
            assert(false);
            return;
        }
        if (--m_drawingSuspensionCount != 0) return;
    }

    for (CGraphView* view : m_views)
    {
        if (view != nullptr) view->SuspendRecalculationDrawing(suspend);
    }
}

void CVisualizationPane::OnSetFocus(CWnd* /*pOldWnd*/)
{
    if (CGraphView* active = GetActiveView(); m_showVisualization && active != nullptr)
        active->SetFocus();
}

void CVisualizationPane::OnSize(UINT /*nType*/, const int cx, const int cy)
{
    if (CGraphView* active = GetActiveView(); active != nullptr && active->Handle())
        active->MoveWindow(0, 0, cx, cy);
}
