// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

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
