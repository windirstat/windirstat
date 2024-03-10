// Localization.cpp - Implementation of CDirStatApp and some globals
//
// WinDirStat - Directory Statistics
// Copyright (C) 2024 WinDirStat Team (windirstat.net)
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
//

#include "stdafx.h"
#include "Localization.h"
#include "CommonHelpers.h"
#include "resource.h"
#include "langs.h"

#include <filesystem>
#include <fstream>
#include <map>

std::unordered_map<std::wstring, std::wstring> Localization::map;

void Localization::SearchReplace(std::wstring& input, const std::wstring& search, const std::wstring& replace)
{
    for (size_t i = 0; (i = input.find(search)) != std::wstring::npos;)
    {
        input.replace(i, search.size(), replace);
    }
}

bool Localization::CrackStrings(std::basic_istream<char>& stream, unsigned int stream_size)
{
    // Size a buffer to the largest string it will contain
    std::wstring buffer_wide;
    buffer_wide.resize((stream_size + 1) * sizeof(WCHAR));

    // Read the file line by line
    std::string line;
    while (std::getline(stream, line))
    {
        // Convert to wide strings
        if (line.empty() || line[0] == L'#') continue;
        int sz = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()),
            buffer_wide.data(), static_cast<int>(buffer_wide.size()));
        ASSERT(sz != 0);
        std::wstring line_wide = buffer_wide.substr(0, sz);

        // Parse the string after the first equals
        SearchReplace(line_wide, L"\r", L"");
        SearchReplace(line_wide, L"\\n", L"\n");
        SearchReplace(line_wide, L"\\t", L"\t");
        if (const auto e = line_wide.find_first_of('='); e != std::string::npos)
        {
            map[line_wide.substr(0, e)] = line_wide.substr(e + 1);
        }
    }

    return true;
}

std::vector<LANGID> Localization::GetLanguageList()
{
    std::vector<LANGID> results;
    if (::PathFileExists(GetAppFileName(L"lang.txt")))
    {
        results.push_back(MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));
    }

    EnumResourceLanguagesExW(nullptr, LANG_RESOURCE_TYPE, MAKEINTRESOURCE(IDR_RT_LANG), [](HMODULE, LPCWSTR, LPCWSTR, WORD wIDLanguage, LONG_PTR lParam)->BOOL
    {
        reinterpret_cast<std::vector<LANGID>*>(lParam)->push_back(wIDLanguage);
        return TRUE;

    }, reinterpret_cast<LONG_PTR>(&results), 0, 0);

    return results;
}

bool Localization::LoadResource(WORD language)
{
    // Load local file for customization
    if (language == MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL))
    {
        return LoadFile(GetAppFileName(L"lang.txt").GetString());
    }

    // Find the resource in the loaded module
    const HRSRC resource = ::FindResourceEx(nullptr, LANG_RESOURCE_TYPE, MAKEINTRESOURCE(IDR_RT_LANG), language);
    if (resource == nullptr) return false;

    // Establish the resource
    const HGLOBAL resource_data = ::LoadResource(nullptr, resource);
    if (resource_data == nullptr) return false;

    // Fetch a pointer to the data
    const LPVOID binary_data = ::LockResource(resource_data);
    if (binary_data == nullptr) return false;

    // Organize the data into a string
    const std::string file(static_cast<LPCSTR>(binary_data), SizeofResource(nullptr, resource));
    std::istringstream is(file);

    // Process the data
    return CrackStrings(is, SizeofResource(nullptr, resource));
}

void Localization::UpdateMenu(CMenu& menu)
{
    for (int i = 0; i < menu.GetMenuItemCount(); i++)
    {
        WCHAR buffer[MAX_VALUE_SIZE];
        MENUITEMINFOW mi{ sizeof(MENUITEMINFO) };
        mi.cch = _countof(buffer);
        mi.dwTypeData = &buffer[0];
        mi.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_SUBMENU;
        menu.GetMenuItemInfoW(i, &mi, TRUE);
        if (mi.fType == MFT_STRING && wcsstr(mi.dwTypeData, L"ID") == mi.dwTypeData &&
            Contains(CStringW(mi.dwTypeData)))
        {
            mi.fMask = MIIM_STRING;
            mi.dwTypeData = const_cast<LPWSTR>(map[mi.dwTypeData].c_str());
            menu.SetMenuItemInfoW(i, &mi, TRUE);
        }
        if (IsMenu(mi.hSubMenu)) UpdateMenu(*menu.GetSubMenu(i));
    }
}

void Localization::UpdateTabControl(CTabCtrl& tab)
{
    for (int i = 0; i < tab.GetItemCount(); i++)
    {
        WCHAR buffer[MAX_VALUE_SIZE];
        TCITEMW ti{ sizeof(TCITEMW) };
        ti.cchTextMax = _countof(buffer);
        ti.mask = TCIF_TEXT;
        ti.pszText = &buffer[0];
        if (tab.GetItem(i, &ti) && wcsstr(ti.pszText, L"ID") == ti.pszText &&
            Contains(CStringW(ti.pszText)))
        {
            ti.mask = TCIF_TEXT;
            ti.pszText = const_cast<LPWSTR>(map[ti.pszText].c_str());
            tab.SetItem(i, &ti);
        }
    }
}

void Localization::UpdateWindowText(HWND hwnd)
{
    WCHAR buffer[MAX_VALUE_SIZE];
    if (GetWindowText(hwnd, buffer, _countof(buffer)) > 0 &&
        wcsstr(&buffer[0], L"ID") == &buffer[0] &&
        Contains(CStringW(&buffer[0])))
    {
        ::SetWindowText(hwnd, map[buffer].c_str());
    }
}


void Localization::UpdateDialogs(CWnd& wnd)
{
    UpdateWindowText(wnd.m_hWnd);

    EnumChildWindows(wnd.m_hWnd, [](HWND hwnd, LPARAM)
        {
            UpdateWindowText(hwnd);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(nullptr));
}

bool Localization::LoadFile(const std::wstring& file)
{
    // Open the file
    std::ifstream fFile(file);
    if (!fFile.good()) return false;

    // Process the data
    return CrackStrings(fFile, static_cast<unsigned int>(std::filesystem::file_size(file)));
}
