// Tracer.h - Implementation of tracer class for debugging purposes
//
// NOTE: this file is under MIT license as opposed to the project as a whole.
//
// WinDirStat - Directory Statistics
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#ifdef _DEBUG
#include <format>
#include <string>
#include <iostream>
#include <source_location>

using VRACE_DETAIL_LEVEL = enum : std::uint8_t
{
    VTRACE_FUNC = 1,
    VTRACE_FILE_LINE = 2,
    VTRACE_FILE_LINE_FUNC = 3
};

constexpr bool VTRACE_OUTPUTDEBUGSTRING = false;

#ifndef VTRACE_DETAIL
#define VTRACE_DETAIL VRACE_DETAIL_LEVEL::VTRACE_FILE_LINE_FUNC
#endif

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
        const std::wstring str = std::vformat(format, std::make_wformat_args(args...));

        std::string errorPrefix;
        std::string fileName = loc.file_name();
        if (fileName.find_last_of('\\')) fileName = fileName.substr(fileName.find_last_of('\\') + 1);
        if (VTRACE_DETAIL == VTRACE_FILE_LINE_FUNC)
            errorPrefix = std::format("{}:{}|{}", fileName, loc.line(), loc.function_name());
        else if (VTRACE_DETAIL == VTRACE_FILE_LINE)
            errorPrefix = std::format("{}:{}", fileName, loc.line());
        else if (VTRACE_DETAIL == VTRACE_FUNC)
            errorPrefix = loc.function_name();

        std::wstring strDbg = errorPrefix.empty() ?
            std::format(L"{}\n", str) :
            std::format(L"[{}] {}\n", std::wstring(errorPrefix.begin(), errorPrefix.end()), str);

        if (VTRACE_OUTPUTDEBUGSTRING)
            OutputDebugStringW(strDbg.c_str());

        if (VTRACE_TO_CONSOLE)
            std::wcout << strDbg;
    }
};

#else
#define VTRACE(x, ...) __noop
#endif // _DEBUG
