// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

// GPU-accelerated treemap cushion renderer.
// Uses a D3D11 compute shader (TreeMapCushion.hlsl) to run the per-pixel
// Phong shading math across all leaf rectangles in parallel on the GPU.
//
// NOTE: This is separate from PR #554 (GpuHasher), which accelerates SHA/MD5
// file hashing for duplicate detection. This module renders the treemap bitmap.

#pragma once
#include "pch.h"

class GpuRenderer final
{
public:
    // Per-leaf input: must match struct LeafInput in TreeMapCushion.hlsl (48 bytes).
    struct LeafInput
    {
        int   rcLeft, rcTop, rcRight, rcBottom; // leaf rectangle
        float s0, s1, s2, s3;                  // cushion surface coefficients
        float colR, colG, colB;                // base colour channels [0..255]
        float brightnessFactor;                // prepared.brightness / PALETTE_BRIGHTNESS
    };

    static bool IsAvailable();

    // Renders leaf cushion shading into bitmapBits in-place.
    // Non-leaf pixels (grid lines, background) are preserved.
    // Returns false if the GPU is unavailable or a D3D11 call fails;
    // caller must fall back to the CPU path.
    static bool Render(std::vector<COLORREF>&       bitmapBits,
                       const std::vector<LeafInput>& leaves,
                       int   stride,
                       float ambientLight,
                       float lx, float ly, float lz);
};
