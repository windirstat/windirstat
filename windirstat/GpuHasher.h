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

#pragma once

#include "pch.h"

//
// GpuHasher. Direct3D 11 compute shader implementation of the file hash
// algorithms (MD5, SHA1, SHA256, SHA384, SHA512) used for duplicate
// detection. The kernels live in Shaders/GpuHash.hlsl, which is embedded
// as a resource and compiled at runtime.
//
// Availability requires a hardware device with D3D_FEATURE_LEVEL_11_0.
// The WARP software rasterizer is deliberately not accepted: a software
// "GPU" would always lose against the BCrypt CPU path.
//
// All entry points are thread-safe; GPU work is serialized internally
// because Direct3D 11 immediate contexts are single-threaded.
//
class GpuHasher final
{
public:
    // True if a hardware D3D11 device with compute support could be
    // created and all kernels compiled. The result is cached, so repeated
    // calls are cheap.
    static bool IsAvailable();

    // Hash an in-memory buffer (used by the settings benchmark).
    // Returns the full digest, or an empty vector on any failure.
    static std::vector<BYTE> Hash(const BYTE* data, size_t size, HashAlgorithm algo);

    // Hash up to sizeLimit bytes of a file, reading it in chunks so large
    // files never have to fit into memory. progressCallback (optional) is
    // invoked once per chunk, mirroring the BCrypt path's pacman updates.
    // Returns the full digest, or an empty vector on any failure.
    static std::vector<BYTE> HashFile(const std::wstring& path, ULONGLONG sizeLimit,
        HashAlgorithm algo, const std::function<void()>& progressCallback = nullptr);
};
