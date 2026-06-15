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

// GPU compute shader for cushion-shaded treemap rendering.
// One thread per leaf; inner loops walk the leaf rectangle pixel by pixel.
// Matches the CPU DrawCushion() math exactly, including NormalizeColor().

// Per-leaf input: 48 bytes, matches GpuRenderer::LeafInput in GpuRenderer.h.
// Four individual floats instead of float3+float to avoid any float3 packing
// ambiguity (StructuredBuffer uses C-style packing, but explicit is safer).
struct LeafInput
{
    int4  rc;    // left, top, right, bottom
    float4 surf; // s0, s1, s2, s3 (cushion surface coefficients)
    float colR;  // base colour R channel [0..255]
    float colG;  // base colour G channel [0..255]
    float colB;  // base colour B channel [0..255]
    float bf;    // brightnessFactor = prepared.brightness / PALETTE_BRIGHTNESS
};

cbuffer Params : register(b0)
{
    uint  stride;     // bitmap row width in pixels
    float Ia;         // ambient light component
    float Is;         // 1.0 - Ia (specular/diffuse component)
    float llx;        // normalised light direction x
    float lly;        // normalised light direction y
    float llz;        // normalised light direction z
    uint  numLeaves;
    uint  pad;
};

StructuredBuffer<LeafInput> g_leaves : register(t0);
RWStructuredBuffer<uint>    g_bitmap : register(u0);

// Port of CColorSpace::DistributeFirst / NormalizeColor.
// Integer division in HLSL truncates toward zero — same as C++ static_cast<int>.
void DistributeFirst(inout int first, inout int second, inout int third)
{
    int h = (first - 255) / 2;
    first = 255;
    second += h;
    third  += h;
    if (second > 255)
    {
        int j = second - 255;
        second = 255;
        third  += j;
    }
    else if (third > 255)
    {
        int j = third - 255;
        third  = 255;
        second += j;
    }
}

void NormalizeColor(inout int red, inout int green, inout int blue)
{
    if      (red   > 255) DistributeFirst(red,   green, blue);
    else if (green > 255) DistributeFirst(green, red,   blue);
    else if (blue  > 255) DistributeFirst(blue,  red,   green);
}

[numthreads(64, 1, 1)]
void CSCushion(uint3 id : SV_DispatchThreadID)
{
    uint leafIdx = id.x;
    if (leafIdx >= numLeaves) return;

    LeafInput leaf = g_leaves[leafIdx];

    int   rcLeft   = leaf.rc.x;
    int   rcTop    = leaf.rc.y;
    int   rcRight  = leaf.rc.z;
    int   rcBottom = leaf.rc.w;
    float s0       = leaf.surf.x;
    float s1       = leaf.surf.y;
    float s2       = leaf.surf.z;
    float s3       = leaf.surf.w;
    float colR     = leaf.colR;
    float colG     = leaf.colG;
    float colB     = leaf.colB;
    float bf       = leaf.bf;

    for (int iy = rcTop; iy < rcBottom; ++iy)
    {
        float ny       = -(2.0f * s1 * ((float)iy + 0.5f) + s3);
        float ny_ly_lz = ny * lly + llz;
        float ny2_1    = ny * ny + 1.0f;

        for (int ix = rcLeft; ix < rcRight; ++ix)
        {
            float nx   = -(2.0f * s0 * ((float)ix + 0.5f) + s2);
            // rsqrt emits hardware rsqrtps — matches the CPU /fp:fast rsqrtss path
            float cosa = (nx * llx + ny_ly_lz) * rsqrt(nx * nx + ny2_1);
            cosa = min(cosa, 1.0f);

            float pixel = Is * cosa;
            pixel = max(pixel, 0.0f);
            pixel += Ia;
            pixel *= bf;

            int red   = (int)(colR * pixel);
            int green = (int)(colG * pixel);
            int blue  = (int)(colB * pixel);

            NormalizeColor(red, green, blue);

            // BGR layout: matches COLORREF / the CPU BGR() helper
            uint bgr = (uint)blue | ((uint)green << 8) | ((uint)red << 16);
            g_bitmap[(uint)iy * stride + (uint)ix] = bgr;
        }
    }
}
