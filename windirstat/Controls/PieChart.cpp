// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "PieChart.h"

#include <numbers>

namespace
{
    constexpr double FULL_CIRCLE = 360.0;
    constexpr double HALF_CIRCLE = FULL_CIRCLE / 2.0;
    constexpr double DEGREES_TO_RADIANS = std::numbers::pi_v<double> / HALF_CIRCLE;
    constexpr double RADIANS_TO_DEGREES = HALF_CIRCLE / std::numbers::pi_v<double>;
    constexpr double TILT = 0.55;                 // vertical / horizontal radius of the top face
    constexpr double DEPTH = 0.10;                // thickness of the pie relative to its horizontal radius
    constexpr double MIN_SLICE_FRACTION = 0.01;   // smaller children are merged into "Others"
    constexpr double ARC_STEP_DEGREES = 1.5;
    constexpr double MIN_RADIUS = 8.0;
    constexpr double MIN_RADIUS_WITH_CALLOUTS = 60.0;
    constexpr std::size_t MAX_SLICES = 8;

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

    // Drive names include free-space statistics that would only crowd the chart text.
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
}

void CPieChart::DrawPie(CDC* pdc, const CRect rc, CItem* root)
{
    if (pdc == nullptr)
    {
        ClearLayout();
        return;
    }

    pdc->FillSolidRect(rc, DarkMode::SystemColor(COLOR_WINDOW));
    if (root == nullptr || rc.Width() <= 0 || rc.Height() <= 0)
    {
        ClearLayout();
        return;
    }

    const int dpiX = std::max(1, pdc->GetDeviceCaps(LOGPIXELSX));
    const int dpiY = std::max(1, pdc->GetDeviceCaps(LOGPIXELSY));
    Gdiplus::Graphics graphics(pdc->Handle());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

    if (m_layoutRoot != root || m_renderArea != rc || m_dpiX != dpiX || m_dpiY != dpiY)
    {
        ClearLayout();
        m_dpiX = dpiX;
        m_dpiY = dpiY;
        BuildLayout(graphics, root, rc);
    }
    Render(graphics);
}

void CPieChart::BuildLayout(Gdiplus::Graphics& graphics, CItem* root, const CRect& rc)
{
    m_layoutRoot = root;
    m_renderArea = rc;
    m_dpiScale = (static_cast<double>(m_dpiX) + m_dpiY) * GetFontSizePercent()
        / (2.0 * USER_DEFAULT_SCREEN_DPI * 100.0);

    const ULONGLONG total = root->TmiGetSize();
    if (total == 0) return;

    // Collect the largest children. Children are kept sorted by size, so the
    // first one below the threshold ends the list.
    const auto addSlice = [&](CItem* item, const ULONGLONG size, const bool remainder,
        const double start, const double end)
    {
        Slice slice;
        slice.item = item;
        slice.remainderSize = remainder ? size : 0;
        slice.startAngle = start;
        slice.sweepAngle = end - start;
        if (remainder)
        {
            slice.color = CColorSpace::BlendColor(DarkMode::SystemColor(COLOR_WINDOW),
                DarkMode::SystemColor(COLOR_WINDOWTEXT), 0.35);
            slice.name = Localization::Lookup(IDS_PIECHART_OTHERS);
        }
        else
        {
            const DWORD rawColor = item->TmiGetGraphColor();
            slice.color = rawColor & 0x00FFFFFF;
            if (slice.color == RGB(0, 0, 0) && !item->TmiIsLeaf()
                && !item->IsTypeOrFlag(IT_FREESPACE, IT_UNKNOWN))
            {
                slice.color = m_folderColors.GetColor(item);
            }
            else slice.color = CTreeMap::GetFlatColor(rawColor, COptions::TreeMapOptions);

            // Slices sharing a color (e.g. files of the same extension) are hard to tell apart,
            // so repeated colors are replaced by distinct palette colors.
            if (std::ranges::any_of(m_slices, [&](const Slice& s) { return s.color == slice.color; }))
            {
                static constexpr std::array palette{
                    RGB(79, 157, 232), RGB(71, 173, 128), RGB(230, 159, 0), RGB(213, 94, 0),
                    RGB(168, 135, 224), RGB(86, 180, 233), RGB(204, 121, 167), RGB(230, 205, 70)
                };
                const std::size_t n = m_slices.size();
                slice.color = palette[n % palette.size()];
                if (n >= palette.size())
                    slice.color = CColorSpace::BlendColor(slice.color, RGB(0, 0, 0), 0.25 * static_cast<double>((n / palette.size()) % 3));
            }
            slice.name = GetDisplayName(item);
        }
        slice.sizeText = FormatBytes(size);
        slice.shareText = FormatDouble(100.0 * static_cast<double>(size)
            / static_cast<double>(total)) + L" %";
        m_slices.push_back(std::move(slice));
    };

    const auto angleOf = [total](const ULONGLONG size)
    {
        return FULL_CIRCLE * static_cast<double>(size) / static_cast<double>(total);
    };
    if (root->TmiIsLeaf()) addSlice(root, total, false, 0.0, FULL_CIRCLE);
    else
    {
        ULONGLONG placed = 0;
        for (CItem* child : root->GetChildren())
        {
            if (m_slices.size() >= MAX_SLICES || placed >= total) break;
            const ULONGLONG size = std::min(child->TmiGetSize(), total - placed);
            if (static_cast<double>(size) < static_cast<double>(total) * MIN_SLICE_FRACTION) break;
            addSlice(child, size, false, angleOf(placed), angleOf(placed + size));
            placed += size;
        }
        if (placed < total) addSlice(root, total - placed, true, angleOf(placed), FULL_CIRCLE);
    }
    if (m_slices.empty()) return;

    // Text metrics.
    const double scale = m_dpiScale;
    const Gdiplus::FontFamily uiFamily(GetUiFontFamilyName().c_str());
    const Gdiplus::FontFamily* family = uiFamily.GetLastStatus() == Gdiplus::Ok
        ? &uiFamily : Gdiplus::FontFamily::GenericSansSerif();
    m_fontSize = static_cast<float>(12.0 * scale);
    const Gdiplus::Font font(family, m_fontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    const Gdiplus::Font boldFont(family, m_fontSize, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    const Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());
    const auto measure = [&](const std::wstring& text, const Gdiplus::Font& textFont)
    {
        Gdiplus::RectF box;
        graphics.MeasureString(text.c_str(), static_cast<INT>(text.size()), &textFont,
            Gdiplus::PointF{}, &format, &box);
        return box.Width;
    };

    const auto margin = static_cast<float>(8.0 * scale);
    const auto pad = static_cast<float>(3.0 * scale);
    const float lineHeight = font.GetHeight(&graphics);
    m_lineHeight = lineHeight;

    // Title.
    m_title = GetDisplayName(root);
    m_subtitle = FormatBytes(total);
    const auto centerX = static_cast<float>(rc.left) + static_cast<float>(rc.Width()) / 2.0f;
    m_titlePosition = { centerX - measure(m_title, boldFont) / 2.0f,
        static_cast<float>(rc.top) + margin };
    m_subtitlePosition = { centerX - measure(m_subtitle, font) / 2.0f,
        m_titlePosition.Y + lineHeight * 1.1f };
    const float topLimit = m_subtitlePosition.Y + lineHeight + margin;

    // Legend, wrapped into centered rows below the chart.
    const float swatch = lineHeight * 0.8f;
    const auto swatchGap = static_cast<float>(4.0 * scale);
    const auto entryGap = static_cast<float>(14.0 * scale);
    const float maxRowWidth = std::max(1.0f, static_cast<float>(rc.Width()) - 2.0f * (margin + pad));
    std::vector<float> rowWidths;
    std::vector<std::size_t> rowOfSlice;
    for (Slice& slice : m_slices)
    {
        slice.legendRect.Width = swatch + swatchGap + measure(slice.name, font);
        slice.legendRect.Height = lineHeight;
        if (rowWidths.empty() || rowWidths.back() + entryGap + slice.legendRect.Width > maxRowWidth)
            rowWidths.push_back(0.0f);
        float& rowWidth = rowWidths.back();
        slice.legendRect.X = rowWidth + (rowWidth > 0.0f ? entryGap : 0.0f);   // relative to the row for now
        rowWidth = slice.legendRect.GetRight();
        rowOfSlice.push_back(rowWidths.size() - 1);
    }
    const float rowHeight = lineHeight + pad;
    const float legendWidth = std::min(static_cast<float>(rc.Width()) - 2.0f * margin,
        *std::ranges::max_element(rowWidths) + 2.0f * pad);
    const float legendHeight = static_cast<float>(rowWidths.size()) * rowHeight + pad;
    const float legendTop = static_cast<float>(rc.bottom) - margin - legendHeight;
    m_legendBox = { centerX - legendWidth / 2.0f, legendTop, legendWidth, legendHeight };
    for (auto [index, slice] : std::views::enumerate(m_slices))
    {
        const std::size_t row = rowOfSlice[static_cast<std::size_t>(index)];
        slice.legendRect.X += centerX - rowWidths[row] / 2.0f;
        slice.legendRect.Y = legendTop + pad + static_cast<float>(row) * rowHeight;
    }

    // Pie geometry. Callouts need room on both sides; drop them if the chart would get tiny.
    const float bottomLimit = legendTop - margin;
    const float availableHeight = bottomLimit - topLimit;
    const auto leaderGap = static_cast<float>(36.0 * scale);
    const float boxHeight = 2.0f * lineHeight + 2.0f * pad;
    float calloutWidth = 0.0f;
    for (Slice& slice : m_slices)
    {
        slice.sizeWidth = measure(slice.sizeText, font);
        slice.shareWidth = measure(slice.shareText, font);
        calloutWidth = std::max(calloutWidth, std::max(slice.sizeWidth, slice.shareWidth) + 2.0f * pad);
    }
    const auto fitRadius = [&](const bool callouts)
    {
        const double width = static_cast<double>(rc.Width()) - 2.0 * margin
            - (callouts ? 2.0 * (calloutWidth + leaderGap) : 0.0);
        return std::min(width / 2.0, availableHeight / (2.0 * TILT + DEPTH));
    };
    bool callouts = true;
    double radius = fitRadius(true);
    if (radius < MIN_RADIUS_WITH_CALLOUTS * scale)
    {
        callouts = false;
        radius = fitRadius(false);
    }
    if (radius < MIN_RADIUS * scale)
    {
        m_slices.clear();
        return;
    }

    m_radiusX = radius;
    m_radiusY = radius * TILT;
    m_depth = radius * DEPTH;
    m_centerX = centerX;
    const double pieHeight = 2.0 * m_radiusY + m_depth;
    m_centerY = topLimit + (availableHeight - pieHeight) / 2.0 + m_radiusY;
    if (!callouts) return;

    // Callouts: one box per side column, spread vertically so none overlap.
    struct Placement
    {
        std::size_t index;
        float desiredY;
    };
    for (const bool right : { true, false })
    {
        std::vector<Placement> column;
        for (auto [index, slice] : std::views::enumerate(m_slices))
        {
            const double middle = (slice.startAngle + slice.sweepAngle / 2.0) * DEGREES_TO_RADIANS;
            const double cosine = std::cos(middle);
            const double sine = std::sin(middle);
            if ((cosine >= 0.0) != right) continue;

            const float wall = sine > 0.0 ? static_cast<float>(m_depth / 2.0) : 0.0f;
            slice.leader[0] = { static_cast<float>(m_centerX + 0.92 * m_radiusX * cosine),
                static_cast<float>(m_centerY + 0.92 * m_radiusY * sine) + wall };
            column.push_back({ static_cast<std::size_t>(index),
                static_cast<float>(m_centerY + 1.1 * m_radiusY * sine) + wall });
        }
        std::ranges::sort(column, {}, &Placement::desiredY);

        const float minCenter = topLimit + boxHeight / 2.0f;
        const float maxCenter = bottomLimit - boxHeight / 2.0f;
        std::vector<float> centers(column.size());
        for (const auto [k, placement] : std::views::enumerate(column))
        {
            centers[static_cast<std::size_t>(k)] = k == 0 ? std::max(placement.desiredY, minCenter)
                : std::max(placement.desiredY, centers[static_cast<std::size_t>(k) - 1] + boxHeight + pad);
        }
        float limit = maxCenter;
        for (std::size_t k = column.size(); k-- > 0;)
        {
            centers[k] = std::min(centers[k], limit);
            limit = centers[k] - boxHeight - pad;
        }

        for (const auto [k, placement] : std::views::enumerate(column))
        {
            Slice& slice = m_slices[placement.index];
            const float width = std::max(slice.sizeWidth, slice.shareWidth) + 2.0f * pad;
            const float y = centers[static_cast<std::size_t>(k)];
            const auto edge = static_cast<float>(m_centerX + (right ? 1.0 : -1.0) * (m_radiusX + leaderGap));
            slice.hasCallout = true;
            slice.calloutRect = { right ? edge : edge - width, y - boxHeight / 2.0f, width, boxHeight };
            slice.leader[1] = { static_cast<float>(m_centerX + (right ? 1.0 : -1.0)
                * (m_radiusX + leaderGap * 0.35)), y };
            slice.leader[2] = { edge, y };
        }
    }
}

void CPieChart::AddArcPoints(std::vector<Gdiplus::PointF>& points, const double startAngle,
    const double endAngle, const double yOffset, const bool reverse) const
{
    const double span = endAngle - startAngle;
    const int steps = std::max(2, static_cast<int>(std::ceil(span / ARC_STEP_DEGREES)));
    for (const int step : std::views::iota(0, steps + 1))
    {
        const double fraction = static_cast<double>(step) / steps;
        const double angle = (reverse ? endAngle - span * fraction : startAngle + span * fraction)
            * DEGREES_TO_RADIANS;
        points.emplace_back(
            static_cast<Gdiplus::REAL>(m_centerX + m_radiusX * std::cos(angle)),
            static_cast<Gdiplus::REAL>(m_centerY + yOffset + m_radiusY * std::sin(angle)));
    }
}

void CPieChart::CreateTopPath(const Slice& slice, Gdiplus::GraphicsPath& path) const
{
    std::vector<Gdiplus::PointF> points;
    if (slice.sweepAngle < FULL_CIRCLE - 0.001)
        points.emplace_back(static_cast<Gdiplus::REAL>(m_centerX), static_cast<Gdiplus::REAL>(m_centerY));
    AddArcPoints(points, slice.startAngle, slice.startAngle + slice.sweepAngle, 0.0, false);
    path.AddPolygon(points.data(), static_cast<INT>(points.size()));
}

void CPieChart::Render(Gdiplus::Graphics& graphics) const
{
    if (m_slices.empty() || m_radiusX <= 0.0) return;

    const COLORREF window = DarkMode::SystemColor(COLOR_WINDOW);
    const COLORREF windowText = DarkMode::SystemColor(COLOR_WINDOWTEXT);
    Gdiplus::SolidBrush brush(Gdiplus::Color{});
    Gdiplus::Pen pen(Gdiplus::Color{}, 1.0f);
    std::vector<Gdiplus::PointF> points;

    // Front walls first: they sit below the top faces and only the lower half is visible.
    for (const Slice& slice : m_slices)
    {
        const double start = std::max(slice.startAngle, 0.0);
        const double end = std::min(slice.startAngle + slice.sweepAngle, HALF_CIRCLE);
        if (end - start < 0.01) continue;

        points.clear();
        AddArcPoints(points, start, end, 0.0, false);
        AddArcPoints(points, start, end, m_depth, true);
        const Gdiplus::Color wall = ToGdiColor(CColorSpace::BlendColor(slice.color, RGB(0, 0, 0), 0.35));
        brush.SetColor(wall);
        pen.SetColor(wall);
        graphics.FillPolygon(&brush, points.data(), static_cast<INT>(points.size()));
        graphics.DrawPolygon(&pen, points.data(), static_cast<INT>(points.size()));
    }

    for (const Slice& slice : m_slices)
    {
        Gdiplus::GraphicsPath path(Gdiplus::FillModeAlternate);
        CreateTopPath(slice, path);
        brush.SetColor(ToGdiColor(slice.color));
        graphics.FillPath(&brush, &path);
        pen.SetColor(ToGdiColor(CColorSpace::BlendColor(slice.color, window, 0.50)));
        graphics.DrawPath(&pen, &path);
    }

    const Gdiplus::FontFamily uiFamily(GetUiFontFamilyName().c_str());
    const Gdiplus::FontFamily* family = uiFamily.GetLastStatus() == Gdiplus::Ok
        ? &uiFamily : Gdiplus::FontFamily::GenericSansSerif();
    const Gdiplus::Font font(family, m_fontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    const Gdiplus::Font boldFont(family, m_fontSize, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    const Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());
    const Gdiplus::SolidBrush textBrush(ToGdiColor(windowText));
    const Gdiplus::SolidBrush windowBrush(ToGdiColor(window));
    const Gdiplus::Pen frame(ToGdiColor(CColorSpace::BlendColor(windowText, window, 0.60)), 1.0f);
    const auto drawText = [&](const std::wstring& text, const Gdiplus::Font& textFont,
        const Gdiplus::PointF& position)
    {
        graphics.DrawString(text.c_str(), static_cast<INT>(text.size()), &textFont,
            position, &format, &textBrush);
    };

    drawText(m_title, boldFont, m_titlePosition);
    drawText(m_subtitle, font, m_subtitlePosition);

    // Callouts with leader lines.
    const auto pad = static_cast<float>(3.0 * m_dpiScale);
    const Gdiplus::Pen leaderPen(ToGdiColor(CColorSpace::BlendColor(windowText, window, 0.30)), 1.0f);
    for (const Slice& slice : m_slices)
    {
        if (!slice.hasCallout) continue;
        graphics.DrawLines(&leaderPen, slice.leader.data(), static_cast<INT>(slice.leader.size()));
        graphics.FillRectangle(&windowBrush, slice.calloutRect);
        graphics.DrawRectangle(&frame, slice.calloutRect);
        const float right = slice.calloutRect.GetRight() - pad;
        drawText(slice.sizeText, font, { right - slice.sizeWidth, slice.calloutRect.Y + pad });
        drawText(slice.shareText, font, { right - slice.shareWidth, slice.calloutRect.Y + pad + m_lineHeight });
    }

    // Legend.
    graphics.FillRectangle(&windowBrush, m_legendBox);
    graphics.DrawRectangle(&frame, m_legendBox);
    const float swatch = m_lineHeight * 0.8f;
    for (const Slice& slice : m_slices)
    {
        const Gdiplus::RectF box(slice.legendRect.X, slice.legendRect.Y + (m_lineHeight - swatch) / 2.0f,
            swatch, swatch);
        brush.SetColor(ToGdiColor(slice.color));
        graphics.FillRectangle(&brush, box);
        pen.SetColor(ToGdiColor(CColorSpace::BlendColor(slice.color, RGB(0, 0, 0), 0.35)));
        graphics.DrawRectangle(&pen, box);
        drawText(slice.name, font, { box.GetRight() + static_cast<float>(4.0 * m_dpiScale), slice.legendRect.Y });
    }
}

void CPieChart::ClearLayout()
{
    m_folderColors.Clear();
    m_layoutRoot = nullptr;
    m_renderArea.Clear();
    m_dpiX = USER_DEFAULT_SCREEN_DPI;
    m_dpiY = USER_DEFAULT_SCREEN_DPI;
    m_dpiScale = 1.0;
    m_centerX = m_centerY = m_radiusX = m_radiusY = m_depth = 0.0;
    m_title.clear();
    m_subtitle.clear();
    m_slices.clear();
}

void CPieChart::TrimMemory()
{
    ClearLayout();
    m_folderColors = {};
    decltype(m_slices){}.swap(m_slices);
}

const CPieChart::Slice* CPieChart::FindRemainder() const
{
    const auto found = std::ranges::find_if(m_slices,
        [](const Slice& slice) { return slice.remainderSize != 0; });
    return found == m_slices.end() ? nullptr : &*found;
}

const CPieChart::Slice* CPieChart::FindSlice(const CItem* item) const
{
    bool belowRoot = false;
    for (const CItem* current = item; current != nullptr; current = current->GetParent())
    {
        const auto found = std::ranges::find_if(m_slices, [current](const Slice& slice)
            { return slice.remainderSize == 0 && slice.item == current; });
        if (found != m_slices.end()) return &*found;
        if (current == m_layoutRoot)
        {
            belowRoot = current != item;
            break;
        }
    }

    // Items under the root that are not visible as slices belong to "Others".
    return belowRoot ? FindRemainder() : nullptr;
}

void CPieChart::DrawOutlineSlice(Gdiplus::Graphics& graphics, const Slice& slice,
    const COLORREF color, const float width) const
{
    Gdiplus::GraphicsPath path(Gdiplus::FillModeAlternate);
    CreateTopPath(slice, path);
    const auto scale = static_cast<float>(m_dpiScale);
    Gdiplus::Pen underlay(ToGdiColor(CColorSpace::GetContrastingColor(color)), (width + 1.0f) * scale);
    Gdiplus::Pen pen(ToGdiColor(color), width * scale);
    underlay.SetLineJoin(Gdiplus::LineJoinRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    graphics.DrawPath(&underlay, &path);
    graphics.DrawPath(&pen, &path);
}

void CPieChart::DrawHoverItem(CDC* pdc, const CItem* item, const bool remainder) const
{
    const Slice* slice = remainder ? FindRemainder() : FindSlice(item);
    if (pdc == nullptr || slice == nullptr) return;

    Gdiplus::Graphics graphics(pdc->Handle());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    DrawOutlineSlice(graphics, *slice, DarkMode::SystemColor(COLOR_WINDOWTEXT), 1.0f);
}

void CPieChart::DrawOutlineItems(CDC* pdc, const std::span<const CItem* const> items,
    const COLORREF color, const float width) const
{
    if (pdc == nullptr || items.empty()) return;

    Gdiplus::Graphics graphics(pdc->Handle());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    std::unordered_set<const Slice*> drawn;
    for (const CItem* item : items)
    {
        const Slice* slice = FindSlice(item);
        if (slice != nullptr && drawn.insert(slice).second)
            DrawOutlineSlice(graphics, *slice, color, width);
    }
}

std::wstring CPieChart::GetSliceText(const CItem* item, const bool remainder) const
{
    const Slice* slice = remainder ? FindRemainder() : FindSlice(item);
    if (slice == nullptr) return {};
    return slice->name + L"\n" + slice->sizeText + L"\n" + slice->shareText;
}

CItem* CPieChart::FindItemByPoint(const CPoint point, ULONGLONG* remainderSize) const
{
    if (remainderSize != nullptr) *remainderSize = 0;
    if (m_slices.empty() || !m_renderArea.Contains(point)) return nullptr;

    const auto result = [remainderSize](const Slice& slice)
    {
        if (remainderSize != nullptr) *remainderSize = slice.remainderSize;
        return slice.item;
    };

    // Legend entries and callouts select their slice as well.
    const Gdiplus::PointF position(static_cast<Gdiplus::REAL>(point.x), static_cast<Gdiplus::REAL>(point.y));
    for (const Slice& slice : m_slices)
    {
        if (slice.legendRect.Contains(position) || (slice.hasCallout && slice.calloutRect.Contains(position)))
            return result(slice);
    }
    if (m_radiusX <= 0.0 || m_radiusY <= 0.0) return nullptr;

    // Top face first, then the front wall (the top face shifted down by the depth).
    const double u = (point.x - m_centerX) / m_radiusX;
    double v = (point.y - m_centerY) / m_radiusY;
    if (u * u + v * v > 1.0)
    {
        v = (point.y - m_depth - m_centerY) / m_radiusY;
        if (v < 0.0 || u * u + v * v > 1.0) return nullptr;
    }

    double angle = std::atan2(v, u) * RADIANS_TO_DEGREES;
    if (angle < 0.0) angle += FULL_CIRCLE;
    for (const Slice& slice : m_slices)
    {
        if (angle >= slice.startAngle && angle < slice.startAngle + slice.sweepAngle)
            return result(slice);
    }
    return result(m_slices.back());
}
