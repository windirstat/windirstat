// CommonHelpers.cpp - Implementation of common global helper functions
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

#include "stdafx.h"
#include <common/MdExceptions.h>
#include <common/Constants.h>
#include <common/CommonHelpers.h>

#include <map>
#include <sddl.h>
#include <string>

CStringW MyStrRetToString(const LPITEMIDLIST pidl, const STRRET* strret)
{
    CStringW s;

    switch (strret->uType)
    {
    case STRRET_CSTR:
        {
            s.Format(L"%hs", strret->cStr);
        }
        break;
    case STRRET_OFFSET:
        {
            s.Format(L"%hs", reinterpret_cast<char*>(pidl) + strret->uOffset);
        }
        break;
    case STRRET_WSTR:
        {
            s = strret->pOleStr;
        }
        break;
    }

    return s;
}

BOOL ShellExecuteNoThrow(HWND hwnd, LPCWSTR lpVerb, LPCWSTR lpFile, LPCWSTR lpParameters, LPCWSTR lpDirectory, INT nShowCmd)
{
    SHELLEXECUTEINFO sei = {
        sizeof(SHELLEXECUTEINFO),
        0,
        hwnd,
        lpVerb,
        lpFile,
        lpParameters,
        lpDirectory,
        nShowCmd,
        nullptr, // hInstApp
        nullptr,
        nullptr,
        nullptr,
        0, // dwHotKey
        {},
        nullptr
    };

    return ::ShellExecuteEx(&sei);
}

BOOL ShellExecuteThrow(HWND hwnd, LPCWSTR lpVerb, LPCWSTR lpFile, LPCWSTR lpParameters, LPCWSTR lpDirectory, INT nShowCmd)
{
    CWaitCursor wc;

    const BOOL bResult = ShellExecuteNoThrow(hwnd, lpVerb, lpFile, lpParameters, lpDirectory, nShowCmd);
    if (!bResult)
    {
        MdThrowStringExceptionF(L"ShellExecute failed: %1!s!", MdGetWinErrorText(::GetLastError()).GetString());
    }
    return bResult;
}

CStringW GetBaseNameFromPath(LPCWSTR path)
{
    CStringW s  = path;
    const int i = s.ReverseFind(wds::chrBackslash);
    if (i < 0)
    {
        return s;
    }
    return s.Mid(i + 1);
}

CStringW GetAppFileName(const CStringW& ext)
{
    CStringW s;
    VERIFY(::GetModuleFileName(nullptr, s.GetBuffer(_MAX_PATH), _MAX_PATH));
    s.ReleaseBuffer();

    // optional substitute extension
    if (!ext.IsEmpty())
    {
        s = s.Left(s.ReverseFind(wds::chrDot) + 1) + ext;
    }

    return s;
}

CStringW GetAppFolder()
{
    const CStringW folder = GetAppFileName();
    return folder.Left(folder.ReverseFind(wds::chrBackslash));
}

constexpr DWORD SidGetLength(PSID x)
{
    return sizeof(SID) + (static_cast<SID*>(x)->SubAuthorityCount - 1) * sizeof(static_cast<SID*>(x)->SubAuthority);
}

std::wstring GetNameFromSid(const PSID sid)
{
    // return immediately if sid is null
    if (sid == nullptr) return L"";

    // define custom lookup function
    auto comp = [](const PSID p1, const PSID p2)
    {
        const DWORD l1 = SidGetLength(p1);
        const DWORD l2 = SidGetLength(p2);
        if (l1 != l2) return l1 < l2;
        return memcmp(p1, p2, l1) > 0;
    };

    // attempt to lookup sid in cache
    static std::map<PSID, std::wstring, decltype(comp)> name_map(comp);
    const auto iter = name_map.find(sid);
    if (iter != name_map.end())
    {
        return iter->second;
    }

    // copy the sid for storage in our cache table
    const DWORD sid_length = SidGetLength(sid);
    const auto sid_copy = memcpy(malloc(sid_length), sid, sid_length);

    // lookup the name for this sid
    SID_NAME_USE name_use;
    WCHAR account_name[UNLEN + 1], domain_name[UNLEN + 1];
    DWORD iAccountNameSize = _countof(account_name), iDomainName = _countof(domain_name);
    if (LookupAccountSid(nullptr, sid, account_name,
        &iAccountNameSize, domain_name, &iDomainName, &name_use) == 0)
    {
        SmartPointer<LPWSTR> sid_buff(LocalFree);
        ConvertSidToStringSid(sid, &sid_buff);
        name_map[sid_copy] = sid_buff;
    }
    else
    {
        // generate full name in domain\name format
        name_map[sid_copy] = std::wstring(domain_name) +
            L"\\" + std::wstring(account_name);
    }

    // return name
    return name_map[sid_copy];
}
