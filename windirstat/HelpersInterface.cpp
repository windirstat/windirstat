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
//

#include "pch.h"
#include "HelpersInterface.h"
#include "Options.h"
#include "Localization.h"

static NTSTATUS(NTAPI* RtlDecompressBufferEx)(USHORT CompressionFormat, PUCHAR UncompressedBuffer,
    ULONG  UncompressedBufferSize, PUCHAR CompressedBuffer, ULONG  CompressedBufferSize,
    PULONG FinalUncompressedSize, PVOID WorkSpace) = reinterpret_cast<decltype(RtlDecompressBufferEx)>(
        reinterpret_cast<LPVOID>(GetProcAddress(GetModuleHandle(L"ntdll.dll"), "RtlDecompressBufferEx")));

static NTSTATUS(NTAPI* RtlGetCompressionWorkSpaceSize)(USHORT CompressionFormat,
    PULONG CompressBufferWorkSpaceSize, PULONG CompressFragmentWorkSpaceSize) = reinterpret_cast<decltype(RtlGetCompressionWorkSpaceSize)>(
        reinterpret_cast<LPVOID>(GetProcAddress(GetModuleHandle(L"ntdll.dll"), "RtlGetCompressionWorkSpaceSize")));

static std::wstring FormatLongLongNormal(ULONGLONG n)
{
    if (n == 0) return L"0";

    const wchar_t sep = GetLocaleThousandSeparator();
    std::array<WCHAR, 32> buffer;
    size_t pos = buffer.size() - 1;
    buffer[pos] = L'\0';

    for (int count = 0; n > 0; ++count, n /= 10)
    {
        if (count && count % 3 == 0) buffer[--pos] = sep;
        buffer[--pos] = L'0' + (n % 10);
    }

    return std::wstring(buffer.begin() + pos, buffer.end() - 1);
}

std::wstring GetLocaleString(const LCTYPE lctype, const LCID lcid)
{
    const int len = ::GetLocaleInfo(lcid, lctype, nullptr, 0);
    if (len <= 0) return {};

    std::wstring s(len - 1, L'\0');
    ::GetLocaleInfo(lcid, lctype, s.data(), len);
    return s;
}

std::wstring GetLocaleLanguage(const LANGID langid)
{
    const std::wstring s = GetLocaleString(LOCALE_SLOCALIZEDLANGUAGENAME, langid);
    const std::wstring n = GetLocaleString(LOCALE_SNATIVELANGNAME, langid);
    return std::format(L"{} ({})", s, n);
}

wchar_t GetLocaleThousandSeparator() noexcept
{
    static LCID cachedLocale = static_cast<LCID>(-1);
    static wchar_t cachedChar;
    if (cachedLocale != COptions::GetLocaleForFormatting())
    {
        cachedLocale = COptions::GetLocaleForFormatting();
        cachedChar = GetLocaleString(LOCALE_STHOUSAND, cachedLocale)[0];
    }
    return cachedChar;
}

wchar_t GetLocaleDecimalSeparator() noexcept
{
    static LCID cachedLocale = static_cast<LCID>(-1);
    static wchar_t cachedChar;
    if (cachedLocale != COptions::GetLocaleForFormatting())
    {
        cachedLocale = COptions::GetLocaleForFormatting();
        cachedChar = GetLocaleString(LOCALE_SDECIMAL, cachedLocale)[0];
    }
    return cachedChar;
}

std::wstring FormatBytes(const ULONGLONG n)
{
    if (COptions::UseSizeSuffixes)
    {
        return FormatSizeSuffixes(n);
    }

    return std::format(L"{} {}", FormatLongLongNormal(n), GetSpec_Bytes());
}

std::wstring FormatSizeSuffixes(ULONGLONG n)
{
    constexpr ULONGLONG BASE_BYTES = 1024;
    constexpr ULONGLONG KIB_BYTES = BASE_BYTES;
    constexpr ULONGLONG MIB_BYTES = KIB_BYTES * BASE_BYTES;
    constexpr ULONGLONG GIB_BYTES = MIB_BYTES * BASE_BYTES;
    constexpr ULONGLONG TIB_BYTES = GIB_BYTES * BASE_BYTES;
    constexpr ULONGLONG TIB_THRESHOLD_BYTES = TIB_BYTES - (GIB_BYTES / 2);
    constexpr ULONGLONG GIB_THRESHOLD_BYTES = GIB_BYTES - (MIB_BYTES / 2);
    constexpr ULONGLONG MIB_THRESHOLD_BYTES = MIB_BYTES - (KIB_BYTES / 2);

    if (n >= TIB_THRESHOLD_BYTES)
    {
        return std::format(L"{} {}", FormatDouble(static_cast<double>(n) / TIB_BYTES), GetSpec_TiB());
    }
    if (n >= GIB_THRESHOLD_BYTES)
    {
        return std::format(L"{} {}", FormatDouble(static_cast<double>(n) / GIB_BYTES), GetSpec_GiB());
    }
    if (n >= MIB_THRESHOLD_BYTES)
    {
        return std::format(L"{} {}", FormatDouble(static_cast<double>(n) / MIB_BYTES), GetSpec_MiB());
    }
    if (n >= KIB_BYTES)
    {
        return std::format(L"{} {}", FormatDouble(static_cast<double>(n) / KIB_BYTES), GetSpec_KiB());
    }
    return std::format(L"{} {}", n, GetSpec_Bytes());
}

std::wstring FormatCount(const ULONGLONG n)
{
    return FormatLongLongNormal(n);
}

std::wstring FormatDouble(double d)
{
    ASSERT(d >= 0);

    const int x = std::lround(d * 10);
    const int i = x / 10;
    const int r = x % 10;

    return std::format(L"{}{}{}", i, GetLocaleDecimalSeparator(), r);
}

std::wstring FormatFileTime(const FILETIME& t)
{
    SYSTEMTIME st;
    FILETIME ft;
    if (FileTimeToLocalFileTime(&t, &ft) == 0 || FileTimeToSystemTime(&ft, &st) == 0)
    {
        return {};
    }

    const LCID lcid = COptions::GetLocaleForFormatting();

    std::array<WCHAR, 64> date;
    GetDateFormat(lcid, DATE_SHORTDATE, &st, nullptr, date.data(), std::ssize(date));

    std::array<WCHAR, 64> time;
    GetTimeFormat(lcid, TIME_NOSECONDS, &st, nullptr, time.data(), std::ssize(time));

    return std::format(L"{}  {}", date.data(), time.data());
}

std::wstring FormatAttributes(const DWORD attr)
{
    if (attr == INVALID_FILE_ATTRIBUTES) return wds::strInvalidAttributes;

    std::wstring attributes;
    if (attr & FILE_ATTRIBUTE_READONLY) attributes += wds::chrAttributeReadonly;
    if (attr & FILE_ATTRIBUTE_HIDDEN) attributes += wds::chrAttributeHidden;
    if (attr & FILE_ATTRIBUTE_SYSTEM) attributes += wds::chrAttributeSystem;
    if (attr & FILE_ATTRIBUTE_ARCHIVE) attributes += wds::chrAttributeArchive;
    if (attr & FILE_ATTRIBUTE_COMPRESSED) attributes += wds::chrAttributeCompressed;
    if (attr & FILE_ATTRIBUTE_ENCRYPTED) attributes += wds::chrAttributeEncrypted;
    if (attr & FILE_ATTRIBUTE_OFFLINE) attributes += wds::chrAttributeOffline;
    if (attr & FILE_ATTRIBUTE_SPARSE_FILE) attributes += wds::chrAttributeSparse;

    return attributes;
}

std::wstring FormatMilliseconds(const ULONGLONG ms)
{
    const ULONGLONG sec = (ms + 500) / 1000;
    const ULONGLONG s = sec % 60;
    const ULONGLONG min = sec / 60;
    const ULONGLONG m = min % 60;
    const ULONGLONG h = min / 60;

    return (h <= 0) ? std::format(L"{}:{:02}", m, s) : std::format(L"{}:{:02}:{:02}", h, m, s);
}

std::wstring FormatVolumeNameOfRootPath(const std::wstring& rootPath)
{
    std::wstring volumeName;
    return GetVolumeName(rootPath, volumeName) ? FormatVolumeName(rootPath, volumeName) : rootPath;
}

std::wstring FormatVolumeName(const std::wstring& rootPath, const std::wstring& volumeName)
{
    return std::format(L"{} ({:.2})", volumeName, rootPath);
}

// File and path helpers
std::wstring GetFolderNameFromPath(const std::wstring& path)
{
    return std::filesystem::path(path).parent_path().wstring();
}

std::wstring GetBaseNameFromPath(const std::wstring& path)
{
    return std::filesystem::path(path).filename().wstring();
}

std::wstring GlobToRegex(const std::wstring& glob, const bool useAnchors)
{
    static const std::wregex escapePattern(LR"([.\\+^$|()[\]{}])", std::regex_constants::optimize);
    static const std::wregex starPattern(LR"(\*+)", std::regex_constants::optimize);
    static const std::wregex questionPattern(LR"(\?)", std::regex_constants::optimize);

    std::wstring regex = glob;
    regex = std::regex_replace(regex, escapePattern, LR"(\$&)");
    regex = std::regex_replace(regex, starPattern, LR"([^\\/:]*)");
    regex = std::regex_replace(regex, questionPattern, LR"([^\\/:])");

    return useAnchors ? (L"^" + regex + L"$") : regex;
}

// String helpers
void ReplaceString(std::wstring& subject, const std::wstring& search, const std::wstring& replace)
{
    if (search.empty()) return;

    for (size_t i = 0; (i = subject.find(search, i)) != std::wstring::npos; i += replace.length())
    {
        subject.replace(i, search.length(), replace);
    }
}

std::wstring& TrimString(std::wstring& s, const wchar_t c, const bool endOnly) noexcept
{
    while (!s.empty() && s.back() == c) s.pop_back();
    if (!endOnly) while (!s.empty() && s.front() == c) s.erase(0, 1);
    return s;
}

std::wstring MakeLower(const std::wstring& s)
{
    std::wstring lower = s;
    _wcslwr_s(lower.data(), lower.size() + 1);
    return lower;
}

// Attribute parsing
DWORD ParseAttributes(const std::wstring& attributes) noexcept
{
    if (attributes == wds::strInvalidAttributes) return 0;

    DWORD attr = 0;
    for (WCHAR ch : attributes)
    {
        if (ch == wds::chrAttributeReadonly) attr |= FILE_ATTRIBUTE_READONLY;
        else if (ch == wds::chrAttributeHidden) attr |= FILE_ATTRIBUTE_HIDDEN;
        else if (ch == wds::chrAttributeSystem) attr |= FILE_ATTRIBUTE_SYSTEM;
        else if (ch == wds::chrAttributeArchive) attr |= FILE_ATTRIBUTE_ARCHIVE;
        else if (ch == wds::chrAttributeCompressed) attr |= FILE_ATTRIBUTE_COMPRESSED;
        else if (ch == wds::chrAttributeEncrypted) attr |= FILE_ATTRIBUTE_ENCRYPTED;
        else if (ch == wds::chrAttributeOffline) attr |= FILE_ATTRIBUTE_OFFLINE;
        else if (ch == wds::chrAttributeSparse) attr |= FILE_ATTRIBUTE_SPARSE_FILE;
    }

    return attr;
}

// Size specifiers
const std::wstring& GetSpec_Bytes() noexcept
{
    static std::wstring s = Localization::Lookup(IDS_SPEC_BYTES);
    return s;
}

const std::wstring& GetSpec_KiB() noexcept
{
    static std::wstring s = Localization::Lookup(IDS_SPEC_KiB);
    return s;
}

const std::wstring& GetSpec_MiB() noexcept
{
    static std::wstring s = Localization::Lookup(IDS_SPEC_MiB);
    return s;
}

const std::wstring& GetSpec_GiB() noexcept
{
    static std::wstring s = Localization::Lookup(IDS_SPEC_GiB);
    return s;
}

const std::wstring& GetSpec_TiB() noexcept
{
    static std::wstring s = Localization::Lookup(IDS_SPEC_TiB);
    return s;
}

// System information
std::wstring GetCOMSPEC()
{
    std::array<WCHAR, _MAX_PATH> cmd;
    if (::GetEnvironmentVariable(L"COMSPEC", cmd.data(), std::ssize(cmd)) == 0)
    {
        VTRACE(L"COMSPEC not set.");
        return L"cmd.exe";
    }

    return cmd.data();
}

const std::wstring& GetSysDirectory() noexcept
{
    static std::wstring s;
    if (!s.empty()) return s;
    s.resize(_MAX_PATH), ::GetSystemDirectory(s.data(), _MAX_PATH);
    s.resize(wcslen(s.data()));
    return s;
}

// UI helpers
void WaitForHandleWithRepainting(const HANDLE h, const DWORD TimeOut) noexcept
{
    while (true)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, WM_PAINT, WM_PAINT, PM_REMOVE))
        {
            ::DispatchMessage(&msg);
        }

        const DWORD r = MsgWaitForMultipleObjects(1, &h, FALSE, TimeOut, QS_PAINT);

        if (r == WAIT_OBJECT_0 + 1)
        {
            continue;
        }

        break;
    }
}

void ProcessMessagesUntilSignaled(const std::function<void()>& callback)
{
    if (CWnd* wnd = AfxGetMainWnd(); wnd != nullptr && GetWindowThreadProcessId(
        wnd->m_hWnd, nullptr) == GetCurrentThreadId())
    {
        static auto waitMessage = RegisterWindowMessage(L"WinDirStatSignalWaiter");
        std::jthread([wnd, &callback]() mutable
            {
                callback();
                wnd->PostMessage(waitMessage, 0, 0);
            }).detach();

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

void DisplayError(const std::wstring& error)
{
    WdsMessageBox(error.c_str(), MB_OK | MB_ICONERROR);
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

bool ShellExecuteWrapper(const std::wstring& lpFile, const std::wstring& lpParameters, const std::wstring& lpVerb,
    const HWND hwnd, const std::wstring& lpDirectory, const INT nShowCmd)
{
    CWaitCursor wc;
    
    SHELLEXECUTEINFO sei = {
        .cbSize = sizeof(SHELLEXECUTEINFO),
        .fMask = 0,
        .hwnd = hwnd,
        .lpVerb = lpVerb.empty() ? nullptr : lpVerb.c_str(),
        .lpFile = lpFile.empty() ? nullptr : lpFile.c_str(),
        .lpParameters = lpParameters.empty() ? nullptr : lpParameters.c_str(),
        .lpDirectory = lpDirectory.empty() ? nullptr : lpDirectory.c_str(),
        .nShow = nShowCmd
    };

    const BOOL bResult = ::ShellExecuteEx(&sei);
    if (!bResult && GetLastError() != ERROR_CANCELLED)
    {
        DisplayError(std::format(L"ShellExecute failed: {}",
            TranslateError(GetLastError())));
    }
    return bResult;
}

// DPI scaling
int DpiRest(int value, CWnd* wnd) noexcept
{
    const HWND h = (wnd && wnd->GetSafeHwnd()) ? wnd->GetSafeHwnd() : nullptr;
    SmartPointer<HDC> dc([h](HDC hdc) { ReleaseDC(h, hdc); }, GetDC(h));
    const int dpi = dc != nullptr ? ::GetDeviceCaps(dc, LOGPIXELSX) : USER_DEFAULT_SCREEN_DPI;
    return ::MulDiv(value, dpi, USER_DEFAULT_SCREEN_DPI);
}

int DpiSave(int value, CWnd* wnd) noexcept
{
    const HWND h = (wnd && wnd->GetSafeHwnd()) ? wnd->GetSafeHwnd() : nullptr;
    SmartPointer<HDC> dc([h](HDC hdc) { ReleaseDC(h, hdc); }, GetDC(h));
    const int dpi = dc != nullptr ? ::GetDeviceCaps(dc, LOGPIXELSX) : USER_DEFAULT_SCREEN_DPI;
    return ::MulDiv(value, USER_DEFAULT_SCREEN_DPI, dpi);
}

// Context menu
IContextMenu* GetContextMenu(const HWND hwnd, const std::vector<std::wstring>& paths)
{
    // structures to hold and track pidls for children
    std::vector<SmartPointer<LPITEMIDLIST>> pidlsForCleanup;
    std::vector<LPCITEMIDLIST> pidlsRelatives;

    // create list of children from paths
    for (auto& path : paths)
    {
        const LPCITEMIDLIST pidl = ILCreateFromPath(path.c_str());
        if (pidl == nullptr) return nullptr;
        pidlsForCleanup.emplace_back(CoTaskMemFree, const_cast<LPITEMIDLIST>(pidl));

        CComPtr<IShellFolder> pParentFolder;
        LPCITEMIDLIST pidlRelative;
        if (FAILED(SHBindToParent(pidl, IID_IShellFolder, reinterpret_cast<LPVOID*>(&pParentFolder), &pidlRelative))) return nullptr;
        pidlsRelatives.push_back(pidlRelative);

        // on last item, return the context menu
        if (pidlsRelatives.size() == paths.size())
        {
            IContextMenu* pContextMenu = nullptr;
            if (FAILED(pParentFolder->GetUIObjectOf(hwnd, static_cast<UINT>(pidlsRelatives.size()),
                pidlsRelatives.data(), IID_IContextMenu, nullptr, reinterpret_cast<LPVOID*>(&pContextMenu)))) return nullptr;
            return pContextMenu;
        }
    }

    return nullptr;
}

// Application info
std::wstring GetAppFileName(const std::wstring& ext)
{
    std::wstring s(_MAX_PATH, wds::chrNull);
    ::GetModuleFileName(nullptr, s.data(), _MAX_PATH);
    s.resize(wcslen(s.data()));

    // optional substitute extension
    if (!ext.empty())
    {
        return std::filesystem::path(s).replace_extension(ext);
    }

    return s;
}

std::wstring GetAppFolder()
{
    return std::filesystem::path(GetAppFileName()).parent_path().wstring();
}

// Resources
std::vector<BYTE> GetCompressedResource(const HRSRC resource) noexcept
{
    // Establish the resource
    const HGLOBAL resourceData = ::LoadResource(nullptr, resource);
    if (resourceData == nullptr) return {};

    // Fetch a pointer to the data
    const LPVOID binaryData = LockResource(resourceData);
    if (binaryData == nullptr) return {};

    // Get workspace size needed for decompression
    ULONG workSpaceSize = 0;
    ULONG fragmentWorkSpaceSize = 0;
    if (!NT_SUCCESS(RtlGetCompressionWorkSpaceSize(COMPRESSION_FORMAT_XPRESS_HUFF, &workSpaceSize,
        &fragmentWorkSpaceSize))) return {};

    // Decompress data
    const size_t resourceSize = SizeofResource(nullptr, resource);
    ULONG decompressedSize = *static_cast<const ULONG*>(binaryData);
    std::vector<BYTE> decompressedData(decompressedSize);
    ULONG finalDecompressedSize = 0;

    std::vector<BYTE> workSpace(workSpaceSize);
    if (!NT_SUCCESS(RtlDecompressBufferEx(COMPRESSION_FORMAT_XPRESS_HUFF, decompressedData.data(),
        static_cast<LONG>(decompressedData.size()), static_cast<PUCHAR>(binaryData) + sizeof(ULONG),
        static_cast<ULONG>(resourceSize) - sizeof(ULONG), &finalDecompressedSize, workSpace.data())))
    {
        return {};
    }

    return decompressedData;
}
