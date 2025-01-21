// FinderBasic.h - Declaration of CFinderBasic
//
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

#include "stdafx.h"
#include <string>

class FinderBasic final
{
    using FILE_FULL_DIR_INFORMATION = struct {
        ULONG         NextEntryOffset;
        ULONG         FileIndex;
        LARGE_INTEGER CreationTime;
        LARGE_INTEGER LastAccessTime;
        LARGE_INTEGER LastWriteTime;
        LARGE_INTEGER ChangeTime;
        LARGE_INTEGER EndOfFile;
        LARGE_INTEGER AllocationSize;
        ULONG         FileAttributes;
        ULONG         FileNameLength;
        ULONG         EaSize;
        WCHAR         FileName[1];
    };

    std::wstring m_Search;
    std::wstring m_Base;
    std::wstring m_Name;
    HANDLE m_Handle = nullptr;
    DWORD m_InitialAttributes = INVALID_FILE_ATTRIBUTES;
    bool m_Firstrun = true;
    FILE_FULL_DIR_INFORMATION* m_CurrentInfo = nullptr;
    static constexpr std::wstring_view m_Dos = L"\\??\\";
    static constexpr std::wstring_view m_DosUNC = L"\\??\\UNC\\";
    static constexpr std::wstring_view m_Long = L"\\\\?\\";
    static constexpr std::wstring_view m_LongUNC = L"\\\\?\\UNC\\";

public:

    FinderBasic() = default;
    ~FinderBasic();

    bool FindNextFile();
    bool FindFile(const std::wstring& strFolder,const std::wstring& strName = L"", DWORD attr = INVALID_FILE_ATTRIBUTES);
    bool IsDirectory() const;
    bool IsDots() const;
    bool IsHidden() const;
    bool IsHiddenSystem() const;
    bool IsProtectedReparsePoint() const;
    DWORD GetAttributes() const;
    std::wstring GetFileName() const;
    ULONGLONG GetFileSizePhysical() const;
    ULONGLONG GetFileSizeLogical() const;
    FILETIME GetLastWriteTime() const;
    std::wstring GetFilePath() const;
    std::wstring GetFilePathLong() const;
    static bool DoesFileExist(const std::wstring& folder, const std::wstring& file = {});
    static std::wstring MakeLongPathCompatible(const std::wstring& path);
};
