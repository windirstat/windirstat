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
#include "HelpersTasks.h"
#include "FinderBasic.h"

#pragma comment(lib,"powrprof.lib")
#pragma comment(lib,"wbemuuid.lib")
#pragma comment(lib,"virtdisk.lib")
#pragma comment(lib,"mpr.lib")

static NTSTATUS(NTAPI* NtSetInformationProcess)(HANDLE ProcessHandle, ULONG ProcessInformationClass,
    PVOID ProcessInformation, ULONG ProcessInformationLength) = reinterpret_cast<decltype(NtSetInformationProcess)>(
        reinterpret_cast<LPVOID>(GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtSetInformationProcess")));

static HRESULT WmiConnect(CComPtr<IWbemServices>& pSvc)
{
    if (thread_local SmartPointer<PVOID> comInit([](PVOID) { CoUninitialize(); }, nullptr);
        comInit == nullptr)
    {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(result) && result != RPC_E_CHANGED_MODE) return result;
        comInit = reinterpret_cast<PVOID>(TRUE);
    }

    CComPtr<IWbemLocator> locObj;
    return SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&locObj))) &&
        SUCCEEDED(locObj->ConnectServer(CComBSTR(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &pSvc)) &&
        SUCCEEDED(CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE)) ? S_OK : E_FAIL;
}

// WMI helpers
void QueryShadowCopies(ULONGLONG& count, ULONGLONG& bytesUsed)
{
    count = 0;
    bytesUsed = 0;
    CComPtr<IWbemServices> svcObj;
    CComPtr<IEnumWbemClassObject> enumObj;
    if (FAILED(WmiConnect(svcObj)) ||
        FAILED(svcObj->ExecQuery(CComBSTR(L"WQL"), CComBSTR(L"SELECT UsedSpace FROM Win32_ShadowStorage"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumObj))) return;

    // Sum up all used space
    while (true)
    {
        ULONG uRet;
        CComPtr<IWbemClassObject> pObj;
        if (enumObj->Next(WBEM_INFINITE, 1, &pObj, &uRet) != WBEM_S_NO_ERROR || uRet == 0) break;

        CComVariant usedStr, usedString;
        if (SUCCEEDED(pObj->Get(L"UsedSpace", 0, &usedStr, nullptr, nullptr)) &&
            SUCCEEDED(VariantChangeType(&usedString, &usedStr, 0, VT_UI8)))
            bytesUsed += usedString.ullVal;
    }

    // Count existing shadow copies
    enumObj.Release();
    if (SUCCEEDED(svcObj->ExecQuery(CComBSTR(L"WQL"), CComBSTR(L"SELECT ID FROM Win32_ShadowCopy"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumObj)))
    {
        for (ULONG ret = 0;; ++count)
        {
            CComPtr<IWbemClassObject> pObj;
            if (enumObj->Next(WBEM_INFINITE, 1, &pObj, &ret) != WBEM_S_NO_ERROR || ret == 0) break;
        }
    }
}

void RemoveWmiInstances(const std::wstring& wmiClass, CProgressDlg* pdlg, const std::wstring& whereClause)
{
    CComPtr<IWbemServices> svcObj;
    CComPtr<IWbemClassObject> classObj;
    CComPtr<IEnumWbemClassObject> enumObj;
    if (FAILED(WmiConnect(svcObj)) || !svcObj ||
        FAILED(svcObj->GetObject(CComBSTR(wmiClass.c_str()), 0, nullptr, &classObj, nullptr)) || !classObj ||
        FAILED(svcObj->ExecQuery(CComBSTR(L"WQL"), CComBSTR(std::format(L"SELECT __PATH FROM {} WHERE {}", wmiClass, whereClause).c_str()),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumObj)))
    {
        return;
    }

    for (; !pdlg->IsCancelled(); pdlg->Increment())
    {
        ULONG uRet;
        CComPtr<IWbemClassObject> pObj;
        if (enumObj->Next(WBEM_INFINITE, 1, &pObj, &uRet) != WBEM_S_NO_ERROR || uRet == 0) break;

        CComVariant vtPath;
        if (SUCCEEDED(pObj->Get(L"__PATH", 0, &vtPath, nullptr, nullptr)) && vtPath.vt == VT_BSTR)
        {
            svcObj->DeleteInstance(vtPath.bstrVal, 0, nullptr, nullptr);
        }
    }
}

// File system helpers
bool FolderExists(const std::wstring& path) noexcept
{
    const DWORD result = GetFileAttributes(FinderBasic::MakeLongPathCompatible(path).c_str());
    return result != INVALID_FILE_ATTRIBUTES && (result & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool DriveExists(const std::wstring& path) noexcept
{
    if (path.size() != 3 || path[1] != wds::chrColon || path[2] != wds::chrBackslash)
    {
        return false;
    }

    const int d = std::toupper(path.at(0)) - wds::strAlpha.at(0);
    if (const DWORD mask = 0x1 << d; (mask & GetLogicalDrives()) == 0)
    {
        return false;
    }

    if (std::wstring dummy; !GetVolumeName(path, dummy))
    {
        return false;
    }

    return true;
}

bool IsLocalDrive(const std::wstring& path) noexcept
{
    if (path.size() < 3 || path[1] != wds::chrColon || path[2] != wds::chrBackslash)
    {
        return false;
    }

    const auto driveType = GetDriveType(path.substr(0, 2).c_str());
    return driveType == DRIVE_REMOVABLE || driveType == DRIVE_FIXED;
}

bool GetVolumeName(const std::wstring& rootPath, std::wstring& volumeName)
{
    volumeName.resize(MAX_PATH);
    const bool success = GetVolumeInformation(rootPath.c_str(), volumeName.data(),
        static_cast<DWORD>(volumeName.size()), nullptr, nullptr, nullptr, nullptr, 0) != FALSE;
    volumeName.resize(success ? wcslen(volumeName.data()) : 0);
    if (!success) VTRACE(L"GetVolumeInformation({}) failed: {}", rootPath.c_str(), ::GetLastError());
    return success;
}

// Path utilities
std::wstring WdsQueryDosDevice(const std::wstring& drive)
{
    if (drive.size() < 2 || drive[1] != wds::chrColon) return {};

    std::array<WCHAR, 512> info;
    if (::QueryDosDevice(drive.substr(0, 2).c_str(), info.data(), std::ssize(info)) == 0)
    {
        VTRACE(L"QueryDosDevice({}) Failed: {}", drive.substr(0, 2), TranslateError());
        return {};
    }

    return info.data();
}

bool IsSUBSTedDrive(const std::wstring& drive)
{
    const std::wstring info = WdsQueryDosDevice(drive);
    return info.starts_with(L"\\??\\");
}

// Hibernation
void DisableHibernate() noexcept
{
    BOOLEAN hibernateEnabled = FALSE;
    (void)CallNtPowerInformation(SystemReserveHiberFile, &hibernateEnabled,
        sizeof(hibernateEnabled), nullptr, 0);

    // Delete file in the event that the above call does not actually delete the file as
    // designed or hibernate was previously disabled in a way that did not delete the file
    if (WCHAR drive[3]; GetEnvironmentVariable(L"SystemDrive", drive, std::size(drive)) == std::size(drive) - 1)
    {
        DeleteFile((drive + std::wstring(L"\\hiberfil.sys")).c_str());
    }
}

bool IsHibernateEnabled() noexcept
{
    WCHAR drive[3];
    return GetEnvironmentVariable(L"SystemDrive", drive, std::size(drive)) == std::size(drive) - 1 &&
        FinderBasic::DoesFileExist(drive + std::wstring(L"\\"), L"hiberfil.sys");
}

// Elevation and privileges
bool IsElevationActive() noexcept
{
    SmartPointer<HANDLE> token(CloseHandle);
    TOKEN_ELEVATION elevation;
    DWORD size = sizeof(TOKEN_ELEVATION);
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) == 0 ||
        GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) == 0)
    {
        return false;
    }

    return elevation.TokenIsElevated != 0;
}

bool IsElevationAvailable() noexcept
{
    if (IsElevationActive()) return false;

    SmartPointer<HANDLE> token(CloseHandle);
    TOKEN_ELEVATION_TYPE elevationType;
    DWORD size = sizeof(TOKEN_ELEVATION_TYPE);
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) == 0 ||
        GetTokenInformation(token, TokenElevationType, &elevationType, sizeof(elevationType), &size) == 0)
    {
        return false;
    }

    return elevationType == TokenElevationTypeLimited;
}

void RunElevated(const std::wstring& cmdLine)
{
    PersistedSetting::WritePersistedProperties();
    if (ShellExecuteWrapper(GetAppFileName(), cmdLine, L"runas"))
        ExitProcess(0);
}

bool EnableReadPrivileges() noexcept
{
    // Open a connection to the currently running process token and request
    // we have the ability to look at and adjust our privileges
    SmartPointer<HANDLE> token(CloseHandle);
    if (OpenProcessToken(GetCurrentProcess(),
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token) == 0)
    {
        return false;
    }

    // Fetch a list of privileges we currently have
    std::vector<BYTE> privsBytes(64 * sizeof(LUID_AND_ATTRIBUTES) + sizeof(DWORD), 0);
    const PTOKEN_PRIVILEGES privsAvailable = reinterpret_cast<PTOKEN_PRIVILEGES>(privsBytes.data());
    DWORD privLength = 0;
    if (GetTokenInformation(token, TokenPrivileges, privsBytes.data(),
        static_cast<DWORD>(privsBytes.size()), &privLength) == 0)
    {
        return false;
    }

    bool ret = true;
    for (const auto priv : { SE_RESTORE_NAME, SE_BACKUP_NAME })
    {
        // Populate the privilege adjustment structure
        TOKEN_PRIVILEGES privEntry = {};
        privEntry.PrivilegeCount = 1;
        privEntry.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        // Translate the privilege name into the binary representation
        if (LookupPrivilegeValue(nullptr, priv, &privEntry.Privileges[0].Luid) == 0)
        {
            ret = false;
            continue;
        }

        // Check if privilege is in the list of ones we have
        if (std::count_if(privsAvailable[0].Privileges, &privsAvailable->Privileges[privsAvailable->PrivilegeCount],
            [&](const LUID_AND_ATTRIBUTES& element) {
                return element.Luid.HighPart == privEntry.Privileges[0].Luid.HighPart &&
                    element.Luid.LowPart == privEntry.Privileges[0].Luid.LowPart; }) == 0)
        {
            ret = false;
            continue;
        }

        // Adjust the process to change the privilege
        if (AdjustTokenPrivileges(token, FALSE, &privEntry,
            sizeof(TOKEN_PRIVILEGES), nullptr, nullptr) == 0)
        {
            ret = false;
            break;
        }

        // Error if not all items were assigned
        if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
        {
            ret = false;
            break;
        }
    }

    return ret;
}

// SID helpers
static constexpr DWORD SidGetLength(const PSID x)
{
    return sizeof(SID) + (static_cast<SID*>(x)->SubAuthorityCount - 1) * sizeof(static_cast<SID*>(x)->SubAuthority);
}

std::wstring GetNameFromSid(const PSID sid)
{
    // return immediately if sid is null or invalid
    if (sid == nullptr || !IsValidSid(sid)) return {};

    // attempt to lookup sid in cache
    const std::vector sidVec(ByteOffset<BYTE>(sid, 0), ByteOffset<BYTE>(sid, SidGetLength(sid)));
    static std::map<std::vector<BYTE>, std::wstring> nameMap;
    if (const auto iter = nameMap.find(sidVec); iter != nameMap.end())
    {
        return iter->second;
    }

    // lookup the name for this sid
    SID_NAME_USE nameUse;
    std::array<WCHAR, UNLEN + 1> accountName;
    std::array<WCHAR, DNLEN + 1> domainName;
    DWORD iAccountNameSize = static_cast<DWORD>(accountName.size());
    DWORD iDomainName = static_cast<DWORD>(domainName.size());
    std::wstring result;
    if (LookupAccountSid(nullptr, sid, accountName.data(),
        &iAccountNameSize, domainName.data(), &iDomainName, &nameUse) != 0)
    {
        // generate full name in domain\name format
        return nameMap.try_emplace(sidVec, std::format(L"{}\\{}",
            domainName.data(), accountName.data())).first->second;
    }

    // fallback: return sid string
    SmartPointer<LPWSTR> sidBuff(LocalFree);
    ConvertSidToStringSid(sid, &sidBuff);
    return nameMap.try_emplace(sidVec, sidBuff).first->second;
}

// Compression
bool CompressFileAllowed(const std::wstring& volumeName, const CompressionAlgorithm algorithm)
{
    static std::unordered_map<std::wstring, bool> compressionStandard;
    static std::unordered_map<std::wstring, bool> compressionModern;
    const auto& compressionMap = (algorithm == CompressionAlgorithm::LZNT1) ?
        compressionStandard : compressionModern;

    // Enable 'none' button if at least standard is available
    if (algorithm == CompressionAlgorithm::NONE)
    {
        return CompressFileAllowed(volumeName, CompressionAlgorithm::LZNT1) ||
            CompressFileAllowed(volumeName, CompressionAlgorithm::XPRESS4K);
    }

    // Return cached value
    if (compressionMap.contains(volumeName))
    {
        return compressionMap.at(volumeName);
    }

    // Query volume for standard compression support based on whether NTFS
    std::array<WCHAR, MAX_PATH + 1> fileSystemName{};
    DWORD fileSystemFlags = 0;
    const bool isNTFS = GetVolumeInformation(volumeName.c_str(), nullptr, 0, nullptr, nullptr,
        &fileSystemFlags, fileSystemName.data(), std::ssize(fileSystemName)) != 0 &&
        std::wstring(fileSystemName.data()) == L"NTFS";

    // Query volume for modern compression support based on NTFS and OS version
    compressionStandard[volumeName.data()] = isNTFS && (fileSystemFlags & FILE_FILE_COMPRESSION) != 0;
    compressionModern[volumeName.data()] = isNTFS && IsWindows10OrGreater() && !volumeName.starts_with(L"\\\\");

    return compressionMap.at(volumeName);
}

bool CompressFile(const std::wstring& filePath, const CompressionAlgorithm algorithm)
{
    USHORT numericAlgorithm = static_cast<USHORT>(algorithm) & ~FILE_PROVIDER_COMPRESSION_MODERN;
    const bool modernAlgorithm = static_cast<USHORT>(algorithm) != numericAlgorithm;

    SmartPointer<HANDLE> handle(CloseHandle, CreateFile(filePath.c_str(),
        GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (handle == INVALID_HANDLE_VALUE)
    {
        VTRACE(L"CreateFile() Error: {:#08X}", GetLastError());
        return false;
    }

    DWORD status = 0;
    if (modernAlgorithm)
    {
        struct
        {
            WOF_EXTERNAL_INFO wof_info;
            FILE_PROVIDER_EXTERNAL_INFO_V1 file_info;
        }
        info =
        {
            .wof_info = {
                .Version = WOF_CURRENT_VERSION,
                .Provider = WOF_PROVIDER_FILE,
            },
            .file_info = {
                .Version = FILE_PROVIDER_CURRENT_VERSION,
                .Algorithm = numericAlgorithm,
            },
        };

        DWORD bytesReturned;
        status = DeviceIoControl(handle, FSCTL_SET_EXTERNAL_BACKING,
            &info, sizeof(info), nullptr, 0, &bytesReturned, nullptr);
    }
    else if (numericAlgorithm == COMPRESSION_FORMAT_LZNT1)
    {
        DWORD bytesReturned = 0;
        status = DeviceIoControl(
            handle, FSCTL_SET_COMPRESSION, &numericAlgorithm,
            sizeof(numericAlgorithm), nullptr, 0, &bytesReturned, nullptr);
    }
    else
    {
        DWORD bytesReturned = 0;
        DeviceIoControl(
            handle, FSCTL_SET_COMPRESSION, &numericAlgorithm,
            sizeof(numericAlgorithm), nullptr, 0, &bytesReturned, nullptr);

        DeviceIoControl(
            handle, FSCTL_DELETE_EXTERNAL_BACKING, nullptr,
            0, nullptr, 0, nullptr, nullptr);
    }

    return status == 1 || status == 0xC000046F;
}

// File hashing
std::wstring ComputeFileHashes(const std::wstring& filePath)
{
    // Open file with smart pointer
    SmartPointer<HANDLE> hFile(CloseHandle, CreateFile(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return TranslateError();
    }

    // Initialize all hash contexts
    using HashContext = struct HashContext {
        LPCWSTR name = nullptr;
        DWORD objectLen = 0;
        std::vector<BYTE> hashObject;
        std::vector<BYTE> hash;
        SmartPointer<BCRYPT_ALG_HANDLE> hAlg = { nullptr, nullptr };
        SmartPointer<BCRYPT_HASH_HANDLE> hHash = { nullptr, nullptr };
    };

    // Define algorithms to compute
    using AlgSet = struct { LPCWSTR id; LPCWSTR name; DWORD hashLen; };
    constexpr std::array algos = {
        AlgSet{BCRYPT_MD5_ALGORITHM, L"MD5", 16},
        AlgSet{BCRYPT_SHA1_ALGORITHM, L"SHA1", 20},
        AlgSet{BCRYPT_SHA256_ALGORITHM, L"SHA256", 32},
        AlgSet{BCRYPT_SHA384_ALGORITHM, L"SHA384", 48},
        AlgSet{BCRYPT_SHA512_ALGORITHM, L"SHA512", 64}
    };

    // Setup all algorithms
    std::vector<HashContext> contexts;
    for (const auto& [id, name, hashLen] : algos)
    {
        HashContext ctx;
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        if (BCryptOpenAlgorithmProvider(&hAlg, id, nullptr, 0) != 0) continue;
        ctx.hAlg = SmartPointer<BCRYPT_ALG_HANDLE>(
            [](const BCRYPT_ALG_HANDLE h) { (void) BCryptCloseAlgorithmProvider(h, 0); }, hAlg);

        if (DWORD bytesWritten = 0; BCryptGetProperty(ctx.hAlg, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PBYTE>(&ctx.objectLen), sizeof(DWORD), &bytesWritten, 0) != ERROR_SUCCESS)
        {
            continue;
        }

        ctx.name = name;
        ctx.hashObject.resize(ctx.objectLen);
        ctx.hash.resize(hashLen);

        BCRYPT_HASH_HANDLE hHash = nullptr;
        if (BCryptCreateHash(ctx.hAlg, &hHash, ctx.hashObject.data(),
            ctx.objectLen, nullptr, 0, 0) != 0) continue;

        ctx.hHash = SmartPointer<BCRYPT_HASH_HANDLE>(BCryptDestroyHash, hHash);

        contexts.emplace_back(std::move(ctx));
    }

    // Read file and update all hashes
    constexpr size_t BUFFER_SIZE = 1024ull * 1024ull; // 1MB chunks
    std::vector<BYTE> buffer(BUFFER_SIZE);
    DWORD bytesRead;

    // Update all valid hashes with the same buffer in parallel
    while (ReadFile(hFile, buffer.data(), BUFFER_SIZE, &bytesRead, nullptr) && bytesRead > 0)
    {
        std::for_each(std::execution::par, contexts.begin(), contexts.end(),
            [&buffer, bytesRead](auto& ctx) {
                (void)BCryptHashData(ctx.hHash, buffer.data(), bytesRead, 0);
            });
    }

    // Finalize all hashes and convert to hex strings
    std::wstring result = filePath + L"\n\n";
    for (auto& ctx : contexts)
    {
        if (BCryptFinishHash(ctx.hHash, ctx.hash.data(),
            static_cast<ULONG>(ctx.hash.size()), 0) != 0) continue;

        // Add to result
        result += std::format(L"{:\u2007<7}\t{}\n",
            std::wstring(ctx.name) + L':', FormatHex(ctx.hash));
    }
    if (!result.empty() && result.back() == L'\n') result.pop_back();

    return result;
}

// I/O priority and VHD optimization
void SetProcessIoPriorityHigh() noexcept
{
    // Define I/O priority constants
    constexpr ULONG ProcessIoPriority = 33;
    constexpr ULONG IoPriorityHigh = 3;

    // Set the I/O priority to high for the current process
    ULONG ioPriority = IoPriorityHigh;
    if (NtSetInformationProcess(GetCurrentProcess(),
        ProcessIoPriority, &ioPriority, sizeof(ioPriority)) != 0)
    {
        VTRACE(L"NtSetInformationProcess() Failed");
    }
}

bool OptimizeVhd(const std::wstring& vhdPath) noexcept
{
    constexpr VIRTUAL_DISK_ACCESS_MASK accessMask = VIRTUAL_DISK_ACCESS_ALL;
    OPEN_VIRTUAL_DISK_PARAMETERS openParams{};
    openParams.Version = OPEN_VIRTUAL_DISK_VERSION_1;

    VIRTUAL_STORAGE_TYPE storageType{
        .DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN,
        .VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT };

    SmartPointer<HANDLE> vhdHandle(CloseHandle, nullptr);
    if (OpenVirtualDisk(&storageType, vhdPath.c_str(),
        accessMask, OPEN_VIRTUAL_DISK_FLAG_NONE,
        &openParams, &vhdHandle) != ERROR_SUCCESS) return false;

    COMPACT_VIRTUAL_DISK_PARAMETERS compactParams{};
    compactParams.Version = COMPACT_VIRTUAL_DISK_VERSION_1;

    return CompactVirtualDisk(vhdHandle, COMPACT_VIRTUAL_DISK_FLAG_NONE,
        &compactParams, nullptr) == ERROR_SUCCESS;
}

// Drive mappings
void CopyAllDriveMappings() noexcept
{
    if (!IsElevationActive() || !COptions::AutoMapDrivesWhenElevated) return;

    // Map the registry key for network drives
    CRegKey keyNetwork;
    if (keyNetwork.Open(HKEY_CURRENT_USER, L"Network", KEY_READ) != ERROR_SUCCESS) return;

    // Enumerate all subkeys (each subkey is a drive letter)
    std::unordered_map<std::wstring, std::wstring> mappings;
    std::array<WCHAR, MAX_PATH + 1> driveLetter;
    ULONG driveLetterSize = static_cast<ULONG>(driveLetter.size());
    for (DWORD index = 0; keyNetwork.EnumKey(index, driveLetter.data(), &driveLetterSize) == ERROR_SUCCESS;
        ++index, driveLetterSize = static_cast<ULONG>(driveLetter.size()))
    {
        // Get the drive letter and remove path
        CRegKey keyDrive;
        std::array<WCHAR, MAX_PATH + 1> remotePath{};
        ULONG remotePathSize = static_cast<ULONG>(remotePath.size());       
        if (keyDrive.Open(keyNetwork, driveLetter.data(), KEY_READ) == ERROR_SUCCESS &&
            keyDrive.QueryStringValue(L"RemotePath", remotePath.data(), &remotePathSize) == ERROR_SUCCESS)
        {
            auto withColon = driveLetter.data() + std::wstring(L":");
            if (DriveExists(withColon)) continue;
            mappings[withColon] = remotePath.data();
        }
    }

    if (!mappings.empty()) CProgressDlg(driveLetter.size(), true, AfxGetMainWnd(), [&](CProgressDlg* pdlg)
    {
        for (const auto & mapping : mappings)
        {
            // Attempt to map the drive
            auto& [drivePath, remotePath] = mapping;
            NETRESOURCEW mapProperties{
                .dwType = RESOURCETYPE_DISK,
                .lpLocalName = const_cast<LPWSTR>(drivePath.data()),
                .lpRemoteName = const_cast<LPWSTR>(remotePath.data())
            };

            (void)WNetAddConnection2(&mapProperties, nullptr, nullptr, 0);
            pdlg->Increment();
        }
    }).DoModal();
}
