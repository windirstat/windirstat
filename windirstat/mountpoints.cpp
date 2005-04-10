// mountpoints.cpp	- Implementation of CMountPoints
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
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
// Author: bseifert@users.sourceforge.net, bseifert@daccord.net
//
// Last modified: $Date$

#include "stdafx.h"
#include "osspecific.h"

#include "mountpoints.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CMountPoints::~CMountPoints()
{
	Clear();
}

void CMountPoints::Clear()
{
	m_drive.RemoveAll();

	POSITION pos= m_volume.GetStartPosition();
	while (pos != NULL)
	{
		CString volume;
		PointVolumeArray *pva= NULL;
		m_volume.GetNextAssoc(pos, volume, pva);
		ASSERT_VALID(pva);
		delete pva;
	}
	m_volume.RemoveAll();
}

void CMountPoints::Initialize()
{
	Clear();

	if (!m_va.IsSupported())
		return;

	GetDriveVolumes();
	GetAllMountPoints();
}

void CMountPoints::GetDriveVolumes()
{
	m_drive.SetSize(32);

	DWORD drives= GetLogicalDrives();
	int i;
	DWORD mask= 0x00000001;
	for (i=0; i < 32; i++, mask <<= 1)
	{
		CString volume;

		if ((drives & mask) != 0)
		{
			CString s;
			s.Format(_T("%c:\\"), i + _T('A'));

			BOOL b= m_va.GetVolumeNameForVolumeMountPoint(s, volume.GetBuffer(_MAX_PATH), _MAX_PATH);
			volume.ReleaseBuffer();

			if (!b)
			{
				TRACE(_T("GetVolumeNameForVolumeMountPoint(%s) failed.\n"), s);
				volume.Empty();
			}
		}

		m_drive[i]= volume;
	}
}

void CMountPoints::GetAllMountPoints()
{
	TCHAR volume[_MAX_PATH];
	HANDLE hvol= m_va.FindFirstVolume(volume, countof(volume));
	if (hvol == INVALID_HANDLE_VALUE)
	{
		TRACE(_T("No volumes found.\r\n"));
		return;
	}

	for (BOOL bContinue=true; bContinue; bContinue= m_va.FindNextVolume(hvol, volume, countof(volume)))
	{
		PointVolumeArray *pva= new PointVolumeArray;
		ASSERT_VALID(pva);

		DWORD sysflags;
		CString fsname;
		BOOL b= GetVolumeInformation(volume, NULL, 0, NULL, NULL, &sysflags, fsname.GetBuffer(_MAX_PATH), _MAX_PATH);
		fsname.ReleaseBuffer();

		if (!b)
		{
			TRACE(_T("This file system (%s) is not ready.\r\n"), volume);
			m_volume.SetAt(volume, pva);		
			continue;
		}

		if ((sysflags & FILE_SUPPORTS_REPARSE_POINTS) == 0)
		{
			// No support for reparse points, and therefore for volume 
			// mount points, which are implemented using reparse points.
			TRACE(_T("This file system (%s) does not support volume mount points.\r\n"), volume);
			m_volume.SetAt(volume, pva);		
			continue;
		} 

		TCHAR point[_MAX_PATH];
		HANDLE h= m_va.FindFirstVolumeMountPoint(volume, point, countof(point));
		if (h == INVALID_HANDLE_VALUE)
		{
			TRACE(_T("No volume mount points found on %s.\r\n"), volume);
			m_volume.SetAt(volume, pva);		
			continue;
		} 

		for (BOOL bCont=true; bCont; bCont= m_va.FindNextVolumeMountPoint(h, point, countof(point)))
		{
			CString uniquePath= volume;
			uniquePath+= point;
			CString mountedVolume;

			BOOL b= m_va.GetVolumeNameForVolumeMountPoint(uniquePath, mountedVolume.GetBuffer(_MAX_PATH), _MAX_PATH);
			mountedVolume.ReleaseBuffer();

			if (!b)
			{
				TRACE(_T("GetVolumeNameForVolumeMountPoint(%s) failed.\r\n"), uniquePath);
				continue;
			}

			SPointVolume pv;
			pv.point= point;
			pv.volume= mountedVolume;

			pv.point.MakeLower();

			pva->Add(pv);
		}
		m_va.FindVolumeMountPointClose(h);

		m_volume.SetAt(volume, pva);		
	}

	(void)m_va.FindVolumeClose(hvol);

#ifdef _DEBUG
	POSITION pos= m_volume.GetStartPosition();
	while (pos != NULL)
	{
		CString volume;
		PointVolumeArray *pva= NULL;
		m_volume.GetNextAssoc(pos, volume, pva);
		pva->AssertValid();
	}
#endif

}


bool CMountPoints::IsMountPoint(CString path)
{
	if (path.GetLength() < 3 || path[1] != _T(':') || path[2] != _T('\\'))
	{
		// Don't know how to make out mount points on UNC paths ###
		return false;
	}

	ASSERT(path.GetLength() >= 3);
	ASSERT(path[1] == _T(':'));
	ASSERT(path[2] == _T('\\'));

	if (!m_va.IsSupported())
		return false;

	if (path.Right(1) != _T('\\'))
		path+= _T("\\");

	path.MakeLower();

	CString volume= m_drive[path[0] - _T('a')];
	path= path.Mid(3);

	return IsVolumeMountPoint(volume, path);
}

// Check wether the current item is a junction point but no volume mount point
// as the latter ones are treated differently (see above).
bool CMountPoints::IsJunctionPoint(CString path)
{
	if (IsMountPoint(path))
		return false;

	DWORD attr = GetFileAttributes(path);
	if (attr == INVALID_FILE_ATTRIBUTES)
		return false;

	return ((attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
}

bool CMountPoints::IsVolumeMountPoint(CString volume, CString path)
{
	for (;;)
	{
		PointVolumeArray *pva;
		if (!m_volume.Lookup(volume, pva))
		{
			TRACE(_T("CMountPoints: Volume(%s) unknown!\r\n"), volume);	
			return false;
		}

		CString point;
		for (int i=0; i < pva->GetSize(); i++)
		{
			point= (*pva)[i].point;
			if (path.Left(point.GetLength()) == point)
				break;
		}
		if (i >= pva->GetSize())
			return false;

		if (path.GetLength() == point.GetLength())
			return true;

		volume= (*pva)[i].volume;
		path= path.Mid(point.GetLength());
	}
}

// $Log$
// Revision 1.6  2005/04/10 16:49:30  assarbad
// - Some smaller fixes including moving the resource string version into the rc2 files
//
// Revision 1.5  2004/12/31 16:01:42  bseifert
// Bugfixes. See changelog 2004-12-31.
//
// Revision 1.4  2004/11/05 16:53:07  assarbad
// Added Date and History tag where appropriate.
//
