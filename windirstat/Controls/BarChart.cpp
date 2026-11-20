// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "BarChart.h"

namespace
{
    constexpr double BAR_FILL = 0.70;          // bar width relative to its slot
    constexpr float MIN_BAR_HEIGHT = 2.0f;
    constexpr double MIN_PLOT_HEIGHT = 40.0;

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

void CBarChart::DrawChart(CDC* pdc, const CRect rc, CItem* root)
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

void CBarChart::BuildLayout(Gdiplus::Graphics& graphics, CItem* root, const CRect& rc)
{
    m_layoutRoot = root;
    m_renderArea = rc;
    m_dpiScale = (static_cast<double>(m_dpiX) + m_dpiY) * GetFontSizePercent()
        / (2.0 * USER_DEFAULT_SCREEN_DPI * 100.0);

    const ULONGLONG total = root->TmiGetSize();
    if (total == 0) return;

    // Children are kept sorted by size (largest first), which is exactly the
    // order of the bars. A leaf is shown as a single bar.
    const auto addBar = [&](CItem* item)
    {
        Bar bar;
        bar.item = item;
        const DWORD rawColor = item->TmiGetGraphColor();
        bar.color = rawColor & 0x00FFFFFF;
        if (bar.color == RGB(0, 0, 0) && !item->TmiIsLeaf()
            && !item->IsTypeOrFlag(IT_FREESPACE, IT_UNKNOWN))
        {
            bar.color = m_folderColors.GetColor(item);
        }
        else bar.color = CTreeMap::GetFlatColor(rawColor, COptions::TreeMapOptions);
        const ULONGLONG size = item->TmiGetSize();
        bar.name = GetDisplayName(item);
        bar.sizeText = FormatBytes(size);
        bar.shareText = FormatDouble(100.0 * static_cast<double>(size)
            / static_cast<double>(total)) + L" %";
        m_bars.push_back(std::move(bar));
    };
    if (root->TmiIsLeaf()) addBar(root);
    else
    {
        for (CItem* child : root->GetChildren())
        {
            if (m_bars.size() >= MaxBars || child->TmiGetSize() == 0) break;
            addBar(child);
        }
    }
    if (m_bars.empty()) return;

    // Guarantee the descending order even if the children were not sorted yet.
    std::ranges::stable_sort(m_bars, std::greater<>{},
        [](const Bar& bar) { return bar.item->TmiGetSize(); });

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

    // A tall, narrow area (docked at the side) shows horizontal bars: names on the
    // left, the size and share at the end of every bar, largest bar on top.
    m_horizontal = rc.Height() > rc.Width();
    if (m_horizontal)
    {
        float nameWidth = 0.0f;
        float valueWidth = 0.0f;
        for (Bar& bar : m_bars)
        {
            bar.valueText = bar.sizeText + L" (" + bar.shareText + L")";
            nameWidth = std::max(nameWidth, measure(bar.name, font));
            valueWidth = std::max(valueWidth, measure(bar.valueText, font));
        }
        nameWidth = std::min(nameWidth, static_cast<float>(rc.Width()) * 0.30f);
        m_labelLeft = static_cast<float>(rc.left) + margin;
        m_plotLeft = m_labelLeft + nameWidth + margin;
        const float plotRight = static_cast<float>(rc.right) - margin - valueWidth - pad;
        const float plotBottom = static_cast<float>(rc.bottom) - margin;
        const float rowHeight = (plotBottom - topLimit) / static_cast<float>(m_bars.size());
        if (plotRight - m_plotLeft < 20.0 * scale || rowHeight < lineHeight)
        {
            m_bars.clear();
            return;
        }

        const float barHeight = static_cast<float>(rowHeight * BAR_FILL);
        const auto maxSize = static_cast<double>(m_bars.front().item->TmiGetSize());
        for (auto [index, bar] : std::views::enumerate(m_bars))
        {
            const float rowTop = topLimit + static_cast<float>(index) * rowHeight;
            const auto width = std::max(MIN_BAR_HEIGHT, static_cast<float>(
                (plotRight - m_plotLeft) * static_cast<double>(bar.item->TmiGetSize()) / maxSize));
            bar.rect = { m_plotLeft, rowTop + (rowHeight - barHeight) / 2.0f, width, barHeight };
            bar.slot = { m_labelLeft, rowTop, static_cast<float>(rc.right) - margin - m_labelLeft, rowHeight };
        }
        return;
    }

    // Plot area: the size and share labels sit above the tallest bar, the name below the baseline.
    const float labelsAbove = 2.0f * lineHeight + 2.0f * pad;
    const float labelsBelow = lineHeight + 2.0f * pad;
    const float plotTop = topLimit + labelsAbove;
    m_baseline = static_cast<float>(rc.bottom) - margin - labelsBelow;
    const float plotHeight = m_baseline - plotTop;
    if (plotHeight < MIN_PLOT_HEIGHT * scale)
    {
        m_bars.clear();
        return;
    }

    const float left = static_cast<float>(rc.left) + margin;
    const float slotWidth = (static_cast<float>(rc.Width()) - 2.0f * margin)
        / static_cast<float>(m_bars.size());
    const float barWidth = static_cast<float>(slotWidth * BAR_FILL);
    const auto maxSize = static_cast<double>(m_bars.front().item->TmiGetSize());
    for (auto [index, bar] : std::views::enumerate(m_bars))
    {
        const float slotLeft = left + static_cast<float>(index) * slotWidth;
        const auto height = std::max(MIN_BAR_HEIGHT, static_cast<float>(
            plotHeight * static_cast<double>(bar.item->TmiGetSize()) / maxSize));
        bar.rect = { slotLeft + (slotWidth - barWidth) / 2.0f, m_baseline - height, barWidth, height };
        bar.slot = { slotLeft, plotTop - labelsAbove, slotWidth,
            m_baseline + labelsBelow - (plotTop - labelsAbove) };
    }
}

void CBarChart::Render(Gdiplus::Graphics& graphics) const
{
    if (m_bars.empty()) return;

    const COLORREF window = DarkMode::SystemColor(COLOR_WINDOW);
    const COLORREF windowText = DarkMode::SystemColor(COLOR_WINDOWTEXT);
    const double scale = m_dpiScale;
    const auto pad = static_cast<float>(3.0 * scale);

    const Gdiplus::FontFamily uiFamily(GetUiFontFamilyName().c_str());
    const Gdiplus::FontFamily* family = uiFamily.GetLastStatus() == Gdiplus::Ok
        ? &uiFamily : Gdiplus::FontFamily::GenericSansSerif();
    const Gdiplus::Font font(family, m_fontSize, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    const Gdiplus::Font boldFont(family, m_fontSize, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::StringFormat format(Gdiplus::StringFormat::GenericTypographic());
    const Gdiplus::SolidBrush textBrush(ToGdiColor(windowText));
    const auto drawText = [&](const std::wstring& text, const Gdiplus::Font& textFont,
        const Gdiplus::PointF& position)
    {
        graphics.DrawString(text.c_str(), static_cast<INT>(text.size()), &textFont,
            position, &format, &textBrush);
    };

    drawText(m_title, boldFont, m_titlePosition);
    drawText(m_subtitle, font, m_subtitlePosition);

    const Gdiplus::Pen axis(ToGdiColor(CColorSpace::BlendColor(windowText, window, 0.45)), 1.0f);
    if (m_horizontal)
    {
        Gdiplus::StringFormat nameFormat(Gdiplus::StringFormat::GenericTypographic());
        nameFormat.SetAlignment(Gdiplus::StringAlignmentFar);
        nameFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        nameFormat.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        nameFormat.SetFormatFlags(nameFormat.GetFormatFlags() | Gdiplus::StringFormatFlagsNoWrap);
        Gdiplus::StringFormat valueFormat(Gdiplus::StringFormat::GenericTypographic());
        valueFormat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        valueFormat.SetFormatFlags(valueFormat.GetFormatFlags() | Gdiplus::StringFormatFlagsNoWrap);

        Gdiplus::SolidBrush barBrush(Gdiplus::Color{});
        Gdiplus::Pen barPen(Gdiplus::Color{}, 1.0f);
        graphics.DrawLine(&axis, m_plotLeft, m_bars.front().slot.Y, m_plotLeft, m_bars.back().slot.GetBottom());
        for (const Bar& bar : m_bars)
        {
            barBrush.SetColor(ToGdiColor(bar.color));
            graphics.FillRectangle(&barBrush, bar.rect);
            barPen.SetColor(ToGdiColor(CColorSpace::BlendColor(bar.color, RGB(0, 0, 0), 0.35)));
            graphics.DrawRectangle(&barPen, bar.rect);

            const Gdiplus::RectF nameBox(m_labelLeft, bar.slot.Y, m_plotLeft - m_labelLeft - 2.0f * pad, bar.slot.Height);
            graphics.DrawString(bar.name.c_str(), static_cast<INT>(bar.name.size()), &font,
                nameBox, &nameFormat, &textBrush);
            const Gdiplus::RectF valueBox(bar.rect.GetRight() + pad, bar.slot.Y,
                bar.slot.GetRight() - bar.rect.GetRight() - pad, bar.slot.Height);
            graphics.DrawString(bar.valueText.c_str(), static_cast<INT>(bar.valueText.size()), &font,
                valueBox, &valueFormat, &textBrush);
        }
        return;
    }

    // Baseline.
    const float firstLeft = m_bars.front().slot.X;
    const float lastRight = m_bars.back().slot.GetRight();
    graphics.DrawLine(&axis, firstLeft, m_baseline, lastRight, m_baseline);

    Gdiplus::SolidBrush brush(Gdiplus::Color{});
    Gdiplus::Pen pen(Gdiplus::Color{}, 1.0f);
    Gdiplus::StringFormat centered(Gdiplus::StringFormat::GenericTypographic());
    centered.SetAlignment(Gdiplus::StringAlignmentCenter);
    centered.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    centered.SetFormatFlags(centered.GetFormatFlags() | Gdiplus::StringFormatFlagsNoWrap);

    for (const Bar& bar : m_bars)
    {
        brush.SetColor(ToGdiColor(bar.color));
        graphics.FillRectangle(&brush, bar.rect);
        pen.SetColor(ToGdiColor(CColorSpace::BlendColor(bar.color, RGB(0, 0, 0), 0.35)));
        graphics.DrawRectangle(&pen, bar.rect);

        // Size and share above the bar, centered; the name below the baseline, trimmed to the slot.
        const float labelWidth = bar.slot.Width;
        const Gdiplus::RectF sizeBox(bar.slot.X, bar.rect.Y - pad - 2.0f * m_lineHeight, labelWidth, m_lineHeight);
        const Gdiplus::RectF shareBox(bar.slot.X, bar.rect.Y - pad - m_lineHeight, labelWidth, m_lineHeight);
        const Gdiplus::RectF nameBox(bar.slot.X, m_baseline + pad, labelWidth, m_lineHeight + pad);
        graphics.DrawString(bar.sizeText.c_str(), static_cast<INT>(bar.sizeText.size()), &font,
            sizeBox, &centered, &textBrush);
        graphics.DrawString(bar.shareText.c_str(), static_cast<INT>(bar.shareText.size()), &font,
            shareBox, &centered, &textBrush);
        graphics.DrawString(bar.name.c_str(), static_cast<INT>(bar.name.size()), &font,
            nameBox, &centered, &textBrush);
    }
}

void CBarChart::ClearLayout()
{
    m_folderColors.Clear();
    m_layoutRoot = nullptr;
    m_renderArea.Clear();
    m_dpiX = USER_DEFAULT_SCREEN_DPI;
    m_dpiY = USER_DEFAULT_SCREEN_DPI;
    m_dpiScale = 1.0;
    m_baseline = 0.0f;
    m_horizontal = false;
    m_plotLeft = m_labelLeft = 0.0f;
    m_title.clear();
    m_subtitle.clear();
    m_bars.clear();
}

void CBarChart::TrimMemory()
{
    ClearLayout();
    m_folderColors = {};
    decltype(m_bars){}.swap(m_bars);
}

const CBarChart::Bar* CBarChart::FindBar(const CItem* item) const
{
    // A hovered or selected descendant belongs to the bar of its ancestor.
    for (const CItem* current = item; current != nullptr; current = current->GetParent())
    {
        const auto found = std::ranges::find_if(m_bars, [current](const Bar& bar) { return bar.item == current; });
        if (found != m_bars.end()) return &*found;
        if (current == m_layoutRoot) break;
    }
    return nullptr;
}

void CBarChart::DrawOutlineBar(Gdiplus::Graphics& graphics, const Bar& bar,
    const COLORREF color, const float width) const
{
    const auto scale = static_cast<float>(m_dpiScale);
    Gdiplus::Pen underlay(ToGdiColor(CColorSpace::GetContrastingColor(color)), (width + 1.0f) * scale);
    Gdiplus::Pen pen(ToGdiColor(color), width * scale);
    graphics.DrawRectangle(&underlay, bar.rect);
    graphics.DrawRectangle(&pen, bar.rect);
}

void CBarChart::DrawHoverItem(CDC* pdc, const CItem* item) const
{
    const Bar* bar = FindBar(item);
    if (pdc == nullptr || bar == nullptr) return;

    Gdiplus::Graphics graphics(pdc->Handle());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    DrawOutlineBar(graphics, *bar, DarkMode::SystemColor(COLOR_WINDOWTEXT), 1.0f);
}

void CBarChart::DrawOutlineItems(CDC* pdc, const std::span<const CItem* const> items,
    const COLORREF color, const float width) const
{
    if (pdc == nullptr || items.empty()) return;

    Gdiplus::Graphics graphics(pdc->Handle());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    std::unordered_set<const Bar*> drawn;
    for (const CItem* item : items)
    {
        const Bar* bar = FindBar(item);
        if (bar != nullptr && drawn.insert(bar).second)
            DrawOutlineBar(graphics, *bar, color, width);
    }
}

std::wstring CBarChart::GetBarText(const CItem* item) const
{
    const Bar* bar = FindBar(item);
    if (bar == nullptr) return {};
    return bar->name + L"\n" + bar->sizeText + L"\n" + bar->shareText;
}

CItem* CBarChart::FindItemByPoint(const CPoint point) const
{
    if (m_bars.empty() || !m_renderArea.Contains(point)) return nullptr;

    const Gdiplus::PointF position(static_cast<Gdiplus::REAL>(point.x), static_cast<Gdiplus::REAL>(point.y));
    for (const Bar& bar : m_bars)
    {
        // The whole column counts, so the small bars are easy to hit.
        if (bar.slot.Contains(position)) return bar.item;
    }
    return nullptr;
}
