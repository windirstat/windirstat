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
#include "HelpersInterface.h"

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
        m_junctionImage = Icons::IconFromFontChar(L'⤷', RGB(0x3A, 0x3A, 0xFF), true);
        m_symlinkImage = Icons::IconFromFontChar(L'⤷', RGB(0x3A, 0xFF, 0x3A), true);
        m_junctionProtected = Icons::IconFromFontChar(L'⤷', RGB(0xFF, 0x3A, 0x3A), true);
        m_freeSpaceImage = Icons::IconFromFontChar(L'▢', RGB(0x3A, 0xCC, 0x3A), true);
        m_emptyImage = Icons::IconFromFontChar(L'▢', DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
        m_hardlinksImage = Icons::IconFromFontChar(L'⧉', DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
        m_dupesImage = Icons::IconFromFontChar(L'⧈', DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
        m_searchImage = Icons::IconFromFontChar(L'⊙', DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
        m_largestImage = Icons::IconFromFontChar(L'⋙', DarkMode::WdsSysColor(COLOR_WINDOWTEXT));
        m_unknownImage = Icons::IconFromFontChar(L'?', RGB(0xCC,0xB8,0x66), true);
        m_defaultFileImage = FetchShellIcon(GetSysDirectory() + L"\\~", 0, FILE_ATTRIBUTE_NORMAL);
        m_defaultFolderImage = FetchShellIcon(GetSysDirectory() + L"\\~", 0, FILE_ATTRIBUTE_DIRECTORY);

        // Cache icon for boot drive
        std::wstring drive(MAX_PATH, wds::chrNull);
        drive.resize(min(wcslen(L"C:\\"), GetWindowsDirectory(drive.data(), MAX_PATH)));
        m_mountPointImage = FetchShellIcon(drive, 0, FILE_ATTRIBUTE_REPARSE_POINT);

        // Cache icon for my computer
        m_myComputerImage = FetchShellIcon(std::to_wstring(CSIDL_DRIVES), SHGFI_PIDL);

        // Use two threads for asynchronous icon lookup
        m_lookupQueue.StartThreads(MAX_ICON_THREADS, [this]
        {
            if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) return;
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
                    if (i == -1) return;

                    *icon = iconTmp;
                    if (desc != nullptr) *desc = descTmp;
                    control->RedrawItems(i, i);
                });
            }
            CoUninitialize();
        });
    });
}

void CIconHandler::DoAsyncShellInfoLookup(IconLookup&& lookupInfo)
{
    const auto& [item, control, path, attr, icon, desc] = lookupInfo;

    // set default icon while loading
    if (*icon == nullptr)
    {
        *icon = (attr & FILE_ATTRIBUTE_DIRECTORY) ? m_defaultFolderImage : m_defaultFileImage;
    }

    // queue for lookup
    m_lookupQueue.PushIfNotQueued(std::move(lookupInfo));
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
    DrawIconEx(*hdc, pt.x, pt.y, image, sz.cx, sz.cy, 0, nullptr, DI_NORMAL);
}

// Returns the icon handle
HICON CIconHandler::FetchShellIcon(const std::wstring & path, UINT flags, const DWORD attr, std::wstring* psTypeName)
{
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
            success = std::bit_cast<HIMAGELIST>(::SHGetFileInfo(
                static_cast<LPCWSTR>(static_cast<LPVOID>(pidl)), attr, &sfi,
                sizeof(sfi), flags)) != nullptr;
        }
    }
    else
    {
        success = std::bit_cast<HIMAGELIST>(::SHGetFileInfo(path.c_str(),
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

HBITMAP Icons::MakeBitmap(const int size, const std::function<void(Graphics&)>& painter)
{
    // 4x super-sampling antialiased render in the canonical 64-unit space,
    // then bicubic downsample to the requested size.
    const int renderSize = size * 4;
    Bitmap renderBitmap(renderSize, renderSize, PixelFormat32bppPARGB);
    Graphics g(&renderBitmap);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.ScaleTransform(static_cast<REAL>(renderSize) / 64.0f,
                     static_cast<REAL>(renderSize) / 64.0f);
    painter(g);
    return ScaleBitmapHighQuality(renderBitmap, size, size, PixelFormat32bppPARGB);
}

HICON Icons::MakeIcon(const int size, const std::function<void(Graphics&)>& painter)
{
    SmartPointer<HBITMAP> color(DeleteObject, MakeBitmap(size, painter));
    if (color == nullptr) return nullptr;

    CBitmap mask;
    mask.CreateBitmap(size, size, 1, 1, nullptr);

    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmColor = color;
    ii.hbmMask = static_cast<HBITMAP>(mask.m_hObject);
    return CreateIconIndirect(&ii);
}
