// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "TreeMap.h"

// Tilted 3D pie chart of the zoomed item's largest children. Smaller children
// are merged into an "Others" slice. Every slice has a callout with its size and
// share, and a legend below the chart names each slice.
class CPieChart final
{
public:
    void DrawPie(CDC* pdc, CRect rc, CItem* root);
    void DrawHoverItem(CDC* pdc, const CItem* item, bool remainder) const;
    void DrawOutlineItems(CDC* pdc, std::span<const CItem* const> items,
        COLORREF color, float width = 2.0f) const;

    CItem* FindItemByPoint(CPoint point,
        ULONGLONG* remainderSize = nullptr) const;
    std::wstring GetSliceText(const CItem* item, bool remainder) const;
    void ClearLayout();
    void TrimMemory();

    template<typename Visitor>
        requires std::invocable<Visitor&, const CItem*>
    void VisitItems(Visitor&& visitor) const
    {
        for (const Slice& slice : m_slices)
        {
            if (slice.remainderSize == 0) std::invoke(visitor, slice.item);
        }
    }

private:
    struct Slice
    {
        CItem* item = nullptr;
        ULONGLONG remainderSize = 0;   // non-zero for the merged "Others" slice
        double startAngle = 0.0;       // degrees, clockwise from 3 o'clock
        double sweepAngle = 0.0;
        COLORREF color = 0;
        std::wstring name;
        std::wstring sizeText;         // callout lines: size and share
        std::wstring shareText;
        float sizeWidth = 0.0f;
        float shareWidth = 0.0f;
        bool hasCallout = false;
        Gdiplus::RectF calloutRect;
        Gdiplus::RectF legendRect;
        std::array<Gdiplus::PointF, 3> leader;
    };

    void BuildLayout(Gdiplus::Graphics& graphics, CItem* root, const CRect& rc);
    void Render(Gdiplus::Graphics& graphics) const;
    void AddArcPoints(std::vector<Gdiplus::PointF>& points, double startAngle,
        double endAngle, double yOffset, bool reverse) const;
    void CreateTopPath(const Slice& slice, Gdiplus::GraphicsPath& path) const;
    void DrawOutlineSlice(Gdiplus::Graphics& graphics, const Slice& slice,
        COLORREF color, float width) const;
    const Slice* FindSlice(const CItem* item) const;
    const Slice* FindRemainder() const;

    CFolderColors m_folderColors;
    CItem* m_layoutRoot = nullptr;
    CRect m_renderArea;
    int m_dpiX = USER_DEFAULT_SCREEN_DPI;
    int m_dpiY = USER_DEFAULT_SCREEN_DPI;
    double m_dpiScale = 1.0;
    double m_centerX = 0.0;
    double m_centerY = 0.0;
    double m_radiusX = 0.0;
    double m_radiusY = 0.0;
    double m_depth = 0.0;
    float m_fontSize = 12.0f;
    float m_lineHeight = 0.0f;
    std::wstring m_title;
    std::wstring m_subtitle;
    Gdiplus::PointF m_titlePosition;
    Gdiplus::PointF m_subtitlePosition;
    Gdiplus::RectF m_legendBox;
    std::vector<Slice> m_slices;
};
