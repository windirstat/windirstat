// GlobalHelpers.cpp - Implementation of global helper functions
//
// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
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
#include "SmartPointer.h"
#include "GlobalHelpers.h"
#include "Options.h"
#include "Localization.h"
#include "FinderBasic.h"

#include <array>
#include <algorithm>
#include <regex>
#include <map>

#pragma comment(lib,"powrprof.lib")
#pragma comment(lib,"ntdll.lib")

EXTERN_C NTSTATUS NTAPI RtlDecompressBuffer(USHORT CompressionFormat, PUCHAR UncompressedBuffer, ULONG  UncompressedBufferSize,
    PUCHAR CompressedBuffer, ULONG  CompressedBufferSize, PULONG FinalUncompressedSize);

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

std::wstring GetLocaleString(const LCTYPE lctype, const LCID lcid)
{
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
    static LCID cachedLocale = static_cast<LCID>(-1);
    static std::wstring cachedString;
    if (cachedLocale != COptions::GetLocaleForFormatting())
    {
        cachedLocale = COptions::GetLocaleForFormatting();
        cachedString = GetLocaleString(LOCALE_STHOUSAND, cachedLocale);
    }
    return cachedString;
}

std::wstring GetLocaleDecimalSeparator()
{
    static LCID cachedLocale = static_cast<LCID>(-1);
    static std::wstring cachedString;
    if (cachedLocale != COptions::GetLocaleForFormatting())
    {
        cachedLocale = COptions::GetLocaleForFormatting();
        cachedString = GetLocaleString(LOCALE_SDECIMAL, cachedLocale);
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

    const double KiB = static_cast<int>(n % base);
    n /= base;

    const double MiB = static_cast<int>(n % base);
    n /= base;

    const double GiB = static_cast<int>(n % base);
    n /= base;

    const double TiB = static_cast<int>(n);

    if (TiB != 0.0 || GiB == base - 1 && MiB >= half)
    {
        return FormatDouble(TiB + GiB / base) + L" " + GetSpec_TiB();
    }
    if (GiB != 0.0 || MiB == base - 1 && KiB >= half)
    {
        return FormatDouble(GiB + MiB / base) + L" " + GetSpec_GiB();
    }
    if (MiB != 0.0 || KiB == base - 1 && B >= half)
    {
        return FormatDouble(MiB + KiB / base) + L" " + GetSpec_MiB();
    }
    if (KiB != 0.0)
    {
        return FormatDouble(KiB + B / base) + L" " + GetSpec_KiB();
    }

    return std::to_wstring(static_cast<ULONG>(B)) + L" " + GetSpec_Bytes();
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
        FileTimeToLocalFileTime(&t, &ft) == 0 ||
        FileTimeToSystemTime(&ft, &st) == 0)
    {
        return L"";
    }
    
    const LCID lcid = COptions::GetLocaleForFormatting();

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

    if (attr & FILE_ATTRIBUTE_SPARSE_FILE)
    {
        attributes.append(wds::strAttributeSparse);
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
    return std::format(L"{} ({:.2})", volumeName, rootPath);
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
        const DWORD r = MsgWaitForMultipleObjects(1, &h, FALSE, TimeOut, QS_PAINT);

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
    const DWORD result = GetFileAttributes(FinderBasic::MakeLongPathCompatible(path).c_str());
    return result != INVALID_FILE_ATTRIBUTES && (result & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool DriveExists(const std::wstring& path)
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
        VTRACE(L"QueryDosDevice({}) Failed: {}", d.c_str(), TranslateError());
        return {};
    }

    return info.data();
}

// drive is a drive spec like C: or C:\ or C:\path (path is ignored).
//
// This function returns true, if QueryDosDevice() is supported
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

const std::wstring& GetSpec_KiB()
{
    static std::wstring s = Localization::Lookup(IDS_SPEC_KiB, L"KiB");
    return s;
}

const std::wstring& GetSpec_MiB()
{
    static std::wstring s = Localization::Lookup(IDS_SPEC_MiB, L"MiB");
    return s;
}

const std::wstring& GetSpec_GiB()
{
    static std::wstring s = Localization::Lookup(IDS_SPEC_GiB, L"GiB");
    return s;
}

const std::wstring& GetSpec_TiB()
{
    static std::wstring s = Localization::Lookup(IDS_SPEC_TiB, L"TiB");
    return s;
}

bool IsElevationActive()
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

bool IsElevationAvailable()
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

std::wstring& TrimString(std::wstring& s, const wchar_t c)
{
    while (!s.empty() && s.back() == c) s.pop_back();
    while (!s.empty() && s.front() == c) s.erase(0, 1);
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

void ProcessMessagesUntilSignaled(const std::function<void()>& callback)
{
    if (CWnd* wnd = AfxGetMainWnd(); wnd != nullptr && GetWindowThreadProcessId(
        wnd->m_hWnd, nullptr) == GetCurrentThreadId())
    {
        // Start thread and wait post message when done
        static auto waitMessage = RegisterWindowMessage(L"WinDirStatSignalWaiter");
        std::thread([wnd, &callback]() mutable
        {
            callback();
            wnd->PostMessageW(waitMessage, 0, 0);
        }).detach();

        // Read all messages in this next loop, removing each message as we read it
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            if (msg.message == waitMessage) break;
            if (msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST) continue;
            if (msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST) continue;
            if (msg.message == WM_NCLBUTTONDOWN || msg.message == WM_NCLBUTTONUP) continue;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    else
    {
        callback();
    }
}

std::wstring GlobToRegex(const std::wstring& glob, const bool useAnchors)
{
    std::wstring regex = glob;

    // Replace escape sequences for '\' in the glob
    regex = std::regex_replace(regex, std::wregex(LR"([.\\+^$|()[\]{}])"), LR"(\$&)");

    // Replace '*' (match any sequence of characters)
    regex = std::regex_replace(regex, std::wregex(LR"(\*)"), LR"([^\\/:]*)");

    // Replace '?' (match any single character)
    regex = std::regex_replace(regex, std::wregex(LR"(\?)"), LR"([^\\/:])");

    return useAnchors ? (L"^" + regex + L"$") : regex;
}

std::vector<BYTE> GetCompressedResource(const HRSRC resource)
{
    // Establish the resource
    const HGLOBAL resourceData = ::LoadResource(nullptr, resource);
    if (resourceData == nullptr) return {};

    // Fetch a pointer to the data
    const LPVOID binaryData = LockResource(resourceData);
    if (binaryData == nullptr) return {};

    // Decompress data
    size_t resourceSize = SizeofResource(nullptr, resource);
    std::vector<BYTE> decompressedData(resourceSize * 4u);
    ULONG finalDecompressedSize = 0;
    if (RtlDecompressBuffer(COMPRESSION_FORMAT_LZNT1, decompressedData.data(), static_cast<LONG>(decompressedData.size()),
        static_cast<PUCHAR>(binaryData), static_cast<ULONG>(resourceSize), &finalDecompressedSize) != ERROR_SUCCESS) return {};
    decompressedData.resize(finalDecompressedSize);

    return decompressedData;
}

std::wstring GetVolumePathNameEx(const std::wstring & path)
{
    // Establish a fallback volume as drive letter or server name
    std::wstring fallback;
    std::wsmatch match;
    if (std::regex_match(path, match, std::wregex(LR"(\\\\\?\\([A-Z]:).*)")) && match.size() > 1 ||
        std::regex_match(path, match, std::wregex(LR"(\\\\\?\\UNC\\([^\\]*).*)")) && match.size() > 1)
    {
        fallback = match[1].str();
    }

    // First, try the regular path resolution
    std::array<WCHAR, MAX_PATH> volume;
    if (GetVolumePathName(path.c_str(),
        volume.data(), static_cast<DWORD>(volume.size())) != 0)
    {
        return volume.data();
    }

    // Create a file handle to do a reverse lookup (in case of subst'd drive)
    SmartPointer<HANDLE> handle(CloseHandle, CreateFile(path.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    if (handle == nullptr) return fallback;

    // Determine the maximum size to hold the resultant final path
    const DWORD bufferSize = GetFinalPathNameByHandle(handle, nullptr, 0, FILE_NAME_NORMALIZED);
    if (bufferSize == 0) return fallback;

    // Lookup the path and then determine the pathname from it
    std::vector<WCHAR> final(bufferSize + 1, L'\0');
    if (GetFinalPathNameByHandle(handle, final.data(), static_cast<DWORD>(final.size()), FILE_NAME_NORMALIZED) != 0 &&
        GetVolumePathName(final.data(), volume.data(), static_cast<DWORD>(volume.size())) != 0)
    {
        return volume.data();
    }

    return fallback;
}

void DisplayError(const std::wstring& error)
{
    AfxMessageBox(error.c_str(), MB_OK | MB_ICONERROR);
}

std::wstring TranslateError(const HRESULT hr)
{
    SmartPointer<LPVOID> lpMsgBuf(LocalFree);
    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&lpMsgBuf), 0, nullptr) == 0)
    {
        const CStringW s(MAKEINTRESOURCE(AFX_IDP_NO_ERROR_AVAILABLE));
        return std::format(L"{} {:#08x}", s.GetString(), static_cast<DWORD>(hr));
    }
    return static_cast<LPWSTR>(*lpMsgBuf);
}

void DisableHibernate()
{
    BOOLEAN hibernateEnabled = FALSE;
    (void) CallNtPowerInformation(SystemReserveHiberFile, &hibernateEnabled,
        sizeof(hibernateEnabled), nullptr, 0);

    // Delete file in the event that the above call does not actually delete the file as
    // designed or hibernate was previously disabled in a way that did not delete the file
    WCHAR drive[3];
    if (GetEnvironmentVariable(L"SystemDrive", drive, std::size(drive)) == std::size(drive) - 1)
    {
        DeleteFile((drive + std::wstring(L"\\hiberfil.sys")).c_str());
    }
}

bool IsHibernateEnabled()
{
    WCHAR drive[3];
    return GetEnvironmentVariable(L"SystemDrive", drive, std::size(drive)) == std::size(drive) - 1 &&
        FinderBasic::DoesFileExist(drive + std::wstring(L"\\"), L"hiberfil.sys");
}

bool ShellExecuteWrapper(const std::wstring& lpFile, const std::wstring& lpParameters, const std::wstring& lpVerb,
    const HWND hwnd, const std::wstring& lpDirectory, const INT nShowCmd)
{
    CWaitCursor wc;

    SHELLEXECUTEINFO sei;
    ZeroMemory(&sei, sizeof(SHELLEXECUTEINFO));
    sei.cbSize = sizeof(SHELLEXECUTEINFO);
    sei.fMask = 0;
    sei.hwnd = hwnd;
    sei.lpParameters = lpParameters.empty() ? nullptr : lpParameters.c_str();
    sei.lpVerb = lpVerb.empty() ? nullptr : lpVerb.c_str();
    sei.lpFile = lpFile.empty() ? nullptr : lpFile.c_str();
    sei.lpDirectory = lpDirectory.empty() ? nullptr : lpDirectory.c_str();
    sei.nShow = nShowCmd;

    const BOOL bResult = ::ShellExecuteEx(&sei);
    if (!bResult)
    {
        DisplayError(std::format(L"ShellExecute failed: {}",
            TranslateError(GetLastError())));
    }
    return bResult;
}

std::wstring GetBaseNameFromPath(const std::wstring& path)
{
    std::wstring s = path;
    const auto i = s.find_last_of(wds::chrBackslash);
    if (i == std::wstring::npos)
    {
        return s;
    }
    return s.substr(i + 1);
}

std::wstring GetAppFileName(const std::wstring& ext)
{
    std::wstring s(_MAX_PATH, wds::chrNull);
    VERIFY(::GetModuleFileName(nullptr, s.data(), _MAX_PATH));
    s.resize(wcslen(s.data()));

    // optional substitute extension
    if (!ext.empty())
    {
        s = s.substr(0, s.find_last_of(wds::chrDot) + 1) + ext;
    }

    return s;
}

std::wstring GetAppFolder()
{
    const std::wstring folder = GetAppFileName();
    return folder.substr(0, folder.find_last_of(wds::chrBackslash));
}

constexpr DWORD SidGetLength(const PSID x)
{
    return sizeof(SID) + (static_cast<SID*>(x)->SubAuthorityCount - 1) * sizeof(static_cast<SID*>(x)->SubAuthority);
}

std::wstring GetNameFromSid(const PSID sid)
{
    // return immediately if sid is null
    if (sid == nullptr) return L"";

    // define custom lookup function
    auto comp = [](const PSID p1, const PSID p2)
        {
            const DWORD l1 = SidGetLength(p1);
            const DWORD l2 = SidGetLength(p2);
            if (l1 != l2) return l1 < l2;
            return memcmp(p1, p2, l1) > 0;
        };

    // attempt to lookup sid in cache
    static std::map<PSID, std::wstring, decltype(comp)> nameMap(comp);
    if (const auto iter = nameMap.find(sid); iter != nameMap.end())
    {
        return iter->second;
    }

    // copy the sid for storage in our cache table
    const DWORD sidLength = SidGetLength(sid);
    const auto sidCopy = std::memcpy(malloc(sidLength), sid, sidLength);

    // lookup the name for this sid
    SID_NAME_USE nameUse;
    WCHAR accountName[UNLEN + 1], domainName[UNLEN + 1];
    DWORD iAccountNameSize = std::size(accountName), iDomainName = std::size(domainName);
    if (LookupAccountSid(nullptr, sid, accountName,
        &iAccountNameSize, domainName, &iDomainName, &nameUse) == 0)
    {
        SmartPointer<LPWSTR> sidBuff(LocalFree);
        ConvertSidToStringSid(sid, &sidBuff);
        nameMap[sidCopy] = sidBuff;
    }
    else
    {
        // generate full name in domain\name format
        nameMap[sidCopy] = std::format(L"{}\\{}", domainName, accountName);
    }

    // return name
    return nameMap[sidCopy];
}

IContextMenu* GetContextMenu(const HWND hwnd, const std::vector<std::wstring>& paths)
{
    // structures to hold and track pidls for children
    std::vector<SmartPointer<LPITEMIDLIST>> pidlsForCleanup;
    std::vector<LPCITEMIDLIST> pidlsRelatives;

    // create list of children from paths
    for (auto& path : paths)
    {
        LPCITEMIDLIST pidl = ILCreateFromPath(path.c_str());
        if (pidl == nullptr) return nullptr;
        pidlsForCleanup.emplace_back(CoTaskMemFree, const_cast<LPITEMIDLIST>(pidl));

        CComPtr<IShellFolder> pParentFolder;
        LPCITEMIDLIST pidlRelative;
        if (FAILED(SHBindToParent(pidl, IID_IShellFolder, reinterpret_cast<LPVOID*>(&pParentFolder), &pidlRelative))) return nullptr;
        pidlsRelatives.push_back(pidlRelative);

        // on last item, return the context menu
        if (pidlsRelatives.size() == paths.size())
        {
            IContextMenu* pContextMenu;
            if (FAILED(pParentFolder->GetUIObjectOf(hwnd, static_cast<UINT>(pidlsRelatives.size()),
                pidlsRelatives.data(), IID_IContextMenu, nullptr, reinterpret_cast<LPVOID*>(&pContextMenu)))) return nullptr;
            return pContextMenu;
        }
    }

    return nullptr;
}

bool CompressFile(const std::wstring& filePath, const CompressionAlgorithm algorithm)
{
    USHORT numericAlgorithm = static_cast<USHORT>(algorithm) & ~FILE_PROVIDER_COMPRESSION_MODERN;
    const bool modernAlgorithm = static_cast<USHORT>(algorithm) != numericAlgorithm;

    SmartPointer<HANDLE> handle(CloseHandle, CreateFileW(filePath.c_str(),
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
            {
                .Version = WOF_CURRENT_VERSION,
                .Provider = WOF_PROVIDER_FILE,
            },
            {
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

bool CompressFileAllowed(const std::wstring& filePath, const CompressionAlgorithm algorithm)
{
    static std::unordered_map<std::wstring, bool> compressionStandard;
    static std::unordered_map<std::wstring, bool> compressionModern;
    auto& compressionMap = (algorithm == CompressionAlgorithm::LZNT1) ?
        compressionStandard : compressionModern;

    // Fetch volume root path
    const auto volumeName = GetVolumePathNameEx(filePath);
    if (volumeName.empty())
    {
        return false;
    }

    // Return cached value
    if (compressionMap.contains(volumeName))
    {
        return compressionMap.at(volumeName);
    }

    // Enable 'none' button if at least standard is available
    if (algorithm == CompressionAlgorithm::NONE)
    {
        return CompressFileAllowed(filePath, CompressionAlgorithm::LZNT1) ||
            CompressFileAllowed(filePath, CompressionAlgorithm::XPRESS4K);
    }

    // Query volume for standard compression support based on whether NTFS
    std::array<WCHAR, MAX_PATH + 1> fileSystemName;
    DWORD fileSystemFlags = 0;
    const bool isNTFS = GetVolumeInformation(volumeName.c_str(), nullptr, 0, nullptr, nullptr,
        &fileSystemFlags, fileSystemName.data(), static_cast<DWORD>(fileSystemName.size())) != 0 &&
        std::wstring(fileSystemName.data()) == L"NTFS";

    // Query volume for modern compression support based on NTFS and OS version
    compressionStandard[volumeName.data()] = isNTFS && (fileSystemFlags & FILE_FILE_COMPRESSION) != 0;
    compressionModern[volumeName.data()] = isNTFS && IsWindows10OrGreater();

    return compressionMap.at(volumeName);
}
