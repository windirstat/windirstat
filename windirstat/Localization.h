// Localization.h - Implementation of CDirStatApp and some globals
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

#include "stdafx.h"
#include "Options.h"

#include <string>
#include <unordered_map>
#include <format>

class Localization
{
    static bool CrackStrings(std::basic_istream<char>& stream, unsigned int streamSize);
    static void SearchReplace(std::wstring& input, const std::wstring_view& search, const std::wstring_view& replace);
    static void UpdateWindowText(HWND hwnd);

public:
    static constexpr auto MAX_VALUE_SIZE = 1024;
    static constexpr auto LANG_RESOURCE_TYPE = L"RT_LANG";
    static std::unordered_map<std::wstring, std::wstring> m_Map;
    static std::unordered_map<USHORT, std::wstring> m_MapInt;

    static bool Contains(const std::wstring& name)
    {
        ASSERT(m_Map.contains(name));
        return m_Map.contains(name);
    }

    static std::wstring & Lookup(const USHORT res, const std::wstring& def = std::wstring())
    {
        // return from cache if already looked up
        if (m_MapInt.contains(res)) return m_MapInt[res];

        CStringW name;
        (void) name.LoadStringW(nullptr, res, static_cast<LANGID>(COptions::LanguageId.Obj()));
        m_MapInt.emplace(res, Lookup(name.GetString(), def));
        return m_MapInt.at(res);
    }

    static std::wstring Lookup(const std::wstring& name, const std::wstring & def = std::wstring())
    {
        return Contains(name) ? m_Map[name] : def;
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
        return std::vformat(format, std::make_wformat_args(args...));
    }

    template <typename... Args>
    static std::wstring Format(USHORT res, const Args&... args)
    {
        return std::vformat(Lookup(res), std::make_wformat_args(args...));
    }

    static void UpdateMenu(CMenu& menu);
    static void UpdateTabControl(CTabCtrl& tab);
    static void UpdateDialogs(const CWnd& wnd);
    static bool LoadFile(const std::wstring& file);
    static bool LoadResource(WORD language);
    static std::vector<LANGID> GetLanguageList();
};
