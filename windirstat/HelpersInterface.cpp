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

#pragma comment(lib, "cabinet.lib")

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

    return { buffer.begin() + pos, buffer.end() - 1 };
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
    // Check if this is a neutral language (no specific region)
    if (SUBLANGID(langid) == SUBLANG_NEUTRAL)
    {
        // Use just the language name without the full display name
        const std::wstring s = GetLocaleString(LOCALE_SENGLISHLANGUAGENAME, langid);
        const std::wstring n = GetLocaleString(LOCALE_SNATIVELANGUAGENAME, langid);
        return s + L" - " + n;
    }

    const std::wstring s = GetLocaleString(LOCALE_SENGLISHDISPLAYNAME, langid);
    const std::wstring n = GetLocaleString(LOCALE_SNATIVEDISPLAYNAME, langid);
    return s + L" - " + n;
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

std::wstring FormatBytes(const ULONGLONG n) noexcept
{
    if (COptions::UseSizeSuffixes)
    {
        return FormatSizeSuffixes(n);
    }

    return FormatLongLongNormal(n) + L" " + GetSpec_Bytes();
}

std::wstring FormatSizeSuffixes(const ULONGLONG n) noexcept
{
    constexpr ULONGLONG K = 1024;
    constexpr ULONGLONG M = K * K;
    constexpr ULONGLONG G = M * K;
    constexpr ULONGLONG T = G * K;

    static constexpr struct {
        ULONGLONG bytes;
        ULONGLONG threshold;
        const std::wstring& (*suffix)();
    } units[] = {
        {T, T - (G / 2), GetSpec_TiB},
        {G, G - (M / 2), GetSpec_GiB},
        {M, M - (K / 2), GetSpec_MiB},
        {K, K,           GetSpec_KiB},
    };

    for (const auto& [bytes, threshold, suffix] : units) [[msvc::flatten]]
    {
        if (n < threshold) continue;
        
        return FormatDouble(static_cast<double>(n)
            / static_cast<double>(bytes)) + L" " + suffix();
    }
    return std::to_wstring(n) + L" " + GetSpec_Bytes();
}

std::wstring FormatCount(const ULONGLONG n) noexcept
{
    return FormatLongLongNormal(n);
}

std::wstring FormatDouble(const double d) noexcept
{
    ASSERT(d >= 0);

    const int x = std::lround(d * 10);
    const int i = x / 10;
    const int r = x % 10;

    return std::to_wstring(i) + GetLocaleDecimalSeparator() + std::to_wstring(r);
}

std::wstring FormatFileTime(const FILETIME& t) noexcept
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

    return std::wstring(date.data()) + L"  " + time.data();
}

std::wstring FormatAttributes(const DWORD attr) noexcept
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

std::wstring FormatHex(const std::vector<BYTE> & bytes, const bool upper) noexcept
{
    const wchar_t* h = upper
        ? L"0123456789ABCDEF"
        : L"0123456789abcdef";

    std::wstring out(bytes.size() * 2, L'\0');
    for (size_t i = 0; i < bytes.size(); ++i)
    {
        const BYTE b = bytes[i];
        out[2 * i] = h[b >> 4];
        out[2 * i + 1] = h[b & 0x0F];
    }
    return out;
}

std::wstring FormatMilliseconds(const ULONGLONG ms) noexcept
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
    s.erase(s.find_last_not_of(c) + 1);
    if (!endOnly && !s.empty()) s.erase(0, s.find_first_not_of(c));
    return s;
}

std::wstring MakeLower(const std::wstring& s)
{
    std::wstring lower = s;
    _wcslwr_s(lower.data(), lower.size() + 1);
    return lower;
}

std::wstring JoinString(const std::vector<std::wstring>& items, const WCHAR delim)
{
    return std::accumulate(items.begin(), items.end(), std::wstring(),
        [&](const std::wstring& a, const std::wstring& b) {
            return a.empty() ? b : a + delim + b;
        });
}

std::vector<std::wstring> SplitString(const std::wstring& string, const WCHAR delim)
{
    std::vector<std::wstring> selections;
    for (const auto part : std::views::split(string, delim)) {
        std::wstring partString(part.begin(), part.end());
        selections.emplace_back(TrimString(partString));
    }

    return selections;
}

// Attribute parsing
DWORD ParseAttributes(const std::wstring& attributes) noexcept
{
    if (attributes == wds::strInvalidAttributes) return 0;

    DWORD attr = 0;
    for (const WCHAR ch : attributes)
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
    std::array<WCHAR, MAX_PATH + 1> cmd;
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
    s.resize(MAX_PATH), ::GetSystemDirectory(s.data(), MAX_PATH);
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
        std::jthread([wnd, callback]() mutable
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
    WdsMessageBox(error, MB_OK | MB_ICONERROR);
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
        DisplayError(L"ShellExecute failed: " + TranslateError(GetLastError()));
    }
    return bResult;
}

// DPI scaling
int DpiRest(const int value, const CWnd* wnd) noexcept
{
    const HWND h = (wnd && wnd->GetSafeHwnd()) ? wnd->GetSafeHwnd() : nullptr;
    SmartPointer<HDC> dc([h](const HDC hdc) { ReleaseDC(h, hdc); }, GetDC(h));
    const int dpi = dc != nullptr ? ::GetDeviceCaps(dc, LOGPIXELSX) : USER_DEFAULT_SCREEN_DPI;
    return ::MulDiv(value, dpi, USER_DEFAULT_SCREEN_DPI);
}

int DpiSave(const int value, const CWnd* wnd) noexcept
{
    const HWND h = (wnd && wnd->GetSafeHwnd()) ? wnd->GetSafeHwnd() : nullptr;
    SmartPointer<HDC> dc([h](const HDC hdc) { ReleaseDC(h, hdc); }, GetDC(h));
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
    std::wstring s(MAX_PATH, wds::chrNull);
    ::GetModuleFileName(nullptr, s.data(), MAX_PATH);
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
    const HGLOBAL resourceData = ::LoadResource(nullptr, resource);
    if (!resourceData) return {};

    const LPVOID binaryData = LockResource(resourceData);
    if (!binaryData) return {};

    // Setup global structure for cabinet callbacks
    struct ExtractContext {
        std::span<BYTE> cabData;
        std::vector<BYTE> output;
        int nextHandle = 1;
        std::map<INT_PTR, size_t> handles;
    } ctx{ { static_cast<BYTE*>(binaryData), SizeofResource(nullptr, resource) } };

    static ExtractContext* g_ctx = nullptr;
    g_ctx = &ctx;

    // Use the cabinet function to decompress the resource
    ERF erf;
    SmartPointer<HFDI> hfdi(FDIDestroy, FDICreate(
        +[](ULONG cb) -> void* { return malloc(cb); },
        +[](void* pv) { free(pv); },
        +[](char*, int, int) -> INT_PTR { return g_ctx->nextHandle++; },
        +[](INT_PTR hf, void* pv, UINT cb) -> UINT {
            size_t& pos = g_ctx->handles[hf];
            const size_t toRead = min(cb, (UINT)(g_ctx->cabData.size() - pos));
            memcpy(pv, g_ctx->cabData.data() + pos, toRead);
            pos += toRead;
            return static_cast<UINT>(toRead);
        },
        +[](INT_PTR, void* pv, UINT cb) -> UINT {
            const char* p = static_cast<const char*>(pv);
            g_ctx->output.insert(g_ctx->output.end(), p, p + cb);
            return cb;
        },
        +[](INT_PTR hf) -> int { g_ctx->handles.erase(hf); return 0; },
        +[](INT_PTR hf, long dist, int seektype) -> long {
            size_t& pos = g_ctx->handles[hf];
            if (seektype == SEEK_SET) pos = dist;
            else if (seektype == SEEK_CUR) pos += dist;
            else pos = g_ctx->cabData.size() + dist;
            return static_cast<long>(pos);
        },
        cpu80386, &erf));

    if (!hfdi) return {};

    FDICopy(hfdi, std::string().data(), std::string().data(), 0,
        +[](FDINOTIFICATIONTYPE fdint, PFDINOTIFICATION) -> INT_PTR {
            return (fdint == fdintCOPY_FILE) ? 1 : (fdint == fdintCLOSE_FILE_INFO);
        }, nullptr, &ctx);

    return ctx.output;
}

// Retrieve the GPL text from our resources
std::wstring GetTextResource(const UINT id)
{
    // Fetch the resource
    const HRSRC hrsrc = ::FindResource(nullptr, MAKEINTRESOURCE(id), L"TEXT");
    if (nullptr == hrsrc) return {};

    // Decompress the resource
    const auto resourceData = GetCompressedResource(hrsrc);
    if (resourceData.empty()) return {};

    return Localization::ConvertToWideString(
        { reinterpret_cast<const char*>(resourceData.data()), resourceData.size()});
}

std::wstring GetAcceleratorString(const UINT commandID)
{
    static std::map<UINT, std::wstring> cache;
    if (!cache.empty())
    {
        return cache[commandID];
    }

    // Load all accelerator object and get count 
    static std::vector<ACCEL> accels;
    HACCEL hAccel = LoadAccelerators(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME));
    const int count = CopyAcceleratorTable(hAccel, nullptr, 0);
    if (count == 0) return L"";

    // Read all from the table
    accels.resize(count);
    ::CopyAcceleratorTable(hAccel, accels.data(), count);

    // Computer strings for all the commands in the table
    for (const auto& [virtKey, key, cmd] : accels)
    {
        auto& result = cache[cmd];
        if (!result.empty()) result += L", ";

        // Build modifier string
        if (virtKey & FCONTROL) result += L"Ctrl+";
        if (virtKey & FALT) result += L"Alt+";
        if (virtKey & FSHIFT) result += L"Shift+";

        // Get key name
        if ((virtKey & FVIRTKEY) == 0)
        {
            result += static_cast<WCHAR>(key);
            continue;
        }

        // Function keys
        if (key >= VK_F1 && key <= VK_F24)
            result += std::format(L"F{}", key - VK_F1 + 1);

        // Letter keys
        else if (key >= 'A' && key <= 'Z')
            result += static_cast<WCHAR>(key);

        // Number keys
        else if (key >= '0' && key <= '9')
            result += static_cast<WCHAR>(key);

        // Special keys
        else switch (key)
        {
            case VK_RETURN:   result += L"Enter"; break;
            case VK_DELETE:   result += L"Del"; break;
            case VK_INSERT:   result += L"Ins"; break;
            case VK_SPACE:    result += L"Space"; break;
            case VK_ADD:      result += L"Num +"; break;
            case VK_SUBTRACT: result += L"Num -"; break;
            case VK_MULTIPLY: result += L"Num *"; break;
            case VK_DIVIDE:   result += L"Num /"; break;
            case VK_OEM_PLUS:  result += L"+"; break;
            case VK_OEM_MINUS: result += L"-"; break;
            default: result += std::format(L"VK_{:X}", key);
        }
    }

    return cache[commandID];
}

// Tree node drawing helper
void DrawTreeNodeConnector(CDC* pdc, const CRect& nodeRect, const COLORREF bgColor,
    const bool toTop, const bool toBottom, const bool toRight, const bool showPlus, const bool showMinus)
{
    const int nodeWidth = nodeRect.Width();
    const int nodeHeight = nodeRect.Height();
    const int centerX = nodeRect.left + nodeWidth / 2;
    const int centerY = nodeRect.top + nodeHeight / 2;

    // Connectors
    static LOGBRUSH lbConn { BS_SOLID, DarkMode::IsDarkModeActive() ? RGB(160,160,160) : RGB(96,96,96), 0 };
    static CPen connPen(PS_GEOMETRIC | PS_DOT, 1, &lbConn);
    CSelectObject soConn(pdc, &connPen);
    if (toBottom && toTop) pdc->MoveTo(centerX, nodeRect.top), pdc->LineTo(centerX, nodeRect.bottom);
    else if (toBottom) pdc->MoveTo(centerX, centerY), pdc->LineTo(centerX, nodeRect.bottom);
    else if (toTop) pdc->MoveTo(centerX, nodeRect.top), pdc->LineTo(centerX, centerY);
    if (toRight) pdc->MoveTo(centerX + 1, centerY), pdc->LineTo(nodeRect.right, centerY);

    if (!(showPlus || showMinus)) return;

    // Outside box
    const int boxSize = nodeHeight / 2 | 1;
    const int boxHalf = boxSize / 2;
    const int boxLeft = centerX - boxHalf;
    const int boxRight = boxLeft + boxSize;
    const int boxTop = centerY - boxHalf;
    const int boxBottom = boxTop + boxSize;
    static LOGBRUSH lbBox { BS_SOLID, DarkMode::IsDarkModeActive() ? RGB(160,160,160) : RGB(96,96,96), 0 };
    static CPen boxPen(PS_GEOMETRIC | PS_ENDCAP_FLAT, 1, &lbBox);
    CBrush bgBox(bgColor);
    CSelectObject soBox(pdc, &boxPen);
    CSelectObject a(pdc, &bgBox);
    pdc->RoundRect(boxLeft, boxTop, boxRight, boxBottom, 2, 2);

    // Minus sign
    const int margin = nodeHeight / 8;
    pdc->MoveTo(boxLeft + margin, centerY), pdc->LineTo(boxRight - margin, centerY);

    // Plus sign
    if (showPlus) pdc->MoveTo(centerX, boxTop + margin), pdc->LineTo(centerX, boxBottom - margin);
}
