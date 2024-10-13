// MountPoints.cpp - Implementation of CMountPoints
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
#include "MountPoints.h"
#include "Constants.h"
#include "SmartPointer.h"
#include "FileFind.h"
#include "GlobalHelpers.h"

#include <algorithm>

bool CReparsePoints::IsReparseType(const std::wstring & longpath, const std::unordered_set<DWORD>& tagTypes, const bool mask)
{
    SmartPointer<HANDLE> handle(CloseHandle, CreateFile(longpath.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    std::vector<BYTE> buf(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
    DWORD dwRet = MAXIMUM_REPARSE_DATA_BUFFER_SIZE;
    if (DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT,
        nullptr, 0, buf.data(), MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &dwRet, nullptr) == FALSE)
    {
        return false;
    }

    // Test if the tag matches the types or mask that was passed
    const DWORD tag = reinterpret_cast<PREPARSE_GUID_DATA_BUFFER>(buf.data())->ReparseTag;
    return std::ranges::any_of(tagTypes, [tag, mask](const DWORD& tagType) {
        return (mask && (tag & tagType) == tag) || (!mask && tag == tagType); });
}

void CReparsePoints::Initialize()
{
    // Enable reading of reparse data for cloud links
    SmartPointer<HMODULE> hmod(FreeLibrary, LoadLibrary(L"ntdll.dll"));
    if (hmod != nullptr)
    {
        CHAR(*RtlSetProcessPlaceholderCompatibilityMode)(CHAR Mode) =
            reinterpret_cast<decltype(RtlSetProcessPlaceholderCompatibilityMode)>(
                static_cast<LPVOID>(GetProcAddress(hmod, "RtlSetProcessPlaceholderCompatibilityMode")));
        if (RtlSetProcessPlaceholderCompatibilityMode != nullptr)
        {
            constexpr CHAR PHCM_EXPOSE_PLACEHOLDERS = 2;
            RtlSetProcessPlaceholderCompatibilityMode(PHCM_EXPOSE_PLACEHOLDERS);
        }
    }

    m_Mountpoints.clear();
    WCHAR volume[_MAX_PATH];
    SmartPointer<HANDLE> hvol(FindVolumeClose, ::FindFirstVolume(volume, _countof(volume)));
    for (BOOL bContinue = hvol != INVALID_HANDLE_VALUE; bContinue; bContinue = ::FindNextVolume(hvol, volume, _countof(volume)))
    {
        // Fetch required buffer size
        DWORD bufSize = 0;
        GetVolumePathNamesForVolumeNameW(volume, nullptr, 0, &bufSize);
        if (bufSize == 0) continue;

        // Get names of all volumes
        std::wstring buf;
        buf.resize(bufSize);
        if (GetVolumePathNamesForVolumeNameW(volume, buf.data(), bufSize, &bufSize) == 0)
        {
            continue;
        }

        // Enumerate double null-terminated list of names
        for (LPWSTR name = buf.data(); *name != wds::chrNull; name += wcslen(name) + 1 + 1)
        {
            // Remove training backslash
            const auto len = wcslen(name);
            if (name[len - 1] == wds::chrBackslash)
            {
                name[len - 1] = wds::chrNull;
            }

            if (IsReparseType(name, { IO_REPARSE_TAG_MOUNT_POINT }))
            {
                _wcslwr_s(name, len + 1);
                m_Mountpoints.emplace_back(name);
                m_Mountpoints.emplace_back(FileFindEnhanced::MakeLongPathCompatible(name));
            }
        }
    }
}

bool CReparsePoints::IsReparsePoint(const DWORD attr)
{
    return attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool CReparsePoints::IsDirectoryReparsePoint(const DWORD attr)
{
    return attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_DIRECTORY) &&
        (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool CReparsePoints::IsVolumeMountPoint(const std::wstring& longpath, DWORD attr) const
{
    if (attr == INVALID_FILE_ATTRIBUTES) attr = ::GetFileAttributes(longpath.c_str());
    if (!IsDirectoryReparsePoint(attr)) return false;
    std::wstring lowerpath = longpath;
    return std::ranges::find(m_Mountpoints, MakeLower(lowerpath)) != m_Mountpoints.end();
}

bool CReparsePoints::IsJunction(const std::wstring& longpath, DWORD attr) const
{
    if (attr == INVALID_FILE_ATTRIBUTES) attr = ::GetFileAttributes(longpath.c_str());
    if (!IsDirectoryReparsePoint(attr)) return false;
    return !IsVolumeMountPoint(longpath) && IsReparseType(longpath, { IO_REPARSE_TAG_MOUNT_POINT });
}

bool CReparsePoints::IsSymbolicLink(const std::wstring& longpath, DWORD attr)
{
    if (attr == INVALID_FILE_ATTRIBUTES) attr = ::GetFileAttributes(longpath.c_str());
    if (!IsReparsePoint(attr)) return false;
    return IsReparseType(longpath, { IO_REPARSE_TAG_SYMLINK });
}

bool CReparsePoints::IsCloudLink(const std::wstring& longpath, DWORD attr)
{
    if (attr == INVALID_FILE_ATTRIBUTES) attr = ::GetFileAttributes(longpath.c_str());
    if (!IsReparsePoint(attr)) return false;
    return IsReparseType(longpath, { IO_REPARSE_TAG_CLOUD_MASK }, true);
}
