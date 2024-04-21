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

#include <format>
#include <map>
#include <sddl.h>
#include <string>

BOOL ShellExecuteThrow(HWND hwnd, const std::wstring & lpVerb, const std::wstring & lpFile,
    const std::wstring & lpDirectory, const INT nShowCmd)
{
    CWaitCursor wc;

    SHELLEXECUTEINFO sei;
    ZeroMemory(&sei, sizeof(SHELLEXECUTEINFO));
    sei.cbSize = sizeof(SHELLEXECUTEINFO);
    sei.fMask = 0;
    sei.hwnd = hwnd;
    sei.lpVerb = lpVerb.c_str();
    sei.lpFile = lpFile.c_str();
    sei.lpDirectory = lpDirectory.c_str();
    sei.nShow = nShowCmd;

    const BOOL bResult = ::ShellExecuteEx(&sei);
    if (!bResult)
    {
        MdThrowStringException(std::format(L"ShellExecute failed: {}",
            MdGetWinErrorText(static_cast<DWORD>(::GetLastError()))));
    }
    return bResult;
}

std::wstring GetBaseNameFromPath(const std::wstring & path)
{
    std::wstring s  = path;
    const auto i = s.find_last_of(wds::chrBackslash);
    if (i == std::wstring::npos)
    {
        return s;
    }
    return s.substr(i + 1);
}

std::wstring GetAppFileName(const std::wstring& ext)
{
    std::wstring s(_MAX_PATH, L'\0');
    VERIFY(::GetModuleFileName(nullptr, s.data(), _MAX_PATH));
    s.resize(wcslen(s.data()));

    // optional substitute extension
    if (!ext.empty())
    {
        s = s.substr(0,s.find_last_of(wds::chrDot) + 1) + ext;
    }

    return s;
}

std::wstring GetAppFolder()
{
    const std::wstring folder = GetAppFileName();
    return folder.substr(0, folder.find_last_of(wds::chrBackslash));
}

constexpr DWORD SidGetLength(const PSID x)
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
    static std::map<PSID, std::wstring, decltype(comp)> nameMap(comp);
    const auto iter = nameMap.find(sid);
    if (iter != nameMap.end())
    {
        return iter->second;
    }

    // copy the sid for storage in our cache table
    const DWORD sidLength = SidGetLength(sid);
    const auto sidCopy = memcpy(malloc(sidLength), sid, sidLength);

    // lookup the name for this sid
    SID_NAME_USE nameUse;
    WCHAR accountName[UNLEN + 1], domainName[UNLEN + 1];
    DWORD iAccountNameSize = _countof(accountName), iDomainName = _countof(domainName);
    if (LookupAccountSid(nullptr, sid, accountName,
        &iAccountNameSize, domainName, &iDomainName, &nameUse) == 0)
    {
        SmartPointer<LPWSTR> sidBuff(LocalFree);
        ConvertSidToStringSid(sid, &sidBuff);
        nameMap[sidCopy] = sidBuff;
    }
    else
    {
        // generate full name in domain\name format
        nameMap[sidCopy] = std::format(L"{}\\{}", domainName, accountName);
    }

    // return name
    return nameMap[sidCopy];
}
