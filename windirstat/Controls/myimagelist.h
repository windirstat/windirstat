// myimagelist.h - Declaration of CMyImageList
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2017 WinDirStat Team (windirstat.net)
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

#ifndef __WDS_MYIMAGELIST_H__
#define __WDS_MYIMAGELIST_H__
#pragma once

#include <common/wds_constants.h>

//
// CMyImageList. Both CDirstatView and CTypeView use this central
// image list. It caches the system image list images as needed,
// and adds a few special images at initialization.
// This is because I don't want to deal with two images lists.
//
class CMyImageList: public CImageList
{
    static const UINT WDS_SHGFI_DEFAULTS = SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_ICON;
    static COLORREF greenify_(COLORREF c);
    static COLORREF blueify_(COLORREF c);
    static COLORREF yellowify_(COLORREF c);

public:
    CMyImageList();
    virtual ~CMyImageList();

    void initialize();

    int getMyComputerImage();
    int getMountPointImage();
    int getJunctionImage();
    int getFolderImage();
    int getFileImage(LPCTSTR path);
    int getExtImageAndDescription(LPCTSTR ext, CString& description);

    int getFilesFolderImage();
    int getFreeSpaceImage();
    int getUnknownImage();
    int getEmptyImage();

protected:
    int cacheIcon(LPCTSTR path, UINT flags, CString *psTypeName = NULL);
    CString getADriveSpec();
    void addCustomImages();

    CMap<int, int, int, int> m_indexMap;    // system image list index -> our index

    int m_filesFolderImage; // <Files>
    int m_freeSpaceImage;   // <Free Space>
    int m_unknownImage;     // <Unknown>
    int m_emptyImage;       // For items whose image cannot be found

    // Junction point
    int m_junctionImage;
};

#endif __WDS_MYIMAGELIST_H__
