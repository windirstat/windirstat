// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.

#include "pch.h"
#include "HilbertMooreTreeMap.h"

// The partition templates and orientation rules are adapted from the Hilbert
// and Moore Treemap Layouts Prototype by the Visual Analytics Research Group
// at the Hasso Plattner Institute:
// https://github.com/varg-dev/hilbert-moore-treemap-layouts-prototype
//
// MIT License
//
// Copyright (c) 2021 Visual Analytics Research Group at the Chair for Computer
// Graphics Systems at the Hasso Plattner Institute, Potsdam
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

namespace
{
    using HilbertMooreTreeMap::Curve;
    using TreeMapLayout::Direction;
    using TreeMapLayout::Rotation;
    using TreeMapLayout::State;

    struct Region
    {
        double left = 0.0;
        double bottom = 0.0;
        double width = 0.0;
        double height = 0.0;
        State orientation;

        [[nodiscard]] double AspectRatio() const
        {
            const double shorter = std::min(width, height);
            return shorter > 0.0 ? std::max(width, height) / shorter
                : std::numeric_limits<double>::infinity();
        }

        void Rotate(const Rotation rotation)
        {
            orientation.rotation = static_cast<Rotation>((static_cast<unsigned int>(orientation.rotation)
                + static_cast<unsigned int>(rotation)) % 4);
        }

        void ReverseDirection()
        {
            orientation.direction = orientation.direction == Direction::Clockwise
                ? Direction::Counterclockwise : Direction::Clockwise;
        }

        [[nodiscard]] std::pair<Region, Region> SplitHorizontal(double fraction) const;
        [[nodiscard]] std::pair<Region, Region> SplitVertical(double fraction) const;

    private:
        [[nodiscard]] std::pair<Region, Region> SplitX(double fraction) const
        {
            fraction = std::clamp(fraction, 0.0, 1.0);
            const double splitWidth = width * fraction;
            return {
                { left, bottom, splitWidth, height, orientation },
                { left + splitWidth, bottom, width - splitWidth, height, orientation },
            };
        }

        [[nodiscard]] std::pair<Region, Region> SplitY(double fraction) const
        {
            fraction = std::clamp(fraction, 0.0, 1.0);
            const double splitHeight = height * fraction;
            return {
                { left, bottom, width, splitHeight, orientation },
                { left, bottom + splitHeight, width, height - splitHeight, orientation },
            };
        }
    };

    std::pair<Region, Region> Region::SplitHorizontal(const double fraction) const
    {
        switch (orientation.rotation)
        {
        case Rotation::None:
            return SplitX(fraction);
        case Rotation::Clockwise90:
            return SplitY(fraction);
        case Rotation::Half:
        {
            auto [first, second] = SplitX(1.0 - fraction);
            return { second, first };
        }
        case Rotation::Counterclockwise90:
        {
            auto [first, second] = SplitY(1.0 - fraction);
            return { second, first };
        }
        }
        ASSERT(false);
        return SplitX(fraction);
    }

    std::pair<Region, Region> Region::SplitVertical(const double fraction) const
    {
        switch (orientation.rotation)
        {
        case Rotation::None:
            return SplitY(fraction);
        case Rotation::Clockwise90:
        {
            auto [first, second] = SplitX(1.0 - fraction);
            return { second, first };
        }
        case Rotation::Half:
        {
            auto [first, second] = SplitY(1.0 - fraction);
            return { second, first };
        }
        case Rotation::Counterclockwise90:
            return SplitX(fraction);
        }
        ASSERT(false);
        return SplitY(fraction);
    }

    enum class Pattern : std::uint8_t
    {
        Identity,
        Hilbert2,
        Moore2,
        Snake3,
        First3,
        Middle3,
        Last3,
        Snake4,
        First4,
        MiddleLeft4,
        MiddleRight4,
        Last4,
        Horizontal4,
        Vertical4,
    };

    struct Layout
    {
        std::array<Region, 4> regions;
        std::size_t count = 0;
    };

    [[nodiscard]] double Fraction(const double numerator, const double denominator)
    {
        return denominator > 0.0 ? numerator / denominator : 0.0;
    }

    [[nodiscard]] Layout Dissect(const Pattern pattern, const std::span<const double> weights,
        const Region& rectangle)
    {
        Layout layout{ .count = weights.size() };
        auto& result = layout.regions;

        switch (pattern)
        {
        case Pattern::Identity:
            result[0] = rectangle;
            break;

        case Pattern::Hilbert2:
            std::tie(result[0], result[1]) = rectangle.SplitHorizontal(
                Fraction(weights[0], weights[0] + weights[1]));
            break;

        case Pattern::Moore2:
            std::tie(result[0], result[1]) = rectangle.SplitHorizontal(
                Fraction(weights[0], weights[0] + weights[1]));
            result[0].Rotate(Rotation::Clockwise90);
            result[1].Rotate(Rotation::Counterclockwise90);
            break;

        case Pattern::Snake3:
        {
            Region remaining;
            std::tie(result[0], remaining) = rectangle.SplitHorizontal(
                Fraction(weights[0], weights[0] + weights[1] + weights[2]));
            std::tie(result[1], result[2]) = remaining.SplitHorizontal(
                Fraction(weights[1], weights[1] + weights[2]));
            break;
        }

        case Pattern::First3:
        {
            Region remaining;
            std::tie(result[0], remaining) = rectangle.SplitHorizontal(
                Fraction(weights[0], weights[0] + weights[1] + weights[2]));
            std::tie(result[2], result[1]) = remaining.SplitVertical(
                Fraction(weights[2], weights[1] + weights[2]));
            result[0].Rotate(Rotation::Clockwise90);
            result[1].Rotate(Rotation::Counterclockwise90);
            result[2].Rotate(Rotation::Counterclockwise90);
            break;
        }

        case Pattern::Middle3:
        {
            Region remaining;
            std::tie(remaining, result[1]) = rectangle.SplitVertical(
                Fraction(weights[0] + weights[2], weights[0] + weights[1] + weights[2]));
            std::tie(result[0], result[2]) = remaining.SplitHorizontal(
                Fraction(weights[0], weights[0] + weights[2]));
            result[0].Rotate(Rotation::Counterclockwise90);
            result[0].ReverseDirection();
            result[2].Rotate(Rotation::Clockwise90);
            result[2].ReverseDirection();
            break;
        }

        case Pattern::Last3:
        {
            Region remaining;
            std::tie(remaining, result[2]) = rectangle.SplitHorizontal(
                Fraction(weights[0] + weights[1], weights[0] + weights[1] + weights[2]));
            std::tie(result[0], result[1]) = remaining.SplitVertical(
                Fraction(weights[0], weights[0] + weights[1]));
            result[0].Rotate(Rotation::Clockwise90);
            result[1].Rotate(Rotation::Clockwise90);
            result[2].Rotate(Rotation::Counterclockwise90);
            break;
        }

        case Pattern::Snake4:
        {
            Region firstHalf;
            Region secondHalf;
            const double firstWeight = weights[0] + weights[1];
            const double secondWeight = weights[2] + weights[3];
            std::tie(firstHalf, secondHalf) = rectangle.SplitHorizontal(
                Fraction(firstWeight, firstWeight + secondWeight));
            std::tie(result[0], result[1]) = firstHalf.SplitHorizontal(
                Fraction(weights[0], firstWeight));
            std::tie(result[2], result[3]) = secondHalf.SplitHorizontal(
                Fraction(weights[2], secondWeight));
            break;
        }

        case Pattern::First4:
        {
            Region remaining;
            Region opposite;
            const double total = std::accumulate(weights.begin(), weights.end(), 0.0);
            std::tie(result[0], remaining) = rectangle.SplitHorizontal(Fraction(weights[0], total));
            std::tie(opposite, result[1]) = remaining.SplitVertical(
                Fraction(weights[2] + weights[3], weights[1] + weights[2] + weights[3]));
            std::tie(result[3], result[2]) = opposite.SplitVertical(
                Fraction(weights[3], weights[2] + weights[3]));
            result[0].Rotate(Rotation::Clockwise90);
            result[1].Rotate(Rotation::Counterclockwise90);
            result[2].Rotate(Rotation::Counterclockwise90);
            result[3].Rotate(Rotation::Counterclockwise90);
            break;
        }

        case Pattern::MiddleLeft4:
        {
            Region remaining;
            Region opposite;
            const double total = std::accumulate(weights.begin(), weights.end(), 0.0);
            std::tie(remaining, result[1]) = rectangle.SplitVertical(
                Fraction(weights[0] + weights[2] + weights[3], total));
            std::tie(result[0], opposite) = remaining.SplitHorizontal(
                Fraction(weights[0], weights[0] + weights[2] + weights[3]));
            std::tie(result[3], result[2]) = opposite.SplitVertical(
                Fraction(weights[3], weights[2] + weights[3]));
            result[0].Rotate(Rotation::Counterclockwise90);
            result[0].ReverseDirection();
            result[2].Rotate(Rotation::Clockwise90);
            result[2].ReverseDirection();
            result[3].Rotate(Rotation::Clockwise90);
            result[3].ReverseDirection();
            break;
        }

        case Pattern::MiddleRight4:
        {
            Region remaining;
            Region opposite;
            const double total = std::accumulate(weights.begin(), weights.end(), 0.0);
            std::tie(remaining, result[2]) = rectangle.SplitVertical(
                Fraction(weights[0] + weights[1] + weights[3], total));
            std::tie(opposite, result[3]) = remaining.SplitHorizontal(
                Fraction(weights[0] + weights[1], weights[0] + weights[1] + weights[3]));
            std::tie(result[0], result[1]) = opposite.SplitVertical(
                Fraction(weights[0], weights[0] + weights[1]));
            result[0].Rotate(Rotation::Counterclockwise90);
            result[0].ReverseDirection();
            result[1].Rotate(Rotation::Counterclockwise90);
            result[1].ReverseDirection();
            result[3].Rotate(Rotation::Clockwise90);
            result[3].ReverseDirection();
            break;
        }

        case Pattern::Last4:
        {
            Region remaining;
            Region opposite;
            const double total = std::accumulate(weights.begin(), weights.end(), 0.0);
            std::tie(remaining, result[3]) = rectangle.SplitHorizontal(
                Fraction(weights[0] + weights[1] + weights[2], total));
            std::tie(opposite, result[2]) = remaining.SplitVertical(
                Fraction(weights[0] + weights[1], weights[0] + weights[1] + weights[2]));
            std::tie(result[0], result[1]) = opposite.SplitVertical(
                Fraction(weights[0], weights[0] + weights[1]));
            result[0].Rotate(Rotation::Clockwise90);
            result[1].Rotate(Rotation::Clockwise90);
            result[2].Rotate(Rotation::Clockwise90);
            result[3].Rotate(Rotation::Counterclockwise90);
            break;
        }

        case Pattern::Horizontal4:
        {
            Region firstHalf;
            Region secondHalf;
            const double middleWeight = weights[1] + weights[2];
            const double outsideWeight = weights[0] + weights[3];
            std::tie(firstHalf, secondHalf) = rectangle.SplitVertical(
                Fraction(outsideWeight, outsideWeight + middleWeight));
            std::tie(result[0], result[3]) = firstHalf.SplitHorizontal(
                Fraction(weights[0], outsideWeight));
            std::tie(result[1], result[2]) = secondHalf.SplitHorizontal(
                Fraction(weights[1], middleWeight));
            result[0].Rotate(Rotation::Counterclockwise90);
            result[0].ReverseDirection();
            result[3].Rotate(Rotation::Clockwise90);
            result[3].ReverseDirection();
            break;
        }

        case Pattern::Vertical4:
        {
            Region firstHalf;
            Region secondHalf;
            const double firstWeight = weights[0] + weights[1];
            const double secondWeight = weights[2] + weights[3];
            std::tie(firstHalf, secondHalf) = rectangle.SplitHorizontal(
                Fraction(firstWeight, firstWeight + secondWeight));
            std::tie(result[0], result[1]) = firstHalf.SplitVertical(
                Fraction(weights[0], firstWeight));
            std::tie(result[3], result[2]) = secondHalf.SplitVertical(
                Fraction(weights[3], secondWeight));
            result[0].Rotate(Rotation::Clockwise90);
            result[1].Rotate(Rotation::Clockwise90);
            result[2].Rotate(Rotation::Counterclockwise90);
            result[3].Rotate(Rotation::Counterclockwise90);
            break;
        }
        }
        return layout;
    }

    [[nodiscard]] double AverageAspectRatio(const Layout& layout)
    {
        const auto regions = std::span(layout.regions).first(layout.count);
        return std::accumulate(regions.begin(), regions.end(), 0.0,
            [](const double sum, const Region& region) { return sum + region.AspectRatio(); })
            / layout.count;
    }

    [[nodiscard]] Layout SelectLayout(const std::span<const double> weights,
        const Region& rectangle, const Curve curve)
    {
        static constexpr std::array identity{ Pattern::Identity };
        static constexpr std::array hilbert2{ Pattern::Hilbert2 };
        static constexpr std::array moore2{ Pattern::Moore2 };
        static constexpr std::array hilbert3{ Pattern::Snake3, Pattern::Middle3 };
        static constexpr std::array moore3{ Pattern::First3, Pattern::Last3 };
        static constexpr std::array hilbert4{
            Pattern::Snake4, Pattern::MiddleLeft4, Pattern::MiddleRight4, Pattern::Horizontal4
        };
        static constexpr std::array moore4{ Pattern::First4, Pattern::Last4, Pattern::Vertical4 };

        std::span<const Pattern> patterns;
        switch (weights.size())
        {
        case 1: patterns = identity; break;
        case 2:
            patterns = curve == Curve::Hilbert
                ? std::span<const Pattern>(hilbert2) : std::span<const Pattern>(moore2);
            break;
        case 3:
            patterns = curve == Curve::Hilbert
                ? std::span<const Pattern>(hilbert3) : std::span<const Pattern>(moore3);
            break;
        case 4:
            patterns = curve == Curve::Hilbert
                ? std::span<const Pattern>(hilbert4) : std::span<const Pattern>(moore4);
            break;
        default:
            ASSERT(false);
            patterns = identity;
            break;
        }

        Layout best = Dissect(patterns.front(), weights, rectangle);
        double bestDifference = std::abs(AverageAspectRatio(best) - 1.0);
        for (const Pattern pattern : patterns.subspan(1))
        {
            Layout candidate = Dissect(pattern, weights, rectangle);
            const double difference = std::abs(AverageAspectRatio(candidate) - 1.0);
            if (difference < bestDifference)
            {
                best = std::move(candidate);
                bestDifference = difference;
            }
        }
        return best;
    }

    struct Range
    {
        std::size_t begin = 0;
        std::size_t end = 0;
    };

    [[nodiscard]] double RangeWeight(const std::span<const double> prefix, const Range range)
    {
        return prefix[range.end] - prefix[range.begin];
    }

    [[nodiscard]] std::array<Range, 4> PartitionByVariance(const Range range,
        const std::span<const double> prefix)
    {
        const double mean = RangeWeight(prefix, range) / 4.0;
        const auto imbalance = [prefix](const std::size_t begin, const std::size_t middle,
            const std::size_t end)
        {
            return std::abs(prefix[middle] - (prefix[end] + prefix[begin]) / 2.0);
        };

        std::size_t left = range.begin + 1;
        std::size_t right = range.begin + 3;
        double bestVariance = std::numeric_limits<double>::infinity();
        std::array<Range, 4> best{};

        for (std::size_t middle = range.begin + 2; middle < range.end - 1; ++middle)
        {
            left = std::min(left, middle - 1);
            right = std::max(right, middle + 1);
            while (left + 1 < middle
                && imbalance(range.begin, left + 1, middle) < imbalance(range.begin, left, middle))
            {
                ++left;
            }
            while (right + 1 < range.end
                && imbalance(middle, right + 1, range.end) < imbalance(middle, right, range.end))
            {
                ++right;
            }

            const std::array candidate{
                Range{ range.begin, left }, Range{ left, middle },
                Range{ middle, right }, Range{ right, range.end },
            };
            const double variance = std::accumulate(candidate.begin(), candidate.end(), 0.0,
                [prefix, mean](const double sum, const Range part)
                {
                    const double difference = RangeWeight(prefix, part) - mean;
                    return sum + difference * difference;
                });
            if (variance < bestVariance)
            {
                best = candidate;
                bestVariance = variance;
            }
        }
        return best;
    }

    void LayoutRange(const Region& rectangle, const std::span<const double> prefix,
        const Range range, const Curve curve, std::span<Region> output)
    {
        const std::size_t count = range.end - range.begin;
        if (count == 1)
        {
            output[range.begin] = rectangle;
            return;
        }

        std::array<Range, 4> parts{};
        std::size_t partCount = count;
        if (count <= parts.size())
        {
            for (std::size_t i = 0; i < count; ++i) parts[i] = { range.begin + i, range.begin + i + 1 };
        }
        else
        {
            parts = PartitionByVariance(range, prefix);
            partCount = parts.size();
        }

        std::array<double, 4> partWeights{};
        for (std::size_t i = 0; i < partCount; ++i) partWeights[i] = RangeWeight(prefix, parts[i]);
        if (rectangle.orientation.direction == Direction::Counterclockwise)
            std::ranges::reverse(partWeights.begin(), partWeights.begin() + static_cast<std::ptrdiff_t>(partCount));

        Layout layout = SelectLayout(std::span(partWeights).first(partCount), rectangle, curve);
        if (rectangle.orientation.direction == Direction::Counterclockwise)
            std::ranges::reverse(layout.regions.begin(),
                layout.regions.begin() + static_cast<std::ptrdiff_t>(partCount));

        for (std::size_t i = 0; i < partCount; ++i)
        {
            if (count <= parts.size()) output[parts[i].begin] = layout.regions[i];
            else LayoutRange(layout.regions[i], prefix, parts[i], Curve::Hilbert, output);
        }
    }

    [[nodiscard]] CRect ToPixelRectangle(const Region& region, const CRect& parent)
    {
        const auto roundCoordinate = [](const double value) { return static_cast<LONG>(std::lround(value)); };
        CRect rectangle(
            parent.left + roundCoordinate(region.left),
            parent.bottom - roundCoordinate(region.bottom + region.height),
            parent.left + roundCoordinate(region.left + region.width),
            parent.bottom - roundCoordinate(region.bottom));
        rectangle.left = std::clamp(rectangle.left, parent.left, parent.right);
        rectangle.top = std::clamp(rectangle.top, parent.top, parent.bottom);
        rectangle.right = std::clamp(rectangle.right, parent.left, parent.right);
        rectangle.bottom = std::clamp(rectangle.bottom, parent.top, parent.bottom);
        return rectangle;
    }
}

void HilbertMooreTreeMap::ArrangeChildren(const std::span<const ULONGLONG> weights,
    const CRect& rectangle, const TreeMapLayout::State state, const Curve curve,
    const std::span<TreeMapLayout::ChildRegion> regions)
{
    ASSERT(regions.size() == weights.size());
    if (weights.empty() || rectangle.IsRectEmpty()) return;

    std::vector<double> positiveWeights;
    std::vector<std::size_t> positiveIndices;
    positiveWeights.reserve(weights.size());
    positiveIndices.reserve(weights.size());
    for (std::size_t i = 0; i < weights.size(); ++i)
    {
        if (weights[i] == 0) continue;
        positiveWeights.push_back(static_cast<double>(weights[i]));
        positiveIndices.push_back(i);
    }

    if (positiveWeights.empty())
    {
        positiveWeights.assign(weights.size(), 1.0);
        positiveIndices.resize(weights.size());
        std::iota(positiveIndices.begin(), positiveIndices.end(), 0u);
    }

    std::vector<double> prefix(positiveWeights.size() + 1, 0.0);
    std::partial_sum(positiveWeights.begin(), positiveWeights.end(), prefix.begin() + 1);

    std::vector<Region> layout(positiveWeights.size());
    const Region root{
        0.0, 0.0, static_cast<double>(rectangle.Width()),
        static_cast<double>(rectangle.Height()), state
    };
    LayoutRange(root, prefix, { 0, positiveWeights.size() }, curve, layout);

    for (std::size_t i = 0; i < layout.size(); ++i)
    {
        regions[positiveIndices[i]] = { ToPixelRectangle(layout[i], rectangle), layout[i].orientation };
    }
}
