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
#include "FinderMtp.h"

static constexpr std::wstring_view MtpPrefix = L"mtp:";

struct FinderMtpWpdDevice
{
    std::wstring id;
    std::wstring name;
};

struct FinderMtpShellDevice
{
    std::wstring path;
    std::wstring name;
    std::vector<std::wstring> identities;
    bool used = false;
};

struct FinderMtpDeviceIds final
{
    FinderMtpDeviceIds() = default;
    ~FinderMtpDeviceIds()
    {
        // Release all device identifiers allocated by the portable device manager.
        FreePortableDevicePnPIDs(values.data(), static_cast<DWORD>(values.size()));
    }

    FinderMtpDeviceIds(const FinderMtpDeviceIds&) = delete;
    FinderMtpDeviceIds& operator=(const FinderMtpDeviceIds&) = delete;

    std::vector<LPWSTR> values;
};

static std::wstring FinderMtpPropertyString(IShellItem2* item, REFPROPERTYKEY key)
{
    // Copy the property before releasing its COM-allocated buffer.
    PWSTR value = nullptr;
    const HRESULT result = item ? item->GetString(key, &value) : E_POINTER;
    std::wstring text = SUCCEEDED(result) && value ? value : L"";
    CoTaskMemFree(value);
    return text;
}

static std::wstring FinderMtpDisplayName(IShellItem* item, SIGDN type)
{
    // Copy the requested display name before releasing its COM-allocated buffer.
    PWSTR value = nullptr;
    const HRESULT result = item ? item->GetDisplayName(type, &value) : E_POINTER;
    std::wstring text = SUCCEEDED(result) && value ? value : L"";
    CoTaskMemFree(value);
    return text;
}

static std::wstring FinderMtpParsingPath(IShellItem2* item)
{
    // Fall back to the absolute Shell name when the parsing-path property is unavailable.
    std::wstring path = FinderMtpPropertyString(item, PKEY_ParsingPath);
    if (path.empty()) path = FinderMtpDisplayName(item, SIGDN_DESKTOPABSOLUTEPARSING);
    return path;
}

static std::wstring FinderMtpItemName(IShellItem2* item)
{
    // Fall back to the normal Shell name when the display-name property is unavailable.
    std::wstring name = FinderMtpPropertyString(item, PKEY_ItemNameDisplay);
    if (name.empty()) name = FinderMtpDisplayName(item, SIGDN_NORMALDISPLAY);
    return name;
}

static CComPtr<IShellItem2> FinderMtpResolve(std::wstring_view path)
{
    // Convert the unprefixed parsing path into a Shell item through its PIDL.
    const std::wstring parsingPath = FinderMtp::StripPrefix(path);
    if (parsingPath.empty()) return {};

    PIDLIST_ABSOLUTE rawPidl = nullptr;
    if (FAILED(SHParseDisplayName(parsingPath.c_str(), nullptr, &rawPidl, 0, nullptr))) return {};
    const SmartPointer pidl(CoTaskMemFree, rawPidl);

    CComPtr<IShellItem2> item;
    if (FAILED(SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&item)))) return {};
    return item;
}

static CComPtr<IShellItem2> FinderMtpResolve(const ULONGLONG token)
{
    // Prefer the registered PIDL and fall back to its parsing path when necessary.
    const SmartPointer pidl(CoTaskMemFree, FinderMtp::CloneShellPidl(token));
    CComPtr<IShellItem2> item;
    if (pidl && SUCCEEDED(SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&item)))) return item;
    return FinderMtpResolve(FinderMtp::GetShellPath(token));
}

static std::wstring FinderMtpPrefixedPath(std::wstring_view path)
{
    if (FinderMtp::IsPath(path)) return std::wstring(path);
    std::wstring prefixedPath(MtpPrefix);
    prefixedPath.append(path);
    return prefixedPath;
}

static std::wstring FinderMtpManagerName(IPortableDeviceManager* manager, const std::wstring& id)
{
    // Query the required buffer length before retrieving and trimming the friendly name.
    DWORD length = 0;
    manager->GetDeviceFriendlyName(id.c_str(), nullptr, &length);
    if (length == 0) return {};

    std::wstring name(length, L'\0');
    if (FAILED(manager->GetDeviceFriendlyName(id.c_str(), name.data(), &length))) return {};
    while (!name.empty() && name.back() == L'\0') name.pop_back();
    return name;
}

static bool FinderMtpIsMassStorage(IPortableDeviceManager* manager, const std::wstring& id)
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = REG_NONE;
    return SUCCEEDED(manager->GetDeviceProperty(id.c_str(), PORTABLE_DEVICE_IS_MASS_STORAGE,
        reinterpret_cast<BYTE*>(&value), &size, &type)) && type == REG_DWORD && value != 0;
}

static std::vector<FinderMtpWpdDevice> FinderMtpGetWpdDevices(IPortableDeviceManager* manager)
{
    // Load the complete set of device identifiers from the portable device manager.
    DWORD count = 0;
    if (FAILED(manager->GetDevices(nullptr, &count)) || count == 0) return {};

    FinderMtpDeviceIds ids;
    ids.values.resize(count);
    if (FAILED(manager->GetDevices(ids.values.data(), &count))) return {};

    // Exclude mass-storage devices and associate each remaining identifier with its friendly name.
    std::vector<FinderMtpWpdDevice> devices;
    devices.reserve(count);
    for (DWORD i = 0; i < count; ++i)
    {
        if (!ids.values[i]) continue;
        std::wstring id(ids.values[i]);
        if (FinderMtpIsMassStorage(manager, id)) continue;
        devices.push_back({ id, FinderMtpManagerName(manager, id) });
    }
    return devices;
}

static std::vector<std::wstring> FinderMtpInterfacePaths(IShellItem2* item)
{
    // Read the interface-path property into a variant that accepts either supported representation.
    PROPVARIANT value;
    PropVariantInit(&value);
    if (!item || FAILED(item->GetProperty(PKEY_Devices_InterfacePaths, &value)))
    {
        PropVariantClear(&value);
        return {};
    }

    // Normalize vector and scalar property values into one path collection.
    std::vector<std::wstring> paths;
    if (value.vt == static_cast<VARTYPE>(VT_VECTOR | VT_LPWSTR))
    {
        paths.reserve(value.calpwstr.cElems);
        for (ULONG i = 0; i < value.calpwstr.cElems; ++i)
        {
            if (value.calpwstr.pElems[i]) paths.emplace_back(value.calpwstr.pElems[i]);
        }
    }
    else if (value.vt == VT_LPWSTR && value.pwszVal)
    {
        paths.emplace_back(value.pwszVal);
    }

    PropVariantClear(&value);
    return paths;
}

static std::vector<FinderMtpShellDevice> FinderMtpGetShellDevices()
{
    // Open and enumerate the entries exposed in the Shell's Computer folder.
    CComPtr<IShellItem> computer;
    if (FAILED(SHGetKnownFolderItem(FOLDERID_ComputerFolder, KF_FLAG_DEFAULT, nullptr,
        IID_PPV_ARGS(&computer)))) return {};

    CComPtr<IEnumShellItems> items;
    if (FAILED(computer->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&items)))) return {};

    std::vector<FinderMtpShellDevice> devices;
    while (true)
    {
        CComPtr<IShellItem> shellItem;
        ULONG fetched = 0;
        if (items->Next(1, &shellItem, &fetched) != S_OK) break;

        // Exclude normal filesystem volumes so only virtual Shell candidates remain.
        SFGAOF attributes = 0;
        if (FAILED(shellItem->GetAttributes(SFGAO_FILESYSTEM, &attributes)) ||
            (attributes & SFGAO_FILESYSTEM) != 0) continue;

        CComQIPtr<IShellItem2> properties(shellItem);
        if (!properties) continue;

        // Capture the Shell path and user-visible name required to present the device.
        std::wstring path = FinderMtpParsingPath(properties);
        if (path.empty()) continue;

        std::wstring name = FinderMtpItemName(properties);

        // Collect every available identity form for later correlation with WPD identifiers.
        std::vector<std::wstring> identities = FinderMtpInterfacePaths(properties);
        if (std::wstring identity = FinderMtpPropertyString(properties, PKEY_Devices_DeviceInstanceId);
            !identity.empty()) identities.push_back(std::move(identity));
        identities.push_back(path);
        devices.push_back({ std::move(path), std::move(name), std::move(identities) });
    }
    return devices;
}

static std::wstring FinderMtpNormalizeId(std::wstring_view id)
{
    // Remove separators and normalize case so equivalent device identifiers compare consistently.
    std::wstring normalized;
    normalized.reserve(id.size());
    for (const wchar_t character : id)
    {
        if (std::iswalnum(character)) normalized.push_back(static_cast<wchar_t>(std::towupper(character)));
    }
    return normalized;
}

static bool FinderMtpIdsMatch(std::wstring_view left, std::wstring_view right)
{
    // Allow containment because WPD and Shell identifiers can include different prefixes or suffixes.
    const std::wstring normalizedLeft = FinderMtpNormalizeId(left);
    const std::wstring normalizedRight = FinderMtpNormalizeId(right);
    if (normalizedLeft.empty() || normalizedRight.empty()) return false;
    return normalizedLeft.find(normalizedRight) != std::wstring::npos ||
        normalizedRight.find(normalizedLeft) != std::wstring::npos;
}

static bool FinderMtpNamesMatch(const std::wstring& left, const std::wstring& right)
{
    return !left.empty() && !right.empty() && _wcsicmp(left.c_str(), right.c_str()) == 0;
}

static void FinderMtpAddSpace(IShellItem2* item, ULONGLONG& total, ULONGLONG& freeBytes)
{
    ULONGLONG value = 0;
    if (SUCCEEDED(item->GetUInt64(PKEY_Capacity, &value))) total += value;
    if (SUCCEEDED(item->GetUInt64(PKEY_FreeSpace, &value))) freeBytes += value;
}

bool FinderMtp::FindNext()
{
    if (!m_com || !m_items) return false;

    // Skip unusable Shell entries until a complete item can be returned.
    while (true)
    {
        // Clear the previous item's Shell identity before advancing the enumerator.
        m_shellPath.clear();
        m_shellPidl = nullptr;

        CComPtr<IShellItem> shellItem;
        ULONG fetched = 0;
        if (m_items->Next(1, &shellItem, &fetched) != S_OK) return false;

        CComQIPtr<IShellItem2> properties(shellItem);
        if (!properties) continue;

        // Retain available PIDL and parsing-path identities so callers can resolve the item later.
        PIDLIST_ABSOLUTE shellPidl = nullptr;
        SHGetIDListFromObject(shellItem, &shellPidl);
        m_shellPidl = shellPidl;

        // Build the public path from the parent path and the item's display name.
        const std::wstring parsingPath = FinderMtpParsingPath(properties);
        if (parsingPath.empty() && !m_shellPidl) continue;
        if (!parsingPath.empty()) m_shellPath = FinderMtpPrefixedPath(parsingPath);
        m_name = FinderMtpItemName(properties);
        if (m_name.empty()) continue;
        m_path = m_basePath;
        if (!m_path.empty() && !m_path.ends_with(L'\\')) m_path += L'\\';
        m_path += m_name;

        // Read the item's size and modification timestamp from Shell properties.
        m_size = 0;
        properties->GetUInt64(PKEY_Size, &m_size);
        m_lastWrite = {};
        properties->GetFileTime(PKEY_DateModified, &m_lastWrite);

        // Merge file attributes with folder and hidden flags reported separately by the Shell.
        m_attributes = 0;
        properties->GetUInt32(PKEY_FileAttributes, &m_attributes);
        SFGAOF shellAttributes = 0;
        if (SUCCEEDED(shellItem->GetAttributes(SFGAO_FOLDER | SFGAO_HIDDEN, &shellAttributes)))
        {
            if ((shellAttributes & SFGAO_FOLDER) != 0)
            {
                m_attributes &= ~FILE_ATTRIBUTE_NORMAL;
                m_attributes |= FILE_ATTRIBUTE_DIRECTORY;
            }
            if ((shellAttributes & SFGAO_HIDDEN) != 0) m_attributes |= FILE_ATTRIBUTE_HIDDEN;
        }
        if (m_attributes == 0) m_attributes = FILE_ATTRIBUTE_NORMAL;
        return true;
    }
}

bool FinderMtp::FindFile(const CItem* item)
{
    // Reset any previous enumeration before resolving the requested parent item.
    m_items = nullptr;
    m_basePath.clear();
    m_path.clear();
    m_name.clear();
    m_shellPath.clear();
    m_shellPidl = nullptr;
    if (!item || !m_com) return false;

    // Bind the resolved Shell item to a child enumerator and return its first usable entry.
    const CComPtr<IShellItem2> shellItem = FinderMtpResolve(item->GetIndex());
    if (!shellItem || FAILED(shellItem->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&m_items)))) return false;
    m_basePath = item->GetPath();
    return FindNext();
}

bool FinderMtp::IsPath(std::wstring_view path) noexcept
{
    return path.size() >= MtpPrefix.size() &&
        _wcsnicmp(path.data(), MtpPrefix.data(), MtpPrefix.size()) == 0;
}

std::wstring FinderMtp::StripPrefix(std::wstring_view path)
{
    return std::wstring(IsPath(path) ? path.substr(MtpPrefix.size()) : path);
}

std::vector<MtpDevice> FinderMtp::GetDevices()
{
    ComApartmentScope com;
    if (!com) return {};

    // Refresh the portable device manager before taking the current WPD snapshot.
    CComPtr<IPortableDeviceManager> manager;
    if (FAILED(CoCreateInstance(CLSID_PortableDeviceManager, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&manager)))) return {};
    manager->RefreshDeviceList();

    // Gather WPD and Shell views so each device can be correlated across both APIs.
    const std::vector<FinderMtpWpdDevice> wpdDevices = FinderMtpGetWpdDevices(manager);
    std::vector<FinderMtpShellDevice> shellDevices = FinderMtpGetShellDevices();
    std::vector<MtpDevice> devices;
    devices.reserve((std::min)(wpdDevices.size(), shellDevices.size()));

    for (const FinderMtpWpdDevice& wpdDevice : wpdDevices)
    {
        // Match unused Shell entries by their normalized hardware identities first.
        auto match = std::ranges::find_if(shellDevices, [&](const FinderMtpShellDevice& shellDevice)
        {
            return !shellDevice.used && std::ranges::any_of(shellDevice.identities, [&](const std::wstring& identity)
            {
                return FinderMtpIdsMatch(wpdDevice.id, identity);
            });
        });
        if (match == shellDevices.end())
        {
            // Fall back to friendly names when the APIs expose no shared identifier.
            match = std::ranges::find_if(shellDevices, [&](const FinderMtpShellDevice& shellDevice)
            {
                return !shellDevice.used && FinderMtpNamesMatch(wpdDevice.name, shellDevice.name);
            });
        }
        if (match == shellDevices.end()) continue;

        // Consume the Shell entry once so duplicate friendly names cannot reuse it.
        match->used = true;
        devices.push_back({ FinderMtpPrefixedPath(match->path),
            match->name.empty() ? wpdDevice.name : match->name });
    }
    return devices;
}

bool FinderMtp::GetDriveInfo(const std::wstring& path, std::wstring& name,
    ULONGLONG& total, ULONGLONG& freeBytes)
{
    // Reset all outputs before attempting to resolve the requested device.
    name.clear();
    total = 0;
    freeBytes = 0;

    ComApartmentScope com;
    if (!com) return false;

    const CComPtr<IShellItem2> item = FinderMtpResolve(path);
    if (!item) return false;

    // Prefer capacity values exposed directly on the device's Shell item.
    name = FinderMtpItemName(item);
    FinderMtpAddSpace(item, total, freeBytes);
    if (total != 0) return true;

    // Fall back to summing storage children when the device itself omits capacity.
    total = 0;
    freeBytes = 0;
    CComPtr<IEnumShellItems> children;
    if (FAILED(item->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&children)))) return true;

    while (true)
    {
        CComPtr<IShellItem> child;
        ULONG fetched = 0;
        if (children->Next(1, &child, &fetched) != S_OK) break;
        const CComQIPtr<IShellItem2> properties(child);
        if (properties) FinderMtpAddSpace(properties, total, freeBytes);
    }
    return true;
}

std::wstring FinderMtp::GetDisplayName(const std::wstring& path)
{
    ComApartmentScope com;
    if (!com) return {};

    const CComPtr<IShellItem2> item = FinderMtpResolve(path);
    if (!item) return {};

    return FinderMtpItemName(item);
}

bool FinderMtp::DoesFileExist(const std::wstring& path)
{
    ComApartmentScope com;
    return com && FinderMtpResolve(path) != nullptr;
}

bool FinderMtp::DoesFileExist(const CItem* item)
{
    if (!item) return false;
    ComApartmentScope com;
    return com && FinderMtpResolve(item->GetIndex()) != nullptr;
}

ULONGLONG FinderMtp::RegisterPath(std::wstring path, std::wstring shellPath, PCIDLIST_ABSOLUTE shellPidl)
{
    if (path.empty() && shellPath.empty() && !shellPidl) return 0;

    // Normalize stored parsing paths so every registered MTP identity uses the same prefix.
    if (!path.empty()) path = FinderMtpPrefixedPath(path);
    if (!shellPath.empty()) shellPath = FinderMtpPrefixedPath(shellPath);

    // Allocate an unused high-bit token while holding exclusive access to the registry.
    const std::scoped_lock lock(s_pathMutex);
    ULONGLONG token = 0;
    do
    {
        token = s_nextPathToken++;
        s_nextPathToken |= PathTokenBit;
    }
    while (s_paths.contains(token));
    // Store a snapshot of every available identity under the generated token.
    s_paths.emplace(token, RegisteredPath(std::move(path), std::move(shellPath), shellPidl));
    return token;
}

void FinderMtp::UnregisterPath(ULONGLONG token) noexcept
{
    if ((token & PathTokenBit) == 0) return;
    const std::scoped_lock lock(s_pathMutex);
    s_paths.erase(token);
}

std::wstring FinderMtp::GetPath(ULONGLONG token)
{
    if ((token & PathTokenBit) == 0) return {};
    const std::scoped_lock lock(s_pathMutex);
    const auto found = s_paths.find(token);
    return found == s_paths.end() ? std::wstring() : found->second.path;
}

std::wstring FinderMtp::GetShellPath(ULONGLONG token)
{
    if ((token & PathTokenBit) == 0) return {};
    const std::scoped_lock lock(s_pathMutex);
    const auto found = s_paths.find(token);
    return found == s_paths.end() ? std::wstring() : found->second.shellPath;
}

PIDLIST_ABSOLUTE FinderMtp::CloneShellPidl(ULONGLONG token)
{
    if ((token & PathTokenBit) == 0) return nullptr;

    // Return a caller-owned PIDL clone without exposing the registered instance.
    const std::scoped_lock lock(s_pathMutex);
    const auto found = s_paths.find(token);
    return found == s_paths.end() || !found->second.shellPidl ?
        nullptr : ILCloneFull(found->second.shellPidl.Get());
}

bool FinderMtp::HasShellIdentity(ULONGLONG token)
{
    if ((token & PathTokenBit) == 0) return false;
    const std::scoped_lock lock(s_pathMutex);
    const auto found = s_paths.find(token);
    return found != s_paths.end() && (!found->second.shellPath.empty() || found->second.shellPidl);
}
