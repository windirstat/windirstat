// tracer.h - Implementation of tracer class for debugging purposes
//
// WinDirStat - Directory Statistics
// Copyright (C) 2010 Oliver Schneider (assarbad.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Author(s): - assarbad -> http://windirstat.info/contact/oliver/
//

#ifndef __TRACER_CPP_VER__
#define __TRACER_CPP_VER__ 2010073115
#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif // Check for "#pragma once" support

#ifdef _DEBUG
#include <afxwin.h> // MFC core and standard components
#include <cstdarg>

class CWDSTracer
{
public:
    CWDSTracer(LPCSTR srcfile, unsigned int srcline)
        : m_srcfile(srcfile)
        , m_srcline(srcline)
        // we can rely on the format with back slashes, no need to check forward slashes here
        , m_srcbasename((srcfile) ? strrchr(srcfile, '\\') : NULL)
    {
        // Skip over the backslash
        m_srcbasename = (m_srcbasename) ? m_srcbasename + 1 : NULL;
    }

    void operator()(LPCSTR format, ...) // ANSI
    {
        CStringA str;
        va_list args;
        va_start(args, format);
        str.FormatV(format, args);
        va_end(args);
        CStringA strDbg;
        strDbg.Format("[%hs:%u] %hs\n", m_srcbasename, m_srcline, str.GetBuffer());
        OutputDebugStringA(strDbg.GetBuffer());
    }

    void operator()(LPCWSTR format, ...) // Unicode
    {
        CStringW str;
        va_list args;
        va_start(args, format);
        str.FormatV(format, args);
        va_end(args);
        CStringW strDbg;
        strDbg.Format(L"[%hs:%u] %ws\n", m_srcbasename, m_srcline, str.GetBuffer());
        OutputDebugStringW(strDbg.GetBuffer());
    }

private:
    const CStringA m_srcfile;
    unsigned int   m_srcline;
    LPCSTR         m_srcbasename;
};

// Use as VTRACE(format, ...) ... *must* be on one long line ;)
#   define VTRACE CWDSTracer(__##FILE##__, __##LINE##__)
#else // note _DEBUG
#   define VTRACE /##/
#endif // _DEBUG

#endif // __TRACER_CPP_VER__