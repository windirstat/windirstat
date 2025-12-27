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
#include "Property.h"

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
    const int def = m_value;
    m_value = CDirStatApp::Get()->GetProfileInt(m_section.c_str(), m_entry.c_str(), m_value);
    if (m_value != def && m_min != m_max) m_value = std::clamp(m_value, m_min, m_max);
}

template <> void Setting<int>::WritePersistedProperty()
{
    CDirStatApp::Get()->WriteProfileInt(m_section.c_str(), m_entry.c_str(), m_value);
}

// Setting<bool> Processing

template <> void Setting<bool>::ReadPersistedProperty()
{
    m_value = CDirStatApp::Get()->GetProfileInt(m_section.c_str(), m_entry.c_str(), m_value == 0 ? 0 : 1) != 0;
}

template <> void Setting<bool>::WritePersistedProperty()
{
    CDirStatApp::Get()->WriteProfileInt(m_section.c_str(), m_entry.c_str(), m_value == 0 ? 0 : 1);
}

// Setting<std::wstring> Processing

template <> void Setting<std::wstring>::ReadPersistedProperty()
{
    m_value = CDirStatApp::Get()->GetProfileString(m_section.c_str(), m_entry.c_str(), m_value.c_str());
    m_value = std::regex_replace(m_value, std::wregex(LR"(\x1e)"), L"\r\n");
}

template <> void Setting<std::wstring>::WritePersistedProperty()
{
    const std::wstring valueCleaned = std::regex_replace(m_value, std::wregex(LR"((\r|\n)+)"), L"\x1e");
    CDirStatApp::Get()->WriteProfileString(m_section.c_str(), m_entry.c_str(), valueCleaned.c_str());
}

// Setting<WINDOWPLACEMENT> Processing

template <> void Setting<WINDOWPLACEMENT>::ReadPersistedProperty()
{
    ReadBinaryProperty(m_section, m_entry, &m_value, sizeof(WINDOWPLACEMENT));
}

template <> void Setting<WINDOWPLACEMENT>::WritePersistedProperty()
{
    CDirStatApp::Get()->WriteProfileBinary(m_section.c_str(), m_entry.c_str(),
        reinterpret_cast<LPBYTE>(&m_value), sizeof(WINDOWPLACEMENT));
}

// Setting<std::vector<std::wstring>> Processing

template <> void Setting<std::vector<std::wstring>>::ReadPersistedProperty()
{
    m_value.clear();
    for (const std::wstring s = CDirStatApp::Get()->GetProfileString(m_section.c_str(), m_entry.c_str()).GetString();
        const auto token_view : std::views::split(s, L'|'))
    {
        m_value.emplace_back(token_view.begin(), token_view.end());
    }
}

template <> void Setting<std::vector<std::wstring>>::WritePersistedProperty()
{
    std::wstring result;
    for (const auto & part : m_value)
    {
        result += part + L'|';
    }
    if (result.ends_with(L'|')) result.pop_back();

    CDirStatApp::Get()->WriteProfileString(m_section.c_str(), m_entry.c_str(), result.c_str());
}

// Setting<std::vector<int>> Processing

template <> void Setting<std::vector<int>>::ReadPersistedProperty()
{
    m_value.clear();
    for (const std::wstring s = CDirStatApp::Get()->GetProfileString(m_section.c_str(), m_entry.c_str()).GetString();
        const auto token_view : std::views::split(s, L','))
    {
        m_value.push_back(std::stoi(std::wstring(token_view.begin(), token_view.end())));
    }
}

template <> void Setting<std::vector<int>>::WritePersistedProperty()
{
    std::wstring result;
    for (const auto part : m_value)
    {
        result += std::to_wstring(part) + L',';
    }
    if (result.ends_with(L',')) result.pop_back();

    CDirStatApp::Get()->WriteProfileString(m_section.c_str(), m_entry.c_str(), result.c_str());
}

// Setting<COLORREF> Processing

template <> void Setting<COLORREF>::ReadPersistedProperty()
{
    ReadBinaryProperty(m_section, m_entry, &m_value, sizeof(COLORREF));
}

template <> void Setting<COLORREF>::WritePersistedProperty()
{
    CDirStatApp::Get()->WriteProfileBinary(m_section.c_str(), m_entry.c_str(), reinterpret_cast<LPBYTE>(&m_value), sizeof(COLORREF));
}

// Setting<double> Processing

template <> void Setting<double>::ReadPersistedProperty()
{
    const double def = m_value;
    ReadBinaryProperty(m_section, m_entry, &m_value, sizeof(double));
    if (m_value != def && m_min != m_max) m_value = std::clamp(m_value, m_min, m_max);
}

template <> void Setting<double>::WritePersistedProperty()
{
    CDirStatApp::Get()->WriteProfileBinary(m_section.c_str(), m_entry.c_str(), reinterpret_cast<LPBYTE>(&m_value), sizeof(double));
}

// Setting<RECT> Processing

template <> void Setting<RECT>::ReadPersistedProperty()
{
    ReadBinaryProperty(m_section, m_entry, &m_value, sizeof(RECT));
}

template <> void Setting<RECT>::WritePersistedProperty()
{
    CDirStatApp::Get()->WriteProfileBinary(m_section.c_str(), m_entry.c_str(), reinterpret_cast<LPBYTE>(&m_value), sizeof(RECT));
}
