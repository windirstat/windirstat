// MyImageList.h - Declaration of CMyImageList
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

#pragma once

//
// CMyImageList. Both CDirStatView and CTypeView use this central
// image list. It caches the system image list images as needed,
// and adds a few special images at initialization.
// This is because I don't want to deal with two images lists.
//
class CMyImageList final : public CImageList
{
    static constexpr UINT WDS_SHGFI_DEFAULTS = SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_ICON;
    static COLORREF greenify_(COLORREF c);
    static COLORREF blueify_(COLORREF c);
    static COLORREF yellowify_(COLORREF c);

public:
    CMyImageList();
    ~CMyImageList() override = default;

    void initialize();

    short getMyComputerImage();
    short getMountPointImage();
    short getJunctionImage() const;
    short getFolderImage();
    short getFileImage(LPCWSTR path);
    short getExtImageAndDescription(LPCWSTR ext, CStringW& description);

    short getFilesFolderImage() const;
    short getFreeSpaceImage();
    short getUnknownImage();
    short getEmptyImage();

protected:
    short cacheIcon(LPCWSTR path, UINT flags, CStringW* psTypeName = nullptr);
    CStringW getADriveSpec();
    void addCustomImages();

    CMap<int, int, short, short> m_indexMap; // system image list index -> our index

    short m_filesFolderImage; // <Files>
    short m_freeSpaceImage;   // <Free Space>
    short m_unknownImage;     // <Unknown>
    short m_emptyImage;       // For items whose image cannot be found

    // Junction point
    short m_junctionImage;
};
