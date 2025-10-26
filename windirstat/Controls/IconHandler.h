// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
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

#include "TreeListControl.h"
#include "BlockingQueue.h"
#include "OleFilterOverride.h"

#include <string>
#include <unordered_map>
#include <mutex>

//
// CIconHandler. Handles all shell information lookup.
//
class CIconHandler final
{
    static constexpr UINT WDS_SHGFI_DEFAULTS = SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON | SHGFI_ICON | SHGFI_ADDOVERLAYS | SHGFI_SYSICONINDEX | SHGFI_OVERLAYINDEX;
    static constexpr auto MAX_ICON_THREADS = 4;

    std::mutex m_CachedIconMutex;
    std::unordered_map<int, HICON> m_CachedIcons;

public:
    CIconHandler() = default;
    ~CIconHandler();

    using IconLookup = std::tuple<COwnerDrawnListItem*, COwnerDrawnListControl*,
        std::wstring, DWORD, HICON*, std::wstring*>;

    void Initialize();
    void DoAsyncShellInfoLookup(const IconLookup& lookupInfo);
    void DrawIcon(const CDC* hdc, HICON image, const CPoint& pt, const CSize& sz);
    void ClearAsyncShellInfoQueue();
    void StopAsyncShellInfoQueue();
    HICON GetMyComputerImage() const;
    HICON GetMountPointImage() const;
    HICON GetJunctionImage() const;
    HICON GetJunctionProtectedImage() const;
    HICON GetFreeSpaceImage() const;
    HICON GetUnknownImage() const;
    HICON GetEmptyImage() const;

    HICON FetchShellIcon(const std::wstring& path, UINT flags = 0, DWORD attr = FILE_ATTRIBUTE_NORMAL, std::wstring* psTypeName = nullptr);

    BlockingQueue<IconLookup> m_LookupQueue;
    COleFilterOverride m_FilterOverride;

    HICON m_FreeSpaceImage = nullptr;    // <Free Space>
    HICON m_UnknownImage = nullptr;      // <Unknown>
    HICON m_EmptyImage = nullptr;        // For items whose icon cannot be found
    HICON m_JunctionImage = nullptr;     // For normal functions
    HICON m_JunctionProtected = nullptr; // For protected junctions
    HICON m_MountPointImage = nullptr;   // Mount point icon
    HICON m_MyComputerImage = nullptr;   // My computer icon
};
