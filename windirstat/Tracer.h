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

#ifdef _DEBUG
#include "pch.h"

#define VTRACE(x, ...) CWDSTracerConsole::ProcessOutput(std::source_location::current(), x, ##__VA_ARGS__)

class CWDSTracerConsole final
{
    FILE* handleErr;
    FILE* handleOut;
    FILE* handleIn;

public:
    CWDSTracerConsole()
    {
        AllocConsole();
        ::SetConsoleTitle(L"WinDirStat Debug Trace Output");

        // Redirect console output to new console
        _wfreopen_s(&handleErr, L"CONOUT$", L"w", stderr);
        _wfreopen_s(&handleOut, L"CONOUT$", L"w", stdout);
        _wfreopen_s(&handleIn, L"CONIN$", L"r", stdin);

        // Disable buffering
        if (handleOut != nullptr) (void) setvbuf(handleOut, nullptr, _IONBF, 0);
        if (handleErr != nullptr) (void) setvbuf(handleErr, nullptr, _IONBF, 0);
    }

    ~CWDSTracerConsole()
    {
        std::wcout << L"Press any key to close this window.\n";
        std::wcin.get();
        FreeConsole();
    }

    static void ProcessOutput(const std::source_location& loc, std::wstring_view format, auto&&... args)
    {
        std::string fileName = loc.file_name();
        if (fileName.find_last_of('\\')) fileName = fileName.substr(fileName.find_last_of('\\') + 1);
        std::string errorPrefix = std::format("{}:{}", fileName, loc.line());
        std::wcout << std::format(L"[{}] {}\n", std::wstring(errorPrefix.begin(), errorPrefix.end()),
            std::vformat(format, std::make_wformat_args(args...)));
    }
};

#else
#define VTRACE(x, ...) __noop
#endif // _DEBUG
