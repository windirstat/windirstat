// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"

struct TraceCall final
{
    std::wstring_view format;
    std::source_location location;

    template <std::size_t N>
    TraceCall(const wchar_t (&value)[N],
        std::source_location caller = std::source_location::current()) noexcept
        : format(value, N - 1), location(caller)
    {
    }
};

void VTRACE(const TraceCall& trace, [[maybe_unused]] auto&&... args)
{
    if constexpr (IsDebugBuild)
    {
        std::string fileName = trace.location.file_name();
        if (const auto pos = fileName.find_last_of('\\'); pos != std::string::npos) fileName = fileName.substr(pos + 1);

        const std::wstring output = std::format(L"[{}:{}] {}\n", std::wstring(fileName.begin(), fileName.end()),
            trace.location.line(), std::vformat(trace.format, std::make_wformat_args(args...)));
        OutputDebugStringW(output.c_str());
    }
}
