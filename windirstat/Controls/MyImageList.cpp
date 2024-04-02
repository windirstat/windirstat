// MyImageList.cpp - Implementation of CMyImageList
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
#include "MyImageList.h"
#include "SmartPointer.h"

void CMyImageList::initialize()
{
    if (m_hImageList == nullptr)
    {
        CStringW s;
        ::GetSystemDirectory(s.GetBuffer(_MAX_PATH), _MAX_PATH);
        s.ReleaseBuffer();
        VTRACE(L"GetSystemDirectory() -> %s", s.GetString());

        SHFILEINFO sfi = {nullptr};
        const auto hil = reinterpret_cast<HIMAGELIST>(::SHGetFileInfo(s, 0, &sfi, sizeof(sfi), WDS_SHGFI_DEFAULTS));

        this->Attach(ImageList_Duplicate(hil));

        VTRACE(L"System image list has %i icons", this->GetImageCount());
        for (short i = 0; i < this->GetImageCount(); i++)
        {
            m_indexMap.SetAt(i, i);
        }

        this->addCustomImages();
    }
}

// Returns the index of the added icon
short CMyImageList::cacheIcon(LPCWSTR path, UINT flags, CStringW* psTypeName)
{
    ASSERT(m_hImageList != nullptr); // should have been initialize()ed.

    flags |= WDS_SHGFI_DEFAULTS;
    if (psTypeName != nullptr)
    {
        // Also retrieve the file type description
        flags |= SHGFI_TYPENAME;
    }

    SHFILEINFO sfi = {nullptr};
    const auto hil = reinterpret_cast<HIMAGELIST>(::SHGetFileInfo(path, 0, &sfi, sizeof(sfi), flags));
    if (hil == nullptr)
    {
        VTRACE(L"SHGetFileInfo() failed");
        return getEmptyImage();
    }

    if (psTypeName != nullptr)
    {
        *psTypeName = sfi.szTypeName;
    }

    short i;
    if (!m_indexMap.Lookup(sfi.iIcon, i)) // part of the system image list?
    {
        CImageList* sil = CImageList::FromHandle(hil); // does not have to be destroyed
        i = static_cast<short>(this->Add(sil->ExtractIcon(sfi.iIcon)));
        m_indexMap.SetAt(sfi.iIcon, i);
    }

    return i;
}

short CMyImageList::getMyComputerImage()
{
    SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree);
    if (FAILED(::SHGetSpecialFolderLocation(nullptr, CSIDL_DRIVES, &pidl)))
    {
        VTRACE(L"SHGetSpecialFolderLocation(CSIDL_DRIVES) failed!");
        return 0;
    }

    return cacheIcon(static_cast<LPCWSTR>(static_cast<LPVOID>(pidl)), SHGFI_PIDL);
}

short CMyImageList::getMountPointImage()
{
    return cacheIcon(getADriveSpec(), 0);
}

short CMyImageList::getJunctionImage() const
{
    // Intermediate solution until we find a nice icon for junction points
    return m_junctionImage;
}

short CMyImageList::getJunctionProtectedImage() const
{
    ASSERT(m_hImageList != nullptr); // should have been initialize()ed.
    return m_junctionProtected;
}

short CMyImageList::getFolderImage()
{
    CStringW s;
    ::GetSystemDirectory(s.GetBuffer(_MAX_PATH), _MAX_PATH);
    s.ReleaseBuffer();

    return cacheIcon(s, 0);
}

short CMyImageList::getFileImage(LPCWSTR path)
{
    return cacheIcon(path, 0);
}

short CMyImageList::getExtImageAndDescription(LPCWSTR ext, CStringW& description)
{
    return cacheIcon(ext, SHGFI_USEFILEATTRIBUTES, &description);
}

short CMyImageList::getFreeSpaceImage() const
{
    ASSERT(m_hImageList != nullptr); // should have been initialize()ed.
    return m_freeSpaceImage;
}

short CMyImageList::getUnknownImage() const
{
    ASSERT(m_hImageList != nullptr); // should have been initialize()ed.
    return m_unknownImage;
}

short CMyImageList::getEmptyImage() const
{
    ASSERT(m_hImageList != nullptr);
    return m_emptyImage;
}

// Returns an arbitrary present drive
// TODO: doesn't work on Vista and up because the system drive has a different icon
CStringW CMyImageList::getADriveSpec()
{
    CStringW s;
    const UINT u = ::GetWindowsDirectory(s.GetBuffer(_MAX_PATH), _MAX_PATH);
    s.ReleaseBuffer();
    if (u == 0 || s.GetLength() < 3 || s[1] != wds::chrColon || s[2] != wds::chrBackslash)
    {
        return L"C:\\";
    }
    return s.Left(3);
}

void CMyImageList::addCustomImages()
{
    m_junctionImage = static_cast<short>(this->Add(GetWDSApp()->LoadIcon(IDI_JUNCTION)));
    m_junctionProtected = static_cast<short>(this->Add(GetWDSApp()->LoadIcon(IDI_JUNCTION_PROTECTED)));
    m_freeSpaceImage = static_cast<short>(this->Add(GetWDSApp()->LoadIcon(IDI_FREE_SPACE)));
    m_unknownImage = static_cast<short>(this->Add(GetWDSApp()->LoadIcon(IDI_UNKNOWN)));
    m_emptyImage = static_cast<short>(this->Add(GetWDSApp()->LoadIcon(IDI_EMPTY)));
}
