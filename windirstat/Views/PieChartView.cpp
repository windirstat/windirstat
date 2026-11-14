// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "PieChartView.h"

void CPieChartView::DrawEmptyPlaceholder(CDC* pDC, const CRect& rect)
{
    const int margin = ScaleForDpi(8);
    const int radiusX = std::max(0, std::min(rect.Width() / 2, rect.Height() * 2 / 3) - margin);
    if (radiusX < ScaleForDpi(8)) return;

    const int radiusY = radiusX * 11 / 20;
    const int depth = radiusX / 10;
    const CPoint center = rect.Center();
    Gdiplus::Graphics graphics(pDC->Handle());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    const std::array shades{
        Gdiplus::Color(255, 58, 61, 68),
        Gdiplus::Color(255, 72, 76, 84),
        Gdiplus::Color(255, 50, 53, 59),
        Gdiplus::Color(255, 84, 88, 97),
    };
    const Gdiplus::SolidBrush wall(Gdiplus::Color(255, 36, 38, 42));
    graphics.FillEllipse(&wall, center.x - radiusX, center.y - radiusY + depth, radiusX * 2, radiusY * 2);
    constexpr float sweep = 360.0f / 4.0f;
    for (const auto [index, shade] : std::views::enumerate(shades))
    {
        const Gdiplus::SolidBrush brush(shade);
        graphics.FillPie(&brush, center.x - radiusX, center.y - radiusY, radiusX * 2, radiusY * 2,
            static_cast<float>(index) * sweep, sweep);
    }
}

void CPieChartView::RenderVisualization(CDC* pDC, const CRect rect)
{
    m_pie.DrawPie(pDC, rect, m_pieRoot != nullptr ? m_pieRoot : CWinDirStatModel::Get()->GetZoomItem());
    // A base render may have rebuilt geometry because of DPI or sizing, even
    // when the view deliberately retained the logical layout cache.
    ClearExtensionHighlightCache();
}

void CPieChartView::DrawHighlightExtension(CDC* pDC)
{
    const CWinDirStatModel* model = CWinDirStatModel::Get();
    const std::wstring extension = model->GetHighlightExtension();
    if (const bool unregistered = model->IsHighlightUnregistered();
        !m_extensionOutlineItemsValid || extension != m_cachedHighlightExtension ||
        unregistered != m_cachedHighlightUnregistered)
    {
        m_extensionOutlineItems.clear();
        m_pie.VisitItems([&](const CItem* item)
        {
            if (IsExtensionHighlighted(item)) m_extensionOutlineItems.push_back(item);
        });
        m_cachedHighlightExtension = extension;
        m_cachedHighlightUnregistered = unregistered;
        m_extensionOutlineItemsValid = true;
    }

    m_pie.DrawOutlineItems(pDC, m_extensionOutlineItems,
        COptions::TreeMapHighlightColor, 2.0f);
}

void CPieChartView::DrawHover(CDC* pDC)
{
    m_pie.DrawHoverItem(pDC, m_hoverItem, m_hoveringRemainder);
}

void CPieChartView::DrawSelection(CDC* pDC)
{
    const auto& items = CWinDirStatModel::Get()->GetAllSelected();
    m_selectionOutlineItems.clear();
    m_selectionOutlineItems.reserve(items.size());
    for (const CItem* item : items)
        m_selectionOutlineItems.push_back(GetDisplayItem(item));
    m_pie.DrawOutlineItems(pDC, m_selectionOutlineItems,
        COptions::TreeMapHighlightColor, items.size() == 1 ? 2.0f : 1.0f);
}

CItem* CPieChartView::FindItemAtPoint(const CPoint point)
{
    return m_pie.FindItemByPoint(point, &m_hitRemainderSize);
}

void CPieChartView::UpdateSliceToolTip(const CItem* item)
{
    std::wstring text = item == nullptr ? std::wstring{} : m_pie.GetSliceText(item, m_hitRemainderSize != 0);
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

bool CPieChartView::UpdateHoverDetails(const CItem* item, const bool itemChanged)
{
    if (item == nullptr) m_hitRemainderSize = 0;
    UpdateSliceToolTip(item);
    if (item == nullptr) m_hitRemainderSize = 0;
    const bool hoveringRemainder = m_hitRemainderSize != 0;
    if (!itemChanged && hoveringRemainder == m_hoveringRemainder) return false;
    m_hoveringRemainder = hoveringRemainder;
    std::wstring path;
    ULONGLONG size = 0;
    if (item != nullptr)
    {
        path = item->GetPath();
        size = item->TmiGetSize();
        if (m_hitRemainderSize != 0)
        {
            if (!path.empty() && path.back() != L'\\') path.push_back(L'\\');
            path.push_back(L'…');
            size = m_hitRemainderSize;
        }
    }
    if (path == m_paneTextOverride && size == m_paneSizeOverride) return false;
    m_paneTextOverride = std::move(path);
    m_paneSizeOverride = size;
    return true;
}

void CPieChartView::ClearVisualizationLayout()
{
    m_pie.ClearLayout();
    m_hitRemainderSize = 0;
    m_hoveringRemainder = false;
    m_selectionOutlineItems.clear();
    ClearExtensionHighlightCache();
}

void CPieChartView::OnRenderCacheTrimmed()
{
    m_pie.TrimMemory();
    decltype(m_extensionOutlineItems){}.swap(m_extensionOutlineItems);
    decltype(m_selectionOutlineItems){}.swap(m_selectionOutlineItems);
    m_cachedHighlightExtension.clear();
    m_cachedHighlightExtension.shrink_to_fit();
    m_extensionOutlineItemsValid = false;
}

void CPieChartView::ClearExtensionHighlightCache()
{
    m_extensionOutlineItems.clear();
    m_cachedHighlightExtension.clear();
    m_cachedHighlightUnregistered = false;
    m_extensionOutlineItemsValid = false;
}

void CPieChartView::OnUpdate(CWnd* sender, const MODEL_CHANGE change, CItem* item)
{
    if (change == MODEL_CHANGE_EXTENSION_SELECTION) ClearExtensionHighlightCache();

    // The pie shows the children of the directory selected in the file tree.
    // Zoom, new scans and refreshes return it to following the zoom item.
    switch (change)
    {
    case MODEL_CHANGE_SELECTION_ACTION:
        if (item != nullptr && !item->TmiIsLeaf() && item->TmiGetSize() > 0 && item != m_pieRoot)
        {
            m_pieRoot = item;
            ClearExtensionHighlightCache();
            CGraphView::OnUpdate(sender, MODEL_CHANGE_NONE, item);
        }
        break;
    case MODEL_CHANGE_SELECTION_REFRESH:
        // Selecting a directory in the file list (e.g. the All Files tab) only
        // raises a refresh without an item, so read the selection directly.
        if (CWinDirStatModel::Get()->IsRootDone())
        {
            const auto selected = CWinDirStatModel::Get()->GetSelectedItemsView();
            CItem* directory = selected.empty() ? nullptr : selected.front();
            if (directory != nullptr && !directory->TmiIsLeaf() && directory->TmiGetSize() > 0 &&
                directory != m_pieRoot)
            {
                m_pieRoot = directory;
                ClearExtensionHighlightCache();
                CGraphView::OnUpdate(sender, MODEL_CHANGE_NONE, directory);
            }
        }
        break;
    case MODEL_CHANGE_NEW_ROOT:
    case MODEL_CHANGE_ZOOM:
    case MODEL_CHANGE_NONE:
    case MODEL_CHANGE_SIZE_MODE:
        m_pieRoot = nullptr;
        break;
    default:
        break;
    }
    CGraphView::OnUpdate(sender, change, item);
}

bool CPieChartView::CanReuseVisualizationLayout(const MODEL_CHANGE change) const
{
    return change == MODEL_CHANGE_TREEMAP_STYLE;
}
