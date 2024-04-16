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
#include "OsSpecific.h"
#include "MountPoints.h"
#include "SmartPointer.h"
#include "FileFind.h"

#include <algorithm>

bool CReparsePoints::IsReparseType(const std::wstring& path, DWORD tag_type)
{
    SmartPointer<HANDLE> handle(CloseHandle, CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
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

    return reinterpret_cast<PREPARSE_GUID_DATA_BUFFER>(buf.data())->ReparseTag == tag_type;
}

void CReparsePoints::Initialize()
{
    m_mountpoints.clear();

    WCHAR volume[_MAX_PATH];
    SmartPointer<HANDLE> hvol(FindVolumeClose, ::FindFirstVolume(volume, _countof(volume)));

    for (BOOL bContinue = hvol != INVALID_HANDLE_VALUE; bContinue; bContinue = ::FindNextVolume(hvol, volume, _countof(volume)))
    {
        // Fetch required buffer size
        DWORD buf_size = 0;
        GetVolumePathNamesForVolumeNameW(volume, nullptr, 0, &buf_size);
        if (buf_size == 0) continue;

        // Get names of all volumes
        std::wstring buf;
        buf.resize(buf_size);
        if (GetVolumePathNamesForVolumeNameW(volume, buf.data(), buf_size, &buf_size) == 0)
        {
            continue;
        }

        // Enumerate double null-terminated list of names
        for (LPWSTR name = buf.data(); *name != L'\0'; name += wcslen(name) + 1 + 1)
        {
            // Remove training backslash
            const auto len = wcslen(name);
            if (name[len - 1] == L'\\')
            {
                name[len - 1] = L'\0';
            }

            if (IsReparseType(name, IO_REPARSE_TAG_MOUNT_POINT))
            {
                _wcslwr_s(name, len + 1);
                m_mountpoints.emplace_back(name);
            }
        }
    }
}

bool CReparsePoints::IsReparsePoint(const DWORD attr)
{
    return attr != INVALID_FILE_ATTRIBUTES &&
        (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool CReparsePoints::IsMountPoint(const CStringW& path, DWORD attr) const
{
    if (attr == INVALID_FILE_ATTRIBUTES) attr = ::GetFileAttributes(path);
    if (!IsReparsePoint(attr)) return false;
    CStringW lookup_path = FileFindEnhanced::StripDosPathCharts(path);
    _wcslwr_s(lookup_path.GetBuffer(), static_cast<size_t>(lookup_path.GetLength()) + 1);
    return std::ranges::find(m_mountpoints, lookup_path.GetString()) != m_mountpoints.end();
}

bool CReparsePoints::IsJunction(const CStringW& path, DWORD attr) const
{
    if (attr == INVALID_FILE_ATTRIBUTES) attr = ::GetFileAttributes(path);
    if (!IsReparsePoint(attr)) return false;
    return !IsMountPoint(path) && IsReparseType(path.GetString(), IO_REPARSE_TAG_MOUNT_POINT);
}
