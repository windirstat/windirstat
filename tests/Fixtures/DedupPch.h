// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

// Compile the production readers without initializing unrelated application settings or UI objects.
#pragma include_alias("pch.h", "DedupPch.h")
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#include <winternl.h>
#include <shlobj.h>
#include <execution>
#include <mutex>
#include <fstream>
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <climits>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <malloc.h>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <regex>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "SmartPointer.h"
#pragma comment(lib, "user32.lib")

class CItem;
constexpr DWORD IO_REPARSE_TAG_JUNCTION_POINT = ~IO_REPARSE_TAG_MOUNT_POINT;
namespace wds
{
    constexpr std::size_t Ki = 1024;
    constexpr std::size_t Mi = 1024 * Ki;
    constexpr ULONGLONG Gi = 1024ull * Mi;
}

template <typename T>
constexpr T* ByteOffset(void* ptr, const std::ptrdiff_t offset) noexcept
{
    return reinterpret_cast<T*>(static_cast<std::byte*>(ptr) + offset);
}

#define VTRACE(...) ((void)0)
std::wstring TranslateError(HRESULT error);
