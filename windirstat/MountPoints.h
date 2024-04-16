// MountPoints.h - Declaratio of CMountPoins
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

#pragma once

#include <string>
#include <vector>

class CReparsePoints final
{
    static bool IsReparseType(const std::wstring& path, DWORD tag_type);

    std::vector<std::wstring> m_mountpoints;

public:

    void Initialize();
    bool IsMountPoint(const CStringW& path, DWORD attr = INVALID_FILE_ATTRIBUTES) const;
    bool IsJunction(const CStringW& path, DWORD attr = INVALID_FILE_ATTRIBUTES) const;
    static bool IsReparsePoint(DWORD attr);
};
