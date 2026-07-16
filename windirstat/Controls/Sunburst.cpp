// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "pch.h"
#include "Sunburst.h"
#include "TreeMap.h"

#include <numbers>

namespace
{
    constexpr double FULL_CIRCLE = 360.0;
    constexpr double HALF_CIRCLE = FULL_CIRCLE / 2.0;
    constexpr double QUARTER_CIRCLE = FULL_CIRCLE / 4.0;
    constexpr double DEGREES_TO_RADIANS = std::numbers::pi_v<double> / HALF_CIRCLE;
    constexpr double RADIANS_TO_DEGREES = HALF_CIRCLE / std::numbers::pi_v<double>;
    constexpr double MIN_ARC_PIXELS = 0.75;
    constexpr int MIN_RING_WIDTH = 14;
    constexpr int MAX_DEPTH = 64;
    constexpr double PALETTE_BRIGHTNESS = 0.6;

    constexpr COLORREF DimColor(const COLORREF rgb, float factor) noexcept
    {
        factor = std::clamp(factor, 0.0f, 1.0f);
        return RGB(
            static_cast<BYTE>(GetRValue(rgb) * factor),
            static_cast<BYTE>(GetGValue(rgb) * factor),
            static_cast<BYTE>(GetBValue(rgb) * factor));
    }

    COLORREF GetDepthColor(const int depth) noexcept
    {
        static constexpr std::array<COLORREF, 8> palette{
            RGB(79, 157, 232),
            RGB(95, 196, 163),
            RGB(244, 190, 89),
            RGB(239, 126, 121),
            RGB(168, 135, 224),
            RGB(94, 186, 207),
            RGB(210, 151, 102),
            RGB(216, 117, 170),
        };
        return depth <= 0
            ? RGB(78, 86, 99)
            : palette[static_cast<std::size_t>(depth - 1) % palette.size()];
    }

    constexpr COLORREF ApplyHoverColor(const COLORREF color) noexcept
    {
        return RGB(
            std::min(255, GetRValue(color) + 55),
            std::min(255, GetGValue(color) + 55),
            std::min(255, GetBValue(color) + 55));
    }

    Gdiplus::Color ToGdiColor(const COLORREF color) noexcept
    {
        return Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color));
    }

    const Gdiplus::FontFamily* SelectUiFontFamily(Gdiplus::FontFamily& uiFontFamily)
    {
        return uiFontFamily.GetLastStatus() == Gdiplus::Ok
            ? &uiFontFamily
            : Gdiplus::FontFamily::GenericSansSerif();
    }

    const std::wstring& GetUiFontFamilyName()
    {
        static const std::wstring fontFamilyName = []
        {
            LOGFONTW logFont{};
            const auto stockFont = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
            if (stockFont != nullptr
                && ::GetObjectW(stockFont, sizeof(logFont), &logFont) == sizeof(logFont)
                && logFont.lfFaceName[0] != L'\0')
            {
                return std::wstring(logFont.lfFaceName);
            }
            return std::wstring(L"Segoe UI");
        }();
        return fontFamilyName;
    }

    double NormalizeAngle(const double angle) noexcept
    {
        const double normalized = std::fmod(angle, FULL_CIRCLE);
        return normalized < 0.0 ? normalized + FULL_CIRCLE : normalized;
    }

    double GetUprightHalfSweep(const double normalizedAngle) noexcept
    {
        return std::abs(QUARTER_CIRCLE - std::fmod(normalizedAngle, HALF_CIRCLE));
    }
}

void CSunburst::DrawSunburst(CDC* pdc, const CRect rc, CItem* root,
    const int maxDepth)
{
    if (pdc == nullptr)
    {
        ClearLayout();
        return;
    }

    pdc->FillSolidRect(rc, BACKGROUND_COLOR);
    if (root == nullptr || rc.Width() <= 0 || rc.Height() <= 0)
    {
        ClearLayout();
        return;
    }

    const int boundedMaxDepth = std::clamp(maxDepth, 1, MAX_DEPTH);
    if (m_layoutRoot != root || m_renderArea != rc || m_maxDepth != boundedMaxDepth)
    {
        BuildLayout(root, rc, boundedMaxDepth);
    }
    RenderLayout(pdc);
}

void CSunburst::BuildLayout(CItem* root, const CRect& rc, const int maxDepth)
{
    ClearLayout();
    m_layoutRoot = root;
    m_renderArea = rc;
    m_maxDepth = std::clamp(maxDepth, 1, MAX_DEPTH);
    m_center = rc.CenterPoint();
    m_outerRadius = std::max(0.0,
        static_cast<double>(std::min(rc.Width(), rc.Height())) / 2.0 - 6.0);
    if (m_outerRadius < 2.0) return;

    m_centerRadius = std::min(m_outerRadius, std::max(12.0, m_outerRadius * 0.20));
    const double availableRadius = std::max(0.0, m_outerRadius - m_centerRadius);
    const int depthLimit = std::min(
        static_cast<int>(availableRadius / MIN_RING_WIDTH), m_maxDepth);

    struct PendingItem
    {
        CItem* item;
        double startAngle;
        double sweepAngle;
        int depth;
    };

    std::vector<PendingItem> pending;
    pending.reserve(256);
    pending.push_back({ root, 0.0, FULL_CIRCLE, 0 });
    std::vector<PendingItem> children;
    children.reserve(256);
    int actualMaxDepth = 0;

    while (!pending.empty())
    {
        const PendingItem current = pending.back();
        pending.pop_back();
        if (current.item == nullptr || current.sweepAngle <= 0.0) continue;

        const std::size_t entryIndex = m_entries.size();
        m_entries.push_back({ current.item, current.startAngle, current.sweepAngle,
            0.0, 0.0, current.depth, true });
        actualMaxDepth = std::max(actualMaxDepth, current.depth);

        if (current.depth >= depthLimit || current.item->TmiIsLeaf()
            || current.item->TmiGetSize() == 0)
        {
            continue;
        }

        const ULONGLONG totalSize = current.item->TmiGetSize();
        ULONGLONG cumulativeSize = 0;
        children.clear();
        const auto& itemChildren = current.item->GetChildren();

        for (CItem* child : itemChildren)
        {
            const ULONGLONG childSize = child->TmiGetSize();
            if (childSize == 0 || cumulativeSize >= totalSize) break;

            const ULONGLONG remaining = totalSize - cumulativeSize;
            const ULONGLONG boundedSize = std::min(childSize, remaining);
            const double childStart = current.startAngle + current.sweepAngle
                * static_cast<double>(cumulativeSize) / static_cast<double>(totalSize);
            const double childSweep = current.sweepAngle
                * static_cast<double>(boundedSize) / static_cast<double>(totalSize);
            cumulativeSize += boundedSize;

            // Sorted children after the first subpixel arc are all safely omitted.
            const double arcPixels = childSweep * DEGREES_TO_RADIANS * m_outerRadius;
            if (arcPixels < MIN_ARC_PIXELS) break;

            children.push_back({ child, childStart, childSweep, current.depth + 1 });
        }

        for (const PendingItem& child : children | std::views::reverse)
        {
            pending.push_back(child);
        }
        m_entries[entryIndex].visualLeaf = children.empty();
    }

    m_ringWidth = actualMaxDepth > 0
        ? availableRadius / static_cast<double>(actualMaxDepth)
        : 0.0;
    m_rings.resize(static_cast<std::size_t>(actualMaxDepth) + 1);
    m_itemEntries.reserve(m_entries.size());

    for (std::size_t index = 0; index < m_entries.size(); ++index)
    {
        LayoutEntry& entry = m_entries[index];
        if (entry.depth == 0)
        {
            entry.innerRadius = 0.0;
            entry.outerRadius = m_centerRadius;
        }
        else
        {
            entry.innerRadius = m_centerRadius + (entry.depth - 1) * m_ringWidth;
            const int outerDepth = entry.visualLeaf ? actualMaxDepth : entry.depth;
            entry.outerRadius = outerDepth == actualMaxDepth
                ? m_outerRadius
                : m_centerRadius + outerDepth * m_ringWidth;
        }

        const int outerDepth = entry.depth == 0 || !entry.visualLeaf
            ? entry.depth : actualMaxDepth;
        for (const int depth : std::views::iota(entry.depth, outerDepth + 1))
        {
            m_rings[static_cast<std::size_t>(depth)].push_back({
                entry.item, entry.startAngle, entry.startAngle + entry.sweepAngle });
        }
        m_itemEntries.emplace(entry.item, index);
    }

#ifdef _DEBUG
    // Reverse stack insertion preserves increasing angular order within every ring.
    for (auto& ring : m_rings)
    {
        ASSERT(std::ranges::is_sorted(ring, {}, &RingEntry::startAngle));
        ASSERT(std::ranges::is_sorted(ring, {}, &RingEntry::endAngle));
    }
#endif
}

void CSunburst::ClearLayout()
{
    m_layoutRoot = nullptr;
    m_renderArea.SetRectEmpty();
    m_center = {};
    m_outerRadius = 0.0;
    m_centerRadius = 0.0;
    m_ringWidth = 0.0;
    m_maxDepth = 0;
    m_entries.clear();
    m_itemEntries.clear();
    m_rings.clear();
}

void CSunburst::CreatePath(const LayoutEntry& entry, Gdiplus::GraphicsPath& path) const
{
    const auto outer = static_cast<Gdiplus::REAL>(entry.outerRadius);
    const auto inner = static_cast<Gdiplus::REAL>(entry.innerRadius);
    const auto start = static_cast<Gdiplus::REAL>(entry.startAngle - 90.0);
    const auto sweep = static_cast<Gdiplus::REAL>(entry.sweepAngle);

    const Gdiplus::RectF outerRect(
        static_cast<Gdiplus::REAL>(m_center.x) - outer,
        static_cast<Gdiplus::REAL>(m_center.y) - outer,
        outer * 2.0f, outer * 2.0f);

    if (inner <= 0.0f)
    {
        path.AddEllipse(outerRect);
        return;
    }

    const Gdiplus::RectF innerRect(
        static_cast<Gdiplus::REAL>(m_center.x) - inner,
        static_cast<Gdiplus::REAL>(m_center.y) - inner,
        inner * 2.0f, inner * 2.0f);

    if (entry.sweepAngle >= FULL_CIRCLE - 0.001)
    {
        path.AddEllipse(outerRect);
        path.AddEllipse(innerRect);
    }
    else
    {
        path.AddArc(outerRect, start, sweep);
        path.AddArc(innerRect, start + sweep, -sweep);
        path.CloseFigure();
    }
}

COLORREF CSunburst::GetItemColor(const LayoutEntry& entry, const bool hover) const
{
    const DWORD rawColor = entry.item->TmiGetGraphColor();
    const DWORD colorFlags = rawColor & CTreeMap::COLORFLAG_MASK;
    COLORREF color = rawColor & 0x00FFFFFF;

    if (color == RGB(0, 0, 0) && entry.item->IsTypeOrFlag(IT_DIRECTORY)
        && !entry.item->IsTypeOrFlag(IT_FREESPACE, IT_UNKNOWN))
    {
        color = GetDepthColor(entry.depth);
    }
    else if (colorFlags == CTreeMap::COLORFLAG_DARKER)
    {
        color = CColorSpace::MakeBrightColor(color, PALETTE_BRIGHTNESS);
        color = DimColor(color, 0.66f);
    }
    else if (colorFlags == CTreeMap::COLORFLAG_LIGHTER)
    {
        color = CColorSpace::MakeBrightColor(color, PALETTE_BRIGHTNESS);
        color = RGB(
            std::min(255, GetRValue(color) + 60),
            std::min(255, GetGValue(color) + 60),
            std::min(255, GetBValue(color) + 60));
    }

    return hover ? ApplyHoverColor(color) : color;
}

void CSunburst::RenderEntry(Gdiplus::Graphics& graphics, const LayoutEntry& entry,
    const bool hover, Gdiplus::SolidBrush& brush, Gdiplus::Pen& separator) const
{
    Gdiplus::GraphicsPath path(Gdiplus::FillModeAlternate);
    CreatePath(entry, path);
    const COLORREF color = GetItemColor(entry, hover);
    brush.SetColor(ToGdiColor(color));
    graphics.FillPath(&brush, &path);

    separator.SetColor(ToGdiColor(DimColor(color, 0.42f)));
    graphics.DrawPath(&separator, &path);
}

void CSunburst::RenderLayout(CDC* pdc) const
{
    Gdiplus::Graphics graphics(pdc->GetSafeHdc());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    Gdiplus::SolidBrush brush(Gdiplus::Color{});
    Gdiplus::Pen separator(Gdiplus::Color{}, 1.0f);
    separator.SetAlignment(Gdiplus::PenAlignmentInset);
    for (const LayoutEntry& entry : m_entries)
    {
        RenderEntry(graphics, entry, false, brush, separator);
    }

    const std::wstring& fontFamilyName = GetUiFontFamilyName();
    Gdiplus::FontFamily uiFontFamily(fontFamilyName.c_str());
    const Gdiplus::FontFamily* fontFamily = SelectUiFontFamily(uiFontFamily);
    std::vector<Gdiplus::PointF> points;
    std::vector<BYTE> types;
    for (const LayoutEntry& entry : m_entries)
    {
        RenderLabel(graphics, fontFamily, entry, points, types);
    }
}

void CSunburst::RenderLabel(Gdiplus::Graphics& graphics,
    const Gdiplus::FontFamily* fontFamily, const LayoutEntry& entry,
    std::vector<Gdiplus::PointF>& points, std::vector<BYTE>& types,
    const bool hover) const
{
    if (fontFamily == nullptr || entry.item->IsTypeOrFlag(IT_FREESPACE, IT_UNKNOWN)) return;

    const auto name = entry.item->GetNameView(true);
    if (name.empty()) return;

    const COLORREF itemColor = GetItemColor(entry, hover);
    const COLORREF textColor = CColorSpace::GetColorBrightness(itemColor) > 0.52
        ? RGB(0, 0, 0) : RGB(255, 255, 255);
    Gdiplus::SolidBrush textBrush(ToGdiColor(textColor));
    const Gdiplus::REAL dpiScale = graphics.GetDpiY() / 96.0f;

    if (entry.depth == 0)
    {
        const auto half = static_cast<Gdiplus::REAL>(entry.outerRadius * 0.70);
        if (half < 14) return;

        const Gdiplus::REAL fontSize = std::min(14.0f * dpiScale, half * 0.45f);
        Gdiplus::Font font(fontFamily, fontSize, Gdiplus::FontStyleRegular,
            Gdiplus::UnitPixel);
        Gdiplus::StringFormat format;
        format.SetAlignment(Gdiplus::StringAlignmentCenter);
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        format.SetTrimming(Gdiplus::StringTrimmingNone);
        format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        Gdiplus::RectF measured;
        graphics.MeasureString(name.data(), static_cast<int>(name.size()), &font,
            Gdiplus::PointF{}, &format, &measured);
        if (measured.Width > half * 2.0f || measured.Height > half * 2.0f) return;

        const Gdiplus::RectF textRect(
            static_cast<Gdiplus::REAL>(m_center.x) - half,
            static_cast<Gdiplus::REAL>(m_center.y) - half,
            half * 2.0f, half * 2.0f);
        graphics.DrawString(name.data(), static_cast<int>(name.size()), &font,
            textRect, &format, &textBrush);
        return;
    }

    const double thickness = entry.outerRadius - entry.innerRadius;
    const double middleRadius = std::midpoint(entry.innerRadius, entry.outerRadius);
    const double arcLength = entry.sweepAngle * DEGREES_TO_RADIANS * middleRadius;
    const Gdiplus::REAL minFontSize = 8.0f * dpiScale;
    const Gdiplus::REAL maxFontSize = 14.0f * dpiScale;
    const Gdiplus::REAL fontSize = static_cast<Gdiplus::REAL>(std::clamp(
        thickness * 0.44, static_cast<double>(minFontSize),
        static_cast<double>(maxFontSize)));
    const double middleAngle = entry.startAngle + entry.sweepAngle / 2.0;
    const double normalizedMiddle = NormalizeAngle(middleAngle);
    if (thickness < fontSize * 1.35) return;

    const bool spansTwoLayers = m_ringWidth > 0.0
        && thickness + 0.01 >= m_ringWidth * 2.0;
    if (entry.visualLeaf && spansTwoLayers)
    {
        const Gdiplus::REAL padding = std::max(2.0f * dpiScale, fontSize * 0.20f);
        const auto radialLength = static_cast<Gdiplus::REAL>(thickness) - padding * 2.0f;
        if (arcLength < fontSize * 1.35 || radialLength < fontSize * 1.5f) return;

        const bool outward = normalizedMiddle <= HALF_CIRCLE;
        const double startRadius = outward
            ? entry.innerRadius + padding
            : entry.outerRadius - padding;
        const double screenAngle = (normalizedMiddle - QUARTER_CIRCLE)
            * DEGREES_TO_RADIANS;
        const auto startX = static_cast<Gdiplus::REAL>(
            m_center.x + std::cos(screenAngle) * startRadius);
        const auto startY = static_cast<Gdiplus::REAL>(
            m_center.y + std::sin(screenAngle) * startRadius);
        const auto rotation = static_cast<Gdiplus::REAL>(outward
            ? normalizedMiddle - QUARTER_CIRCLE
            : normalizedMiddle - (FULL_CIRCLE - QUARTER_CIRCLE));

        Gdiplus::Font font(fontFamily, fontSize, Gdiplus::FontStyleRegular,
            Gdiplus::UnitPixel);
        Gdiplus::StringFormat format;
        format.SetAlignment(Gdiplus::StringAlignmentNear);
        format.SetTrimming(Gdiplus::StringTrimmingNone);
        format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        Gdiplus::RectF measured;
        graphics.MeasureString(name.data(), static_cast<int>(name.size()), &font,
            Gdiplus::PointF{}, &format, &measured);
        if (measured.Width > radialLength || measured.Height > arcLength) return;

        const Gdiplus::GraphicsState state = graphics.Save();
        graphics.TranslateTransform(startX, startY);
        graphics.RotateTransform(rotation);
        graphics.DrawString(name.data(), static_cast<int>(name.size()), &font,
            Gdiplus::PointF(-measured.X, -measured.Y - measured.Height / 2.0f),
            &format, &textBrush);
        graphics.Restore(state);
        return;
    }

    if (arcLength < fontSize * 2.6 || entry.sweepAngle < 3.0) return;

    // Keep the entire shaped label in one upright semicircle.
    const double direction = normalizedMiddle > QUARTER_CIRCLE
        && normalizedMiddle < FULL_CIRCLE - QUARTER_CIRCLE ? -1.0 : 1.0;
    const double uprightArcLength = 2.0 * GetUprightHalfSweep(normalizedMiddle)
        * DEGREES_TO_RADIANS * middleRadius;
    const Gdiplus::REAL sidePadding = std::max(4.0f * dpiScale, fontSize * 0.45f);
    const auto availableWidth = static_cast<Gdiplus::REAL>(
        std::min(arcLength, uprightArcLength)) - sidePadding * 2.0f;
    if (availableWidth < fontSize * 1.5f) return;

    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentCenter);
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    format.SetTrimming(Gdiplus::StringTrimmingNone);
    format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap
        | Gdiplus::StringFormatFlagsNoClip);
    Gdiplus::GraphicsPath textPath(Gdiplus::FillModeAlternate);
    if (textPath.AddString(name.data(), static_cast<int>(name.size()), fontFamily,
        Gdiplus::FontStyleRegular, fontSize, Gdiplus::PointF{}, &format)
        != Gdiplus::Ok)
    {
        return;
    }

    const int pointCount = textPath.GetPointCount();
    if (pointCount <= 0) return;

    Gdiplus::RectF bounds;
    if (textPath.GetBounds(&bounds) != Gdiplus::Ok || bounds.IsEmptyArea()) return;
    if (bounds.Width > availableWidth || bounds.Height > thickness * 0.80) return;

    points.resize(static_cast<std::size_t>(pointCount));
    types.resize(static_cast<std::size_t>(pointCount));
    if (textPath.GetPathPoints(points.data(), pointCount) != Gdiplus::Ok
        || textPath.GetPathTypes(types.data(), pointCount) != Gdiplus::Ok)
    {
        return;
    }

    // Bend the shaped outline around the ring without rotating it past vertical.
    const double textCenterX = bounds.X + bounds.Width / 2.0;
    const double textCenterY = bounds.Y + bounds.Height / 2.0;
    const double radialScale = std::min(1.0, thickness * 0.80 / bounds.Height);
    for (Gdiplus::PointF& point : points)
    {
        const double glyphAngle = middleAngle + direction
            * (point.X - textCenterX) / middleRadius * RADIANS_TO_DEGREES;
        const double radius = middleRadius - direction
            * (point.Y - textCenterY) * radialScale;
        const double screenAngle = (glyphAngle - QUARTER_CIRCLE) * DEGREES_TO_RADIANS;
        point.X = static_cast<Gdiplus::REAL>(m_center.x + std::cos(screenAngle) * radius);
        point.Y = static_cast<Gdiplus::REAL>(m_center.y + std::sin(screenAngle) * radius);
    }

    Gdiplus::GraphicsPath curvedPath(points.data(), types.data(), pointCount,
        textPath.GetFillMode());
    graphics.FillPath(&textBrush, &curvedPath);
}

const CSunburst::LayoutEntry* CSunburst::FindLayoutEntry(const CItem* item) const
{
    const auto found = m_itemEntries.find(item);
    return found == m_itemEntries.end() ? nullptr : &m_entries[found->second];
}

void CSunburst::DrawHoverItem(CDC* pdc, const CItem* item) const
{
    const LayoutEntry* entry = FindLayoutEntry(item);
    if (pdc == nullptr || entry == nullptr) return;

    Gdiplus::Graphics graphics(pdc->GetSafeHdc());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    Gdiplus::SolidBrush brush(Gdiplus::Color{});
    Gdiplus::Pen separator(Gdiplus::Color{}, 1.0f);
    separator.SetAlignment(Gdiplus::PenAlignmentInset);
    RenderEntry(graphics, *entry, true, brush, separator);

    const std::wstring& fontFamilyName = GetUiFontFamilyName();
    Gdiplus::FontFamily uiFontFamily(fontFamilyName.c_str());
    std::vector<Gdiplus::PointF> points;
    std::vector<BYTE> types;
    RenderLabel(graphics, SelectUiFontFamily(uiFontFamily), *entry, points, types, true);
}

void CSunburst::DrawOutlineItems(CDC* pdc, const std::span<const CItem* const> items,
    const COLORREF color, const float width) const
{
    if (pdc == nullptr || items.empty()) return;

    Gdiplus::Graphics graphics(pdc->GetSafeHdc());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    Gdiplus::Pen pen(ToGdiColor(color), std::max(1.0f, width));
    pen.SetAlignment(Gdiplus::PenAlignmentInset);

    for (const CItem* item : items)
    {
        const LayoutEntry* entry = FindLayoutEntry(item);
        if (entry == nullptr) continue;

        Gdiplus::GraphicsPath path(Gdiplus::FillModeAlternate);
        CreatePath(*entry, path);
        graphics.DrawPath(&pen, &path);
    }
}

CItem* CSunburst::FindItemByPoint(const CPoint point) const
{
    if (m_entries.empty() || !m_renderArea.PtInRect(point)) return nullptr;

    const double dx = static_cast<double>(point.x - m_center.x);
    const double dy = static_cast<double>(point.y - m_center.y);
    const double radius = std::hypot(dx, dy);
    if (radius > m_outerRadius) return nullptr;
    if (radius <= m_centerRadius) return m_layoutRoot;
    if (m_ringWidth <= 0.0) return nullptr;

    const auto depth = std::min(
        static_cast<std::size_t>((radius - m_centerRadius) / m_ringWidth) + 1,
        m_rings.size() - 1);

    const double angle = NormalizeAngle(
        std::atan2(dy, dx) * RADIANS_TO_DEGREES + QUARTER_CIRCLE);

    const auto& ring = m_rings[depth];
    const auto found = std::ranges::upper_bound(ring, angle,
        std::ranges::less{}, &RingEntry::endAngle);
    if (found == ring.end() || angle < found->startAngle) return nullptr;
    return found->item;
}
