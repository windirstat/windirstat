// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "TreeMap.h"

// Vertical bar chart of the largest children of the zoomed item. Bars are
// ordered from largest (left) to smallest (right). Every bar shows its size
// above it and its name below it. In a tall, narrow area the bars are horizontal
// instead, from largest (top) to smallest (bottom), with the name on the left
// and the size at the end of the bar.
class CBarChart final
{
public:
    static constexpr std::size_t MaxBars = 12;

    void DrawChart(CDC* pdc, CRect rc, CItem* root);
    void DrawHoverItem(CDC* pdc, const CItem* item) const;
    void DrawOutlineItems(CDC* pdc, std::span<const CItem* const> items,
        COLORREF color, float width = 2.0f) const;

    CItem* FindItemByPoint(CPoint point) const;
    std::wstring GetBarText(const CItem* item) const;
    void ClearLayout();
    void TrimMemory();

    template<typename Visitor>
        requires std::invocable<Visitor&, const CItem*>
    void VisitItems(Visitor&& visitor) const
    {
        for (const Bar& bar : m_bars) std::invoke(visitor, bar.item);
    }

private:
    struct Bar
    {
        CItem* item = nullptr;
        COLORREF color = 0;
        std::wstring name;
        std::wstring sizeText;
        std::wstring shareText;
        std::wstring valueText;       // size and share on one line (horizontal bars)
        Gdiplus::RectF rect;          // the bar itself
        Gdiplus::RectF slot;          // bar plus its labels, used for hit testing
    };

    void BuildLayout(Gdiplus::Graphics& graphics, CItem* root, const CRect& rc);
    void Render(Gdiplus::Graphics& graphics) const;
    const Bar* FindBar(const CItem* item) const;
    void DrawOutlineBar(Gdiplus::Graphics& graphics, const Bar& bar, COLORREF color, float width) const;

    CFolderColors m_folderColors;
    CItem* m_layoutRoot = nullptr;
    CRect m_renderArea;
    int m_dpiX = USER_DEFAULT_SCREEN_DPI;
    int m_dpiY = USER_DEFAULT_SCREEN_DPI;
    double m_dpiScale = 1.0;
    float m_fontSize = 12.0f;
    float m_lineHeight = 0.0f;
    float m_baseline = 0.0f;          // y coordinate of the bottom of the bars (vertical bars)
    bool m_horizontal = false;        // tall, narrow areas get horizontal bars
    float m_plotLeft = 0.0f;          // x coordinate where horizontal bars start
    float m_labelLeft = 0.0f;         // names of horizontal bars are drawn in [m_labelLeft, m_plotLeft)
    std::wstring m_title;
    std::wstring m_subtitle;
    Gdiplus::PointF m_titlePosition;
    Gdiplus::PointF m_subtitlePosition;
    std::vector<Bar> m_bars;
};
