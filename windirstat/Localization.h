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

class Localization final
{
    static bool CrackStrings(const std::wstring& sFileData, const std::wstring& sPrefix = {});
    static void SearchReplace(std::wstring& input, const std::wstring_view& search, const std::wstring_view& replace);
    static void UpdateWindowText(CWnd& wnd);

public:
    static std::unordered_map<std::wstring, std::wstring> m_map;

    static bool Contains(const std::wstring_view& name)
    {
        ASSERT(m_map.contains(std::wstring(name)));
        return m_map.contains(std::wstring(name));
    }

    static std::wstring Lookup(const std::wstring_view& name)
    {
        const auto it = m_map.find(std::wstring(name));
        return it != m_map.end() ? it->second : std::wstring();
    }

    static std::wstring LookupNeutral(const UINT res)
    {
        CStringW name;
        (void) name.LoadString(AfxGetResourceHandle(), res, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
        return name.GetString();
    }

    template <typename... Args>
    static std::wstring Format(const std::wstring_view format, const Args&... args)
    {
        const auto & formatString = Lookup(format);
        return std::vformat(formatString, std::make_wformat_args(args...));
    }

    static void UpdateMenu(CMenu& menu);
    static void UpdateTabControl(CMFCTabCtrl& tab);
    static void UpdateDialogs(CWnd& wnd);
    static bool LoadExternalLanguage(LCTYPE lcttype, LCID lcid);
    static bool LoadFile(const std::wstring& file);
    static bool LoadResource(WORD language);
    static std::wstring ConvertToWideString(const std::string_view& sv);
    static std::set<LANGID> GetLanguageList();
};
