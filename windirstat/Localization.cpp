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

#include "stdafx.h"
#include "Localization.h"
#include "GlobalHelpers.h"
#include "FinderBasic.h"
#include "langs.h"

#include <filesystem>
#include <fstream>
#include <array>
#include <unordered_map>

std::unordered_map<std::wstring, std::wstring> Localization::m_Map;

void Localization::SearchReplace(std::wstring& input, const std::wstring_view& search, const std::wstring_view& replace)
{
    for (size_t i = input.find(search); i != std::wstring::npos; i = input.find(search))
    {
        input.replace(i, search.size(), replace);
    }
}

bool Localization::CrackStrings(std::basic_istream<char>& stream, const unsigned int streamSize)
{
    // Size a buffer to the largest string it will contain
    std::wstring bufferWide;
    bufferWide.resize((streamSize + 1) * sizeof(WCHAR));

    // Read the file line by line
    std::string line;
    while (std::getline(stream, line))
    {
        // Convert to wide strings
        if (line.empty() || line[0] == L'#') continue;
        const int sz = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), static_cast<int>(line.size()),
            bufferWide.data(), static_cast<int>(bufferWide.size()));
        ASSERT(sz != 0);
        std::wstring lineWide = bufferWide.substr(0, sz);

        // Parse the string after the first equals
        SearchReplace(lineWide, L"\r", L"");
        SearchReplace(lineWide, L"\\n", L"\n");
        SearchReplace(lineWide, L"\\t", L"\t");
        if (const auto e = lineWide.find_first_of('='); e != std::string::npos)
        {
            m_Map[lineWide.substr(0, e)] = lineWide.substr(e + 1);
        }
    }

    return true;
}

std::vector<LANGID> Localization::GetLanguageList()
{
    std::vector<LANGID> results;
    EnumResourceLanguagesExW(nullptr, LANG_RESOURCE_TYPE, MAKEINTRESOURCE(IDR_RT_LANG), [](HMODULE, LPCWSTR, LPCWSTR, const WORD wIDLanguage, const LONG_PTR lParam)->BOOL
    {
        std::bit_cast<std::vector<LANGID>*>(lParam)->push_back(wIDLanguage);
        return TRUE;

    }, reinterpret_cast<LONG_PTR>(&results), 0, 0);

    FinderBasic finder;
    for (BOOL b = finder.FindFile(GetAppFolder(), L"lang_??.txt"); b; b = finder.FindNext())
    {
        const std::wstring lang = finder.GetFileName().substr(5, 2);
        const LCID lcid = LocaleNameToLCID(lang.c_str(), LOCALE_ALLOW_NEUTRAL_NAMES);
        if (lcid == LOCALE_NEUTRAL || lcid == LOCALE_CUSTOM_UNSPECIFIED) continue;

        const LANGID langid = LANGIDFROMLCID(lcid);
        const LANGID langidn = MAKELANGID(PRIMARYLANGID(langid), SUBLANG_NEUTRAL);
        if (std::ranges::find(results, langidn) == results.end()) results.push_back(langidn);
    }

    return results;
}

bool Localization::LoadResource(const WORD language)
{
    if (LoadExternalLanguage(LOCALE_SISO639LANGNAME, language)) return true; // ISO 639-1 language code
    if (LoadExternalLanguage(LOCALE_SNAME, language)) return true; // BCP 47 language code

    // Find the resource in the loaded module
    const HRSRC resource = ::FindResourceEx(nullptr, LANG_RESOURCE_TYPE, MAKEINTRESOURCE(IDR_RT_LANG), language);
    if (resource == nullptr) return false;

    // Decompress the resource
    auto resourceData = GetCompressedResource(resource);
    if (resourceData.empty()) return false;

    // Organize the data into a string
    const std::string file(reinterpret_cast<PCHAR>(resourceData.data()), resourceData.size());
    std::istringstream is(file);

    // Process the data
    return CrackStrings(is, SizeofResource(nullptr, resource));
}

void Localization::UpdateMenu(CMenu& menu)
{
    for (int i = 0; i < menu.GetMenuItemCount(); i++)
    {
        std::array<WCHAR, MAX_VALUE_SIZE> buffer;
        MENUITEMINFOW mi{ sizeof(MENUITEMINFO) };
        mi.cch = static_cast<UINT>(buffer.size());
        mi.dwTypeData = buffer.data();
        mi.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_SUBMENU;
        menu.GetMenuItemInfoW(i, &mi, TRUE);
        if (mi.fType == MFT_STRING && wcsstr(mi.dwTypeData, L"ID") == mi.dwTypeData &&
            Contains(mi.dwTypeData))
        {
            mi.fMask = MIIM_STRING;
            mi.dwTypeData = const_cast<LPWSTR>(m_Map[mi.dwTypeData].c_str());
            menu.SetMenuItemInfoW(i, &mi, TRUE);
        }
        if (mi.hSubMenu != nullptr && IsMenu(mi.hSubMenu))
            UpdateMenu(*menu.GetSubMenu(i));
    }
}

void Localization::UpdateTabControl(CMFCTabCtrl& tab)
{
    for (int i = 0; i < tab.GetTabsNum(); i++)
    {
        CString tabLabel;
        tab.GetTabLabel(i, tabLabel);
        std::wstring tabLabelStr = tabLabel.GetString();
        if (tabLabelStr.starts_with(L"ID") && Contains(tabLabelStr))
        {
            tab.SetTabLabel(i, m_Map[tabLabelStr].c_str());
        }
    }
}

void Localization::UpdateWindowText(const HWND hwnd)
{
    std::array<WCHAR, MAX_VALUE_SIZE> buffer;
    if (GetWindowText(hwnd, buffer.data(), static_cast<int>(buffer.size())) > 0 &&
        wcsstr(buffer.data(), L"ID") == buffer.data() &&
        Contains(buffer.data()))
    {
        ::SetWindowText(hwnd, m_Map[buffer.data()].c_str());
    }
}

void Localization::UpdateDialogs(const CWnd& wnd)
{
    UpdateWindowText(wnd.m_hWnd);

    EnumChildWindows(wnd.m_hWnd, [](HWND hwnd, LPARAM)
    {
        UpdateWindowText(hwnd);
        return TRUE;
    },
    reinterpret_cast<LPARAM>(nullptr));
}

// Try to find and load external language file, return false if failed.
bool Localization::LoadExternalLanguage(const LCTYPE lcttype, const LCID lcid)
{
    const std::wstring name = L"lang_" + GetLocaleString(lcttype, lcid) + L".txt";
    const std::wstring langFolder = GetAppFolder() + L"\\";

    return FinderBasic::DoesFileExist(langFolder, name) && LoadFile(langFolder + name);
}

bool Localization::LoadFile(const std::wstring& file)
{
    // Open the file
    std::ifstream fileStream(file);
    if (!fileStream.good()) return false;

    // Process the data
    return CrackStrings(fileStream, static_cast<unsigned int>(std::filesystem::file_size(file)));
}
