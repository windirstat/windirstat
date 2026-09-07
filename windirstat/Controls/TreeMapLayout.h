// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.

#pragma once

#include "pch.h"

namespace TreeMapLayout
{
    enum class Style : std::uint8_t
    {
        Rows,
        Squarified,
        Hilbert,
        Moore,
    };

    enum class Rotation : std::uint8_t
    {
        None,
        Clockwise90,
        Half,
        Counterclockwise90,
    };

    enum class Direction : std::uint8_t
    {
        Clockwise,
        Counterclockwise,
    };

    // Opaque layout continuation carried from a rectangle to its descendants.
    // Row and squarified layouts leave it unchanged; curve layouts use it to
    // preserve traversal orientation through the hierarchy.
    struct State
    {
        Rotation rotation = Rotation::None;
        Direction direction = Direction::Clockwise;
    };

    struct Request
    {
        Style style = Style::Rows;
        CRect bounds;
        ULONGLONG parentWeight = 0;
        // Weights must be non-increasing, with zeroes last, and sum to
        // parentWeight. Output order always matches this input order.
        std::span<const ULONGLONG> weights;
        State state;
    };

    struct ChildRegion
    {
        CRect bounds;
        State state;
    };

    // Arrange one sibling list. Results correspond by index to Request::weights.
    void ArrangeChildren(const Request& request, std::vector<ChildRegion>& regions);
}
