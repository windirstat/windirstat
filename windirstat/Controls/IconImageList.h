// IconImageList.h - Declaration of CIconImageList
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

#include <string>
#include <unordered_map>
#include <shared_mutex>

//
// CIconImageList. Both CFileTreeView and CExtensionView use this central
// image list. It caches the system image list images as needed,
// and adds a few special images at initialization.
// This is because I don't want to deal with two images lists.
//
class CIconImageList final : public CImageList
{
    static constexpr UINT WDS_SHGFI_DEFAULTS = SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON | SHGFI_SYSICONINDEX;

public:
    CIconImageList() = default;
    ~CIconImageList() override = default;

    void Initialize();

    short GetMyComputerImage();
    short GetMountPointImage();
    short GetJunctionImage() const;
    short GetJunctionProtectedImage() const;
    short GetFolderImage();
    short GetFileImage(const std::wstring& path, DWORD attr = 0);
    short GetExtImageAndDescription(const std::wstring& ext, std::wstring& description, DWORD attr = 0);

    short GetFreeSpaceImage() const;
    short GetUnknownImage() const;
    short GetEmptyImage() const;

    short CacheIcon(const std::wstring& path, UINT flags = 0, DWORD attr = 0, std::wstring* psTypeName = nullptr);
    static std::wstring GetADriveSpec();
    void AddCustomImages();

    std::shared_mutex m_IndexMutex;
    std::unordered_map<int, short> m_IndexMap; // system image list index -> our index

    short m_FreeSpaceImage = -1;    // <Free Space>
    short m_UnknownImage = -1;      // <Unknown>
    short m_EmptyImage = -1;        // For items whose image cannot be found
    short m_JunctionImage = -1;     // For normal functions
    short m_JunctionProtected = -1; // For protected junctions
};
