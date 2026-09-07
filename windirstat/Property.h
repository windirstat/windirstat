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

class PersistedSetting
{
protected:

    static std::vector<PersistedSetting*>& GetPropertySet();
    static bool ReadBinaryProperty(const std::wstring & section, const std::wstring & entry, LPVOID dest, size_t size);
    std::wstring m_entry;
    std::wstring m_section;

public:

    virtual void ReadPersistedProperty() = 0;
    virtual void WritePersistedProperty() = 0;

    PersistedSetting();
    virtual ~PersistedSetting();

    static void UseRegistryStorage() noexcept;
    static void UseIniStorage(std::wstring path);
    static void ReadPersistedProperties();
    static void WritePersistedProperties();
};

template <typename T = void>
class Setting final : PersistedSetting
{
    T m_value{};
    T m_min{};
    T m_max{};

public:

    T* Ptr() noexcept { return &m_value; }
    T& Obj() noexcept { return m_value; }
    const T& Min() const noexcept { return m_min; }
    const T& Max() const noexcept { return m_max; }
    const T& Obj() const noexcept { return m_value; }

    // Member persistence read/write settings
    void ReadPersistedProperty() override;
    void WritePersistedProperty() override;

    // Implicit conversion back to T.
    operator const T& () noexcept { return m_value; }

    // Copy assignment operators
    T& operator=(const T& other) noexcept { return (m_value = other); }
    T& operator=(const Setting& other) noexcept { return m_value = other.m_value; }

    // Math operators
    T& operator++() noexcept { return ++m_value; }
    T& operator--() noexcept { return --m_value; }
    T operator++(int) noexcept { return m_value++; }
    T operator--(int) noexcept { return m_value--; }
    T& operator+=(const T& other) noexcept { return m_value = m_value + other; }
    T& operator-=(const T& other) noexcept { return m_value = m_value - other; }
    T operator+(const T& other) noexcept { return m_value + other; }
    T operator-(const T& other) noexcept { return m_value - other; }
    T operator*(const T& other) noexcept { return m_value * other; }
    T operator/(const T& other) noexcept { return m_value / other; }

    // Forces identical type assignment
    template <typename T2> T2& operator=(const T2&) = delete;

    Setting(const std::wstring& section, const std::wstring& entry, T defaultValue = {}, T checkMin = {}, T checkMax = {}) :
        m_value(std::move(defaultValue)), m_min(std::move(checkMin)), m_max(std::move(checkMax))
    {
        m_entry = entry;
        m_section = section;
    }

    // Default constructor (used by non-persisted properties)
    Setting() = default;
    ~Setting() override = default;

    // Move constructor to allow for use in dynamic containers
    Setting(Setting&& other) noexcept : Setting(other.m_section, other.m_entry, other.m_value, other.m_min, other.m_max) {}
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
