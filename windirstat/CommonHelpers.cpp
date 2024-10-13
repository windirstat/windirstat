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

#include "MdExceptions.h"
#include <Constants.h>
#include "CommonHelpers.h"

#include <format>
#include <map>
#include <sddl.h>
#include <string>
#include <array>
#include <unordered_set>

bool ShellExecuteThrow(const std::wstring& lpFile, const std::wstring& lpParameters, const std::wstring & lpVerb,
    const HWND hwnd, const std::wstring & lpDirectory, const INT nShowCmd)
{
    CWaitCursor wc;

    SHELLEXECUTEINFO sei;
    ZeroMemory(&sei, sizeof(SHELLEXECUTEINFO));
    sei.cbSize = sizeof(SHELLEXECUTEINFO);
    sei.fMask = 0;
    sei.hwnd = hwnd;
    sei.lpParameters = lpParameters.empty() ? nullptr : lpParameters.c_str();
    sei.lpVerb = lpVerb.empty() ? nullptr : lpVerb.c_str();
    sei.lpFile = lpFile.empty() ? nullptr : lpFile.c_str();
    sei.lpDirectory = lpDirectory.empty() ? nullptr : lpDirectory.c_str();
    sei.nShow = nShowCmd;

    const BOOL bResult = ::ShellExecuteEx(&sei);
    if (!bResult)
    {
        MdThrowStringException(std::format(L"ShellExecute failed: {}",
            MdGetWinErrorText(static_cast<DWORD>(GetLastError()))));
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
    std::wstring s(_MAX_PATH, wds::chrNull);
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

IContextMenu* GetContextMenu(const HWND hwnd, const std::vector<std::wstring>& paths)
{
    // structures to hold and track pidls for children
    std::vector<SmartPointer<LPITEMIDLIST>> pidlsForCleanup;
    std::vector<LPCITEMIDLIST> pidlsRelatives;

    // create list of children from paths
    for (auto& path : paths)
    {
        LPCITEMIDLIST pidl = ILCreateFromPath(path.c_str());
        if (pidl == nullptr) return nullptr;
        pidlsForCleanup.emplace_back(CoTaskMemFree, const_cast<LPITEMIDLIST>(pidl));

        CComPtr<IShellFolder> pParentFolder;
        LPCITEMIDLIST pidlRelative;
        if (FAILED(SHBindToParent(pidl, IID_IShellFolder, reinterpret_cast<LPVOID*>(&pParentFolder), &pidlRelative))) return nullptr;
        pidlsRelatives.push_back(pidlRelative);

        // on last item, return the context menu
        if (pidlsRelatives.size() == paths.size())
        {
            IContextMenu* pContextMenu;
            if (FAILED(pParentFolder->GetUIObjectOf(hwnd, static_cast<UINT>(pidlsRelatives.size()),
                pidlsRelatives.data(), IID_IContextMenu, nullptr, reinterpret_cast<LPVOID*>(&pContextMenu)))) return nullptr;
            return pContextMenu;
        }
    }

    return nullptr;
}

bool CompressFile(const std::wstring& filePath, CompressionAlgorithm algorithm)
{
    USHORT numericAlgorithm = static_cast<USHORT>(algorithm) & ~FILE_PROVIDER_COMPRESSION_MODERN;
    const bool modernAlgorithm = static_cast<USHORT>(algorithm) != numericAlgorithm;

    SmartPointer<HANDLE> handle(CloseHandle, CreateFileW( filePath.c_str(),
        GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD status = 0;
    if (modernAlgorithm)
    {
        struct
        {
            WOF_EXTERNAL_INFO wof_info;
            FILE_PROVIDER_EXTERNAL_INFO_V1 file_info;
        }
        info =
        {
            {
                .Version = WOF_CURRENT_VERSION,
                .Provider = WOF_PROVIDER_FILE,
            },
            {
                .Version = FILE_PROVIDER_CURRENT_VERSION,
                .Algorithm = numericAlgorithm,
            },
        };

        DWORD bytesReturned;
        status = DeviceIoControl(handle, FSCTL_SET_EXTERNAL_BACKING,
            &info, sizeof(info), nullptr, 0, &bytesReturned, nullptr);
    }
    else if (numericAlgorithm == COMPRESSION_FORMAT_LZNT1)
    {
        DWORD bytesReturned = 0;
        status = DeviceIoControl(
            handle, FSCTL_SET_COMPRESSION, &numericAlgorithm,
            sizeof(numericAlgorithm), nullptr, 0, &bytesReturned, nullptr);
    }
    else
    {
        DWORD bytesReturned = 0;
        DeviceIoControl(
            handle, FSCTL_SET_COMPRESSION, &numericAlgorithm,
            sizeof(numericAlgorithm), nullptr, 0, &bytesReturned, nullptr);

        DeviceIoControl(
            handle, FSCTL_DELETE_EXTERNAL_BACKING, nullptr,
            0, nullptr, 0, nullptr, nullptr);
    }

    return status == 1 || status == 0xC000046F;
}

bool CompressFileAllowed(const std::wstring& filePath, CompressionAlgorithm algorithm)
{
    static std::unordered_map<std::wstring,bool> compressionStandard;
    static std::unordered_map<std::wstring,bool> compressionModern;
    auto& compressionMap = (algorithm == CompressionAlgorithm::LZNT1) ?
        compressionStandard : compressionModern;

    // Fetch volume root path
    std::array<WCHAR, MAX_PATH> volumeName;
    if (GetVolumePathName(filePath.c_str(), volumeName.data(),
        static_cast<DWORD>(volumeName.size())) == 0)
    {
        return false;
    }

    // Return cached value
    if (compressionMap.contains(volumeName.data()))
    {
        return compressionMap.at(volumeName.data());
    }

    // Enable 'none' button if either normal or modern are available
    if (algorithm == CompressionAlgorithm::NONE)
    {
        return CompressFileAllowed(filePath, CompressionAlgorithm::LZNT1) ||
            CompressFileAllowed(filePath, CompressionAlgorithm::XPRESS4K);
    }

    // Query volume for standard compression support
    if (algorithm == CompressionAlgorithm::LZNT1)
    {
        DWORD fileSystemFlags = 0;
        compressionMap[volumeName.data()] = GetVolumeInformation(volumeName.data(),
            nullptr, 0, nullptr, nullptr, &fileSystemFlags, nullptr, 0) != 0 &&
            (fileSystemFlags & FILE_FILE_COMPRESSION) != 0;
    }
    else
    {
        compressionMap[volumeName.data()] = false;
        SmartPointer<HANDLE> handle(CloseHandle, CreateFileW(volumeName.data(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
        if (handle != INVALID_HANDLE_VALUE)
        {
            DWORD bytesReturned = 0;
            const BOOL status = DeviceIoControl(handle, FSCTL_GET_EXTERNAL_BACKING,
                nullptr, 0, nullptr, 0, &bytesReturned, nullptr);
            compressionMap[volumeName.data()] = (status != 0 || GetLastError() == 342);
        }
    }

    return compressionMap.at(volumeName.data());
}
