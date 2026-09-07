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

class PersistedSettingStorage final
{
public:
    void UseRegistry() noexcept { m_iniPath.reset(); }
    void UseIni(std::wstring path) { m_iniPath = std::move(path); }

    int ReadInt(const std::wstring& section, const std::wstring& entry, int defaultValue) const;
    bool WriteInt(const std::wstring& section, const std::wstring& entry, int value) const;
    std::wstring ReadString(const std::wstring& section, const std::wstring& entry,
        const std::wstring& defaultValue = {}) const;
    bool WriteString(const std::wstring& section, const std::wstring& entry, const std::wstring& value) const;
    bool ReadBinary(const std::wstring& section, const std::wstring& entry, std::span<std::byte> destination) const;
    bool WriteBinary(const std::wstring& section, const std::wstring& entry, std::span<const std::byte> data) const;

private:
    static std::wstring RegistryPath(const std::wstring& section)
    {
        return L"Software\\WinDirStat\\WinDirStat\\" + section;
    }

    std::optional<std::wstring> m_iniPath;
};

static PersistedSettingStorage& GetPersistedSettingStorage()
{
    static PersistedSettingStorage storage;
    return storage;
}

int PersistedSettingStorage::ReadInt(const std::wstring& section, const std::wstring& entry,
    const int defaultValue) const
{
    if (m_iniPath)
        return static_cast<int>(GetPrivateProfileIntW(section.c_str(), entry.c_str(), defaultValue,
            m_iniPath->c_str()));

    DWORD value = 0;
    if (CRegKey key; key.Open(HKEY_CURRENT_USER, RegistryPath(section).c_str(), KEY_READ) != ERROR_SUCCESS ||
        key.QueryDWORDValue(entry.c_str(), value) != ERROR_SUCCESS) return defaultValue;
    return static_cast<int>(value);
}

bool PersistedSettingStorage::WriteInt(const std::wstring& section, const std::wstring& entry,
    const int value) const
{
    if (m_iniPath)
    {
        wchar_t buffer[32] = {};
        if (_itow_s(value, buffer, 10) != 0) return false;
        return WritePrivateProfileStringW(section.c_str(), entry.c_str(), buffer, m_iniPath->c_str());
    }

    CRegKey key;
    return key.Create(HKEY_CURRENT_USER, RegistryPath(section).c_str(), nullptr, REG_OPTION_NON_VOLATILE,
        KEY_WRITE) == ERROR_SUCCESS && key.SetDWORDValue(entry.c_str(), static_cast<DWORD>(value)) == ERROR_SUCCESS;
}

std::wstring PersistedSettingStorage::ReadString(const std::wstring& section, const std::wstring& entry,
    const std::wstring& defaultValue) const
{
    if (m_iniPath)
    {
        constexpr DWORD initialBufferSize = 8192;
        constexpr DWORD maximumValueLength = 1024 * 1024;
        constexpr DWORD maximumBufferSize = maximumValueLength + 2;
        const size_t defaultBufferSize = std::min(defaultValue.size(),
            static_cast<size_t>(maximumValueLength)) + 1;
        DWORD bufferSize = static_cast<DWORD>(std::max(defaultBufferSize,
            static_cast<size_t>(initialBufferSize)));

        while (true)
        {
            std::wstring value(bufferSize, L'\0');
            const DWORD length = GetPrivateProfileStringW(section.c_str(), entry.c_str(), defaultValue.c_str(),
                value.data(), bufferSize, m_iniPath->c_str());
            if (length < bufferSize - 1)
            {
                value.resize(length);
                return value;
            }
            // Do not return a truncated setting that could later be saved destructively.
            if (bufferSize == maximumBufferSize) return defaultValue;
            bufferSize = std::min(bufferSize * 2, maximumBufferSize);
        }
    }

    CRegKey key;
    if (key.Open(HKEY_CURRENT_USER, RegistryPath(section).c_str(), KEY_READ) != ERROR_SUCCESS) return defaultValue;

    DWORD type = 0;
    DWORD size = 0;
    if (RegQueryValueExW(key, entry.c_str(), nullptr, &type, nullptr, &size) != ERROR_SUCCESS ||
        type != REG_SZ || size == 0) return defaultValue;

    const size_t chars = (static_cast<size_t>(size) + sizeof(wchar_t) - 1) / sizeof(wchar_t);
    std::wstring value(chars + 1, L'\0');
    DWORD bytesRead = size;
    if (RegQueryValueExW(key, entry.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(value.data()),
        &bytesRead) != ERROR_SUCCESS || type != REG_SZ) return defaultValue;

    value.resize(std::min(chars, static_cast<size_t>(bytesRead) / sizeof(wchar_t)));
    while (!value.empty() && value.back() == L'\0') value.pop_back();
    return value;
}

bool PersistedSettingStorage::WriteString(const std::wstring& section, const std::wstring& entry,
    const std::wstring& value) const
{
    if (m_iniPath)
        return WritePrivateProfileStringW(section.c_str(), entry.c_str(), value.c_str(), m_iniPath->c_str());
    if (value.size() >= std::numeric_limits<DWORD>::max() / sizeof(wchar_t)) return false;

    CRegKey key;
    if (key.Create(HKEY_CURRENT_USER, RegistryPath(section).c_str(), nullptr, REG_OPTION_NON_VOLATILE,
        KEY_WRITE) != ERROR_SUCCESS) return false;
    const DWORD size = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    return RegSetValueExW(key, entry.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()), size) ==
        ERROR_SUCCESS;
}

bool PersistedSettingStorage::ReadBinary(const std::wstring& section, const std::wstring& entry,
    const std::span<std::byte> destination) const
{
    if (destination.empty() || destination.size() > std::numeric_limits<DWORD>::max()) return false;

    if (!m_iniPath)
    {
        CRegKey key;
        if (key.Open(HKEY_CURRENT_USER, RegistryPath(section).c_str(), KEY_READ) != ERROR_SUCCESS) return false;

        DWORD type = 0;
        DWORD size = 0;
        if (RegQueryValueExW(key, entry.c_str(), nullptr, &type, nullptr, &size) != ERROR_SUCCESS ||
            type != REG_BINARY || std::cmp_not_equal(size, destination.size())) return false;

        DWORD bytesRead = size;
        return RegQueryValueExW(key, entry.c_str(), nullptr, &type,
            reinterpret_cast<LPBYTE>(destination.data()), &bytesRead) == ERROR_SUCCESS &&
            type == REG_BINARY && std::cmp_equal(bytesRead, destination.size());
    }

    if (destination.size() > (std::numeric_limits<DWORD>::max() - 1) / 2) return false;
    const DWORD expectedLength = static_cast<DWORD>(destination.size() * 2);
    std::wstring hex(static_cast<size_t>(expectedLength) + 1, L'\0');
    const DWORD length = GetPrivateProfileStringW(section.c_str(), entry.c_str(), L"", hex.data(),
        static_cast<DWORD>(hex.size()), m_iniPath->c_str());
    if (length != expectedLength) return false;

    const auto hexValue = [](const wchar_t value) -> int
        {
            if (value >= L'0' && value <= L'9') return value - L'0';
            if (value >= L'a' && value <= L'f') return value - L'a' + 10;
            if (value >= L'A' && value <= L'F') return value - L'A' + 10;
            return -1;
        };
    for (size_t i = 0; i < destination.size(); ++i)
    {
        const int high = hexValue(hex[i * 2]);
        const int low = hexValue(hex[i * 2 + 1]);
        if (high < 0 || low < 0) return false;
        destination[i] = static_cast<std::byte>((high << 4) | low);
    }
    return true;
}

bool PersistedSettingStorage::WriteBinary(const std::wstring& section, const std::wstring& entry,
    const std::span<const std::byte> data) const
{
    if (data.size() > std::numeric_limits<DWORD>::max()) return false;
    if (!m_iniPath)
    {
        CRegKey key;
        if (key.Create(HKEY_CURRENT_USER, RegistryPath(section).c_str(), nullptr, REG_OPTION_NON_VOLATILE,
            KEY_WRITE) != ERROR_SUCCESS) return false;
        return RegSetValueExW(key, entry.c_str(), 0, REG_BINARY, reinterpret_cast<const BYTE*>(data.data()),
            static_cast<DWORD>(data.size())) == ERROR_SUCCESS;
    }

    static constexpr wchar_t hexDigits[] = L"0123456789ABCDEF";
    std::wstring hex;
    hex.reserve(data.size() * 2);
    for (const std::byte value : data)
    {
        const unsigned byte = std::to_integer<unsigned>(value);
        hex.push_back(hexDigits[byte >> 4]);
        hex.push_back(hexDigits[byte & 0x0f]);
    }
    return WritePrivateProfileStringW(section.c_str(), entry.c_str(), hex.c_str(), m_iniPath->c_str());
}

PersistedSetting::PersistedSetting()
{
    GetPropertySet().push_back(this);
}

PersistedSetting::~PersistedSetting()
{
    std::erase(GetPropertySet(), this);
}

void PersistedSetting::UseRegistryStorage() noexcept
{
    GetPersistedSettingStorage().UseRegistry();
}

void PersistedSetting::UseIniStorage(std::wstring path)
{
    GetPersistedSettingStorage().UseIni(std::move(path));
}

void PersistedSetting::ReadPersistedProperties()
{
    for (PersistedSetting* property : GetPropertySet()) property->ReadPersistedProperty();
}

void PersistedSetting::WritePersistedProperties()
{
    for (PersistedSetting* property : GetPropertySet())
    {
        if (!property->m_section.empty()) property->WritePersistedProperty();
    }
}

std::vector<PersistedSetting*>& PersistedSetting::GetPropertySet()
{
    static std::vector<PersistedSetting*> _properties;
    return _properties;
}

bool PersistedSetting::ReadBinaryProperty(const std::wstring& section, const std::wstring& entry,
    const LPVOID dest, const size_t size)
{
    return GetPersistedSettingStorage().ReadBinary(section, entry,
        { static_cast<std::byte*>(dest), size });
}

// Setting<int> Processing

template <> void Setting<int>::ReadPersistedProperty()
{
    const int def = m_value;
    m_value = GetPersistedSettingStorage().ReadInt(m_section, m_entry, m_value);
    if (m_value != def && m_min != m_max) m_value = std::clamp(m_value, m_min, m_max);
}

template <> void Setting<int>::WritePersistedProperty()
{
    GetPersistedSettingStorage().WriteInt(m_section, m_entry, m_value);
}

// Setting<bool> Processing

template <> void Setting<bool>::ReadPersistedProperty()
{
    m_value = GetPersistedSettingStorage().ReadInt(m_section, m_entry, m_value ? 1 : 0) != 0;
}

template <> void Setting<bool>::WritePersistedProperty()
{
    GetPersistedSettingStorage().WriteInt(m_section, m_entry, m_value ? 1 : 0);
}

// Setting<std::wstring> Processing

template <> void Setting<std::wstring>::ReadPersistedProperty()
{
    m_value = GetPersistedSettingStorage().ReadString(m_section, m_entry, m_value);
    static const std::wregex reRead(LR"(\x1e)");
    m_value = std::regex_replace(m_value, reRead, L"\r\n");
}

template <> void Setting<std::wstring>::WritePersistedProperty()
{
    static const std::wregex reWrite(LR"((\r|\n)+)");
    const std::wstring valueCleaned = std::regex_replace(m_value, reWrite, L"\x1e");
    GetPersistedSettingStorage().WriteString(m_section, m_entry, valueCleaned);
}

// Setting<WINDOWPLACEMENT> Processing

template <> void Setting<WINDOWPLACEMENT>::ReadPersistedProperty()
{
    if (ReadBinaryProperty(m_section, m_entry, &m_value, sizeof(WINDOWPLACEMENT)))
    {
        // The user owns the main-window size; only adjust it for the display DPI.
        m_value.rcNormalPosition.left = ScaleForScreenDpi(m_value.rcNormalPosition.left);
        m_value.rcNormalPosition.top = ScaleForScreenDpi(m_value.rcNormalPosition.top);
        m_value.rcNormalPosition.right = ScaleForScreenDpi(m_value.rcNormalPosition.right);
        m_value.rcNormalPosition.bottom = ScaleForScreenDpi(m_value.rcNormalPosition.bottom);
    }
}

template <> void Setting<WINDOWPLACEMENT>::WritePersistedProperty()
{
    // Scale rcNormalPosition from current DPI to 96 DPI for storage
    WINDOWPLACEMENT normalizedWp = m_value;
    normalizedWp.rcNormalPosition.left = UnscaleForScreenDpi(m_value.rcNormalPosition.left);
    normalizedWp.rcNormalPosition.top = UnscaleForScreenDpi(m_value.rcNormalPosition.top);
    normalizedWp.rcNormalPosition.right = UnscaleForScreenDpi(m_value.rcNormalPosition.right);
    normalizedWp.rcNormalPosition.bottom = UnscaleForScreenDpi(m_value.rcNormalPosition.bottom);
    GetPersistedSettingStorage().WriteBinary(m_section, m_entry,
        std::as_bytes(std::span{ &normalizedWp, 1 }));
}

// Setting<std::vector<std::wstring>> Processing

template <> void Setting<std::vector<std::wstring>>::ReadPersistedProperty()
{
    const std::wstring s = GetPersistedSettingStorage().ReadString(m_section, m_entry);
    m_value = SplitString(s);
}

template <> void Setting<std::vector<std::wstring>>::WritePersistedProperty()
{
    const std::wstring result = JoinString(m_value);
    GetPersistedSettingStorage().WriteString(m_section, m_entry, result);
}

// Setting<std::vector<int>> Processing

template <> void Setting<std::vector<int>>::ReadPersistedProperty()
{
    m_value.clear();
    for (const std::wstring s = GetPersistedSettingStorage().ReadString(m_section, m_entry);
        const auto& token : SplitString(s, L','))
    {
        // Scale from stored 96 DPI values to current DPI
        int value = std::stoi(token);
        if (m_entry == L"ColumnWidths") value = ScaleForDpi(value);
        m_value.push_back(value);
    }
}

template <> void Setting<std::vector<int>>::WritePersistedProperty()
{
    std::wstring result;
    for (const auto part : m_value)
    {
        const auto val = (m_entry == L"ColumnWidths") ? UnscaleForDpi(part) : part;
        result += std::to_wstring(val) + L',';
    }
    if (result.ends_with(L',')) result.pop_back();

    GetPersistedSettingStorage().WriteString(m_section, m_entry, result);
}

// Setting<COLORREF> Processing

template <> void Setting<COLORREF>::ReadPersistedProperty()
{
    ReadBinaryProperty(m_section, m_entry, &m_value, sizeof(COLORREF));
}

template <> void Setting<COLORREF>::WritePersistedProperty()
{
    GetPersistedSettingStorage().WriteBinary(m_section, m_entry, std::as_bytes(std::span{ &m_value, 1 }));
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
    GetPersistedSettingStorage().WriteBinary(m_section, m_entry, std::as_bytes(std::span{ &m_value, 1 }));
}

// Setting<RECT> Processing

template <> void Setting<RECT>::ReadPersistedProperty()
{
    if (ReadBinaryProperty(m_section, m_entry, &m_value, sizeof(RECT)))
    {
        const RECT stored = m_value;
        m_value.left = ScaleForScreenDpi(stored.left);
        m_value.top = ScaleForScreenDpi(stored.top);
        m_value.right = m_value.left + ScaleForDpi(stored.right - stored.left);
        m_value.bottom = m_value.top + ScaleForDpi(stored.bottom - stored.top);
    }
}

template <> void Setting<RECT>::WritePersistedProperty()
{
    // Scale from current DPI to 96 DPI for storage
    RECT normalizedRect;
    normalizedRect.left = UnscaleForScreenDpi(m_value.left);
    normalizedRect.top = UnscaleForScreenDpi(m_value.top);
    normalizedRect.right = normalizedRect.left + UnscaleForDpi(m_value.right - m_value.left);
    normalizedRect.bottom = normalizedRect.top + UnscaleForDpi(m_value.bottom - m_value.top);
    GetPersistedSettingStorage().WriteBinary(m_section, m_entry,
        std::as_bytes(std::span{ &normalizedRect, 1 }));
}
