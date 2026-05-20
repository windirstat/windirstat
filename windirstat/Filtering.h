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

#include "pch.h"

class CFiltering final
{
    static std::wstring ExtractIncludeAnchor(std::wstring_view pattern, bool useRegex);
    static std::wstring NormalizePathRegex(std::wstring_view pattern);

public:
    CFiltering() = delete;

    static std::vector<std::wregex> ExcludeDirsRegex;
    static std::vector<std::wregex> ExcludeFilesRegex;
    static std::vector<std::wregex> IncludeDirsRegex;
    static std::vector<std::wregex> IncludeFilesRegex;
    static std::vector<std::wstring> IncludeDirsAnchors;
    static ULONGLONG SizeMinimumCalculated;
    static FILETIME MaxAgeFileTimeCutoff;
    static bool FilterActive;

    static void CompileFilters();
    static bool IsFilterActive();
    static std::wstring WithoutTrailingBackslashes(std::wstring path);
    static bool MatchesAnyPath(const std::wstring& path, const std::vector<std::wregex>& patterns);
    static bool IsFilteredOut(const std::wstring& path);
    static bool IsFilteredOut(const std::wstring& fileName, const std::wstring& filePath,
        ULONGLONG fileSizeLogical, const FILETIME& lastWriteTime);
    static bool IsFilteredOut(const CItem* item);
};
