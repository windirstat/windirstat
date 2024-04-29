// IconImageList.cpp - Implementation of CIconImageList
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
#include "WinDirStat.h"
#include "IconImageList.h"
#include "GlobalHelpers.h"
#include "SmartPointer.h"

void CIconImageList::Initialize()
{
    if (m_hImageList == nullptr)
    {
        const std::wstring & s = GetSysDirectory();
        SHFILEINFO sfi = {nullptr};
        const auto hil = reinterpret_cast<HIMAGELIST>(::SHGetFileInfo(s.c_str(), 0, &sfi, sizeof(sfi), WDS_SHGFI_DEFAULTS));

        this->Attach(ImageList_Duplicate(hil));

        VTRACE(L"System image list has {} icons", this->GetImageCount());
        std::lock_guard lock(m_IndexMutex);
        for (short i = 0; i < static_cast<short>(this->GetImageCount()); i++)
        {
            m_IndexMap[i] = i;
        }

        this->AddCustomImages();
    }
}

// Returns the index of the added icon
short CIconImageList::CacheIcon(const std::wstring & path, UINT flags, const DWORD attr, std::wstring* psTypeName)
{
    ASSERT(m_hImageList != nullptr);
    flags |= WDS_SHGFI_DEFAULTS;

    if (psTypeName != nullptr)
    {
        // Also retrieve the file type description
        flags |= SHGFI_TYPENAME;
    }

    SHFILEINFO sfi{};
    const auto hil = reinterpret_cast<HIMAGELIST>(::SHGetFileInfo(path.c_str(), attr, &sfi, sizeof(sfi), flags));
    if (hil == nullptr)
    {
        VTRACE(L"SHGetFileInfo() failed: {}", path);
        return GetEmptyImage();
    }

    if (psTypeName != nullptr)
    {
        *psTypeName = sfi.szTypeName;
    }

    // Check if image is already in index and, if so, return
    std::lock_guard lock(m_IndexMutex);
    const auto i = m_IndexMap.find(sfi.iIcon);
    if (i != m_IndexMap.end()) return i->second;

    // Extract image and add to cache
    CImageList* sil = CImageList::FromHandle(hil); // does not have to be destroyed
    m_IndexMap[sfi.iIcon] = static_cast<short>(this->Add(sil->ExtractIcon(sfi.iIcon)));
    return m_IndexMap[sfi.iIcon];
}

short CIconImageList::GetMyComputerImage()
{
    SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree);
    if (FAILED(::SHGetSpecialFolderLocation(nullptr, CSIDL_DRIVES, &pidl)))
    {
        VTRACE(L"SHGetSpecialFolderLocation(CSIDL_DRIVES) failed!");
        return 0;
    }

    return CacheIcon(static_cast<LPCWSTR>(static_cast<LPVOID>(pidl)), SHGFI_PIDL);
}

short CIconImageList::GetMountPointImage()
{
    return CacheIcon(GetADriveSpec(), 0, FILE_ATTRIBUTE_REPARSE_POINT);
}

short CIconImageList::GetJunctionImage() const
{
    return m_JunctionImage;
}

short CIconImageList::GetJunctionProtectedImage() const
{
    return m_JunctionProtected;
}

short CIconImageList::GetFolderImage()
{
    const std::wstring & s = GetSysDirectory();
    return CacheIcon(s, 0, FILE_ATTRIBUTE_DIRECTORY);
}

short CIconImageList::GetFileImage(const std::wstring & path, const DWORD attr)
{
    return CacheIcon(path, 0, attr);
}

short CIconImageList::GetExtImageAndDescription(const std::wstring & ext, std::wstring& description, const DWORD attr)
{
    return CacheIcon(ext, 0, attr, &description);
}

short CIconImageList::GetFreeSpaceImage() const
{
    ASSERT(m_hImageList != nullptr); // should have been initialize()ed.
    return m_FreeSpaceImage;
}

short CIconImageList::GetUnknownImage() const
{
    ASSERT(m_hImageList != nullptr); // should have been initialize()ed.
    return m_UnknownImage;
}

short CIconImageList::GetEmptyImage() const
{
    ASSERT(m_hImageList != nullptr);
    return m_EmptyImage;
}

// Returns the boot drive icon
std::wstring CIconImageList::GetADriveSpec()
{
    std::wstring s(_MAX_PATH, wds::chrNull);
    const UINT u = ::GetWindowsDirectory(s.data(), _MAX_PATH);
    s.resize(wcslen(s.data()));
    if (u == 0 || s.size() < 3 || s[1] != wds::chrColon || s[2] != wds::chrBackslash)
    {
        return L"C:\\";
    }
    return s.substr(0, 3);
}

void CIconImageList::AddCustomImages()
{
    m_JunctionImage = static_cast<short>(this->Add(CDirStatApp::Get()->LoadIcon(IDI_JUNCTION)));
    m_JunctionProtected = static_cast<short>(this->Add(CDirStatApp::Get()->LoadIcon(IDI_JUNCTION_PROTECTED)));
    m_FreeSpaceImage = static_cast<short>(this->Add(CDirStatApp::Get()->LoadIcon(IDI_FREE_SPACE)));
    m_UnknownImage = static_cast<short>(this->Add(CDirStatApp::Get()->LoadIcon(IDI_UNKNOWN)));
    m_EmptyImage = static_cast<short>(this->Add(CDirStatApp::Get()->LoadIcon(IDI_EMPTY)));
}
