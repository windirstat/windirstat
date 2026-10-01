// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"

struct string_hash {
    using is_transparent = void;
    size_t operator()(const std::wstring_view txt) const { return std::hash<std::wstring_view>{}(txt); }
};

class Localization final
{
    static bool m_rightToLeft;
    static bool CrackStrings(const std::wstring& sFileData, const std::wstring& sPrefix = {});
    static void SearchReplace(std::wstring& input, const std::wstring_view& search, const std::wstring_view& replace);
    static void UpdateWindowText(WindowRef wnd);

public:
    static std::unordered_map<std::wstring, std::wstring, string_hash, std::equal_to<>> m_map;

    static bool Contains(const std::wstring_view name)
    {
        assert(m_map.contains(name));
        return m_map.contains(name);
    }

    static std::wstring Lookup(const std::wstring_view name)
    {
        const auto it = m_map.find(name);
        return it != m_map.end() ? it->second : std::wstring();
    }

    static std::wstring LookupNeutral(const UINT res)
    {
        return LoadResourceString(res, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
    }

    template <typename... Args>
    static std::wstring Format(const std::wstring_view format, const Args&... args)
    {
        const auto & formatString = Lookup(format);
        try
        {
            return std::vformat(formatString, std::make_wformat_args(args...));
        }
        catch (const std::format_error&)
        {
            // Translation files are user-editable so a malformed
            // substitution must not take down the application
            return formatString;
        }
    }

    static bool IsRightToLeft() noexcept { return m_rightToLeft; }
    static void UpdateMenu(MenuRef menu);
    static void UpdateTabControl(CTabControl& tab);
    static void UpdateDialogs(WindowRef wnd);
    static bool LoadExternalLanguage(LCTYPE lcttype, LCID lcid);
    static bool LoadFile(const std::wstring& file);
    static bool LoadResource(LANGID language);
    static std::wstring ConvertToWideString(const std::string_view& sv);
    static std::set<LANGID> GetLanguageList();
};
