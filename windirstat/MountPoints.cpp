// mountpoints.cpp - Implementation of CMountPoints
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
#include "osspecific.h"
#include "MountPoints.h"
#include "GlobalHelpers.h"
#include <common/Tracer.h>

CReparsePoints::~CReparsePoints()
{
    Clear();
}

void CReparsePoints::Clear()
{
    m_drive.RemoveAll();

    POSITION pos = m_volume.GetStartPosition();
    while (pos != nullptr)
    {
        CStringW volume;
        PointVolumeArray* pva = nullptr;
        m_volume.GetNextAssoc(pos, volume, pva);
        ASSERT_VALID(pva);
        delete pva;
    }
    m_volume.RemoveAll();
}

void CReparsePoints::Initialize()
{
    Clear();

    GetDriveVolumes();
    GetAllMountPoints();
}

void CReparsePoints::GetDriveVolumes()
{
    m_drive.SetSize(wds::iNumDriveLetters);

    const DWORD drives = ::GetLogicalDrives();
    DWORD mask         = 0x00000001;
    for (int i = 0; i < wds::iNumDriveLetters; i++, mask <<= 1)
    {
        WCHAR volume[_MAX_PATH] = L"";

        if ((drives & mask) != 0)
        {
            CStringW s;
            s.Format(L"%c:\\", i + wds::chrCapA);

            const BOOL b = ::GetVolumeNameForVolumeMountPoint(s, volume, _countof(volume));

            if (!b)
            {
#               ifdef _DEBUG
                if(ERROR_NOT_READY == ::GetLastError())
                    VTRACE(L"GetVolumeNameForVolumeMountPoint(%s): not ready (%d).", volume, ::GetLastError());
                else
                    VTRACE(L"GetVolumeNameForVolumeMountPoint(%s): unexpected error (%d).", volume, ::GetLastError());
#               endif // _DEBUG
                volume[0] = 0;
            }
        }

        m_drive[i] = volume;
    }
}

void CReparsePoints::GetAllMountPoints()
{
    WCHAR volume[_MAX_PATH];
    const HANDLE hvol = ::FindFirstVolume(volume, _countof(volume));
    if (hvol == INVALID_HANDLE_VALUE)
    {
        VTRACE(L"No volumes found.");
        return;
    }

    for (BOOL bContinue = true; bContinue; bContinue = ::FindNextVolume(hvol, volume, _countof(volume)))
    {
        WCHAR fsname[_MAX_PATH], vname[_MAX_PATH];
        auto pva = new PointVolumeArray;
        ASSERT_VALID(pva);

        DWORD fsflags;
        const BOOL b = ::GetVolumeInformation(volume, vname, _countof(vname), nullptr, nullptr, &fsflags, fsname, _countof(fsname));

        if (!b)
        {
#           ifdef _DEBUG
            if(ERROR_NOT_READY == ::GetLastError())
                VTRACE(L"%s (%s) is not ready (%d).", vname, volume, ::GetLastError());
            else
                VTRACE(L"Unexpected error on %s (%s, %d).", vname, volume, ::GetLastError());
#           endif // _DEBUG
            m_volume.SetAt(volume, pva);
            continue;
        }

        if ((fsflags & FILE_SUPPORTS_REPARSE_POINTS) == 0)
        {
            // No support for reparse points, and therefore for volume
            // mount points, which are implemented using reparse points.
            VTRACE(L"%s, %s, does not support reparse points (%d).", volume, fsname, ::GetLastError());
            m_volume.SetAt(volume, pva);
            continue;
        }

        WCHAR point[_MAX_PATH];
        const HANDLE h = ::FindFirstVolumeMountPoint(volume, point, _countof(point));
        if (h == INVALID_HANDLE_VALUE)
        {
#           ifdef _DEBUG
            if(ERROR_ACCESS_DENIED == ::GetLastError())
            {
                VTRACE(L"Access denied to %s (%d).", volume, ::GetLastError());
            }
            else if(ERROR_NO_MORE_FILES != ::GetLastError())
            {
                VTRACE(L"Unexpected error for %s (%d).", volume, ::GetLastError());
            }
#           endif // _DEBUG
            m_volume.SetAt(volume, pva);
            continue;
        }

        for (BOOL bCont = TRUE; bCont; bCont = ::FindNextVolumeMountPoint(h, point, _countof(point)))
        {
            CStringW uniquePath = volume;
            uniquePath += point;
            WCHAR mountedVolume[_MAX_PATH];

            const BOOL bGotMountPoints = ::GetVolumeNameForVolumeMountPoint(uniquePath, mountedVolume, _countof(mountedVolume));

            if (!bGotMountPoints)
            {
                VTRACE(L"GetVolumeNameForVolumeMountPoint(%s) failed (%d).", uniquePath.GetBuffer(), ::GetLastError());
                continue;
            }

            SPointVolume pv;
            pv.point  = point;
            pv.volume = mountedVolume;
            pv.flags  = fsflags;
            VTRACE(L"%s (%s) -> %08X", point, mountedVolume, fsflags);

            pv.point.MakeLower();

            pva->Add(pv);
        }
        ::FindVolumeMountPointClose(h);

        m_volume.SetAt(volume, pva);
    }

    (void)::FindVolumeClose(hvol);

#ifdef _DEBUG
    POSITION pos = m_volume.GetStartPosition();
    while(pos != nullptr)
    {
        CStringW lvolume;
        PointVolumeArray *pva = nullptr;
        m_volume.GetNextAssoc(pos, lvolume, pva);
        pva->AssertValid();
    }
#endif
}

bool CReparsePoints::IsVolumeMountPoint(CStringW path)
{
    if (path.GetLength() < 3 || path[1] != wds::chrColon || path[2] != wds::chrBackslash)
    {
        // Don't know how to make out mount points on UNC paths ###
        return false;
    }

    ASSERT(path.GetLength() >= 3);
    ASSERT(path[1] == wds::chrColon);
    ASSERT(path[2] == wds::chrBackslash);

    if (path.Right(1) != wds::chrBackslash)
    {
        path += L"\\";
    }

    path.MakeLower();

    const CStringW volume = m_drive[path[0] - wds::chrSmallA];
    path                  = path.Mid(3);

    return IsVolumeMountPoint(volume, path);
}

// Check whether the current item is a junction point but no volume mount point
// as the latter ones are treated differently (see above).
bool CReparsePoints::IsFolderJunction(DWORD attr)
{
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    return (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

// ... same as before, but based on the full path
bool CReparsePoints::IsFolderJunction(const CStringW& path)
{
    if (IsVolumeMountPoint(path))
    {
        return false;
    }

    return IsFolderJunction(::GetFileAttributes(path));
}

bool CReparsePoints::IsVolumeMountPoint(CStringW volume, CStringW path) const
{
    while (true)
    {
        PointVolumeArray* pva;
        if (!m_volume.Lookup(volume, pva))
        {
            VTRACE(L"CMountPoints: Volume(%s) unknown!", volume.GetString());
            return false;
        }

        CStringW point;
        int i = 0;
        for (; i < pva->GetSize(); i++)
        {
            point = (*pva)[i].point;
            if (path.Left(point.GetLength()) == point)
            {
                break;
            }
        }
        if (i >= pva->GetSize())
        {
            return false;
        }

        if (path.GetLength() == point.GetLength())
        {
            return true;
        }

        volume = (*pva)[i].volume;
        path   = path.Mid(point.GetLength());
    }
}
