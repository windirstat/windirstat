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

#pragma once

#include <vector>
#include <string>

class PersistedSetting
{
protected:

    static std::vector<PersistedSetting*>& GetPropertySet();
    static bool ReadBinaryProperty(const std::wstring & section, const std::wstring & entry, LPVOID dest, size_t size);
    std::wstring m_Entry;
    std::wstring m_Section;

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
        for (const auto & p : GetPropertySet())
        {
            p->ReadPersistedProperty();
        }
    }

    static void WritePersistedProperties()
    {
        for (const auto & p : GetPropertySet())
        {
            if (p->m_Section.empty()) continue;
            p->WritePersistedProperty();
        }
    }
};

template <typename T = void>
class Setting final : PersistedSetting
{
    T m_Value{};
    T m_Min{};
    T m_Max{};

public:

    T* Ptr() { return &m_Value; }
    T& Obj() { return m_Value; }
    T Min() { return m_Min; }
    T Max() { return m_Max; }
    const T& Obj() const { return m_Value; }

    // Member persistence read/write settings
    void ReadPersistedProperty() override;
    void WritePersistedProperty() override;

    // Implicit conversion back to T.
    operator const T& () { return m_Value; }

    // Copy assignment operators
    T operator=(T other) { return (m_Value = other); }
    T operator=(Setting other) { return (m_Value = other.m_Value); }

    // Math operators
    T operator++() { return ++m_Value; }
    T operator--() { return --m_Value; }
    T operator++(int) { return m_Value++; }
    T operator--(int) { return m_Value--; }
    T operator+=(const T& other) { return m_Value = m_Value + other; }
    T operator-=(const T& other) { return m_Value = m_Value - other; }
    T operator+(const T& other) { return m_Value + other; }
    T operator-(const T& other) { return m_Value - other; }
    T operator*(const T& other) { return m_Value * other; }
    T operator/(const T& other) { return m_Value / other; }

    // Forces identical type assignment
    template <typename T2> T2& operator=(const T2&)
    {
        const T2& guard = m_Value;
        throw guard; // Never reached.
    }

    Setting(const std::wstring& section, const std::wstring& entry, const T& defaultValue = {}, const T& checkMin = {}, const T& checkMax = {}) :
        m_Value(defaultValue), m_Min(checkMin), m_Max(checkMax)
    {
        m_Entry = entry;
        m_Section = section;
    }

    // Default constructor (used by non-persisted properties)
    Setting() = default;
    ~Setting() override = default;

    // Move constructor to allow for use in dynamic containers
    Setting(Setting&& other) noexcept : Setting(other.m_Section, other.m_Entry, other.m_Value, other.m_Min, other.m_Max) {}
};

// explicit instantiation declaration
extern template class Setting<int>;
extern template class Setting<bool>;
extern template class Setting<double>;
extern template class Setting<std::wstring>;
extern template class Setting<std::vector<std::wstring>>;
extern template class Setting<std::vector<int>>;
extern template class Setting<WINDOWPLACEMENT>;
extern template class Setting<RECT>;
extern template class Setting<COLORREF>;
