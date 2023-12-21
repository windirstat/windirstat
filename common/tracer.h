// tracer.h - Implementation of tracer class for debugging purposes
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

#ifndef __TRACER_H_VER__
#define __TRACER_H_VER__ 2017112219
#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif // Check for "#pragma once" support

#ifdef _DEBUG
#define VTRACE_FILE_LINE_FUNC   3
#define VTRACE_FILE_LINE        2
#define VTRACE_FUNC             1

#ifndef VTRACE_DETAIL
#   define VTRACE_DETAIL 3
#endif

#if VTRACE_TO_CONSOLE
class CWDSTracerConsole
{
public:
    CWDSTracerConsole();
    ~CWDSTracerConsole();
};
#endif // VTRACE_TO_CONSOLE

class CWDSTracer
{
public:
    CWDSTracer(LPCSTR srcfile, LPCSTR fctname, unsigned int srcline);
    void operator()(LPCSTR format, ...);
    void operator()(LPCWSTR format, ...);
private:
    const CStringA m_srcfile;
    unsigned int   m_srcline;
    LPCSTR         m_srcbasename;
    LPCSTR         m_srcfunc;
    CWDSTracer&    operator=(const CWDSTracer&); // hide it
};

// Use as VTRACE(format, ...) ... *must* be on one long line ;)
#   define VTRACE CWDSTracer(__##FILE##__, __##FUNCTION##__, __##LINE##__)
#else
#   define VTRACE __noop
#endif // _DEBUG

#endif // __TRACER_H_VER__
