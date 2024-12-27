// IconHandler.cpp - Implementation of CIconHandler
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
#include "IconHandler.h"
#include "GlobalHelpers.h"
#include "SmartPointer.h"
#include "MainFrame.h"

CIconHandler::~CIconHandler()
{
    m_LookupQueue.SuspendExecution();
    m_LookupQueue.CancelExecution();
}

void CIconHandler::Initialize()
{
    static bool isInitialized = false;
    if (isInitialized) return;
    isInitialized = true;

    m_FilterOverride.RegisterFilter();

    m_JunctionImage = CDirStatApp::Get()->LoadIcon(IDI_JUNCTION);
    m_JunctionProtected = CDirStatApp::Get()->LoadIcon(IDI_JUNCTION_PROTECTED);
    m_FreeSpaceImage = CDirStatApp::Get()->LoadIcon(IDI_FREE_SPACE);
    m_UnknownImage = CDirStatApp::Get()->LoadIcon(IDI_UNKNOWN);
    m_EmptyImage = CDirStatApp::Get()->LoadIcon(IDI_EMPTY);

    // Cache icon for boot drive
    const auto driveLen = wcslen(L"C:\\");
    std::wstring drive(_MAX_PATH, wds::chrNull);
    const UINT u = ::GetWindowsDirectory(drive.data(), _MAX_PATH);
    if (u > driveLen) drive.resize(driveLen);
    m_MountPointImage = FetchShellIcon(drive, 0, FILE_ATTRIBUTE_REPARSE_POINT);

    // Cache icon for my computer
    m_MyComputerImage = FetchShellIcon(std::to_wstring(CSIDL_DRIVES), SHGFI_PIDL);

    // Use two threads for asynchronous icon lookup
    m_LookupQueue.StartThreads(MAX_ICON_THREADS, [this]()
    {
        while (true)
        {
            auto [item, control, path, attr, icon, desc] = m_LookupQueue.Pop();
            if (item == nullptr) return;

            //  Query the icon from the system
            std::wstring descTmp;
            const HICON iconTmp = FetchShellIcon(path, 0, attr,
                desc != nullptr ? &descTmp : nullptr);

            // Join the UI thread and see if the item still exists
            // since it could have been deleted since originally
            // requested
            CMainFrame::Get()->InvokeInMessageThread([&]
            {
                const auto i = control->FindListItem(item);
                if (i == -1 || *icon != nullptr)
                {
                    // Delete icon if not longer in list or
                    // if already set by another thread
                    DestroyIcon(iconTmp);
                    return;
                }
                 
                *icon = iconTmp;
                if (desc != nullptr) *desc = descTmp;
                control->RedrawItems(i, i);
            });
        }
    });
}

void CIconHandler::DoAsyncShellInfoLookup(const IconLookup& lookupInfo)
{
    m_LookupQueue.PushIfNotQueued(lookupInfo);
}

void CIconHandler::ClearAsyncShellInfoQueue()
{
    ProcessMessagesUntilSignaled([this]
    {
        m_LookupQueue.SuspendExecution(true);
        m_LookupQueue.ResumeExecution();
    });
}

void CIconHandler::StopAsyncShellInfoQueue()
{
    ProcessMessagesUntilSignaled([this]
    {
        m_LookupQueue.SuspendExecution();
        m_LookupQueue.CancelExecution();
    });
}

void CIconHandler::DrawIcon(CDC* hdc, const HICON image, const CPoint & pt, const CSize& sz)
{
    m_FilterOverride.SetDefaultHandler(false);
    DrawIconEx(*hdc, pt.x, pt.y, image, sz.cx, sz.cy, 0, nullptr, DI_NORMAL);
    m_FilterOverride.SetDefaultHandler(true);
}

// Returns the index of the added icon
HICON CIconHandler::FetchShellIcon(const std::wstring & path, UINT flags, const DWORD attr, std::wstring* psTypeName)
{
    ASSERT(AfxGetThread() != GetCurrentThread());
    flags |= WDS_SHGFI_DEFAULTS;

    if (psTypeName != nullptr)
    {
        // Also retrieve the file type description
        flags |= SHGFI_TYPENAME;
    }

    bool success = false;
    SHFILEINFO sfi{};

    if (flags & SHGFI_PIDL)
    {
        // assume folder id numeric encoded as string
        SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree);
        if (SUCCEEDED(SHGetSpecialFolderLocation(nullptr, std::stoi(path), &pidl)))
        {
            m_FilterOverride.SetDefaultHandler(false);
            success = reinterpret_cast<HIMAGELIST>(::SHGetFileInfo(static_cast<LPCWSTR>(
                static_cast<LPVOID>(pidl)), attr, &sfi, sizeof(sfi), flags)) != nullptr;
            m_FilterOverride.SetDefaultHandler(true);
        }
    }
    else
    {
        m_FilterOverride.SetDefaultHandler(false);
        success = reinterpret_cast<HIMAGELIST>(::SHGetFileInfo(path.c_str(),
            attr, &sfi, sizeof(sfi), flags)) != nullptr;
        m_FilterOverride.SetDefaultHandler(true);
    }

    if (!success)
    {
        VTRACE(L"SHGetFileInfo() failed: {}", path);
        return GetEmptyImage();
    }

    if (psTypeName != nullptr)
    {
        *psTypeName = sfi.szTypeName;
    }

    // Check if icon is already in index and, if so, return
    return sfi.hIcon;
}

HICON CIconHandler::GetMyComputerImage() const
{
    return CopyIcon(m_MyComputerImage);
}

HICON CIconHandler::GetMountPointImage() const
{
    return CopyIcon(m_MountPointImage);
}

HICON CIconHandler::GetJunctionImage() const
{
    return CopyIcon(m_JunctionImage);
}

HICON CIconHandler::GetJunctionProtectedImage() const
{
    return CopyIcon(m_JunctionProtected);
}

HICON CIconHandler::GetFreeSpaceImage() const
{
    return CopyIcon(m_FreeSpaceImage);
}

HICON CIconHandler::GetUnknownImage() const
{
    return CopyIcon(m_UnknownImage);
}

HICON CIconHandler::GetEmptyImage() const
{
    return CopyIcon(m_EmptyImage);
}
