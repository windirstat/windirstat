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

// This file provides implementations of certain Windows API functions that are
// only available on Windows 8 and later. The implementations here are compatible
// with Windows 7 and earlier, allowing the application to run on those versions

#define CreateFile2 CreateFile2_SdkDeclarationOnly
#define GetSystemTimePreciseAsFileTime GetSystemTimePreciseAsFileTime_SdkDeclarationOnly
#define WaitOnAddress WaitOnAddress_SdkDeclarationOnly
#define WakeByAddressSingle WakeByAddressSingle_SdkDeclarationOnly
#define WakeByAddressAll WakeByAddressAll_SdkDeclarationOnly
#include <windows.h>
#undef CreateFile2
#undef GetSystemTimePreciseAsFileTime
#undef WaitOnAddress
#undef WakeByAddressSingle
#undef WakeByAddressAll

#if _WIN32_WINNT < 0x0602
typedef struct _CREATEFILE2_EXTENDED_PARAMETERS {
    DWORD dwSize;
    DWORD dwFileAttributes;
    DWORD dwFileFlags;
    DWORD dwSecurityQosFlags;
    LPSECURITY_ATTRIBUTES lpSecurityAttributes;
    HANDLE hTemplateFile;
} *LPCREATEFILE2_EXTENDED_PARAMETERS;
#endif

template<auto Function>
static decltype(Function) GetNativeFunction(LPCWSTR moduleName, LPCSTR name)
{
    const DWORD savedError = GetLastError();
    static const HMODULE module = GetModuleHandleW(moduleName);
    static const auto native = module ? reinterpret_cast<decltype(Function)>(GetProcAddress(module, name)) : nullptr;
    SetLastError(savedError);
    return native;
}

EXTERN_C HANDLE WINAPI CreateFile2(LPCWSTR name, DWORD access, DWORD sharing,
    DWORD disposition, LPCREATEFILE2_EXTENDED_PARAMETERS p)
{
    static const auto native = GetNativeFunction<&CreateFile2>(L"kernel32.dll", "CreateFile2");
    if (native) return native(name, access, sharing, disposition, p);

    if (p && p->dwSize != sizeof(*p))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    // Only file flags that can be translated to the legacy API.
    constexpr DWORD supportedFlags =
        FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING |
        FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_DELETE_ON_CLOSE |
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_POSIX_SEMANTICS |
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_OPEN_NO_RECALL;
    if (p && (p->dwFileFlags & ~supportedFlags))
    {
        SetLastError(ERROR_NOT_SUPPORTED);
        return INVALID_HANDLE_VALUE;
    }

    DWORD flags = p ? p->dwFileAttributes | p->dwFileFlags : FILE_ATTRIBUTE_NORMAL;
    if (p && p->dwSecurityQosFlags) flags |= SECURITY_SQOS_PRESENT | p->dwSecurityQosFlags;

    return CreateFileW(name, access, sharing,
        p ? p->lpSecurityAttributes : nullptr, disposition,
        flags, p ? p->hTemplateFile : nullptr);
}

EXTERN_C VOID WINAPI GetSystemTimePreciseAsFileTime(LPFILETIME time)
{
    static const auto native = GetNativeFunction<&GetSystemTimePreciseAsFileTime>(
        L"kernel32.dll", "GetSystemTimePreciseAsFileTime");
    if (native) native(time);
    else GetSystemTimeAsFileTime(time);
}

EXTERN_C BOOL WINAPI WaitOnAddress(volatile VOID* Address, PVOID CompareAddress,
    SIZE_T AddressSize, DWORD dwMilliseconds)
{
    static const auto native = GetNativeFunction<&WaitOnAddress>(L"kernelbase.dll", "WaitOnAddress");
    if (native) return native(Address, CompareAddress, AddressSize, dwMilliseconds);

    if (!Address || !CompareAddress ||
        (AddressSize != 1 && AddressSize != 2 && AddressSize != 4 && AddressSize != 8))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    const ULONGLONG start = GetTickCount64();
    while (memcmp((const void*)Address, CompareAddress, AddressSize) == 0)
    {
        if (dwMilliseconds != INFINITE)
        {
            const ULONGLONG elapsed = GetTickCount64() - start;
            if (elapsed >= dwMilliseconds)
            {
                SetLastError(ERROR_TIMEOUT);
                return FALSE;
            }
            const DWORD remaining = dwMilliseconds - static_cast<DWORD>(elapsed);
            Sleep(remaining > 10 ? 10 : remaining);
        }
        else Sleep(1);
    }

    return TRUE;
}

EXTERN_C VOID WINAPI WakeByAddressSingle(PVOID Address)
{
    static const auto native = GetNativeFunction<&WakeByAddressSingle>(L"kernelbase.dll", "WakeByAddressSingle");
    if (native) native(Address);
}

EXTERN_C VOID WINAPI WakeByAddressAll(PVOID Address)
{
    static const auto native = GetNativeFunction<&WakeByAddressAll>(L"kernelbase.dll", "WakeByAddressAll");
    if (native) native(Address);
}

EXTERN_C{
#if defined(_M_IX86)
    constinit decltype(&CreateFile2) __identifier("_imp__CreateFile2@20") = &CreateFile2;
    constinit decltype(&GetSystemTimePreciseAsFileTime) __identifier("_imp__GetSystemTimePreciseAsFileTime@4") = &GetSystemTimePreciseAsFileTime;
    constinit decltype(&WaitOnAddress) __identifier("_imp__WaitOnAddress@16") = &WaitOnAddress;
    constinit decltype(&WakeByAddressSingle) __identifier("_imp__WakeByAddressSingle@4") = &WakeByAddressSingle;
    constinit decltype(&WakeByAddressAll) __identifier("_imp__WakeByAddressAll@4") = &WakeByAddressAll;
#elif defined(_M_X64)
    constinit decltype(&CreateFile2) __imp_CreateFile2 = &CreateFile2;
    constinit decltype(&GetSystemTimePreciseAsFileTime) __imp_GetSystemTimePreciseAsFileTime = &GetSystemTimePreciseAsFileTime;
    constinit decltype(&WaitOnAddress) __imp_WaitOnAddress = &WaitOnAddress;
    constinit decltype(&WakeByAddressSingle) __imp_WakeByAddressSingle = &WakeByAddressSingle;
    constinit decltype(&WakeByAddressAll) __imp_WakeByAddressAll = &WakeByAddressAll;
#endif
}
