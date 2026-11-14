// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "ScanHistory.h"

// Line chart of the size, allocated size and file count of a folder over the
// scans stored in the history file of the current scan origin.
class CHistoryChart final
{
public:
    // paper = light colors for output (clipboard, file, printer); the screen keeps the theme colors.
    void DrawChart(CDC* pdc, CRect rc, const CItem* item, const std::wstring& origin, bool paper = false);

    // Index of the history point drawn near the position of the last screen drawing, or -1.
    std::int64_t FindPointTime(CPoint point) const;

    // Toggles the series whose legend entry is at the position; returns false if none is there.
    bool ToggleLegendAt(CPoint point);

    // Selects the point at the position (or clears the selection if none is there); returns true if it changed.
    bool SelectPointAt(CPoint point);

    // Time of the selected point, or -1 when none is selected.
    std::int64_t GetSelectedTime() const noexcept { return m_selectedTime; }
    void ClearSelection() noexcept { m_selectedTime = -1; }

    // Zooms the time axis by the factor (>1 zooms in) keeping the time under the position fixed.
    bool ZoomAt(CPoint point, double factor);

    // Moves the visible time range by a number of horizontal pixels.
    bool PanBy(int pixels);

    // Shows the whole history again; returns true if the view changed.
    bool ResetView();
    bool IsZoomed() const noexcept { return m_viewEnd > m_viewStart; }

    // Scroll bar shown while zoomed (buttons, track and thumb), drawn by the chart itself.
    enum class BarPart { None, Left, Right, All, Thumb, TrackBefore, TrackAfter };
    BarPart HitBar(CPoint point) const;

    // Moves the visible range by a fraction of its width (negative = earlier).
    bool ScrollBy(double views);

    // Moves the visible range by a thumb movement of the given number of pixels.
    bool ScrollThumb(int pixels);

    // True if the position is inside the plot area of the last screen drawing.
    bool IsInPlot(CPoint point) const noexcept;

    // Tab-separated values of the stored points of the item.
    static std::wstring GetValuesText(const CItem* item, const std::wstring& origin);

    // Draws the chart into a new bitmap of the given size.
    bool RenderToBitmap(const CItem* item, const std::wstring& origin, CSize size, CBitmap& bitmap);

private:
    struct Marker { std::int64_t time; std::array<Gdiplus::PointF, 3> positions; };
    std::vector<Marker> m_markers;
    float m_markerRadius = 0.0f;
    std::int64_t m_selectedTime = -1;
    std::int64_t m_viewStart = 0, m_viewEnd = 0;                // visible time range; empty = everything
    std::int64_t m_dataStart = 0, m_dataEnd = 0;                // time range of the stored points
    float m_plotLeft = 0.0f, m_plotRight = 0.0f, m_plotTop = 0.0f, m_plotBottom = 0.0f, m_plotInset = 0.0f;
    std::int64_t VisibleStart() const noexcept { return IsZoomed() ? m_viewStart : m_dataStart; }
    std::int64_t VisibleEnd() const noexcept { return IsZoomed() ? m_viewEnd : m_dataEnd; }
    std::array<bool, 3> m_visible{ true, true, true };          // size, allocated, files
    std::array<Gdiplus::RectF, 3> m_legendRects{};
    Gdiplus::RectF m_barLeft, m_barRight, m_barAll, m_barTrack, m_barThumb;
};
