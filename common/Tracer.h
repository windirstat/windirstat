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

#include <string>

#ifdef _DEBUG
#define VTRACE_FILE_LINE_FUNC   3
#define VTRACE_FILE_LINE        2
#define VTRACE_FUNC             1

#ifndef VTRACE_DETAIL
#   define VTRACE_DETAIL 3
#endif

#if VTRACE_TO_CONSOLE
class CWDSTracerConsole final
{
public:
    CWDSTracerConsole();
    ~CWDSTracerConsole();
};
#endif // VTRACE_TO_CONSOLE

class CWDSTracer final
{
public:
    CWDSTracer(const std::wstring& srcfile, const std::wstring& fctname, unsigned int srcline);
    CWDSTracer& operator=(const CWDSTracer&) = delete; // hide it
    void operator()(const std::wstring& format, ...) const;
private:
    std::wstring m_Srcfile;
    unsigned int m_Srcline;
    std::wstring m_Srcfunc;
    std::wstring m_Srcbasename;
};

// Use as VTRACE(format, ...) ... *must* be on one long line ;)
#   define VTRACE CWDSTracer(__##FILEW##__, __##FUNCTIONW__, __##LINE##__)
#else
#   define VTRACE __noop
#endif // _DEBUG
