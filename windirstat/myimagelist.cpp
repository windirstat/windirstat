// myimagelist.cpp	 - Implementation of CMyImageList
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003 Bernhard Seifert
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

#include "stdafx.h"
#include "windirstat.h"
#include "myimagelist.h"


CMyImageList::CMyImageList()
{
	m_filesFolderImage= 0;
	m_freeSpaceImage= 0;
	m_unknownImage= 0;
}

CMyImageList::~CMyImageList()
{
}

void CMyImageList::Initialize()
{
	if (m_hImageList == NULL)
	{
		SHFILEINFO sfi;
		HIMAGELIST hil= (HIMAGELIST)SHGetFileInfo(_T(""), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);

		Attach(ImageList_Duplicate(hil));

		for (int i=0; i < GetImageCount(); i++)
			m_indexMap.SetAt(i, i);

		AddCustomImages();
	}
}

int CMyImageList::CacheIcon(LPCTSTR path, UINT flags, CString *psTypeName)
{
	ASSERT(m_hImageList != NULL); // should have been Initialize()ed.

	flags|= SHGFI_SYSICONINDEX | SHGFI_SMALLICON;
	if (psTypeName != NULL)
		flags|= SHGFI_TYPENAME;
	
	SHFILEINFO sfi;
	HIMAGELIST hil= (HIMAGELIST)SHGetFileInfo(path, 0, &sfi, sizeof(sfi), flags);
	if (hil == NULL)
	{
		TRACE(_T("SHGetFileInfo() failed\n"));
		return GetEmptyImage();
	}
	
	if (psTypeName != NULL)
		*psTypeName= sfi.szTypeName;

	int i;
	if (!m_indexMap.Lookup(sfi.iIcon, i))
	{
		CImageList *sil= CImageList::FromHandle(hil);
	
		/*
			This doesn't work:
			IMAGEINFO ii;	
			VERIFY(sil->GetImageInfo(sfi.iIcon, &ii));

			i= Add(CBitmap::FromHandle(ii.hbmImage), CBitmap::FromHandle(ii.hbmMask));

			So we use this method:
		*/
		i= Add(sil->ExtractIcon(sfi.iIcon));
		m_indexMap.SetAt(sfi.iIcon, i);
	}

	return i;
}

int CMyImageList::GetMyComputerImage()
{
	LPITEMIDLIST pidl= NULL;
	HRESULT hr= SHGetSpecialFolderLocation(NULL, CSIDL_DRIVES, &pidl);
	if (FAILED(hr))
	{
		TRACE(_T("SHGetSpecialFolderLocation(CSIDL_DRIVES) failed!\n"));
		return 0;
	}

	int i= CacheIcon((LPCTSTR)pidl, SHGFI_PIDL);

	CoTaskMemFree(pidl);

	return i;
}

int CMyImageList::GetMountPointImage()
{
	return CacheIcon(GetADriveSpec(), 0); // The flag SHGFI_USEFILEATTRIBUTES doesn't work on W95.
}

int CMyImageList::GetFolderImage()
{
	CString s;
	GetSystemDirectory(s.GetBuffer(_MAX_PATH), _MAX_PATH);
	s.ReleaseBuffer();

	return CacheIcon(s, 0);
}

int CMyImageList::GetFileImage(LPCTSTR path)
{
	return CacheIcon(path, 0);
}

int CMyImageList::GetExtImageAndDescription(LPCTSTR ext, CString& description)
{
	return CacheIcon(ext, SHGFI_USEFILEATTRIBUTES, &description);
}


int CMyImageList::GetFilesFolderImage()
{
	ASSERT(m_hImageList != NULL); // should have been Initialize()ed.
	return m_filesFolderImage;
}

int CMyImageList::GetFreeSpaceImage()
{
	ASSERT(m_hImageList != NULL); // should have been Initialize()ed.
	return m_freeSpaceImage;
}

int CMyImageList::GetUnknownImage()
{
	ASSERT(m_hImageList != NULL); // should have been Initialize()ed.
	return m_unknownImage;
}

int CMyImageList::GetEmptyImage()
{
	ASSERT(m_hImageList != NULL);
	return m_emptyImage;
}


// Returns an arbitrary present drive
CString CMyImageList::GetADriveSpec()
{
	CString s;
	UINT u= GetWindowsDirectory(s.GetBuffer(_MAX_PATH), _MAX_PATH);
	s.ReleaseBuffer();
	if (u == 0 || s.GetLength() < 3 || s[1] != _T(':') || s[2] != _T('\\'))
		return _T("C:\\");
	return s.Left(3);
}

void CMyImageList::AddCustomImages()
{
	const CUSTOM_IMAGE_COUNT = 4;
	const COLORREF bgcolor= RGB(255,0,255);

	int folderImage= GetFolderImage();
	int driveImage= GetMountPointImage();

	IMAGEINFO ii;
	ZeroMemory(&ii, sizeof(ii));
	VERIFY(GetImageInfo(folderImage, &ii));
	CRect rc= ii.rcImage;

	CClientDC dcClient(CWnd::GetDesktopWindow());

	CDC dcmem;
	dcmem.CreateCompatibleDC(&dcClient);
	CBitmap target;
	target.CreateCompatibleBitmap(&dcClient, rc.Width() * CUSTOM_IMAGE_COUNT, rc.Height());
	{
		CSelectObject sotarget(&dcmem, &target);
		dcmem.FillSolidRect(0, 0, rc.Width() * CUSTOM_IMAGE_COUNT, rc.Height(), bgcolor);
		CPoint pt(0, 0);
		COLORREF safe= SetBkColor(bgcolor);
		VERIFY(Draw(&dcmem, folderImage, pt, ILD_NORMAL));
		pt.x+= rc.Width();
		VERIFY(Draw(&dcmem, driveImage, pt, ILD_NORMAL));
		pt.x+= rc.Width();
		VERIFY(Draw(&dcmem, driveImage, pt, ILD_NORMAL));
		SetBkColor(safe);

		// Now we re-color the imagees
		for (int i=0; i < rc.Width(); i++)
		for (int j=0; j < rc.Height(); j++)
		{
			// We "blueify" the folder image ("<Files>")
			COLORREF c= dcmem.GetPixel(i, j);
			if (c != bgcolor)
			{
				int brightness= (GetRValue(c) + GetGValue(c) + GetBValue(c)) / 3;
				dcmem.SetPixel(i, j, RGB(0, brightness, brightness));
			}
	
			// ... "greenify" the drive image ("<Free Space>")
			c= dcmem.GetPixel(rc.Width() + i, j);
			if (c != bgcolor)
			{
				int brightness= (GetRValue(c) + GetGValue(c) + GetBValue(c)) / 3;
				dcmem.SetPixel(rc.Width() + i, j, RGB(64, brightness, 64));
			}
		
			// ...and "yellowify" the drive image ("<Unknown>")
			c= dcmem.GetPixel(2 * rc.Width() + i, j);
			if (c != bgcolor)
			{
				int brightness= (GetRValue(c) + GetGValue(c) + GetBValue(c)) / 3;
				dcmem.SetPixel(2 * rc.Width() + i, j, RGB(brightness, brightness, 0));
			}
		}
	}
	int k= Add(&target, bgcolor);
	m_filesFolderImage= k++;
	m_freeSpaceImage= k++;
	m_unknownImage= k++;
	m_emptyImage= k++;
}
