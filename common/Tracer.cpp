// Ttracer.h - Implementation of tracer class for debugging purposes
//
// NOTE: this file is under MIT license as opposed to the project as a whole.
//
// WinDirStat - Directory Statistics
// Copyright (C) 2004-2017 WinDirStat Team (windirstat.net)
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
// Author(s): - Oliver
//

#include "stdafx.h"
#include "Tracer.h"
#include <cstdarg>
#include <fcntl.h>
#include <io.h>
#include <conio.h>

#ifdef _DEBUG
#if VTRACE_TO_CONSOLE
CWDSTracerConsole::CWDSTracerConsole()
{
    ::AllocConsole();
    ::SetConsoleTitle(L"WinDirStat Debug Trace Output");
    // Standard output
    int hCrt = _open_osfhandle(reinterpret_cast<intptr_t>(::GetStdHandle(STD_OUTPUT_HANDLE)), _O_TEXT);
    FILE * hf = _fdopen(hCrt, "w");
    *stdout = *hf;
    setvbuf(stdout, nullptr, _IONBF, 0);
    // Standard error
    hCrt = _open_osfhandle(reinterpret_cast<intptr_t>(::GetStdHandle(STD_ERROR_HANDLE)), _O_TEXT);
    hf = _fdopen(hCrt, "w");
    *stderr = *hf;
    setvbuf(stderr, nullptr, _IONBF, 0);
}

CWDSTracerConsole::~CWDSTracerConsole()
{
    wprintf(L"Press a key to continue/close.\n");
    (void)_getch();
    ::FreeConsole();
}
#endif // VTRACE_TO_CONSOLE

CWDSTracer::CWDSTracer(LPCWSTR srcfile, LPCWSTR fctname, unsigned int srcline)
    : m_srcfile(srcfile)
    , m_srcline(srcline)
    , m_srcfunc(fctname)
    // we can rely on the format with back slashes, no need to check forward slashes here
    , m_srcbasename((srcfile) ? wcsrchr(srcfile, '\\') : nullptr)
{
    // Skip over the backslash
    m_srcbasename = (m_srcbasename) ? m_srcbasename + 1 : srcfile;
}

void CWDSTracer::operator()(LPCWSTR format, ...) const
{
    CStringW str;
    va_list args;
    va_start(args, format);
    str.FormatV(format, args);
    va_end(args);
    CStringW strDbg, strPfx;
#   if (VTRACE_DETAIL == VTRACE_FILE_LINE_FUNC)
    strPfx.Format(L"%s:%u|%s", m_srcbasename, m_srcline, m_srcfunc);
#   elif (VTRACE_DETAIL == VTRACE_FILE_LINE)
    strPfx.Format(L"%s:%u", m_srcbasename, m_srcline);
#   elif (VTRACE_DETAIL == VTRACE_FUNC)
    strPfx = m_srcfunc;
#   endif
    if(strPfx.IsEmpty())
        strDbg.Format(L"%s\n", str.GetBuffer());
    else
        strDbg.Format(L"[%s] %s\n", strPfx.GetBuffer(), str.GetBuffer());
#   if !VTRACE_TO_CONSOLE || (VTRACE_TO_CONSOLE && !VTRACE_NO_OUTPUTDEBUGSTRING)
    OutputDebugStringW(strDbg.GetBuffer());
#   endif
#   if VTRACE_TO_CONSOLE
    wprintf(strDbg.GetBuffer());
#   endif // VTRACE_TO_CONSOLE
}
#endif
