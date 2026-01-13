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
#include "FinderBasic.h"
#include "langs.h"

std::unordered_map<std::wstring, std::wstring> Localization::m_map;

void Localization::SearchReplace(std::wstring& input, const std::wstring_view& search, const std::wstring_view& replace)
{
    for (size_t pos = 0; (pos = input.find(search, pos)) != std::wstring::npos; pos += replace.size())
    {
        input.replace(pos, search.size(), replace);
    }
}

bool Localization::CrackStrings(std::basic_istream<char>& stream, const unsigned int streamSize)
{
    // Size a buffer to the largest string it will contain
    std::wstring bufferWide(streamSize + 1, L'\0');

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
        if (const auto e = lineWide.find_first_of(L'='); e != std::string::npos)
        {
            m_map[lineWide.substr(0, e)] = lineWide.substr(e + 1);
        }
    }

    return true;
}

std::vector<LANGID> Localization::GetLanguageList()
{
    std::vector<LANGID> results;
    EnumResourceLanguagesEx(nullptr, LANG_RESOURCE_TYPE, MAKEINTRESOURCE(IDR_RT_LANG), [](HMODULE, LPCWSTR, LPCWSTR, const WORD wIDLanguage, const LONG_PTR lParam)->BOOL
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
    for (const int i : std::views::iota(0, menu.GetMenuItemCount()))
    {
        CString text;
        if (menu.GetMenuString(i, text, MF_BYPOSITION) == 0) continue;

        if (text.Find(L"ID") == 0 && Contains(text.GetString()))
        {
            MENUITEMINFOW mi{ .cbSize = sizeof(MENUITEMINFOW) };
            mi.fMask = MIIM_STRING,
            mi.dwTypeData = const_cast<LPWSTR>(m_map[text.GetString()].c_str());
            menu.SetMenuItemInfo(i, &mi, TRUE);
        }

        if (CMenu* sub = menu.GetSubMenu(i); sub != nullptr) UpdateMenu(*sub);
    }
}

void Localization::UpdateTabControl(CMFCTabCtrl& tab)
{
    for (const int i : std::views::iota(0, tab.GetTabsNum()))
    {
        CString tabLabel;
        tab.GetTabLabel(i, tabLabel);
        std::wstring tabLabelStr = tabLabel.GetString();
        if (tabLabelStr.starts_with(L"ID") && Contains(tabLabelStr))
        {
            tab.SetTabLabel(i, m_map[tabLabelStr].c_str());
        }
    }
}

void Localization::UpdateWindowText(CWnd& wnd)
{
    // Lookup and cache system font
    static CFont* systemFont = [] {
        NONCLIENTMETRICS ncm{ .cbSize = sizeof(NONCLIENTMETRICS) };
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        auto* font = new CFont();
        font->CreateFontIndirect(&ncm.lfMessageFont);
        return font;
        }();

    wnd.SetFont(systemFont);

    // Update window text if it's a localizable ID
    CString text;
    wnd.GetWindowText(text);
    if (text.Find(L"ID") == 0 && Contains(text.GetString()))
        wnd.SetWindowText(m_map[text.GetString()].c_str());
}

void Localization::UpdateDialogs(CWnd& wnd)
{
    UpdateWindowText(wnd);

    for (CWnd* child = wnd.GetWindow(GW_CHILD); child != nullptr;
        child = child->GetWindow(GW_HWNDNEXT))
    {
        UpdateWindowText(*child);
    }
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
