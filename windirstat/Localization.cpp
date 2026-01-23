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

std::unordered_map<std::wstring, std::wstring> Localization::m_map;

void Localization::SearchReplace(std::wstring& input, const std::wstring_view& search, const std::wstring_view& replace)
{
    for (size_t pos = 0; (pos = input.find(search, pos)) != std::wstring::npos; pos += replace.size())
    {
        input.replace(pos, search.size(), replace);
    }
}

bool Localization::CrackStrings(const std::wstring& sFileData, const std::wstring& sPrefix)
{
    // Read the file line by line
    std::wstring line;
    std::wistringstream stream(sFileData);
    while (std::getline(stream, line))
    {
        // Filter out unwanted languages
        if (!sPrefix.empty() && !line.starts_with(sPrefix + L":")) continue;

        // Parse the string after the first equals
        SearchReplace(line, L"\r", L"");
        SearchReplace(line, L"\\n", L"\n");
        SearchReplace(line, L"\\t", L"\t");
        if (const auto e = line.find_first_of(L'='); e != std::string::npos)
        {
            // Strip the prefix if any and add to map
            size_t startPos = 0;
            if (!sPrefix.empty()) startPos = line.find_first_of(L':') + 1;
            m_map[line.substr(startPos, e - startPos)] = line.substr(e + 1);
        }
    }

    return !m_map.empty();
}

std::set<LANGID> Localization::GetLanguageList()
{
    // Decompress the resource
    const auto resourceData = GetTextResource(IDR_LANGS);
    if (resourceData.empty()) return {};

    // Read the combined language line by line
    std::wstring line;
    std::wistringstream is(resourceData);
    std::set<std::wstring> uniqueLangs;
    while (std::getline(is, line))
    {
        // Convert to wide strings
        SearchReplace(line, L"\r", L"");
        auto linePos = line.find_first_of(L':');
        if (linePos == std::wstring::npos) continue;
        uniqueLangs.emplace(line.substr(0, linePos));
    }

    // Also check for external language files
    FinderBasic finder;
    for (BOOL b = finder.FindFile(GetAppFolder(), L"lang_*.txt"); b; b = finder.FindNext())
    {
        auto langString = finder.GetFileName().substr(5);
        langString = langString.substr(0, langString.find_first_of(L'.'));
        uniqueLangs.emplace(langString);
    }

    // Convert to langids
    std::set<LANGID> results;
    for (const auto& lang : uniqueLangs)
    {
        const LCID lcid = LocaleNameToLCID(lang.c_str(), LOCALE_ALLOW_NEUTRAL_NAMES);
        if (lcid == LOCALE_NEUTRAL || lcid == LOCALE_CUSTOM_UNSPECIFIED) continue;

        const LANGID langid = LANGIDFROMLCID(lcid);
        results.emplace(langid);
    }

    return results;
}

bool Localization::LoadResource(const WORD language)
{
    const LCID lcid = MAKELCID(language, SORT_DEFAULT);
    std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> name{};
    if (LCIDToLocaleName(lcid, name.data(), LOCALE_NAME_MAX_LENGTH, 0) == 0) return {};
    
    // Try to load external language file first
    if (LoadExternalLanguage(LOCALE_SISO639LANGNAME, language) ||
        LoadExternalLanguage(LOCALE_SNAME, language)) return true;

    // Try to load built-in resource
    const std::wstring sResourceData = GetTextResource(IDR_LANGS);
    return CrackStrings(sResourceData, GetLocaleString(LOCALE_SISO639LANGNAME, language)) ||
        CrackStrings(sResourceData, GetLocaleString(LOCALE_SNAME, language));
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
            mi.fMask = MIIM_STRING;
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
    std::ifstream f(file, std::ios::binary);
    if (!f) return {};

    const std::vector b((std::istreambuf_iterator(f)), {});
    if (b.empty()) return {};

    // Process the data
    return CrackStrings(CComBSTR(static_cast<int>(b.size()), b.data()).m_str);
}
