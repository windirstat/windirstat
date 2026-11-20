// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "TreeMap.h"

// 3D bar chart of the space used by the files below an item, grouped by the age
// of their last change date. Every bar has a callout with its size and share.
class CAgeChart final
{
public:
    static constexpr std::size_t BucketCount = 7;

    void DrawChart(CDC* pdc, CRect rc, const CItem* root) const;

private:
    struct Buckets
    {
        std::array<ULONGLONG, BucketCount> size{};
        ULONGLONG total = 0;
    };

    static Buckets Collect(const CItem* root);
};
