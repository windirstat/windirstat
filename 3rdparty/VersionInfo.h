///////////////////////////////////////////////////////////////////////////////
///
/// Little class which reads the version info from a loaded PE file.
///
/// Licensed under the MIT license (see below).
///
///////////////////////////////////////////////////////////////////////////////
///
/// Copyright (c) 2016, 2017 Oliver Schneider (assarbad.net)
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the "Software"),
/// to deal in the Software without restriction, including without limitation
/// the rights to use, copy, modify, merge, publish, distribute, sublicense,
/// and/or sell copies of the Software, and to permit persons to whom the
/// Software is furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in
/// all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
/// DEALINGS IN THE SOFTWARE.
///
///////////////////////////////////////////////////////////////////////////////

#ifndef __VERSIONINFO_H_VER__
#define __VERSIONINFO_H_VER__ 2018030119
#if (defined(_MSC_VER) && (_MSC_VER >= 1020)) || defined(__MCPP)
#pragma once
#endif // Check for "#pragma once" support

#include <Windows.h>
#include <tchar.h>
#pragma warning(disable:4995)
#if defined(DDKBUILD)
#include <stdio.h>
#else
#include <cstdio>
#endif
#pragma warning(default:4995)
#pragma comment(lib, "delayimp")

class CVersionInfo
{
    LPVOID m_lpVerInfo;
    VS_FIXEDFILEINFO* m_pFixedFileInfo;
    DWORD m_useTranslation;
public:
    CVersionInfo(HINSTANCE hInstance)
        : m_lpVerInfo(NULL)
        , m_pFixedFileInfo(NULL)
        , m_useTranslation(0)
    {
        HRSRC hVersionResource = ::FindResource(hInstance, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
        if (NULL != hVersionResource)
        {
            if (DWORD dwSize = ::SizeofResource(hInstance, hVersionResource))
            {
                if (HGLOBAL hVersionResourceData = ::LoadResource(hInstance, hVersionResource))
                {
                    if (LPVOID pVerInfoRO = ::LockResource(hVersionResourceData))
                    {
                        if (NULL != (m_lpVerInfo = ::LocalAlloc(LPTR, dwSize)))
                        {
                            ::CopyMemory(m_lpVerInfo, pVerInfoRO, dwSize);
                            UINT uLen;
                            if (::VerQueryValue(m_lpVerInfo, _T("\\"), (LPVOID*)&m_pFixedFileInfo, &uLen))
                            {
#ifdef ATLTRACE2
                                ATLTRACE2(_T("%u.%u\n"), HIWORD(m_pFixedFileInfo->dwFileVersionMS), LOWORD(m_pFixedFileInfo->dwFileVersionMS));
#endif // ATLTRACE2
                                DWORD* translations;
                                if (::VerQueryValue(m_lpVerInfo, _T("\\VarFileInfo\\Translation"), (LPVOID*)&translations, &uLen))
                                {
                                    size_t const numTranslations = uLen / sizeof(DWORD);
#ifdef ATLTRACE2
                                    ATLTRACE2(_T("Number of translations: %u\n"), (UINT)numTranslations);
#endif // ATLTRACE2
                                    for (size_t i = 0; i < numTranslations; i++)
                                    {
#ifdef ATLTRACE2
                                        ATLTRACE2(_T("Translation %u: %08X\n"), (UINT)i, translations[i]);
#endif // ATLTRACE2
                                        switch (LOWORD(translations[i]))
                                        {
                                        case MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL): // fall through
                                        case MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US):
                                            if (1200 == HIWORD(translations[i])) // only Unicode entries
                                            {
                                                m_useTranslation = translations[i];
                                                return;
                                            }
                                            break;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                m_pFixedFileInfo = NULL;
                            }
                        }
                    }
                }
            }
        }
    }

    virtual ~CVersionInfo()
    {
        ::LocalFree(m_lpVerInfo);
        m_lpVerInfo = NULL;
    }

    LPCTSTR operator[](LPCTSTR lpszKey) const
    {
        if (!m_lpVerInfo || !lpszKey)
        {
            return NULL;
        }
        size_t const addend = MAX_PATH;
        if(_tcslen(lpszKey) >= addend)
        {
            return NULL;
        }
        TCHAR const fmtstr[] = _T("\\StringFileInfo\\%04X%04X\\%s");
        size_t const fmtbuflen = sizeof(fmtstr)/sizeof(fmtstr[0]) + addend;
        TCHAR fullName[fmtbuflen] = {0};
        _stprintf_s(fullName, fmtbuflen, fmtstr, LOWORD(m_useTranslation), HIWORD(m_useTranslation), lpszKey);
        fullName[fmtbuflen-1] = 0;

#ifdef ATLTRACE2
        ATLTRACE2(_T("Full name: %s\n"), fullName);
#endif // ATLTRACE2
        UINT uLen = 0;
        LPTSTR lpszBuf = NULL;
        if (::VerQueryValue(m_lpVerInfo, fullName, (LPVOID*)&lpszBuf, &uLen))
        {
#ifdef ATLTRACE2
            ATLTRACE2(_T("Value: %s\n"), lpszBuf);
#endif // ATLTRACE2
            return lpszBuf;
        }
#ifdef ATLTRACE2
        ATLTRACE2(_T("Value: NULL\n"));
#endif // ATLTRACE2
        return NULL;
    }
};

#endif // __VERSIONINFO_H_VER__