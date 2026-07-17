// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.

#pragma once

#include "TreeMapLayout.h"

namespace HilbertMooreTreeMap
{
    enum class Curve : std::uint8_t
    {
        Hilbert,
        Moore,
    };

    // Curve backend for the common TreeMapLayout dispatcher.
    void ArrangeChildren(std::span<const ULONGLONG> weights, const CRect& rectangle,
        TreeMapLayout::State state, Curve curve,
        std::span<TreeMapLayout::ChildRegion> regions);
}
