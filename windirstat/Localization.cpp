// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "FinderBasic.h"

std::unordered_map<std::wstring, std::wstring, string_hash, std::equal_to<>> Localization::m_map;
bool Localization::m_rightToLeft = false;

std::wstring Localization::FormatImpl(const std::wstring_view format, const std::wformat_args args)
{
    const auto& formatString = Lookup(format);
    try
    {
        return std::vformat(formatString, args);
    }
    catch (const std::format_error&)
    {
        // Translation files are user-editable so a malformed
        // substitution must not take down the application
        return formatString;
    }
}

void Localization::SearchReplace(std::wstring& input, const std::wstring_view& search, const std::wstring_view& replace)
{
    for (size_t pos = 0; (pos = input.find(search, pos)) != std::wstring::npos; pos += replace.size())
    {
        input.replace(pos, search.size(), replace);
    }
}

const std::wstring& Localization::GetResourceData()
{
    // Decompress the combined language resource once; the leading newline
    // lets every "[language]" section header be found by searching for "\n["
    static const std::wstring data = L"\n" + GetTextResource(IDR_LANGS);
    return data;
}

bool Localization::CrackStrings(const std::wstring_view data)
{
    // Read the data line by line
    bool loaded = false;
    for (const auto range : std::views::split(data, L'\n'))
    {
        // Parse the string after the first equals
        std::wstring line(std::from_range, range);
        SearchReplace(line, L"\r", wds::strEmpty);
        SearchReplace(line, L"\\n", L"\n");
        SearchReplace(line, L"\\t", L"\t");
        if (const auto e = line.find_first_of(wds::chrEqual); e != wds::szNpos)
        {
            m_map[line.substr(0, e)] = line.substr(e + 1);
            loaded = true;
        }
    }
    return loaded;
}

std::set<LANGID> Localization::GetLanguageList()
{
    // Collect the section names from the combined language resource
    std::set<std::wstring> uniqueLangs;
    const std::wstring& data = GetResourceData();
    for (size_t pos = data.find(L"\n["); pos != std::wstring::npos; pos = data.find(L"\n[", pos + 1))
    {
        uniqueLangs.emplace(data.substr(pos + 2, data.find(L']', pos) - pos - 2));
    }

    // Also check for external language files
    FinderBasic finder;
    for (bool b = finder.FindFile(GetAppFolder(), L"lang_*.txt"); b; b = finder.FindNext())
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

bool Localization::LoadResource(const LANGID language)
{
    const LCID lcid = MAKELCID(language, SORT_DEFAULT);

    // Parse a "[language]" section of the combined resource up to the next section
    const auto crackSection = [](const std::wstring& name)
    {
        const std::wstring_view data = GetResourceData();
        const std::wstring header = L"\n[" + name + L"]";
        const size_t start = data.find(header);
        return start != std::wstring_view::npos &&
            CrackStrings(data.substr(0, data.find(L"\n[", start + 1)).substr(start + header.size()));
    };

    // Load English strings first as a baseline fallback
    crackSection(L"en");
    m_rightToLeft = false;

    if (GetLocaleInfo(lcid, LOCALE_SLANGUAGE, nullptr, 0) == 0) return true;

    // Short-circuit the language resource loading sequence at the first successful load
    const bool loaded =
        LoadExternalLanguage(LOCALE_SNAME, language) ||                                 // External BCP 47 language file
        LoadExternalLanguage(LOCALE_SISO639LANGNAME, language) ||                       // External ISO 639-1 language file
        crackSection(GetLocaleString(LOCALE_SNAME, language)) ||                        // Built-in BCP 47 resource
        crackSection(GetLocaleString(LOCALE_SISO639LANGNAME, language));                // Built-in ISO 639-1 resource
    m_rightToLeft = loaded && GetLocaleString(LOCALE_IREADINGLAYOUT, lcid) == L"1";
    return loaded;
}

void Localization::UpdateMenu(MenuRef menu)
{
    for (const int i : std::views::iota(0, menu.GetItemCount()))
    {
        const std::wstring text = menu.GetItemText(i);
        if (text.empty()) continue;

        if (text.starts_with(L"ID") && Contains(text))
        {
            MENUITEMINFOW mi{ .cbSize = sizeof(MENUITEMINFOW) };
            mi.fMask = MIIM_ID;
            menu.GetItemInfo(i, &mi);

            // Build the menu text with localized string and accelerator
            std::wstring menuText = m_map[text];
            if (mi.wID != std::bit_cast<UINT>(-1))
            {
                const std::wstring accel = GetAcceleratorString(mi.wID);
                if (!accel.empty()) menuText += L"\t" + accel;
            }

            // Set the item text
            mi.fMask = MIIM_STRING;
            mi.dwTypeData = const_cast<LPWSTR>(menuText.c_str());
            menu.SetItemInfo(i, &mi);
        }

        if (const auto sub = menu.GetSubMenu(i); sub != nullptr) UpdateMenu(sub);
    }
}

void Localization::UpdateTabControl(CTabControl& tab)
{
    for (const int i : std::views::iota(0, tab.GetTabCount()))
    {
        const std::wstring_view label = tab.GetTabLabel(i);
        if (label.starts_with(L"ID") && Contains(label))
            tab.SetTabLabel(i, L" " + Lookup(label) + L" ");
    }
}

void Localization::UpdateWindowText(WindowRef wnd)
{
    wnd.SetFont(GetAppFont(wnd.Handle()));

    // Update window text if it's a localizable ID
    const std::wstring text = wnd.GetText();
    const bool localizable = text.starts_with(L"ID") && Contains(text);
    if (localizable) wnd.SetText(m_map[text]);
    std::array<WCHAR, MAX_CLASS_NAME> className{};
    GetClassNameW(wnd.Handle(), className.data(), static_cast<int>(className.size()));
    if (_wcsicmp(className.data(), WC_BUTTON) == 0 ||
        (localizable && _wcsicmp(className.data(), WC_STATIC) == 0))
        wnd.ModifyStyleEx(WS_EX_RTLREADING, m_rightToLeft ? WS_EX_RTLREADING : 0);
}

void Localization::UpdateDialogs(WindowRef wnd)
{
    UpdateWindowText(wnd);

    for (auto child = wnd.GetWindow(GW_CHILD); child != nullptr;
        child = child.GetWindow(GW_HWNDNEXT))
    {
        UpdateWindowText(child);
    }
}

// Try to find and load external language file, return false if failed.
bool Localization::LoadExternalLanguage(const LCTYPE lcttype, const LCID lcid)
{
    const std::wstring filename = L"lang_" + GetLocaleString(lcttype, lcid) + L".txt";
    const std::wstring langFolder = GetAppFolder() + L"\\";

    return FinderBasic::DoesFileExist(langFolder, filename) && LoadFile(langFolder + filename);
}

bool Localization::LoadFile(const std::wstring& file)
{
    // Open the file
    std::ifstream f(file, std::ios::binary);
    if (!f) return {};

    const std::vector b((std::istreambuf_iterator(f)), {});
    if (b.empty()) return {};

    // Process the data
    return CrackStrings(ConvertToWideString({ b.data(), b.size() }));
}

std::wstring Localization::ConvertToWideString(const std::string_view & sv)
{
    if (sv.empty()) return {};

    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, sv.data(),
        static_cast<int>(sv.size()), nullptr, 0);
    if (required <= 0) return {};

    std::wstring out(required, L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, sv.data(),
        static_cast<int>(sv.size()), out.data(), required) <= 0) return {};

    return out;
}
