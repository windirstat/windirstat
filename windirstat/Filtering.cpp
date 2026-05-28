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
#include "Filtering.h"
#include "Finder.h"
#include "HelpersInterface.h"
#include "Options.h"

// --- Static member definitions ---

std::vector<std::wregex> CFiltering::ExcludeDirsRegex;
std::vector<std::wregex> CFiltering::ExcludeFilesRegex;
std::vector<std::wregex> CFiltering::IncludeDirsRegex;
std::vector<std::wregex> CFiltering::IncludeFilesRegex;
std::vector<std::wstring> CFiltering::IncludeDirsAnchors;
ULONGLONG CFiltering::SizeMinimumCalculated = 0;
FILETIME  CFiltering::MaxAgeFileTimeCutoff  = {};
bool      CFiltering::FilterActive          = false;

// --- Private helpers ---

static bool HasUnescapedTrailingDollar(std::wstring_view pattern)
{
    if (pattern.empty() || pattern.back() != L'$') return false;

    size_t slashCount = 0;
    for (size_t i = pattern.size() - 1; i > 0 && pattern[i - 1] == L'\\'; --i)
    {
        ++slashCount;
    }
    return slashCount % 2 == 0;
}

static std::wstring MatchDirectoryAndDescendants(std::wstring pattern)
{
    if (HasUnescapedTrailingDollar(pattern)) pattern.pop_back();
    return L"(?:" + pattern + L")(?:\\\\.*)?";
}

// Extracts the longest fixed-path prefix from an include-dir pattern that can
// be used as a scan anchor (i.e., the deepest directory that must exist for
// the pattern to ever match). Examples:
//   "C:\Windows\Sys*" -> "C:\Windows"
//   "*\foo" -> ""
std::wstring CFiltering::ExtractIncludeAnchor(std::wstring_view pattern, const bool useRegex)
{
    constexpr std::wstring_view literalEscapes = LR"(\.+*?^$|()[]{}/)";
    constexpr std::wstring_view regexSpecials = LR"(.+*?^$|()[]{})";
    constexpr std::wstring_view globWildcards = L"*?";

    std::wstring literal;
    bool truncated = false;
    for (size_t i = 0; i < pattern.size(); ++i)
    {
        const wchar_t c = pattern[i];
        if (useRegex)
        {
            if (i == 0 && c == L'^') continue;
            if (c == L'\\' && i + 1 < pattern.size() &&
                literalEscapes.find(pattern[i + 1]) != std::wstring_view::npos)
            {
                literal.push_back(pattern[++i]);
                continue;
            }
            if (c == L'$' && i + 1 == pattern.size()) break;
            if (c == L'\\' || regexSpecials.find(c) != std::wstring_view::npos)
            {
                truncated = true;
                break;
            }
        }
        else if (globWildcards.find(c) != std::wstring_view::npos)
        {
            truncated = true;
            break;
        }
        literal.push_back(c);
    }

    if (truncated)
    {
        // Trim back to the last directory boundary; chars after the last
        // backslash are part of the wildcarded segment, not a fixed dir.
        const auto bs = literal.find_last_of(L'\\');
        literal.resize(bs == std::wstring::npos ? 0 : bs);
    }
    while (!literal.empty() && literal.back() == L'\\') literal.pop_back();
    return literal;
}

// In path filters, treat single backslashes as Windows separators even in regex
// mode. Already escaped separators and escaped regex metacharacters are preserved.
std::wstring CFiltering::NormalizePathRegex(std::wstring_view pattern)
{
    constexpr std::wstring_view preservedEscapes = L"\\.+*?()[]{}^$|";
    std::wstring result;
    result.reserve(pattern.size());
    for (size_t i = 0; i < pattern.size(); ++i)
    {
        if (pattern[i] == L'\\' &&
            (i + 1 >= pattern.size() || preservedEscapes.find(pattern[i + 1]) == std::wstring_view::npos))
        {
            result += L"\\\\";
        }
        else if (pattern[i] == L'\\')
        {
            result.push_back(pattern[i]);
            result.push_back(pattern[++i]);
        }
        else
        {
            result.push_back(pattern[i]);
        }
    }
    return result;
}

// --- Public methods ---

bool CFiltering::IsFilterActive()
{
    return FilterActive;
}

void CFiltering::CompileFilters()
{
    ExcludeDirsRegex.clear();
    ExcludeFilesRegex.clear();
    IncludeDirsRegex.clear();
    IncludeFilesRegex.clear();
    IncludeDirsAnchors.clear();

    for (const auto& [optionString, optionRegex] : {
        std::pair{COptions::FilteringExcludeDirs.Obj(), std::ref(ExcludeDirsRegex)},
        std::pair{COptions::FilteringExcludeFiles.Obj(), std::ref(ExcludeFilesRegex)},
        std::pair{COptions::FilteringIncludeDirs.Obj(), std::ref(IncludeDirsRegex)},
        std::pair{COptions::FilteringIncludeFiles.Obj(), std::ref(IncludeFilesRegex)}})
    {
        const bool isIncludeDirs = &optionRegex.get() == &IncludeDirsRegex;
        const bool isExcludeDirs = &optionRegex.get() == &ExcludeDirsRegex;
        const bool isPathFilter = isIncludeDirs || isExcludeDirs;
        for (auto& token : SplitString(optionString, L'\n'))
        {
            try
            {
                while (!token.empty() && (token.back() == L'\r' || token.back() == L'\\')) token.pop_back();
                if (token.empty()) continue;

                // In regex mode, normalize lone backslashes in path-based patterns so
                // users can type V:\Folder without having to escape the path separators.
                const std::wstring normalized = (COptions::FilteringUseRegex && isPathFilter)
                    ? NormalizePathRegex(token) : token;

                // Directory filters apply to the directory itself and everything below it.
                std::wstring expr = COptions::FilteringUseRegex ? normalized : GlobToRegex(normalized, false);
                if (isIncludeDirs || isExcludeDirs) expr = MatchDirectoryAndDescendants(std::move(expr));
                optionRegex.get().emplace_back(expr,
                    std::regex_constants::icase | std::regex_constants::optimize);

                if (isIncludeDirs)
                {
                    IncludeDirsAnchors.emplace_back(ExtractIncludeAnchor(normalized, COptions::FilteringUseRegex));
                }
            }
            catch (const std::regex_error&)
            {
                DisplayError(Localization::Lookup(IDS_PAGE_FILTERING_INVALID_FILTER) + L" " + token);
            }
        }
    }

    // Calculate the total number of bytes to test as a scan minimum
    SizeMinimumCalculated = static_cast<ULONGLONG>(COptions::FilteringSizeMinimum) * (1ull << (10 * static_cast<int>(COptions::FilteringSizeUnits)));

    // Calculate the FILETIME cutoff for max-age filtering
    MaxAgeFileTimeCutoff = {};
    if (COptions::FilteringMaxAgeDays > 0)
    {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft); // Windows 7 compatible
        uint64_t t = (uint64_t(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        t -= COptions::FilteringMaxAgeDays.Obj() * 864'000'000'000ULL; // 100ns ticks per day
        MaxAgeFileTimeCutoff ={ DWORD(t), DWORD(t >> 32) };
    }

    // Cache whether any filter is active so callers can short-circuit cheaply
    FilterActive = !ExcludeDirsRegex.empty() || !ExcludeFilesRegex.empty() ||
                   !IncludeDirsRegex.empty()  || !IncludeFilesRegex.empty() ||
                   SizeMinimumCalculated != 0  ||
                   std::bit_cast<ULONGLONG>(MaxAgeFileTimeCutoff) != 0;

    // Rebuild toolbar to reflect status
    CMainFrame::Get()->RebuildToolBar();
}

std::wstring_view CFiltering::WithoutTrailingBackslashes(std::wstring_view path)
{
    while (!path.empty() && path.back() == L'\\') path.remove_suffix(1);
    return path;
}

bool CFiltering::MatchesAnyPath(const std::wstring& path, const std::vector<std::wregex>& patterns)
{
    if (patterns.empty()) return false;

    const auto matches = [&patterns](std::wstring_view candidate)
    {
        return std::ranges::any_of(patterns,
            [&candidate](const auto& pattern) { return std::regex_match(candidate.begin(), candidate.end(), pattern); });
    };

    if (matches(path)) return true;

    const std::wstring_view trimmed = WithoutTrailingBackslashes(path);
    return trimmed != path && matches(trimmed);
}

bool CFiltering::IsFilteredOut(const std::wstring& directoryName)
{
    if (!FilterActive) return false;
    if (MatchesAnyPath(directoryName, ExcludeDirsRegex)) return true;
    if (IncludeDirsRegex.empty()) return false;
    if (MatchesAnyPath(directoryName, IncludeDirsRegex)) return false;

    // Check if path is the same as or an ancestor of any include-anchor,
    // meaning we still need to descend into this directory to reach an included one.
    return !std::ranges::any_of(IncludeDirsAnchors, [&](const auto& anchor) -> bool
    {
        if (anchor.empty()) return true;
        const std::wstring_view a = WithoutTrailingBackslashes(anchor);
        const std::wstring_view d = WithoutTrailingBackslashes(directoryName);
        if (d.empty() || d.size() > a.size()) return false;
        if (_wcsnicmp(d.data(), a.data(), d.size()) != 0) return false;
        return d.size() == a.size() || a[d.size()] == L'\\';
    });
}

bool CFiltering::IsFilteredOut(const std::wstring& fileName, const std::wstring& filePath,
    ULONGLONG fileSizeLogical, const FILETIME& lastWriteTime)
{
    if (!FilterActive) return false;

    // Exclude files matching name filter
    if (!ExcludeFilesRegex.empty() && std::ranges::any_of(ExcludeFilesRegex,
        [&fileName](const auto& pattern) { return std::regex_match(fileName, pattern); }))
    {
        return true;
    }

    // Include only files whose path matches the include directory filter
    if (!IncludeDirsRegex.empty() && !MatchesAnyPath(filePath, IncludeDirsRegex))
    {
        return true;
    }

    // Include only files whose name matches the include name filter
    if (!IncludeFilesRegex.empty() && std::ranges::none_of(IncludeFilesRegex,
        [&fileName](const auto& pattern) { return std::regex_match(fileName, pattern); }))
    {
        return true;
    }

    // Exclude files below the minimum size threshold
    if (SizeMinimumCalculated > 0 && fileSizeLogical < SizeMinimumCalculated)
    {
        return true;
    }

    // Exclude files older than the max-age cutoff
    if (std::bit_cast<ULONGLONG>(MaxAgeFileTimeCutoff) != 0)
    {
        if (CompareFileTime(&lastWriteTime, &MaxAgeFileTimeCutoff) < 0)
            return true;
    }

    return false;
}

bool CFiltering::IsFilteredOut(const CItem* item)
{
    if (item->IsTypeOrFlag(IT_FILE))
        return IsFilteredOut(item->GetName(), item->GetPath(),
            item->GetSizeLogical(), item->GetLastChange());
    return IsFilteredOut(item->GetPath());
}
