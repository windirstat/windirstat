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

#include "SmartPointer.h"

#include <string>
#include <format>

class CMdStringException final : public CException
{
public:
    explicit CMdStringException(const std::wstring & pszText) : m_SText(pszText)
    {
    }

    BOOL GetErrorMessage(LPWSTR lpszError, const UINT nMaxError, UINT* pnHelpContext = NULL) override
    {
        if (pnHelpContext != nullptr)
        {
            *pnHelpContext = 0;
        }
        if (nMaxError != 0 && lpszError != nullptr)
        {
            wcscpy_s(lpszError, nMaxError, m_SText.data());
        }
        return true;
    }

protected:
    std::wstring m_SText;
};

inline std::wstring MdGetExceptionMessage(const CException* pe)
{
    constexpr INT ccBufferSize = 0x400;
    std::wstring s(ccBufferSize, L' ');
    const BOOL b = pe->GetErrorMessage(s.data(), ccBufferSize);
    s.resize(wcslen(s.data()));

    if (!b)
    {
        s = L"(no error message available)";
    }

    return s;
}

inline std::wstring MdGetWinErrorText(const HRESULT hr)
{
    std::wstring sRet;
    SmartPointer<LPVOID> lpMsgBuf(LocalFree);
    const DWORD dw = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr,
        hr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&lpMsgBuf),
        0,
        nullptr
    );
    if (0 == dw)
    {
        const CStringW s(MAKEINTRESOURCE(AFX_IDP_NO_ERROR_AVAILABLE));
        sRet = std::format(L"{} {:#08x}", s.GetString(), static_cast<DWORD>(hr));
    }
    else
    {
        sRet = static_cast<LPWSTR>(*lpMsgBuf);
    }
    return sRet;
}

inline void MdThrowStringException(const std::wstring & pszText)
{
    throw new CMdStringException(pszText); //-V1022
}

inline void MdThrowWinError(const DWORD dw, const std::wstring & pszPrefix = {})
{
    std::wstring sMsg = pszPrefix;
    sMsg += L": " + MdGetWinErrorText(static_cast<HRESULT>(dw));
    MdThrowStringException(sMsg);
}

inline void MdThrowLastWinError(const std::wstring & pszPrefix = {})
{
    MdThrowWinError(GetLastError(), pszPrefix);
}
