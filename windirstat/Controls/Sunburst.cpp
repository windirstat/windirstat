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
    constexpr double OUTER_MARGIN = 6.0;
    constexpr double MIN_CENTER_RADIUS = 12.0;
    constexpr double MIN_RING_WIDTH = 14.0;
    constexpr double MIN_HIT_ARC = 2.0;
    constexpr double MAX_LABEL_SEAM_OVERFLOW = 12.0;
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

    COLORREF ScaleColor(const COLORREF rgb, const double factor) noexcept
    {
        return RGB(
            std::clamp(static_cast<int>(std::lround(GetRValue(rgb) * factor)), 0, 255),
            std::clamp(static_cast<int>(std::lround(GetGValue(rgb) * factor)), 0, 255),
            std::clamp(static_cast<int>(std::lround(GetBValue(rgb) * factor)), 0, 255));
    }

    COLORREF GetBranchBaseColor(const std::size_t branch) noexcept
    {
        // A color-vision-safe palette whose hue is inherited by a whole branch.
        static constexpr std::array<COLORREF, 8> palette{
            RGB(79, 157, 232),  // blue
            RGB(71, 173, 128),  // bluish green
            RGB(230, 159, 0),   // orange
            RGB(213, 94, 0),    // vermilion
            RGB(168, 135, 224), // purple
            RGB(86, 180, 233),  // sky blue
            RGB(204, 121, 167), // reddish purple
            RGB(230, 205, 70),  // yellow
        };
        return palette[branch % palette.size()];
    }

    COLORREF GetBranchColor(const COLORREF branchColor, const int depth) noexcept
    {
        if (depth <= 0) return RGB(78, 86, 99);
        const double factor = 0.90 + static_cast<double>((depth - 1) % 4) * 0.06;
        return ScaleColor(branchColor, factor);
    }

    COLORREF GetContrastingMonochrome(const COLORREF color) noexcept
    {
        const double luminance = CColorSpace::GetRelativeLuminance(color);
        const double blackContrast = (luminance + 0.05) / 0.05;
        const double whiteContrast = 1.05 / (luminance + 0.05);
        return blackContrast >= whiteContrast ? RGB(0, 0, 0) : RGB(255, 255, 255);
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

    bool FindUprightLabelPlacement(const double sectorStart,
        const double sectorSweep, const double middleRadius,
        const double requiredArcLength, double& labelAngle,
        double& direction) noexcept
    {
        if (sectorSweep <= 0.0 || middleRadius <= 0.0 || requiredArcLength <= 0.0)
            return false;

        const double requiredSweep = requiredArcLength / middleRadius
            * RADIANS_TO_DEGREES;
        const double sectorEnd = sectorStart + sectorSweep;
        const double preferredAngle = sectorStart + sectorSweep / 2.0;
        double bestDistance = std::numeric_limits<double>::max();
        double bestSpan = 0.0;
        double largestUprightSpan = 0.0;
        bool found = false;

        const auto consider = [&](const double uprightStart,
            const double uprightEnd, const double candidateDirection)
        {
            const double start = std::max(sectorStart, uprightStart);
            const double end = std::min(sectorEnd, uprightEnd);
            const double span = end - start;
            largestUprightSpan = std::max(largestUprightSpan, span);
            if (span + 0.001 < requiredSweep) return;

            const double halfSweep = requiredSweep / 2.0;
            const double center = std::clamp(preferredAngle,
                start + halfSweep, end - halfSweep);
            const double distance = std::abs(center - preferredAngle);
            if (!found || distance < bestDistance
                || (distance == bestDistance && span > bestSpan))
            {
                found = true;
                bestDistance = distance;
                bestSpan = span;
                labelAngle = center;
                direction = candidateDirection;
            }
        };

        // Keep a whole word in one readable half-circle. If its sector crosses
        // 3 or 9 o'clock, shift the word into whichever side has room instead
        // of treating the sector midpoint as having zero available width.
        for (int turn = -1; turn <= 2; ++turn)
        {
            const double offset = static_cast<double>(turn) * FULL_CIRCLE;
            consider(-QUARTER_CIRCLE + offset, QUARTER_CIRCLE + offset, 1.0);
            consider(QUARTER_CIRCLE + offset,
                FULL_CIRCLE - QUARTER_CIRCLE + offset, -1.0);
        }
        if (!found && requiredSweep <= HALF_CIRCLE
            && sectorSweep + 0.001 >= requiredSweep
            && requiredSweep - largestUprightSpan <= MAX_LABEL_SEAM_OVERFLOW)
        {
            // A sector straddling an orientation boundary may not have enough
            // room wholly on either side even though the measured word fits.
            // Let it cross that seam, but never bend one label past a semicircle.
            labelAngle = preferredAngle;
            const double normalized = NormalizeAngle(labelAngle);
            direction = normalized > QUARTER_CIRCLE
                && normalized < FULL_CIRCLE - QUARTER_CIRCLE ? -1.0 : 1.0;
            found = true;
        }
        return found;
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
    const int dpiX = std::max(1, pdc->GetDeviceCaps(LOGPIXELSX));
    const int dpiY = std::max(1, pdc->GetDeviceCaps(LOGPIXELSY));
    if (m_layoutRoot != root || m_renderArea != rc || m_maxDepth != boundedMaxDepth
        || m_dpiX != dpiX || m_dpiY != dpiY)
    {
        BuildLayout(root, rc, boundedMaxDepth, dpiX, dpiY);
    }
    RenderLayout(pdc);
}

void CSunburst::BuildLayout(CItem* root, const CRect& rc, const int maxDepth,
    const int dpiX, const int dpiY)
{
    ClearLayout();
    m_layoutRoot = root;
    m_renderArea = rc;
    m_maxDepth = std::clamp(maxDepth, 1, MAX_DEPTH);
    m_dpiX = dpiX;
    m_dpiY = dpiY;
    m_dpiScale = (static_cast<double>(dpiX) + dpiY)
        / (2.0 * USER_DEFAULT_SCREEN_DPI);
    m_separatorWidth = static_cast<float>(std::max(1.0, m_dpiScale));
    m_center = rc.CenterPoint();
    m_outerRadius = std::max(0.0,
        static_cast<double>(std::min(rc.Width(), rc.Height())) / 2.0
        - OUTER_MARGIN * m_dpiScale);
    if (m_outerRadius < 2.0 * m_dpiScale) return;

    m_centerRadius = std::min(m_outerRadius,
        std::max(MIN_CENTER_RADIUS * m_dpiScale, m_outerRadius * 0.20));
    const double availableRadius = std::max(0.0, m_outerRadius - m_centerRadius);
    const int depthLimit = std::min(
        static_cast<int>(availableRadius / (MIN_RING_WIDTH * m_dpiScale)), m_maxDepth);
    const double provisionalRingWidth = depthLimit > 0
        ? availableRadius / static_cast<double>(depthLimit)
        : 0.0;
    const double minArcPixels = std::max(
        MIN_HIT_ARC * m_dpiScale, static_cast<double>(m_separatorWidth) * 2.0);

    struct PendingItem
    {
        CItem* item;
        double startAngle;
        double sweepAngle;
        int depth;
        COLORREF branchColor;
        ULONGLONG remainderSize;
    };

    std::vector<PendingItem> pending;
    pending.reserve(256);
    pending.push_back({ root, 0.0, FULL_CIRCLE, 0, RGB(78, 86, 99), 0 });
    std::vector<PendingItem> children;
    children.reserve(256);
    int actualMaxDepth = 0;

    while (!pending.empty())
    {
        const PendingItem current = pending.back();
        pending.pop_back();
        if (current.item == nullptr || current.sweepAngle <= 0.0) continue;

        const std::size_t entryIndex = m_entries.size();
        m_entries.push_back({ current.item, current.startAngle,
            current.sweepAngle, 0.0, 0.0, current.remainderSize, current.depth,
            current.branchColor, true });
        actualMaxDepth = std::max(actualMaxDepth, current.depth);

        if (current.remainderSize != 0 || current.depth >= depthLimit || current.item->TmiIsLeaf()
            || current.item->TmiGetSize() == 0)
        {
            continue;
        }

        const ULONGLONG totalSize = current.item->TmiGetSize();
        ULONGLONG placedSize = 0;
        children.clear();
        const auto& itemChildren = current.item->GetChildren();
        std::size_t branchOrdinal = 0;
        double nextStart = current.startAngle;
        const double parentEnd = current.startAngle + current.sweepAngle;

        for (CItem* child : itemChildren)
        {
            const ULONGLONG childSize = child->TmiGetSize();
            if (childSize == 0 || placedSize >= totalSize) break;

            const ULONGLONG remaining = totalSize - placedSize;
            const ULONGLONG boundedSize = std::min(childSize, remaining);
            const double childSweep = current.sweepAngle
                * static_cast<double>(boundedSize) / static_cast<double>(totalSize);

            // Evaluate visibility where this child is actually drawn. Sorted children
            // after the first sector narrower than the stroke/hit target are omitted.
            const double targetRadius = m_centerRadius
                + (static_cast<double>(current.depth) + 0.5) * provisionalRingWidth;
            const double arcPixels = childSweep * DEGREES_TO_RADIANS * targetRadius;
            if (arcPixels < minArcPixels) break;

            placedSize += boundedSize;
            const COLORREF branchColor = current.depth == 0
                ? GetBranchBaseColor(branchOrdinal)
                : current.branchColor;
            const double childEnd = current.startAngle + current.sweepAngle
                * static_cast<double>(placedSize) / static_cast<double>(totalSize);
            children.push_back({ child, nextStart,
                std::max(0.0, childEnd - nextStart), current.depth + 1,
                branchColor, 0 });
            nextStart = childEnd;
            ++branchOrdinal;
        }

        // Show omitted children as a muted residual sector that preserves the parent's proportions.
        const double remainderSweep = std::max(0.0, parentEnd - nextStart);
        if (placedSize < totalSize && remainderSweep > 0.0)
        {
            children.push_back({ current.item, nextStart, remainderSweep, current.depth + 1,
                current.branchColor, totalSize - placedSize });
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
    m_remainderEntries.reserve(m_entries.size() / 4);

    for (std::size_t index = 0; index < m_entries.size(); ++index)
    {
        LayoutEntry& entry = m_entries[index];
        if (entry.depth == 0)
        {
            entry.innerRadius = 0.0;
            entry.outerRadius = entry.visualLeaf ? m_outerRadius : m_centerRadius;
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
                index, entry.startAngle, entry.startAngle + entry.sweepAngle });
        }
        (entry.remainderSize != 0 ? m_remainderEntries : m_itemEntries).emplace(entry.item, index);
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
    m_dpiScale = 1.0;
    m_separatorWidth = 1.0f;
    m_dpiX = USER_DEFAULT_SCREEN_DPI;
    m_dpiY = USER_DEFAULT_SCREEN_DPI;
    m_maxDepth = 0;
    m_entries.clear();
    m_itemEntries.clear();
    m_remainderEntries.clear();
    m_rings.clear();
}

void CSunburst::TrimMemory()
{
    ClearLayout();
    decltype(m_entries){}.swap(m_entries);
    decltype(m_itemEntries){}.swap(m_itemEntries);
    decltype(m_remainderEntries){}.swap(m_remainderEntries);
    decltype(m_rings){}.swap(m_rings);
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

COLORREF CSunburst::GetItemColor(const LayoutEntry& entry) const
{
    if (entry.remainderSize != 0) return RGB(92, 96, 104);

    const DWORD rawColor = entry.item->TmiGetGraphColor();
    const DWORD colorFlags = rawColor & CTreeMap::COLORFLAG_MASK;
    COLORREF color = rawColor & 0x00FFFFFF;

    if (color == RGB(0, 0, 0) && entry.item->IsTypeOrFlag(IT_DIRECTORY)
        && !entry.item->IsTypeOrFlag(IT_FREESPACE, IT_UNKNOWN))
    {
        color = GetBranchColor(entry.branchColor, entry.depth);
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

    return color;
}

void CSunburst::RenderEntry(Gdiplus::Graphics& graphics, const LayoutEntry& entry,
    Gdiplus::SolidBrush& brush, Gdiplus::Pen& separator) const
{
    Gdiplus::GraphicsPath path(Gdiplus::FillModeAlternate);
    CreatePath(entry, path);
    const COLORREF color = GetItemColor(entry);
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
    Gdiplus::Pen separator(Gdiplus::Color{}, m_separatorWidth);
    separator.SetAlignment(Gdiplus::PenAlignmentInset);
    for (const LayoutEntry& entry : m_entries)
    {
        RenderEntry(graphics, entry, brush, separator);
    }

    const std::wstring& fontFamilyName = GetUiFontFamilyName();
    Gdiplus::FontFamily uiFontFamily(fontFamilyName.c_str());
    const Gdiplus::FontFamily* fontFamily = SelectUiFontFamily(uiFontFamily);
    std::vector<Gdiplus::PointF> points;
    std::vector<BYTE> types;
    struct LabelCandidate
    {
        double priority;
        const LayoutEntry* entry;
    };
    std::vector<LabelCandidate> directoryLabels;
    std::vector<LabelCandidate> leafLabels;
    directoryLabels.reserve(std::min<std::size_t>(m_entries.size(), 128));
    leafLabels.reserve(std::min<std::size_t>(m_entries.size(), 256));
    for (const LayoutEntry& entry : m_entries)
    {
        const double priority = GetLabelPriority(entry);
        if (priority <= 0.0) continue;

        const bool directory = entry.depth == 0
            || entry.item->IsTypeOrFlag(IT_DIRECTORY)
            || !entry.item->TmiIsLeaf();
        (directory ? directoryLabels : leafLabels).push_back({ priority, &entry });
    }

    const auto byPriority = [](const LabelCandidate& left, const LabelCandidate& right)
    {
        if (left.priority != right.priority) return left.priority > right.priority;
        if (left.entry->depth != right.entry->depth)
            return left.entry->depth < right.entry->depth;
        return left.entry->startAngle < right.entry->startAngle;
    };
    const double logicalRadius = m_outerRadius / std::max(0.01, m_dpiScale);
    const auto labelBudget = static_cast<std::size_t>(std::clamp(
        std::numbers::pi_v<double> * logicalRadius * logicalRadius / 2800.0,
        8.0, 128.0));

    // Directory context is the main navigational value of a sunburst, so never
    // discard it because file labels filled a global budget. Exact-fit testing
    // still rejects names that cannot be drawn inside their own sectors.
    std::size_t drawnLabels = 0;
    for (const LabelCandidate& candidate : directoryLabels)
    {
        if (RenderLabel(graphics, fontFamily, *candidate.entry, points, types))
            ++drawnLabels;
    }
    if (drawnLabels >= labelBudget) return;

    // Sorting filename candidates is unnecessary when directory context has
    // already filled the density budget.
    std::ranges::sort(leafLabels, byPriority);
    std::size_t remainingLeafLabels = labelBudget - drawnLabels;
    for (const LabelCandidate& candidate : leafLabels)
    {
        if (remainingLeafLabels == 0) break;
        if (RenderLabel(graphics, fontFamily, *candidate.entry, points, types))
            --remainingLeafLabels;
    }
}

double CSunburst::GetLabelPriority(const LayoutEntry& entry) const
{
    if (entry.item->IsTypeOrFlag(IT_FREESPACE, IT_UNKNOWN))
        return 0.0;

    const double dpiScale = static_cast<double>(m_dpiY) / USER_DEFAULT_SCREEN_DPI;
    if (entry.depth == 0)
    {
        const double half = entry.outerRadius * 0.70;
        return half >= 14.0 * dpiScale
            ? std::numeric_limits<double>::max()
            : 0.0;
    }

    const double thickness = entry.outerRadius - entry.innerRadius;
    const double middleRadius = std::midpoint(entry.innerRadius, entry.outerRadius);
    const double arcLength = entry.sweepAngle * DEGREES_TO_RADIANS * middleRadius;
    const double fontSize = std::clamp(thickness * 0.44,
        8.0 * dpiScale, 14.0 * dpiScale);
    if (thickness < fontSize * 1.35) return 0.0;

    // Exact glyph bounds and upright placement are checked only for candidates
    // that reach RenderLabel. Avoid coarse radial/angle/em gates here: they can
    // reject short names, curved fallbacks, and sectors at 3 or 9 o'clock.
    if (arcLength <= 0.0) return 0.0;

    return entry.sweepAngle * DEGREES_TO_RADIANS
        * (entry.outerRadius * entry.outerRadius
            - entry.innerRadius * entry.innerRadius) / 2.0;
}

bool CSunburst::RenderLabel(Gdiplus::Graphics& graphics,
    const Gdiplus::FontFamily* fontFamily, const LayoutEntry& entry,
    std::vector<Gdiplus::PointF>& points, std::vector<BYTE>& types) const
{
    if (fontFamily == nullptr || GetLabelPriority(entry) <= 0.0) return false;

    const std::wstring_view name = entry.remainderSize != 0
        ? std::wstring_view{ L"\u2026" } : entry.item->GetNameView(true);
    if (name.empty()) return false;

    const Gdiplus::REAL dpiScale = static_cast<Gdiplus::REAL>(m_dpiY)
        / USER_DEFAULT_SCREEN_DPI;
    const COLORREF itemColor = GetItemColor(entry);
    Gdiplus::SolidBrush textBrush(ToGdiColor(GetContrastingMonochrome(itemColor)));
    if (entry.depth == 0)
    {
        const auto half = static_cast<Gdiplus::REAL>(entry.outerRadius * 0.70);
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
        if (measured.Width > half * 2.0f || measured.Height > half * 2.0f)
            return false;

        const Gdiplus::RectF textRect(
            static_cast<Gdiplus::REAL>(m_center.x) - half,
            static_cast<Gdiplus::REAL>(m_center.y) - half,
            half * 2.0f, half * 2.0f);
        return graphics.DrawString(name.data(), static_cast<int>(name.size()), &font,
            textRect, &format, &textBrush) == Gdiplus::Ok;
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
    const bool spansTwoLayers = m_ringWidth > 0.0
        && thickness + 0.01 >= m_ringWidth * 2.0;

    if (entry.visualLeaf && spansTwoLayers)
    {
        const Gdiplus::REAL padding = std::max(2.0f * dpiScale, fontSize * 0.20f);
        const auto radialLength = static_cast<Gdiplus::REAL>(thickness) - padding * 2.0f;
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
        if (graphics.MeasureString(name.data(), static_cast<int>(name.size()), &font,
                Gdiplus::PointF{}, &format, &measured) == Gdiplus::Ok
            && measured.Width <= radialLength && measured.Height <= arcLength)
        {
            const Gdiplus::GraphicsState state = graphics.Save();
            graphics.TranslateTransform(startX, startY);
            graphics.RotateTransform(rotation);
            const Gdiplus::Status status = graphics.DrawString(name.data(),
                static_cast<int>(name.size()), &font,
                Gdiplus::PointF(-measured.X, -measured.Y - measured.Height / 2.0f),
                &format, &textBrush);
            graphics.Restore(state);
            if (status == Gdiplus::Ok) return true;
        }
        // A multi-ring visual leaf usually reads best radially, but a long name
        // can still fit comfortably around its arc. Fall through to curved text.
    }

    const Gdiplus::REAL sidePadding = std::max(4.0f * dpiScale, fontSize * 0.45f);
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
        return false;
    }

    const int pointCount = textPath.GetPointCount();
    if (pointCount <= 0) return false;

    Gdiplus::RectF bounds;
    if (textPath.GetBounds(&bounds) != Gdiplus::Ok || bounds.IsEmptyArea())
        return false;
    if (bounds.Height > thickness * 0.80) return false;

    double labelAngle = middleAngle;
    double direction = 1.0;
    if (!FindUprightLabelPlacement(entry.startAngle, entry.sweepAngle,
        middleRadius, bounds.Width + sidePadding * 2.0,
        labelAngle, direction))
    {
        return false;
    }

    points.resize(static_cast<std::size_t>(pointCount));
    types.resize(static_cast<std::size_t>(pointCount));
    if (textPath.GetPathPoints(points.data(), pointCount) != Gdiplus::Ok
        || textPath.GetPathTypes(types.data(), pointCount) != Gdiplus::Ok)
    {
        return false;
    }

    // Bend the shaped outline around the ring. Normal placement stays inside one
    // upright half-circle; the bounded seam fallback may cross slightly beyond it.
    const double textCenterX = bounds.X + bounds.Width / 2.0;
    const double textCenterY = bounds.Y + bounds.Height / 2.0;
    const double radialScale = std::min(1.0, thickness * 0.80 / bounds.Height);
    for (Gdiplus::PointF& point : points)
    {
        const double glyphAngle = labelAngle + direction
            * (point.X - textCenterX) / middleRadius * RADIANS_TO_DEGREES;
        const double radius = middleRadius - direction
            * (point.Y - textCenterY) * radialScale;
        const double screenAngle = (glyphAngle - QUARTER_CIRCLE) * DEGREES_TO_RADIANS;
        point.X = static_cast<Gdiplus::REAL>(m_center.x + std::cos(screenAngle) * radius);
        point.Y = static_cast<Gdiplus::REAL>(m_center.y + std::sin(screenAngle) * radius);
    }

    Gdiplus::GraphicsPath curvedPath(points.data(), types.data(), pointCount,
        textPath.GetFillMode());
    return graphics.FillPath(&textBrush, &curvedPath) == Gdiplus::Ok;
}

const CSunburst::LayoutEntry* CSunburst::FindLayoutEntry(const CItem* item) const
{
    const auto found = m_itemEntries.find(item);
    if (found != m_itemEntries.end()) return &m_entries[found->second];

    for (item = item == nullptr ? nullptr : item->GetParent(); item != nullptr;
        item = item->GetParent())
    {
        const auto remainder = m_remainderEntries.find(item);
        if (remainder != m_remainderEntries.end()) return &m_entries[remainder->second];
        const auto ancestor = m_itemEntries.find(item);
        if (ancestor != m_itemEntries.end()) return &m_entries[ancestor->second];
    }
    return nullptr;
}

void CSunburst::DrawOutlineItems(CDC* pdc, const std::span<const CItem* const> items,
    const COLORREF color, const float width) const
{
    if (pdc == nullptr || items.empty()) return;

    Gdiplus::Graphics graphics(pdc->GetSafeHdc());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    const float outlineWidth = std::max(
        m_separatorWidth, width * static_cast<float>(m_dpiScale));
    Gdiplus::Pen pen(ToGdiColor(color), outlineWidth);
    pen.SetAlignment(Gdiplus::PenAlignmentInset);

    std::unordered_set<const LayoutEntry*> drawnEntries;
    drawnEntries.reserve(items.size() * 2);
    const auto drawEntry = [&](const LayoutEntry* entry)
    {
        if (entry == nullptr || !drawnEntries.emplace(entry).second) return;
        Gdiplus::GraphicsPath path(Gdiplus::FillModeAlternate);
        CreatePath(*entry, path);
        graphics.DrawPath(&pen, &path);
    };

    for (const CItem* item : items)
    {
        drawEntry(FindLayoutEntry(item));
        const auto remainder = m_remainderEntries.find(item);
        if (remainder != m_remainderEntries.end()) drawEntry(&m_entries[remainder->second]);
    }
}

CItem* CSunburst::FindItemByPoint(const CPoint point, ULONGLONG* remainderSize) const
{
    if (remainderSize != nullptr) *remainderSize = 0;
    if (m_entries.empty() || !m_renderArea.PtInRect(point)) return nullptr;

    const double dx = static_cast<double>(point.x - m_center.x);
    const double dy = static_cast<double>(point.y - m_center.y);
    const double radius = std::hypot(dx, dy);
    if (radius > m_outerRadius) return nullptr;
    if (radius <= m_centerRadius) return m_layoutRoot;
    if (m_ringWidth <= 0.0) return m_layoutRoot;

    const auto depth = std::min(
        static_cast<std::size_t>((radius - m_centerRadius) / m_ringWidth) + 1,
        m_rings.size() - 1);

    const double angle = NormalizeAngle(
        std::atan2(dy, dx) * RADIANS_TO_DEGREES + QUARTER_CIRCLE);

    const auto& ring = m_rings[depth];
    const auto found = std::ranges::upper_bound(ring, angle,
        std::ranges::less{}, &RingEntry::endAngle);
    if (found == ring.end() || angle < found->startAngle) return nullptr;
    const LayoutEntry& entry = m_entries[found->entryIndex];
    if (remainderSize != nullptr && entry.remainderSize != 0) *remainderSize = entry.remainderSize;
    return entry.item;
}
