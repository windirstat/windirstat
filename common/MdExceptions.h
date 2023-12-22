// MdExceptions.h
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
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
// This general purpose header is derived from a file
// created by www.daccord.net und published
// here under GPL with friendly permission of D'accord.
//

// Note: Md is just a prefix.

#pragma once

#ifndef _INC_STDARG
#include <stdarg.h>
#endif

class CMdStringException final : public CException
{
public:
    CMdStringException(LPCWSTR pszText)
        : m_sText(pszText) // pszText may be an ordinal resource (MAKEINTRESOURCE)
    {
    }

    BOOL GetErrorMessage(LPWSTR lpszError, UINT nMaxError, UINT* pnHelpContext = nullptr) override
    {
        if (pnHelpContext != nullptr)
        {
            *pnHelpContext = 0;
        }
        if (nMaxError != 0 && lpszError != nullptr)
        {
            // TODO, fix parameters
            wcscpy_s(lpszError, nMaxError, m_sText);
        }
        return true;
    }

protected:
    CStringW m_sText;
};

inline CStringW MdGetExceptionMessage(const CException* pe)
{
    constexpr INT ccBufferSize = 0x400;
    CStringW s;
    const BOOL b = pe->GetErrorMessage(s.GetBuffer(ccBufferSize), ccBufferSize);
    s.ReleaseBuffer();

    if (!b)
    {
        s = "(no error message available)";
    }

    return s;
}

inline CStringW MdGetWinErrorText(HRESULT hr)
{
    CStringW sRet;
    LPVOID lpMsgBuf = nullptr;
    const DWORD dw = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr,
        hr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&lpMsgBuf,
        0,
        nullptr
    );
    if (NULL == dw)
    {
        const CStringW s(MAKEINTRESOURCE(AFX_IDP_NO_ERROR_AVAILABLE));
        sRet.Format(L"%s (0x%08lx)", s.GetString(), hr);
    }
    else
    {
        sRet = CStringW(static_cast<LPCWSTR>(lpMsgBuf));
        ::LocalFree(lpMsgBuf);
    }
    return sRet;
}

inline void MdThrowStringException(UINT resId)
{
    throw new CMdStringException(MAKEINTRESOURCE(resId));
}

inline void MdThrowStringException(LPCWSTR pszText)
{
    throw new CMdStringException(pszText);
}

inline void __MdFormatStringExceptionV(CStringW& rsText, LPCWSTR pszFormat, va_list vlist)
{
    // CStringW sFormat(); // may be a MAKEINTRESOURCE
    rsText.FormatMessageV(CStringW(pszFormat), &vlist);
}

inline void AFX_CDECL MdThrowStringExceptionF(LPCWSTR pszFormat, ...)
{
    CStringW sText;

    va_list vlist;
    va_start(vlist, pszFormat);
    __MdFormatStringExceptionV(sText, pszFormat, vlist);
    va_end(vlist);

    MdThrowStringException(sText);
}

inline void MdThrowStringExceptionV(LPCWSTR pszFormat, va_list vlist)
{
    CStringW sText;
    __MdFormatStringExceptionV(sText, pszFormat, vlist);
    MdThrowStringException(sText);
}

inline void AFX_CDECL MdThrowStringExceptionF(UINT nResIdFormat, ...)
{
    CStringW sText;

    va_list vlist;
    va_start(vlist, nResIdFormat);
    __MdFormatStringExceptionV(sText, MAKEINTRESOURCE(nResIdFormat), vlist);
    va_end(vlist);

    MdThrowStringException(sText);
}

inline void MdThrowStringExceptionF(UINT nResIdFormat, va_list vlist)
{
    CStringW sText;
    __MdFormatStringExceptionV(sText, MAKEINTRESOURCE(nResIdFormat), vlist);
    MdThrowStringException(sText);
}

inline void MdThrowWinError(DWORD dw, LPCWSTR pszPrefix = nullptr)
{
    CStringW sMsg = pszPrefix;
    sMsg += L": " + MdGetWinErrorText(dw);
    MdThrowStringException(sMsg);
}

inline void MdThrowHresult(HRESULT hr, LPCWSTR pszPrefix = nullptr)
{
    CStringW sMsg = pszPrefix;
    sMsg += L": " + MdGetWinErrorText(hr);
    MdThrowStringException(sMsg);
}


inline void MdThrowLastWinerror(LPCWSTR pszPrefix = nullptr)
{
    MdThrowWinError(::GetLastError(), pszPrefix);
}

inline void MdThrowFailed(HRESULT hr, LPCWSTR pszPrefix = nullptr)
{
    if (FAILED(hr))
    {
        MdThrowHresult(hr, pszPrefix);
    }
}
