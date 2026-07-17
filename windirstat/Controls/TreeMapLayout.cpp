// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.

#include "pch.h"
#include "TreeMapLayout.h"
#include "HilbertMooreTreeMap.h"

namespace
{
    using TreeMapLayout::ChildRegion;
    using TreeMapLayout::Request;

    void ArrangeEqualRows(const Request& request, std::vector<ChildRegion>& regions)
    {
        const double width = static_cast<double>(request.bounds.Width()) / request.weights.size();
        double left = request.bounds.left;
        for (const std::size_t i : std::views::iota(std::size_t{ 0 }, request.weights.size()))
        {
            const double next = left + width;
            const int right = i + 1 == request.weights.size()
                ? request.bounds.right : static_cast<int>(next);
            regions[i] = { CRect(static_cast<int>(left), request.bounds.top,
                right, request.bounds.bottom), request.state };
            left = next;
        }
    }

    void ArrangeRows(const Request& request, std::vector<ChildRegion>& regions)
    {
        if (request.parentWeight == 0)
        {
            ArrangeEqualRows(request, regions);
            return;
        }

        static constexpr double MinProportion = 0.4;
        const bool horizontalRows = request.bounds.Width() >= request.bounds.Height();
        const double normalizedWidth = horizontalRows
            ? static_cast<double>(request.bounds.Width()) / request.bounds.Height()
            : static_cast<double>(request.bounds.Height()) / request.bounds.Width();
        const double rowOrigin = horizontalRows ? request.bounds.top : request.bounds.left;
        const int rowExtent = horizontalRows ? request.bounds.Height() : request.bounds.Width();
        const int columnExtent = horizontalRows ? request.bounds.Width() : request.bounds.Height();

        double top = rowOrigin;
        for (std::size_t rowBegin = 0; rowBegin < request.weights.size();)
        {
            ULONGLONG rowWeight = 0;
            double rowFraction = 0.0;
            std::size_t rowEnd = rowBegin;
            for (; rowEnd < request.weights.size(); ++rowEnd)
            {
                const ULONGLONG childWeight = request.weights[rowEnd];
                if (childWeight == 0) break;

                rowWeight += childWeight;
                const double candidateFraction = static_cast<double>(rowWeight) / request.parentWeight;
                const double childWidth = static_cast<double>(childWeight) / request.parentWeight
                    * normalizedWidth / candidateFraction;
                if (childWidth / candidateFraction < MinProportion)
                {
                    rowWeight -= childWeight;
                    break;
                }
                rowFraction = candidateFraction;
            }

            ASSERT(rowEnd > rowBegin);
            if (rowEnd == rowBegin) return;
            while (rowEnd < request.weights.size() && request.weights[rowEnd] == 0) ++rowEnd;

            const double nextTop = top + rowFraction * rowExtent;
            const int bottom = rowEnd == request.weights.size()
                ? (horizontalRows ? request.bounds.bottom : request.bounds.right)
                : static_cast<int>(nextTop);
            double left = horizontalRows ? request.bounds.left : request.bounds.top;
            for (std::size_t i = rowBegin; i < rowEnd; ++i)
            {
                const double nextLeft = left + static_cast<double>(request.weights[i])
                    / rowWeight * columnExtent;
                const bool lastChild = i + 1 == rowEnd
                    || (i + 1 < request.weights.size() && request.weights[i + 1] == 0);
                const int right = lastChild
                    ? (horizontalRows ? request.bounds.right : request.bounds.bottom)
                    : static_cast<int>(nextLeft);
                const CRect childBounds = horizontalRows
                    ? CRect(static_cast<int>(left), static_cast<int>(top), right, bottom)
                    : CRect(static_cast<int>(top), static_cast<int>(left), bottom, right);
                regions[i] = { childBounds, request.state };
                left = nextLeft;
            }

            top = nextTop;
            rowBegin = rowEnd;
        }
    }

    void ArrangeSquarified(const Request& request, std::vector<ChildRegion>& regions)
    {
        if (request.parentWeight == 0)
        {
            ArrangeEqualRows(request, regions);
            return;
        }

        CRect remaining = request.bounds;
        ULONGLONG remainingWeight = request.parentWeight;
        const double weightPerPixel = static_cast<double>(remainingWeight)
            / remaining.Width() / remaining.Height();
        std::size_t head = 0;
        while (head < request.weights.size())
        {
            ASSERT(remaining.Width() > 0 && remaining.Height() > 0);
            if (remaining.Width() <= 0 || remaining.Height() <= 0) break;

            const bool horizontal = remaining.Width() >= remaining.Height();
            const int rowThickness = horizontal ? remaining.Height() : remaining.Width();
            const double squaredRowWeight = rowThickness * rowThickness * weightPerPixel;
            ASSERT(squaredRowWeight > 0);

            const std::size_t rowBegin = head;
            std::size_t rowEnd = head;
            double worst = DBL_MAX;
            const ULONGLONG largest = request.weights[rowBegin];
            ULONGLONG rowWeight = 0;
            while (rowEnd < request.weights.size())
            {
                const ULONGLONG childWeight = request.weights[rowEnd];
                if (childWeight == 0)
                {
                    rowEnd = request.weights.size();
                    break;
                }

                const double nextWeight = static_cast<double>(rowWeight) + childWeight;
                const double squaredWeight = nextWeight * nextWeight;
                const double nextWorst = std::max(squaredRowWeight * largest / squaredWeight,
                    squaredWeight / squaredRowWeight / childWeight);
                if (nextWorst > worst) break;

                rowWeight += childWeight;
                ++rowEnd;
                worst = nextWorst;
            }
            if (rowWeight == 0) break;

            const int remainingExtent = horizontal ? remaining.Width() : remaining.Height();
            const int rowWidth = rowWeight < remainingWeight
                ? std::clamp(static_cast<int>(static_cast<double>(rowWeight)
                    / remainingWeight * remainingExtent), 1, remainingExtent)
                : remainingExtent;
            CRect rowBounds = remaining;
            if (horizontal) rowBounds.right = rowBounds.left + rowWidth;
            else rowBounds.bottom = rowBounds.top + rowWidth;

            double begin = horizontal ? rowBounds.top : rowBounds.left;
            for (std::size_t i = rowBegin; i < rowEnd; ++i)
            {
                const double next = begin + static_cast<double>(request.weights[i])
                    / rowWeight * (horizontal ? rowBounds.Height() : rowBounds.Width());
                const bool lastChild = i + 1 == rowEnd
                    || (i + 1 < request.weights.size() && request.weights[i + 1] == 0);
                const int end = lastChild
                    ? (horizontal ? rowBounds.bottom : rowBounds.right)
                    : static_cast<int>(next);
                const CRect childBounds = horizontal
                    ? CRect(rowBounds.left, static_cast<int>(begin), rowBounds.right, end)
                    : CRect(static_cast<int>(begin), rowBounds.top, end, rowBounds.bottom);
                regions[i] = { childBounds, request.state };
                begin = next;
            }

            (horizontal ? remaining.left : remaining.top) += rowWidth;
            remainingWeight -= rowWeight;
            head += rowEnd - rowBegin;
        }
    }
}

void TreeMapLayout::ArrangeChildren(const Request& request, std::vector<ChildRegion>& regions)
{
    ASSERT(std::ranges::is_sorted(request.weights, std::greater<>()));
    ASSERT(std::accumulate(request.weights.begin(), request.weights.end(), ULONGLONG{ 0 })
        == request.parentWeight);
    regions.assign(request.weights.size(), ChildRegion{ .state = request.state });
    if (request.weights.empty() || request.bounds.IsRectEmpty()) return;

    switch (request.style)
    {
    case Style::Rows:
        ArrangeRows(request, regions);
        break;
    case Style::Squarified:
        ArrangeSquarified(request, regions);
        break;
    case Style::Hilbert:
        HilbertMooreTreeMap::ArrangeChildren(request.weights, request.bounds,
            request.state, HilbertMooreTreeMap::Curve::Hilbert, regions);
        break;
    case Style::Moore:
        HilbertMooreTreeMap::ArrangeChildren(request.weights, request.bounds,
            request.state, HilbertMooreTreeMap::Curve::Moore, regions);
        break;
    }
}
