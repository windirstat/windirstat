// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "HistoryChart.h"

namespace
{
    constexpr COLORREF SIZE_COLOR = RGB(66, 139, 240);
    constexpr COLORREF ALLOCATED_COLOR = RGB(240, 170, 60);
    constexpr COLORREF FILES_COLOR = RGB(90, 180, 110);
    constexpr int Y_INTERVALS = 4;
    constexpr std::size_t MAX_X_TICKS = 6;
    constexpr std::size_t MAX_LABELLED_POINTS = 8;

    Gdiplus::Color ToGdiColor(const COLORREF color) noexcept
    {
        return Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color));
    }

    const std::wstring& GetUiFontFamilyName()
    {
        static const std::wstring fontFamilyName = []
        {
            LOGFONTW logFont{};
            const HFONT stockFont = GetAppFont();
            if (stockFont != nullptr
                && GetObjectW(stockFont, sizeof(logFont), &logFont) == sizeof(logFont)
                && logFont.lfFaceName[0] != L'\0')
            {
                return std::wstring(logFont.lfFaceName);
            }
            return std::wstring(L"Segoe UI");
        }();
        return fontFamilyName;
    }

    std::wstring GetDisplayName(const CItem* item)
    {
        std::wstring_view name = item->GetNameView(true);
        if (item->IsTypeOrFlag(IT_DRIVE) && name.ends_with(L"%)"))
        {
            if (const auto separator = name.rfind(L" - "); separator != std::wstring_view::npos)
                name = name.substr(0, separator);
        }
        return std::wstring(name);
    }

    std::wstring FormatTime(const std::int64_t seconds, const bool withTime)
    {
        FILETIME fileTime = std::bit_cast<FILETIME>((static_cast<ULONGLONG>(seconds) + 11644473600ULL) * 10'000'000ULL);
        FILETIME localTime{};
        SYSTEMTIME st{};
        if (!FileTimeToLocalFileTime(&fileTime, &localTime) || !FileTimeToSystemTime(&localTime, &st)) return {};

        wchar_t buffer[64]{};
        std::wstring text;
        if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &st, nullptr, buffer,
            static_cast<int>(std::size(buffer)), nullptr) > 0) text = buffer;
        if (withTime && GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &st, nullptr, buffer,
            static_cast<int>(std::size(buffer))) > 0) text += L" " + std::wstring(buffer);
        return text;
    }

    struct Axis
    {
        double min = 0;
        double max = 1;
    };

    // Range with some air around the data so lines do not touch the plot border.
    Axis MakeAxis(const double low, const double high)
    {
        double span = high - low;
        if (span <= 0) span = std::max(1.0, std::abs(high) * 0.2);
        const double pad = span * 0.15;
        return { std::max(0.0, low - pad), high + pad + (high == low ? span * 0.5 : 0.0) };
    }
}

void CHistoryChart::DrawChart(CDC* pdc, const CRect rc, const CItem* item, const std::wstring& origin, const bool paper)
{
    m_markers.clear();
    m_legendRects = {};
    m_barLeft = m_barRight = m_barAll = m_barTrack = m_barThumb = {};
    if (pdc == nullptr) return;

    const COLORREF windowColor = paper ? RGB(255, 255, 255) : DarkMode::SystemColor(COLOR_WINDOW);
    const COLORREF textColor = paper ? RGB(0, 0, 0) : DarkMode::SystemColor(COLOR_WINDOWTEXT);
    pdc->FillSolidRect(rc, windowColor);
    if (item == nullptr || rc.Width() <= 0 || rc.Height() <= 0) return;

    const int dpiX = std::max(1, pdc->GetDeviceCaps(LOGPIXELSX));
    const int dpiY = std::max(1, pdc->GetDeviceCaps(LOGPIXELSY));
    const double scale = (static_cast<double>(dpiX) + dpiY) * GetFontSizePercent()
        / (2.0 * USER_DEFAULT_SCREEN_DPI * 100.0);

    Gdiplus::Graphics graphics(pdc->Handle());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

    const Gdiplus::FontFamily uiFamily(GetUiFontFamilyName().c_str());
    const Gdiplus::FontFamily* family = uiFamily.GetLastStatus() == Gdiplus::Ok
        ? &uiFamily : Gdiplus::FontFamily::GenericSansSerif();
    const auto fontSize = static_cast<float>(12.0 * scale);
    const Gdiplus::Font font(family, fontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    const Gdiplus::Font boldFont(family, fontSize, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    const Gdiplus::StringFormat typographic(Gdiplus::StringFormat::GenericTypographic());
    Gdiplus::StringFormat centered(Gdiplus::StringFormat::GenericTypographic());
    centered.SetAlignment(Gdiplus::StringAlignmentCenter);
    const Gdiplus::SolidBrush textBrush(ToGdiColor(textColor));
    const auto measure = [&](const std::wstring& text)
    {
        Gdiplus::RectF box;
        graphics.MeasureString(text.c_str(), static_cast<INT>(text.size()), &font,
            Gdiplus::PointF{}, &typographic, &box);
        return box.Width;
    };
    const auto drawCentered = [&](const std::wstring& text, const Gdiplus::Font& textFont,
        const float centerX, const float y)
    {
        graphics.DrawString(text.c_str(), static_cast<INT>(text.size()), &textFont,
            Gdiplus::PointF{ centerX, y }, &centered, &textBrush);
    };

    const auto margin = static_cast<float>(8.0 * scale);
    const auto pad = static_cast<float>(3.0 * scale);
    const float lineHeight = font.GetHeight(&graphics);
    const auto centerX = static_cast<float>(rc.left) + static_cast<float>(rc.Width()) / 2.0f;

    // Title and subtitle.
    const auto titleY = static_cast<float>(rc.top) + margin;
    drawCentered(GetDisplayName(item), boldFont, centerX, titleY);
    drawCentered(Localization::Lookup(IDS_HISTORYCHART_SUBTITLE), font, centerX, titleY + lineHeight * 1.1f);
    const float topLimit = titleY + lineHeight * 2.1f + margin;

    const std::vector<HistoryPoint>* history = CScanHistory::Find(origin, item->GetPath());
    if (history == nullptr || history->empty())
    {
        drawCentered(Localization::Lookup(IDS_HISTORYCHART_NODATA), font, centerX,
            topLimit + static_cast<float>(rc.Height() - topLimit) / 3.0f);
        return;
    }
    const std::vector<HistoryPoint>& points = *history;

    // Visible time range: everything, or the zoomed part of it (reset if it no longer fits the data).
    m_dataStart = points.front().time;
    m_dataEnd = points.back().time;
    if (m_viewStart >= m_dataEnd || m_viewEnd <= m_dataStart) m_viewStart = m_viewEnd = 0;
    const std::int64_t t0 = VisibleStart();
    const std::int64_t span = VisibleEnd() - t0;
    const auto inView = [&](const HistoryPoint& p) { return p.time >= t0 && p.time <= t0 + span; };

    // Value ranges. Sizes share one axis; the file count has its own on the right.
    // Zooming only affects the time axis, so they always cover the whole history.
    ULONGLONG sizeLow = ULLONG_MAX, sizeHigh = 0, filesLow = ULLONG_MAX, filesHigh = 0;
    for (const HistoryPoint& p : points)
    {
        if (m_visible[0] || !m_visible[1])
        {
            sizeLow = std::min(sizeLow, p.size);
            sizeHigh = std::max(sizeHigh, p.size);
        }
        if (m_visible[1] || !m_visible[0])
        {
            sizeLow = std::min(sizeLow, p.allocated);
            sizeHigh = std::max(sizeHigh, p.allocated);
        }
        filesLow = std::min(filesLow, p.files);
        filesHigh = std::max(filesHigh, p.files);
    }
    const Axis sizeAxis = MakeAxis(static_cast<double>(sizeLow), static_cast<double>(sizeHigh));
    const Axis filesAxis = MakeAxis(static_cast<double>(filesLow), static_cast<double>(filesHigh));
    const auto sizeTick = [&](const int i)
    {
        return static_cast<ULONGLONG>(sizeAxis.min + (sizeAxis.max - sizeAxis.min) * i / Y_INTERVALS);
    };
    const auto filesTick = [&](const int i)
    {
        return static_cast<ULONGLONG>(filesAxis.min + (filesAxis.max - filesAxis.min) * i / Y_INTERVALS);
    };

    float leftLabelWidth = 0.0f, rightLabelWidth = 0.0f;
    for (int i = 0; i <= Y_INTERVALS; ++i)
    {
        leftLabelWidth = std::max(leftLabelWidth, measure(FormatBytes(sizeTick(i))));
        rightLabelWidth = std::max(rightLabelWidth, measure(FormatCount(filesTick(i))));
    }

    // Legend at the bottom, date labels above it.
    const std::array<std::pair<std::wstring, COLORREF>, 3> legend{ {
        { Localization::Lookup(IDS_HISTORYCHART_SIZE), SIZE_COLOR },
        { Localization::Lookup(IDS_HISTORYCHART_ALLOCATED), ALLOCATED_COLOR },
        { Localization::Lookup(IDS_HISTORYCHART_FILES), FILES_COLOR } } };
    const float swatch = 22.0f * static_cast<float>(scale);
    const float legendY = static_cast<float>(rc.bottom) - margin - lineHeight;

    const float plotLeft = static_cast<float>(rc.left) + margin + leftLabelWidth + margin;
    const float plotRight = static_cast<float>(rc.right) - margin - rightLabelWidth - margin;
    const float plotTop = topLimit + lineHeight * 1.5f;
    // A scroll bar row appears under the date labels while zoomed.
    const float barHeight = lineHeight * 1.1f;
    const float plotBottom = legendY - margin - lineHeight - pad - (IsZoomed() ? barHeight + 2.0f * pad : 0.0f);
    if (plotRight - plotLeft < 40.0f * static_cast<float>(scale)
        || plotBottom - plotTop < 40.0f * static_cast<float>(scale)) return;

    float legendWidth = 0.0f;
    const float box = lineHeight * 0.75f;
    for (const auto& entry : legend) legendWidth += box + pad + swatch + pad + measure(entry.first) + 2.0f * margin;
    float legendX = centerX - (legendWidth - 2.0f * margin) / 2.0f;
    for (std::size_t index = 0; index < legend.size(); ++index)
    {
        const auto& [name, color] = legend[index];
        const float entryX = legendX;
        const float boxY = legendY + (lineHeight - box) / 2.0f;
        const Gdiplus::Pen boxPen(ToGdiColor(textColor), 1.0f);
        graphics.DrawRectangle(&boxPen, legendX, boxY, box, box);
        if (m_visible[index])
        {
            Gdiplus::Pen tick(ToGdiColor(textColor), 1.5f * static_cast<float>(scale));
            const Gdiplus::PointF mark[3]{ { legendX + box * 0.2f, boxY + box * 0.55f },
                { legendX + box * 0.43f, boxY + box * 0.78f }, { legendX + box * 0.82f, boxY + box * 0.22f } };
            graphics.DrawLines(&tick, mark, 3);
        }
        legendX += box + pad;
        const Gdiplus::Pen pen(ToGdiColor(color), 2.0f);
        graphics.DrawLine(&pen, legendX, legendY + lineHeight / 2.0f, legendX + swatch, legendY + lineHeight / 2.0f);
        const Gdiplus::SolidBrush dot(ToGdiColor(color));
        const float radius = 3.5f * static_cast<float>(scale);
        graphics.FillEllipse(&dot, legendX + swatch / 2.0f - radius, legendY + lineHeight / 2.0f - radius,
            2.0f * radius, 2.0f * radius);
        graphics.DrawString(name.c_str(), static_cast<INT>(name.size()), &font,
            Gdiplus::PointF{ legendX + swatch + pad, legendY }, &typographic, &textBrush);
        legendX += swatch + pad + measure(name);
        m_legendRects[index] = Gdiplus::RectF(entryX, legendY, legendX - entryX, lineHeight);
        legendX += 2.0f * margin;
    }

    // Time axis: a single point is centered, otherwise the visible range spans the plot.
    const float inset = 12.0f * static_cast<float>(scale);
    m_plotLeft = plotLeft, m_plotRight = plotRight, m_plotTop = plotTop, m_plotBottom = plotBottom, m_plotInset = inset;
    const auto xOf = [&](const std::int64_t time)
    {
        if (span <= 0) return (plotLeft + plotRight) / 2.0f;
        return plotLeft + inset + (plotRight - plotLeft - 2.0f * inset)
            * static_cast<float>(static_cast<double>(time - t0) / static_cast<double>(span));
    };
    const auto yOf = [&](const double value, const Axis& axis)
    {
        return plotBottom - (plotBottom - plotTop) * static_cast<float>((value - axis.min) / (axis.max - axis.min));
    };

    // Horizontal gridlines with both axes' labels.
    const Gdiplus::Pen gridPen(ToGdiColor(CColorSpace::BlendColor(windowColor, textColor, 0.35)), 1.0f);
    for (int i = 0; i <= Y_INTERVALS; ++i)
    {
        const float y = std::round(plotBottom - (plotBottom - plotTop) * static_cast<float>(i) / Y_INTERVALS);
        graphics.DrawLine(&gridPen, plotLeft, y, plotRight, y);
        const std::wstring left = FormatBytes(sizeTick(i));
        graphics.DrawString(left.c_str(), static_cast<INT>(left.size()), &font,
            Gdiplus::PointF{ plotLeft - pad - measure(left), y - lineHeight / 2.0f }, &typographic, &textBrush);
        const std::wstring right = FormatCount(filesTick(i));
        graphics.DrawString(right.c_str(), static_cast<INT>(right.size()), &font,
            Gdiplus::PointF{ plotRight + pad, y - lineHeight / 2.0f }, &typographic, &textBrush);
    }

    // Vertical gridlines with dates.
    const bool withTime = span < 2 * 24 * 3600;
    const std::size_t ticks = span <= 0 ? 1
        : IsZoomed() ? MAX_X_TICKS : std::min<std::size_t>(MAX_X_TICKS, points.size());
    for (std::size_t i = 0; i < ticks; ++i)
    {
        const std::int64_t time = ticks == 1 ? t0
            : t0 + span * static_cast<std::int64_t>(i) / static_cast<std::int64_t>(ticks - 1);
        const float x = std::round(xOf(time));
        graphics.DrawLine(&gridPen, x, plotTop, x, plotBottom);
        drawCentered(FormatTime(time, withTime), font, x, plotBottom + pad);
    }

    const Gdiplus::Pen axisPen(ToGdiColor(textColor), 1.0f);
    graphics.DrawLine(&axisPen, plotLeft, plotBottom, plotRight, plotBottom);
    graphics.DrawLine(&axisPen, plotLeft, plotTop, plotLeft, plotBottom);

    if (IsZoomed())
    {
        // [<] [track with thumb] [>] [show all]
        const float barTop = plotBottom + pad + lineHeight + pad;
        const float b = barHeight;
        m_barLeft = Gdiplus::RectF(plotLeft, barTop, b, b);
        m_barAll = Gdiplus::RectF(plotRight - b, barTop, b, b);
        m_barRight = Gdiplus::RectF(m_barAll.X - pad - b, barTop, b, b);
        m_barTrack = Gdiplus::RectF(m_barLeft.GetRight() + pad, barTop,
            m_barRight.X - pad - (m_barLeft.GetRight() + pad), b);

        const Gdiplus::SolidBrush trackBrush(ToGdiColor(CColorSpace::BlendColor(windowColor, textColor, 0.12)));
        const Gdiplus::SolidBrush thumbBrush(ToGdiColor(CColorSpace::BlendColor(windowColor, textColor, 0.45)));
        const Gdiplus::SolidBrush glyphBrush(ToGdiColor(textColor));
        const Gdiplus::Pen buttonPen(ToGdiColor(CColorSpace::BlendColor(windowColor, textColor, 0.45)), 1.0f);
        graphics.FillRectangle(&trackBrush, m_barTrack);

        const double full = static_cast<double>(m_dataEnd - m_dataStart);
        const float minThumb = std::min(m_barTrack.Width, 16.0f * static_cast<float>(scale));
        const float thumbWidth = std::max(minThumb, m_barTrack.Width
            * static_cast<float>(static_cast<double>(span) / full));
        const float thumbX = m_barTrack.X + (m_barTrack.Width - thumbWidth)
            * static_cast<float>(static_cast<double>(t0 - m_dataStart) / std::max(1.0, full - static_cast<double>(span)));
        m_barThumb = Gdiplus::RectF(thumbX, barTop, thumbWidth, b);
        graphics.FillRectangle(&thumbBrush, m_barThumb);

        const float g = b * 0.22f;     // glyph half-size
        const auto button = [&](const Gdiplus::RectF& r)
        {
            graphics.DrawRectangle(&buttonPen, r);
            return Gdiplus::PointF{ r.X + r.Width / 2.0f, r.Y + r.Height / 2.0f };
        };
        const auto triangle = [&](const Gdiplus::PointF c, const float dir)
        {
            const Gdiplus::PointF tri[3]{ { c.X + dir * g, c.Y - g * 1.4f }, { c.X + dir * g, c.Y + g * 1.4f },
                { c.X - dir * g, c.Y } };
            graphics.FillPolygon(&glyphBrush, tri, 3);
        };
        triangle(button(m_barLeft), 1.0f);       // points left
        triangle(button(m_barRight), -1.0f);     // points right
        const Gdiplus::PointF c = button(m_barAll);   // outward arrows: whole range
        triangle({ c.X - g * 1.1f, c.Y }, 1.0f);
        triangle({ c.X + g * 1.1f, c.Y }, -1.0f);
    }

    // Series.
    const auto drawSeries = [&](const std::size_t index, const COLORREF color, const Axis& axis, const bool dashed,
        const auto& valueOf)
    {
        Gdiplus::Pen pen(ToGdiColor(color), 2.5f * static_cast<float>(scale));
        if (dashed) pen.SetDashStyle(Gdiplus::DashStyleDash);
        std::vector<Gdiplus::PointF> line;
        line.reserve(points.size());
        for (const HistoryPoint& p : points) line.emplace_back(xOf(p.time), yOf(valueOf(p), axis));
        if (!m_visible[index]) return line;
        if (line.size() > 1) graphics.DrawLines(&pen, line.data(), static_cast<INT>(line.size()));

        const Gdiplus::SolidBrush fill(ToGdiColor(CColorSpace::BlendColor(color, windowColor, 0.45)));
        const Gdiplus::Pen outline(ToGdiColor(color), 1.5f);
        const float radius = 4.5f * static_cast<float>(scale);
        for (const Gdiplus::PointF& pt : line)
        {
            graphics.FillEllipse(&fill, pt.X - radius, pt.Y - radius, 2.0f * radius, 2.0f * radius);
            graphics.DrawEllipse(&outline, pt.X - radius, pt.Y - radius, 2.0f * radius, 2.0f * radius);
        }
        return line;
    };
    graphics.SetClip(Gdiplus::RectF(plotLeft, plotTop - 1.0f, plotRight - plotLeft, plotBottom - plotTop + 2.0f));
    const auto filesLine = drawSeries(2, FILES_COLOR, filesAxis, true,
        [](const HistoryPoint& p) { return static_cast<double>(p.files); });
    const auto allocatedLine = drawSeries(1, ALLOCATED_COLOR, sizeAxis, false,
        [](const HistoryPoint& p) { return static_cast<double>(p.allocated); });
    const auto sizeLine = drawSeries(0, SIZE_COLOR, sizeAxis, false,
        [](const HistoryPoint& p) { return static_cast<double>(p.size); });

    graphics.ResetClip();

    m_markerRadius = 6.0f * static_cast<float>(scale);
    const Gdiplus::PointF hidden{ -1.0e6f, -1.0e6f };
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        if (!inView(points[i])) continue;
        m_markers.push_back({ points[i].time, { m_visible[0] ? sizeLine[i] : hidden,
            m_visible[1] ? allocatedLine[i] : hidden, m_visible[2] ? filesLine[i] : hidden } });
    }

    // Value callouts for short histories: the larger series above its points, the smaller below.
    const Gdiplus::SolidBrush calloutBrush(ToGdiColor(windowColor));
    const Gdiplus::Pen calloutPen(ToGdiColor(CColorSpace::BlendColor(windowColor, textColor, 0.35)), 1.0f);
    const auto callout = [&](const std::wstring& text, const Gdiplus::PointF& pt, const bool above)
    {
        const float width = measure(text) + 2.0f * pad;
        const float height = lineHeight + 2.0f * pad;
        const float gap = 8.0f * static_cast<float>(scale);
        const float x = std::clamp(pt.X - width / 2.0f, plotLeft, std::max(plotLeft, plotRight - width));
        const float y = above ? pt.Y - gap - height : pt.Y + gap;
        graphics.FillRectangle(&calloutBrush, x, y, width, height);
        graphics.DrawRectangle(&calloutPen, x, y, width, height);
        drawCentered(text, font, x + width / 2.0f, y + pad);
    };
    if (std::ranges::count_if(points, inView) <= static_cast<std::ptrdiff_t>(MAX_LABELLED_POINTS))
    {
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            if (!inView(points[i])) continue;
            const bool sizeOnTop = points[i].size >= points[i].allocated;
            const bool bothShown = m_visible[0] && m_visible[1];
            if (m_visible[0]) callout(FormatBytes(points[i].size), sizeLine[i], bothShown ? sizeOnTop : true);
            if (m_visible[1]) callout(FormatBytes(points[i].allocated), allocatedLine[i], bothShown ? !sizeOnTop : true);
        }
    }

    // Selected point: highlighted column plus a box with the exact date and the value of every visible series.
    if (m_selectedTime < 0) return;
    const auto selected = std::ranges::find(points, m_selectedTime, &HistoryPoint::time);
    if (selected == points.end() || !inView(*selected)) return;
    const float selectedX = xOf(selected->time);
    const Gdiplus::Pen selectionPen(ToGdiColor(textColor), 1.5f);
    graphics.DrawLine(&selectionPen, selectedX, plotTop, selectedX, plotBottom);

    const std::array<std::wstring, 3> values{ FormatBytes(selected->size), FormatBytes(selected->allocated),
        FormatCount(selected->files) };
    const std::array<const Gdiplus::PointF*, 3> lines{ &sizeLine[selected - points.begin()],
        &allocatedLine[selected - points.begin()], &filesLine[selected - points.begin()] };
    std::vector<std::pair<std::wstring, COLORREF>> rows;
    for (std::size_t s = 0; s < 3; ++s)
    {
        if (!m_visible[s]) continue;
        rows.emplace_back(std::format(L"{}: {}", legend[s].first, values[s]), legend[s].second);
        const Gdiplus::Pen ring(ToGdiColor(textColor), 2.0f);
        const float r = m_markerRadius;
        graphics.DrawEllipse(&ring, lines[s]->X - r, lines[s]->Y - r, 2.0f * r, 2.0f * r);
    }

    const std::wstring dateText = FormatTime(selected->time, true);
    float boxWidth = measure(dateText);
    for (const auto& row : rows) boxWidth = std::max(boxWidth, swatch / 2.0f + pad + measure(row.first));
    boxWidth += 2.0f * margin;
    const float boxHeight = lineHeight * static_cast<float>(rows.size() + 1) + 2.0f * pad + margin;
    const float gap = 10.0f * static_cast<float>(scale);
    const float boxX = selectedX + gap + boxWidth <= plotRight ? selectedX + gap
        : std::max(plotLeft, selectedX - gap - boxWidth);
    const float boxY = plotTop + gap;
    graphics.FillRectangle(&calloutBrush, boxX, boxY, boxWidth, boxHeight);
    graphics.DrawRectangle(&selectionPen, boxX, boxY, boxWidth, boxHeight);
    graphics.DrawString(dateText.c_str(), static_cast<INT>(dateText.size()), &boldFont,
        Gdiplus::PointF{ boxX + margin, boxY + pad }, &typographic, &textBrush);
    float rowY = boxY + pad + lineHeight + margin / 2.0f;
    for (const auto& [text, color] : rows)
    {
        const Gdiplus::SolidBrush dot(ToGdiColor(color));
        const float dotSize = lineHeight * 0.5f;
        graphics.FillEllipse(&dot, boxX + margin, rowY + (lineHeight - dotSize) / 2.0f, dotSize, dotSize);
        graphics.DrawString(text.c_str(), static_cast<INT>(text.size()), &font,
            Gdiplus::PointF{ boxX + margin + dotSize + pad, rowY }, &typographic, &textBrush);
        rowY += lineHeight;
    }
}

bool CHistoryChart::SelectPointAt(const CPoint point)
{
    const std::int64_t time = FindPointTime(point);
    const bool changed = time != m_selectedTime;
    m_selectedTime = time;
    return changed;
}

std::int64_t CHistoryChart::FindPointTime(const CPoint point) const
{
    std::int64_t best = -1;
    float bestDistance = m_markerRadius * m_markerRadius;
    for (const Marker& marker : m_markers)
    {
        for (const Gdiplus::PointF& pos : marker.positions)
        {
            const float dx = pos.X - static_cast<float>(point.x), dy = pos.Y - static_cast<float>(point.y);
            if (const float distance = dx * dx + dy * dy; distance <= bestDistance)
            {
                bestDistance = distance;
                best = marker.time;
            }
        }
    }
    return best;
}

std::wstring CHistoryChart::GetValuesText(const CItem* item, const std::wstring& origin)
{
    std::wstring text = std::format(L"{}\t{}\t{}\t{}\r\n", Localization::Lookup(IDS_HISTORYCHART_DATE),
        Localization::Lookup(IDS_HISTORYCHART_SIZE), Localization::Lookup(IDS_HISTORYCHART_ALLOCATED),
        Localization::Lookup(IDS_HISTORYCHART_FILES));
    if (item == nullptr) return text;
    if (const auto* points = CScanHistory::Find(origin, item->GetPath()); points != nullptr)
        for (const HistoryPoint& p : *points)
            text += std::format(L"{}\t{}\t{}\t{}\r\n", FormatTime(p.time, true), p.size, p.allocated, p.files);
    return text;
}

bool CHistoryChart::RenderToBitmap(const CItem* item, const std::wstring& origin, const CSize size, CBitmap& bitmap)
{
    const HDC screenDc = ::GetDC(nullptr);
    CDC screen = CDC::Borrow(screenDc);
    CDC memory(&screen);
    const bool ok = memory && bitmap.CreateCompatible(&screen, size.cx, size.cy);
    if (ok)
    {
        GdiObjectSelection select(memory.Handle(), &bitmap);
        DrawChart(&memory, CRect(0, 0, size.cx, size.cy), item, origin, true);
    }
    ::ReleaseDC(nullptr, screenDc);
    return ok;
}

bool CHistoryChart::IsInPlot(const CPoint point) const noexcept
{
    return m_plotRight > m_plotLeft && static_cast<float>(point.x) >= m_plotLeft
        && static_cast<float>(point.x) <= m_plotRight && static_cast<float>(point.y) >= m_plotTop
        && static_cast<float>(point.y) <= m_plotBottom;
}

bool CHistoryChart::ZoomAt(const CPoint point, const double factor)
{
    const double width = static_cast<double>(m_plotRight - m_plotLeft - 2.0f * m_plotInset);
    if (m_dataEnd <= m_dataStart || width <= 0.0 || !IsInPlot(point)) return false;

    const double start = static_cast<double>(VisibleStart());
    const double span = static_cast<double>(VisibleEnd()) - start;
    const double fraction = std::clamp((static_cast<double>(point.x) - m_plotLeft - m_plotInset) / width, 0.0, 1.0);
    const double anchor = start + span * fraction;

    const double fullSpan = static_cast<double>(m_dataEnd - m_dataStart);
    const double newSpan = std::min(span / factor, fullSpan);
    if (newSpan >= fullSpan) return ResetView();
    if (newSpan < 60.0 && factor > 1.0) return false;      // one minute is the finest view

    const double newStart = std::clamp(anchor - newSpan * fraction, static_cast<double>(m_dataStart),
        static_cast<double>(m_dataEnd) - newSpan);
    m_viewStart = static_cast<std::int64_t>(std::llround(newStart));
    m_viewEnd = m_viewStart + static_cast<std::int64_t>(std::llround(newSpan));
    return true;
}

bool CHistoryChart::PanBy(const int pixels)
{
    const double width = static_cast<double>(m_plotRight - m_plotLeft - 2.0f * m_plotInset);
    if (!IsZoomed() || width <= 0.0 || pixels == 0) return false;

    const std::int64_t span = m_viewEnd - m_viewStart;
    const auto shift = static_cast<std::int64_t>(std::llround(-static_cast<double>(pixels) * static_cast<double>(span) / width));
    const std::int64_t start = std::clamp(m_viewStart + shift, m_dataStart, m_dataEnd - span);
    if (start == m_viewStart) return false;
    m_viewStart = start;
    m_viewEnd = start + span;
    return true;
}

CHistoryChart::BarPart CHistoryChart::HitBar(const CPoint point) const
{
    if (!IsZoomed()) return BarPart::None;
    const auto x = static_cast<float>(point.x), y = static_cast<float>(point.y);
    if (m_barLeft.Contains(x, y)) return BarPart::Left;
    if (m_barRight.Contains(x, y)) return BarPart::Right;
    if (m_barAll.Contains(x, y)) return BarPart::All;
    if (m_barThumb.Contains(x, y)) return BarPart::Thumb;
    if (m_barTrack.Contains(x, y)) return x < m_barThumb.X ? BarPart::TrackBefore : BarPart::TrackAfter;
    return BarPart::None;
}

bool CHistoryChart::ScrollBy(const double views)
{
    if (!IsZoomed()) return false;
    const std::int64_t span = m_viewEnd - m_viewStart;
    const std::int64_t start = std::clamp(m_viewStart + static_cast<std::int64_t>(std::llround(views * static_cast<double>(span))),
        m_dataStart, m_dataEnd - span);
    if (start == m_viewStart) return false;
    m_viewStart = start;
    m_viewEnd = start + span;
    return true;
}

bool CHistoryChart::ScrollThumb(const int pixels)
{
    const float movable = m_barTrack.Width - m_barThumb.Width;
    if (!IsZoomed() || movable <= 0.0f || pixels == 0) return false;
    const double full = static_cast<double>(m_dataEnd - m_dataStart);
    const double span = static_cast<double>(m_viewEnd - m_viewStart);
    return ScrollBy(static_cast<double>(pixels) / movable * (full - span) / span);
}

bool CHistoryChart::ResetView()
{
    const bool changed = IsZoomed();
    m_viewStart = m_viewEnd = 0;
    return changed;
}

bool CHistoryChart::ToggleLegendAt(const CPoint point)
{
    for (std::size_t i = 0; i < m_legendRects.size(); ++i)
    {
        if (!m_legendRects[i].Contains(static_cast<float>(point.x), static_cast<float>(point.y))) continue;
        m_visible[i] = !m_visible[i];
        return true;
    }
    return false;
}
