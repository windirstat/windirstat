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

CIconImageList::~CIconImageList()
{
    m_LookupQueue.SuspendExecution();
    m_LookupQueue.CancelExecution();
}

void CIconImageList::Initialize()
{
    if (m_hImageList != nullptr) return;

    m_FilterOverride.RegisterFilter();

    const std::wstring & s = GetSysDirectory();
    SHFILEINFO sfi = {nullptr};
    const auto hil = reinterpret_cast<HIMAGELIST>(::SHGetFileInfo(s.c_str(), 0, &sfi, sizeof(sfi), WDS_SHGFI_DEFAULTS));

    Attach(ImageList_Duplicate(hil));

    VTRACE(L"System image list has {} icons", this->GetImageCount());
    for (short i = 0; i < static_cast<short>(this->GetImageCount()); i++)
    {
        m_IndexMap[i] = i;
    }

    m_JunctionImage = static_cast<short>(this->Add(CDirStatApp::Get()->LoadIcon(IDI_JUNCTION)));
    m_JunctionProtected = static_cast<short>(this->Add(CDirStatApp::Get()->LoadIcon(IDI_JUNCTION_PROTECTED)));
    m_FreeSpaceImage = static_cast<short>(this->Add(CDirStatApp::Get()->LoadIcon(IDI_FREE_SPACE)));
    m_UnknownImage = static_cast<short>(this->Add(CDirStatApp::Get()->LoadIcon(IDI_UNKNOWN)));
    m_EmptyImage = static_cast<short>(this->Add(CDirStatApp::Get()->LoadIcon(IDI_EMPTY)));

    // Cache icon for boot drive
    const auto driveLen = wcslen(L"C:\\");
    std::wstring drive(_MAX_PATH, wds::chrNull);
    const UINT u = ::GetWindowsDirectory(drive.data(), _MAX_PATH);
    if (u > driveLen) drive.resize(driveLen);
    m_MountPointImage = CacheIcon(drive, 0, FILE_ATTRIBUTE_REPARSE_POINT);

    // Cache icon for my computer
    m_MyComputerImage = CacheIcon(std::to_wstring(CSIDL_DRIVES), SHGFI_PIDL);

    // Use two threads for asynchronous icon lookup
    m_LookupQueue.StartThreads(MAX_ICON_THREADS, [this]()
    {
        while (const auto item = m_LookupQueue.Pop())
        {
            item->FetchShellInfo();
        }
    });
}

void CIconImageList::DoAsyncShellInfoLookup(COwnerDrawnListItem* item)
{
    m_LookupQueue.PushIfNotQueued(item);
}

void CIconImageList::ClearAsyncShellInfoQueue()
{
    ProcessMessagesUntilSignaled([this]
    {
        m_LookupQueue.SuspendExecution(true);
        m_LookupQueue.ResumeExecution();
    });
}

void CIconImageList::StopAsyncShellInfoQueue()
{
    ProcessMessagesUntilSignaled([this]
    {
        m_LookupQueue.SuspendExecution();
        m_LookupQueue.CancelExecution();
    });
}

// Returns the index of the added icon
short CIconImageList::CacheIcon(const std::wstring & path, UINT flags, const DWORD attr, std::wstring* psTypeName)
{
    ASSERT(AfxGetThread() != GetCurrentThread());
    ASSERT(m_hImageList != nullptr);
    flags |= WDS_SHGFI_DEFAULTS;

    if (psTypeName != nullptr)
    {
        // Also retrieve the file type description
        flags |= SHGFI_TYPENAME;
    }

    HIMAGELIST hil = nullptr;
    SHFILEINFO sfi{};

    if (flags & SHGFI_PIDL)
    {
        // assume folder id numeric encoded as string
        SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree);
        if (SUCCEEDED(SHGetSpecialFolderLocation(nullptr, std::stoi(path), &pidl)))
        {
            m_FilterOverride.SetDefaultHandler(false);
            hil = reinterpret_cast<HIMAGELIST>(::SHGetFileInfo(static_cast<LPCWSTR>(
                static_cast<LPVOID>(pidl)), attr, &sfi, sizeof(sfi), flags));
            m_FilterOverride.SetDefaultHandler(true);
        }
    }
    else
    {
        m_FilterOverride.SetDefaultHandler(false);
        hil = reinterpret_cast<HIMAGELIST>(::SHGetFileInfo(path.c_str(), attr, &sfi, sizeof(sfi), flags));
        m_FilterOverride.SetDefaultHandler(true);
    }

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
    if (const auto i = m_IndexMap.find(sfi.iIcon); i != m_IndexMap.end()) return i->second;
    m_FilterOverride.SetDefaultHandler(false);
    
    // Extract image and add to cache
    m_IndexMutex.unlock();
    const auto icon = static_cast<short>(this->Add(ImageList_ExtractIcon(NULL, hil, sfi.iIcon)));
    m_IndexMutex.lock();

    // Add to map
    m_FilterOverride.SetDefaultHandler(true);
    m_IndexMap[sfi.iIcon] = icon;
    return m_IndexMap[sfi.iIcon];
}

short CIconImageList::GetMyComputerImage() const
{
    return m_MyComputerImage;
}

short CIconImageList::GetMountPointImage() const
{
    return m_MountPointImage;
}

short CIconImageList::GetJunctionImage() const
{
    return m_JunctionImage;
}

short CIconImageList::GetJunctionProtectedImage() const
{
    return m_JunctionProtected;
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
    ASSERT(m_hImageList != nullptr);
    return m_FreeSpaceImage;
}

short CIconImageList::GetUnknownImage() const
{
    ASSERT(m_hImageList != nullptr);
    return m_UnknownImage;
}

short CIconImageList::GetEmptyImage() const
{
    ASSERT(m_hImageList != nullptr);
    return m_EmptyImage;
}
