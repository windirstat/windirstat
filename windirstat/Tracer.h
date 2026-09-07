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
