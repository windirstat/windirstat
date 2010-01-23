// mountpoints.h - Declaratio of CMountPoins
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2006, 2008 Oliver Schneider (assarbad.net)
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
// Author(s): - bseifert -> http://windirstat.info/contact/bernhard/
//            - assarbad -> http://windirstat.info/contact/oliver/
//
// $Id$

#ifndef __WDS_MOUNTPOINTS_H__
#define __WDS_MOUNTPOINTS_H__
#pragma once

#include "../common/wds_constants.h"

class CReparsePoints
{
    struct SPointVolume
    {
        CString point;  // Path like "mount\backup\"
        CString volume; // Volume identifier
    };

    typedef CArray<SPointVolume, SPointVolume&> PointVolumeArray;

public:
    ~CReparsePoints();
    void Initialize();
    bool IsVolumeMountPoint(CString path);
    bool IsFolderJunction(CString path);

private:
    void Clear();
    void GetDriveVolumes();
    void GetAllMountPoints();

    bool IsVolumeMountPoint(CString volume, CString path);

    CVolumeApi m_va;

    // m_drive contains the volume identifiers of the Drives A:, B: etc.
    // mdrive[0] = Volume identifier of A:\.
    CArray<CString, LPCTSTR> m_drive;

    // m_volume maps all volume identifiers to PointVolumeArrays
    CMap<CString, LPCTSTR, PointVolumeArray *, PointVolumeArray *> m_volume;
};

#endif // __WDS_MOUNTPOINTS_H__
