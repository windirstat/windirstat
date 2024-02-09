// Property.h - Implementation of CPageTreelist
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

#pragma once

#include <vector>
#include <string>

class PersistedSetting
{
protected:

    static std::vector<PersistedSetting*>& GetPropertySet();
    static bool ReadBinaryProperty(std::wstring & section, std::wstring & entry, LPVOID dest, size_t size);
    std::wstring _entry;
    std::wstring _section;

public:

    virtual void ReadPersistedProperty() = 0;
    virtual void WritePersistedProperty() = 0;

    PersistedSetting()
    {
        GetPropertySet().push_back(this);
    }

    virtual ~PersistedSetting()
    {
        std::erase(GetPropertySet(), this);
    }

    static void ReadPersistedProperties()
    {
        for (auto p : GetPropertySet())
        {
            p->ReadPersistedProperty();
        }
    }

    static void WritePersistedProperties()
    {
        for (auto p : GetPropertySet())
        {
            if (p->_section.empty()) continue;
            p->WritePersistedProperty();
        }
    }
};

template <typename T = void>
class Setting final : PersistedSetting
{
protected:

    T _value;
    T _min;
    T _max;

public:

    T* Ptr() { return &_value; }
    T& Obj() { return _value; }
    const T& Obj() const { return _value; }

    // Member persistence read/write settings
    void ReadPersistedProperty() override;
    void WritePersistedProperty() override;

    // Implicit conversion back to T.
    operator const T& () { return _value; }

    // Copy assignment operators
    T operator=(T other) { return (_value = other); }
    T operator=(Setting<T> other) { return (_value == other._value); }

    // Math operators
    T operator++() { return ++_value; }
    T operator--() { return --_value; }
    T operator++(int) { return _value++; }
    T operator--(int) { return _value--; }
    T operator+=(const T& other) { return _value = _value + other; }
    T operator-=(const T& other) { return _value = _value - other; }
    T operator+(const T& other) { return _value + other; }
    T operator-(const T& other) { return _value - other; }
    T operator*(const T& other) { return _value * other; }
    T operator/(const T& other) { return _value / other; }

    // Forces identical type assignment
    template <typename T2> T2& operator=(const T2& other)
    {
        T2& guard = _value;
        throw guard; // Never reached.
    }

    Setting(const std::wstring& section, const std::wstring& entry, const T& default_value = {}, const T& check_min = {}, const T& check_max = {}) : PersistedSetting()
    {
        _value = default_value;
        _entry = entry;
        _section = section;
        _min = check_min;
        _max = check_max;
    }

    // Default constructor (used by non-persisted properties)
    Setting<T>() = default;
    ~Setting<T>() override = default;

    // Move constructor to allow for use in dynamic containers
    Setting(Setting&& other) noexcept : Setting(other._section, other._entry, other._value, other._min, other._max) {}
};
