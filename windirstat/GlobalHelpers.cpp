// GlobalHelpers.cpp - Implementation of global helper functions
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
// Copyright (C) 2010 Chris Wimmer
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
#include <common/MdExceptions.h>
#include <common/SmartPointer.h>
#include "GlobalHelpers.h"
#include "Options.h"
#include "Localization.h"
#include "FileFind.h"

#include <array>
#include <algorithm>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

std::wstring FormatLongLongNormal(ULONGLONG n)
{
    // Returns formatted number like "123.456.789".

    ASSERT(n >= 0);

    std::wstring all;

    do
    {
        const auto rest = n % 1000;
        n /= 1000;

        all.insert(0, (n <= 0) ? std::to_wstring(rest) :
            std::format(L"{}{:03}", GetLocaleThousandSeparator(), rest));
    } while (n > 0);

    return all;
}

std::wstring GetLocaleString(const LCTYPE lctype, const LANGID langid)
{
    const LCID lcid = MAKELCID(langid, SORT_DEFAULT);
    const int len = ::GetLocaleInfo(lcid, lctype, nullptr, 0);

    std::wstring s;
    s.resize(len);
    ::GetLocaleInfo(lcid, lctype, s.data(), len);
    s.resize(wcslen(s.data()));

    return s;
}

std::wstring GetLocaleLanguage(const LANGID langid)
{
    const std::wstring s = GetLocaleString(LOCALE_SLOCALIZEDLANGUAGENAME, langid);
    const std::wstring n = GetLocaleString(LOCALE_SNATIVELANGNAME, langid);
    return s + L" (" + n + L")";
}

std::wstring GetLocaleThousandSeparator()
{
    static LANGID cachedLang = static_cast<LANGID>(-1);
    static std::wstring cachedString;
    if (cachedLang != COptions::GetEffectiveLangId())
    {
        cachedLang = COptions::GetEffectiveLangId();
        cachedString = GetLocaleString(LOCALE_STHOUSAND, cachedLang);
    }
    return cachedString;
}

std::wstring GetLocaleDecimalSeparator()
{
    static LANGID cachedLang = static_cast<LANGID>(-1);
    static std::wstring cachedString;
    if (cachedLang != COptions::GetEffectiveLangId())
    {
        cachedLang = COptions::GetEffectiveLangId();
        cachedString = GetLocaleString(LOCALE_SDECIMAL, cachedLang);
    }
    return cachedString;
}

std::wstring FormatBytes(const ULONGLONG& n)
{
    if (COptions::UseSizeSuffixes)
    {
        return FormatSizeSuffixes(n);
    }

    return FormatLongLongNormal(n);

}

std::wstring FormatSizeSuffixes(ULONGLONG n)
{
    // Returns formatted number like "12,4 GB".
    ASSERT(n >= 0);
    constexpr int base = 1024;
    constexpr int half = base / 2;

    const double B = static_cast<int>(n % base);
    n /= base;

    const double KB = static_cast<int>(n % base);
    n /= base;

    const double MB = static_cast<int>(n % base);
    n /= base;

    const double GB = static_cast<int>(n % base);
    n /= base;

    const double TB = static_cast<int>(n);

    if (TB != 0.0 || GB == base - 1 && MB >= half)
    {
        return FormatDouble(TB + GB / base) + L" " + GetSpec_TB();
    }
    if (GB != 0.0 || MB == base - 1 && KB >= half)
    {
        return FormatDouble(GB + MB / base) + L" " + GetSpec_GB();
    }
    if (MB != 0.0 || KB == base - 1 && B >= half)
    {
        return FormatDouble(MB + KB / base) + L" " + GetSpec_MB();
    }
    if (KB != 0.0)
    {
        return FormatDouble(KB + B / base) + L" " + GetSpec_KB();
    }
    if (B != 0.0)
    {
        return std::to_wstring(static_cast<ULONG>(B)) + L" " + GetSpec_Bytes();
    }

    return L"0";
}

std::wstring FormatCount(const ULONGLONG& n)
{
    return FormatLongLongNormal(n);
}

std::wstring FormatDouble(double d)
{
    ASSERT(d >= 0);

    d += 0.05;

    const int i = static_cast<int>(floor(d));
    const int r = static_cast<int>(10 * fmod(d, 1));

    return std::to_wstring(i) + GetLocaleDecimalSeparator() + std::to_wstring(r);
}

std::wstring PadWidthBlanks(std::wstring n, const int width)
{
    const auto blankCount = width - n.size();
    if (blankCount <= 0) return n;
    return n + std::wstring(blankCount, wds::chrBlankSpace);
}

std::wstring FormatFileTime(const FILETIME& t)
{
    SYSTEMTIME st;
    if (FILETIME ft;
        ::FileTimeToLocalFileTime(&t, &ft) == 0 ||
        ::FileTimeToSystemTime(&ft, &st) == 0)
    {
        return L"";
    }

    const LCID lcid = MAKELCID(COptions::LanguageId.Obj(), SORT_DEFAULT);

    std::array<WCHAR, 64> date;
    VERIFY(0 < ::GetDateFormat(lcid, DATE_SHORTDATE, &st, nullptr, date.data(), static_cast<int>(date.size())));

    std::array<WCHAR, 64> time;
    VERIFY(0 < GetTimeFormat(lcid, TIME_NOSECONDS, &st, nullptr, time.data(), static_cast<int>(time.size())));
 
    return date.data() + std::wstring(L"  ") + time.data();
}

std::wstring FormatAttributes(const DWORD attr)
{
    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        return wds::strInvalidAttributes;
    }

    std::wstring attributes;
    if (attr & FILE_ATTRIBUTE_READONLY)
    {
        attributes.append(wds::strAttributeReadonly);
    }

    if (attr & FILE_ATTRIBUTE_HIDDEN)
    {
        attributes.append(wds::strAttributeHidden);
    }

    if (attr & FILE_ATTRIBUTE_SYSTEM)
    {
        attributes.append(wds::strAttributeSystem);
    }

    if (attr & FILE_ATTRIBUTE_ARCHIVE)
    {
        attributes.append(wds::strAttributeArchive);
    }

    if (attr & FILE_ATTRIBUTE_COMPRESSED)
    {
        attributes.append(wds::strAttributeCompressed);
    }

    if (attr & FILE_ATTRIBUTE_ENCRYPTED)
    {
        attributes.append(wds::strAttributeEncrypted);
    }

    if (attr & FILE_ATTRIBUTE_OFFLINE)
    {
        attributes.append(wds::strAttributeOffline);
    }

    return attributes;
}

std::wstring FormatMilliseconds(const ULONGLONG ms)
{
    const ULONGLONG sec = (ms + 500) / 1000;

    const ULONGLONG s   = sec % 60;
    const ULONGLONG min = sec / 60;

    const ULONGLONG m = min % 60;
    const ULONGLONG h = min / 60;

    return (h <= 0) ? std::format(L"{}:{:02}", m, s) :
        std::format(L"{}:{:02}:{:02}", h, m, s);
}

bool GetVolumeName(const std::wstring & rootPath, std::wstring& volumeName)
{
    volumeName.resize(MAX_PATH);
    const bool success = GetVolumeInformation(rootPath.c_str(), volumeName.data(),
        static_cast<DWORD>(volumeName.size()), nullptr, nullptr, nullptr, nullptr, 0) != FALSE;
    volumeName.resize(wcslen(volumeName.data()));

    if (!success)
    {
        VTRACE(L"GetVolumeInformation({}) failed: {}", rootPath.c_str(), ::GetLastError());
    }

    return success;
}

// Given a root path like "C:\", this function
// obtains the volume name and returns a complete display string
// like "BOOT (C:)".
std::wstring FormatVolumeNameOfRootPath(const std::wstring& rootPath)
{
    std::wstring ret;
    std::wstring volumeName;
    if (GetVolumeName(rootPath, volumeName))
    {
        ret = FormatVolumeName(rootPath, volumeName);
    }
    else
    {
        ret = rootPath;
    }
    return ret;
}

std::wstring FormatVolumeName(const std::wstring& rootPath, const std::wstring& volumeName)
{
    return volumeName + L" (" + rootPath.substr(0, 2) + L")";
}

// The inverse of FormatVolumeNameOfRootPath().
// Given a name like "BOOT (C:)", it returns "C:" (without trailing backslash).
// Or, if name like "C:\", it returns "C:".
std::wstring PathFromVolumeName(const std::wstring& name)
{
    const auto i = name.find_last_of(wds::chrBracketClose);
    if (i == std::wstring::npos)
    {
        ASSERT(name.size() == 3);
        return name.substr(0, 2);
    }

    ASSERT(i != std::wstring::npos);
    const auto k = name.find_last_of(wds::chrBracketOpen);
    ASSERT(k != std::wstring::npos);
    ASSERT(k < i);
    std::wstring path = name.substr(k + 1, i - k - 1);
    ASSERT(path.size() == 2);
    ASSERT(path[1] == wds::chrColon);

    return path;
}

// Retrieve the "fully qualified parse name" of "My Computer"
std::wstring GetParseNameOfMyComputer()
{
    CComPtr<IShellFolder> sf;
    HRESULT hr = ::SHGetDesktopFolder(&sf);
    MdThrowFailed(hr, L"::SHGetDesktopFolder");

    SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree);
    hr = ::SHGetSpecialFolderLocation(nullptr, CSIDL_DRIVES, &pidl);
    MdThrowFailed(hr, L"SHGetSpecialFolderLocation(CSIDL_DRIVES)");

    STRRET name;
    ZeroMemory(&name, sizeof(name));
    name.uType = STRRET_WSTR;
    hr = sf->GetDisplayNameOf(pidl, SHGDN_FORPARSING, &name);
    MdThrowFailed(hr, L"GetDisplayNameOf(My Computer)");
    return name.pOleStr;
}

void GetPidlOfMyComputer(LPITEMIDLIST* ppidl)
{
    CComPtr<IShellFolder> sf;
    HRESULT hr = ::SHGetDesktopFolder(&sf);
    MdThrowFailed(hr, L"SHGetDesktopFolder");

    hr = ::SHGetSpecialFolderLocation(nullptr, CSIDL_DRIVES, ppidl);
    MdThrowFailed(hr, L"SHGetSpecialFolderLocation(CSIDL_DRIVES)");
}

std::wstring GetFolderNameFromPath(const std::wstring & path)
{
    const auto i = path.find_last_of(wds::chrBackslash);
    return i == std::wstring::npos ? path : path.substr(0, i);
}

std::wstring GetCOMSPEC()
{
    std::array<WCHAR, _MAX_PATH> cmd;
    if (::GetEnvironmentVariable(L"COMSPEC", cmd.data(), static_cast<DWORD>(cmd.size())) == 0)
    {
        VTRACE(L"COMSPEC not set.");
        return L"cmd.exe";
    }

    return cmd.data();
}

void WaitForHandleWithRepainting(const HANDLE h, const DWORD TimeOut)
{
    while (true)
    {
        // Read all messages in this next loop, removing each message as we read it.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, WM_PAINT, WM_PAINT, PM_REMOVE))
        {
            ::DispatchMessage(&msg);
        }

        // Wait for WM_PAINT message sent or posted to this queue
        // or for one of the passed handles be set to signal.
        const DWORD r = ::MsgWaitForMultipleObjects(1, &h, FALSE, TimeOut, QS_PAINT);

        // The result tells us the type of event we have.
        if (r == WAIT_OBJECT_0 + 1)
        {
            // New messages have arrived.
            // Continue to the top of the always while loop to dispatch them and resume waiting.
            continue;
        }

        // The handle became signaled.
        break;
    }
}

bool FolderExists(const std::wstring & path)
{
    DWORD result = GetFileAttributes(FileFindEnhanced::MakeLongPathCompatible(path).c_str());
    return result != INVALID_FILE_ATTRIBUTES && (result & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool DriveExists(const std::wstring& path)
{
    if (path.size() != 3 || path[1] != wds::chrColon || path[2] != wds::chrBackslash)
    {
        return false;
    }

    const int d = std::toupper(path.at(0)) - wds::strAlpha.at(0);
    if (const DWORD mask = 0x1 << d; (mask & ::GetLogicalDrives()) == 0)
    {
        return false;
    }

    if (std::wstring dummy; !::GetVolumeName(path, dummy))
    {
        return false;
    }

    return true;
}

// drive is a drive spec like C: or C:\ or C:\path (path is ignored).
//
// This function returns
// "", if QueryDosDevice is unsupported or drive doesn't begin with a drive letter,
// 'Information about MS-DOS device names' otherwise:
// Something like
//
// \Device\Harddisk\Volume1                               for a local drive
// \Device\LanmanRedirector\;T:0000000011e98\spock\temp   for a network drive
// \??\C:\programme                                       for a SUBSTed local path
// \??\T:\Neuer Ordner                                    for a SUBSTed SUBSTed path
// \??\UNC\spock\temp                                     for a SUBSTed UNC path
//
// As always, I had to experimentally determine these strings, Microsoft
// didn't think it was necessary to document them. (Sometimes I think, they
// even don't document such things internally...)
//
// I hope that a drive is SUBSTed iff this string starts with \??\.
//
// assarbad:
//   It cannot be safely determined whether a path is or is not SUBSTed on NT
//   via this API. You would have to look up the volume mount points because
//   SUBST only works per session by definition whereas volume mount points
//   work across sessions (after restarts).
//
std::wstring MyQueryDosDevice(const std::wstring & drive)
{
    std::wstring d = drive;

    if (d.size() < 2 || d[1] != wds::chrColon)
    {
        return wds::strEmpty;
    }

    d = d.substr(0, 2);

    std::array<WCHAR, 512> info;
    if (::QueryDosDevice(d.c_str(), info.data(), static_cast<DWORD>(info.size())) == 0)
    {
        VTRACE(L"QueryDosDevice({}) failed: {}", d.c_str(), MdGetWinErrorText(::GetLastError()).c_str());
        return {};
    }

    return info.data();
}

// drive is a drive spec like C: or C:\ or C:\path (path is ignored).
// 
// This function returnes true, if QueryDosDevice() is supported
// and drive is a SUBSTed drive.
//
bool IsSUBSTedDrive(const std::wstring & drive)
{
    const std::wstring info = MyQueryDosDevice(drive);
    return info.size() >= 4 && info.substr(0, 4) == L"\\??\\";
}

const std::wstring & GetSpec_Bytes()
{
    static std::wstring s = Localization::Lookup(IDS_SPEC_BYTES, L"Bytes");
    return s;
}

const std::wstring& GetSpec_KB()
{
    static std::wstring s = Localization::Lookup(IDS_SPEC_KB, L"KiB");
    return s;
}

const std::wstring& GetSpec_MB()
{
    static std::wstring s = Localization::Lookup(IDS_SPEC_MB, L"MiB");
    return s;
}

const std::wstring& GetSpec_GB()
{
    static std::wstring s = Localization::Lookup(IDS_SPEC_GB, L"GiB");
    return s;
}

const std::wstring& GetSpec_TB()
{
    static std::wstring s = Localization::Lookup(IDS_SPEC_TB, L"TiB");
    return s;
}

bool IsAdmin()
{
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (SmartPointer<PSID> pSid(FreeSid); ::AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pSid))
    {
        BOOL bResult = FALSE;
        if (!::CheckTokenMembership(nullptr, pSid, &bResult))
        {
            return false;
        }
        return bResult != FALSE;
    }

    return false;
}

bool FileIconInit()
{
    // Required to use the system image lists
    SmartPointer<HMODULE> hmod(FreeLibrary, LoadLibrary(L"shell32.dll"));
    if (hmod != nullptr)
    {
        BOOL(WINAPI * FileIconInitFunc)(BOOL) =
            reinterpret_cast<decltype(FileIconInitFunc)>(
                static_cast<LPVOID>(GetProcAddress(hmod, reinterpret_cast<LPCSTR>(660))));
        if (FileIconInitFunc != nullptr)
        {
            return FileIconInitFunc(TRUE);
        }
    }

    return true;
}

bool EnableReadPrivileges()
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
    for (const std::wstring & priv : { SE_RESTORE_NAME, SE_BACKUP_NAME })
    {
        // Populate the privilege adjustment structure
        TOKEN_PRIVILEGES privEntry = {};
        privEntry.PrivilegeCount = 1;
        privEntry.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        // Translate the privilege name into the binary representation
        if (LookupPrivilegeValue(nullptr, priv.c_str(), &privEntry.Privileges[0].Luid) == 0)
        {
            ret = false;
            continue;
        }

        // Check if privilege is in the list of ones we have
        if (std::count_if(privsAvailable[0].Privileges, &privsAvailable->Privileges[privsAvailable->PrivilegeCount],
            [&](const LUID_AND_ATTRIBUTES& element) {
                return element.Luid.HighPart == privEntry.Privileges[0].Luid.HighPart &&
                    element.Luid.LowPart == privEntry.Privileges[0].Luid.LowPart;}) == 0)
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

void ReplaceString(std::wstring& subject, const std::wstring& search, const std::wstring& replace)
{
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}

std::wstring& TrimString(std::wstring& s, wchar_t c)
{
    while (!s.empty() && s.back() == c) s.pop_back();
    while (!s.empty() && s.front() == c) s.erase();
    return s;
}

std::wstring& MakeLower(std::wstring& s)
{
    _wcslwr_s(s.data(), s.size() + 1);
    return s;
}

const std::wstring & GetSysDirectory()
{
    static std::wstring s;
    if (!s.empty()) return s;
    s.resize(_MAX_PATH), ::GetSystemDirectory(s.data(), _MAX_PATH);
    s.resize(wcslen(s.data()));
    return s;
}
