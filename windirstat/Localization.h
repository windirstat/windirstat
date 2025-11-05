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
#include "res\LangStrings.h"
#include "Options.h"

#include <string>
#include <unordered_map>
#include <format>

class Localization final
{
    static bool CrackStrings(std::basic_istream<char>& stream, unsigned int streamSize);
    static void SearchReplace(std::wstring& input, const std::wstring_view& search, const std::wstring_view& replace);
    static void UpdateWindowText(HWND hwnd);

public:
    static constexpr auto MAX_VALUE_SIZE = 1024;
    static constexpr auto LANG_RESOURCE_TYPE = L"RT_LANG";
    static std::unordered_map<std::wstring, std::wstring> m_Map;

    static bool Contains(const std::wstring_view& name)
    {
        ASSERT(m_Map.contains(std::wstring(name)));
        return m_Map.contains(std::wstring(name));
    }

    static std::wstring Lookup(const std::wstring_view& name, const std::wstring & def = std::wstring())
    {
        return Contains(name) ? m_Map[std::wstring(name)] : def;
    }

    static std::wstring LookupNeutral(const UINT res)
    {
        CStringW name;
        (void) name.LoadStringW(nullptr, res, MAKELANGID(LANG_NEUTRAL,SUBLANG_NEUTRAL));
        return name.GetString();
    }

    template <typename... Args>
    static std::wstring Format(std::wstring_view format, const Args&... args)
    {
        const auto & formatString = Lookup(format);
        return std::vformat(formatString, std::make_wformat_args(args...));
    }

    static void UpdateMenu(CMenu& menu);
    static void UpdateTabControl(CMFCTabCtrl& tab);
    static void UpdateDialogs(const CWnd& wnd);
    static bool LoadExternalLanguage(LCTYPE lcttype, LCID lcid);
    static bool LoadFile(const std::wstring& file);
    static bool LoadResource(WORD language);
    static std::vector<LANGID> GetLanguageList();
};
