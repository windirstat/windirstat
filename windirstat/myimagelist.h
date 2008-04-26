// myimagelist.h - Declaration of CMyImageList
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
// Author(s): - bseifert -> bernhard@windirstat.info
//            - assarbad -> oliver@windirstat.info
//
// $Id$

#ifndef __WDS_MYIMAGELIST_H__
#define __WDS_MYIMAGELIST_H__
#pragma once

#include "../common/wds_constants.h"

//
// CMyImageList. Both CDirstatView and CTypeView use this central
// image list. It caches the system image list images as needed,
// and adds 4 special images at initialization.
// This is because I don't want to deal with two images lists.
//
class CMyImageList: public CImageList
{
public:
	CMyImageList();
	virtual ~CMyImageList();

	void Initialize();

	int GetMyComputerImage();
	int GetMountPointImage();
	int GetJunctionImage();
	int GetFolderImage();
	int GetFileImage(LPCTSTR path);
	int GetExtImageAndDescription(LPCTSTR ext, CString& description);

	int GetFilesFolderImage();
	int GetFreeSpaceImage();
	int GetUnknownImage();
	int GetEmptyImage();

protected:
	int CacheIcon(LPCTSTR path, UINT flags, CString *psTypeName = NULL);
	CString GetADriveSpec();
	void AddCustomImages();
	
	CMap<int, int, int, int> m_indexMap;	// system image list index -> our index

	int m_filesFolderImage;	// <Files>
	int m_freeSpaceImage;	// <Free Space>
	int m_unknownImage;		// <Unknown>
	int m_emptyImage;		// For items whose image cannot be found

	// Junction point
	int m_junctionImage;
};

#endif __WDS_MYIMAGELIST_H__
