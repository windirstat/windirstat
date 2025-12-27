// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "pch.h"
#include "IconHandler.h"

struct CFilterGuard
{
    COleFilterOverride& f;
    explicit CFilterGuard(COleFilterOverride& x) : f(x) { f.SetDefaultHandler(false); }
    ~CFilterGuard() { f.SetDefaultHandler(true); }
};

CIconHandler::~CIconHandler()
{
    m_lookupQueue.SuspendExecution();
    m_lookupQueue.CancelExecution();
}

void CIconHandler::Initialize()
{
    static std::once_flag s_once;
    std::call_once(s_once, [this]
    {
        m_filterOverride.RegisterFilter();

        m_junctionImage = DarkMode::LightenIcon(CDirStatApp::Get()->LoadIcon(IDI_JUNCTION));
        m_junctionProtected = DarkMode::LightenIcon(CDirStatApp::Get()->LoadIcon(IDI_JUNCTION_PROTECTED), true);
        m_freeSpaceImage = DarkMode::LightenIcon(CDirStatApp::Get()->LoadIcon(IDI_FREE_SPACE), true);
        m_unknownImage = DarkMode::LightenIcon(CDirStatApp::Get()->LoadIcon(IDI_UNKNOWN), true);
        m_emptyImage = DarkMode::LightenIcon(CDirStatApp::Get()->LoadIcon(IDI_EMPTY), true);
        m_hardlinksImage = DarkMode::LightenIcon(CDirStatApp::Get()->LoadIcon(IDI_HARDLINKS), true);

        // Cache icon for boot drive
        std::wstring drive(_MAX_PATH, wds::chrNull);
        drive.resize(min(wcslen(L"C:\\"), GetWindowsDirectory(drive.data(), _MAX_PATH)));
        m_mountPointImage = FetchShellIcon(drive, 0, FILE_ATTRIBUTE_REPARSE_POINT);

        // Cache icon for my computer
        m_myComputerImage = FetchShellIcon(std::to_wstring(CSIDL_DRIVES), SHGFI_PIDL);

        // Use two threads for asynchronous icon lookup
        m_lookupQueue.StartThreads(MAX_ICON_THREADS, [this]
        {
            for (auto itemOpt = m_lookupQueue.Pop(); itemOpt.has_value(); itemOpt = m_lookupQueue.Pop())
            {
                // Fetch item from queue
                auto& [item, control, path, attr, icon, desc] = itemOpt.value();

                // Query the icon from the system
                std::wstring descTmp;
                const HICON iconTmp = FetchShellIcon(
                    path, 0, attr, desc != nullptr ? &descTmp : nullptr);

                // Join the UI thread and see if the item still exists
                // since it could have been deleted since originally
                // requested
                CMainFrame::Get()->InvokeInMessageThread([&]
                {
                    const auto i = control->FindListItem(item);
                    if (i == -1 || *icon != nullptr) return;
                     
                    *icon = iconTmp;
                    if (desc != nullptr) *desc = descTmp;
                    control->RedrawItems(i, i);
                });
            }
        });
    });
}

void CIconHandler::DoAsyncShellInfoLookup(const IconLookup& lookupInfo)
{
    m_lookupQueue.PushIfNotQueued(lookupInfo);
}

void CIconHandler::ClearAsyncShellInfoQueue()
{
    ProcessMessagesUntilSignaled([this]
    {
        m_lookupQueue.SuspendExecution(true);
        m_lookupQueue.ResumeExecution();
    });
}

void CIconHandler::StopAsyncShellInfoQueue()
{
    ProcessMessagesUntilSignaled([this]
    {
        m_lookupQueue.SuspendExecution();
        m_lookupQueue.CancelExecution();
    });
}

void CIconHandler::DrawIcon(const CDC* hdc, const HICON image, const CPoint & pt, const CSize& sz)
{
    CFilterGuard guard(m_filterOverride);
    DrawIconEx(*hdc, pt.x, pt.y, image, sz.cx, sz.cy, 0, nullptr, DI_NORMAL);
}

// Returns the icon handle
HICON CIconHandler::FetchShellIcon(const std::wstring & path, UINT flags, const DWORD attr, std::wstring* psTypeName)
{
    ASSERT(AfxGetThread() != GetCurrentThread());
    flags |= WDS_SHGFI_DEFAULTS;

    // Also retrieve the file type description
    if (psTypeName != nullptr && !path.empty()) flags |= SHGFI_TYPENAME;

    SHFILEINFO sfi{};
    bool success = false;

    if (flags & SHGFI_PIDL)
    {
        // Assume folder id numeric encoded as string
        SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree);
        if (SUCCEEDED(SHGetSpecialFolderLocation(nullptr, std::stoi(path), &pidl)))
        {
            CFilterGuard guard(m_filterOverride);
            success = reinterpret_cast<HIMAGELIST>(::SHGetFileInfo(
                static_cast<LPCWSTR>(static_cast<LPVOID>(pidl)), attr, &sfi,
                sizeof(sfi), flags)) != nullptr;
        }
    }
    else
    {
        CFilterGuard guard(m_filterOverride);
        success = reinterpret_cast<HIMAGELIST>(::SHGetFileInfo(path.c_str(),
            attr, &sfi, sizeof(sfi), flags)) != nullptr;
    }

    if (!success)
    {
        VTRACE(L"SHGetFileInfo() failed: {}", path);
        return GetEmptyImage();
    }

    if (psTypeName != nullptr)
    {
        *psTypeName = path.empty() ?
            Localization::Lookup(IDS_EXTENSION_MISSING) : sfi.szTypeName;
    }

    std::scoped_lock lock(m_cachedIconMutex);
    if (const auto it = m_cachedIcons.find(sfi.iIcon); it != m_cachedIcons.end())
    {
        if (sfi.hIcon != nullptr) DestroyIcon(sfi.hIcon);
        return it->second;
    }

    m_cachedIcons[sfi.iIcon] = sfi.hIcon;
    return sfi.hIcon;
}
