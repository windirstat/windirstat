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

#pragma once

#include "stdafx.h"
#include "Item.h"

class Finder
{
protected:

    static constexpr std::wstring_view m_Dos = L"\\??\\";
    static constexpr std::wstring_view m_DosUNC = L"\\??\\UNC\\";
    static constexpr std::wstring_view m_Long = L"\\\\?\\";
    static constexpr std::wstring_view m_LongUNC = L"\\\\?\\UNC\\";

public:

    virtual bool FindNext() = 0;
    virtual bool FindFile(const CItem* item) = 0;
    virtual bool IsDots() const = 0;
    virtual inline DWORD GetAttributes() const = 0;
    virtual inline ULONGLONG GetFileSizePhysical() const = 0;
    virtual inline ULONGLONG GetFileSizeLogical() const = 0;
    virtual inline FILETIME GetLastWriteTime() const = 0;
    virtual std::wstring GetFilePath() const = 0;
    virtual std::wstring GetFileName() const = 0;
    virtual ULONG GetIndex() const { return 0; }
    virtual DWORD GetReparseTag() const = 0;

    bool IsReparsePoint() const
    {
        return (GetAttributes() & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    }

    bool IsDirectory() const
    {
        return (GetAttributes() & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    bool IsHidden() const
    {
        return (GetAttributes() & FILE_ATTRIBUTE_HIDDEN) != 0;
    }

    bool IsHiddenSystem() const
    {
        constexpr DWORD hiddenSystem = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM;
        return (GetAttributes() & hiddenSystem) == hiddenSystem;
    }

    bool IsProtectedReparsePoint() const
    {
        constexpr DWORD protect = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_REPARSE_POINT;
        return (GetAttributes() & protect) == protect;
    }

    virtual std::wstring GetFilePathLong() const
    {
        return MakeLongPathCompatible(GetFilePath());
    }

    static std::wstring MakeLongPathCompatible(const std::wstring& path)
    {
        if (path.find(L":\\", 1) == 1) return m_Long.data() + path;
        if (path.starts_with(L"\\\\?")) return path;
        if (path.starts_with(L"\\\\")) return m_LongUNC.data() + path.substr(2);
        return path;
    }

    using REPARSE_DATA_BUFFER = struct REPARSE_DATA_BUFFER {
        ULONG  ReparseTag;
        USHORT ReparseDataLength;
        USHORT Reserved;
        USHORT SubstituteNameOffset;
        USHORT SubstituteNameLength;
        USHORT PrintNameOffset;
        USHORT PrintNameLength;
        WCHAR PathBuffer[1];
    };

    static bool IsMountPoint(REPARSE_DATA_BUFFER & reparseBuffer)
    {
        if (reparseBuffer.ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
        {
            const auto volumeIdentifier = LR"(\??\Volume)";
            const auto path = ByteOffset<WCHAR>(reparseBuffer.PathBuffer, reparseBuffer.SubstituteNameOffset);
            if (_wcsnicmp(path, volumeIdentifier, min(reparseBuffer.SubstituteNameLength, wcslen(volumeIdentifier))) == 0)
            {
                return true;
            }
        }

        return false;
    }

    bool IsOffVolumeReparsePoint() const
    {
        return GetReparseTag() == IO_REPARSE_TAG_MOUNT_POINT ||
            GetReparseTag() == IO_REPARSE_TAG_SYMLINK;
    }

    static bool IsJunction(REPARSE_DATA_BUFFER& reparseBuffer)
    {
        return reparseBuffer.ReparseTag == IO_REPARSE_TAG_MOUNT_POINT &&
            !IsMountPoint(reparseBuffer);
    }
};
