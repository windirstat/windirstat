// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "AgeChart.h"

namespace
{
    constexpr double DAY_TICKS = 24.0 * 3600.0 * 10'000'000.0;   // FILETIME ticks per day
    // Upper age limit (in days) of every bucket except the last, open-ended one.
    constexpr std::array<double, CAgeChart::BucketCount - 1> BUCKET_LIMIT_DAYS{ 1, 7, 30, 182, 365, 730 };
    constexpr double BAR_FILL = 0.80;        // bar width relative to its slot
    constexpr double BAR_DEPTH = 0.20;       // extrusion relative to the bar width
    constexpr std::size_t MAX_TICKS = 6;
    constexpr COLORREF BAR_COLOR = RGB(66, 139, 240);

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

    // Rounds the largest value up to 1, 2 or 5 times a power of ten (in bytes)
    // and returns the tick step that yields at most MAX_TICKS gridlines.
    ULONGLONG NiceStep(const ULONGLONG maxValue)
    {
        double magnitude = 1.0;
        while (magnitude * 10.0 * static_cast<double>(MAX_TICKS) <= static_cast<double>(maxValue))
            magnitude *= 10.0;
        for (const double factor : { 1.0, 2.0, 5.0, 10.0 })
        {
            const double step = magnitude * factor;
            if (static_cast<double>(maxValue) / step <= static_cast<double>(MAX_TICKS))
                return static_cast<ULONGLONG>(step);
        }
        return static_cast<ULONGLONG>(magnitude * 10.0);
    }
}

CAgeChart::Buckets CAgeChart::Collect(const CItem* root)
{
    Buckets result;
    FILETIME nowFileTime;
    GetSystemTimeAsFileTime(&nowFileTime);
    const auto now = static_cast<double>(std::bit_cast<ULONGLONG>(nowFileTime));

    const auto add = [&](const CItem* file)
    {
        const ULONGLONG size = file->TmiGetSize();
        if (size == 0) return;
        const FILETIME lastChange = file->GetLastChange();
        const double ageDays = std::max(0.0,
            now - static_cast<double>(std::bit_cast<ULONGLONG>(lastChange))) / DAY_TICKS;
        std::size_t bucket = 0;
        while (bucket < BUCKET_LIMIT_DAYS.size() && ageDays >= BUCKET_LIMIT_DAYS[bucket]) ++bucket;
        result.size[bucket] += size;
        result.total += size;
    };

    if (root->IsLeaf())
    {
        if (root->IsTypeOrFlag(IT_FILE)) add(root);
        return result;
    }

    std::vector<const CItem*> stack{ root };
    while (!stack.empty())
    {
        const CItem* item = stack.back();
        stack.pop_back();
        // Hardlink bookkeeping folders only repeat files that exist elsewhere.
        if (item->IsTypeOrFlag(IT_HLINKS, IT_HLINKS_SET, IT_HLINKS_IDX)) continue;
        for (const CItem* child : item->GetChildren())
        {
            if (child->IsLeaf())
            {
                if (child->IsTypeOrFlag(IT_FILE)) add(child);
            }
            else stack.push_back(child);
        }
    }
    return result;
}

void CAgeChart::DrawChart(CDC* pdc, const CRect rc, const CItem* root) const
{
    if (pdc == nullptr) return;

    const COLORREF windowColor = DarkMode::SystemColor(COLOR_WINDOW);
    const COLORREF textColor = DarkMode::SystemColor(COLOR_WINDOWTEXT);
    pdc->FillSolidRect(rc, windowColor);
    if (root == nullptr || rc.Width() <= 0 || rc.Height() <= 0) return;

    const Buckets data = Collect(root);
    if (data.total == 0) return;

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
    const auto measure = [&](const std::wstring& text, const Gdiplus::Font& textFont)
    {
        Gdiplus::RectF box;
        graphics.MeasureString(text.c_str(), static_cast<INT>(text.size()), &textFont,
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
    const auto left = static_cast<float>(rc.left);
    const auto right = static_cast<float>(rc.right);
    const auto centerX = left + static_cast<float>(rc.Width()) / 2.0f;

    // Title and subtitle.
    const std::wstring title = GetDisplayName(root);
    const std::wstring subtitle = Localization::Lookup(IDS_AGECHART_SUBTITLE) + L" - " + FormatBytes(data.total);
    const auto titleY = static_cast<float>(rc.top) + margin;
    drawCentered(title, boldFont, centerX, titleY);
    drawCentered(subtitle, font, centerX, titleY + lineHeight * 1.1f);
    const float topLimit = titleY + lineHeight * 2.1f + margin;

    // Value axis.
    const ULONGLONG maxValue = *std::ranges::max_element(data.size);
    const ULONGLONG step = std::max<ULONGLONG>(1, NiceStep(maxValue));
    const ULONGLONG axisMax = (maxValue + step - 1) / step * step;
    float tickLabelWidth = 0.0f;
    for (ULONGLONG tick = 0; tick <= axisMax; tick += step)
        tickLabelWidth = std::max(tickLabelWidth, measure(FormatBytes(tick), font));

    // Category labels are wrapped to the slot width, so reserve up to three lines.
    const std::vector<std::wstring> names = SplitString(Localization::Lookup(IDS_AGECHART_BUCKETS));
    const float calloutHeight = 2.0f * lineHeight + 2.0f * pad;

    // A tall, narrow pane (docked at the side) shows horizontal bars instead.
    if (rc.Height() > rc.Width())
    {
        float nameWidth = 0.0f;
        for (const std::wstring& name : names) nameWidth = std::max(nameWidth, measure(name, font));
        nameWidth = std::min(nameWidth, static_cast<float>(rc.Width()) * 0.35f);
        float calloutWidth = 0.0f;
        for (const ULONGLONG size : data.size)
        {
            calloutWidth = std::max(calloutWidth, std::max(measure(FormatBytes(size), font),
                measure(FormatDouble(100.0 * static_cast<double>(size)
                    / static_cast<double>(data.total)) + L" %", font)) + 2.0f * pad);
        }

        const float hPlotLeft = left + margin + nameWidth + margin;
        const float hSlot = (static_cast<float>(rc.bottom) - margin - lineHeight - margin - topLimit)
            / static_cast<float>(BucketCount);
        const float hDepthReserve = hSlot * static_cast<float>(BAR_FILL * BAR_DEPTH);
        const float hPlotRight = right - margin - calloutWidth - pad - hDepthReserve;
        const float hPlotTop = topLimit + hDepthReserve;
        const float hPlotBottom = static_cast<float>(rc.bottom) - margin - lineHeight - margin;
        if (hPlotRight - hPlotLeft < 20.0f * static_cast<float>(scale)
            || hPlotBottom - hPlotTop < 20.0f * static_cast<float>(scale)) return;

        const float rowHeight = (hPlotBottom - hPlotTop) / static_cast<float>(BucketCount);
        const float barHeight = rowHeight * static_cast<float>(BAR_FILL);
        const float hDepth = barHeight * static_cast<float>(BAR_DEPTH);
        const auto xOf = [&](const ULONGLONG value)
        {
            return hPlotLeft + (hPlotRight - hPlotLeft)
                * static_cast<float>(static_cast<double>(value) / static_cast<double>(axisMax));
        };

        // Gridlines and tick labels along the bottom.
        const Gdiplus::Pen hGridPen(ToGdiColor(CColorSpace::BlendColor(windowColor, textColor, 0.35)), 1.0f);
        for (ULONGLONG tick = 0; tick <= axisMax; tick += step)
        {
            const float x = std::round(xOf(tick));
            graphics.DrawLine(&hGridPen, x, hPlotTop - hDepthReserve, x, hPlotBottom + pad);
            drawCentered(FormatBytes(tick), font, x, hPlotBottom + 2.0f * pad);
        }

        const Gdiplus::SolidBrush hFront(ToGdiColor(BAR_COLOR));
        const Gdiplus::SolidBrush hTop(ToGdiColor(CColorSpace::BlendColor(BAR_COLOR, RGB(255, 255, 255), 0.25)));
        const Gdiplus::SolidBrush hSide(ToGdiColor(CColorSpace::BlendColor(BAR_COLOR, RGB(0, 0, 0), 0.30)));
        const Gdiplus::SolidBrush hCalloutBrush(ToGdiColor(windowColor));
        const Gdiplus::Pen hCalloutPen(ToGdiColor(CColorSpace::BlendColor(windowColor, textColor, 0.35)), 1.0f);
        Gdiplus::StringFormat farAligned(Gdiplus::StringFormat::GenericTypographic());
        farAligned.SetAlignment(Gdiplus::StringAlignmentFar);
        farAligned.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        farAligned.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        for (const auto [index, size] : std::views::enumerate(data.size))
        {
            // The first (youngest) bucket is at the top.
            const float y = hPlotTop + static_cast<float>(index) * rowHeight + (rowHeight - barHeight) / 2.0f;
            const float x = xOf(size);

            if (size > 0)
            {
                const std::array<Gdiplus::PointF, 4> topFace{ {
                    { hPlotLeft, y }, { hPlotLeft + hDepth, y - hDepth },
                    { x + hDepth, y - hDepth }, { x, y } } };
                const std::array<Gdiplus::PointF, 4> sideFace{ {
                    { x, y }, { x + hDepth, y - hDepth },
                    { x + hDepth, y + barHeight - hDepth }, { x, y + barHeight } } };
                graphics.FillRectangle(&hFront, hPlotLeft, y, x - hPlotLeft, barHeight);
                graphics.FillPolygon(&hTop, topFace.data(), static_cast<INT>(topFace.size()));
                graphics.FillPolygon(&hSide, sideFace.data(), static_cast<INT>(sideFace.size()));
            }

            const std::wstring sizeText = FormatBytes(size);
            const std::wstring shareText = FormatDouble(100.0 * static_cast<double>(size)
                / static_cast<double>(data.total)) + L" %";
            const float boxWidth = std::max(measure(sizeText, font), measure(shareText, font)) + 2.0f * pad;
            const float boxX = x + hDepth + pad;
            const float boxY = y + (barHeight - hDepth) / 2.0f - calloutHeight / 2.0f;
            graphics.FillRectangle(&hCalloutBrush, boxX, boxY, boxWidth, calloutHeight);
            graphics.DrawRectangle(&hCalloutPen, boxX, boxY, boxWidth, calloutHeight);
            drawCentered(sizeText, font, boxX + boxWidth / 2.0f, boxY + pad);
            drawCentered(shareText, font, boxX + boxWidth / 2.0f, boxY + pad + lineHeight);

            if (static_cast<std::size_t>(index) < names.size())
            {
                const Gdiplus::RectF labelRect{ left + margin, hPlotTop + static_cast<float>(index) * rowHeight,
                    nameWidth, rowHeight };
                const std::wstring& name = names[static_cast<std::size_t>(index)];
                graphics.DrawString(name.c_str(), -1, &font, labelRect, &farAligned, &textBrush);
            }
        }

        const Gdiplus::Pen hAxisPen(ToGdiColor(textColor), 1.0f);
        graphics.DrawLine(&hAxisPen, hPlotLeft, hPlotTop - hDepthReserve, hPlotLeft, hPlotBottom + pad);
        graphics.DrawLine(&hAxisPen, hPlotLeft, hPlotBottom, hPlotRight + hDepthReserve, hPlotBottom);
        return;
    }

    const float labelHeight = lineHeight * 3.0f;
    const float plotLeft = left + margin + tickLabelWidth + margin;
    const float depthReserve = (right - margin - plotLeft) / static_cast<float>(BucketCount)
        * static_cast<float>(BAR_FILL * BAR_DEPTH);
    const float plotRight = right - margin - depthReserve;
    const float plotBottom = static_cast<float>(rc.bottom) - margin - labelHeight - margin;
    const float plotTop = topLimit + calloutHeight + pad + depthReserve;
    if (plotRight - plotLeft < 20.0f * static_cast<float>(scale)
        || plotBottom - plotTop < 20.0f * static_cast<float>(scale)) return;

    const float slot = (plotRight - plotLeft) / static_cast<float>(BucketCount);
    const float barWidth = slot * static_cast<float>(BAR_FILL);
    const float depth = barWidth * static_cast<float>(BAR_DEPTH);
    const auto yOf = [&](const ULONGLONG value)
    {
        return plotBottom - (plotBottom - plotTop)
            * static_cast<float>(static_cast<double>(value) / static_cast<double>(axisMax));
    };

    // Gridlines and tick labels.
    const Gdiplus::Pen gridPen(ToGdiColor(CColorSpace::BlendColor(windowColor, textColor, 0.35)), 1.0f);
    for (ULONGLONG tick = 0; tick <= axisMax; tick += step)
    {
        const float y = std::round(yOf(tick));
        graphics.DrawLine(&gridPen, plotLeft - pad, y, plotRight + depthReserve, y);
        const std::wstring label = FormatBytes(tick);
        graphics.DrawString(label.c_str(), static_cast<INT>(label.size()), &font,
            Gdiplus::PointF{ plotLeft - 2.0f * pad - measure(label, font), y - lineHeight / 2.0f },
            &typographic, &textBrush);
    }

    // Bars with their callouts and category labels.
    const Gdiplus::SolidBrush frontBrush(ToGdiColor(BAR_COLOR));
    const Gdiplus::SolidBrush topBrush(ToGdiColor(CColorSpace::BlendColor(BAR_COLOR, RGB(255, 255, 255), 0.25)));
    const Gdiplus::SolidBrush sideBrush(ToGdiColor(CColorSpace::BlendColor(BAR_COLOR, RGB(0, 0, 0), 0.30)));
    const Gdiplus::SolidBrush calloutBrush(ToGdiColor(windowColor));
    const Gdiplus::Pen calloutPen(ToGdiColor(CColorSpace::BlendColor(windowColor, textColor, 0.35)), 1.0f);
    for (const auto [index, size] : std::views::enumerate(data.size))
    {
        const float x = plotLeft + static_cast<float>(index) * slot + (slot - barWidth) / 2.0f;
        const float top = yOf(size);

        if (size > 0)
        {
            const std::array<Gdiplus::PointF, 4> topFace{ {
                { x, top }, { x + depth, top - depth },
                { x + barWidth + depth, top - depth }, { x + barWidth, top } } };
            const std::array<Gdiplus::PointF, 4> sideFace{ {
                { x + barWidth, top }, { x + barWidth + depth, top - depth },
                { x + barWidth + depth, plotBottom - depth }, { x + barWidth, plotBottom } } };
            graphics.FillRectangle(&frontBrush, x, top, barWidth, plotBottom - top);
            graphics.FillPolygon(&topBrush, topFace.data(), static_cast<INT>(topFace.size()));
            graphics.FillPolygon(&sideBrush, sideFace.data(), static_cast<INT>(sideFace.size()));
        }

        const std::wstring sizeText = FormatBytes(size);
        const std::wstring shareText = FormatDouble(100.0 * static_cast<double>(size)
            / static_cast<double>(data.total)) + L" %";
        const float boxWidth = std::max(measure(sizeText, font), measure(shareText, font)) + 2.0f * pad;
        const float boxX = x + (barWidth + depth) / 2.0f - boxWidth / 2.0f;
        const float boxY = top - depth - pad - calloutHeight;
        graphics.FillRectangle(&calloutBrush, boxX, boxY, boxWidth, calloutHeight);
        graphics.DrawRectangle(&calloutPen, boxX, boxY, boxWidth, calloutHeight);
        drawCentered(sizeText, font, boxX + boxWidth / 2.0f, boxY + pad);
        drawCentered(shareText, font, boxX + boxWidth / 2.0f, boxY + pad + lineHeight);

        if (static_cast<std::size_t>(index) < names.size())
        {
            const Gdiplus::RectF labelRect{ plotLeft + static_cast<float>(index) * slot, plotBottom + pad,
                slot, labelHeight };
            graphics.DrawString(names[static_cast<std::size_t>(index)].c_str(), -1, &font,
                labelRect, &centered, &textBrush);
        }
    }

    // Axes drawn last so they stay on top of the bar bases.
    const Gdiplus::Pen axisPen(ToGdiColor(textColor), 1.0f);
    graphics.DrawLine(&axisPen, plotLeft - pad, plotBottom, plotRight + depthReserve, plotBottom);
    graphics.DrawLine(&axisPen, plotLeft - pad, plotTop, plotLeft - pad, plotBottom);
}
