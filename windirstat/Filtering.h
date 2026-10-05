// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"

class CFiltering final
{
    static std::wstring ExtractIncludeAnchor(std::wstring_view pattern, bool useRegex);
    static std::wstring NormalizePathRegex(std::wstring_view pattern);
    static std::wregex CompilePattern(const std::wstring& pattern, bool useRegex, bool pathFilter);

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
    static std::optional<std::wstring> ValidateFilters(const std::wstring& text, bool useRegex, bool pathFilter);
    static bool IsFilterActive() { return FilterActive; }
    static std::wstring_view WithoutTrailingBackslashes(std::wstring_view path);
    static bool MatchesAnyPath(const std::wstring& path, const std::vector<std::wregex>& patterns);
    static bool IsFilteredOut(const std::wstring& directoryName, bool scanRoot = false);
    static bool IsFilteredOut(const std::wstring& fileName, const std::wstring& filePath,
        ULONGLONG fileSizeLogical, const FILETIME& lastWriteTime);
    static bool IsFilteredOut(const CItem* item);
};
