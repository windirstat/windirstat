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

#include "stdafx.h"
#include "WinDirStat.h"
#include "Property.h"

#include <regex>
#include <sstream>

std::vector<PersistedSetting*>& PersistedSetting::GetPropertySet()
{
    static std::vector<PersistedSetting*> _properties;
    return _properties;
}

bool PersistedSetting::ReadBinaryProperty(const std::wstring& section, const std::wstring& entry, LPVOID dest, const size_t size)
{
    LPBYTE data = nullptr;
    UINT dataSize = 0;
    CDirStatApp::Get()->GetProfileBinary(section.c_str(), entry.c_str(), &data, &dataSize);
    const bool success = (dataSize == size);
    if (success) std::memcpy(dest, data, dataSize);
    delete[] data;
    return success;
}

// Setting<int> Processing

template <> void Setting<int>::ReadPersistedProperty()
{
    const int def = m_Value;
    m_Value = CDirStatApp::Get()->GetProfileInt(m_Section.c_str(), m_Entry.c_str(), m_Value);
    if (m_Value != def && m_Min != m_Max) m_Value = max(min(m_Value, m_Max), m_Min);
}

template <> void Setting<int>::WritePersistedProperty()
{
    CDirStatApp::Get()->WriteProfileInt(m_Section.c_str(), m_Entry.c_str(), m_Value);
}

// Setting<bool> Processing

template <> void Setting<bool>::ReadPersistedProperty()
{
    m_Value = CDirStatApp::Get()->GetProfileInt(m_Section.c_str(), m_Entry.c_str(), m_Value == 0 ? 0 : 1) != 0;
}

template <> void Setting<bool>::WritePersistedProperty()
{
    CDirStatApp::Get()->WriteProfileInt(m_Section.c_str(), m_Entry.c_str(), m_Value == 0 ? 0 : 1);
}

// Setting<std::wstring> Processing

template <> void Setting<std::wstring>::ReadPersistedProperty()
{
    m_Value = CDirStatApp::Get()->GetProfileString(m_Section.c_str(), m_Entry.c_str(), m_Value.c_str());
    m_Value = std::regex_replace(m_Value, std::wregex(LR"(\x1e)"), L"\r\n");
}

template <> void Setting<std::wstring>::WritePersistedProperty()
{
    const std::wstring valueCleaned = std::regex_replace(m_Value, std::wregex(LR"((\r|\n)+)"), L"\x1e");
    CDirStatApp::Get()->WriteProfileString(m_Section.c_str(), m_Entry.c_str(), valueCleaned.c_str());
}

// Setting<WINDOWPLACEMENT> Processing

template <> void Setting<WINDOWPLACEMENT>::ReadPersistedProperty()
{
    ReadBinaryProperty(m_Section, m_Entry, &m_Value, sizeof(WINDOWPLACEMENT));
}

template <> void Setting<WINDOWPLACEMENT>::WritePersistedProperty()
{
    CDirStatApp::Get()->WriteProfileBinary(m_Section.c_str(), m_Entry.c_str(),
        reinterpret_cast<LPBYTE>(&m_Value), sizeof(WINDOWPLACEMENT));
}

// Setting<std::vector<std::wstring>> Processing

template <> void Setting<std::vector<std::wstring>>::ReadPersistedProperty()
{
    const std::wstring s = CDirStatApp::Get()->GetProfileString(m_Section.c_str(), m_Entry.c_str()).GetString();
    std::wstringstream iss(s);

    m_Value.clear();
    for (std::wstring part; std::getline(iss, part, L'|');)
    {
        m_Value.push_back(part);
    }
}

template <> void Setting<std::vector<std::wstring>>::WritePersistedProperty()
{
    std::wstring result;
    for (const auto & part : m_Value)
    {
        result += part + L'|';
    }
    if (result.ends_with(L'|')) result.pop_back();

    CDirStatApp::Get()->WriteProfileString(m_Section.c_str(), m_Entry.c_str(), result.c_str());
}

// Setting<std::vector<int>> Processing

template <> void Setting<std::vector<int>>::ReadPersistedProperty()
{
    const std::wstring s = CDirStatApp::Get()->GetProfileString(m_Section.c_str(), m_Entry.c_str()).GetString();
    std::wstringstream iss(s);

    m_Value.clear();
    for (std::wstring part; std::getline(iss, part, L',');)
    {
        m_Value.push_back(std::stoi(part));
    }
}

template <> void Setting<std::vector<int>>::WritePersistedProperty()
{
    std::wstring result;
    for (const auto part : m_Value)
    {
        result += std::to_wstring(part) + L',';
    }
    if (result.ends_with(L',')) result.pop_back();

    CDirStatApp::Get()->WriteProfileString(m_Section.c_str(), m_Entry.c_str(), result.c_str());
}

// Setting<COLORREF> Processing

template <> void Setting<COLORREF>::ReadPersistedProperty()
{
    ReadBinaryProperty(m_Section, m_Entry, &m_Value, sizeof(COLORREF));
}

template <> void Setting<COLORREF>::WritePersistedProperty()
{
    CDirStatApp::Get()->WriteProfileBinary(m_Section.c_str(), m_Entry.c_str(), reinterpret_cast<LPBYTE>(&m_Value), sizeof(COLORREF));
}

// Setting<double> Processing

template <> void Setting<double>::ReadPersistedProperty()
{
    const double def = m_Value;
    ReadBinaryProperty(m_Section, m_Entry, &m_Value, sizeof(double));
    if (m_Value != def && m_Min != m_Max) m_Value = max(min(m_Value, m_Max), m_Min);
}

template <> void Setting<double>::WritePersistedProperty()
{
    CDirStatApp::Get()->WriteProfileBinary(m_Section.c_str(), m_Entry.c_str(), reinterpret_cast<LPBYTE>(&m_Value), sizeof(double));
}

// Setting<RECT> Processing

template <> void Setting<RECT>::ReadPersistedProperty()
{
    ReadBinaryProperty(m_Section, m_Entry, &m_Value, sizeof(RECT));
}

template <> void Setting<RECT>::WritePersistedProperty()
{
    CDirStatApp::Get()->WriteProfileBinary(m_Section.c_str(), m_Entry.c_str(), reinterpret_cast<LPBYTE>(&m_Value), sizeof(RECT));
}
