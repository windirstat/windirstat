// tracer.h - Implementation of tracer class for debugging purposes
//
// NOTE: this file is under MIT license as opposed to the project as a whole.
//
// WinDirStat - Directory Statistics
// Copyright (C) 2010 Oliver Schneider (assarbad.net)
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
// Author(s): - assarbad -> http://windirstat.info/contact/oliver/
//

#ifndef __TRACER_CPP_VER__
#define __TRACER_CPP_VER__ 2014012617
#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif // Check for "#pragma once" support
#include "tracer.h"
#include <cstdarg>
#include <fcntl.h>
#include <io.h>

#if VTRACE_TO_CONSOLE
CWDSTracerConsole::CWDSTracerConsole()
{
    ::AllocConsole();
    // Standard output
    int hCrt = _open_osfhandle((intptr_t)::GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);
    FILE *hf = _fdopen(hCrt, "w");
    *stdout = *hf;
    setvbuf(stdout, NULL, _IONBF, 0);
    // Standard error
    hCrt = _open_osfhandle((intptr_t)::GetStdHandle(STD_ERROR_HANDLE), _O_TEXT);
    hf = _fdopen(hCrt, "w");
    *stderr = *hf;
    setvbuf(stderr, NULL, _IONBF, 0);
}

CWDSTracerConsole::~CWDSTracerConsole()
{
    ::FreeConsole();
}
#endif // VTRACE_TO_CONSOLE

CWDSTracer::CWDSTracer(LPCSTR srcfile, LPCSTR fctname, unsigned int srcline)
    : m_srcfile(srcfile)
    , m_srcfunc(fctname)
    , m_srcline(srcline)
    // we can rely on the format with back slashes, no need to check forward slashes here
    , m_srcbasename((srcfile) ? strrchr(srcfile, '\\') : NULL)
{
    // Skip over the backslash
    m_srcbasename = (m_srcbasename) ? m_srcbasename + 1 : srcfile;
}

void CWDSTracer::operator()(LPCSTR format, ...) // ANSI
{
    CStringA str;
    va_list args;
    va_start(args, format);
    str.FormatV(format, args);
    va_end(args);
    CStringA strDbg;
    strDbg.Format("[%hs:%u:%hs] %hs\n", m_srcbasename, m_srcline, m_srcfunc, str.GetBuffer());
#   if !VTRACE_TO_CONSOLE || (VTRACE_TO_CONSOLE && !VTRACE_NO_OUTPUTDEBUGSTRING)
    OutputDebugStringA(strDbg.GetBuffer());
#   endif
#   if VTRACE_TO_CONSOLE
    printf(strDbg.GetBuffer());
#   endif // VTRACE_TO_CONSOLE
}

void CWDSTracer::operator()(LPCWSTR format, ...) // Unicode
{
    CStringW str;
    va_list args;
    va_start(args, format);
    str.FormatV(format, args);
    va_end(args);
    CStringW strDbg;
    strDbg.Format(L"[%hs:%u:%hs] %ws\n", m_srcbasename, m_srcline, m_srcfunc, str.GetBuffer());
#   if !VTRACE_TO_CONSOLE || (VTRACE_TO_CONSOLE && !VTRACE_NO_OUTPUTDEBUGSTRING)
    OutputDebugStringW(strDbg.GetBuffer());
#   endif
#   ifdef VTRACE_TO_CONSOLE
    wprintf(strDbg.GetBuffer());
#   endif // VTRACE_TO_CONSOLE
}
#endif // __TRACER_CPP_VER__
