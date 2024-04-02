// Property.cpp - Implementation of CPageTreelist
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

#include "stdafx.h"
#include "Property.h"

#include <sstream>

std::vector<PersistedSetting*>& PersistedSetting::GetPropertySet()
{
    static std::vector<PersistedSetting*> _properties;
    return _properties;
}

bool PersistedSetting::ReadBinaryProperty(const std::wstring& section, const std::wstring& entry, LPVOID dest, size_t size)
{
    LPBYTE data = nullptr;
    UINT data_size = 0;
    AfxGetApp()->GetProfileBinary(section.c_str(), entry.c_str(), &data, &data_size);
    const bool success = (data_size == size);
    if (success) memcpy(dest, data, data_size);
    delete[] data;
    return success;
}

// Setting<int> Processing

template <> void Setting<int>::ReadPersistedProperty()
{
    const int def = _value;
    _value = AfxGetApp()->GetProfileInt(_section.c_str(), _entry.c_str(), _value);
    if (_value != def && _min != _max) _value = max(min(_value, _max), _min);
}

template <> void Setting<int>::WritePersistedProperty()
{
    AfxGetApp()->WriteProfileInt(_section.c_str(), _entry.c_str(), _value);
}

// Setting<bool> Processing

template <> void Setting<bool>::ReadPersistedProperty()
{
    _value = AfxGetApp()->GetProfileInt(_section.c_str(), _entry.c_str(), _value == 0 ? 0 : 1) != 0;
}

template <> void Setting<bool>::WritePersistedProperty()
{
    AfxGetApp()->WriteProfileInt(_section.c_str(), _entry.c_str(), _value == 0 ? 0 : 1);
}

// Setting<std::wstring> Processing

template <> void Setting<std::wstring>::ReadPersistedProperty()
{
    _value = AfxGetApp()->GetProfileString(_section.c_str(), _entry.c_str(), _value.c_str()).GetBuffer();
}

template <> void Setting<std::wstring>::WritePersistedProperty()
{
    AfxGetApp()->WriteProfileString(_section.c_str(), _entry.c_str(), _value.c_str());
}

// Setting<WINDOWPLACEMENT> Processing

template <> void Setting<WINDOWPLACEMENT>::ReadPersistedProperty()
{
    ReadBinaryProperty(_section, _entry, &_value, sizeof(WINDOWPLACEMENT));
}

template <> void Setting<WINDOWPLACEMENT>::WritePersistedProperty()
{
    AfxGetApp()->WriteProfileBinary(_section.c_str(), _entry.c_str(), LPBYTE(&_value), sizeof(WINDOWPLACEMENT));
}

// Setting<std::vector<std::wstring>> Processing

template <> void Setting<std::vector<std::wstring>>::ReadPersistedProperty()
{
    const std::wstring s = AfxGetApp()->GetProfileString(_section.c_str(), _entry.c_str()).GetBuffer();
    std::wstringstream iss(s);

    _value.clear();
    for (std::wstring part; std::getline(iss, part, L'|');)
    {
        _value.push_back(part);
    }
}

template <> void Setting<std::vector<std::wstring>>::WritePersistedProperty()
{
    std::wstring result;
    for (const auto & part : _value)
    {
        result += part + L'|';
    }
    if (result.ends_with(L'|')) result.pop_back();

    AfxGetApp()->WriteProfileString(_section.c_str(), _entry.c_str(), result.c_str());
}

// Setting<std::vector<int>> Processing

template <> void Setting<std::vector<int>>::ReadPersistedProperty()
{
    const std::wstring s = AfxGetApp()->GetProfileString(_section.c_str(), _entry.c_str()).GetBuffer();
    std::wstringstream iss(s);

    _value.clear();
    for (std::wstring part; std::getline(iss, part, L',');)
    {
        _value.push_back(_wtoi(part.c_str()));
    }
}

template <> void Setting<std::vector<int>>::WritePersistedProperty()
{
    std::wstring result;
    for (const auto part : _value)
    {
        result += std::to_wstring(part) + L',';
    }
    if (result.ends_with(L',')) result.pop_back();

    AfxGetApp()->WriteProfileString(_section.c_str(), _entry.c_str(), result.c_str());
}

// Setting<COLORREF> Processing

template <> void Setting<COLORREF>::ReadPersistedProperty()
{
    ReadBinaryProperty(_section, _entry, &_value, sizeof(COLORREF));
}

template <> void Setting<COLORREF>::WritePersistedProperty()
{
    AfxGetApp()->WriteProfileBinary(_section.c_str(), _entry.c_str(), reinterpret_cast<LPBYTE>(&_value), sizeof(COLORREF));
}

// Setting<double> Processing

template <> void Setting<double>::ReadPersistedProperty()
{
    const double def = _value;
    ReadBinaryProperty(_section, _entry, &_value, sizeof(double));
    if (_value != def && _min != _max) _value = max(min(_value, _max), _min);
}

template <> void Setting<double>::WritePersistedProperty()
{
    AfxGetApp()->WriteProfileBinary(_section.c_str(), _entry.c_str(), reinterpret_cast<LPBYTE>(&_value), sizeof(double));
}

// Setting<RECT> Processing

template <> void Setting<RECT>::ReadPersistedProperty()
{
    ReadBinaryProperty(_section, _entry, &_value, sizeof(RECT));
}

template <> void Setting<RECT>::WritePersistedProperty()
{
    AfxGetApp()->WriteProfileBinary(_section.c_str(), _entry.c_str(), reinterpret_cast<LPBYTE>(&_value), sizeof(RECT));
}

