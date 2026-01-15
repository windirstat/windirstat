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

#include "pch.h"

// Interface helpers declarations

// Locale and formatting
std::wstring GetLocaleString(LCTYPE lctype, LCID lcid);
std::wstring GetLocaleLanguage(LANGID langid);
wchar_t GetLocaleThousandSeparator() noexcept;
wchar_t GetLocaleDecimalSeparator() noexcept;
std::wstring FormatBytes(ULONGLONG n);
std::wstring FormatSizeSuffixes(ULONGLONG n);
std::wstring FormatCount(ULONGLONG n);
std::wstring FormatDouble(double d);
std::wstring FormatFileTime(const FILETIME& t);
std::wstring FormatAttributes(DWORD attr);
std::wstring FormatMilliseconds(ULONGLONG ms);
std::wstring FormatVolumeNameOfRootPath(const std::wstring& rootPath);
std::wstring FormatVolumeName(const std::wstring& rootPath, const std::wstring& volumeName);

// File and path helpers
std::wstring GetFolderNameFromPath(const std::wstring& path);
std::wstring GetBaseNameFromPath(const std::wstring& path);
std::wstring GlobToRegex(const std::wstring& glob, bool useAnchors = true);

// String helpers
void ReplaceString(std::wstring& subject, const std::wstring& search, const std::wstring& replace);
std::wstring& TrimString(std::wstring& s, wchar_t c = L' ', bool endOnly = false) noexcept;
std::wstring MakeLower(const std::wstring& s);

// Attribute parsing
DWORD ParseAttributes(const std::wstring& attributes) noexcept;

// Size specifiers
const std::wstring& GetSpec_Bytes() noexcept;
const std::wstring& GetSpec_KiB() noexcept;
const std::wstring& GetSpec_MiB() noexcept;
const std::wstring& GetSpec_GiB() noexcept;
const std::wstring& GetSpec_TiB() noexcept;

// System information
std::wstring GetCOMSPEC();
const std::wstring& GetSysDirectory() noexcept;

// UI helpers
void WaitForHandleWithRepainting(HANDLE h, DWORD TimeOut = INFINITE) noexcept;
void ProcessMessagesUntilSignaled(const std::function<void()>& callback);
void DisplayError(const std::wstring& error);
std::wstring TranslateError(HRESULT hr = static_cast<HRESULT>(GetLastError()));
bool ShellExecuteWrapper(const std::wstring& lpFile, const std::wstring& lpParameters = L"",
        const std::wstring& lpVerb = L"", HWND hwnd = *AfxGetMainWnd(),
        const std::wstring& lpDirectory = L"", INT nShowCmd = SW_NORMAL);

// DPI scaling
int DpiRest(int value, const CWnd* wnd = nullptr) noexcept;
int DpiSave(int value, const CWnd* wnd = nullptr) noexcept;

// Context menu
constexpr auto CONTENT_MENU_MINCMD = 0x1ul;
constexpr auto CONTENT_MENU_MAXCMD = 0x7FFFul;
IContextMenu* GetContextMenu(HWND hwnd, const std::vector<std::wstring>& paths);

// Application info
std::wstring GetAppFileName(const std::wstring& ext = L"");
std::wstring GetAppFolder();

// Resources
std::vector<BYTE> GetCompressedResource(HRSRC resource) noexcept;

// Input state
inline bool IsControlKeyDown() noexcept { return (HSHELL_HIGHBIT & GetKeyState(VK_CONTROL)) != 0; };
inline bool IsShiftKeyDown() noexcept { return (HSHELL_HIGHBIT & GetKeyState(VK_SHIFT)) != 0; };

// Tree node drawing helper
void DrawTreeNodeConnector(CDC* pdc, const CRect& nodeRect, COLORREF bgColor,
    bool toTop, bool toBottom, bool toRight, bool showPlus = false, bool showMinus = false);

// Rect struct
using CSmallRect = struct CSmallRect
{
    WORD left = 0, top = 0, right = 0, bottom = 0;

    // Default constructor
    constexpr CSmallRect() noexcept = default;

    // Constructor from CRect
    explicit constexpr CSmallRect(const CRect& rect) noexcept :
        left(static_cast<WORD>(rect.left)), top(static_cast<WORD>(rect.top)),
        right(static_cast<WORD>(rect.right)), bottom(static_cast<WORD>(rect.bottom)) {}

    // Assignment from CRect
    constexpr CSmallRect& operator=(const CRect& rect) noexcept
    {
        left = static_cast<WORD>(rect.left);
        top = static_cast<WORD>(rect.top);
        right = static_cast<WORD>(rect.right);
        bottom = static_cast<WORD>(rect.bottom);
        return *this;
    }

    // Conversion to CRect
    operator CRect() const noexcept
    {
        return { left, top, right, bottom };
    }
};

