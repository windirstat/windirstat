// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "Filtering.h"
#include "StorageSource.h"
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

static bool HasUnescapedTrailingDollar(const std::wstring_view pattern)
{
    if (pattern.empty() || pattern.back() != L'$') return false;

    size_t slashCount = 0;
    for (size_t i = pattern.size() - 1; i > 0 && pattern[i - 1] == L'\\'; --i)
    {
        ++slashCount;
    }
    return slashCount % 2 == 0;
}

static bool CompareThreshold(const ULONGLONG value, const ULONGLONG threshold, const int comparison)
{
    return comparison == 1 ? (value > threshold) : (value < threshold);
}

static bool CompareFileAge(const FILETIME& lastWriteTime, const FILETIME& cutoff, const int comparison)
{
    const int lastWriteCmp = CompareFileTime(&lastWriteTime, &cutoff);
    return comparison == 1 ? (lastWriteCmp < 0) : (lastWriteCmp > 0);
}

// Extracts the longest fixed-path prefix from an include-dir pattern that can
// be used as a scan anchor (i.e., the deepest directory that must exist for
// the pattern to ever match). Examples:
//   "C:\Windows\Sys*" -> "C:\Windows"
//   "*\foo" -> ""
std::wstring CFiltering::ExtractIncludeAnchor(const std::wstring_view pattern, const bool useRegex)
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
                (literalEscapes.contains(pattern[i + 1]) || iswspace(pattern[i + 1]) || iswpunct(pattern[i + 1])))
            {
                literal.push_back(pattern[++i]);
                continue;
            }
            if (c == L'$' && i + 1 == pattern.size()) break;
            if (c == L'\\' || regexSpecials.contains(c))
            {
                truncated = true;
                break;
            }
        }
        else if (globWildcards.contains(c))
        {
            truncated = true;
            break;
        }
        literal.push_back(c);
    }

    if (truncated)
    {
        // Trim wildcarded components; a partial remote authority provides no fixed directory anchor.
        const bool remote = StorageSource::IsPath(literal);
        const auto bs = literal.find_last_of(remote ? L'/' : L'\\');
        literal.resize(bs == std::wstring::npos || (remote && bs < literal.find(L"://") + 3) ? 0 : bs);
    }
    while (!literal.empty() && literal.back() == L'\\') literal.pop_back();
    return literal;
}

// In path filters, treat single backslashes as Windows separators even in regex
// mode, except for valid regex escapes, which retain their regex meaning.
std::wstring CFiltering::NormalizePathRegex(const std::wstring_view pattern)
{
    constexpr std::wstring_view preservedEscapes = LR"(\.+*?()[]{}^$|/bBdDsSwWfnrtv0cux123456789)";
    std::wstring result;
    result.reserve(pattern.size());
    for (size_t i = 0; i < pattern.size(); ++i)
    {
        if (pattern[i] == L'\\' &&
            (i + 1 >= pattern.size() || (!preservedEscapes.contains(pattern[i + 1]) &&
                !iswspace(pattern[i + 1]) && !iswpunct(pattern[i + 1]))))
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

std::wregex CFiltering::CompilePattern(const std::wstring& pattern, const bool useRegex, const bool pathFilter)
{
    // In regex mode, normalize lone backslashes in path-based patterns so
    // users can type V:\Folder without having to escape the path separators.
    const std::wstring normalized = useRegex && pathFilter ? NormalizePathRegex(pattern) : pattern;

    // Directory filters apply to the directory itself and everything below it.
    const bool remote = StorageSource::IsPath(pattern) ||
        pattern.starts_with(L'^') && StorageSource::IsPath(std::wstring_view(pattern).substr(1));
    std::wstring expression = useRegex ? normalized : GlobToRegex(normalized, false);
    if (pathFilter)
    {
        if (HasUnescapedTrailingDollar(expression)) expression.pop_back();
        expression = L"(?:" + expression + (remote ? L")(?:/.*)?" : L")(?:\\\\.*)?");
    }
    return std::wregex(expression, std::regex_constants::optimize |
        (remote ? std::regex_constants::ECMAScript : std::regex_constants::icase));
}

std::optional<std::wstring> CFiltering::ValidateFilters(
    const std::wstring& text, const bool useRegex, const bool pathFilter)
{
    for (auto& token : SplitString(text, L'\n'))
    {
        while (!token.empty() && (token.back() == L'\r' || token.back() == L'\\')) token.pop_back();
        if (token.empty()) continue;
        try { CompilePattern(token, useRegex, pathFilter); }
        catch (const std::regex_error&) { return token; }
    }
    return {};
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

                optionRegex.get().push_back(CompilePattern(token, COptions::FilteringUseRegex, isPathFilter));

                if (isIncludeDirs)
                {
                    IncludeDirsAnchors.emplace_back(ExtractIncludeAnchor(COptions::FilteringUseRegex
                        ? NormalizePathRegex(token) : token, COptions::FilteringUseRegex));
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
        uint64_t t = std::bit_cast<uint64_t>(CurrentSystemFileTime());
        t -= COptions::FilteringMaxAgeDays.Obj() * 864'000'000'000ULL; // 100ns ticks per day
        MaxAgeFileTimeCutoff = std::bit_cast<FILETIME>(t);
    }

    // Cache whether any filter is active so callers can short-circuit cheaply
    FilterActive = !ExcludeDirsRegex.empty() || !ExcludeFilesRegex.empty() ||
                   !IncludeDirsRegex.empty()  || !IncludeFilesRegex.empty() ||
                   SizeMinimumCalculated != 0  ||
                   std::bit_cast<ULONGLONG>(MaxAgeFileTimeCutoff) != 0;

    // Rebuild toolbar to reflect status
    if (auto* frame = CMainFrame::Get(); frame != nullptr) frame->RebuildToolBar();
}

std::wstring_view CFiltering::WithoutTrailingBackslashes(std::wstring_view path)
{
    const wchar_t separator = StorageSource::IsPath(path) ? L'/' : L'\\';
    if (separator == L'/') path = path.substr(0, path.find(L'?'));
    while (!path.empty() && path.back() == separator) path.remove_suffix(1);
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

    // Match remote ancestors at URI boundaries, preserving connection options after each candidate path.
    if (StorageSource::IsPath(path))
    {
        const auto query = path.find(L'?');
        std::wstring candidate = path.substr(0, query);
        const std::wstring options = query == path.npos ? std::wstring{} : path.substr(query);
        const auto rootEnd = candidate.find(L'/', candidate.find(L"://") + 3);
        for (;;)
        {
            if (matches(candidate) || (!options.empty() && matches(candidate + options))) return true;
            if (candidate.ends_with(L'/')) { candidate.pop_back(); continue; }
            const auto slash = candidate.rfind(L'/');
            if (slash == candidate.npos || slash < rootEnd) return false;
            candidate.resize(slash + 1);
        }
    }

    const std::wstring_view trimmed = WithoutTrailingBackslashes(path);
    return trimmed != path && matches(trimmed);
}

bool CFiltering::IsFilteredOut(const std::wstring& directoryName, const bool scanRoot)
{
    if (!FilterActive) return false;
    if (MatchesAnyPath(directoryName, ExcludeDirsRegex)) return true;
    if (IncludeDirsRegex.empty()) return false;
    if (MatchesAnyPath(directoryName, IncludeDirsRegex)) return false;

    // Object scan roots may end within a key component rather than at a directory boundary.
    const bool objectPrefix = scanRoot &&
        (directoryName.starts_with(L"s3://") || directoryName.starts_with(L"azure://")) &&
        !std::wstring_view(directoryName).substr(0, directoryName.find(L'?')).ends_with(L'/');

    // Check if path is the same as or an ancestor of any include-anchor,
    // meaning we still need to descend into this directory to reach an included one.
    return !std::ranges::any_of(IncludeDirsAnchors, [&](const auto& anchor) -> bool
    {
        if (anchor.empty()) return true;
        const std::wstring_view a = WithoutTrailingBackslashes(anchor);
        const std::wstring_view d = WithoutTrailingBackslashes(directoryName);
        if (d.empty() || d.size() > a.size()) return false;
        const bool remote = StorageSource::IsPath(d);
        if (remote ? d != a.substr(0, d.size()) : _wcsnicmp(d.data(), a.data(), d.size()) != 0) return false;
        return d.size() == a.size() || objectPrefix || a[d.size()] == (remote ? L'/' : L'\\');
    });
}

bool CFiltering::IsFilteredOut(const std::wstring& fileName, const std::wstring& filePath,
    const ULONGLONG fileSizeLogical, const FILETIME& lastWriteTime)
{
    if (!FilterActive) return false;

    // Exclude files beyond the size threshold based on comparison selection
    if (SizeMinimumCalculated > 0 &&
        CompareThreshold(fileSizeLogical, SizeMinimumCalculated, COptions::FilteringSizeComparison))
    {
        return true;
    }

    // Exclude files outside the max-age cutoff based on comparison selection
    if (std::bit_cast<ULONGLONG>(MaxAgeFileTimeCutoff) != 0)
    {
        if (CompareFileAge(lastWriteTime, MaxAgeFileTimeCutoff, COptions::FilteringMaxAgeComparison))
            return true;
    }

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

    return false;
}

bool CFiltering::IsFilteredOut(const CItem* item)
{
    if (!FilterActive) return false;
    if (item->IsTypeOrFlag(IT_FILE))
        return IsFilteredOut(item->GetName(), IncludeDirsRegex.empty() ? std::wstring() : item->GetPath(),
            item->GetSizeLogical(), item->GetLastChange());
    return IsFilteredOut(item->GetPath(), item->IsTypeOrFlag(ITF_REMOTE) && item->IsScanRoot());
}
