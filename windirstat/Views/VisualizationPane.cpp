// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "VisualizationPane.h"
#include "TreeMapView.h"
#include "FlameGraphView.h"
#include "SunburstView.h"

static_assert(std::to_underlying(GraphPane::TreeMap) == 0
    && std::to_underlying(GraphPane::FlameGraph) == 1
    && std::to_underlying(GraphPane::Sunburst) == 2);

CVisualizationPane::~CVisualizationPane()
{
    DestroyWindow();
}

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
        std::unique_ptr<CGraphView> (*create)();
        DWORD style;
    };
    const std::array definitions{
        ViewDefinition{ []() -> std::unique_ptr<CGraphView> { return std::make_unique<CTreeMapView>(); }, WS_CHILD },
        ViewDefinition{ []() -> std::unique_ptr<CGraphView> { return std::make_unique<CFlameGraphView>(); },
            WS_CHILD | WS_VSCROLL },
        ViewDefinition{ []() -> std::unique_ptr<CGraphView> { return std::make_unique<CSunburstView>(); }, WS_CHILD },
    };

    for (const auto [index, def] : std::views::enumerate(definitions))
    {
        m_views[index] = def.create();
        if (!m_views[index]->Create(nullptr, nullptr, def.style, CRect{}, this,
            static_cast<UINT>(WDS_PANE_ID_BASE + index)))
            return -1;
    }

    m_activePane = DecodeGraphPane(COptions::GraphPaneStyle);
    m_showVisualization = COptions::ShowVisualization;
    ShowVisualization(m_showVisualization);
    return 0;
}

void CVisualizationPane::OnDraw(CDC* pDC)
{
    pDC->FillSolidRect(GetClientRect(), CGraphView::GetBackgroundColor());
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

    active->MoveWindow(GetClientRect(), false);
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

    active->MoveWindow(GetClientRect(), false);
    active->ShowWindow(SW_SHOW);
    active->Invalidate(false);
}

void CVisualizationPane::OnUpdate(CWnd* sender, const MODEL_CHANGE change, CItem* item)
{
    for (const auto& view : m_views)
    {
        if (view != nullptr && view.get() != sender)
            static_cast<CWinDirStatPane*>(view.get())->OnUpdate(sender, change, item);
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

    for (const auto& view : m_views)
    {
        if (view != nullptr) view->SuspendRecalculationDrawing(suspend);
    }
}

void CVisualizationPane::OnSetFocus(WindowRef /*pOldWnd*/)
{
    if (CGraphView* active = GetActiveView(); m_showVisualization && active != nullptr)
        active->SetFocus();
}

void CVisualizationPane::OnSize(UINT /*nType*/, const int cx, const int cy)
{
    if (CGraphView* active = GetActiveView(); active != nullptr && active->Handle())
        active->MoveWindow(0, 0, cx, cy);
}
