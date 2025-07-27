// FinderBasic.h - Declaration of FinderBasic
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

#include <stdafx.h>
#include "Item.h"

#include <string>

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
    virtual inline DWORD GetAttributes() const = 0;
    virtual inline ULONGLONG GetFileSizePhysical() const = 0;
    virtual inline ULONGLONG GetFileSizeLogical() const = 0;
    virtual inline FILETIME GetLastWriteTime() const = 0;
    virtual std::wstring GetFilePath() const = 0;
    virtual std::wstring GetFileName() const = 0;
    virtual ULONG GetIndex() const { return 0; }

    bool IsReparsePoint() const
    {
        return (GetAttributes() & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    }

    bool IsDirectory() const
    {
        return (GetAttributes() & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    bool IsDots() const
    {
        return GetFileName() == L"." || GetFileName() == L"..";
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
};
