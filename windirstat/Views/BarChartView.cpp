// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "BarChartView.h"

void CBarChartView::DrawEmptyPlaceholder(CDC* pDC, const CRect& rect)
{
    const int margin = ScaleForDpi(8);
    const int width = rect.Width() - 2 * margin;
    const int height = rect.Height() - 2 * margin;
    if (width < ScaleForDpi(40) || height < ScaleForDpi(40)) return;

    // Gray placeholder bars, largest on the left.
    constexpr std::array heights{ 100, 78, 62, 45, 32, 20, 12 };
    const int slot = width / static_cast<int>(heights.size());
    for (const auto [index, percent] : std::views::enumerate(heights))
    {
        const int barHeight = height * percent / 100;
        const CRect bar(rect.left + margin + static_cast<int>(index) * slot + slot / 8,
            rect.bottom - margin - barHeight,
            rect.left + margin + static_cast<int>(index + 1) * slot - slot / 8,
            rect.bottom - margin);
        pDC->FillSolidRect(bar, RGB(58 + 8 * static_cast<int>(index % 3), 61, 68));
    }
}

void CBarChartView::RenderVisualization(CDC* pDC, const CRect rect)
{
    m_chart.DrawChart(pDC, rect, m_barRoot != nullptr ? m_barRoot : CWinDirStatModel::Get()->GetZoomItem());
    ClearExtensionHighlightCache();
}

void CBarChartView::DrawHighlightExtension(CDC* pDC)
{
    const CWinDirStatModel* model = CWinDirStatModel::Get();
    const std::wstring extension = model->GetHighlightExtension();
    if (const bool unregistered = model->IsHighlightUnregistered();
        !m_extensionOutlineItemsValid || extension != m_cachedHighlightExtension ||
        unregistered != m_cachedHighlightUnregistered)
    {
        m_extensionOutlineItems.clear();
        m_chart.VisitItems([&](const CItem* item)
        {
            if (IsExtensionHighlighted(item)) m_extensionOutlineItems.push_back(item);
        });
        m_cachedHighlightExtension = extension;
        m_cachedHighlightUnregistered = unregistered;
        m_extensionOutlineItemsValid = true;
    }

    m_chart.DrawOutlineItems(pDC, m_extensionOutlineItems, COptions::TreeMapHighlightColor, 2.0f);
}

void CBarChartView::DrawHover(CDC* pDC)
{
    m_chart.DrawHoverItem(pDC, m_hoverItem);
}

void CBarChartView::DrawSelection(CDC* pDC)
{
    const auto& items = CWinDirStatModel::Get()->GetAllSelected();
    m_selectionOutlineItems.clear();
    m_selectionOutlineItems.reserve(items.size());
    for (const CItem* item : items)
        m_selectionOutlineItems.push_back(GetDisplayItem(item));
    m_chart.DrawOutlineItems(pDC, m_selectionOutlineItems,
        COptions::TreeMapHighlightColor, items.size() == 1 ? 2.0f : 1.0f);
}

CItem* CBarChartView::FindItemAtPoint(const CPoint point)
{
    return m_chart.FindItemByPoint(point);
}

void CBarChartView::UpdateBarToolTip(const CItem* item)
{
    std::wstring text = item == nullptr ? std::wstring{} : m_chart.GetBarText(item);
    if (text.empty())
    {
        if (!m_toolTipText.empty() && IsWindow(m_toolTip.Handle())) m_toolTip.HideTracking(this);
        m_toolTipText.clear();
        return;
    }

    if (!IsWindow(m_toolTip.Handle()))
    {
        if (!m_toolTip.Create(this) || !m_toolTip.AddTrackingTool(this)) return;
        m_toolTip.SetMaxTipWidth(ScaleForDpi(400));
    }
    POINT cursor{};
    GetCursorPos(&cursor);
    m_toolTipText = std::move(text);
    m_toolTip.ShowTracking(this, m_toolTipText, cursor);
}

bool CBarChartView::UpdateHoverDetails(const CItem* item, const bool itemChanged)
{
    UpdateBarToolTip(item);
    if (!itemChanged) return false;

    const std::wstring path = item != nullptr ? item->GetPath() : std::wstring{};
    const ULONGLONG size = item != nullptr ? item->TmiGetSize() : 0;
    if (path == m_paneTextOverride && size == m_paneSizeOverride) return false;
    m_paneTextOverride = path;
    m_paneSizeOverride = size;
    return true;
}

void CBarChartView::ClearVisualizationLayout()
{
    m_chart.ClearLayout();
    m_selectionOutlineItems.clear();
    ClearExtensionHighlightCache();
}

void CBarChartView::OnRenderCacheTrimmed()
{
    m_chart.TrimMemory();
    decltype(m_extensionOutlineItems){}.swap(m_extensionOutlineItems);
    decltype(m_selectionOutlineItems){}.swap(m_selectionOutlineItems);
    m_cachedHighlightExtension.clear();
    m_cachedHighlightExtension.shrink_to_fit();
    m_extensionOutlineItemsValid = false;
}

void CBarChartView::ClearExtensionHighlightCache()
{
    m_extensionOutlineItems.clear();
    m_cachedHighlightExtension.clear();
    m_cachedHighlightUnregistered = false;
    m_extensionOutlineItemsValid = false;
}

void CBarChartView::OnUpdate(CWnd* sender, const MODEL_CHANGE change, CItem* item)
{
    if (change == MODEL_CHANGE_EXTENSION_SELECTION) ClearExtensionHighlightCache();

    // The chart shows the children of the directory selected in the file tree.
    // Zoom, new scans and refreshes return it to following the zoom item.
    switch (change)
    {
    case MODEL_CHANGE_SELECTION_ACTION:
        if (item != nullptr && !item->TmiIsLeaf() && item->TmiGetSize() > 0 && item != m_barRoot)
        {
            m_barRoot = item;
            ClearExtensionHighlightCache();
            CGraphView::OnUpdate(sender, MODEL_CHANGE_NONE, item);
        }
        break;
    case MODEL_CHANGE_NEW_ROOT:
    case MODEL_CHANGE_ZOOM:
    case MODEL_CHANGE_NONE:
    case MODEL_CHANGE_SIZE_MODE:
        m_barRoot = nullptr;
        break;
    default:
        break;
    }
    CGraphView::OnUpdate(sender, change, item);
}

bool CBarChartView::CanReuseVisualizationLayout(const MODEL_CHANGE change) const
{
    return change == MODEL_CHANGE_TREEMAP_STYLE;
}
