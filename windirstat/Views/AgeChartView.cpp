// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "AgeChartView.h"

void CAgeChartView::DrawEmptyPlaceholder(CDC* pDC, const CRect& rect)
{
    const int margin = ScaleForDpi(8);
    const int width = rect.Width() - 2 * margin;
    const int height = rect.Height() - 2 * margin;
    if (width < ScaleForDpi(40) || height < ScaleForDpi(40)) return;

    // Gray placeholder bars of increasing height.
    constexpr std::array heights{ 50, 70, 100, 70, 60, 20, 30 };
    const int slot = width / static_cast<int>(heights.size());
    for (const auto [index, percent] : std::views::enumerate(heights))
    {
        const int barHeight = height * percent / 130;
        const CRect bar(rect.left + margin + static_cast<int>(index) * slot + slot / 8,
            rect.bottom - margin - barHeight,
            rect.left + margin + static_cast<int>(index + 1) * slot - slot / 8,
            rect.bottom - margin);
        pDC->FillSolidRect(bar, RGB(58 + 8 * static_cast<int>(index % 3), 61, 68));
    }
}

// The folder whose files are analyzed: the selected folder (the parent for a
// selected file) or the zoom item when nothing is selected.
CItem* CAgeChartView::GetTargetItem() const
{
    CWinDirStatModel* model = CWinDirStatModel::Get();
    if (const auto selected = model->GetSelectedItemsView(); !selected.empty())
    {
        CItem* item = selected.front();
        if (item->IsLeaf() && item->GetParent() != nullptr) item = item->GetParent();
        return item;
    }
    return model->GetZoomItem();
}

void CAgeChartView::RenderVisualization(CDC* pDC, const CRect rect)
{
    m_shownItem = GetTargetItem();
    m_chart.DrawChart(pDC, rect, m_shownItem);
}

void CAgeChartView::OnUpdate(CWnd* sender, const MODEL_CHANGE change, CItem* item)
{
    CGraphView::OnUpdate(sender, change, item);

    // The base class only refreshes overlays on selection changes; this chart
    // has to be rebuilt as soon as the selection moves to another folder.
    switch (change)
    {
    case MODEL_CHANGE_SELECTION_ACTION:
    case MODEL_CHANGE_SELECTION_REFRESH:
    case MODEL_CHANGE_SELECTION_STYLE:
        if (IsWindowVisible() && CWinDirStatModel::Get()->IsRootDone()
            && m_shownItem != nullptr && GetTargetItem() != m_shownItem)
        {
            Inactivate();
            Invalidate(false);
        }
        break;
    default:
        break;
    }
}
