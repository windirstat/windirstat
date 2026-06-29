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
// =============================================================================
//  WinDirStatShim.h
//
//  A self-contained, header-only re-implementation of the (sizable) subset of
//  the MFC and MFC-feature-pack API that WinDirStat consumes.  It is backed by
//  the raw Win32 API, the common controls library and GDI.  The goal is to let
//  the rest of the WinDirStat source compile and behave exactly as it did with
//  Microsoft's MFC, but without linking against the MFC libraries at all.
//
//  Light and dark mode are both supported: the CMFC* control surrogates draw
//  themselves and consult the active CMFCVisualManager, which is exactly how
//  DarkMode.cpp drives the dark appearance.
//
//  Layering (top to bottom in this file):
//    1.  Win32 includes + AFX macros/types
//    2.  CString
//    3.  Geometry (CPoint / CSize / CRect)
//    4.  CObject + runtime-class system + exceptions
//    5.  Message maps + CCmdTarget + CCmdUI
//    6.  GDI (CGdiObject / CDC family / CPen / CBrush / CFont / CBitmap / CRgn)
//    7.  CWnd + window maps + DDX / CDataExchange
//    8.  Standard controls + CMenu + CImageList + dialogs + common dialogs
//    9.  CWinThread / CWinApp / CWinAppEx + Afx globals
//   10.  CFrameWnd(Ex) + CSplitterWnd(Ex)
//   11.  CMFC* feature-pack surrogates + visual manager + global data
//   12.  Standard MFC/AFX resource ids
// =============================================================================

#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <richedit.h>
#include <uxtheme.h>
#include <olectl.h>
#include <atlbase.h>   // ATL (not MFC): CComPtr / CComQIPtr / CRegKey
#include <gdiplus.h>

#include <cstdarg>
#include <cstdint>
#include <cwchar>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <algorithm>
#include <type_traits>
#include <mutex>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "uxtheme.lib")
// Libraries MFC used to pull in automatically.
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "powrprof.lib")
#pragma comment(lib, "virtdisk.lib")
#pragma comment(lib, "cabinet.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "winspool.lib")

// windowsx.h / commctrl.h define function-like helper macros whose names collide
// with our CWnd-style member functions.  Undefine the colliding ones.
#ifdef SubclassWindow
#undef SubclassWindow
#endif
#ifdef SubclassDialog
#undef SubclassDialog
#endif
#ifdef GetNextWindow
#undef GetNextWindow
#endif

// MFC message maps pass handlers as bare member names (e.g. ON_COMMAND(id, OnFoo)),
// which relies on the MSVC "address of member without &" extension (warning C4867).
#pragma warning(disable: 4867)

// Enable common-controls v6 (themed controls / list-view custom draw etc.)
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Force a single, fully-general representation for every pointer-to-member so that
// the message-map machinery can reinterpret_cast between member-function-pointer
// types (exactly as MFC does).  Without this, member pointers to classes whose
// definition is not yet seen get a different (smaller) representation and the casts
// fail with "pointers to members have different representations".
#pragma pointers_to_members(full_generality, virtual_inheritance)

// -----------------------------------------------------------------------------
//  AFX-style macros / keywords
// -----------------------------------------------------------------------------
#ifndef afx_msg
#define afx_msg
#endif
#ifndef AFXAPI
#define AFXAPI __stdcall
#endif
#ifndef AFX_CDECL
#define AFX_CDECL __cdecl
#endif
#ifndef AFX_EXT_CLASS
#define AFX_EXT_CLASS
#endif

#ifndef ASSERT
#ifdef _DEBUG
#define ASSERT(f) ((void)(!!(f) || (_wassert(_CRT_WIDE(#f), _CRT_WIDE(__FILE__), (unsigned)(__LINE__)), 0)))
#else
#define ASSERT(f) ((void)0)
#endif
#endif

inline int WdsDpiForWindow(HWND hWnd) noexcept
{
    HDC hdc = ::GetDC(hWnd);
    const int dpi = hdc != nullptr ? ::GetDeviceCaps(hdc, LOGPIXELSX) : 96;
    if (hdc != nullptr) ::ReleaseDC(hWnd, hdc);
    return dpi > 0 ? dpi : 96;
}

inline int WdsDpiScale(int value, HWND hWnd = nullptr) noexcept
{
    return ::MulDiv(value, WdsDpiForWindow(hWnd), 96);
}

#ifndef VERIFY
#ifdef _DEBUG
#define VERIFY(f) ASSERT(f)
#else
#define VERIFY(f) ((void)(f))
#endif
#endif

#ifndef ASSERT_VALID
#define ASSERT_VALID(p) ASSERT((p) != nullptr)
#endif
#ifndef ENSURE
#define ENSURE(p) ASSERT(p)
#endif
#ifndef ENSURE_ARG
#define ENSURE_ARG(p) ASSERT(p)
#endif

#ifndef TRACE
#ifdef _DEBUG
#define TRACE(...) ((void)0)
#else
#define TRACE(...) ((void)0)
#endif
#endif

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) ((void)(P))
#endif

#ifndef DEBUG_ONLY
#ifdef _DEBUG
#define DEBUG_ONLY(f) (f)
#else
#define DEBUG_ONLY(f) ((void)0)
#endif
#endif

// MFC POSITION handle (opaque)
struct __POSITION {};
using POSITION = __POSITION*;

// Forward declarations of the few Afx helpers needed early.
class CWnd;
HINSTANCE AFXAPI AfxGetResourceHandle();
HINSTANCE AFXAPI AfxGetInstanceHandle();
CWnd* AFXAPI AfxGetMainWnd();

// Standard MFC command ids / constants referenced by the shim itself (full set near EOF).
#ifndef ID_APPLY_NOW
inline constexpr UINT ID_APPLY_NOW = 0x3021;
#endif
#ifndef TBBS_DISABLED
inline constexpr UINT TBBS_CHECKED       = 0x0001;
inline constexpr UINT TBBS_DISABLED      = 0x0002;
inline constexpr UINT TBBS_INDETERMINATE = 0x0004;
inline constexpr UINT TBBS_BUTTON        = 0x0008;
inline constexpr UINT TBBS_SEPARATOR     = 0x0010;
#endif
#ifndef ULONGLONG_MAX
inline constexpr ULONGLONG ULONGLONG_MAX = 0xffffffffffffffffULL;
#endif
#ifndef RT_DLGINIT
#define RT_DLGINIT MAKEINTRESOURCEW(240)  // pointer cast — cannot be constexpr
#endif

// -----------------------------------------------------------------------------
//  CStringT — minimal MFC/ATL CString work-alike backed by std::basic_string
// -----------------------------------------------------------------------------
template <typename CharT>
class CStringT final
{
public:
    using XCHAR  = CharT;
    using PXSTR  = CharT*;
    using PCXSTR = const CharT*;

    CStringT() = default;
    CStringT(const CStringT& s) : m_str(s.m_str) {}
    CStringT(CStringT&& s) noexcept : m_str(std::move(s.m_str)) {}
    CStringT(const std::basic_string<CharT>& s) : m_str(s) {}
    CStringT(std::basic_string<CharT>&& s) noexcept : m_str(std::move(s)) {}
    CStringT(std::basic_string_view<CharT> s) : m_str(s) {}

    CStringT(PCXSTR psz)
    {
        if (psz == nullptr) return;
        if (IS_INTRESOURCE(psz)) { LoadString(static_cast<UINT>(reinterpret_cast<ULONG_PTR>(psz))); return; }
        m_str = psz;
    }
    CStringT(PCXSTR psz, int nLength) { if (psz && nLength > 0) m_str.assign(psz, static_cast<size_t>(nLength)); }
    CStringT(CharT ch, int nRepeat = 1) { if (nRepeat > 0) m_str.assign(static_cast<size_t>(nRepeat), ch); }

    CStringT& operator=(const CStringT& s) { m_str = s.m_str; return *this; }
    CStringT& operator=(CStringT&& s) noexcept { m_str = std::move(s.m_str); return *this; }
    CStringT& operator=(PCXSTR psz) { m_str = psz ? psz : std::basic_string<CharT>(); return *this; }
    CStringT& operator=(const std::basic_string<CharT>& s) { m_str = s; return *this; }
    CStringT& operator=(CharT ch) { m_str.assign(1, ch); return *this; }

    operator PCXSTR() const noexcept { return m_str.c_str(); }
    PCXSTR GetString() const noexcept { return m_str.c_str(); }
    const std::basic_string<CharT>& Str() const noexcept { return m_str; }

    int  GetLength() const noexcept { return static_cast<int>(m_str.size()); }
    bool IsEmpty() const noexcept { return m_str.empty(); }
    void Empty() noexcept { m_str.clear(); }
    CharT GetAt(int i) const noexcept { return m_str[static_cast<size_t>(i)]; }
    CharT operator[](int i) const noexcept { return m_str[static_cast<size_t>(i)]; }

    CStringT& operator+=(const CStringT& s) { m_str += s.m_str; return *this; }
    CStringT& operator+=(PCXSTR psz) { if (psz) m_str += psz; return *this; }
    CStringT& operator+=(CharT ch) { m_str += ch; return *this; }
    void Append(PCXSTR psz) { if (psz) m_str += psz; }
    void AppendChar(CharT ch) { m_str += ch; }
    int Insert(int i, PCXSTR psz) { if (!psz) return GetLength(); i = std::clamp(i, 0, GetLength()); m_str.insert(static_cast<size_t>(i), psz); return GetLength(); }
    int Insert(int i, CharT ch) { i = std::clamp(i, 0, GetLength()); m_str.insert(static_cast<size_t>(i), 1, ch); return GetLength(); }
    int Delete(int i, int count = 1) { if (i < 0 || i >= GetLength() || count <= 0) return GetLength(); m_str.erase(static_cast<size_t>(i), static_cast<size_t>(count)); return GetLength(); }

    CStringT Left(int n) const { n = std::clamp(n, 0, GetLength()); return CStringT(m_str.substr(0, static_cast<size_t>(n))); }
    CStringT Right(int n) const { n = std::clamp(n, 0, GetLength()); return CStringT(m_str.substr(m_str.size() - static_cast<size_t>(n))); }
    CStringT Mid(int first) const { if (first < 0) first = 0; if (first >= GetLength()) return {}; return CStringT(m_str.substr(static_cast<size_t>(first))); }
    CStringT Mid(int first, int count) const { if (first < 0) first = 0; if (first >= GetLength() || count <= 0) return {}; return CStringT(m_str.substr(static_cast<size_t>(first), static_cast<size_t>(count))); }

    int Find(CharT ch, int start = 0) const { const auto p = m_str.find(ch, static_cast<size_t>(std::max(0, start))); return p == std::basic_string<CharT>::npos ? -1 : static_cast<int>(p); }
    int Find(PCXSTR psz, int start = 0) const { if (!psz) return -1; const auto p = m_str.find(psz, static_cast<size_t>(std::max(0, start))); return p == std::basic_string<CharT>::npos ? -1 : static_cast<int>(p); }

    void MakeLower() { std::transform(m_str.begin(), m_str.end(), m_str.begin(), [](CharT c){ return static_cast<CharT>(::tolower(c)); }); }
    void MakeUpper() { std::transform(m_str.begin(), m_str.end(), m_str.begin(), [](CharT c){ return static_cast<CharT>(::toupper(c)); }); }

    void TrimRight() { while (!m_str.empty() && (m_str.back() == ' ' || m_str.back() == '\t' || m_str.back() == '\r' || m_str.back() == '\n')) m_str.pop_back(); }
    void TrimRight(CharT ch) { while (!m_str.empty() && m_str.back() == ch) m_str.pop_back(); }
    void TrimRight(PCXSTR chars) { if (!chars) return; std::basic_string_view<CharT> s(chars); while (!m_str.empty() && s.find(m_str.back()) != std::basic_string_view<CharT>::npos) m_str.pop_back(); }
    void TrimLeft()  { size_t i = 0; while (i < m_str.size() && (m_str[i] == ' ' || m_str[i] == '\t' || m_str[i] == '\r' || m_str[i] == '\n')) ++i; m_str.erase(0, i); }
    void TrimLeft(CharT ch) { size_t i = 0; while (i < m_str.size() && m_str[i] == ch) ++i; m_str.erase(0, i); }
    void TrimLeft(PCXSTR chars) { if (!chars) return; std::basic_string_view<CharT> s(chars); size_t i = 0; while (i < m_str.size() && s.find(m_str[i]) != std::basic_string_view<CharT>::npos) ++i; m_str.erase(0, i); }
    void Trim() { TrimRight(); TrimLeft(); }
    void Trim(CharT ch) { TrimRight(ch); TrimLeft(ch); }

    PXSTR GetBuffer(int nMinLen = 0)
    {
        if (nMinLen > GetLength()) m_str.resize(static_cast<size_t>(nMinLen));
        return m_str.data();
    }
    void ReleaseBuffer(int nNewLen = -1)
    {
        if (nNewLen < 0) m_str.resize(std::char_traits<CharT>::length(m_str.c_str()));
        else m_str.resize(static_cast<size_t>(nNewLen));
    }

    BOOL LoadString(UINT nID);
    BOOL LoadString(HINSTANCE hInst, UINT nID, WORD = 0)
    {
        PCXSTR p = nullptr; int len;
        if constexpr (std::is_same_v<CharT, wchar_t>) len = ::LoadStringW(hInst, nID, reinterpret_cast<LPWSTR>(&p), 0);
        else len = ::LoadStringA(hInst, nID, reinterpret_cast<LPSTR>(&p), 0);
        if (len <= 0) { m_str.clear(); return FALSE; }
        m_str.assign(p, static_cast<size_t>(len));
        return TRUE;
    }

    void __cdecl Format(PCXSTR fmt, ...);
    void __cdecl AppendFormat(PCXSTR fmt, ...);

    friend CStringT operator+(const CStringT& a, const CStringT& b) { return CStringT(a.m_str + b.m_str); }
    friend CStringT operator+(const CStringT& a, PCXSTR b) { return CStringT(b ? a.m_str + b : a.m_str); }
    friend CStringT operator+(PCXSTR a, const CStringT& b) { return CStringT(a ? a + b.m_str : b.m_str); }
    friend CStringT operator+(const CStringT& a, CharT b) { return CStringT(a.m_str + b); }

    bool operator==(const CStringT& o) const { return m_str == o.m_str; }
    bool operator==(PCXSTR o) const { return o && m_str == o; }

private:
    std::basic_string<CharT> m_str;
};

template <>
inline BOOL CStringT<wchar_t>::LoadString(UINT nID)
{
    const WCHAR* p = nullptr;
    const int len = ::LoadStringW(AfxGetResourceHandle(), nID, reinterpret_cast<LPWSTR>(&p), 0);
    if (len <= 0) { m_str.clear(); return FALSE; }
    m_str.assign(p, static_cast<size_t>(len));
    return TRUE;
}
template <>
inline BOOL CStringT<char>::LoadString(UINT nID)
{
    const CHAR* p = nullptr;
    const int len = ::LoadStringA(AfxGetResourceHandle(), nID, reinterpret_cast<LPSTR>(&p), 0);
    if (len <= 0) { m_str.clear(); return FALSE; }
    m_str.assign(p, static_cast<size_t>(len));
    return TRUE;
}

template <>
inline void __cdecl CStringT<wchar_t>::Format(PCXSTR fmt, ...)
{
    va_list args; va_start(args, fmt);
    const int n = _vscwprintf(fmt, args); va_end(args);
    if (n < 0) { m_str.clear(); return; }
    m_str.resize(static_cast<size_t>(n));
    va_start(args, fmt);
    _vsnwprintf_s(m_str.data(), static_cast<size_t>(n) + 1, _TRUNCATE, fmt, args);
    va_end(args);
}
template <>
inline void __cdecl CStringT<char>::Format(PCXSTR fmt, ...)
{
    va_list args; va_start(args, fmt);
    const int n = _vscprintf(fmt, args); va_end(args);
    if (n < 0) { m_str.clear(); return; }
    m_str.resize(static_cast<size_t>(n));
    va_start(args, fmt);
    _vsnprintf_s(m_str.data(), static_cast<size_t>(n) + 1, _TRUNCATE, fmt, args);
    va_end(args);
}
template <>
inline void __cdecl CStringT<wchar_t>::AppendFormat(PCXSTR fmt, ...)
{
    va_list args; va_start(args, fmt);
    const int n = _vscwprintf(fmt, args); va_end(args);
    if (n < 0) return;
    std::wstring tmp(static_cast<size_t>(n), L'\0');
    va_start(args, fmt);
    _vsnwprintf_s(tmp.data(), static_cast<size_t>(n) + 1, _TRUNCATE, fmt, args);
    va_end(args);
    m_str += tmp;
}
template <>
inline void __cdecl CStringT<char>::AppendFormat(PCXSTR fmt, ...)
{
    va_list args; va_start(args, fmt);
    const int n = _vscprintf(fmt, args); va_end(args);
    if (n < 0) return;
    std::string tmp(static_cast<size_t>(n), '\0');
    va_start(args, fmt);
    _vsnprintf_s(tmp.data(), static_cast<size_t>(n) + 1, _TRUNCATE, fmt, args);
    va_end(args);
    m_str += tmp;
}

using CStringW = CStringT<wchar_t>;
using CStringA = CStringT<char>;
using CString  = CStringW;

// -----------------------------------------------------------------------------
//  Geometry: CSize / CPoint / CRect
// -----------------------------------------------------------------------------
class CSize final : public SIZE
{
public:
    CSize() noexcept { cx = cy = 0; }
    CSize(int x, int y) noexcept { cx = x; cy = y; }
    CSize(SIZE s) noexcept { cx = s.cx; cy = s.cy; }
    CSize(POINT p) noexcept { cx = p.x; cy = p.y; }
    CSize(DWORD dw) noexcept { cx = static_cast<short>(LOWORD(dw)); cy = static_cast<short>(HIWORD(dw)); }
    bool operator==(const CSize& s) const noexcept { return cx == s.cx && cy == s.cy; }
    bool operator==(SIZE s) const noexcept { return cx == s.cx && cy == s.cy; }
    CSize operator+(SIZE s) const noexcept { return { cx + s.cx, cy + s.cy }; }
    CSize operator-(SIZE s) const noexcept { return { cx - s.cx, cy - s.cy }; }
    CSize operator-() const noexcept { return { -cx, -cy }; }
};

class CPoint final : public POINT
{
public:
    CPoint() noexcept { x = y = 0; }
    CPoint(int xx, int yy) noexcept { x = xx; y = yy; }
    CPoint(POINT p) noexcept { x = p.x; y = p.y; }
    CPoint(SIZE s) noexcept { x = s.cx; y = s.cy; }
    CPoint(DWORD dw) noexcept { x = static_cast<short>(LOWORD(dw)); y = static_cast<short>(HIWORD(dw)); }
    CPoint(LPARAM lp) noexcept { x = static_cast<short>(LOWORD(lp)); y = static_cast<short>(HIWORD(lp)); }
    void Offset(int dx, int dy) noexcept { x += dx; y += dy; }
    void Offset(POINT p) noexcept { x += p.x; y += p.y; }
    void Offset(SIZE s) noexcept { x += s.cx; y += s.cy; }
    bool operator==(const CPoint& p) const noexcept { return x == p.x && y == p.y; }
    bool operator==(POINT p) const noexcept { return x == p.x && y == p.y; }
    CPoint operator+(SIZE s) const noexcept { return { x + s.cx, y + s.cy }; }
    CPoint operator+(POINT p) const noexcept { return { x + p.x, y + p.y }; }
    CSize  operator-(POINT p) const noexcept { return { x - p.x, y - p.y }; }
    CPoint operator-(SIZE s) const noexcept { return { x - s.cx, y - s.cy }; }
    CPoint operator-() const noexcept { return { -x, -y }; }
    CPoint& operator+=(SIZE s) noexcept { x += s.cx; y += s.cy; return *this; }
    CPoint& operator-=(SIZE s) noexcept { x -= s.cx; y -= s.cy; return *this; }
    CPoint& operator+=(POINT p) noexcept { x += p.x; y += p.y; return *this; }
};

class CRect final : public RECT
{
public:
    CRect() noexcept { left = top = right = bottom = 0; }
    CRect(int l, int t, int r, int b) noexcept { left = l; top = t; right = r; bottom = b; }
    CRect(const RECT& r) noexcept { left = r.left; top = r.top; right = r.right; bottom = r.bottom; }
    CRect(POINT topLeft, POINT bottomRight) noexcept { left = topLeft.x; top = topLeft.y; right = bottomRight.x; bottom = bottomRight.y; }
    CRect(POINT p, SIZE s) noexcept { left = p.x; top = p.y; right = p.x + s.cx; bottom = p.y + s.cy; }

    int Width()  const noexcept { return right - left; }
    int Height() const noexcept { return bottom - top; }
    CSize Size() const noexcept { return { Width(), Height() }; }
    CPoint TopLeft() const noexcept { return { left, top }; }
    CPoint BottomRight() const noexcept { return { right, bottom }; }
    CPoint CenterPoint() const noexcept { return { (left + right) / 2, (top + bottom) / 2 }; }
    bool IsRectEmpty() const noexcept { return ::IsRectEmpty(this) != FALSE; }
    bool IsRectNull() const noexcept { return left == 0 && top == 0 && right == 0 && bottom == 0; }

    operator LPRECT() noexcept { return this; }
    operator LPCRECT() const noexcept { return this; }

    void SetRect(int l, int t, int r, int b) noexcept { left = l; top = t; right = r; bottom = b; }
    void SetRectEmpty() noexcept { left = top = right = bottom = 0; }
    void OffsetRect(int dx, int dy) noexcept { ::OffsetRect(this, dx, dy); }
    void OffsetRect(SIZE s) noexcept { ::OffsetRect(this, s.cx, s.cy); }
    void OffsetRect(POINT p) noexcept { ::OffsetRect(this, p.x, p.y); }
    void InflateRect(int dx, int dy) noexcept { ::InflateRect(this, dx, dy); }
    void InflateRect(int l, int t, int r, int b) noexcept { left -= l; top -= t; right += r; bottom += b; }
    void InflateRect(SIZE s) noexcept { ::InflateRect(this, s.cx, s.cy); }
    void InflateRect(LPCRECT lp) noexcept { left -= lp->left; top -= lp->top; right += lp->right; bottom += lp->bottom; }
    void DeflateRect(int dx, int dy) noexcept { ::InflateRect(this, -dx, -dy); }
    void DeflateRect(int l, int t, int r, int b) noexcept { left += l; top += t; right -= r; bottom -= b; }
    void DeflateRect(SIZE s) noexcept { ::InflateRect(this, -s.cx, -s.cy); }
    void DeflateRect(LPCRECT lp) noexcept { left += lp->left; top += lp->top; right -= lp->right; bottom -= lp->bottom; }
    void NormalizeRect() noexcept { if (left > right) std::swap(left, right); if (top > bottom) std::swap(top, bottom); }
    BOOL PtInRect(POINT p) const noexcept { return ::PtInRect(this, p); }
    BOOL IntersectRect(LPCRECT a, LPCRECT b) noexcept { return ::IntersectRect(this, a, b); }
    BOOL UnionRect(LPCRECT a, LPCRECT b) noexcept { return ::UnionRect(this, a, b); }
    void MoveToXY(int x, int y) noexcept { ::OffsetRect(this, x - left, y - top); }
    void MoveToXY(POINT p) noexcept { MoveToXY(p.x, p.y); }

    bool operator==(const CRect& r) const noexcept { return ::EqualRect(this, &r) != FALSE; }
    bool operator==(const RECT& r) const noexcept { return ::EqualRect(this, &r) != FALSE; }
    CRect& operator=(const RECT& r) noexcept { left = r.left; top = r.top; right = r.right; bottom = r.bottom; return *this; }
    CRect& operator+=(POINT p) noexcept { ::OffsetRect(this, p.x, p.y); return *this; }
    CRect& operator-=(POINT p) noexcept { ::OffsetRect(this, -p.x, -p.y); return *this; }
    CRect& operator+=(SIZE s) noexcept { ::OffsetRect(this, s.cx, s.cy); return *this; }
    CRect& operator-=(SIZE s) noexcept { ::OffsetRect(this, -s.cx, -s.cy); return *this; }
    CRect operator+(POINT p) const noexcept { CRect r(*this); ::OffsetRect(&r, p.x, p.y); return r; }
    CRect operator-(POINT p) const noexcept { CRect r(*this); ::OffsetRect(&r, -p.x, -p.y); return r; }
    CRect operator+(SIZE s) const noexcept { CRect r(*this); ::OffsetRect(&r, s.cx, s.cy); return r; }
    CRect operator-(SIZE s) const noexcept { CRect r(*this); ::OffsetRect(&r, -s.cx, -s.cy); return r; }
};

// -----------------------------------------------------------------------------
//  CObject + runtime-class system
// -----------------------------------------------------------------------------
class CObject;

struct CRuntimeClass
{
    LPCSTR        m_lpszClassName;
    int           m_nObjectSize;
    UINT          m_wSchema;
    CObject*      (__stdcall* m_pfnCreateObject)();
    CRuntimeClass* m_pBaseClass;

    CObject* CreateObject() const;
    BOOL IsDerivedFrom(const CRuntimeClass* pBaseClass) const noexcept
    {
        for (const CRuntimeClass* p = this; p != nullptr; p = p->m_pBaseClass)
            if (p == pBaseClass) return TRUE;
        return FALSE;
    }
};

class CObject
{
public:
    CObject() = default;
    CObject(const CObject&) = delete;
    CObject& operator=(const CObject&) = delete;
    virtual ~CObject() = default;

    virtual CRuntimeClass* GetRuntimeClass() const;
    BOOL IsKindOf(const CRuntimeClass* pClass) const
    {
        return GetRuntimeClass()->IsDerivedFrom(pClass);
    }

    static const CRuntimeClass classCObject;
};

inline const CRuntimeClass CObject::classCObject =
    { "CObject", static_cast<int>(sizeof(CObject)), 0xFFFF, nullptr, nullptr };
inline CRuntimeClass* CObject::GetRuntimeClass() const
    { return const_cast<CRuntimeClass*>(&classCObject); }

inline CObject* CRuntimeClass::CreateObject() const
    { return m_pfnCreateObject ? (*m_pfnCreateObject)() : nullptr; }

#define RUNTIME_CLASS(class_name) ((CRuntimeClass*)(&class_name::class##class_name))

#define DECLARE_DYNAMIC(class_name) \
public: \
    static const CRuntimeClass class##class_name; \
    CRuntimeClass* GetRuntimeClass() const override;

#define DECLARE_DYNCREATE(class_name) \
public: \
    static const CRuntimeClass class##class_name; \
    CRuntimeClass* GetRuntimeClass() const override; \
    static CObject* __stdcall CreateObject();

#define IMPLEMENT_DYNAMIC(class_name, base_class_name) \
    const CRuntimeClass class_name::class##class_name = \
        { #class_name, static_cast<int>(sizeof(class_name)), 0xFFFF, nullptr, RUNTIME_CLASS(base_class_name) }; \
    CRuntimeClass* class_name::GetRuntimeClass() const { return RUNTIME_CLASS(class_name); }

#define IMPLEMENT_DYNCREATE(class_name, base_class_name) \
    CObject* __stdcall class_name::CreateObject() { return new class_name; } \
    const CRuntimeClass class_name::class##class_name = \
        { #class_name, static_cast<int>(sizeof(class_name)), 0xFFFF, class_name::CreateObject, RUNTIME_CLASS(base_class_name) }; \
    CRuntimeClass* class_name::GetRuntimeClass() const { return RUNTIME_CLASS(class_name); }

// Inline runtime-class definition for shim-internal classes (header-only).
#define SHIM_IMPLEMENT_DYNAMIC_INLINE(class_name, base_ptr) \
    inline const CRuntimeClass class_name::class##class_name = \
        { #class_name, static_cast<int>(sizeof(class_name)), 0xFFFF, nullptr, base_ptr }; \
    inline CRuntimeClass* class_name::GetRuntimeClass() const { return RUNTIME_CLASS(class_name); }

inline CObject* AfxDynamicDownCast(CRuntimeClass* pClass, CObject* pObject)
{
    return (pObject != nullptr && pObject->IsKindOf(pClass)) ? pObject : nullptr;
}
#define DYNAMIC_DOWNCAST(class_name, pObject) \
    (static_cast<class_name*>(AfxDynamicDownCast(RUNTIME_CLASS(class_name), pObject)))
#define STATIC_DOWNCAST(class_name, pObject) (static_cast<class_name*>(pObject))

// -----------------------------------------------------------------------------
//  Exceptions
// -----------------------------------------------------------------------------
class CException : public CObject
{
public:
    CException() = default;
    CException(const CException&) noexcept {}
    CException& operator=(const CException&) noexcept { return *this; }
};
class CSimpleException : public CException {};
class CUserException final : public CSimpleException {};
class CMemoryException final : public CSimpleException {};

[[noreturn]] inline void AFXAPI AfxThrowUserException() { throw CUserException{}; }
[[noreturn]] inline void AFXAPI AfxThrowMemoryException() { throw CMemoryException{}; }

// -----------------------------------------------------------------------------
//  Message-map infrastructure
// -----------------------------------------------------------------------------
class CCmdTarget;
class CCmdUI;
class CWnd;
class CDC;
class CMenu;
class CScrollBar;
class CBitmap;
class CPen;
class CBrush;
class CFont;
class CRgn;
class CGdiObject;

using AFX_PMSG  = void (CCmdTarget::*)();
using AfxThunk  = LRESULT (*)(CCmdTarget* pTarget, AFX_PMSG pfn, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

struct AFX_MSGMAP_ENTRY
{
    UINT        nMessage;   // windows message (or WM_COMMAND / WM_NOTIFY family marker)
    UINT        nCode;      // control / notification code (or command-UI marker)
    UINT        nID;        // control id (first id for ranges)
    UINT        nLastID;    // last id (ranges)
    AFX_PMSG    pfn;        // pointer to member (reinterpret_cast)
    AfxThunk    pThunk;     // type-erased invoker
    const UINT* pReg;       // ON_REGISTERED_MESSAGE: address of message variable
};

struct AFX_MSGMAP
{
    const AFX_MSGMAP* (__stdcall* pfnGetBaseMap)();
    const AFX_MSGMAP_ENTRY* lpEntries;
};

struct AFX_CMDHANDLERINFO { CCmdTarget* pTarget; void* pmf; };

// Command-UI notification code (matches MFC's CN_UPDATE_COMMAND_UI sentinel).
constexpr UINT WdsCN_COMMAND = 0;
constexpr UINT WdsCN_UPDATE_COMMAND_UI = static_cast<UINT>(-1);
constexpr UINT WdsReflectID = static_cast<UINT>(-1);   // nID sentinel for reflected handlers

#define DECLARE_MESSAGE_MAP() \
protected: \
    static const AFX_MSGMAP* __stdcall _GetBaseMessageMap(); \
    static const AFX_MSGMAP* GetThisMessageMap(); \
    const AFX_MSGMAP* GetMessageMap() const override; \
protected:

#define BEGIN_MESSAGE_MAP(theClass, baseClass) \
    const AFX_MSGMAP* theClass::GetMessageMap() const { return GetThisMessageMap(); } \
    const AFX_MSGMAP* __stdcall theClass::_GetBaseMessageMap() { return baseClass::GetThisMessageMap(); } \
    const AFX_MSGMAP* theClass::GetThisMessageMap() { \
        using ThisClass = theClass; \
        (void)sizeof(ThisClass); \
        static const AFX_MSGMAP_ENTRY _messageEntries[] = {

#define END_MESSAGE_MAP() \
            { 0, 0, 0, 0, nullptr, nullptr, nullptr } \
        }; \
        static const AFX_MSGMAP messageMap = { &_GetBaseMessageMap, &_messageEntries[0] }; \
        return &messageMap; \
    }

// Helper to cast a typed member-function pointer into the generic AFX_PMSG slot.
// Variadic so the signature type (which contains commas in its parameter list)
// is not split into multiple macro arguments by the preprocessor.
#define WDS_PMSG(memberFxn, ...) \
    reinterpret_cast<AFX_PMSG>(static_cast<__VA_ARGS__>(memberFxn))

// ---- forward decls of the (late-defined) thunks --------------------------------
LRESULT WdsThunk_Command   (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);
LRESULT WdsThunk_CommandU   (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);   // void(UINT)
LRESULT WdsThunk_UpdateUI    (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // void(CCmdUI*)
LRESULT WdsThunk_Message     (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // LRESULT(WPARAM,LPARAM)
LRESULT WdsThunk_Notify      (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // void(NMHDR*,LRESULT*)
LRESULT WdsThunk_NotifyRange (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // void(UINT,NMHDR*,LRESULT*)
LRESULT WdsThunk_NotifyEx    (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // BOOL(UINT,NMHDR*,LRESULT*)
LRESULT WdsThunk_Create      (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);
LRESULT WdsThunk_Void        (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // void()
LRESULT WdsThunk_Size        (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);
LRESULT WdsThunk_EraseBkgnd  (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);
LRESULT WdsThunk_MouseBtn    (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // void(UINT,CPoint)
LRESULT WdsThunk_MouseWheel  (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // BOOL(UINT,short,CPoint)
LRESULT WdsThunk_Key         (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // void(UINT,UINT,UINT)
LRESULT WdsThunk_Focus       (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // void(CWnd*)
LRESULT WdsThunk_ContextMenu (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // void(CWnd*,CPoint)
LRESULT WdsThunk_Timer       (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // void(UINT_PTR)
LRESULT WdsThunk_InitMenuPopup(CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&); // void(CMenu*,UINT,BOOL)
LRESULT WdsThunk_PowerBroadcast(CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);// UINT(UINT,LPARAM)
LRESULT WdsThunk_NcActivate  (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // BOOL(BOOL)
LRESULT WdsThunk_CtlColor    (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // HBRUSH(CDC*,CWnd*,UINT)
LRESULT WdsThunk_NcHitTest   (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // LRESULT(CPoint)
LRESULT WdsThunk_GetMinMaxInfo(CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&); // void(MINMAXINFO*)
LRESULT WdsThunk_Enable      (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // void(BOOL)
LRESULT WdsThunk_SetCursor   (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // BOOL(CWnd*,UINT,UINT)
LRESULT WdsThunk_ActivateApp (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // void(BOOL,DWORD)
LRESULT WdsThunk_CaptureChanged(CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);// void(CWnd*)
LRESULT WdsThunk_ShowWindow  (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // void(BOOL,UINT)
LRESULT WdsThunk_Scroll      (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // void(UINT,UINT,CScrollBar*)
LRESULT WdsThunk_GetDlgCode  (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // UINT()
LRESULT WdsThunk_NcCalcSize  (CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&);  // void(BOOL,NCCALCSIZE_PARAMS*)
LRESULT WdsThunk_MouseActivate(CCmdTarget*, AFX_PMSG, WPARAM, LPARAM, BOOL&); // int(CWnd*,UINT,UINT)

// ---- command / message / notify macros ----------------------------------------
#define ON_COMMAND(id, memberFxn) \
    { WM_COMMAND, WdsCN_COMMAND, (UINT)(id), (UINT)(id), \
      WDS_PMSG(memberFxn, void (ThisClass::*)()), &WdsThunk_Command, nullptr },
#define ON_COMMAND_RANGE(id0, id1, memberFxn) \
    { WM_COMMAND, WdsCN_COMMAND, (UINT)(id0), (UINT)(id1), \
      WDS_PMSG(memberFxn, void (ThisClass::*)(UINT)), &WdsThunk_CommandU, nullptr },
#define ON_UPDATE_COMMAND_UI(id, memberFxn) \
    { WM_COMMAND, WdsCN_UPDATE_COMMAND_UI, (UINT)(id), (UINT)(id), \
      WDS_PMSG(memberFxn, void (ThisClass::*)(CCmdUI*)), &WdsThunk_UpdateUI, nullptr },
#define ON_UPDATE_COMMAND_UI_RANGE(id0, id1, memberFxn) \
    { WM_COMMAND, WdsCN_UPDATE_COMMAND_UI, (UINT)(id0), (UINT)(id1), \
      WDS_PMSG(memberFxn, void (ThisClass::*)(CCmdUI*)), &WdsThunk_UpdateUI, nullptr },
#define ON_CONTROL(wCode, id, memberFxn) \
    { WM_COMMAND, (UINT)(wCode), (UINT)(id), (UINT)(id), \
      WDS_PMSG(memberFxn, void (ThisClass::*)()), &WdsThunk_Command, nullptr },
#define ON_CONTROL_RANGE(wCode, id0, id1, memberFxn) \
    { WM_COMMAND, (UINT)(wCode), (UINT)(id0), (UINT)(id1), \
      WDS_PMSG(memberFxn, void (ThisClass::*)(UINT)), &WdsThunk_CommandU, nullptr },
#define ON_BN_CLICKED(id, memberFxn)      ON_CONTROL(BN_CLICKED, id, memberFxn)
#define ON_BN_DOUBLECLICKED(id, memberFxn) ON_CONTROL(BN_DOUBLECLICKED, id, memberFxn)
#define ON_STN_CLICKED(id, memberFxn)     ON_CONTROL(STN_CLICKED, id, memberFxn)
#define ON_EN_CHANGE(id, memberFxn)       ON_CONTROL(EN_CHANGE, id, memberFxn)
#define ON_CBN_SELENDOK(id, memberFxn)    ON_CONTROL(CBN_SELENDOK, id, memberFxn)
#define ON_CBN_SELCHANGE(id, memberFxn)   ON_CONTROL(CBN_SELCHANGE, id, memberFxn)
#define ON_CBN_EDITCHANGE(id, memberFxn)  ON_CONTROL(CBN_EDITCHANGE, id, memberFxn)
#define ON_LBN_SELCHANGE(id, memberFxn)   ON_CONTROL(LBN_SELCHANGE, id, memberFxn)

#define ON_MESSAGE(message, memberFxn) \
    { (UINT)(message), 0, 0, 0, \
      WDS_PMSG(memberFxn, LRESULT (ThisClass::*)(WPARAM, LPARAM)), &WdsThunk_Message, nullptr },
#define ON_REGISTERED_MESSAGE(nMessageVariable, memberFxn) \
    { 0, 0, 0, 0, \
      WDS_PMSG(memberFxn, LRESULT (ThisClass::*)(WPARAM, LPARAM)), &WdsThunk_Message, &(nMessageVariable) },

#define ON_NOTIFY(wCode, id, memberFxn) \
    { WM_NOTIFY, (UINT)(wCode), (UINT)(id), (UINT)(id), \
      WDS_PMSG(memberFxn, void (ThisClass::*)(NMHDR*, LRESULT*)), &WdsThunk_Notify, nullptr },
#define ON_NOTIFY_RANGE(wCode, id0, id1, memberFxn) \
    { WM_NOTIFY, (UINT)(wCode), (UINT)(id0), (UINT)(id1), \
      WDS_PMSG(memberFxn, void (ThisClass::*)(UINT, NMHDR*, LRESULT*)), &WdsThunk_NotifyRange, nullptr },
#define ON_NOTIFY_EX(wCode, id, memberFxn) \
    { WM_NOTIFY, (UINT)(wCode), (UINT)(id), (UINT)(id), \
      WDS_PMSG(memberFxn, BOOL (ThisClass::*)(UINT, NMHDR*, LRESULT*)), &WdsThunk_NotifyEx, nullptr },
#define ON_NOTIFY_REFLECT(wCode, memberFxn) \
    { WM_NOTIFY, (UINT)(wCode), WdsReflectID, WdsReflectID, \
      WDS_PMSG(memberFxn, void (ThisClass::*)(NMHDR*, LRESULT*)), &WdsThunk_Notify, nullptr },
#define ON_CONTROL_REFLECT(wCode, memberFxn) \
    { WM_COMMAND, (UINT)(wCode), WdsReflectID, WdsReflectID, \
      WDS_PMSG(memberFxn, void (ThisClass::*)()), &WdsThunk_Command, nullptr },

// ---- window-message macros -----------------------------------------------------
#define ON_WM_CREATE() \
    { WM_CREATE, 0, 0, 0, WDS_PMSG(OnCreate, int (ThisClass::*)(LPCREATESTRUCT)), &WdsThunk_Create, nullptr },
#define ON_WM_DESTROY() \
    { WM_DESTROY, 0, 0, 0, WDS_PMSG(OnDestroy, void (ThisClass::*)()), &WdsThunk_Void, nullptr },
#define ON_WM_PAINT() \
    { WM_PAINT, 0, 0, 0, WDS_PMSG(OnPaint, void (ThisClass::*)()), &WdsThunk_Void, nullptr },
#define ON_WM_CLOSE() \
    { WM_CLOSE, 0, 0, 0, WDS_PMSG(OnClose, void (ThisClass::*)()), &WdsThunk_Void, nullptr },
#define ON_WM_NCPAINT() \
    { WM_NCPAINT, 0, 0, 0, WDS_PMSG(OnNcPaint, void (ThisClass::*)()), &WdsThunk_Void, nullptr },
#define ON_WM_SYSCOLORCHANGE() \
    { WM_SYSCOLORCHANGE, 0, 0, 0, WDS_PMSG(OnSysColorChange, void (ThisClass::*)()), &WdsThunk_Void, nullptr },
#define ON_WM_SIZE() \
    { WM_SIZE, 0, 0, 0, WDS_PMSG(OnSize, void (ThisClass::*)(UINT, int, int)), &WdsThunk_Size, nullptr },
#define ON_WM_ERASEBKGND() \
    { WM_ERASEBKGND, 0, 0, 0, WDS_PMSG(OnEraseBkgnd, BOOL (ThisClass::*)(CDC*)), &WdsThunk_EraseBkgnd, nullptr },
#define ON_WM_LBUTTONDOWN() \
    { WM_LBUTTONDOWN, 0, 0, 0, WDS_PMSG(OnLButtonDown, void (ThisClass::*)(UINT, CPoint)), &WdsThunk_MouseBtn, nullptr },
#define ON_WM_LBUTTONUP() \
    { WM_LBUTTONUP, 0, 0, 0, WDS_PMSG(OnLButtonUp, void (ThisClass::*)(UINT, CPoint)), &WdsThunk_MouseBtn, nullptr },
#define ON_WM_LBUTTONDBLCLK() \
    { WM_LBUTTONDBLCLK, 0, 0, 0, WDS_PMSG(OnLButtonDblClk, void (ThisClass::*)(UINT, CPoint)), &WdsThunk_MouseBtn, nullptr },
#define ON_WM_MBUTTONDOWN() \
    { WM_MBUTTONDOWN, 0, 0, 0, WDS_PMSG(OnMButtonDown, void (ThisClass::*)(UINT, CPoint)), &WdsThunk_MouseBtn, nullptr },
#define ON_WM_RBUTTONDOWN() \
    { WM_RBUTTONDOWN, 0, 0, 0, WDS_PMSG(OnRButtonDown, void (ThisClass::*)(UINT, CPoint)), &WdsThunk_MouseBtn, nullptr },
#define ON_WM_MOUSEMOVE() \
    { WM_MOUSEMOVE, 0, 0, 0, WDS_PMSG(OnMouseMove, void (ThisClass::*)(UINT, CPoint)), &WdsThunk_MouseBtn, nullptr },
#define ON_WM_MOUSEWHEEL() \
    { WM_MOUSEWHEEL, 0, 0, 0, WDS_PMSG(OnMouseWheel, BOOL (ThisClass::*)(UINT, short, CPoint)), &WdsThunk_MouseWheel, nullptr },
#define ON_WM_KEYDOWN() \
    { WM_KEYDOWN, 0, 0, 0, WDS_PMSG(OnKeyDown, void (ThisClass::*)(UINT, UINT, UINT)), &WdsThunk_Key, nullptr },
#define ON_WM_CHAR() \
    { WM_CHAR, 0, 0, 0, WDS_PMSG(OnChar, void (ThisClass::*)(UINT, UINT, UINT)), &WdsThunk_Key, nullptr },
#define ON_WM_SETFOCUS() \
    { WM_SETFOCUS, 0, 0, 0, WDS_PMSG(OnSetFocus, void (ThisClass::*)(CWnd*)), &WdsThunk_Focus, nullptr },
#define ON_WM_KILLFOCUS() \
    { WM_KILLFOCUS, 0, 0, 0, WDS_PMSG(OnKillFocus, void (ThisClass::*)(CWnd*)), &WdsThunk_Focus, nullptr },
#define ON_WM_CONTEXTMENU() \
    { WM_CONTEXTMENU, 0, 0, 0, WDS_PMSG(OnContextMenu, void (ThisClass::*)(CWnd*, CPoint)), &WdsThunk_ContextMenu, nullptr },
#define ON_WM_TIMER() \
    { WM_TIMER, 0, 0, 0, WDS_PMSG(OnTimer, void (ThisClass::*)(UINT_PTR)), &WdsThunk_Timer, nullptr },
#define ON_WM_INITMENUPOPUP() \
    { WM_INITMENUPOPUP, 0, 0, 0, WDS_PMSG(OnInitMenuPopup, void (ThisClass::*)(CMenu*, UINT, BOOL)), &WdsThunk_InitMenuPopup, nullptr },
#define ON_WM_POWERBROADCAST() \
    { WM_POWERBROADCAST, 0, 0, 0, WDS_PMSG(OnPowerBroadcast, UINT (ThisClass::*)(UINT, LPARAM)), &WdsThunk_PowerBroadcast, nullptr },
#define ON_WM_NCACTIVATE() \
    { WM_NCACTIVATE, 0, 0, 0, WDS_PMSG(OnNcActivate, BOOL (ThisClass::*)(BOOL)), &WdsThunk_NcActivate, nullptr },
#define ON_WM_CTLCOLOR() \
    { WM_CTLCOLOR, 0, 0, 0, WDS_PMSG(OnCtlColor, HBRUSH (ThisClass::*)(CDC*, CWnd*, UINT)), &WdsThunk_CtlColor, nullptr },
#define ON_WM_NCHITTEST() \
    { WM_NCHITTEST, 0, 0, 0, WDS_PMSG(OnNcHitTest, LRESULT (ThisClass::*)(CPoint)), &WdsThunk_NcHitTest, nullptr },
#define ON_WM_GETMINMAXINFO() \
    { WM_GETMINMAXINFO, 0, 0, 0, WDS_PMSG(OnGetMinMaxInfo, void (ThisClass::*)(MINMAXINFO*)), &WdsThunk_GetMinMaxInfo, nullptr },
#define ON_WM_ENABLE() \
    { WM_ENABLE, 0, 0, 0, WDS_PMSG(OnEnable, void (ThisClass::*)(BOOL)), &WdsThunk_Enable, nullptr },
#define ON_WM_SETCURSOR() \
    { WM_SETCURSOR, 0, 0, 0, WDS_PMSG(OnSetCursor, BOOL (ThisClass::*)(CWnd*, UINT, UINT)), &WdsThunk_SetCursor, nullptr },
#define ON_WM_ACTIVATEAPP() \
    { WM_ACTIVATEAPP, 0, 0, 0, WDS_PMSG(OnActivateApp, void (ThisClass::*)(BOOL, DWORD)), &WdsThunk_ActivateApp, nullptr },
#define ON_WM_CAPTURECHANGED() \
    { WM_CAPTURECHANGED, 0, 0, 0, WDS_PMSG(OnCaptureChanged, void (ThisClass::*)(CWnd*)), &WdsThunk_CaptureChanged, nullptr },
#define ON_WM_SHOWWINDOW() \
    { WM_SHOWWINDOW, 0, 0, 0, WDS_PMSG(OnShowWindow, void (ThisClass::*)(BOOL, UINT)), &WdsThunk_ShowWindow, nullptr },
#define ON_WM_VSCROLL() \
    { WM_VSCROLL, 0, 0, 0, WDS_PMSG(OnVScroll, void (ThisClass::*)(UINT, UINT, CScrollBar*)), &WdsThunk_Scroll, nullptr },
#define ON_WM_HSCROLL() \
    { WM_HSCROLL, 0, 0, 0, WDS_PMSG(OnHScroll, void (ThisClass::*)(UINT, UINT, CScrollBar*)), &WdsThunk_Scroll, nullptr },
#define ON_WM_GETDLGCODE() \
    { WM_GETDLGCODE, 0, 0, 0, WDS_PMSG(OnGetDlgCode, UINT (ThisClass::*)()), &WdsThunk_GetDlgCode, nullptr },
#define ON_WM_NCCALCSIZE() \
    { WM_NCCALCSIZE, 0, 0, 0, WDS_PMSG(OnNcCalcSize, void (ThisClass::*)(BOOL, NCCALCSIZE_PARAMS*)), &WdsThunk_NcCalcSize, nullptr },
#define ON_WM_MOUSEACTIVATE() \
    { WM_MOUSEACTIVATE, 0, 0, 0, WDS_PMSG(OnMouseActivate, int (ThisClass::*)(CWnd*, UINT, UINT)), &WdsThunk_MouseActivate, nullptr },

// -----------------------------------------------------------------------------
//  CCmdTarget — message-map base + command routing
// -----------------------------------------------------------------------------
class CCmdTarget : public CObject
{
public:
    CCmdTarget() = default;

    virtual const AFX_MSGMAP* GetMessageMap() const { return GetThisMessageMap(); }
    static const AFX_MSGMAP* __stdcall _GetBaseMessageMap() { return nullptr; }
    static const AFX_MSGMAP* GetThisMessageMap()
    {
        static const AFX_MSGMAP_ENTRY _entries[] = { { 0, 0, 0, 0, nullptr, nullptr, nullptr } };
        static const AFX_MSGMAP map = { &CCmdTarget::_GetBaseMessageMap, _entries };
        return &map;
    }

    virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);

    static const CRuntimeClass classCCmdTarget;
    CRuntimeClass* GetRuntimeClass() const override { return const_cast<CRuntimeClass*>(&classCCmdTarget); }
};

inline const CRuntimeClass CCmdTarget::classCCmdTarget =
    { "CCmdTarget", static_cast<int>(sizeof(CCmdTarget)), 0xFFFF, nullptr, const_cast<CRuntimeClass*>(&CObject::classCObject) };

// -----------------------------------------------------------------------------
//  CCmdUI — command-UI updater
// -----------------------------------------------------------------------------
class CCmdUI
{
public:
    UINT   m_nID = 0;
    UINT   m_nIndex = 0;
    UINT   m_nIndexMax = 0;
    CMenu* m_pMenu = nullptr;
    CMenu* m_pSubMenu = nullptr;
    CWnd*  m_pOther = nullptr;   // owning bar / parent for control-based UI
    BOOL   m_bEnableChanged = FALSE;
    BOOL   m_bContinueRouting = FALSE;

    virtual void Enable(BOOL bOn = TRUE);
    virtual void SetCheck(int nCheck = 1);
    virtual void SetRadio(BOOL bOn = TRUE);
    virtual void SetText(LPCWSTR lpszText);
    virtual BOOL DoUpdate(CCmdTarget* pTarget, BOOL bDisableIfNoHndler);
};

// -----------------------------------------------------------------------------
//  GDI objects
// -----------------------------------------------------------------------------
class CGdiObject : public CObject
{
public:
    HGDIOBJ m_hObject = nullptr;

    CGdiObject() = default;
    explicit CGdiObject(HGDIOBJ h) : m_hObject(h) {}
    ~CGdiObject() override { DeleteObject(); }

    operator HGDIOBJ() const noexcept { return m_hObject; }
    HGDIOBJ GetSafeHandle() const noexcept { return this ? m_hObject : nullptr; }

    BOOL Attach(HGDIOBJ h) noexcept { if (m_hObject) return FALSE; m_hObject = h; return h != nullptr; }
    HGDIOBJ Detach() noexcept { HGDIOBJ h = m_hObject; m_hObject = nullptr; return h; }
    BOOL DeleteObject() noexcept
    {
        if (m_hObject == nullptr) return FALSE;
        const BOOL r = ::DeleteObject(m_hObject);
        m_hObject = nullptr;
        return r;
    }
    int GetObject(int nCount, LPVOID lpObject) const noexcept { return ::GetObjectW(m_hObject, nCount, lpObject); }
    static CGdiObject* FromHandle(HGDIOBJ h);
};

// Non-owning GDI handle wrapper.  Zeroes m_hObject before the CGdiObject base
// dtor runs so that CGdiObject::~CGdiObject never calls ::DeleteObject on a
// handle it doesn't own.  This prevents double-frees during TLS teardown when
// ExitProcess is called while borrowed handles are still in the map below.
struct WdsGdiHandleRef final : CGdiObject { ~WdsGdiHandleRef() override { m_hObject = nullptr; } };

// Per-handle stable wrapper map (mirrors MFC's GDI handle map) so nested
// CSelectObject/CSelectStockObject don't clobber each other's saved object.
// Uses a thread_local MRU single-slot cache to avoid hash lookups in the
// common case where the same handle is queried repeatedly.
inline CGdiObject* WdsWrapGdiHandle(HGDIOBJ h)
{
    if (h == nullptr) return nullptr;
    thread_local HGDIOBJ              s_mruKey = nullptr;
    thread_local CGdiObject*          s_mruVal = nullptr;
    thread_local std::unordered_map<HGDIOBJ, std::unique_ptr<CGdiObject>> map;
    if (h == s_mruKey) return s_mruVal;
    auto& slot = map[h];
    if (!slot) { slot = std::make_unique<WdsGdiHandleRef>(); slot->m_hObject = h; }
    s_mruKey = h;
    s_mruVal = slot.get();
    return s_mruVal;
}
inline CGdiObject* CGdiObject::FromHandle(HGDIOBJ h) { return WdsWrapGdiHandle(h); }

class CPen final : public CGdiObject
{
public:
    CPen() = default;
    CPen(int nStyle, int nWidth, COLORREF c) { CreatePen(nStyle, nWidth, c); }
    CPen(int nPenStyle, int nWidth, const LOGBRUSH* pLogBrush, int nStyleCount = 0, const DWORD* lpStyle = nullptr) { CreatePen(nPenStyle, nWidth, pLogBrush, nStyleCount, lpStyle); }
    BOOL CreatePen(int nStyle, int nWidth, COLORREF c) noexcept { DeleteObject(); m_hObject = ::CreatePen(nStyle, nWidth, c); return m_hObject != nullptr; }
    BOOL CreatePen(int nPenStyle, int nWidth, const LOGBRUSH* pLogBrush, int nStyleCount = 0, const DWORD* lpStyle = nullptr) noexcept { DeleteObject(); m_hObject = ::ExtCreatePen(static_cast<DWORD>(nPenStyle), static_cast<DWORD>(nWidth), pLogBrush, static_cast<DWORD>(nStyleCount), lpStyle); return m_hObject != nullptr; }
    BOOL CreatePenIndirect(const LOGPEN* lp) noexcept { DeleteObject(); m_hObject = ::CreatePenIndirect(lp); return m_hObject != nullptr; }
    operator HPEN() const noexcept { return static_cast<HPEN>(m_hObject); }
    static CPen* FromHandle(HPEN h) { return reinterpret_cast<CPen*>(WdsWrapGdiHandle(h)); }
};

class CBrush final : public CGdiObject
{
public:
    CBrush() = default;
    explicit CBrush(COLORREF c) { CreateSolidBrush(c); }
    BOOL CreateSolidBrush(COLORREF c) noexcept { DeleteObject(); m_hObject = ::CreateSolidBrush(c); return m_hObject != nullptr; }
    BOOL CreateHatchBrush(int nIndex, COLORREF c) noexcept { DeleteObject(); m_hObject = ::CreateHatchBrush(nIndex, c); return m_hObject != nullptr; }
    BOOL CreatePatternBrush(CBitmap* pBitmap);
    BOOL CreateBrushIndirect(const LOGBRUSH* lb) noexcept { DeleteObject(); m_hObject = ::CreateBrushIndirect(lb); return m_hObject != nullptr; }
    operator HBRUSH() const noexcept { return static_cast<HBRUSH>(m_hObject); }
    static CBrush* FromHandle(HBRUSH h) { return reinterpret_cast<CBrush*>(WdsWrapGdiHandle(h)); }
};

class CFont final : public CGdiObject
{
public:
    CFont() = default;
    BOOL CreateFontIndirect(const LOGFONTW* lf) noexcept { DeleteObject(); m_hObject = ::CreateFontIndirectW(lf); return m_hObject != nullptr; }
    BOOL CreateFont(int nHeight, int nWidth, int nEsc, int nOrient, int nWeight,
        BYTE bItalic, BYTE bUnderline, BYTE cStrikeOut, BYTE nCharSet, BYTE nOutPrecision,
        BYTE nClipPrecision, BYTE nQuality, BYTE nPitchAndFamily, LPCWSTR lpszFacename) noexcept
    {
        DeleteObject();
        m_hObject = ::CreateFontW(nHeight, nWidth, nEsc, nOrient, nWeight, bItalic, bUnderline,
            cStrikeOut, nCharSet, nOutPrecision, nClipPrecision, nQuality, nPitchAndFamily, lpszFacename);
        return m_hObject != nullptr;
    }
    BOOL CreatePointFont(int nPointSize, LPCWSTR lpszFaceName, CDC* pDC = nullptr);
    int GetLogFont(LOGFONTW* lf) const noexcept { return ::GetObjectW(m_hObject, static_cast<int>(sizeof(LOGFONTW)), lf); }
    operator HFONT() const noexcept { return static_cast<HFONT>(m_hObject); }
    static CFont* FromHandle(HFONT h) { return reinterpret_cast<CFont*>(WdsWrapGdiHandle(h)); }
};

class CBitmap final : public CGdiObject
{
public:
    CBitmap() = default;
    BOOL CreateBitmap(int w, int h, UINT planes, UINT bits, const void* lpBits) noexcept
    { DeleteObject(); m_hObject = ::CreateBitmap(w, h, planes, bits, lpBits); return m_hObject != nullptr; }
    BOOL CreateBitmapIndirect(LPBITMAP lpb) noexcept { DeleteObject(); m_hObject = ::CreateBitmapIndirect(lpb); return m_hObject != nullptr; }
    BOOL CreateCompatibleBitmap(CDC* pDC, int w, int h);
    BOOL LoadBitmap(LPCWSTR lpszResourceName) noexcept { DeleteObject(); m_hObject = ::LoadBitmapW(AfxGetResourceHandle(), lpszResourceName); return m_hObject != nullptr; }
    BOOL LoadBitmap(UINT nID) noexcept { return LoadBitmap(MAKEINTRESOURCEW(nID)); }
    int GetBitmap(BITMAP* pBitMap) const noexcept { return ::GetObjectW(m_hObject, static_cast<int>(sizeof(BITMAP)), pBitMap); }
    BOOL CreateFromGdiplus(Gdiplus::Bitmap& bitmap)
    {
        DeleteObject();
        const int w = static_cast<int>(bitmap.GetWidth());
        const int h = static_cast<int>(bitmap.GetHeight());

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* pBits = nullptr;
        m_hObject = ::CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
        if (!m_hObject) return FALSE;

        Gdiplus::BitmapData data;
        Gdiplus::Rect rect(0, 0, w, h);
        if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &data) == Gdiplus::Ok)
        {
            std::memcpy(pBits, data.Scan0, static_cast<size_t>(w) * h * 4);
            bitmap.UnlockBits(&data);
            return TRUE;
        }

        DeleteObject();
        return FALSE;
    }
    BOOL Create32BitBitmap(int size, const std::function<void(Gdiplus::Graphics&)>& painter)
    {
        Gdiplus::Bitmap renderBitmap(size, size, PixelFormat32bppPARGB);
        Gdiplus::Graphics g(&renderBitmap);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        g.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
        const Gdiplus::REAL scale = static_cast<Gdiplus::REAL>(size) / 64.0f;
        g.ScaleTransform(scale, scale);
        painter(g);
        return CreateFromGdiplus(renderBitmap);
    }
    operator HBITMAP() const noexcept { return static_cast<HBITMAP>(m_hObject); }
    static CBitmap* FromHandle(HBITMAP h) { return reinterpret_cast<CBitmap*>(WdsWrapGdiHandle(h)); }
};

class CRgn final : public CGdiObject
{
public:
    CRgn() = default;
    BOOL CreateRectRgn(int x1, int y1, int x2, int y2) noexcept { DeleteObject(); m_hObject = ::CreateRectRgn(x1, y1, x2, y2); return m_hObject != nullptr; }
    BOOL CreateRectRgnIndirect(LPCRECT lp) noexcept { DeleteObject(); m_hObject = ::CreateRectRgnIndirect(lp); return m_hObject != nullptr; }
    BOOL CreateRoundRectRgn(int x1, int y1, int x2, int y2, int x3, int y3) noexcept { DeleteObject(); m_hObject = ::CreateRoundRectRgn(x1, y1, x2, y2, x3, y3); return m_hObject != nullptr; }
    BOOL CreatePolygonRgn(const POINT* p, int n, int mode) noexcept { DeleteObject(); m_hObject = ::CreatePolygonRgn(p, n, mode); return m_hObject != nullptr; }
    int CombineRgn(CRgn* a, CRgn* b, int mode) noexcept { return ::CombineRgn(static_cast<HRGN>(m_hObject), static_cast<HRGN>(a->m_hObject), static_cast<HRGN>(b->m_hObject), mode); }
    int OffsetRgn(int x, int y) noexcept { return ::OffsetRgn(static_cast<HRGN>(m_hObject), x, y); }
    BOOL PtInRegion(int x, int y) const noexcept { return ::PtInRegion(static_cast<HRGN>(m_hObject), x, y); }
    operator HRGN() const noexcept { return static_cast<HRGN>(m_hObject); }
};

class CPalette final : public CGdiObject
{
public:
    CPalette() = default;
    BOOL CreatePalette(LPLOGPALETTE lp) noexcept { DeleteObject(); m_hObject = ::CreatePalette(lp); return m_hObject != nullptr; }
    operator HPALETTE() const noexcept { return static_cast<HPALETTE>(m_hObject); }
};

// -----------------------------------------------------------------------------
//  CDC
// -----------------------------------------------------------------------------
class CDC : public CObject
{
public:
    HDC m_hDC = nullptr;
    HDC m_hAttribDC = nullptr;

    CDC() = default;
    ~CDC() override { if (m_bOwned && m_hDC) ::DeleteDC(m_hDC); }

    operator HDC() const noexcept { return m_hDC; }
    HDC GetSafeHdc() const noexcept { return this ? m_hDC : nullptr; }

    BOOL Attach(HDC hDC) noexcept { m_hDC = m_hAttribDC = hDC; return hDC != nullptr; }
    HDC Detach() noexcept { HDC h = m_hDC; m_hDC = m_hAttribDC = nullptr; m_bOwned = false; return h; }
    BOOL CreateCompatibleDC(CDC* pDC) noexcept { m_hDC = m_hAttribDC = ::CreateCompatibleDC(pDC ? pDC->m_hDC : nullptr); m_bOwned = true; return m_hDC != nullptr; }
    BOOL DeleteDC() noexcept { if (!m_hDC) return FALSE; const BOOL r = ::DeleteDC(m_hDC); m_hDC = m_hAttribDC = nullptr; m_bOwned = false; return r; }

    static CDC* FromHandle(HDC hDC);

    // Object selection (per-handle wrappers; returns previous object)
    CGdiObject* SelectObject(CGdiObject* p)
    {
        HGDIOBJ old = ::SelectObject(m_hDC, p ? p->m_hObject : nullptr);
        return WdsWrapGdiHandle(old);
    }
    CPen*    SelectObject(CPen* p)    { return reinterpret_cast<CPen*>(WdsWrapGdiHandle(::SelectObject(m_hDC, p ? p->m_hObject : nullptr))); }
    CBrush*  SelectObject(CBrush* p)  { return reinterpret_cast<CBrush*>(WdsWrapGdiHandle(::SelectObject(m_hDC, p ? p->m_hObject : nullptr))); }
    CFont*   SelectObject(CFont* p)   { return reinterpret_cast<CFont*>(WdsWrapGdiHandle(::SelectObject(m_hDC, p ? p->m_hObject : nullptr))); }
    CBitmap* SelectObject(CBitmap* p) { return reinterpret_cast<CBitmap*>(WdsWrapGdiHandle(::SelectObject(m_hDC, p ? p->m_hObject : nullptr))); }
    CGdiObject* SelectStockObject(int nIndex) { return WdsWrapGdiHandle(::SelectObject(m_hDC, ::GetStockObject(nIndex))); }
    int SelectClipRgn(CRgn* pRgn) noexcept { return ::SelectClipRgn(m_hDC, pRgn ? static_cast<HRGN>(pRgn->m_hObject) : nullptr); }
    int SelectClipRgn(CRgn* pRgn, int nMode) noexcept { return ::ExtSelectClipRgn(m_hDC, pRgn ? static_cast<HRGN>(pRgn->m_hObject) : nullptr, nMode); }
    CPalette* SelectPalette(CPalette* pPal, BOOL bForceBkgd) { return reinterpret_cast<CPalette*>(WdsWrapGdiHandle(::SelectPalette(m_hDC, pPal ? static_cast<HPALETTE>(pPal->m_hObject) : nullptr, bForceBkgd))); }
    UINT RealizePalette() noexcept { return ::RealizePalette(m_hDC); }

    // Attributes
    COLORREF SetTextColor(COLORREF c) noexcept { return ::SetTextColor(m_hDC, c); }
    COLORREF GetTextColor() const noexcept { return ::GetTextColor(m_hDC); }
    COLORREF SetBkColor(COLORREF c) noexcept { return ::SetBkColor(m_hDC, c); }
    COLORREF GetBkColor() const noexcept { return ::GetBkColor(m_hDC); }
    int SetBkMode(int mode) noexcept { return ::SetBkMode(m_hDC, mode); }
    int GetBkMode() const noexcept { return ::GetBkMode(m_hDC); }
    UINT SetTextAlign(UINT n) noexcept { return ::SetTextAlign(m_hDC, n); }
    int SetStretchBltMode(int m) noexcept { return ::SetStretchBltMode(m_hDC, m); }
    int SetROP2(int m) noexcept { return ::SetROP2(m_hDC, m); }
    int SetMapMode(int m) noexcept { return ::SetMapMode(m_hDC, m); }
    int GetDeviceCaps(int n) const noexcept { return ::GetDeviceCaps(m_hDC, n); }
    HGDIOBJ GetCurrentObject(UINT t) const noexcept { return ::GetCurrentObject(m_hDC, t); }

    CPoint SetViewportOrg(int x, int y) noexcept { POINT p; ::SetViewportOrgEx(m_hDC, x, y, &p); return p; }
    CPoint OffsetViewportOrg(int dx, int dy) noexcept { POINT p; ::OffsetViewportOrgEx(m_hDC, dx, dy, &p); return p; }
    CPoint GetViewportOrg() const noexcept { POINT p; ::GetViewportOrgEx(m_hDC, &p); return p; }

    int SaveDC() noexcept { return ::SaveDC(m_hDC); }
    BOOL RestoreDC(int n) noexcept { return ::RestoreDC(m_hDC, n); }

    // Rect/region clipping
    int GetClipBox(LPRECT lpRect) const noexcept { return ::GetClipBox(m_hDC, lpRect); }
    int IntersectClipRect(int l, int t, int r, int b) noexcept { return ::IntersectClipRect(m_hDC, l, t, r, b); }
    int IntersectClipRect(LPCRECT rc) noexcept { return ::IntersectClipRect(m_hDC, rc->left, rc->top, rc->right, rc->bottom); }
    int ExcludeClipRect(int l, int t, int r, int b) noexcept { return ::ExcludeClipRect(m_hDC, l, t, r, b); }

    // Drawing primitives
    CPoint MoveTo(int x, int y) noexcept { POINT p; ::MoveToEx(m_hDC, x, y, &p); return p; }
    CPoint MoveTo(POINT p) noexcept { return MoveTo(p.x, p.y); }
    BOOL LineTo(int x, int y) noexcept { return ::LineTo(m_hDC, x, y); }
    BOOL LineTo(POINT p) noexcept { return ::LineTo(m_hDC, p.x, p.y); }
    BOOL Rectangle(int l, int t, int r, int b) noexcept { return ::Rectangle(m_hDC, l, t, r, b); }
    BOOL Rectangle(LPCRECT rc) noexcept { return ::Rectangle(m_hDC, rc->left, rc->top, rc->right, rc->bottom); }
    BOOL RoundRect(int l, int t, int r, int b, int w, int h) noexcept { return ::RoundRect(m_hDC, l, t, r, b, w, h); }
    BOOL RoundRect(LPCRECT rc, POINT pt) noexcept { return ::RoundRect(m_hDC, rc->left, rc->top, rc->right, rc->bottom, pt.x, pt.y); }
    BOOL Ellipse(int l, int t, int r, int b) noexcept { return ::Ellipse(m_hDC, l, t, r, b); }
    BOOL Ellipse(LPCRECT rc) noexcept { return ::Ellipse(m_hDC, rc->left, rc->top, rc->right, rc->bottom); }
    BOOL Polygon(const POINT* p, int n) noexcept { return ::Polygon(m_hDC, p, n); }
    BOOL Polyline(const POINT* p, int n) noexcept { return ::Polyline(m_hDC, p, n); }
    BOOL Pie(int x1,int y1,int x2,int y2,int x3,int y3,int x4,int y4) noexcept { return ::Pie(m_hDC,x1,y1,x2,y2,x3,y3,x4,y4); }
    COLORREF SetPixel(int x, int y, COLORREF c) noexcept { return ::SetPixel(m_hDC, x, y, c); }
    COLORREF GetPixel(int x, int y) const noexcept { return ::GetPixel(m_hDC, x, y); }

    void FillSolidRect(LPCRECT rc, COLORREF clr) noexcept
    {
        ::SetBkColor(m_hDC, clr);
        ::ExtTextOutW(m_hDC, 0, 0, ETO_OPAQUE, rc, nullptr, 0, nullptr);
    }
    void FillSolidRect(int x, int y, int cx, int cy, COLORREF clr) noexcept
    {
        const RECT rc{ x, y, x + cx, y + cy };
        FillSolidRect(&rc, clr);
    }
    int FillRect(LPCRECT rc, CBrush* pBrush) noexcept { return ::FillRect(m_hDC, rc, pBrush ? static_cast<HBRUSH>(pBrush->m_hObject) : nullptr); }
    void FrameRect(LPCRECT rc, CBrush* pBrush) noexcept { ::FrameRect(m_hDC, rc, pBrush ? static_cast<HBRUSH>(pBrush->m_hObject) : nullptr); }
    void Draw3dRect(LPCRECT rc, COLORREF clrTopLeft, COLORREF clrBottomRight) noexcept
    { Draw3dRect(rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top, clrTopLeft, clrBottomRight); }
    void Draw3dRect(int x, int y, int cx, int cy, COLORREF clrTopLeft, COLORREF clrBottomRight) noexcept
    {
        FillSolidRect(x, y, cx - 1, 1, clrTopLeft);
        FillSolidRect(x, y, 1, cy - 1, clrTopLeft);
        FillSolidRect(x + cx, y, -1, cy, clrBottomRight);
        FillSolidRect(x, y + cy, cx, -1, clrBottomRight);
    }
    BOOL DrawEdge(LPRECT rc, UINT edge, UINT flags) noexcept { return ::DrawEdge(m_hDC, rc, edge, flags); }
    void DrawFocusRect(LPCRECT rc) noexcept { ::DrawFocusRect(m_hDC, rc); }
    BOOL DrawIconEx(int x, int y, HICON hIcon, int cx, int cy) noexcept { return ::DrawIconEx(m_hDC, x, y, hIcon, cx, cy, 0, nullptr, DI_NORMAL); }

    // Text
    int DrawText(LPCWSTR psz, int n, LPRECT rc, UINT fmt) noexcept { return ::DrawTextW(m_hDC, psz, n, rc, fmt); }
    int DrawText(const CString& s, LPRECT rc, UINT fmt) noexcept { return ::DrawTextW(m_hDC, s.GetString(), s.GetLength(), rc, fmt); }
    BOOL TextOut(int x, int y, LPCWSTR psz, int n) noexcept { return ::TextOutW(m_hDC, x, y, psz, n); }
    BOOL TextOut(int x, int y, const CString& s) noexcept { return ::TextOutW(m_hDC, x, y, s.GetString(), s.GetLength()); }
    BOOL ExtTextOut(int x, int y, UINT opts, LPCRECT rc, LPCWSTR psz, UINT n, const int* dx) noexcept { return ::ExtTextOutW(m_hDC, x, y, opts, rc, psz, n, dx); }
    CSize GetTextExtent(LPCWSTR psz, int n) const noexcept { SIZE s{}; ::GetTextExtentPoint32W(m_hDC, psz, n, &s); return s; }
    CSize GetTextExtent(const CString& str) const noexcept { return GetTextExtent(str.GetString(), str.GetLength()); }
    BOOL GetTextMetrics(LPTEXTMETRICW lptm) const noexcept { return ::GetTextMetricsW(m_hDC, lptm); }

    // Blit
    BOOL BitBlt(int x, int y, int cx, int cy, CDC* pSrc, int xs, int ys, DWORD rop) noexcept { return ::BitBlt(m_hDC, x, y, cx, cy, pSrc ? pSrc->m_hDC : nullptr, xs, ys, rop); }
    BOOL StretchBlt(int x, int y, int cx, int cy, CDC* pSrc, int xs, int ys, int cxs, int cys, DWORD rop) noexcept { return ::StretchBlt(m_hDC, x, y, cx, cy, pSrc ? pSrc->m_hDC : nullptr, xs, ys, cxs, cys, rop); }
    BOOL PatBlt(int x, int y, int cx, int cy, DWORD rop) noexcept { return ::PatBlt(m_hDC, x, y, cx, cy, rop); }
    BOOL TransparentBlt(int x, int y, int cx, int cy, CDC* pSrc, int xs, int ys, int cxs, int cys, UINT crTransparent) noexcept { return ::TransparentBlt(m_hDC, x, y, cx, cy, pSrc ? pSrc->m_hDC : nullptr, xs, ys, cxs, cys, crTransparent); }
    BOOL AlphaBlend(int x, int y, int cx, int cy, CDC* pSrc, int xs, int ys, int cxs, int cys, BLENDFUNCTION bf) noexcept { return ::AlphaBlend(m_hDC, x, y, cx, cy, pSrc ? pSrc->m_hDC : nullptr, xs, ys, cxs, cys, bf); }
    BOOL GradientFill(TRIVERTEX* pV, ULONG nV, void* pMesh, ULONG nMesh, ULONG mode) noexcept { return ::GradientFill(m_hDC, pV, nV, pMesh, nMesh, mode); }
    COLORREF SetDCPenColor(COLORREF c) noexcept { return ::SetDCPenColor(m_hDC, c); }
    COLORREF SetDCBrushColor(COLORREF c) noexcept { return ::SetDCBrushColor(m_hDC, c); }
    CFont* GetCurrentFont() const { return CFont::FromHandle(static_cast<HFONT>(::GetCurrentObject(m_hDC, OBJ_FONT))); }

protected:
    bool m_bOwned = false;
};

inline CDC* CDC::FromHandle(HDC hDC)
{
    if (hDC == nullptr) return nullptr;
    thread_local HDC                s_mruKey = nullptr;
    thread_local CDC*               s_mruVal = nullptr;
    thread_local std::unordered_map<HDC, std::unique_ptr<CDC>> map;
    if (hDC == s_mruKey) { s_mruVal->m_hDC = s_mruVal->m_hAttribDC = hDC; return s_mruVal; }
    auto& slot = map[hDC];
    if (!slot) slot = std::make_unique<CDC>();
    slot->m_hDC = slot->m_hAttribDC = hDC;
    s_mruKey = hDC;
    s_mruVal = slot.get();
    return s_mruVal;
}

inline BOOL CBrush::CreatePatternBrush(CBitmap* pBitmap)
{ DeleteObject(); m_hObject = ::CreatePatternBrush(pBitmap ? static_cast<HBITMAP>(pBitmap->m_hObject) : nullptr); return m_hObject != nullptr; }
inline BOOL CBitmap::CreateCompatibleBitmap(CDC* pDC, int w, int h)
{ DeleteObject(); m_hObject = ::CreateCompatibleBitmap(pDC ? pDC->m_hDC : nullptr, w, h); return m_hObject != nullptr; }
inline BOOL CFont::CreatePointFont(int nPointSize, LPCWSTR lpszFaceName, CDC* pDC)
{
    LOGFONTW lf{};
    const HDC hDC = pDC ? pDC->m_hDC : ::GetDC(nullptr);
    lf.lfHeight = -MulDiv(nPointSize, ::GetDeviceCaps(hDC, LOGPIXELSY), 720);
    if (!pDC) ::ReleaseDC(nullptr, hDC);
    lf.lfWeight = FW_NORMAL; lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy_s(lf.lfFaceName, lpszFaceName, _TRUNCATE);
    return CreateFontIndirect(&lf);
}

// -----------------------------------------------------------------------------
//  CWnd
// -----------------------------------------------------------------------------
class CWnd;
class CMenu;
struct CCreateContext
{
    CRuntimeClass* m_pNewViewClass = nullptr;
    CObject*       m_pCurrentDoc = nullptr;
    CObject*       m_pNewDocTemplate = nullptr;
    CWnd*          m_pLastView = nullptr;
    CWnd*          m_pCurrentFrame = nullptr;
};

struct WdsNotify { NMHDR* pNMHDR; LRESULT* pResult; UINT id; };

LRESULT CALLBACK AfxWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LPCWSTR AFXAPI AfxRegisterWndClass(UINT classStyle, HCURSOR hCursor = nullptr,
    HBRUSH hbrBackground = nullptr, HICON hIcon = nullptr);
LPCWSTR AFXAPI WdsDefaultWndClass();

inline thread_local CWnd* g_pWndInit = nullptr;
inline thread_local MSG   g_currentMsg = {};
inline thread_local int   g_ddxOutputDepth = 0;

// Set for the duration of each WM_DRAWITEM dispatch so CListCtrl::GetItemRect /
// GetSubItemRect can compute column rects from the already-known item rect + header
// widths without sending any messages back to the (subclassed) list control.
struct AfxDrawItemCtx
{
    HWND hWnd{};
    int  item = -1;
    RECT rcItem{};
    int  colX[17]{};  // cumulative X offsets: colX[c] = left edge of column c from item left
    int  colCount = 0;
};
inline thread_local AfxDrawItemCtx g_drawItemCtx;

class CWnd : public CCmdTarget
{
    DECLARE_DYNCREATE(CWnd)
public:
    HWND m_hWnd = nullptr;

    CWnd() = default;
    explicit CWnd(HWND h) : m_hWnd(h) {}
    ~CWnd() override { if (m_hWnd && m_bAutoDelete == 0) { /* leave window alone */ } }

    operator HWND() const noexcept { return m_hWnd; }
    HWND GetSafeHwnd() const noexcept { return this ? m_hWnd : nullptr; }

    // ---- handle maps ----
    static CWnd* FromHandlePermanent(HWND hWnd)
    {
        if (hWnd == nullptr) return nullptr;
        return static_cast<CWnd*>(::GetPropW(hWnd, kProp()));
    }
    static CWnd* FromHandle(HWND hWnd);
    void Attach(HWND hWnd)
    {
        m_hWnd = hWnd;
        if (hWnd) ::SetPropW(hWnd, kProp(), this);
    }
    HWND Detach()
    {
        HWND h = m_hWnd;
        if (h) ::RemovePropW(h, kProp());
        m_hWnd = nullptr;
        return h;
    }

    // ---- creation ----
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    virtual void PreSubclassWindow() {}
    virtual void PostNcDestroy();
    virtual BOOL PreTranslateMessage(MSG*) { return FALSE; }
    BOOL ExecuteDlgInit(LPCWSTR lpszResourceName);
    BOOL ExecuteDlgInit(UINT nIDResource) { return ExecuteDlgInit(MAKEINTRESOURCEW(nIDResource)); }
    BOOL ExecuteDlgInit(LPVOID lpResource);

    BOOL CreateEx(DWORD dwExStyle, LPCWSTR lpszClassName, LPCWSTR lpszWindowName, DWORD dwStyle,
        int x, int y, int nWidth, int nHeight, HWND hwndParent, HMENU nIDorHMenu, LPVOID lpParam = nullptr)
    {
        CREATESTRUCT cs{};
        cs.dwExStyle = dwExStyle; cs.lpszClass = lpszClassName; cs.lpszName = lpszWindowName;
        cs.style = dwStyle; cs.x = x; cs.y = y; cs.cx = nWidth; cs.cy = nHeight;
        cs.hwndParent = hwndParent; cs.hMenu = nIDorHMenu;
        cs.hInstance = AfxGetInstanceHandle(); cs.lpCreateParams = lpParam;
        if (!PreCreateWindow(cs)) return FALSE;
        if (cs.lpszClass == nullptr) cs.lpszClass = WdsDefaultWndClass();
        g_pWndInit = this;
        const HWND h = ::CreateWindowExW(cs.dwExStyle, cs.lpszClass, cs.lpszName, cs.style,
            cs.x, cs.y, cs.cx, cs.cy, cs.hwndParent, cs.hMenu, cs.hInstance, cs.lpCreateParams);
        g_pWndInit = nullptr;
        if (h == nullptr) return FALSE;
        // If the window used our AfxWndProc class, the creation hook already attached it.
        // Otherwise it is a system/common-control class — subclass it so our message map runs
        // (this is how MFC routes WM_ERASEBKGND, custom-draw, etc. to control-derived classes).
        if (m_hWnd == nullptr) SubclassWindow(h);
        return TRUE;
    }
    BOOL CreateEx(DWORD dwExStyle, LPCWSTR lpszClassName, LPCWSTR lpszWindowName, DWORD dwStyle,
        const RECT& rect, CWnd* pParentWnd, UINT nID, LPVOID lpParam = nullptr)
    {
        return CreateEx(dwExStyle, lpszClassName, lpszWindowName, dwStyle,
            rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
            pParentWnd ? pParentWnd->m_hWnd : nullptr, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(nID)), lpParam);
    }
    virtual BOOL Create(LPCWSTR lpszClassName, LPCWSTR lpszWindowName, DWORD dwStyle,
        const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* = nullptr)
    {
        return CreateEx(0, lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID);
    }

    BOOL SubclassWindow(HWND hWnd)
    {
        if (hWnd == nullptr) return FALSE;
        WNDPROC old = reinterpret_cast<WNDPROC>(::GetWindowLongPtrW(hWnd, GWLP_WNDPROC));
        if (old == AfxWndProc) return TRUE;
        Attach(hWnd);
        m_pfnSuper = reinterpret_cast<WNDPROC>(::SetWindowLongPtrW(hWnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(AfxWndProc)));
        PreSubclassWindow();
        return TRUE;
    }
    BOOL SubclassDlgItem(UINT nID, CWnd* pParent)
    {
        const HWND h = ::GetDlgItem(pParent ? pParent->m_hWnd : nullptr, static_cast<int>(nID));
        return h ? SubclassWindow(h) : FALSE;
    }
    HWND UnsubclassWindow()
    {
        if (m_pfnSuper) { ::SetWindowLongPtrW(m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_pfnSuper)); m_pfnSuper = nullptr; }
        return Detach();
    }

    // ---- message dispatch ----
    LRESULT WindowProcEntry(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // Fast path: control-protocol messages (LVM_*, HDM_*, TCM_*, etc.) in
        // [0x1000, WM_APP) are never handled by application code.  Skip the
        // virtual dispatch chain, GetMessagePos/Time, and g_currentMsg bookkeeping
        // entirely and go straight to the superclass wndproc.
        if (msg >= 0x1000u && msg < WM_APP)
            return DefWindowProc(msg, wParam, lParam);

        const MSG saved = g_currentMsg;
        const DWORD pos = ::GetMessagePos();
        g_currentMsg = { m_hWnd, msg, wParam, lParam, static_cast<DWORD>(::GetMessageTime()), POINT{ static_cast<LONG>(static_cast<short>(LOWORD(pos))), static_cast<LONG>(static_cast<short>(HIWORD(pos))) } };
        const LRESULT r = WindowProc(msg, wParam, lParam);
        if (msg == WM_NCDESTROY)
        {
            const HWND h = m_hWnd;
            Detach();
            if (m_pfnSuper) m_pfnSuper = nullptr;
            PostNcDestroy();
            (void)h;
        }
        g_currentMsg = saved;
        return r;
    }
    virtual LRESULT WindowProc(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        LRESULT lResult = 0;
        if (!OnWndMsg(msg, wParam, lParam, &lResult))
            lResult = DefWindowProc(msg, wParam, lParam);
        return lResult;
    }
    virtual BOOL OnWndMsg(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* pResult);
    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
    virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
    BOOL ReflectChildCommand(int code);
    BOOL ReflectChildNotify(NMHDR* pNMHDR, LRESULT* pResult);

    LRESULT DefWindowProc(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (m_pfnSuper) return ::CallWindowProcW(m_pfnSuper, m_hWnd, msg, wParam, lParam);
        return ::DefWindowProcW(m_hWnd, msg, wParam, lParam);
    }
    LRESULT Default() { return DefWindowProc(g_currentMsg.message, g_currentMsg.wParam, g_currentMsg.lParam); }
    static const MSG* GetCurrentMessage() { return &g_currentMsg; }

    // ---- common operations ----
    BOOL DestroyWindow() { if (!m_hWnd) return FALSE; return ::DestroyWindow(m_hWnd); }
    BOOL ShowWindow(int nCmdShow) { return ::ShowWindow(m_hWnd, nCmdShow); }
    BOOL UpdateWindow() { return ::UpdateWindow(m_hWnd); }
    void Invalidate(BOOL bErase = TRUE) { ::InvalidateRect(m_hWnd, nullptr, bErase); }
    void InvalidateRect(LPCRECT rc, BOOL bErase = TRUE) { ::InvalidateRect(m_hWnd, rc, bErase); }
    void ValidateRect(LPCRECT rc) { ::ValidateRect(m_hWnd, rc); }
    BOOL RedrawWindow(LPCRECT rc = nullptr, CRgn* pRgn = nullptr, UINT flags = RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE)
    { return ::RedrawWindow(m_hWnd, rc, pRgn ? static_cast<HRGN>(pRgn->m_hObject) : nullptr, flags); }

    BOOL MoveWindow(int x, int y, int cx, int cy, BOOL bRepaint = TRUE) noexcept { return ::MoveWindow(m_hWnd, x, y, cx, cy, bRepaint); }
    BOOL MoveWindow(LPCRECT rc, BOOL bRepaint = TRUE) noexcept { return ::MoveWindow(m_hWnd, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top, bRepaint); }
    BOOL SetWindowPos(const CWnd* pAfter, int x, int y, int cx, int cy, UINT flags) noexcept { return ::SetWindowPos(m_hWnd, pAfter ? pAfter->m_hWnd : nullptr, x, y, cx, cy, flags); }

    void GetClientRect(LPRECT rc) const noexcept { ::GetClientRect(m_hWnd, rc); }
    void GetWindowRect(LPRECT rc) const noexcept { ::GetWindowRect(m_hWnd, rc); }
    void ClientToScreen(LPPOINT p) const noexcept { ::ClientToScreen(m_hWnd, p); }
    void ClientToScreen(LPRECT rc) const noexcept { ::ClientToScreen(m_hWnd, reinterpret_cast<LPPOINT>(rc)); ::ClientToScreen(m_hWnd, reinterpret_cast<LPPOINT>(rc) + 1); }
    void ScreenToClient(LPPOINT p) const noexcept { ::ScreenToClient(m_hWnd, p); }
    void ScreenToClient(LPRECT rc) const noexcept { ::ScreenToClient(m_hWnd, reinterpret_cast<LPPOINT>(rc)); ::ScreenToClient(m_hWnd, reinterpret_cast<LPPOINT>(rc) + 1); }
    int MapWindowPoints(CWnd* pTo, LPPOINT p, UINT n) const noexcept { return ::MapWindowPoints(m_hWnd, pTo ? pTo->m_hWnd : nullptr, p, n); }
    void MapWindowPoints(CWnd* pTo, LPRECT rc) const noexcept { ::MapWindowPoints(m_hWnd, pTo ? pTo->m_hWnd : nullptr, reinterpret_cast<LPPOINT>(rc), 2); }

    CWnd* GetParent() const { return FromHandle(::GetParent(m_hWnd)); }
    CWnd* GetTopLevelParent() const { return FromHandle(::GetAncestor(m_hWnd, GA_ROOT)); }
    BOOL IsChild(const CWnd* pWnd) const noexcept { return pWnd != nullptr && ::IsChild(m_hWnd, pWnd->m_hWnd); }
    HWND SetParent(CWnd* pNew) noexcept { return ::SetParent(m_hWnd, pNew ? pNew->m_hWnd : nullptr); }
    CWnd* GetDlgItem(int nID) const { return FromHandle(::GetDlgItem(m_hWnd, nID)); }
    void GetDlgItem(int nID, HWND* phWnd) const noexcept { *phWnd = ::GetDlgItem(m_hWnd, nID); }
    CWnd* GetWindow(UINT nCmd) const { return FromHandle(::GetWindow(m_hWnd, nCmd)); }
    CWnd* GetNextWindow(UINT nFlag = GW_HWNDNEXT) const { return FromHandle(::GetWindow(m_hWnd, nFlag)); }
    CWnd* GetFocus() const { return FromHandle(::GetFocus()); }
    CWnd* ChildWindowFromPoint(POINT pt) const { return FromHandle(::ChildWindowFromPoint(m_hWnd, pt)); }
    CWnd* ChildWindowFromPoint(POINT pt, UINT flags) const { return FromHandle(::ChildWindowFromPointEx(m_hWnd, pt, flags)); }
    static CWnd* GetActiveWindow() { return FromHandle(::GetActiveWindow()); }
    static CWnd* WindowFromPoint(POINT pt) { return FromHandle(::WindowFromPoint(pt)); }
    static CWnd* GetCapture() { return FromHandle(::GetCapture()); }
    CWnd* GetSafeOwner() const
    {
        HWND h = m_hWnd;
        for (; h != nullptr && (::GetWindowLongPtrW(h, GWL_STYLE) & WS_CHILD); h = ::GetParent(h)) {}
        return h ? CWnd::FromHandle(h) : nullptr;
    }

    LRESULT SendMessage(UINT msg, WPARAM wParam = 0, LPARAM lParam = 0) const noexcept { return ::SendMessageW(m_hWnd, msg, wParam, lParam); }
    BOOL PostMessage(UINT msg, WPARAM wParam = 0, LPARAM lParam = 0) const noexcept { return ::PostMessageW(m_hWnd, msg, wParam, lParam); }
    // Like SendMessage but, when we have subclassed this window, calls the previous wndproc
    // directly via CallWindowProcW instead of routing through SendMessage's user32 dispatch
    // machinery (thread-queue check, hook chain, etc.).  Safe to call only from the window's
    // own thread — which is always true for MFC-style control methods.
    LRESULT SendSelf(UINT msg, WPARAM wParam = 0, LPARAM lParam = 0) const
    {
        return m_pfnSuper
            ? ::CallWindowProcW(m_pfnSuper, m_hWnd, msg, wParam, lParam)
            : ::SendMessageW(m_hWnd, msg, wParam, lParam);
    }
    LRESULT SendDlgItemMessage(int nID, UINT msg, WPARAM wParam = 0, LPARAM lParam = 0) noexcept { return ::SendDlgItemMessageW(m_hWnd, nID, msg, wParam, lParam); }

    void SetWindowText(LPCWSTR psz) noexcept { ::SetWindowTextW(m_hWnd, psz); }
    int GetWindowTextLength() const noexcept { return ::GetWindowTextLengthW(m_hWnd); }
    int GetWindowText(LPWSTR psz, int n) const noexcept { return ::GetWindowTextW(m_hWnd, psz, n); }
    void GetWindowText(CString& str) const
    {
        const int len = GetWindowTextLength();
        std::wstring s(static_cast<size_t>(len), L'\0');
        const int got = ::GetWindowTextW(m_hWnd, s.data(), len + 1);
        s.resize(static_cast<size_t>(got));
        str = std::move(s);
    }
    void SetDlgItemText(int nID, LPCWSTR psz) noexcept { ::SetDlgItemTextW(m_hWnd, nID, psz); }
    int GetDlgItemText(int nID, CString& str) const
    {
        const int len = ::GetWindowTextLengthW(::GetDlgItem(m_hWnd, nID));
        if (len == 0) { str.Empty(); return 0; }
        const int got = ::GetDlgItemTextW(m_hWnd, nID, str.GetBuffer(len + 1), len + 1);
        str.ReleaseBuffer(got);
        return got;
    }
    UINT GetDlgItemInt(int nID, BOOL* pTrans = nullptr, BOOL bSigned = TRUE) const noexcept { return ::GetDlgItemInt(m_hWnd, nID, pTrans, bSigned); }
    void SetDlgItemInt(int nID, int v, BOOL bSigned = TRUE) noexcept { ::SetDlgItemInt(m_hWnd, nID, v, bSigned); }
    void CheckDlgButton(int nID, UINT check) noexcept { ::CheckDlgButton(m_hWnd, nID, check); }
    UINT IsDlgButtonChecked(int nID) const noexcept { return ::IsDlgButtonChecked(m_hWnd, nID); }
    void CheckRadioButton(int f, int l, int c) noexcept { ::CheckRadioButton(m_hWnd, f, l, c); }

    BOOL EnableWindow(BOOL bEnable = TRUE) noexcept { return ::EnableWindow(m_hWnd, bEnable); }
    BOOL IsWindowEnabled() const noexcept { return ::IsWindowEnabled(m_hWnd); }
    BOOL IsWindowVisible() const noexcept { return ::IsWindowVisible(m_hWnd); }
    BOOL IsIconic() const noexcept { return ::IsIconic(m_hWnd); }
    BOOL IsZoomed() const noexcept { return ::IsZoomed(m_hWnd); }
    HWND SetFocus() noexcept { return ::SetFocus(m_hWnd); }
    HWND SetCapture() noexcept { return ::SetCapture(m_hWnd); }
    BOOL BringWindowToTop() noexcept { return ::BringWindowToTop(m_hWnd); }
    BOOL SetForegroundWindow() noexcept { return ::SetForegroundWindow(m_hWnd); }

    LONG GetStyle() const noexcept { return static_cast<LONG>(::GetWindowLongPtrW(m_hWnd, GWL_STYLE)); }
    LONG GetExStyle() const noexcept { return static_cast<LONG>(::GetWindowLongPtrW(m_hWnd, GWL_EXSTYLE)); }
    BOOL ModifyStyle(DWORD remove, DWORD add, UINT flags = 0)
    {
        LONG_PTR s = ::GetWindowLongPtrW(m_hWnd, GWL_STYLE);
        const LONG_PTR n = (s & ~static_cast<LONG_PTR>(remove)) | add;
        if (s == n) return FALSE;
        ::SetWindowLongPtrW(m_hWnd, GWL_STYLE, n);
        if (flags) ::SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | flags);
        return TRUE;
    }
    BOOL ModifyStyleEx(DWORD remove, DWORD add, UINT flags = 0)
    {
        LONG_PTR s = ::GetWindowLongPtrW(m_hWnd, GWL_EXSTYLE);
        const LONG_PTR n = (s & ~static_cast<LONG_PTR>(remove)) | add;
        if (s == n) return FALSE;
        ::SetWindowLongPtrW(m_hWnd, GWL_EXSTYLE, n);
        if (flags) ::SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | flags);
        return TRUE;
    }

    UINT_PTR SetTimer(UINT_PTR nID, UINT elapse, void (CALLBACK* lpfn)(HWND, UINT, UINT_PTR, DWORD) = nullptr) noexcept { return ::SetTimer(m_hWnd, nID, elapse, lpfn); }
    BOOL KillTimer(UINT_PTR nID) noexcept { return ::KillTimer(m_hWnd, nID); }

    BOOL GetWindowPlacement(WINDOWPLACEMENT* p) const noexcept { return ::GetWindowPlacement(m_hWnd, p); }
    BOOL SetWindowPlacement(const WINDOWPLACEMENT* p) noexcept { return ::SetWindowPlacement(m_hWnd, p); }

    CDC* GetDC() { return CDC::FromHandle(::GetDC(m_hWnd)); }
    CDC* GetWindowDC() { return CDC::FromHandle(::GetWindowDC(m_hWnd)); }
    int ReleaseDC(CDC* pDC) noexcept { return ::ReleaseDC(m_hWnd, pDC ? pDC->m_hDC : nullptr); }

    CFont* GetFont() const { return CFont::FromHandle(reinterpret_cast<HFONT>(::SendMessageW(m_hWnd, WM_GETFONT, 0, 0))); }
    void SetFont(CFont* pFont, BOOL bRedraw = TRUE) { ::SendMessageW(m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(pFont ? pFont->m_hObject : nullptr), bRedraw); }
    void SetRedraw(BOOL bRedraw = TRUE) { ::SendMessageW(m_hWnd, WM_SETREDRAW, static_cast<WPARAM>(bRedraw), 0); }

    BOOL OpenClipboard() noexcept { return ::OpenClipboard(m_hWnd); }

    int  GetDlgCtrlID() const noexcept { return ::GetDlgCtrlID(m_hWnd); }
    HICON SetIcon(HICON hIcon, BOOL bBig = TRUE) { return reinterpret_cast<HICON>(::SendMessageW(m_hWnd, WM_SETICON, bBig ? ICON_BIG : ICON_SMALL, reinterpret_cast<LPARAM>(hIcon))); }
    static CWnd* GetDesktopWindow() { return FromHandle(::GetDesktopWindow()); }
    BOOL LockWindowUpdate() noexcept { return ::LockWindowUpdate(m_hWnd); }
    void UnlockWindowUpdate() noexcept { ::LockWindowUpdate(nullptr); }
    BOOL HideCaret() noexcept { return ::HideCaret(m_hWnd); }
    BOOL ShowCaret() noexcept { return ::ShowCaret(m_hWnd); }

    CMenu* GetMenu() const;
    BOOL SetMenu(CMenu* pMenu);
    void DrawMenuBar() noexcept { ::DrawMenuBar(m_hWnd); }
    CMenu* GetSystemMenu(BOOL bRevert) const;

    int GetScrollPos(int bar) const noexcept { return ::GetScrollPos(m_hWnd, bar); }
    int SetScrollPos(int bar, int pos, BOOL bRedraw = TRUE) noexcept { return ::SetScrollPos(m_hWnd, bar, pos, bRedraw); }
    BOOL GetScrollInfo(int bar, SCROLLINFO* psi, UINT mask = SIF_ALL) noexcept { psi->cbSize = sizeof(SCROLLINFO); psi->fMask = mask; return ::GetScrollInfo(m_hWnd, bar, psi); }
    int SetScrollInfo(int bar, SCROLLINFO* psi, BOOL bRedraw = TRUE) { psi->cbSize = sizeof(SCROLLINFO); return ::SetScrollInfo(m_hWnd, bar, psi, bRedraw); }
    BOOL GetScrollRange(int bar, LPINT pMin, LPINT pMax) const { return ::GetScrollRange(m_hWnd, bar, pMin, pMax); }

    int SetWindowRgn(HRGN hRgn, BOOL bRedraw) { return ::SetWindowRgn(m_hWnd, hRgn, bRedraw); }
    void CenterWindow(CWnd* pAlternate = nullptr);
    BOOL UpdateData(BOOL bSaveAndValidate = TRUE);   // dialogs/pages
    virtual void DoDataExchange(class CDataExchange*) {}

    // ---- default message handlers (call Default()) ----
    int  OnCreate(LPCREATESTRUCT) { return static_cast<int>(Default()); }
    void OnDestroy() { Default(); }
    void OnPaint() { Default(); }
    void OnClose() { Default(); }
    void OnNcPaint() { Default(); }
    void OnSize(UINT, int, int) { Default(); }
    BOOL OnEraseBkgnd(CDC*) { return static_cast<BOOL>(Default()); }
    void OnLButtonDown(UINT, CPoint) { Default(); }
    void OnLButtonUp(UINT, CPoint) { Default(); }
    void OnLButtonDblClk(UINT, CPoint) { Default(); }
    void OnMButtonDown(UINT, CPoint) { Default(); }
    void OnRButtonDown(UINT, CPoint) { Default(); }
    void OnMouseMove(UINT, CPoint) { Default(); }
    BOOL OnMouseWheel(UINT, short, CPoint) { return static_cast<BOOL>(Default()); }
    void OnKeyDown(UINT, UINT, UINT) { Default(); }
    void OnChar(UINT, UINT, UINT) { Default(); }
    void OnSetFocus(CWnd*) { Default(); }
    void OnKillFocus(CWnd*) { Default(); }
    void OnContextMenu(CWnd*, CPoint) { Default(); }
    void OnTimer(UINT_PTR) { Default(); }
    void OnInitMenuPopup(CMenu*, UINT, BOOL) { Default(); }
    void OnSysColorChange() { Default(); }
    UINT OnPowerBroadcast(UINT, LPARAM) { return static_cast<UINT>(Default()); }
    BOOL OnNcActivate(BOOL) { return static_cast<BOOL>(Default()); }
    HBRUSH OnCtlColor(CDC*, CWnd*, UINT) { return reinterpret_cast<HBRUSH>(Default()); }
    LRESULT OnNcHitTest(CPoint) { return Default(); }
    void OnGetMinMaxInfo(MINMAXINFO*) { Default(); }
    void OnEnable(BOOL) { Default(); }
    BOOL OnSetCursor(CWnd*, UINT, UINT) { return static_cast<BOOL>(Default()); }
    void OnActivateApp(BOOL, DWORD) { Default(); }
    void OnCaptureChanged(CWnd*) { Default(); }
    void OnShowWindow(BOOL, UINT) { Default(); }
    void OnHScroll(UINT, UINT, CScrollBar*) { Default(); }
    void OnVScroll(UINT, UINT, CScrollBar*) { Default(); }
    UINT OnGetDlgCode() { return static_cast<UINT>(Default()); }
    void OnNcCalcSize(BOOL, NCCALCSIZE_PARAMS*) { Default(); }
    virtual int OnMouseActivate(CWnd*, UINT, UINT) { return (int)Default(); }
    BOOL IsTopParentActive() const
    {
        HWND hWnd = m_hWnd;
        while (hWnd != nullptr && (::GetWindowLongW(hWnd, GWL_STYLE) & WS_CHILD))
            hWnd = ::GetParent(hWnd);
        return hWnd != nullptr && hWnd == ::GetActiveWindow();
    }

    // owner-draw reflection targets
    virtual void DrawItem(LPDRAWITEMSTRUCT) {}
    virtual void MeasureItem(LPMEASUREITEMSTRUCT) {}
    // docked-bar sizing (frame layout queries this)
    virtual CSize CalcFixedLayout(BOOL /*bStretch*/, BOOL /*bHorz*/) { CRect rc; GetWindowRect(&rc); return rc.Size(); }

protected:
    WNDPROC m_pfnSuper = nullptr;
    int     m_bAutoDelete = 0;
    static LPCWSTR kProp() { return L"_WdsShimCWnd"; }
};

SHIM_IMPLEMENT_DYNAMIC_INLINE(CWnd, const_cast<CRuntimeClass*>(&CCmdTarget::classCCmdTarget))
inline CObject* __stdcall CWnd::CreateObject() { return new CWnd; }

inline CWnd* CWnd::FromHandle(HWND hWnd)
{
    if (hWnd == nullptr) return nullptr;
    if (CWnd* p = FromHandlePermanent(hWnd)) return p;
    thread_local HWND               s_mruKey = nullptr;
    thread_local CWnd*              s_mruVal = nullptr;
    thread_local std::unordered_map<HWND, std::unique_ptr<CWnd>> tempMap;
    if (hWnd == s_mruKey) { s_mruVal->m_hWnd = hWnd; return s_mruVal; }
    auto& slot = tempMap[hWnd];
    if (!slot) slot = std::make_unique<CWnd>();
    slot->m_hWnd = hWnd;
    s_mruKey = hWnd;
    s_mruVal = slot.get();
    return s_mruVal;
}

inline LRESULT WdsSendSelf(HWND hWnd, UINT msg, WPARAM wParam = 0, LPARAM lParam = 0)
{
    if (hWnd == nullptr) return 0;
    if (CWnd* pWnd = CWnd::FromHandlePermanent(hWnd))
        return pWnd->SendSelf(msg, wParam, lParam);
    return ::SendMessageW(hWnd, msg, wParam, lParam);
}

// Global scope helpers expected by the codebase that take CWnd via HWND.
inline BOOL IsWindow_(const CWnd* p) { return p && ::IsWindow(p->m_hWnd); }

// ---- Device-context wrappers tied to a window ----
class CClientDC final : public CDC
{
public:
    explicit CClientDC(CWnd* pWnd) : m_hWndDC(pWnd ? pWnd->m_hWnd : nullptr) { Attach(::GetDC(m_hWndDC)); }
    ~CClientDC() override { if (m_hDC) ::ReleaseDC(m_hWndDC, Detach()); }
private:
    HWND m_hWndDC;
};
class CWindowDC final : public CDC
{
public:
    explicit CWindowDC(CWnd* pWnd) : m_hWndDC(pWnd ? pWnd->m_hWnd : nullptr) { Attach(::GetWindowDC(m_hWndDC)); }
    ~CWindowDC() override { if (m_hDC) ::ReleaseDC(m_hWndDC, Detach()); }
private:
    HWND m_hWndDC;
};
class CPaintDC final : public CDC
{
public:
    PAINTSTRUCT m_ps{};
    explicit CPaintDC(CWnd* pWnd) : m_hWndDC(pWnd ? pWnd->m_hWnd : nullptr) { Attach(::BeginPaint(m_hWndDC, &m_ps)); }
    ~CPaintDC() override { if (m_hDC) { ::EndPaint(m_hWndDC, &m_ps); Detach(); } }
private:
    HWND m_hWndDC;
};

// -----------------------------------------------------------------------------
//  CMenu / CScrollBar / CImageList / CDataExchange
// -----------------------------------------------------------------------------
class CMenu final : public CObject
{
public:
    HMENU m_hMenu = nullptr;
    CMenu() = default;
    ~CMenu() override { if (m_hMenu && m_bAutoDestroy) ::DestroyMenu(m_hMenu); }

    operator HMENU() const noexcept { return m_hMenu; }
    HMENU GetSafeHmenu() const noexcept { return this ? m_hMenu : nullptr; }
    BOOL Attach(HMENU h) noexcept { m_hMenu = h; return h != nullptr; }
    HMENU Detach() noexcept { HMENU h = m_hMenu; m_hMenu = nullptr; return h; }

    BOOL CreatePopupMenu() { m_hMenu = ::CreatePopupMenu(); m_bAutoDestroy = true; return m_hMenu != nullptr; }
    BOOL CreateMenu() { m_hMenu = ::CreateMenu(); m_bAutoDestroy = true; return m_hMenu != nullptr; }
    BOOL LoadMenu(UINT nID) { DestroyMenu(); m_hMenu = ::LoadMenuW(AfxGetResourceHandle(), MAKEINTRESOURCEW(nID)); m_bAutoDestroy = true; return m_hMenu != nullptr; }
    BOOL DestroyMenu() noexcept { if (!m_hMenu) return FALSE; const BOOL r = ::DestroyMenu(m_hMenu); m_hMenu = nullptr; return r; }

    int  GetMenuItemCount() const noexcept { return ::GetMenuItemCount(m_hMenu); }
    UINT GetMenuItemID(int pos) const noexcept { return ::GetMenuItemID(m_hMenu, pos); }
    UINT GetMenuState(UINT id, UINT flags) const noexcept { return ::GetMenuState(m_hMenu, id, flags); }
    int  GetMenuString(UINT pos, CString& str, UINT flags) const
    { wchar_t buf[512]; buf[0] = 0; const int n = ::GetMenuStringW(m_hMenu, pos, buf, 512, flags); str = buf; return n; }
    int  GetMenuString(UINT pos, LPWSTR psz, int n, UINT flags) const noexcept { return ::GetMenuStringW(m_hMenu, pos, psz, n, flags); }
    CMenu* GetSubMenu(int pos) const;
    BOOL AppendMenu(UINT flags, UINT_PTR id = 0, LPCWSTR psz = nullptr) noexcept { return ::AppendMenuW(m_hMenu, flags, id, psz); }
    BOOL InsertMenu(UINT pos, UINT flags, UINT_PTR id = 0, LPCWSTR psz = nullptr) noexcept { return ::InsertMenuW(m_hMenu, pos, flags, id, psz); }
    BOOL ModifyMenu(UINT pos, UINT flags, UINT_PTR id = 0, LPCWSTR psz = nullptr) noexcept { return ::ModifyMenuW(m_hMenu, pos, flags, id, psz); }
    BOOL DeleteMenu(UINT pos, UINT flags) noexcept { return ::DeleteMenu(m_hMenu, pos, flags); }
    BOOL RemoveMenu(UINT pos, UINT flags) noexcept { return ::RemoveMenu(m_hMenu, pos, flags); }
    UINT EnableMenuItem(UINT id, UINT flags) noexcept { return ::EnableMenuItem(m_hMenu, id, flags); }
    UINT CheckMenuItem(UINT id, UINT flags) noexcept { return ::CheckMenuItem(m_hMenu, id, flags); }
    BOOL CheckMenuRadioItem(UINT f, UINT l, UINT c, UINT flags) noexcept { return ::CheckMenuRadioItem(m_hMenu, f, l, c, flags); }
    BOOL SetDefaultItem(UINT item, BOOL byPos = FALSE) noexcept { return ::SetMenuDefaultItem(m_hMenu, item, byPos); }
    BOOL GetMenuItemInfo(UINT item, MENUITEMINFOW* lpii, BOOL byPos) const { return ::GetMenuItemInfoW(m_hMenu, item, byPos, lpii); }
    BOOL SetMenuItemInfo(UINT item, MENUITEMINFOW* lpii, BOOL byPos) { return ::SetMenuItemInfoW(m_hMenu, item, byPos, lpii); }
    BOOL SetMenuItemBitmaps(UINT pos, UINT flags, CBitmap* u, CBitmap* c)
    { return ::SetMenuItemBitmaps(m_hMenu, pos, flags, u ? static_cast<HBITMAP>(u->m_hObject) : nullptr, c ? static_cast<HBITMAP>(c->m_hObject) : nullptr); }
    BOOL TrackPopupMenu(UINT flags, int x, int y, CWnd* pWnd, LPCRECT rc = nullptr)
    { return ::TrackPopupMenu(m_hMenu, flags, x, y, 0, pWnd ? pWnd->m_hWnd : nullptr, rc); }
    BOOL TrackPopupMenuEx(UINT flags, int x, int y, CWnd* pWnd, LPTPMPARAMS lptpm = nullptr)
    { return ::TrackPopupMenuEx(m_hMenu, flags, x, y, pWnd ? pWnd->m_hWnd : nullptr, lptpm); }

    static CMenu* FromHandle(HMENU h);
protected:
    bool m_bAutoDestroy = false;
};

inline CMenu* CMenu::FromHandle(HMENU h)
{
    if (h == nullptr) return nullptr;
    thread_local HMENU              s_mruKey = nullptr;
    thread_local CMenu*             s_mruVal = nullptr;
    thread_local std::unordered_map<HMENU, std::unique_ptr<CMenu>> map;
    if (h == s_mruKey) { s_mruVal->m_hMenu = h; return s_mruVal; }
    auto& s = map[h]; if (!s) s = std::make_unique<CMenu>(); s->m_hMenu = h;
    s_mruKey = h;
    s_mruVal = s.get();
    return s_mruVal;
}
inline CMenu* CMenu::GetSubMenu(int pos) const { HMENU h = ::GetSubMenu(m_hMenu, pos); return h ? CMenu::FromHandle(h) : nullptr; }

class CScrollBar final : public CWnd { public: CScrollBar() = default; };

class CImageList final : public CObject
{
public:
    HIMAGELIST m_hImageList = nullptr;
    CImageList() = default;
    ~CImageList() override { if (m_hImageList) ::ImageList_Destroy(m_hImageList); }
    operator HIMAGELIST() const noexcept { return m_hImageList; }
    HIMAGELIST GetSafeHandle() const noexcept { return this ? m_hImageList : nullptr; }
    BOOL Create(int cx, int cy, UINT flags, int nInitial, int nGrow) { m_hImageList = ::ImageList_Create(cx, cy, flags, nInitial, nGrow); return m_hImageList != nullptr; }
    BOOL Attach(HIMAGELIST h) { m_hImageList = h; return h != nullptr; }
    HIMAGELIST Detach() { HIMAGELIST h = m_hImageList; m_hImageList = nullptr; return h; }
    int Add(CBitmap* img, CBitmap* mask) { return ::ImageList_Add(m_hImageList, img ? static_cast<HBITMAP>(img->m_hObject) : nullptr, mask ? static_cast<HBITMAP>(mask->m_hObject) : nullptr); }
    int Add(HICON hIcon) { return ::ImageList_AddIcon(m_hImageList, hIcon); }
    int GetImageCount() const { return ::ImageList_GetImageCount(m_hImageList); }
};

class CDataExchange
{
public:
    BOOL  m_bSaveAndValidate;
    CWnd* m_pDlgWnd;
    HWND  m_hWndLastControl = nullptr;
    BOOL  m_bEditLastControl = FALSE;

    CDataExchange(CWnd* pDlgWnd, BOOL bSaveAndValidate)
        : m_bSaveAndValidate(bSaveAndValidate), m_pDlgWnd(pDlgWnd) {}
    HWND PrepareCtrl(int nID) { m_hWndLastControl = ::GetDlgItem(m_pDlgWnd->m_hWnd, nID); m_bEditLastControl = FALSE; return m_hWndLastControl; }
    HWND PrepareEditCtrl(int nID) { m_hWndLastControl = ::GetDlgItem(m_pDlgWnd->m_hWnd, nID); m_bEditLastControl = TRUE; return m_hWndLastControl; }
    [[noreturn]] void Fail() { throw CUserException{}; }
};

inline CMenu* CWnd::GetMenu() const { HMENU h = ::GetMenu(m_hWnd); return h ? CMenu::FromHandle(h) : nullptr; }
inline BOOL CWnd::SetMenu(CMenu* pMenu) { return ::SetMenu(m_hWnd, pMenu ? pMenu->m_hMenu : nullptr); }
inline CMenu* CWnd::GetSystemMenu(BOOL bRevert) const { HMENU h = ::GetSystemMenu(m_hWnd, bRevert); return h ? CMenu::FromHandle(h) : nullptr; }
inline BOOL CWnd::UpdateData(BOOL bSaveAndValidate)
{
    CDataExchange dx(this, bSaveAndValidate);
    if (!bSaveAndValidate) ++g_ddxOutputDepth;
    try
    {
        DoDataExchange(&dx);
    }
    catch (CUserException&)
    {
        if (!bSaveAndValidate) --g_ddxOutputDepth;
        return FALSE;
    }
    if (!bSaveAndValidate) --g_ddxOutputDepth;
    return TRUE;
}

inline BOOL CWnd::ExecuteDlgInit(LPCWSTR lpszResourceName)
{
    if (lpszResourceName == nullptr) return TRUE;

    const HINSTANCE hInst = AfxGetResourceHandle();
    const HRSRC hDlgInit = ::FindResourceW(hInst, lpszResourceName, RT_DLGINIT);
    if (hDlgInit == nullptr) return TRUE;

    const HGLOBAL hResource = ::LoadResource(hInst, hDlgInit);
    if (hResource == nullptr) return FALSE;

    LPVOID lpResource = ::LockResource(hResource);
    if (lpResource == nullptr) return FALSE;

    return ExecuteDlgInit(lpResource);
}

inline BOOL CWnd::ExecuteDlgInit(LPVOID lpResource)
{
    if (lpResource == nullptr) return TRUE;

    auto* p = static_cast<const BYTE*>(lpResource);
    auto readWord = [&p]
    {
        WORD value;
        std::memcpy(&value, p, sizeof(value));
        p += sizeof(value);
        return value;
    };
    auto readDword = [&p]
    {
        DWORD value;
        std::memcpy(&value, p, sizeof(value));
        p += sizeof(value);
        return value;
    };

    BOOL success = TRUE;
    while (success)
    {
        const WORD nIDC = readWord();
        if (nIDC == 0) break;

        WORD nMsg = readWord();
        const DWORD len = readDword();

        // Visual C++ stores these in the legacy Win16 form inside RT_DLGINIT.
        if (nMsg == 0x0401) nMsg = LB_ADDSTRING;
        else if (nMsg == 0x0403) nMsg = CB_ADDSTRING;

        if (nMsg == LB_ADDSTRING || nMsg == CB_ADDSTRING)
        {
            const LRESULT result = ::SendDlgItemMessageA(m_hWnd, nIDC, nMsg, 0, reinterpret_cast<LPARAM>(p));
            if (result == LB_ERR || result == LB_ERRSPACE) success = FALSE;
        }

        p += len;
    }

    return success;
}

// -----------------------------------------------------------------------------
//  Message-map dispatch thunks (defined now that CDC/CWnd/CMenu exist)
// -----------------------------------------------------------------------------
inline LRESULT WdsThunk_Command(CCmdTarget* p, AFX_PMSG pfn, WPARAM, LPARAM, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)()>(pfn))(); return 0; }
inline LRESULT WdsThunk_CommandU(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)(UINT)>(pfn))(static_cast<UINT>(w)); return 0; }
inline LRESULT WdsThunk_UpdateUI(CCmdTarget* p, AFX_PMSG pfn, WPARAM, LPARAM l, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)(CCmdUI*)>(pfn))(reinterpret_cast<CCmdUI*>(l)); return 0; }
inline LRESULT WdsThunk_Message(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM l, BOOL&)
{ return (p->*reinterpret_cast<LRESULT (CCmdTarget::*)(WPARAM, LPARAM)>(pfn))(w, l); }
inline LRESULT WdsThunk_Notify(CCmdTarget* p, AFX_PMSG pfn, WPARAM, LPARAM l, BOOL&)
{ auto* n = reinterpret_cast<WdsNotify*>(l); (p->*reinterpret_cast<void (CCmdTarget::*)(NMHDR*, LRESULT*)>(pfn))(n->pNMHDR, n->pResult); return 0; }
inline LRESULT WdsThunk_NotifyRange(CCmdTarget* p, AFX_PMSG pfn, WPARAM, LPARAM l, BOOL&)
{ auto* n = reinterpret_cast<WdsNotify*>(l); (p->*reinterpret_cast<void (CCmdTarget::*)(UINT, NMHDR*, LRESULT*)>(pfn))(n->id, n->pNMHDR, n->pResult); return 0; }
inline LRESULT WdsThunk_NotifyEx(CCmdTarget* p, AFX_PMSG pfn, WPARAM, LPARAM l, BOOL& b)
{ auto* n = reinterpret_cast<WdsNotify*>(l); b = (p->*reinterpret_cast<BOOL (CCmdTarget::*)(UINT, NMHDR*, LRESULT*)>(pfn))(n->id, n->pNMHDR, n->pResult); return 0; }
inline LRESULT WdsThunk_Create(CCmdTarget* p, AFX_PMSG pfn, WPARAM, LPARAM l, BOOL&)
{ return static_cast<LRESULT>((p->*reinterpret_cast<int (CCmdTarget::*)(LPCREATESTRUCT)>(pfn))(reinterpret_cast<LPCREATESTRUCT>(l))); }
inline LRESULT WdsThunk_Void(CCmdTarget* p, AFX_PMSG pfn, WPARAM, LPARAM, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)()>(pfn))(); return 0; }
inline LRESULT WdsThunk_Size(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM l, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)(UINT, int, int)>(pfn))(static_cast<UINT>(w), static_cast<int>(static_cast<short>(LOWORD(l))), static_cast<int>(static_cast<short>(HIWORD(l)))); return 0; }
inline LRESULT WdsThunk_EraseBkgnd(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM, BOOL&)
{ return (p->*reinterpret_cast<BOOL (CCmdTarget::*)(CDC*)>(pfn))(CDC::FromHandle(reinterpret_cast<HDC>(w))); }
inline LRESULT WdsThunk_MouseBtn(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM l, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)(UINT, CPoint)>(pfn))(static_cast<UINT>(w), CPoint(static_cast<short>(LOWORD(l)), static_cast<short>(HIWORD(l)))); return 0; }
inline LRESULT WdsThunk_MouseWheel(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM l, BOOL& b)
{ const BOOL r = (p->*reinterpret_cast<BOOL (CCmdTarget::*)(UINT, short, CPoint)>(pfn))(LOWORD(w), static_cast<short>(HIWORD(w)), CPoint(static_cast<short>(LOWORD(l)), static_cast<short>(HIWORD(l)))); b = r; return r; }
inline LRESULT WdsThunk_Key(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM l, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)(UINT, UINT, UINT)>(pfn))(static_cast<UINT>(w), static_cast<UINT>(l & 0xFFFF), static_cast<UINT>((l >> 16) & 0xFFFF)); return 0; }
inline LRESULT WdsThunk_Focus(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)(CWnd*)>(pfn))(CWnd::FromHandle(reinterpret_cast<HWND>(w))); return 0; }
inline LRESULT WdsThunk_ContextMenu(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM l, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)(CWnd*, CPoint)>(pfn))(CWnd::FromHandle(reinterpret_cast<HWND>(w)), CPoint(static_cast<short>(LOWORD(l)), static_cast<short>(HIWORD(l)))); return 0; }
inline LRESULT WdsThunk_Timer(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)(UINT_PTR)>(pfn))(static_cast<UINT_PTR>(w)); return 0; }
inline LRESULT WdsThunk_InitMenuPopup(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM l, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)(CMenu*, UINT, BOOL)>(pfn))(CMenu::FromHandle(reinterpret_cast<HMENU>(w)), static_cast<UINT>(LOWORD(l)), static_cast<BOOL>(HIWORD(l))); return 0; }
inline LRESULT WdsThunk_PowerBroadcast(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM l, BOOL&)
{ return static_cast<LRESULT>((p->*reinterpret_cast<UINT (CCmdTarget::*)(UINT, LPARAM)>(pfn))(static_cast<UINT>(w), l)); }
inline LRESULT WdsThunk_NcActivate(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM, BOOL&)
{ return static_cast<LRESULT>((p->*reinterpret_cast<BOOL (CCmdTarget::*)(BOOL)>(pfn))(static_cast<BOOL>(w))); }
inline LRESULT WdsThunk_CtlColor(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM l, BOOL& b)
{
    const UINT nCtlColor = g_currentMsg.message - WM_CTLCOLORMSGBOX;
    const HBRUSH br = (p->*reinterpret_cast<HBRUSH (CCmdTarget::*)(CDC*, CWnd*, UINT)>(pfn))(CDC::FromHandle(reinterpret_cast<HDC>(w)), CWnd::FromHandle(reinterpret_cast<HWND>(l)), nCtlColor);
    if (br == nullptr) { b = FALSE; return 0; }
    return reinterpret_cast<LRESULT>(br);
}
inline LRESULT WdsThunk_NcHitTest(CCmdTarget* p, AFX_PMSG pfn, WPARAM, LPARAM l, BOOL&)
{ return (p->*reinterpret_cast<LRESULT (CCmdTarget::*)(CPoint)>(pfn))(CPoint(static_cast<short>(LOWORD(l)), static_cast<short>(HIWORD(l)))); }
inline LRESULT WdsThunk_GetMinMaxInfo(CCmdTarget* p, AFX_PMSG pfn, WPARAM, LPARAM l, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)(MINMAXINFO*)>(pfn))(reinterpret_cast<MINMAXINFO*>(l)); return 0; }
inline LRESULT WdsThunk_Enable(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)(BOOL)>(pfn))(static_cast<BOOL>(w)); return 0; }
inline LRESULT WdsThunk_SetCursor(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM l, BOOL& b)
{ const BOOL r = (p->*reinterpret_cast<BOOL (CCmdTarget::*)(CWnd*, UINT, UINT)>(pfn))(CWnd::FromHandle(reinterpret_cast<HWND>(w)), LOWORD(l), HIWORD(l)); b = r; return r; }
inline LRESULT WdsThunk_ActivateApp(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM l, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)(BOOL, DWORD)>(pfn))(static_cast<BOOL>(w), static_cast<DWORD>(l)); return 0; }
inline LRESULT WdsThunk_CaptureChanged(CCmdTarget* p, AFX_PMSG pfn, WPARAM, LPARAM l, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)(CWnd*)>(pfn))(CWnd::FromHandle(reinterpret_cast<HWND>(l))); return 0; }
inline LRESULT WdsThunk_ShowWindow(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM l, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)(BOOL, UINT)>(pfn))(static_cast<BOOL>(w), static_cast<UINT>(l)); return 0; }
inline LRESULT WdsThunk_Scroll(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM l, BOOL&)
{ CScrollBar* pBar = l ? reinterpret_cast<CScrollBar*>(CWnd::FromHandle(reinterpret_cast<HWND>(l))) : nullptr; (p->*reinterpret_cast<void (CCmdTarget::*)(UINT, UINT, CScrollBar*)>(pfn))(LOWORD(w), HIWORD(w), pBar); return 0; }
inline LRESULT WdsThunk_GetDlgCode(CCmdTarget* p, AFX_PMSG pfn, WPARAM, LPARAM, BOOL&)
{ return static_cast<LRESULT>((p->*reinterpret_cast<UINT (CCmdTarget::*)()>(pfn))()); }
inline LRESULT WdsThunk_NcCalcSize(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM l, BOOL&)
{ (p->*reinterpret_cast<void (CCmdTarget::*)(BOOL, NCCALCSIZE_PARAMS*)>(pfn))(static_cast<BOOL>(w), reinterpret_cast<NCCALCSIZE_PARAMS*>(l)); return 0; }
inline LRESULT WdsThunk_MouseActivate(CCmdTarget* p, AFX_PMSG pfn, WPARAM w, LPARAM l, BOOL&)
{ CWnd* pTop = CWnd::FromHandle(reinterpret_cast<HWND>(w)); return static_cast<LRESULT>((p->*reinterpret_cast<int (CCmdTarget::*)(CWnd*, UINT, UINT)>(pfn))(pTop, LOWORD(l), HIWORD(l))); }

// -----------------------------------------------------------------------------
//  Dispatch implementations
// -----------------------------------------------------------------------------
inline BOOL CCmdTarget::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
    for (const AFX_MSGMAP* pMap = GetMessageMap(); pMap != nullptr; pMap = pMap->pfnGetBaseMap())
    {
        for (const AFX_MSGMAP_ENTRY* e = pMap->lpEntries; e->pThunk != nullptr; ++e)
        {
            if (e->nMessage != WM_COMMAND) continue;
            if (e->nID == WdsReflectID) continue;
            if (e->nCode != static_cast<UINT>(nCode)) continue;
            if (nID < e->nID || nID > e->nLastID) continue;
            if (pHandlerInfo != nullptr)
            {
                pHandlerInfo->pTarget = this;
                union { AFX_PMSG pfn; void* voidptr; } u = { e->pfn };
                pHandlerInfo->pmf = u.voidptr;
                return TRUE;
            }
            BOOL bHandled = TRUE;
            e->pThunk(this, e->pfn, static_cast<WPARAM>(nID), reinterpret_cast<LPARAM>(pExtra), bHandled);
            if (bHandled) return TRUE;
        }
    }
    return FALSE;
}

inline BOOL CWnd::OnWndMsg(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
    if (msg == WM_COMMAND)
    {
        if (OnCommand(wParam, lParam)) { if (pResult) *pResult = 0; return TRUE; }
        return FALSE;
    }
    if (msg == WM_NOTIFY)
    {
        LRESULT r = 0;
        if (OnNotify(wParam, lParam, &r)) { if (pResult) *pResult = r; return TRUE; }
        return FALSE;
    }
    if (msg == WM_DRAWITEM)
    {
        auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (dis->CtlType != ODT_MENU && dis->hwndItem)
            if (CWnd* pChild = FromHandlePermanent(dis->hwndItem))
            {
                // Publish item rect so GetItemRect/GetSubItemRect can avoid SendMessage.
                const AfxDrawItemCtx savedCtx = g_drawItemCtx;
                g_drawItemCtx.hWnd     = dis->hwndItem;
                g_drawItemCtx.item     = static_cast<int>(dis->itemID);
                g_drawItemCtx.rcItem   = dis->rcItem;
                g_drawItemCtx.colCount = 0;  // mark column cache as not yet built
                pChild->DrawItem(dis);
                g_drawItemCtx = savedCtx;
                if (pResult) *pResult = TRUE; return TRUE;
            }
    }
    if (msg == WM_MEASUREITEM)
    {
        auto* mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (mis->CtlType != ODT_MENU)
            if (HWND hCtl = ::GetDlgItem(m_hWnd, static_cast<int>(mis->CtlID)))
                if (CWnd* pChild = FromHandlePermanent(hCtl))
                { pChild->MeasureItem(mis); if (pResult) *pResult = TRUE; return TRUE; }
    }
    if (msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC)
    {
        for (const AFX_MSGMAP* pMap = GetMessageMap(); pMap != nullptr; pMap = pMap->pfnGetBaseMap())
            for (const AFX_MSGMAP_ENTRY* e = pMap->lpEntries; e->pThunk != nullptr; ++e)
            {
                if (e->nMessage != WM_CTLCOLOR) continue;
                BOOL b = TRUE;
                const LRESULT r = e->pThunk(this, e->pfn, wParam, lParam, b);
                if (b) { if (pResult) *pResult = r; return TRUE; }
            }
        return FALSE;
    }
    for (const AFX_MSGMAP* pMap = GetMessageMap(); pMap != nullptr; pMap = pMap->pfnGetBaseMap())
        for (const AFX_MSGMAP_ENTRY* e = pMap->lpEntries; e->pThunk != nullptr; ++e)
        {
            bool match;
            if (e->pReg) match = (*e->pReg != 0 && *e->pReg == msg);
            else match = (e->nMessage == msg && e->nMessage != WM_COMMAND && e->nMessage != WM_NOTIFY && e->nMessage != WM_CTLCOLOR);
            if (!match) continue;
            BOOL b = TRUE;
            const LRESULT r = e->pThunk(this, e->pfn, wParam, lParam, b);
            if (b) { if (pResult) *pResult = r; return TRUE; }
        }
    return FALSE;
}

inline BOOL CWnd::OnCommand(WPARAM wParam, LPARAM lParam)
{
    const UINT nID = LOWORD(wParam);
    const int nCode = HIWORD(wParam);
    const HWND hWndCtrl = reinterpret_cast<HWND>(lParam);
    if (hWndCtrl != nullptr && g_ddxOutputDepth > 0) return TRUE;

    if (hWndCtrl != nullptr)
    {
        CWnd* pCtrl = FromHandlePermanent(hWndCtrl);
        if (pCtrl && pCtrl != this && pCtrl->ReflectChildCommand(nCode)) return TRUE;
    }
    if (OnCmdMsg(nID, nCode, nullptr, nullptr)) return TRUE;

    if (hWndCtrl != nullptr && nCode == CBN_SELCHANGE)
    {
        wchar_t cls[16];
        if (::GetClassNameW(hWndCtrl, cls, _countof(cls)) != 0 &&
            _wcsicmp(cls, WC_COMBOBOXW) == 0)
        {
            return OnCmdMsg(nID, CBN_SELENDOK, nullptr, nullptr);
        }
    }

    return FALSE;
}

inline BOOL CWnd::ReflectChildCommand(int code)
{
    for (const AFX_MSGMAP* pMap = GetMessageMap(); pMap != nullptr; pMap = pMap->pfnGetBaseMap())
        for (const AFX_MSGMAP_ENTRY* e = pMap->lpEntries; e->pThunk != nullptr; ++e)
            if (e->nMessage == WM_COMMAND && e->nID == WdsReflectID && e->nCode == static_cast<UINT>(code))
            { BOOL b = TRUE; e->pThunk(this, e->pfn, 0, 0, b); if (b) return TRUE; }
    return FALSE;
}

inline BOOL CWnd::OnNotify(WPARAM, LPARAM lParam, LRESULT* pResult)
{
    NMHDR* pNMHDR = reinterpret_cast<NMHDR*>(lParam);
    if (pNMHDR == nullptr) return FALSE;
    if (pNMHDR->hwndFrom)
    {
        CWnd* pCtrl = FromHandlePermanent(pNMHDR->hwndFrom);
        if (pCtrl && pCtrl != this && pCtrl->ReflectChildNotify(pNMHDR, pResult)) return TRUE;
    }
    WdsNotify n{ pNMHDR, pResult, static_cast<UINT>(pNMHDR->idFrom) };
    for (const AFX_MSGMAP* pMap = GetMessageMap(); pMap != nullptr; pMap = pMap->pfnGetBaseMap())
        for (const AFX_MSGMAP_ENTRY* e = pMap->lpEntries; e->pThunk != nullptr; ++e)
        {
            if (e->nMessage != WM_NOTIFY || e->nID == WdsReflectID) continue;
            if (e->nCode != pNMHDR->code) continue;
            const UINT idFrom = static_cast<UINT>(pNMHDR->idFrom);
            if (idFrom < e->nID || idFrom > e->nLastID) continue;
            BOOL b = TRUE;
            e->pThunk(this, e->pfn, 0, reinterpret_cast<LPARAM>(&n), b);
            if (b) return TRUE;
        }
    return FALSE;
}

inline BOOL CWnd::ReflectChildNotify(NMHDR* pNMHDR, LRESULT* pResult)
{
    WdsNotify n{ pNMHDR, pResult, static_cast<UINT>(pNMHDR->idFrom) };
    for (const AFX_MSGMAP* pMap = GetMessageMap(); pMap != nullptr; pMap = pMap->pfnGetBaseMap())
        for (const AFX_MSGMAP_ENTRY* e = pMap->lpEntries; e->pThunk != nullptr; ++e)
            if (e->nMessage == WM_NOTIFY && e->nID == WdsReflectID && e->nCode == pNMHDR->code)
            { BOOL b = TRUE; e->pThunk(this, e->pfn, 0, reinterpret_cast<LPARAM>(&n), b); if (b) return TRUE; }
    return FALSE;
}

inline LRESULT CALLBACK AfxWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    CWnd* pWnd = CWnd::FromHandlePermanent(hWnd);
    if (pWnd == nullptr && g_pWndInit != nullptr)
    {
        pWnd = g_pWndInit; g_pWndInit = nullptr; pWnd->Attach(hWnd);
    }
    if (pWnd == nullptr) return ::DefWindowProcW(hWnd, msg, wParam, lParam);
    return pWnd->WindowProcEntry(msg, wParam, lParam);
}

inline LPCWSTR AFXAPI WdsDefaultWndClass()
{
    static const wchar_t* name = L"WdsAfxWnd";
    static const bool reg = []
    {
        WNDCLASSEXW wc{ sizeof(wc) };
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = AfxWndProc;
        wc.hInstance = AfxGetInstanceHandle();
        wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = name;
        ::RegisterClassExW(&wc);
        return true;
    }();
    (void)reg;
    return name;
}

inline LPCWSTR AFXAPI AfxRegisterWndClass(UINT classStyle, HCURSOR hCursor, HBRUSH hbrBackground, HICON hIcon)
{
    static std::map<std::wstring, std::wstring> registered;
    wchar_t name[160];
    swprintf_s(name, L"WdsAfx.%x.%p.%p.%p", classStyle,
        static_cast<void*>(hCursor), static_cast<void*>(hbrBackground), static_cast<void*>(hIcon));
    if (const auto it = registered.find(name); it != registered.end()) return it->second.c_str();
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = classStyle;
    wc.lpfnWndProc = AfxWndProc;
    wc.hInstance = AfxGetInstanceHandle();
    wc.hCursor = hCursor ? hCursor : ::LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = hbrBackground;
    wc.hIcon = hIcon;
    wc.lpszClassName = name;
    ::RegisterClassExW(&wc);
    return registered.emplace(name, name).first->second.c_str();
}

inline BOOL CWnd::PreCreateWindow(CREATESTRUCT& cs)
{
    if (cs.lpszClass == nullptr) cs.lpszClass = WdsDefaultWndClass();
    return TRUE;
}
inline void CWnd::PostNcDestroy() {}
inline void CWnd::CenterWindow(CWnd* pAlternate)
{
    HWND hParent = pAlternate ? pAlternate->m_hWnd : ::GetParent(m_hWnd);
    if (hParent == nullptr) hParent = ::GetDesktopWindow();
    RECT rcParent, rcWnd; ::GetWindowRect(hParent, &rcParent); ::GetWindowRect(m_hWnd, &rcWnd);
    const int w = rcWnd.right - rcWnd.left, h = rcWnd.bottom - rcWnd.top;
    const int x = rcParent.left + ((rcParent.right - rcParent.left) - w) / 2;
    const int y = rcParent.top + ((rcParent.bottom - rcParent.top) - h) / 2;
    ::SetWindowPos(m_hWnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// -----------------------------------------------------------------------------
//  CCmdUI implementation (menu / generic control)
// -----------------------------------------------------------------------------
inline void CCmdUI::Enable(BOOL bOn)
{
    m_bEnableChanged = TRUE;
    if (m_pMenu)
        m_pMenu->EnableMenuItem(m_nIndex, MF_BYPOSITION | (bOn ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)));
    else if (m_pOther)
        m_pOther->EnableWindow(bOn);
}
inline void CCmdUI::SetCheck(int nCheck)
{
    if (m_pMenu)
        m_pMenu->CheckMenuItem(m_nIndex, MF_BYPOSITION | (nCheck ? MF_CHECKED : MF_UNCHECKED));
    else if (m_pOther)
        m_pOther->SendSelf(BM_SETCHECK, static_cast<WPARAM>(nCheck));
}
inline void CCmdUI::SetRadio(BOOL bOn) { SetCheck(bOn ? 1 : 0); }
inline void CCmdUI::SetText(LPCWSTR lpszText)
{
    if (m_pMenu && lpszText)
    {
        const UINT state = ::GetMenuState(m_pMenu->m_hMenu, m_nIndex, MF_BYPOSITION);
        m_pMenu->ModifyMenu(m_nIndex, MF_BYPOSITION | MF_STRING | (state & ~(MF_BITMAP | MF_OWNERDRAW)), m_nID, lpszText);
    }
    else if (m_pOther && lpszText)
        m_pOther->SetWindowText(lpszText);
}
inline BOOL CCmdUI::DoUpdate(CCmdTarget* pTarget, BOOL bDisableIfNoHndler)
{
    m_bEnableChanged = FALSE;
    BOOL handled = pTarget ? pTarget->OnCmdMsg(m_nID, WdsCN_UPDATE_COMMAND_UI, this, nullptr) : FALSE;
    if (!handled && bDisableIfNoHndler && !m_bEnableChanged)
    {
        AFX_CMDHANDLERINFO info{};
        if (pTarget && pTarget->OnCmdMsg(m_nID, WdsCN_COMMAND, nullptr, &info))
        {
            Enable(TRUE);
            handled = TRUE;
        }
        else
        {
            Enable(FALSE);
        }
    }
    return handled;
}

// -----------------------------------------------------------------------------
//  Standard controls
// -----------------------------------------------------------------------------
class CStatic : public CWnd
{
public:
    static const CRuntimeClass classCStatic;
    CRuntimeClass* GetRuntimeClass() const override;
    CStatic() = default;
    BOOL Create(LPCWSTR lpszText, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID = 0xffff)
    { return CWnd::CreateEx(0, WC_STATICW, lpszText, dwStyle, rect, pParentWnd, nID); }
    HICON   SetIcon(HICON hIcon) { return reinterpret_cast<HICON>(SendSelf(STM_SETICON, reinterpret_cast<WPARAM>(hIcon))); }
    HICON   GetIcon() const { return reinterpret_cast<HICON>(SendSelf(STM_GETICON)); }
    HBITMAP SetBitmap(HBITMAP hBmp) { return reinterpret_cast<HBITMAP>(SendSelf(STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(hBmp))); }
};
SHIM_IMPLEMENT_DYNAMIC_INLINE(CStatic, RUNTIME_CLASS(CWnd))

class CButton : public CWnd
{
public:
    CButton() = default;
    BOOL Create(LPCWSTR lpszCaption, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
    { return CWnd::CreateEx(0, WC_BUTTONW, lpszCaption, dwStyle, rect, pParentWnd, nID); }
    UINT GetCheck() const { return static_cast<UINT>(SendSelf(BM_GETCHECK)); }
    void SetCheck(int nCheck) { SendSelf(BM_SETCHECK, static_cast<WPARAM>(nCheck)); }
    UINT GetState() const { return static_cast<UINT>(SendSelf(BM_GETSTATE)); }
    void SetState(BOOL b) { SendSelf(BM_SETSTATE, static_cast<WPARAM>(b)); }
    UINT GetButtonStyle() const { return static_cast<UINT>(GetStyle() & 0xFF); }
    void SetButtonStyle(UINT nStyle, BOOL bRedraw = TRUE) { SendSelf(BM_SETSTYLE, nStyle, MAKELPARAM(bRedraw, 0)); }
    HICON SetIcon(HICON hIcon) { return reinterpret_cast<HICON>(SendSelf(BM_SETIMAGE, IMAGE_ICON, reinterpret_cast<LPARAM>(hIcon))); }
    HBITMAP SetBitmap(HBITMAP hBmp) { return reinterpret_cast<HBITMAP>(SendSelf(BM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(hBmp))); }
};

class CEdit : public CWnd
{
public:
    CEdit() = default;
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
    { return CWnd::CreateEx(0, WC_EDITW, nullptr, dwStyle, rect, pParentWnd, nID); }
    void SetSel(int s, int e, BOOL = FALSE) { SendSelf(EM_SETSEL, static_cast<WPARAM>(s), static_cast<LPARAM>(e)); }
    void SetSel(DWORD sel, BOOL = FALSE) { SendSelf(EM_SETSEL, LOWORD(sel), HIWORD(sel)); }
    void GetSel(int& nStart, int& nEnd) const { DWORD s = static_cast<DWORD>(SendSelf(EM_GETSEL)); nStart = LOWORD(s); nEnd = HIWORD(s); }
    DWORD GetSel() const { return static_cast<DWORD>(SendSelf(EM_GETSEL)); }
    void ReplaceSel(LPCWSTR psz, BOOL bCanUndo = FALSE) { SendSelf(EM_REPLACESEL, static_cast<WPARAM>(bCanUndo), reinterpret_cast<LPARAM>(psz)); }
    void SetReadOnly(BOOL b = TRUE) { SendSelf(EM_SETREADONLY, static_cast<WPARAM>(b)); }
    void LimitText(int n = 0) { SendSelf(EM_SETLIMITTEXT, static_cast<WPARAM>(n)); }
    int  LineLength(int line = -1) const { return static_cast<int>(SendSelf(EM_LINELENGTH, static_cast<WPARAM>(line))); }
    void SetCueBanner(LPCWSTR psz, BOOL focused = FALSE) { SendSelf(EM_SETCUEBANNER, static_cast<WPARAM>(focused), reinterpret_cast<LPARAM>(psz)); }
};

class CComboBox final : public CWnd
{
public:
    CComboBox() = default;
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
    { return CWnd::CreateEx(0, WC_COMBOBOXW, nullptr, dwStyle, rect, pParentWnd, nID); }
    int  AddString(LPCWSTR psz) { return static_cast<int>(SendSelf(CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(psz))); }
    int  InsertString(int i, LPCWSTR psz) { return static_cast<int>(SendSelf(CB_INSERTSTRING, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(psz))); }
    int  DeleteString(UINT i) { return static_cast<int>(SendSelf(CB_DELETESTRING, static_cast<WPARAM>(i))); }
    void ResetContent() { SendSelf(CB_RESETCONTENT); }
    int  GetCount() const { return static_cast<int>(SendSelf(CB_GETCOUNT)); }
    int  GetCurSel() const { return static_cast<int>(SendSelf(CB_GETCURSEL)); }
    int  SetCurSel(int i) { return static_cast<int>(SendSelf(CB_SETCURSEL, static_cast<WPARAM>(i))); }
    DWORD_PTR GetItemData(int i) const { return static_cast<DWORD_PTR>(SendSelf(CB_GETITEMDATA, static_cast<WPARAM>(i))); }
    int  SetItemData(int i, DWORD_PTR data) { return static_cast<int>(SendSelf(CB_SETITEMDATA, static_cast<WPARAM>(i), static_cast<LPARAM>(data))); }
    BOOL GetDroppedState() const { return static_cast<BOOL>(SendSelf(CB_GETDROPPEDSTATE)); }
    int  GetLBText(int i, LPWSTR psz) const { return static_cast<int>(SendSelf(CB_GETLBTEXT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(psz))); }
    void GetLBText(int i, CString& str) const { const int n = GetLBTextLen(i); if (n == CB_ERR) { str.Empty(); return; } GetLBText(i, str.GetBuffer(n + 1)); str.ReleaseBuffer(n); }
    int  GetLBTextLen(int i) const { return static_cast<int>(SendSelf(CB_GETLBTEXTLEN, static_cast<WPARAM>(i))); }
    int  FindStringExact(int start, LPCWSTR psz) const { return static_cast<int>(SendSelf(CB_FINDSTRINGEXACT, static_cast<WPARAM>(start), reinterpret_cast<LPARAM>(psz))); }
    int  SetItemHeight(int i, UINT h) { return static_cast<int>(SendSelf(CB_SETITEMHEIGHT, static_cast<WPARAM>(i), static_cast<LPARAM>(h))); }
    void SetDroppedWidth(int w) { SendSelf(CB_SETDROPPEDWIDTH, static_cast<WPARAM>(w)); }
};

class CListBox final : public CWnd
{
public:
    CListBox() = default;
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
    { return CWnd::CreateEx(0, WC_LISTBOXW, nullptr, dwStyle, rect, pParentWnd, nID); }
    int  AddString(LPCWSTR psz) { return static_cast<int>(SendSelf(LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(psz))); }
    int  InsertString(int i, LPCWSTR psz) { return static_cast<int>(SendSelf(LB_INSERTSTRING, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(psz))); }
    int  DeleteString(UINT i) { return static_cast<int>(SendSelf(LB_DELETESTRING, static_cast<WPARAM>(i))); }
    void ResetContent() { SendSelf(LB_RESETCONTENT); }
    int  GetCount() const { return static_cast<int>(SendSelf(LB_GETCOUNT)); }
    int  GetCurSel() const { return static_cast<int>(SendSelf(LB_GETCURSEL)); }
    int  SetCurSel(int i) { return static_cast<int>(SendSelf(LB_SETCURSEL, static_cast<WPARAM>(i))); }
    int  GetText(int i, LPWSTR psz) const { return static_cast<int>(SendSelf(LB_GETTEXT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(psz))); }
    void GetText(int i, CString& str) const { const int n = GetTextLen(i); if (n == LB_ERR) { str.Empty(); return; } GetText(i, str.GetBuffer(n + 1)); str.ReleaseBuffer(n); }
    int  GetTextLen(int i) const { return static_cast<int>(SendSelf(LB_GETTEXTLEN, static_cast<WPARAM>(i))); }
    DWORD_PTR GetItemData(int i) const { return static_cast<DWORD_PTR>(SendSelf(LB_GETITEMDATA, static_cast<WPARAM>(i))); }
    int  SetItemData(int i, DWORD_PTR data) { return static_cast<int>(SendSelf(LB_SETITEMDATA, static_cast<WPARAM>(i), static_cast<LPARAM>(data))); }
};

class CProgressCtrl : public CWnd
{
public:
    CProgressCtrl() = default;
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParent, UINT nID)
    { return CreateEx(0, PROGRESS_CLASSW, nullptr, dwStyle, rect, pParent, nID); }
    int  SetPos(int n) { return static_cast<int>(SendSelf(PBM_SETPOS, static_cast<WPARAM>(n))); }
    int  GetPos() const { return static_cast<int>(SendSelf(PBM_GETPOS)); }
    void SetRange(short lower, short upper) { SendSelf(PBM_SETRANGE, 0, MAKELPARAM(lower, upper)); }
    void SetRange32(int lower, int upper) { SendSelf(PBM_SETRANGE32, static_cast<WPARAM>(lower), static_cast<LPARAM>(upper)); }
    void GetRange(int& lower, int& upper) const { PBRANGE r{}; SendSelf(PBM_GETRANGE, TRUE, reinterpret_cast<LPARAM>(&r)); lower = r.iLow; upper = r.iHigh; }
    int  SetStep(int n) { return static_cast<int>(SendSelf(PBM_SETSTEP, static_cast<WPARAM>(n))); }
    int  StepIt() { return static_cast<int>(SendSelf(PBM_STEPIT)); }
    COLORREF SetBkColor(COLORREF c) { return static_cast<COLORREF>(SendSelf(PBM_SETBKCOLOR, 0, static_cast<LPARAM>(c))); }
    COLORREF SetBarColor(COLORREF c) { return static_cast<COLORREF>(SendSelf(PBM_SETBARCOLOR, 0, static_cast<LPARAM>(c))); }
    void SetMarquee(BOOL on, int ms) { SendSelf(PBM_SETMARQUEE, static_cast<WPARAM>(on), static_cast<LPARAM>(ms)); }
};

class CSliderCtrl final : public CWnd
{
public:
    CSliderCtrl() = default;
    void SetRange(int nMin, int nMax, BOOL bRedraw = FALSE) { SendSelf(TBM_SETRANGE, static_cast<WPARAM>(bRedraw), MAKELPARAM(nMin, nMax)); }
    void SetRangeMin(int n, BOOL bRedraw = FALSE) { SendSelf(TBM_SETRANGEMIN, static_cast<WPARAM>(bRedraw), static_cast<LPARAM>(n)); }
    void SetRangeMax(int n, BOOL bRedraw = FALSE) { SendSelf(TBM_SETRANGEMAX, static_cast<WPARAM>(bRedraw), static_cast<LPARAM>(n)); }
    int  GetPos() const { return static_cast<int>(SendSelf(TBM_GETPOS)); }
    void SetPos(int n) { SendSelf(TBM_SETPOS, TRUE, static_cast<LPARAM>(n)); }
    void SetTicFreq(int n) { SendSelf(TBM_SETTICFREQ, static_cast<WPARAM>(n)); }
    void SetPageSize(int n) { SendSelf(TBM_SETPAGESIZE, 0, static_cast<LPARAM>(n)); }
    void SetLineSize(int n) { SendSelf(TBM_SETLINESIZE, 0, static_cast<LPARAM>(n)); }
};

class CRichEditCtrl final : public CWnd
{
public:
    CRichEditCtrl() = default;
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
    { return CWnd::CreateEx(0, MSFTEDIT_CLASS, nullptr, dwStyle, rect, pParentWnd, nID); }
    DWORD SetEventMask(DWORD mask) { return static_cast<DWORD>(SendSelf(EM_SETEVENTMASK, 0, static_cast<LPARAM>(mask))); }
    void  SetSel(long s, long e) { CHARRANGE cr{ s, e }; SendSelf(EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&cr)); }
    void  SetBackgroundColor(BOOL bSysColor, COLORREF c) { SendSelf(EM_SETBKGNDCOLOR, static_cast<WPARAM>(bSysColor), static_cast<LPARAM>(c)); }
    void  SetReadOnly(BOOL b = TRUE) { SendSelf(EM_SETREADONLY, static_cast<WPARAM>(b)); }
    long  GetTextLength() const { return static_cast<long>(SendSelf(WM_GETTEXTLENGTH)); }
    void  SetAutoURLDetect(BOOL b = TRUE) { SendSelf(EM_AUTOURLDETECT, static_cast<WPARAM>(b)); }
    DWORD SetOptions(WORD op, DWORD mask) { return static_cast<DWORD>(SendSelf(EM_SETOPTIONS, op, static_cast<LPARAM>(mask))); }
    void  HideSelection(BOOL bHide, BOOL bPerm) { SendSelf(EM_HIDESELECTION, static_cast<WPARAM>(bHide), static_cast<LPARAM>(bPerm)); }
    BOOL  SetDefaultCharFormat(CHARFORMAT2W& cf) { return static_cast<BOOL>(SendSelf(EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&cf))); }
    long  GetTextRange(long cpMin, long cpMax, CString& str) const
    {
        std::wstring buf(static_cast<size_t>(std::max(0L, cpMax - cpMin)) + 1, L'\0');
        TEXTRANGEW tr{}; tr.chrg.cpMin = cpMin; tr.chrg.cpMax = cpMax; tr.lpstrText = buf.data();
        const long n = static_cast<long>(SendSelf(EM_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&tr)));
        buf.resize(static_cast<size_t>(std::max(0L, n))); str = buf.c_str(); return n;
    }
    DWORD GetSel() const { CHARRANGE cr{}; SendSelf(EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&cr)); return MAKELONG(cr.cpMin, cr.cpMax); }
    long  SetSelectionCharFormat(CHARFORMAT2W& cf) { return static_cast<long>(SendSelf(EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf))); }
};

class CToolTipCtrl final : public CWnd
{
public:
    CToolTipCtrl() = default;
    BOOL Create(CWnd* pParentWnd, DWORD dwStyle = 0)
    {
        return CWnd::CreateEx(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX | dwStyle,
            CRect(), pParentWnd, 0);
    }
    BOOL AddTool(CWnd* pWnd, LPCWSTR lpszText, LPCRECT = nullptr, UINT_PTR = 0)
    {
        if (pWnd == nullptr) return FALSE;
        TTTOOLINFOW ti{}; ti.cbSize = sizeof(ti);
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = pWnd->m_hWnd;
        ti.uId = reinterpret_cast<UINT_PTR>(pWnd->m_hWnd);
        ti.lpszText = const_cast<LPWSTR>(lpszText);
        return static_cast<BOOL>(SendSelf(TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti)));
    }
    void Activate(BOOL bActivate) { SendSelf(TTM_ACTIVATE, static_cast<WPARAM>(bActivate)); }
    void SetMaxTipWidth(int w) { SendSelf(TTM_SETMAXTIPWIDTH, 0, static_cast<LPARAM>(w)); }
    void RelayEvent(MSG* pMsg) { SendSelf(TTM_RELAYEVENT, 0, reinterpret_cast<LPARAM>(pMsg)); }
};

class CHeaderCtrl final : public CWnd
{
public:
    CHeaderCtrl() = default;
    int  GetItemCount() const { return static_cast<int>(SendSelf(HDM_GETITEMCOUNT)); }
    BOOL GetItem(int i, HDITEMW* p) const { return static_cast<BOOL>(SendSelf(HDM_GETITEMW, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(p))); }
    BOOL SetItem(int i, HDITEMW* p) { return static_cast<BOOL>(SendSelf(HDM_SETITEMW, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(p))); }
    BOOL GetItemRect(int i, LPRECT r) const { return static_cast<BOOL>(SendSelf(HDM_GETITEMRECT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(r))); }
    BOOL GetOrderArray(LPINT p, int n) { return static_cast<BOOL>(SendSelf(HDM_GETORDERARRAY, static_cast<WPARAM>(n), reinterpret_cast<LPARAM>(p))); }
    BOOL SetOrderArray(int n, LPINT p) { return static_cast<BOOL>(SendSelf(HDM_SETORDERARRAY, static_cast<WPARAM>(n), reinterpret_cast<LPARAM>(p))); }
    int  Layout(HDLAYOUT* p) { return static_cast<int>(SendSelf(HDM_LAYOUT, 0, reinterpret_cast<LPARAM>(p))); }
};

class CListCtrl : public CWnd
{
public:
    static const CRuntimeClass classCListCtrl;
    CRuntimeClass* GetRuntimeClass() const override;
    CListCtrl() = default;
    int  FindItem(LVFINDINFOW* pInfo, int nStart = -1) const { return (int)SendSelf(LVM_FINDITEMW, (WPARAM)nStart, (LPARAM)pInfo); }
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParent, UINT nID) { return CreateEx(0, dwStyle, rect, pParent, nID); }
    BOOL CreateEx(DWORD dwExStyle, DWORD dwStyle, const RECT& rect, CWnd* pParent, UINT nID)
    { return CWnd::CreateEx(dwExStyle, WC_LISTVIEWW, nullptr, dwStyle, rect, pParent, nID); }

    int  GetItemCount() const { return (int)SendSelf(LVM_GETITEMCOUNT); }
    BOOL DeleteItem(int i) { return (BOOL)SendSelf(LVM_DELETEITEM, (WPARAM)i); }
    BOOL DeleteAllItems() { return (BOOL)SendSelf(LVM_DELETEALLITEMS); }
    int  InsertItem(const LVITEMW* p) { return (int)SendSelf(LVM_INSERTITEMW, 0, (LPARAM)p); }
    int  InsertItem(int i, LPCWSTR psz) { LVITEMW it{}; it.mask = LVIF_TEXT; it.iItem = i; it.pszText = const_cast<LPWSTR>(psz); return (int)SendSelf(LVM_INSERTITEMW, 0, (LPARAM)&it); }
    BOOL GetItem(LVITEMW* p) const { return (BOOL)SendSelf(LVM_GETITEMW, 0, (LPARAM)p); }
    BOOL SetItem(const LVITEMW* p) { return (BOOL)SendSelf(LVM_SETITEMW, 0, (LPARAM)p); }
    BOOL SetItemText(int i, int sub, LPCWSTR psz) { LVITEMW it{}; it.iSubItem = sub; it.pszText = const_cast<LPWSTR>(psz); return (BOOL)SendSelf(LVM_SETITEMTEXTW, (WPARAM)i, (LPARAM)&it); }
    int  GetItemText(int i, int sub, LPWSTR psz, int n) const { LVITEMW it{}; it.iSubItem = sub; it.cchTextMax = n; it.pszText = psz; return (int)SendSelf(LVM_GETITEMTEXTW, (WPARAM)i, (LPARAM)&it); }
    CString GetItemText(int i, int sub) const {
        CString str;
        for (int n = 256; n < 65536; n *= 2) {
            int got = GetItemText(i, sub, str.GetBuffer(n), n);
            str.ReleaseBuffer(got);
            if (got < n - 1) break;
        }
        return str;
    }
    DWORD_PTR GetItemData(int i) const { LVITEMW it{}; it.mask = LVIF_PARAM; it.iItem = i; SendSelf(LVM_GETITEMW, 0, (LPARAM)&it); return (DWORD_PTR)it.lParam; }
    BOOL SetItemData(int i, DWORD_PTR data) { LVITEMW it{}; it.mask = LVIF_PARAM; it.iItem = i; it.lParam = (LPARAM)data; return (BOOL)SendSelf(LVM_SETITEMW, 0, (LPARAM)&it); }
    UINT GetItemState(int i, UINT mask) const { return (UINT)SendSelf(LVM_GETITEMSTATE, (WPARAM)i, (LPARAM)mask); }
    BOOL SetItemState(int i, UINT state, UINT mask) { LVITEMW it{}; it.state = state; it.stateMask = mask; return (BOOL)SendSelf(LVM_SETITEMSTATE, (WPARAM)i, (LPARAM)&it); }
    int  GetNextItem(int i, int flags) const { return (int)SendSelf(LVM_GETNEXTITEM, (WPARAM)i, (LPARAM)flags); }
    int  GetSelectionMark() const { return (int)SendSelf(LVM_GETSELECTIONMARK); }
    int  SetSelectionMark(int i) { return (int)SendSelf(LVM_SETSELECTIONMARK, 0, (LPARAM)i); }
    UINT GetSelectedCount() const { return (UINT)SendSelf(LVM_GETSELECTEDCOUNT); }
    int  GetTopIndex() const { return (int)SendSelf(LVM_GETTOPINDEX); }
    int  GetCountPerPage() const { return (int)SendSelf(LVM_GETCOUNTPERPAGE); }
    BOOL EnsureVisible(int i, BOOL partial) { return (BOOL)SendSelf(LVM_ENSUREVISIBLE, (WPARAM)i, (LPARAM)partial); }
    BOOL GetItemRect(int i, LPRECT r, UINT code) const
    {
        if (r && i == g_drawItemCtx.item && m_hWnd == g_drawItemCtx.hWnd
            && (code == LVIR_LABEL || code == LVIR_BOUNDS))
        {
            EnsureDrawColCache();
            if (g_drawItemCtx.colCount > 0)
            {
                r->top    = g_drawItemCtx.rcItem.top;
                r->bottom = g_drawItemCtx.rcItem.bottom;
                r->left   = g_drawItemCtx.rcItem.left;
                // LVIR_LABEL = column-0 width; LVIR_BOUNDS = full row
                r->right  = (code == LVIR_LABEL)
                            ? g_drawItemCtx.rcItem.left + g_drawItemCtx.colX[1]
                            : g_drawItemCtx.rcItem.right;
                return TRUE;
            }
        }
        // Fallback: set r->left = code before sending per ListView_GetItemRect convention
        r->left = (LONG)code; return (BOOL)SendSelf(LVM_GETITEMRECT, (WPARAM)i, (LPARAM)r);
    }
    BOOL GetSubItemRect(int i, int sub, int code, RECT& r) const
    {
        if (i == g_drawItemCtx.item && m_hWnd == g_drawItemCtx.hWnd && code == LVIR_LABEL)
        {
            EnsureDrawColCache();
            if (sub >= 0 && sub + 1 <= g_drawItemCtx.colCount)
            {
                r.top    = g_drawItemCtx.rcItem.top;
                r.bottom = g_drawItemCtx.rcItem.bottom;
                r.left   = g_drawItemCtx.rcItem.left + g_drawItemCtx.colX[sub];
                r.right  = g_drawItemCtx.rcItem.left + g_drawItemCtx.colX[sub + 1];
                return TRUE;
            }
        }
        // Fallback: set left = code, top = sub before sending per ListView_GetSubItemRect convention
        r.left = (LONG)code; r.top = sub; return (BOOL)SendSelf(LVM_GETSUBITEMRECT, (WPARAM)i, (LPARAM)&r);
    }
    int  HitTest(LVHITTESTINFO* p) const { return (int)SendSelf(LVM_HITTEST, 0, (LPARAM)p); }
    int  HitTest(CPoint pt, UINT* pFlags = nullptr) const { LVHITTESTINFO h{}; h.pt = pt; const int r = (int)SendSelf(LVM_HITTEST, 0, (LPARAM)&h); if (pFlags) *pFlags = h.flags; return r; }
    int  SubItemHitTest(LVHITTESTINFO* p) const { return (int)SendSelf(LVM_SUBITEMHITTEST, 0, (LPARAM)p); }
    void RedrawItems(int first, int last) { SendSelf(LVM_REDRAWITEMS, (WPARAM)first, (LPARAM)last); }
    BOOL Scroll(CSize size) { return (BOOL)SendSelf(LVM_SCROLL, (WPARAM)size.cx, (LPARAM)size.cy); }
    DWORD SetExtendedStyle(DWORD ex) { return (DWORD)SendSelf(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)ex); }
    DWORD GetExtendedStyle() const { return (DWORD)SendSelf(LVM_GETEXTENDEDLISTVIEWSTYLE); }
    BOOL SetItemCount(int n) { SendSelf(LVM_SETITEMCOUNT, (WPARAM)n); return TRUE; }
    BOOL SetItemCountEx(int n, DWORD flags = LVSICF_NOINVALIDATEALL) { return (BOOL)SendSelf(LVM_SETITEMCOUNT, (WPARAM)n, (LPARAM)flags); }
    CImageList* SetImageList(CImageList* pImageList, int nType) { SendSelf(LVM_SETIMAGELIST, (WPARAM)nType, (LPARAM)(pImageList ? pImageList->m_hImageList : nullptr)); return nullptr; }

    int  InsertColumn(int nCol, LPCWSTR psz, int fmt = LVCFMT_LEFT, int width = -1, int sub = -1)
    {
        LVCOLUMNW c{}; c.mask = LVCF_TEXT | LVCF_FMT; c.pszText = const_cast<LPWSTR>(psz); c.fmt = fmt;
        if (width != -1) { c.mask |= LVCF_WIDTH; c.cx = width; }
        if (sub != -1) { c.mask |= LVCF_SUBITEM; c.iSubItem = sub; }
        return (int)SendSelf(LVM_INSERTCOLUMNW, (WPARAM)nCol, (LPARAM)&c);
    }
    int  InsertColumn(int nCol, const LVCOLUMNW* p) { return (int)SendSelf(LVM_INSERTCOLUMNW, (WPARAM)nCol, (LPARAM)p); }
    BOOL DeleteColumn(int nCol) { return (BOOL)SendSelf(LVM_DELETECOLUMN, (WPARAM)nCol); }
    BOOL GetColumn(int nCol, LVCOLUMNW* p) const { return (BOOL)SendSelf(LVM_GETCOLUMN, (WPARAM)nCol, (LPARAM)p); }
    BOOL SetColumn(int nCol, const LVCOLUMNW* p) { return (BOOL)SendSelf(LVM_SETCOLUMN, (WPARAM)nCol, (LPARAM)p); }
    int  GetColumnWidth(int nCol) const { return (int)SendSelf(LVM_GETCOLUMNWIDTH, (WPARAM)nCol); }
    BOOL SetColumnWidth(int nCol, int cx) { return (BOOL)SendSelf(LVM_SETCOLUMNWIDTH, (WPARAM)nCol, (LPARAM)cx); }
    BOOL GetColumnOrderArray(LPINT p, int n = -1) const { if (n < 0) n = GetHeaderCtrl()->GetItemCount(); return (BOOL)SendSelf(LVM_GETCOLUMNORDERARRAY, (WPARAM)n, (LPARAM)p); }
    BOOL SetColumnOrderArray(int n, LPINT p) { return (BOOL)SendSelf(LVM_SETCOLUMNORDERARRAY, (WPARAM)n, (LPARAM)p); }

    CHeaderCtrl* GetHeaderCtrl() const
    {
        if (!m_header.m_hWnd)
            m_header.m_hWnd = reinterpret_cast<HWND>(m_pfnSuper
                ? ::CallWindowProcW(m_pfnSuper, m_hWnd, LVM_GETHEADER, 0, 0)
                : ::SendMessageW(m_hWnd, LVM_GETHEADER, 0, 0));
        return &m_header;
    }

    POSITION GetFirstSelectedItemPosition() const
    {
        const int i = (int)SendSelf(LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
        return (i < 0) ? nullptr : reinterpret_cast<POSITION>(static_cast<INT_PTR>(i) + 1);
    }
    int GetNextSelectedItem(POSITION& pos) const
    {
        const int cur = static_cast<int>(reinterpret_cast<INT_PTR>(pos)) - 1;
        const int next = (int)SendSelf(LVM_GETNEXTITEM, (WPARAM)cur, LVNI_SELECTED);
        pos = (next < 0) ? nullptr : reinterpret_cast<POSITION>(static_cast<INT_PTR>(next) + 1);
        return cur;
    }

protected:
    mutable CHeaderCtrl m_header;

    // Populate g_drawItemCtx.colX[0..n] from the header control (which is NOT subclassed,
    // so HDM_GETITEM goes straight to the system proc — no AfxWndProc chain, no LVN_GETDISPINFO).
    // Called at most once per WM_DRAWITEM dispatch; subsequent calls are no-ops.
    void EnsureDrawColCache() const
    {
        if (g_drawItemCtx.colCount != 0 || m_hWnd != g_drawItemCtx.hWnd) return;
        HWND hdr = m_header.m_hWnd;
        if (!hdr)
        {
            hdr = reinterpret_cast<HWND>(m_pfnSuper
                ? ::CallWindowProcW(m_pfnSuper, m_hWnd, LVM_GETHEADER, 0, 0)
                : ::SendMessageW(m_hWnd, LVM_GETHEADER, 0, 0));
            m_header.m_hWnd = hdr;
        }
        if (!hdr) return;
        const int n = static_cast<int>(::SendMessageW(hdr, HDM_GETITEMCOUNT, 0, 0));
        const int cap = static_cast<int>(std::size(g_drawItemCtx.colX)) - 1;
        const int cols = (n < cap) ? n : cap;
        int x = 0;
        for (int c = 0; c < cols; ++c)
        {
            g_drawItemCtx.colX[c] = x;
            HDITEMW hi{ .mask = HDI_WIDTH };
            ::SendMessageW(hdr, HDM_GETITEMW, static_cast<WPARAM>(c), reinterpret_cast<LPARAM>(&hi));
            x += hi.cxy;
        }
        g_drawItemCtx.colX[cols] = x;
        g_drawItemCtx.colCount   = cols;
    }
};
SHIM_IMPLEMENT_DYNAMIC_INLINE(CListCtrl, RUNTIME_CLASS(CWnd))

// -----------------------------------------------------------------------------
//  CWaitCursor / CMemDC
// -----------------------------------------------------------------------------
class CWaitCursor final
{
public:
    CWaitCursor() { ::SetCursor(::LoadCursorW(nullptr, IDC_WAIT)); }
    ~CWaitCursor() { Restore(); }
    void Restore() { ::SetCursor(::LoadCursorW(nullptr, IDC_ARROW)); }
};

class CMemDC final
{
public:
    CMemDC(CDC& dc, CWnd* pWnd) : m_pDC(&dc) { CRect rc; pWnd->GetClientRect(&rc); Init(dc, rc); }
    CMemDC(CDC& dc, const CRect& rect) : m_pDC(&dc) { Init(dc, rect); }
    ~CMemDC()
    {
        m_dcMem.SetViewportOrg(0, 0);
        m_pDC->BitBlt(m_rect.left, m_rect.top, m_rect.Width(), m_rect.Height(), &m_dcMem, 0, 0, SRCCOPY);
        m_dcMem.SelectObject(m_pOldBmp);
    }
    CMemDC(const CMemDC&) = delete;
    CMemDC& operator=(const CMemDC&) = delete;
    CDC& GetDC() { return m_dcMem; }

private:
    void Init(CDC& dc, const CRect& rect)
    {
        m_rect = rect;
        m_dcMem.CreateCompatibleDC(&dc);
        m_bmp.CreateCompatibleBitmap(&dc, rect.Width(), rect.Height());
        m_pOldBmp = m_dcMem.SelectObject(&m_bmp);
        m_dcMem.SetViewportOrg(-rect.left, -rect.top);
    }
    CDC* m_pDC;
    CDC  m_dcMem;
    CBitmap m_bmp;
    CGdiObject* m_pOldBmp = nullptr;
    CRect m_rect;
};

// -----------------------------------------------------------------------------
//  Common dialogs: CFileDialog / CFolderPickerDialog
// -----------------------------------------------------------------------------
class CFileDialog
{
public:
    OPENFILENAMEW m_ofn{};

    CFileDialog(BOOL bOpenFileDialog, LPCWSTR lpszDefExt = nullptr, LPCWSTR lpszFileName = nullptr,
        DWORD dwFlags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, LPCWSTR lpszFilter = nullptr,
        CWnd* pParentWnd = nullptr, DWORD dwSize = 0, BOOL bVistaStyle = TRUE)
        : m_bOpen(bOpenFileDialog), m_pParent(pParentWnd)
    {
        (void)dwSize; (void)bVistaStyle;
        m_strFile.resize(4096);
        if (lpszFileName) wcsncpy_s(m_strFile.data(), m_strFile.size(), lpszFileName, _TRUNCATE);
        if (lpszDefExt) m_defExt = lpszDefExt;
        if (lpszFilter) { m_filter = lpszFilter; for (auto& ch : m_filter) if (ch == L'|') ch = L'\0'; m_filter.push_back(L'\0'); }

        m_ofn.lStructSize = sizeof(OPENFILENAMEW);
        m_ofn.lpstrFilter = m_filter.empty() ? nullptr : m_filter.c_str();
        m_ofn.lpstrFile = m_strFile.data();
        m_ofn.nMaxFile = static_cast<DWORD>(m_strFile.size());
        m_ofn.lpstrDefExt = m_defExt.empty() ? nullptr : m_defExt.c_str();
        m_ofn.Flags = dwFlags;
    }
    virtual ~CFileDialog() = default;

    virtual INT_PTR DoModal()
    {
        m_ofn.hwndOwner = m_pParent ? m_pParent->m_hWnd : nullptr;
        const BOOL ok = m_bOpen ? ::GetOpenFileNameW(&m_ofn) : ::GetSaveFileNameW(&m_ofn);
        return ok ? IDOK : IDCANCEL;
    }
    CString GetPathName() const { return CString(m_strFile.c_str()); }
    CString GetFileName() const { const std::wstring s = m_strFile.c_str(); const auto p = s.find_last_of(L"\\/"); return CString((p == std::wstring::npos) ? s : s.substr(p + 1)); }

protected:
    BOOL  m_bOpen;
    CWnd* m_pParent;
    std::wstring m_filter;
    std::wstring m_defExt;
    std::wstring m_strFile;
};

class CFolderPickerDialog final : public CFileDialog
{
public:
    CFolderPickerDialog(LPCWSTR lpszFolder = nullptr, DWORD dwFlags = 0, CWnd* pParentWnd = nullptr, DWORD dwSize = 0, BOOL bVistaStyle = TRUE)
        : CFileDialog(TRUE, nullptr, lpszFolder, dwFlags, nullptr, pParentWnd, dwSize, bVistaStyle)
    {
        if (lpszFolder) m_initial = lpszFolder;
    }
    INT_PTR DoModal() override
    {
        CComPtr<IFileOpenDialog> dlg;
        if (FAILED(dlg.CoCreateInstance(CLSID_FileOpenDialog))) return IDCANCEL;
        DWORD opts = 0; dlg->GetOptions(&opts);
        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        if (m_ofn.lpstrTitle) dlg->SetTitle(m_ofn.lpstrTitle);
        if (!m_initial.empty())
        {
            CComPtr<IShellItem> si;
            if (SUCCEEDED(::SHCreateItemFromParsingName(m_initial.c_str(), nullptr, IID_PPV_ARGS(&si)))) dlg->SetFolder(si);
        }
        if (FAILED(dlg->Show(m_pParent ? m_pParent->m_hWnd : nullptr))) return IDCANCEL;
        CComPtr<IShellItem> result;
        if (FAILED(dlg->GetResult(&result))) return IDCANCEL;
        PWSTR psz = nullptr;
        if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &psz)) && psz)
        {
            m_path = psz; ::CoTaskMemFree(psz);
            m_strFile.assign(m_path.begin(), m_path.end()); m_strFile.push_back(L'\0');
        }
        return IDOK;
    }
    CString GetFolderPath() const { return CString(m_path.c_str()); }

private:
    std::wstring m_initial;
    std::wstring m_path;
};

// Color picker — backed by the common ChooseColor dialog.
class CMFCColorDialog final
{
public:
    explicit CMFCColorDialog(COLORREF clrInit = 0, DWORD = 0, CWnd* pParentWnd = nullptr, HWND = nullptr)
        : m_color(clrInit), m_pParent(pParentWnd) {}
    INT_PTR DoModal()
    {
        static COLORREF custom[16] = {};
        CHOOSECOLORW cc{}; cc.lStructSize = sizeof(cc);
        cc.hwndOwner = m_pParent ? m_pParent->m_hWnd : (AfxGetMainWnd() ? AfxGetMainWnd()->m_hWnd : nullptr);
        cc.rgbResult = m_color; cc.lpCustColors = custom; cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR;
        if (::ChooseColorW(&cc)) { m_color = cc.rgbResult; return IDOK; }
        return IDCANCEL;
    }
    COLORREF GetColor() const { return m_color; }
    void SetCurrentColor(COLORREF c) { m_color = c; }
private:
    COLORREF m_color;
    CWnd* m_pParent;
};

// -----------------------------------------------------------------------------
//  CDialog / CDialogEx + DDX
// -----------------------------------------------------------------------------
CWnd* AFXAPI AfxGetMainWnd();

INT_PTR CALLBACK AfxDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
inline BOOL WdsWalkPreTranslate(HWND hWndStop, MSG* pMsg);

inline thread_local std::vector<CWnd*> g_modalPreTranslateStack;
inline thread_local HHOOK g_modalPreTranslateHook = nullptr;

inline LRESULT CALLBACK WdsModalPreTranslateHookProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code >= 0 && lParam != 0 && !g_modalPreTranslateStack.empty())
    {
        MSG* pMsg = reinterpret_cast<MSG*>(lParam);
        if (CWnd* pDlg = g_modalPreTranslateStack.back();
            pDlg != nullptr && pDlg->GetSafeHwnd() != nullptr &&
            pMsg->hwnd != nullptr &&
            (pMsg->hwnd == pDlg->m_hWnd || ::IsChild(pDlg->m_hWnd, pMsg->hwnd)) &&
            WdsWalkPreTranslate(pDlg->m_hWnd, pMsg))
        {
            return TRUE;
        }
    }

    return ::CallNextHookEx(g_modalPreTranslateHook, code, wParam, lParam);
}

class WdsModalPreTranslateScope final
{
public:
    explicit WdsModalPreTranslateScope(CWnd* pDlg)
    {
        if (g_modalPreTranslateHook == nullptr)
        {
            g_modalPreTranslateHook = ::SetWindowsHookExW(WH_MSGFILTER,
                WdsModalPreTranslateHookProc, nullptr, ::GetCurrentThreadId());
        }

        g_modalPreTranslateStack.push_back(pDlg);
    }

    ~WdsModalPreTranslateScope()
    {
        if (!g_modalPreTranslateStack.empty())
            g_modalPreTranslateStack.pop_back();

        if (g_modalPreTranslateStack.empty() && g_modalPreTranslateHook != nullptr)
        {
            ::UnhookWindowsHookEx(g_modalPreTranslateHook);
            g_modalPreTranslateHook = nullptr;
        }
    }

    WdsModalPreTranslateScope(const WdsModalPreTranslateScope&) = delete;
    WdsModalPreTranslateScope& operator=(const WdsModalPreTranslateScope&) = delete;
};

class CDialog : public CWnd
{
public:
    static const CRuntimeClass classCDialog;
    CRuntimeClass* GetRuntimeClass() const override;

    CDialog() = default;
    explicit CDialog(UINT nIDTemplate, CWnd* pParent = nullptr) : m_nIDTemplate(nIDTemplate), m_pParentWnd(pParent) {}
    explicit CDialog(LPCWSTR, CWnd* pParent = nullptr) : m_pParentWnd(pParent) {}

    virtual INT_PTR DoModal()
    {
        const HWND hParent = m_pParentWnd ? m_pParentWnd->m_hWnd
            : (AfxGetMainWnd() ? AfxGetMainWnd()->m_hWnd : nullptr);
        const WdsModalPreTranslateScope modalPreTranslate(this);
        return ::DialogBoxParamW(AfxGetResourceHandle(), MAKEINTRESOURCEW(m_nIDTemplate),
            hParent, AfxDlgProc, reinterpret_cast<LPARAM>(static_cast<CWnd*>(this)));
    }
    BOOL Create(UINT nIDTemplate, CWnd* pParent = nullptr)
    {
        m_nIDTemplate = nIDTemplate; m_pParentWnd = pParent;
        const HWND h = ::CreateDialogParamW(AfxGetResourceHandle(), MAKEINTRESOURCEW(nIDTemplate),
            pParent ? pParent->m_hWnd : nullptr, AfxDlgProc, reinterpret_cast<LPARAM>(static_cast<CWnd*>(this)));
        return h != nullptr;
    }

    virtual BOOL OnInitDialog()
    {
        if (!ExecuteDlgInit(m_nIDTemplate))
        {
            EndDialog(-1);
            return FALSE;
        }
        if (!UpdateData(FALSE))
        {
            EndDialog(-1);
            return FALSE;
        }
        return TRUE;
    }
    virtual void OnOK() { if (!UpdateData(TRUE)) return; EndDialog(IDOK); }
    virtual void OnCancel() { EndDialog(IDCANCEL); }
    void EndDialog(int nResult) { ::EndDialog(m_hWnd, nResult); }
    void GotoDlgCtrl(CWnd* pWnd) { if (pWnd) ::SendMessageW(m_hWnd, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(pWnd->m_hWnd), TRUE); }
    void SetDefID(UINT nID) { ::SendMessageW(m_hWnd, DM_SETDEFID, nID, 0); }

    static const AFX_MSGMAP* __stdcall _GetBaseMessageMap() { return CWnd::GetThisMessageMap(); }
    static const AFX_MSGMAP* GetThisMessageMap()
    {
        static const AFX_MSGMAP_ENTRY entries[] = {
            { WM_COMMAND, WdsCN_COMMAND, IDOK, IDOK,
              reinterpret_cast<AFX_PMSG>(static_cast<void (CDialog::*)()>(&CDialog::OnOK)), &WdsThunk_Command, nullptr },
            { WM_COMMAND, WdsCN_COMMAND, IDCANCEL, IDCANCEL,
              reinterpret_cast<AFX_PMSG>(static_cast<void (CDialog::*)()>(&CDialog::OnCancel)), &WdsThunk_Command, nullptr },
            { 0, 0, 0, 0, nullptr, nullptr, nullptr }
        };
        static const AFX_MSGMAP map = { &CDialog::_GetBaseMessageMap, entries };
        return &map;
    }
    const AFX_MSGMAP* GetMessageMap() const override { return GetThisMessageMap(); }

    UINT  m_nIDTemplate = 0;
    CWnd* m_pParentWnd = nullptr;
};
SHIM_IMPLEMENT_DYNAMIC_INLINE(CDialog, RUNTIME_CLASS(CWnd))

class CDialogEx : public CDialog
{
public:
    static const CRuntimeClass classCDialogEx;
    CRuntimeClass* GetRuntimeClass() const override;

    CDialogEx() = default;
    explicit CDialogEx(UINT nIDTemplate, CWnd* pParent = nullptr) : CDialog(nIDTemplate, pParent) {}
    explicit CDialogEx(LPCWSTR psz, CWnd* pParent = nullptr) : CDialog(psz, pParent) {}
    void SetBackgroundColor(COLORREF, BOOL = TRUE) {}

    static const AFX_MSGMAP* __stdcall _GetBaseMessageMap() { return CDialog::GetThisMessageMap(); }
    static const AFX_MSGMAP* GetThisMessageMap()
    {
        static const AFX_MSGMAP_ENTRY entries[] = { { 0, 0, 0, 0, nullptr, nullptr, nullptr } };
        static const AFX_MSGMAP map = { &CDialogEx::_GetBaseMessageMap, entries };
        return &map;
    }
    const AFX_MSGMAP* GetMessageMap() const override { return GetThisMessageMap(); }
};
SHIM_IMPLEMENT_DYNAMIC_INLINE(CDialogEx, RUNTIME_CLASS(CDialog))

inline INT_PTR CALLBACK AfxDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Make GetCurrentMessage()/the WM_CTLCOLOR thunk see the real message (needed for
    // dark-mode OnCtlColor, which derives the control type from the current message).
    const MSG saved = g_currentMsg;
    const DWORD pos = ::GetMessagePos();
    g_currentMsg = { hWnd, msg, wParam, lParam, static_cast<DWORD>(::GetMessageTime()), POINT{ static_cast<LONG>(static_cast<short>(LOWORD(pos))), static_cast<LONG>(static_cast<short>(HIWORD(pos))) } };
    struct RestoreMsg { ~RestoreMsg() { g_currentMsg = m; } MSG m; } restore{ saved };

    if (msg == WM_INITDIALOG)
    {
        CWnd* pDlg = reinterpret_cast<CWnd*>(lParam);
        if (pDlg == nullptr) return FALSE;
        pDlg->Attach(hWnd);
        return static_cast<CDialog*>(pDlg)->OnInitDialog();
    }
    CWnd* pDlg = CWnd::FromHandlePermanent(hWnd);
    if (pDlg == nullptr) return FALSE;
    LRESULT lResult = 0;
    if (pDlg->OnWndMsg(msg, wParam, lParam, &lResult))
    {
        if (msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC)
        {
            return reinterpret_cast<INT_PTR>(reinterpret_cast<void*>(lResult));
        }
        ::SetWindowLongPtrW(hWnd, DWLP_MSGRESULT, lResult);
        return TRUE;
    }
    return FALSE;
}

// ---- DDX routines --------------------------------------------------------------
inline void WdsSetWindowTextIfChanged(HWND hWnd, LPCWSTR psz)
{
    if (hWnd == nullptr) return;
    if (psz == nullptr) psz = L"";

    const int newLen = static_cast<int>(std::wcslen(psz));
    const int oldLen = ::GetWindowTextLengthW(hWnd);
    if (oldLen == newLen)
    {
        std::wstring oldText(static_cast<size_t>(oldLen), L'\0');
        const int got = ::GetWindowTextW(hWnd, oldText.data(), oldLen + 1);
        if (got == newLen && oldText == psz) return;
    }

    ::SetWindowTextW(hWnd, psz);
}

inline void DDX_Control(CDataExchange* pDX, int nID, CWnd& rControl)
{
    if (rControl.GetSafeHwnd() == nullptr)
    {
        const HWND h = pDX->PrepareCtrl(nID);
        if (h) rControl.SubclassWindow(h);
    }
}
inline void DDX_Check(CDataExchange* pDX, int nID, int& value)
{
    const HWND h = pDX->PrepareCtrl(nID);
    if (pDX->m_bSaveAndValidate) value = static_cast<int>(WdsSendSelf(h, BM_GETCHECK));
    else WdsSendSelf(h, BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED);
}
inline void DDX_Radio(CDataExchange* pDX, int nID, int& value)
{
    HWND hWndCtrl = pDX->PrepareCtrl(nID);
    if (pDX->m_bSaveAndValidate) value = -1;
    int iButton = 0;
    do
    {
        wchar_t cls[16]; ::GetClassNameW(hWndCtrl, cls, 16);
        if (_wcsicmp(cls, L"Button") == 0 && (::GetWindowLongW(hWndCtrl, GWL_STYLE) & 0x0F) >= BS_RADIOBUTTON)
        {
            if (pDX->m_bSaveAndValidate)
            {
                if (WdsSendSelf(hWndCtrl, BM_GETCHECK) != 0) value = iButton;
            }
            else
            {
                WdsSendSelf(hWndCtrl, BM_SETCHECK, (iButton == value) ? BST_CHECKED : BST_UNCHECKED);
            }
            ++iButton;
        }
        hWndCtrl = ::GetWindow(hWndCtrl, GW_HWNDNEXT);
    } while (hWndCtrl != nullptr && (::GetWindowLongW(hWndCtrl, GWL_STYLE) & WS_GROUP) == 0);
}
inline void DDX_Text(CDataExchange* pDX, int nID, CString& value)
{
    const HWND h = pDX->PrepareEditCtrl(nID);
    if (pDX->m_bSaveAndValidate)
    {
        const int len = ::GetWindowTextLengthW(h);
        std::wstring s(static_cast<size_t>(len), L'\0');
        const int got = ::GetWindowTextW(h, s.data(), len + 1);
        s.resize(static_cast<size_t>(got));
        value = std::move(s);
    }
    else WdsSetWindowTextIfChanged(h, value.GetString());
}
inline void DDX_Text(CDataExchange* pDX, int nID, int& value)
{
    const HWND h = pDX->PrepareEditCtrl(nID);
    const HWND hDlg = pDX->m_pDlgWnd->m_hWnd;
    if (pDX->m_bSaveAndValidate) value = static_cast<int>(::GetDlgItemInt(hDlg, nID, nullptr, TRUE));
    else { wchar_t buf[32]; swprintf_s(buf, L"%d", value); WdsSetWindowTextIfChanged(h, buf); }
}
inline void DDX_Text(CDataExchange* pDX, int nID, UINT& value)
{
    const HWND h = pDX->PrepareEditCtrl(nID);
    const HWND hDlg = pDX->m_pDlgWnd->m_hWnd;
    if (pDX->m_bSaveAndValidate) value = ::GetDlgItemInt(hDlg, nID, nullptr, FALSE);
    else { wchar_t buf[32]; swprintf_s(buf, L"%u", value); WdsSetWindowTextIfChanged(h, buf); }
}
inline void DDX_Text(CDataExchange* pDX, int nID, double& value)
{
    const HWND h = pDX->PrepareEditCtrl(nID);
    if (pDX->m_bSaveAndValidate)
    {
        wchar_t buf[64]; ::GetWindowTextW(h, buf, 64); value = std::wcstod(buf, nullptr);
    }
    else { wchar_t buf[64]; swprintf_s(buf, L"%g", value); WdsSetWindowTextIfChanged(h, buf); }
}
inline void DDX_CBIndex(CDataExchange* pDX, int nID, int& value)
{
    const HWND h = pDX->PrepareCtrl(nID);
    if (pDX->m_bSaveAndValidate) value = static_cast<int>(WdsSendSelf(h, CB_GETCURSEL));
    else if (static_cast<int>(WdsSendSelf(h, CB_GETCURSEL)) != value)
        WdsSendSelf(h, CB_SETCURSEL, static_cast<WPARAM>(value));
}
inline void DDX_CBString(CDataExchange* pDX, int nID, CString& value)
{
    const HWND h = pDX->PrepareCtrl(nID);
    if (pDX->m_bSaveAndValidate)
    {
        const int len = ::GetWindowTextLengthW(h);
        std::wstring s(static_cast<size_t>(len), L'\0');
        const int got = ::GetWindowTextW(h, s.data(), len + 1);
        s.resize(static_cast<size_t>(got));
        value = std::move(s);
    }
    else
    {
        const int i = static_cast<int>(WdsSendSelf(h, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(value.GetString())));
        if (i >= 0)
        {
            if (static_cast<int>(WdsSendSelf(h, CB_GETCURSEL)) != i)
                WdsSendSelf(h, CB_SETCURSEL, static_cast<WPARAM>(i));
        }
        else WdsSetWindowTextIfChanged(h, value.GetString());
    }
}
inline void DDX_Slider(CDataExchange* pDX, int nID, int& value)
{
    const HWND h = pDX->PrepareCtrl(nID);
    if (pDX->m_bSaveAndValidate) value = static_cast<int>(WdsSendSelf(h, TBM_GETPOS));
    else WdsSendSelf(h, TBM_SETPOS, TRUE, static_cast<LPARAM>(value));
}

// -----------------------------------------------------------------------------
//  Application object: CWinThread / CWinApp / CWinAppEx
// -----------------------------------------------------------------------------
class CWinApp;
class CWinThread;
class CCommandLineInfo;
class CFrameImpl {};   // opaque (CWinAppEx state helper)

inline HINSTANCE g_hInstance = nullptr;
inline CWinApp*  g_pApp = nullptr;
inline CWinThread* g_pThread = nullptr;
inline std::function<void()> g_idleCmdUiUpdate;   // set by the frame to refresh toolbar UI

class CWinThread : public CCmdTarget
{
public:
    CWnd*  m_pMainWnd = nullptr;
    CWnd*  m_pActiveWnd = nullptr;
    DWORD  m_nThreadID = 0;
    HANDLE m_hThread = nullptr;
    MSG    m_msgCur{};
    BOOL   m_bAutoDelete = TRUE;

    CWinThread() { g_pThread = this; }

    virtual BOOL InitInstance() { return TRUE; }
    virtual int  ExitInstance() { return static_cast<int>(m_msgCur.wParam); }
    virtual BOOL OnIdle(LONG lCount)
    {
        if (lCount == 0) { if (g_idleCmdUiUpdate) g_idleCmdUiUpdate(); return TRUE; }
        return FALSE;
    }
    virtual BOOL IsIdleMessage(MSG* pMsg)
    {
        return !(pMsg->message == WM_MOUSEMOVE || pMsg->message == WM_NCMOUSEMOVE ||
                 pMsg->message == WM_PAINT || pMsg->message == 0x0118 /*WM_SYSTIMER*/);
    }
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual BOOL PumpMessage();
    virtual int  Run();
};

class CWinApp : public CWinThread
{
public:
    HINSTANCE m_hInstance = nullptr;
    LPWSTR    m_lpCmdLine = nullptr;
    int       m_nCmdShow = SW_SHOWNORMAL;
    LPCWSTR   m_pszAppName      = nullptr;
    LPCWSTR   m_pszProfileName  = nullptr;
    LPCWSTR   m_pszRegistryKey  = nullptr;
    LPCWSTR   m_pszExeName      = nullptr;

    CWinApp()
    {
        g_pApp = this;
        m_pszAppName     = m_appNameStorage.c_str();
        m_pszProfileName = nullptr;
    }

    BOOL InitInstance() override { return TRUE; }

    void SetRegistryKey(LPCWSTR lpszRegistryKey)
    {
        m_regKeyStorage  = lpszRegistryKey ? lpszRegistryKey : L"WinDirStat";
        m_pszRegistryKey = m_regKeyStorage.c_str();
        m_pszProfileName = nullptr;
    }
    void SetRegistryKey(UINT nIDRegistryKey) { CString s; s.LoadString(nIDRegistryKey); SetRegistryKey(s.GetString()); }
    void LoadStdProfileSettings(UINT = 0) {}
    void EnableShellOpen() {}

    HICON   LoadIcon(LPCWSTR lpszResourceName) const { return ::LoadIconW(AfxGetResourceHandle(), lpszResourceName); }
    HICON   LoadIcon(UINT nIDResource) const { return LoadIcon(MAKEINTRESOURCEW(nIDResource)); }
    HICON   LoadStandardIcon(LPCWSTR lpszIconName) const { return ::LoadIconW(nullptr, lpszIconName); }
    HCURSOR LoadCursor(LPCWSTR lpszResourceName) const { return ::LoadCursorW(AfxGetResourceHandle(), lpszResourceName); }
    HCURSOR LoadCursor(UINT nIDResource) const { return LoadCursor(MAKEINTRESOURCEW(nIDResource)); }
    HCURSOR LoadStandardCursor(LPCWSTR lpszCursorName) const { return ::LoadCursorW(nullptr, lpszCursorName); }

    void ParseCommandLine(CCommandLineInfo& rCmdInfo);

    UINT    GetProfileInt(LPCWSTR section, LPCWSTR entry, int nDefault);
    BOOL    WriteProfileInt(LPCWSTR section, LPCWSTR entry, int nValue);
    CString GetProfileString(LPCWSTR section, LPCWSTR entry, LPCWSTR def = L"");
    BOOL    WriteProfileString(LPCWSTR section, LPCWSTR entry, LPCWSTR value);
    BOOL    GetProfileBinary(LPCWSTR section, LPCWSTR entry, LPBYTE* ppData, UINT* pBytes);
    BOOL    WriteProfileBinary(LPCWSTR section, LPCWSTR entry, LPBYTE pData, UINT nBytes);

private:
    std::wstring m_appNameStorage   = L"WinDirStat";
    std::wstring m_profileStorage;
    std::wstring m_regKeyStorage;

    std::wstring RegProfilePath(LPCWSTR section) const
    {
        return std::wstring(L"Software\\") + (m_pszRegistryKey ? m_pszRegistryKey : L"WinDirStat") +
               L"\\" + (m_pszProfileName ? m_pszProfileName : L"WinDirStat") + L"\\" + section;
    }
};

class CWinAppEx : public CWinApp
{
public:
    BOOL m_bSaveState = TRUE;

    BOOL InitInstance() override { return CWinApp::InitInstance(); }
    virtual BOOL LoadState(LPCWSTR /*lpszSectionName*/ = nullptr, CFrameImpl* /*pFrameImpl*/ = nullptr) { return TRUE; }
    virtual BOOL SaveState(LPCWSTR = nullptr, CFrameImpl* = nullptr) { return TRUE; }
    void EnableLoadWindowPlacement(BOOL) {}
};

// ---- CCommandLineInfo ----------------------------------------------------------
class CCommandLineInfo : public CObject
{
public:
    CString m_strFileName;
    BOOL m_bShowSplash = TRUE;
    BOOL m_bRunEmbedded = FALSE;
    BOOL m_bRunAutomated = FALSE;
    CCommandLineInfo() = default;
    virtual ~CCommandLineInfo() = default;
    virtual void ParseParam(const WCHAR* pszParam, BOOL bFlag, BOOL bLast)
    { UNREFERENCED_PARAMETER(pszParam); UNREFERENCED_PARAMETER(bFlag); UNREFERENCED_PARAMETER(bLast); }
};

inline void CWinApp::ParseCommandLine(CCommandLineInfo& rCmdInfo)
{
    int argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (argv == nullptr) return;
    for (int i = 1; i < argc; ++i)
    {
        LPWSTR pszParam = argv[i];
        BOOL bFlag = FALSE;
        if (pszParam[0] == L'-' || pszParam[0] == L'/') { bFlag = TRUE; ++pszParam; }
        rCmdInfo.ParseParam(pszParam, bFlag, (i == argc - 1));
    }
    ::LocalFree(argv);
}

inline void WdsLog(const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    wchar_t buf[2048];
    int len = vswprintf_s(buf, format, args);
    va_end(args);
    if (len <= 0) return;
    wcscat_s(buf, L"\r\n");
    len += 2;
    char utf8[4096];
    int utf8Len = ::WideCharToMultiByte(CP_UTF8, 0, buf, len, utf8, sizeof(utf8), nullptr, nullptr);
    if (utf8Len <= 0) return;
    HANDLE hFile = ::CreateFileW(L"C:\\Users\\Bryan Berns\\.gemini\\antigravity\\brain\\147ab7ba-d819-4c0a-b1ee-6eebe865fa32\\scratch\\shim_debug.log",
        FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD written;
        ::WriteFile(hFile, utf8, utf8Len, &written, nullptr);
        ::CloseHandle(hFile);
    }
}

inline UINT CWinApp::GetProfileInt(LPCWSTR section, LPCWSTR entry, int nDefault)
{
    WdsLog(L"GetProfileInt: section=%s, entry=%s, default=%d, reg=%s, profile=%s",
        section, entry, nDefault, m_pszRegistryKey ? m_pszRegistryKey : L"NULL", m_pszProfileName ? m_pszProfileName : L"NULL");
    if (m_pszRegistryKey)
    {
        HKEY hk;
        if (::RegOpenKeyExW(HKEY_CURRENT_USER, RegProfilePath(section).c_str(), 0, KEY_READ, &hk) == ERROR_SUCCESS)
        {
            DWORD val = 0, sz = sizeof(val), type = 0;
            const LONG r = ::RegQueryValueExW(hk, entry, nullptr, &type, reinterpret_cast<LPBYTE>(&val), &sz);
            ::RegCloseKey(hk);
            if (r == ERROR_SUCCESS && type == REG_DWORD) {
                WdsLog(L"  GetProfileInt: returning REG_DWORD value=%d", val);
                return val;
            }
        }
        WdsLog(L"  GetProfileInt: returning REG default=%d", nDefault);
        return static_cast<UINT>(nDefault);
    }
    UINT res = ::GetPrivateProfileIntW(section, entry, nDefault, m_pszProfileName);
    WdsLog(L"  GetProfileInt: returning INI value=%d", res);
    return res;
}
inline BOOL CWinApp::WriteProfileInt(LPCWSTR section, LPCWSTR entry, int nValue)
{
    WdsLog(L"WriteProfileInt: section=%s, entry=%s, value=%d, reg=%s, profile=%s",
        section, entry, nValue, m_pszRegistryKey ? m_pszRegistryKey : L"NULL", m_pszProfileName ? m_pszProfileName : L"NULL");
    if (m_pszRegistryKey)
    {
        HKEY hk;
        if (::RegCreateKeyExW(HKEY_CURRENT_USER, RegProfilePath(section).c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hk, nullptr) == ERROR_SUCCESS)
        {
            const DWORD v = static_cast<DWORD>(nValue);
            ::RegSetValueExW(hk, entry, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&v), sizeof(v));
            ::RegCloseKey(hk); return TRUE;
        }
        return FALSE;
    }
    wchar_t buf[32]; _itow_s(nValue, buf, 10);
    return ::WritePrivateProfileStringW(section, entry, buf, m_pszProfileName);
}
inline CString CWinApp::GetProfileString(LPCWSTR section, LPCWSTR entry, LPCWSTR def)
{
    WdsLog(L"GetProfileString: section=%s, entry=%s, default=%s, reg=%s, profile=%s",
        section, entry, def, m_pszRegistryKey ? m_pszRegistryKey : L"NULL", m_pszProfileName ? m_pszProfileName : L"NULL");
    if (m_pszRegistryKey)
    {
        HKEY hk;
        if (::RegOpenKeyExW(HKEY_CURRENT_USER, RegProfilePath(section).c_str(), 0, KEY_READ, &hk) == ERROR_SUCCESS)
        {
            DWORD type = 0, sz = 0;
            if (::RegQueryValueExW(hk, entry, nullptr, &type, nullptr, &sz) == ERROR_SUCCESS && type == REG_SZ && sz > 0)
            {
                std::wstring s(sz / sizeof(wchar_t), L'\0');
                ::RegQueryValueExW(hk, entry, nullptr, nullptr, reinterpret_cast<LPBYTE>(s.data()), &sz);
                ::RegCloseKey(hk);
                while (!s.empty() && s.back() == L'\0') s.pop_back();
                WdsLog(L"  GetProfileString: returning REG value=%s", s.c_str());
                return CString(s.c_str());
            }
            ::RegCloseKey(hk);
        }
        WdsLog(L"  GetProfileString: returning REG default=%s", def);
        return CString(def);
    }
    std::wstring buf(8192, L'\0');
    const DWORD n = ::GetPrivateProfileStringW(section, entry, def, buf.data(), static_cast<DWORD>(buf.size()), m_pszProfileName);
    buf.resize(n);
    WdsLog(L"  GetProfileString: returning INI value=%s", buf.c_str());
    return CString(buf.c_str());
}
inline BOOL CWinApp::WriteProfileString(LPCWSTR section, LPCWSTR entry, LPCWSTR value)
{
    WdsLog(L"WriteProfileString: section=%s, entry=%s, value=%s, reg=%s, profile=%s",
        section, entry, value, m_pszRegistryKey ? m_pszRegistryKey : L"NULL", m_pszProfileName ? m_pszProfileName : L"NULL");
    if (m_pszRegistryKey)
    {
        HKEY hk;
        if (::RegCreateKeyExW(HKEY_CURRENT_USER, RegProfilePath(section).c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hk, nullptr) == ERROR_SUCCESS)
        {
            ::RegSetValueExW(hk, entry, 0, REG_SZ, reinterpret_cast<const BYTE*>(value),
                static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));
            ::RegCloseKey(hk); return TRUE;
        }
        return FALSE;
    }
    return ::WritePrivateProfileStringW(section, entry, value, m_pszProfileName);
}
inline BOOL CWinApp::GetProfileBinary(LPCWSTR section, LPCWSTR entry, LPBYTE* ppData, UINT* pBytes)
{
    *ppData = nullptr; *pBytes = 0;
    if (m_pszRegistryKey)
    {
        HKEY hk;
        if (::RegOpenKeyExW(HKEY_CURRENT_USER, RegProfilePath(section).c_str(), 0, KEY_READ, &hk) == ERROR_SUCCESS)
        {
            DWORD type = 0, sz = 0;
            if (::RegQueryValueExW(hk, entry, nullptr, &type, nullptr, &sz) == ERROR_SUCCESS && sz > 0)
            {
                *ppData = new BYTE[sz]; *pBytes = sz;
                ::RegQueryValueExW(hk, entry, nullptr, nullptr, *ppData, &sz);
                ::RegCloseKey(hk); return TRUE;
            }
            ::RegCloseKey(hk);
        }
        return FALSE;
    }
    std::wstring buf(16384, L'\0');
    const DWORD n = ::GetPrivateProfileStringW(section, entry, L"", buf.data(), static_cast<DWORD>(buf.size()), m_pszProfileName);
    buf.resize(n);
    if (buf.size() < 2) return FALSE;
    const UINT count = static_cast<UINT>(buf.size() / 2);
    *ppData = new BYTE[count]; *pBytes = count;
    auto hexVal = [](wchar_t c) -> int { if (c >= L'0' && c <= L'9') return c - L'0'; if (c >= L'a' && c <= L'f') return c - L'a' + 10; if (c >= L'A' && c <= L'F') return c - L'A' + 10; return 0; };
    for (UINT i = 0; i < count; ++i) (*ppData)[i] = static_cast<BYTE>((hexVal(buf[i * 2]) << 4) | hexVal(buf[i * 2 + 1]));
    return TRUE;
}
inline BOOL CWinApp::WriteProfileBinary(LPCWSTR section, LPCWSTR entry, LPBYTE pData, UINT nBytes)
{
    if (m_pszRegistryKey)
    {
        HKEY hk;
        if (::RegCreateKeyExW(HKEY_CURRENT_USER, RegProfilePath(section).c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hk, nullptr) == ERROR_SUCCESS)
        {
            ::RegSetValueExW(hk, entry, 0, REG_BINARY, pData, nBytes);
            ::RegCloseKey(hk); return TRUE;
        }
        return FALSE;
    }
    static const wchar_t* hexd = L"0123456789ABCDEF";
    std::wstring hex; hex.reserve(static_cast<size_t>(nBytes) * 2);
    for (UINT i = 0; i < nBytes; ++i) { hex.push_back(hexd[pData[i] >> 4]); hex.push_back(hexd[pData[i] & 0xF]); }
    return ::WritePrivateProfileStringW(section, entry, hex.c_str(), m_pszProfileName);
}

// ---- message pump --------------------------------------------------------------
inline BOOL WdsWalkPreTranslate(HWND hWndStop, MSG* pMsg)
{
    for (HWND hWnd = pMsg->hwnd; hWnd != nullptr; hWnd = ::GetParent(hWnd))
    {
        if (CWnd* pWnd = CWnd::FromHandlePermanent(hWnd))
            if (pWnd->PreTranslateMessage(pMsg)) return TRUE;
        if (hWnd == hWndStop) break;
    }
    return FALSE;
}
inline BOOL CWinThread::PreTranslateMessage(MSG* pMsg)
{
    return WdsWalkPreTranslate(m_pMainWnd ? m_pMainWnd->m_hWnd : nullptr, pMsg);
}
inline BOOL CWinThread::PumpMessage()
{
    if (!::GetMessageW(&m_msgCur, nullptr, 0, 0)) return FALSE;
    if (!PreTranslateMessage(&m_msgCur))
    {
        ::TranslateMessage(&m_msgCur);
        ::DispatchMessageW(&m_msgCur);
    }
    return TRUE;
}
inline int CWinThread::Run()
{
    BOOL bIdle = TRUE;
    LONG lIdleCount = 0;
    for (;;)
    {
        while (bIdle && !::PeekMessageW(&m_msgCur, nullptr, 0, 0, PM_NOREMOVE))
        {
            if (!OnIdle(lIdleCount++)) bIdle = FALSE;
        }
        do
        {
            if (!PumpMessage()) return ExitInstance();
            if (IsIdleMessage(&m_msgCur)) { bIdle = TRUE; lIdleCount = 0; }
        } while (::PeekMessageW(&m_msgCur, nullptr, 0, 0, PM_NOREMOVE));
    }
}

// ---- Afx globals ---------------------------------------------------------------
inline CWinApp*    AFXAPI AfxGetApp() { return g_pApp; }
inline CWinThread* AFXAPI AfxGetThread() { return g_pThread ? g_pThread : g_pApp; }
inline CWnd*       AFXAPI AfxGetMainWnd() { return g_pApp ? g_pApp->m_pMainWnd : nullptr; }
inline HINSTANCE   AFXAPI AfxGetInstanceHandle() { return g_hInstance ? g_hInstance : ::GetModuleHandleW(nullptr); }
inline HINSTANCE   AFXAPI AfxGetResourceHandle() { return AfxGetInstanceHandle(); }
inline void        AFXAPI AfxSetResourceHandle(HINSTANCE) {}
inline int AFXAPI AfxMessageBox(LPCWSTR lpszText, UINT nType = MB_OK, UINT = 0)
{ return ::MessageBoxW(AfxGetMainWnd() ? AfxGetMainWnd()->m_hWnd : nullptr, lpszText, L"WinDirStat", nType); }
inline int AFXAPI AfxMessageBox(UINT nIDPrompt, UINT nType = MB_OK, UINT = 0)
{ CString s; s.LoadString(nIDPrompt); return AfxMessageBox(s.GetString(), nType); }
inline BOOL AFXAPI AfxInitRichEdit2()
{
    static HMODULE h = ::LoadLibraryW(L"Msftedit.dll");
    if (h == nullptr) h = ::LoadLibraryW(L"Riched20.dll");
    return h != nullptr;
}
inline BOOL AFXAPI AfxInitRichEdit() { return AfxInitRichEdit2(); }

inline void AfxWinInit(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow)
{
    g_hInstance = hInstance;
    ::InitCommonControls();
    if (CWinApp* p = AfxGetApp())
    {
        p->m_hInstance = hInstance;
        p->m_lpCmdLine = lpCmdLine;
        p->m_nCmdShow = nCmdShow;
        p->m_nThreadID = ::GetCurrentThreadId();
        p->m_hThread = ::GetCurrentThread();
    }
}
inline int WdsAfxWinMain(HINSTANCE hInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    AfxWinInit(hInstance, nullptr, lpCmdLine, nCmdShow);
    CWinApp* pApp = AfxGetApp();
    if (pApp == nullptr) return -1;
    int nReturnCode = -1;
    if (!pApp->InitInstance())
    {
        if (pApp->m_pMainWnd != nullptr) pApp->m_pMainWnd->DestroyWindow();
        nReturnCode = pApp->ExitInstance();
    }
    else nReturnCode = pApp->Run();
    return nReturnCode;
}

// -----------------------------------------------------------------------------
//  Frame / control-bar / splitter constants
// -----------------------------------------------------------------------------
#ifndef AFX_IDW_PANE_FIRST
inline constexpr UINT AFX_IDW_PANE_FIRST = 0xE900;
inline constexpr UINT AFX_IDW_PANE_LAST  = 0xE9FF;
inline constexpr UINT AFX_IDW_TOOLBAR    = 0xE81B;
inline constexpr UINT AFX_IDW_STATUS_BAR = 0xE801;
#endif
#ifndef FWS_ADDTOTITLE
inline constexpr DWORD FWS_ADDTOTITLE  = 0x00008000UL;
inline constexpr DWORD FWS_PREFIXTITLE = 0x00010000UL;
inline constexpr DWORD FWS_SNAPTOBARS  = 0x00020000UL;
#endif
#ifndef CBRS_ALIGN_TOP
inline constexpr DWORD CBRS_ALIGN_LEFT   = 0x00001000UL;
inline constexpr DWORD CBRS_ALIGN_TOP    = 0x00002000UL;
inline constexpr DWORD CBRS_ALIGN_RIGHT  = 0x00004000UL;
inline constexpr DWORD CBRS_ALIGN_BOTTOM = 0x00008000UL;
inline constexpr DWORD CBRS_ALIGN_ANY    = 0x0000F000UL;
inline constexpr DWORD CBRS_TOP         = CBRS_ALIGN_TOP;
inline constexpr DWORD CBRS_BOTTOM      = CBRS_ALIGN_BOTTOM;
inline constexpr DWORD CBRS_LEFT        = CBRS_ALIGN_LEFT;
inline constexpr DWORD CBRS_RIGHT       = CBRS_ALIGN_RIGHT;
inline constexpr DWORD CBRS_TOOLTIPS    = 0x00000010UL;
inline constexpr DWORD CBRS_FLYBY       = 0x00000020UL;
inline constexpr DWORD CBRS_SIZE_DYNAMIC= 0x00000040UL;
inline constexpr DWORD CBRS_SIZE_FIXED  = 0x00000080UL;
inline constexpr DWORD CBRS_BORDER_TOP  = 0x00000100UL;
inline constexpr DWORD CBRS_BORDER_BOTTOM = 0x00000200UL;
inline constexpr DWORD CBRS_BORDER_LEFT = 0x00000400UL;
inline constexpr DWORD CBRS_BORDER_RIGHT= 0x00000800UL;
inline constexpr DWORD CBRS_GRIPPER     = 0x00400000UL;
inline constexpr UINT  SBPS_NORMAL      = 0x0000;
inline constexpr UINT  SBPS_STRETCH     = 0x1000;
inline constexpr UINT  SBPS_NOBORDERS   = 0x0100;
inline constexpr UINT  SBPS_POPOUT      = 0x0200;
inline constexpr UINT  SBPS_DISABLED    = 0x0400;
#endif

class CBasePane;        // defined in the feature-pack section
void AFXAPI WdsFillSplitterBg(CDC* pDC, CWnd* pSplitter, const CRect& rc);

// -----------------------------------------------------------------------------
//  CFrameWnd / CFrameWndEx
// -----------------------------------------------------------------------------
class CFrameWnd : public CWnd
{
public:
    static const CRuntimeClass classCFrameWnd;
    CRuntimeClass* GetRuntimeClass() const override;

    HACCEL          m_hAccelTable = nullptr;
    CCreateContext* m_pCreateContext = nullptr;
    std::wstring    m_strTitle;
    std::vector<std::pair<CWnd*, DWORD>> m_bars;

    virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) override
    {
        if (CWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo)) return TRUE;
        if (g_pApp && g_pApp->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo)) return TRUE;
        return FALSE;
    }

    CFrameWnd() = default;

    virtual BOOL LoadFrame(UINT nIDResource, DWORD dwDefaultStyle = WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE,
        CWnd* pParentWnd = nullptr, CCreateContext* pContext = nullptr);
    virtual BOOL OnCreateClient(LPCREATESTRUCT, CCreateContext*) { return TRUE; }
    virtual void RecalcLayout(BOOL bNotify = TRUE);
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;

    int OnCreate(LPCREATESTRUCT lpcs)
    {
        if (CWnd::OnCreate(lpcs) == -1) return -1;
        return OnCreateClient(lpcs, m_pCreateContext) ? 0 : -1;
    }
    void OnSize(UINT nType, int cx, int cy) { RecalcLayout(); CWnd::OnSize(nType, cx, cy); }
    void OnDestroy()
    {
        CWnd::OnDestroy();
        if (CWinApp* pApp = AfxGetApp(); pApp != nullptr && pApp->m_pMainWnd == this)
            ::PostQuitMessage(0);
    }
    void OnInitMenuPopup(CMenu* pMenu, UINT /*nIndex*/, BOOL bSysMenu) { UpdateMenuCmdUI(pMenu, bSysMenu); }
    void OnUpdateFrameTitle(BOOL) {}

    void UpdateFrameTitleForDocument(LPCWSTR lpszDocName)
    {
        std::wstring prefix = m_strTitle.empty() ? (AfxGetApp() && AfxGetApp()->m_pszAppName ? AfxGetApp()->m_pszAppName : L"WinDirStat") : m_strTitle;
        std::wstring t;
        if (lpszDocName && lpszDocName[0] != L'\0')
        {
            t = prefix + L" - " + lpszDocName;
        }
        else
        {
            t = prefix;
        }
        if (m_hWnd) ::SetWindowTextW(m_hWnd, t.c_str());
    }
    void SetTitle(LPCWSTR psz) { m_strTitle = psz ? psz : L""; if (m_hWnd) ::SetWindowTextW(m_hWnd, m_strTitle.c_str()); }
    CString GetTitle() const { return CString(m_strTitle.c_str()); }

    void RegisterControlBar(CWnd* pBar, DWORD align) { m_bars.emplace_back(pBar, align); RecalcLayout(); }
    CWnd* GetClientFrameWnd() const { return CWnd::FromHandle(::GetDlgItem(m_hWnd, AFX_IDW_PANE_FIRST)); }
    void UpdateMenuCmdUI(CMenu* pMenu, BOOL bSysMenu);

    BOOL PreTranslateMessage(MSG* pMsg) override;

    static const AFX_MSGMAP* __stdcall _GetBaseMessageMap() { return CWnd::GetThisMessageMap(); }
    static const AFX_MSGMAP* GetThisMessageMap()
    {
        static const AFX_MSGMAP_ENTRY entries[] = { { 0, 0, 0, 0, nullptr, nullptr, nullptr } };
        static const AFX_MSGMAP map = { &CFrameWnd::_GetBaseMessageMap, entries };
        return &map;
    }
    const AFX_MSGMAP* GetMessageMap() const override { return GetThisMessageMap(); }
};
SHIM_IMPLEMENT_DYNAMIC_INLINE(CFrameWnd, RUNTIME_CLASS(CWnd))

inline BOOL CFrameWnd::PreTranslateMessage(MSG* pMsg)
{
    if (m_hAccelTable && ::TranslateAccelerator(m_hWnd, m_hAccelTable, pMsg))
        return TRUE;
    return CWnd::PreTranslateMessage(pMsg);
}

inline BOOL CFrameWnd::PreCreateWindow(CREATESTRUCT& cs)
{
    if (cs.lpszName) m_strTitle = cs.lpszName;
    if (cs.lpszClass == nullptr)
        cs.lpszClass = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursorW(nullptr, IDC_ARROW),
            reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1),
            ::LoadIconW(AfxGetResourceHandle(), MAKEINTRESOURCEW(128 /*IDR_MAINFRAME*/)));
    return TRUE;
}

inline BOOL CFrameWnd::LoadFrame(UINT nIDResource, DWORD dwDefaultStyle, CWnd* pParentWnd, CCreateContext* pContext)
{
    m_pCreateContext = pContext;
    HMENU hMenu = ::LoadMenuW(AfxGetResourceHandle(), MAKEINTRESOURCEW(nIDResource));
    m_hAccelTable = ::LoadAcceleratorsW(AfxGetResourceHandle(), MAKEINTRESOURCEW(nIDResource));

    CString strTitle; strTitle.LoadString(nIDResource);

    CREATESTRUCT cs{};
    cs.style = dwDefaultStyle;
    cs.lpszName = strTitle.IsEmpty() ? nullptr : strTitle.GetString();
    cs.x = cs.y = CW_USEDEFAULT; cs.cx = cs.cy = CW_USEDEFAULT;
    cs.hMenu = hMenu;
    cs.hwndParent = pParentWnd ? pParentWnd->m_hWnd : nullptr;
    cs.hInstance = AfxGetInstanceHandle();
    if (!PreCreateWindow(cs)) return FALSE;

    g_pWndInit = this;
    const HWND h = ::CreateWindowExW(cs.dwExStyle, cs.lpszClass, cs.lpszName, cs.style,
        cs.x, cs.y, cs.cx, cs.cy, cs.hwndParent, cs.hMenu, cs.hInstance, cs.lpCreateParams);
    g_pWndInit = nullptr;
    if (h == nullptr) return FALSE;
    if (m_hWnd == nullptr) Attach(h);
    RecalcLayout();
    return TRUE;
}

inline void CFrameWnd::RecalcLayout(BOOL)
{
    if (!::IsWindow(m_hWnd)) return;
    CRect rc; GetClientRect(&rc);
    int top = rc.top, bottom = rc.bottom;
    for (auto& [bar, align] : m_bars)
    {
        if (bar == nullptr || !::IsWindow(bar->m_hWnd) || !bar->IsWindowVisible()) continue;
        const CSize sz = bar->CalcFixedLayout(TRUE, TRUE);
        if (align == CBRS_TOP) { bar->SetWindowPos(nullptr, rc.left, top, rc.Width(), sz.cy, SWP_NOZORDER | SWP_NOACTIVATE); top += sz.cy; }
        else if (align == CBRS_BOTTOM) { bar->SetWindowPos(nullptr, rc.left, bottom - sz.cy, rc.Width(), sz.cy, SWP_NOZORDER | SWP_NOACTIVATE); bottom -= sz.cy; }
    }
    if (CWnd* client = GetClientFrameWnd())
        if (::IsWindow(client->m_hWnd))
            client->SetWindowPos(nullptr, rc.left, top, rc.Width(), bottom - top, SWP_NOZORDER | SWP_NOACTIVATE);
}

inline void CFrameWnd::UpdateMenuCmdUI(CMenu* pMenu, BOOL bSysMenu)
{
    if (bSysMenu || pMenu == nullptr || pMenu->m_hMenu == nullptr) return;
    CCmdUI state;
    state.m_pMenu = pMenu;
    const int n = pMenu->GetMenuItemCount();
    state.m_nIndexMax = static_cast<UINT>(n);
    for (int i = 0; i < n; ++i)
    {
        const UINT id = pMenu->GetMenuItemID(i);
        if (id == 0 || id == static_cast<UINT>(-1)) continue;   // separator / popup
        state.m_nIndex = static_cast<UINT>(i);
        state.m_nID = id;
        state.DoUpdate(this, TRUE);
    }
}

class CFrameWndEx : public CFrameWnd
{
public:
    static const CRuntimeClass classCFrameWndEx;
    CRuntimeClass* GetRuntimeClass() const override;
    CFrameWndEx() = default;
    void DockPane(CBasePane* pBar, UINT nDockBarID = 0, LPCRECT lpRect = nullptr);
    void AddPane(CBasePane*) {}
    BOOL EnableDocking(DWORD) { return TRUE; }

    static const AFX_MSGMAP* __stdcall _GetBaseMessageMap() { return CFrameWnd::GetThisMessageMap(); }
    static const AFX_MSGMAP* GetThisMessageMap()
    {
        static const AFX_MSGMAP_ENTRY entries[] = { { 0, 0, 0, 0, nullptr, nullptr, nullptr } };
        static const AFX_MSGMAP map = { &CFrameWndEx::_GetBaseMessageMap, entries };
        return &map;
    }
    const AFX_MSGMAP* GetMessageMap() const override { return GetThisMessageMap(); }
};
SHIM_IMPLEMENT_DYNAMIC_INLINE(CFrameWndEx, RUNTIME_CLASS(CFrameWnd))

// -----------------------------------------------------------------------------
//  CSplitterWnd / CSplitterWndEx (static splitter)
// -----------------------------------------------------------------------------
struct CRowColInfo
{
    int nMinSize = 0;
    int nIdealSize = 0;
    int nCurSize = 0;
};

class CSplitterWnd : public CWnd
{
public:
    static const CRuntimeClass classCSplitterWnd;
    CRuntimeClass* GetRuntimeClass() const override;

    int m_nRows = 0, m_nCols = 0, m_nMaxRows = 0, m_nMaxCols = 0;
    CRowColInfo* m_pRowInfo = nullptr;
    CRowColInfo* m_pColInfo = nullptr;
    int m_cxSplitter = 6, m_cySplitter = 6;
    int m_cxBorder = 0, m_cyBorder = 0;

    CSplitterWnd() = default;
    ~CSplitterWnd() override { delete[] m_pRowInfo; delete[] m_pColInfo; }

    BOOL CreateStatic(CWnd* pParentWnd, int nRows, int nCols,
        DWORD dwStyle = WS_CHILD | WS_VISIBLE, UINT nID = AFX_IDW_PANE_FIRST)
    {
        m_nRows = m_nMaxRows = nRows;
        m_nCols = m_nMaxCols = nCols;
        m_cxBorder = m_cyBorder = (dwStyle & WS_BORDER) ? 1 : 0;
        delete[] m_pRowInfo; delete[] m_pColInfo;
        m_pRowInfo = new CRowColInfo[nRows]{};
        m_pColInfo = new CRowColInfo[nCols]{};
        const CRect rc(0, 0, 0, 0);
        const LPCWSTR cls = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursorW(nullptr, IDC_ARROW), nullptr);
        return CreateEx(0, cls, nullptr, (dwStyle & ~WS_BORDER) | WS_CLIPCHILDREN, rc, pParentWnd, nID);
    }
    virtual BOOL CreateView(int row, int col, CRuntimeClass* pViewClass, SIZE sizeInit, CCreateContext* pContext)
    {
        CWnd* pView = static_cast<CWnd*>(pViewClass->CreateObject());
        if (pView == nullptr) return FALSE;
        const CRect rc(0, 0, sizeInit.cx, sizeInit.cy);
        if (!pView->Create(nullptr, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            rc, this, IdFromRowCol(row, col), pContext))
        {
            delete pView; return FALSE;
        }
        return TRUE;
    }
    CWnd* GetPane(int row, int col) const { return CWnd::FromHandle(::GetDlgItem(m_hWnd, IdFromRowCol(row, col))); }
    int IdFromRowCol(int row, int col) const { return AFX_IDW_PANE_FIRST + row * 16 + col; }
    int GetRowCount() const { return m_nRows; }
    int GetColumnCount() const { return m_nCols; }
    void GetRowInfo(int row, int& cur, int& minimum) const { cur = m_pRowInfo[row].nCurSize; minimum = m_pRowInfo[row].nMinSize; }
    void SetRowInfo(int row, int cur, int minimum) { m_pRowInfo[row].nIdealSize = m_pRowInfo[row].nCurSize = cur; m_pRowInfo[row].nMinSize = minimum; }
    void GetColumnInfo(int col, int& cur, int& minimum) const { cur = m_pColInfo[col].nCurSize; minimum = m_pColInfo[col].nMinSize; }
    void SetColumnInfo(int col, int cur, int minimum) { m_pColInfo[col].nIdealSize = m_pColInfo[col].nCurSize = cur; m_pColInfo[col].nMinSize = minimum; }

    virtual void RecalcLayout();
    virtual void StopTracking(BOOL bAccept);
    void PostNcDestroy() override {}   // splitter is an embedded member; nothing to delete

    // message handlers
    void OnSize(UINT, int, int) { RecalcLayout(); }
    BOOL OnEraseBkgnd(CDC*) { return TRUE; }
    void OnPaint() { CPaintDC dc(this); CRect rc; GetClientRect(&rc); WdsFillSplitterBg(&dc, this, rc); if (m_bTrackerVisible) DrawTrackerRect(dc, m_rectTracker); }
    void OnLButtonDown(UINT nFlags, CPoint pt);
    void OnLButtonUp(UINT nFlags, CPoint pt);
    void OnMouseMove(UINT nFlags, CPoint pt);
    void OnCaptureChanged(CWnd* pWnd);
    void OnCancelMode();
    BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);

    static const AFX_MSGMAP* __stdcall _GetBaseMessageMap() { return CWnd::GetThisMessageMap(); }
    static const AFX_MSGMAP* GetThisMessageMap()
    {
        static const AFX_MSGMAP_ENTRY entries[] = {
            { WM_SIZE, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<void (CSplitterWnd::*)(UINT,int,int)>(&CSplitterWnd::OnSize)), &WdsThunk_Size, nullptr },
            { WM_ERASEBKGND, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<BOOL (CSplitterWnd::*)(CDC*)>(&CSplitterWnd::OnEraseBkgnd)), &WdsThunk_EraseBkgnd, nullptr },
            { WM_PAINT, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<void (CSplitterWnd::*)()>(&CSplitterWnd::OnPaint)), &WdsThunk_Void, nullptr },
            { WM_LBUTTONDOWN, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<void (CSplitterWnd::*)(UINT,CPoint)>(&CSplitterWnd::OnLButtonDown)), &WdsThunk_MouseBtn, nullptr },
            { WM_LBUTTONUP, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<void (CSplitterWnd::*)(UINT,CPoint)>(&CSplitterWnd::OnLButtonUp)), &WdsThunk_MouseBtn, nullptr },
            { WM_MOUSEMOVE, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<void (CSplitterWnd::*)(UINT,CPoint)>(&CSplitterWnd::OnMouseMove)), &WdsThunk_MouseBtn, nullptr },
            { WM_CAPTURECHANGED, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<void (CSplitterWnd::*)(CWnd*)>(&CSplitterWnd::OnCaptureChanged)), &WdsThunk_CaptureChanged, nullptr },
            { WM_CANCELMODE, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<void (CSplitterWnd::*)()>(&CSplitterWnd::OnCancelMode)), &WdsThunk_Void, nullptr },
            { WM_SETCURSOR, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<BOOL (CSplitterWnd::*)(CWnd*,UINT,UINT)>(&CSplitterWnd::OnSetCursor)), &WdsThunk_SetCursor, nullptr },
            { 0, 0, 0, 0, nullptr, nullptr, nullptr }
        };
        static const AFX_MSGMAP map = { &CSplitterWnd::_GetBaseMessageMap, entries };
        return &map;
    }
    const AFX_MSGMAP* GetMessageMap() const override { return GetThisMessageMap(); }

protected:
    bool m_bTracking = false;
    bool m_bTrackingColumn = false;
    bool m_bTrackerVisible = false;
    int m_nTrackPos = 0;
    CRect m_rectTracker;

    CRect GetSplitterWorkRect() const;
    int ClampTrackPos(CPoint pt) const;
    CRect GetTrackerRect(int pos) const;
    void DrawTrackerRect(CDC& dc, const CRect& rect) const;
    void DrawTracker();
    void FinishTracking(BOOL bAccept, bool releaseCapture);
};
SHIM_IMPLEMENT_DYNAMIC_INLINE(CSplitterWnd, RUNTIME_CLASS(CWnd))

inline void CSplitterWnd::RecalcLayout()
{
    if (!::IsWindow(m_hWnd) || m_nRows == 0 || m_nCols == 0) return;
    CRect rc; GetClientRect(&rc);
    CRect rcWork = rc;
    rcWork.DeflateRect(m_cxBorder, m_cyBorder);
    if (m_nCols > 1)
    {
        const int total = rcWork.Width();
        const int bar = m_cxSplitter;
        int w0 = m_pColInfo[0].nCurSize;
        if (w0 <= 0) w0 = (total - bar) / 2;
        w0 = std::clamp(w0, 0, std::max(0, total - bar));
        const int w1 = std::max(0, total - bar - w0);
        m_pColInfo[0].nCurSize = w0; m_pColInfo[1].nCurSize = w1;
        if (CWnd* p = GetPane(0, 0); p && ::IsWindow(p->m_hWnd)) p->MoveWindow(rcWork.left, rcWork.top, w0, rcWork.Height());
        if (CWnd* p = GetPane(0, 1); p && ::IsWindow(p->m_hWnd)) p->MoveWindow(rcWork.left + w0 + bar, rcWork.top, w1, rcWork.Height());
    }
    else if (m_nRows > 1)
    {
        const int total = rcWork.Height();
        const int bar = m_cySplitter;
        int h0 = m_pRowInfo[0].nCurSize;
        if (h0 <= 0) h0 = (total - bar) / 2;
        h0 = std::clamp(h0, 0, std::max(0, total - bar));
        const int h1 = std::max(0, total - bar - h0);
        m_pRowInfo[0].nCurSize = h0; m_pRowInfo[1].nCurSize = h1;
        if (CWnd* p = GetPane(0, 0); p && ::IsWindow(p->m_hWnd)) p->MoveWindow(rcWork.left, rcWork.top, rcWork.Width(), h0);
        if (CWnd* p = GetPane(1, 0); p && ::IsWindow(p->m_hWnd)) p->MoveWindow(rcWork.left, rcWork.top + h0 + bar, rcWork.Width(), h1);
    }
    else if (CWnd* p = GetPane(0, 0); p && ::IsWindow(p->m_hWnd)) p->MoveWindow(rcWork.left, rcWork.top, rcWork.Width(), rcWork.Height());
    Invalidate(FALSE);
}
inline CRect CSplitterWnd::GetSplitterWorkRect() const
{
    CRect rc;
    GetClientRect(&rc);
    rc.DeflateRect(m_cxBorder, m_cyBorder);
    return rc;
}

inline int CSplitterWnd::ClampTrackPos(CPoint pt) const
{
    const CRect rcWork = GetSplitterWorkRect();
    if (m_bTrackingColumn)
        return std::clamp(static_cast<int>(pt.x - rcWork.left), 0, std::max(0, rcWork.Width() - m_cxSplitter));

    return std::clamp(static_cast<int>(pt.y - rcWork.top), 0, std::max(0, rcWork.Height() - m_cySplitter));
}

inline CRect CSplitterWnd::GetTrackerRect(int pos) const
{
    const CRect rcWork = GetSplitterWorkRect();
    if (m_bTrackingColumn)
    {
        const int x = rcWork.left + std::clamp(pos, 0, std::max(0, rcWork.Width() - m_cxSplitter));
        return CRect(x, rcWork.top, x + m_cxSplitter, rcWork.bottom);
    }

    const int y = rcWork.top + std::clamp(pos, 0, std::max(0, rcWork.Height() - m_cySplitter));
    return CRect(rcWork.left, y, rcWork.right, y + m_cySplitter);
}

inline void CSplitterWnd::DrawTrackerRect(CDC& dc, const CRect& rect) const
{
    if (!rect.IsRectEmpty())
        dc.PatBlt(rect.left, rect.top, rect.Width(), rect.Height(), DSTINVERT);
}

inline void CSplitterWnd::DrawTracker()
{
    if (!::IsWindow(m_hWnd) || m_rectTracker.IsRectEmpty())
        return;

    CClientDC dc(this);
    DrawTrackerRect(dc, m_rectTracker);
}

inline void CSplitterWnd::FinishTracking(BOOL bAccept, bool releaseCapture)
{
    if (!m_bTracking)
        return;

    const bool trackingColumn = m_bTrackingColumn;
    const int trackPos = m_nTrackPos;

    if (m_bTrackerVisible)
        DrawTracker();

    m_bTracking = false;
    m_bTrackerVisible = false;
    m_rectTracker.SetRectEmpty();

    if (releaseCapture && ::GetCapture() == m_hWnd)
        ::ReleaseCapture();

    if (!bAccept)
    {
        Invalidate(FALSE);
        return;
    }

    if (trackingColumn && m_nCols > 1 && m_pColInfo != nullptr)
        m_pColInfo[0].nCurSize = trackPos;
    else if (!trackingColumn && m_nRows > 1 && m_pRowInfo != nullptr)
        m_pRowInfo[0].nCurSize = trackPos;

    RecalcLayout();
}

inline void CSplitterWnd::StopTracking(BOOL bAccept) { FinishTracking(bAccept, true); }
inline void CSplitterWnd::OnLButtonDown(UINT, CPoint pt)
{
    if (m_bTracking)
        StopTracking(FALSE);

    const CRect rcWork = GetSplitterWorkRect();
    bool onBar = false;
    bool trackingColumn = false;
    int trackPos = 0;

    if (m_nCols > 1 && m_pColInfo != nullptr)
    {
        trackingColumn = true;
        trackPos = std::clamp(m_pColInfo[0].nCurSize, 0, std::max(0, rcWork.Width() - m_cxSplitter));
        const int x = rcWork.left + trackPos;
        onBar = (pt.x >= x && pt.x <= x + m_cxSplitter);
    }
    else if (m_nRows > 1 && m_pRowInfo != nullptr)
    {
        trackPos = std::clamp(m_pRowInfo[0].nCurSize, 0, std::max(0, rcWork.Height() - m_cySplitter));
        const int y = rcWork.top + trackPos;
        onBar = (pt.y >= y && pt.y <= y + m_cySplitter);
    }

    if (!onBar)
        return;

    m_bTracking = true;
    m_bTrackingColumn = trackingColumn;
    m_nTrackPos = trackPos;
    m_rectTracker = GetTrackerRect(m_nTrackPos);
    m_bTrackerVisible = !m_rectTracker.IsRectEmpty();
    SetCapture();
    if (m_bTrackerVisible)
        DrawTracker();
}
inline void CSplitterWnd::OnMouseMove(UINT, CPoint pt)
{
    if (!m_bTracking) return;

    const int trackPos = ClampTrackPos(pt);
    if (trackPos == m_nTrackPos)
        return;

    if (m_bTrackerVisible)
        DrawTracker();

    m_nTrackPos = trackPos;
    m_rectTracker = GetTrackerRect(m_nTrackPos);
    m_bTrackerVisible = !m_rectTracker.IsRectEmpty();
    if (m_bTrackerVisible)
        DrawTracker();
}
inline void CSplitterWnd::OnLButtonUp(UINT, CPoint) { if (m_bTracking) StopTracking(TRUE); }
inline void CSplitterWnd::OnCaptureChanged(CWnd* pWnd)
{
    if (m_bTracking && (pWnd == nullptr || pWnd->m_hWnd != m_hWnd))
        FinishTracking(FALSE, false);
}
inline void CSplitterWnd::OnCancelMode() { StopTracking(FALSE); }
inline BOOL CSplitterWnd::OnSetCursor(CWnd*, UINT nHitTest, UINT)
{
    if (nHitTest == HTCLIENT)
    {
        CPoint pt; ::GetCursorPos(&pt); ScreenToClient(&pt);
        const CRect rcWork = GetSplitterWorkRect();
        if (m_nCols > 1 && m_pColInfo != nullptr) { const int x = rcWork.left + m_pColInfo[0].nCurSize; if (pt.x >= x && pt.x <= x + m_cxSplitter) { ::SetCursor(::LoadCursorW(nullptr, IDC_SIZEWE)); return TRUE; } }
        else if (m_nRows > 1 && m_pRowInfo != nullptr) { const int y = rcWork.top + m_pRowInfo[0].nCurSize; if (pt.y >= y && pt.y <= y + m_cySplitter) { ::SetCursor(::LoadCursorW(nullptr, IDC_SIZENS)); return TRUE; } }
    }
    return static_cast<BOOL>(Default());
}

class CSplitterWndEx : public CSplitterWnd
{
public:
    static const CRuntimeClass classCSplitterWndEx;
    CRuntimeClass* GetRuntimeClass() const override;
    CSplitterWndEx() = default;

    static const AFX_MSGMAP* __stdcall _GetBaseMessageMap() { return CSplitterWnd::GetThisMessageMap(); }
    static const AFX_MSGMAP* GetThisMessageMap()
    {
        static const AFX_MSGMAP_ENTRY entries[] = { { 0, 0, 0, 0, nullptr, nullptr, nullptr } };
        static const AFX_MSGMAP map = { &CSplitterWndEx::_GetBaseMessageMap, entries };
        return &map;
    }
    const AFX_MSGMAP* GetMessageMap() const override { return GetThisMessageMap(); }
};
SHIM_IMPLEMENT_DYNAMIC_INLINE(CSplitterWndEx, RUNTIME_CLASS(CSplitterWnd))

// -----------------------------------------------------------------------------
//  Feature-pack global data + visual manager
// -----------------------------------------------------------------------------
inline UINT AFXAPI WdsRegisteredMessage(const wchar_t* name) { static std::map<std::wstring, UINT> m; auto& v = m[name]; if (!v) v = ::RegisterWindowMessageW(name); return v; }
inline const UINT AFX_WM_CHANGING_ACTIVE_TAB = WdsRegisteredMessage(L"AFX_WM_CHANGING_ACTIVE_TAB");
inline const UINT AFX_WM_CHANGE_ACTIVE_TAB = WdsRegisteredMessage(L"AFX_WM_CHANGE_ACTIVE_TAB");

inline COLORREF WdsBlendColor(COLORREF from, COLORREF to, double amount)
{
    const auto ch = [amount](BYTE a, BYTE b) -> BYTE
    {
        return static_cast<BYTE>(std::clamp<int>(static_cast<int>(a + (b - a) * amount), 0, 255));
    };
    return RGB(ch(GetRValue(from), GetRValue(to)), ch(GetGValue(from), GetGValue(to)), ch(GetBValue(from), GetBValue(to)));
}

struct AFX_GLOBAL_DATA
{
    COLORREF clrBarFace = ::GetSysColor(COLOR_BTNFACE);
    COLORREF clrBarShadow = ::GetSysColor(COLOR_BTNSHADOW);
    COLORREF clrBarHilite = ::GetSysColor(COLOR_BTNHIGHLIGHT);
    COLORREF clrBtnFace = ::GetSysColor(COLOR_BTNFACE);
    COLORREF clrBtnShadow = ::GetSysColor(COLOR_BTNSHADOW);
    COLORREF clrBtnHilite = ::GetSysColor(COLOR_BTNHIGHLIGHT);
    COLORREF clrBtnText = ::GetSysColor(COLOR_BTNTEXT);
    COLORREF clrBtnDkShadow = ::GetSysColor(COLOR_3DDKSHADOW);
    COLORREF clrBtnLight = ::GetSysColor(COLOR_3DLIGHT);
    COLORREF clrWindow = ::GetSysColor(COLOR_WINDOW);
    COLORREF clrWindowText = ::GetSysColor(COLOR_WINDOWTEXT);
    COLORREF clrHilite = ::GetSysColor(COLOR_HIGHLIGHT);
    COLORREF clrTextHilite = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
    COLORREF clrGrayedText = ::GetSysColor(COLOR_GRAYTEXT);
    CBrush brBarFace;
    CBrush brBtnFace;
    AFX_GLOBAL_DATA() { brBarFace.CreateSolidBrush(clrBarFace); brBtnFace.CreateSolidBrush(clrBtnFace); }
};
inline AFX_GLOBAL_DATA& WdsGlobalData() { static AFX_GLOBAL_DATA d; return d; }
inline AFX_GLOBAL_DATA* AFXAPI GetGlobalData() { return &WdsGlobalData(); }
#define afxGlobalData WdsGlobalData()

class CBasePane;
class CMFCStatusBar;
class CMFCBaseTabCtrl;
class CSplitterWndEx;

class CMFCVisualManager : public CObject
{
public:
    static const CRuntimeClass classCMFCVisualManager;
    CRuntimeClass* GetRuntimeClass() const override;

    static CMFCVisualManager* GetInstance();
    static void SetDefaultManager(CRuntimeClass* pRTC);

    virtual ~CMFCVisualManager() = default;
    virtual BOOL IsDark() const { return FALSE; }
    virtual void OnUpdateSystemColors() {}
    virtual void GetTabFrameColors(const CMFCBaseTabCtrl* pTabWnd, COLORREF& clrDark, COLORREF& clrBlack,
        COLORREF& clrHighlight, COLORREF& clrFace, COLORREF& clrDarkShadow, COLORREF& clrLight,
        CBrush*& pbrFace, CBrush*& pbrBlack);
    virtual void OnFillBarBackground(CDC* pDC, CBasePane* pBar, CRect rectClient, CRect rectClip, BOOL bNCArea = FALSE);
    virtual void OnDrawSeparator(CDC* pDC, CBasePane* pBar, CRect rect, BOOL bIsHoriz);
    virtual void OnDrawStatusBarPaneBorder(CDC* pDC, CMFCStatusBar* pBar, CRect rectPane, UINT uiID, UINT nStyle);
    virtual void OnFillSplitterBackground(CDC* pDC, CSplitterWndEx* pSplitterWnd, CRect rect);

protected:
    inline static CMFCVisualManager* s_pInstance = nullptr;
    inline static CRuntimeClass* s_pRTCDefault = nullptr;
};
SHIM_IMPLEMENT_DYNAMIC_INLINE(CMFCVisualManager, RUNTIME_CLASS(CObject))

class CMFCVisualManagerWindows : public CMFCVisualManager
{
public:
    static const CRuntimeClass classCMFCVisualManagerWindows;
    CRuntimeClass* GetRuntimeClass() const override;
    static CObject* __stdcall CreateObject() { return new CMFCVisualManagerWindows; }
};
inline const CRuntimeClass CMFCVisualManagerWindows::classCMFCVisualManagerWindows =
    { "CMFCVisualManagerWindows", sizeof(CMFCVisualManagerWindows), 0xFFFF, &CMFCVisualManagerWindows::CreateObject, RUNTIME_CLASS(CMFCVisualManager) };
inline CRuntimeClass* CMFCVisualManagerWindows::GetRuntimeClass() const { return const_cast<CRuntimeClass*>(&classCMFCVisualManagerWindows); }

inline CMFCVisualManager* CMFCVisualManager::GetInstance()
{
    if (s_pInstance == nullptr)
        s_pInstance = s_pRTCDefault ? static_cast<CMFCVisualManager*>(s_pRTCDefault->CreateObject())
                                    : new CMFCVisualManagerWindows;
    return s_pInstance;
}
inline void CMFCVisualManager::SetDefaultManager(CRuntimeClass* pRTC)
{
    s_pRTCDefault = pRTC;
    delete s_pInstance; s_pInstance = nullptr;
    GetInstance();
    GetInstance()->OnUpdateSystemColors();
}
inline void CMFCVisualManager::GetTabFrameColors(const CMFCBaseTabCtrl*, COLORREF& clrDark, COLORREF& clrBlack,
    COLORREF& clrHighlight, COLORREF& clrFace, COLORREF& clrDarkShadow, COLORREF& clrLight,
    CBrush*& pbrFace, CBrush*& pbrBlack)
{
    static CBrush brFace(::GetSysColor(COLOR_3DFACE));
    static CBrush brBlack(RGB(0, 0, 0));
    clrFace = ::GetSysColor(COLOR_3DFACE);
    clrDark = ::GetSysColor(COLOR_3DSHADOW);
    clrBlack = RGB(0, 0, 0);
    clrHighlight = ::GetSysColor(COLOR_3DHILIGHT);
    clrDarkShadow = ::GetSysColor(COLOR_3DDKSHADOW);
    clrLight = ::GetSysColor(COLOR_3DLIGHT);
    pbrFace = &brFace; pbrBlack = &brBlack;
}
inline void CMFCVisualManager::OnFillBarBackground(CDC* pDC, CBasePane*, CRect rectClient, CRect, BOOL)
{ pDC->FillSolidRect(rectClient, ::GetSysColor(COLOR_BTNFACE)); }
inline void CMFCVisualManager::OnDrawSeparator(CDC* pDC, CBasePane*, CRect rect, BOOL bIsHoriz)
{
    if (bIsHoriz) pDC->FillSolidRect(rect.left + rect.Width() / 2, rect.top + 2, 1, rect.Height() - 4, ::GetSysColor(COLOR_3DSHADOW));
    else pDC->FillSolidRect(rect.left + 2, rect.top + rect.Height() / 2, rect.Width() - 4, 1, ::GetSysColor(COLOR_3DSHADOW));
}
inline void CMFCVisualManager::OnDrawStatusBarPaneBorder(CDC* pDC, CMFCStatusBar*, CRect rectPane, UINT, UINT)
{ pDC->FillSolidRect(rectPane.right - 1, rectPane.top, 1, rectPane.Height(), ::GetSysColor(COLOR_3DSHADOW)); }
inline void CMFCVisualManager::OnFillSplitterBackground(CDC* pDC, CSplitterWndEx* pSplitterWnd, CRect rect)
{
    const COLORREF face = afxGlobalData.clrBtnFace;
    const COLORREF highlight = WdsBlendColor(face, RGB(255, 255, 255), 0.65);
    const COLORREF midline = WdsBlendColor(face, afxGlobalData.clrBtnShadow, 0.18);
    const COLORREF border = WdsBlendColor(face, afxGlobalData.clrBtnShadow, 0.28);

    pDC->FillSolidRect(rect, face);

    if (pSplitterWnd == nullptr)
        return;

    auto drawOutline = [&]
    {
        if (pSplitterWnd->m_cxBorder <= 0 && pSplitterWnd->m_cyBorder <= 0)
            return;

        pDC->FillSolidRect(rect.left, rect.top, rect.Width(), 1, highlight);
        pDC->FillSolidRect(rect.left, rect.top, 1, rect.Height(), highlight);
        pDC->FillSolidRect(rect.left, rect.bottom - 1, rect.Width(), 1, border);
        pDC->FillSolidRect(rect.right - 1, rect.top, 1, rect.Height(), border);
    };

    auto drawVerticalBar = [&](const CRect& bar)
    {
        if (bar.IsRectEmpty())
            return;

        pDC->FillSolidRect(bar, face);
        pDC->FillSolidRect(bar.left, bar.top, 1, bar.Height(), highlight);
        pDC->FillSolidRect(bar.right - 1, bar.top, 1, bar.Height(), border);
        const int x = bar.left + bar.Width() / 2;
        pDC->FillSolidRect(x, bar.top + 2, 1, std::max(0, bar.Height() - 4), midline);
    };

    auto drawHorizontalBar = [&](const CRect& bar)
    {
        if (bar.IsRectEmpty())
            return;

        pDC->FillSolidRect(bar, face);
        pDC->FillSolidRect(bar.left, bar.top, bar.Width(), 1, highlight);
        pDC->FillSolidRect(bar.left, bar.bottom - 1, bar.Width(), 1, border);
        const int y = bar.top + bar.Height() / 2;
        pDC->FillSolidRect(bar.left + 2, y, std::max(0, bar.Width() - 4), 1, midline);
    };

    CRect rcWork = rect;
    rcWork.DeflateRect(pSplitterWnd->m_cxBorder, pSplitterWnd->m_cyBorder);
    drawOutline();

    if (pSplitterWnd->m_nCols > 1 && pSplitterWnd->m_pColInfo != nullptr)
    {
        const int x = rcWork.left + pSplitterWnd->m_pColInfo[0].nCurSize;
        drawVerticalBar(CRect(x, rcWork.top, x + pSplitterWnd->m_cxSplitter, rcWork.bottom));
    }
    if (pSplitterWnd->m_nRows > 1 && pSplitterWnd->m_pRowInfo != nullptr)
    {
        const int y = rcWork.top + pSplitterWnd->m_pRowInfo[0].nCurSize;
        drawHorizontalBar(CRect(rcWork.left, y, rcWork.right, y + pSplitterWnd->m_cySplitter));
    }
}

inline void AFXAPI WdsFillSplitterBg(CDC* pDC, CWnd* pSplitter, const CRect& rc)
{ CMFCVisualManager::GetInstance()->OnFillSplitterBackground(pDC, reinterpret_cast<CSplitterWndEx*>(pSplitter), rc); }

// -----------------------------------------------------------------------------
//  CBasePane / CMFCButton
// -----------------------------------------------------------------------------
class CBasePane : public CWnd
{
public:
    static const CRuntimeClass classCBasePane;
    CRuntimeClass* GetRuntimeClass() const override;
    DWORD m_dwControlBarStyle = 0;     // CBRS alignment
    DWORD m_dwPaneStyle = 0;
    DWORD GetPaneStyle() const { return m_dwPaneStyle; }
    void  SetPaneStyle(DWORD dw) { m_dwPaneStyle = dw; }
    void  SetBorders(const CRect& = CRect()) {}
    BOOL  IsHorizontal() const { return TRUE; }
};
SHIM_IMPLEMENT_DYNAMIC_INLINE(CBasePane, RUNTIME_CLASS(CWnd))

class CMFCButton final : public CButton
{
public:
    CMFCButton() = default;
    void SetImage(HBITMAP) {}
    void SetImage(HICON) {}
    void EnableWindowsTheming(BOOL = TRUE) {}
};

// -----------------------------------------------------------------------------
//  CFrameWndEx::DockPane (needs CBasePane)
// -----------------------------------------------------------------------------
inline void CFrameWndEx::DockPane(CBasePane* pBar, UINT, LPCRECT)
{
    if (pBar == nullptr) return;
    const DWORD align = pBar->m_dwControlBarStyle & CBRS_ALIGN_ANY;
    const DWORD side = (align & CBRS_ALIGN_BOTTOM) ? CBRS_BOTTOM : CBRS_TOP;
    RegisterControlBar(pBar, side);
}

// -----------------------------------------------------------------------------
//  CMFCToolBar
// -----------------------------------------------------------------------------
inline HBITMAP CreateDisabledBitmap(HBITMAP hbmSrc)
{
    BITMAP bmp = {};
    if (::GetObjectW(hbmSrc, sizeof(BITMAP), &bmp) == 0 || bmp.bmBits == nullptr)
        return nullptr;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bmp.bmWidth;
    bmi.bmiHeader.biHeight = bmp.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pNewBits = nullptr;
    HBITMAP hbmDst = ::CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &pNewBits, nullptr, 0);
    if (!hbmDst) return nullptr;

    const int pixelCount = bmp.bmWidth * (bmp.bmHeight < 0 ? -bmp.bmHeight : bmp.bmHeight);
    const BYTE* src = static_cast<const BYTE*>(bmp.bmBits);
    BYTE* dst = static_cast<BYTE*>(pNewBits);

    const COLORREF disabledText = afxGlobalData.clrGrayedText;
    const BYTE disabledR = GetRValue(disabledText);
    const BYTE disabledG = GetGValue(disabledText);
    const BYTE disabledB = GetBValue(disabledText);

    for (int i = 0; i < pixelCount; ++i)
    {
        BYTE b = src[i * 4 + 0];
        BYTE g = src[i * 4 + 1];
        BYTE r = src[i * 4 + 2];
        BYTE a = src[i * 4 + 3];

        // Grayscale using NTSC weights
        BYTE gray = static_cast<BYTE>((r * 299 + g * 587 + b * 114) / 1000);

        dst[i * 4 + 0] = static_cast<BYTE>((disabledB * 3 + gray) / 4);
        dst[i * 4 + 1] = static_cast<BYTE>((disabledG * 3 + gray) / 4);
        dst[i * 4 + 2] = static_cast<BYTE>((disabledR * 3 + gray) / 4);
        dst[i * 4 + 3] = static_cast<BYTE>((static_cast<int>(a) * 3) / 4);
    }

    return hbmDst;
}

inline HBITMAP WdsScaleToolbarBitmap(HBITMAP hbmSrc, int cx, int cy)
{
    BITMAP bmp = {};
    if (hbmSrc == nullptr || ::GetObjectW(hbmSrc, sizeof(BITMAP), &bmp) == 0 || cx <= 0 || cy <= 0)
        return nullptr;

    const int srcCx = bmp.bmWidth;
    const int srcCy = bmp.bmHeight < 0 ? -bmp.bmHeight : bmp.bmHeight;
    if (srcCx == cx && srcCy == cy)
        return nullptr;

    std::vector<BYTE> srcPixels(static_cast<size_t>(srcCx) * static_cast<size_t>(srcCy) * 4);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = srcCx;
    bmi.bmiHeader.biHeight = -srcCy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = ::GetDC(nullptr);
    const int copied = hdc != nullptr ?
        ::GetDIBits(hdc, hbmSrc, 0, static_cast<UINT>(srcCy), srcPixels.data(), &bmi, DIB_RGB_COLORS) : 0;
    if (hdc != nullptr) ::ReleaseDC(nullptr, hdc);
    if (copied == 0)
        return nullptr;

    Gdiplus::Bitmap src(srcCx, srcCy, srcCx * 4, PixelFormat32bppPARGB, srcPixels.data());
    if (src.GetLastStatus() != Gdiplus::Ok)
        return nullptr;

    Gdiplus::Bitmap dst(cx, cy, PixelFormat32bppPARGB);
    Gdiplus::Graphics graphics(&dst);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
    graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
    if (graphics.DrawImage(&src, 0, 0, cx, cy) != Gdiplus::Ok)
        return nullptr;

    HBITMAP hbmDst = nullptr;
    return dst.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hbmDst) == Gdiplus::Ok ? hbmDst : nullptr;
}

class CMFCToolBarImages final
{
public:
    HIMAGELIST m_hImageList = nullptr;
    HIMAGELIST m_hDisabledImageList = nullptr;
    int m_cx = 16, m_cy = 16;
    ~CMFCToolBarImages()
    {
        if (m_hImageList) ::ImageList_Destroy(m_hImageList);
        if (m_hDisabledImageList) ::ImageList_Destroy(m_hDisabledImageList);
    }
    void SetImageSize(SIZE sz, BOOL = FALSE) { m_cx = WdsDpiScale(sz.cx); m_cy = WdsDpiScale(sz.cy); Recreate(); }
    void Recreate()
    {
        if (m_hImageList) ::ImageList_Destroy(m_hImageList);
        if (m_hDisabledImageList) ::ImageList_Destroy(m_hDisabledImageList);

        m_hImageList = ::ImageList_Create(m_cx, m_cy, ILC_COLOR32, 0, 16);
        if (m_hImageList) ::ImageList_SetBkColor(m_hImageList, CLR_NONE);

        m_hDisabledImageList = ::ImageList_Create(m_cx, m_cy, ILC_COLOR32, 0, 16);
        if (m_hDisabledImageList) ::ImageList_SetBkColor(m_hDisabledImageList, CLR_NONE);
    }
    void Clear() { Recreate(); }
    int AddImage(CBitmap& bmp, BOOL = TRUE)
    {
        if (!m_hImageList) Recreate();
        const HBITMAP hbmOriginal = static_cast<HBITMAP>(bmp.m_hObject);
        HBITMAP hbmScaled = WdsScaleToolbarBitmap(hbmOriginal, m_cx, m_cy);
        HBITMAP hbmToAdd = hbmScaled != nullptr ? hbmScaled : hbmOriginal;
        int index = ::ImageList_Add(m_hImageList, hbmToAdd, nullptr);
        HBITMAP hDisabledBmp = CreateDisabledBitmap(hbmToAdd);
        if (hDisabledBmp)
        {
            ::ImageList_Add(m_hDisabledImageList, hDisabledBmp, nullptr);
            ::DeleteObject(hDisabledBmp);
        }
        if (hbmScaled != nullptr) ::DeleteObject(hbmScaled);
        return index;
    }
};

class CMFCToolBarButton final : public CObject
{
public:
    UINT    m_nID = 0;
    int     m_iImage = -1;
    CString m_strText;
    BOOL    m_bText = FALSE;
    UINT    m_nStyle = 0;
    CMFCToolBarButton() = default;
    CMFCToolBarButton(UINT uiId, int iImage, LPCWSTR lpszText = nullptr, BOOL = FALSE, BOOL = FALSE)
        : m_nID(uiId), m_iImage(iImage) { if (lpszText) m_strText = lpszText; }
};

class CMFCToolBar : public CBasePane
{
public:
    static const CRuntimeClass classCMFCToolBar;
    CRuntimeClass* GetRuntimeClass() const override;

    static CMFCToolBarImages* GetImages() { static CMFCToolBarImages img; return &img; }
    static CSize& s_ButtonSize() { static CSize sz(23, 22); return sz; }
    static void SetSizes(SIZE sizeButton, SIZE sizeImage) { s_ButtonSize() = sizeButton; GetImages()->SetImageSize(sizeImage); }

    CMFCToolBar() = default;

    BOOL CreateEx(CWnd* pParentWnd, DWORD /*dwCtrlStyle*/ = TBSTYLE_FLAT, DWORD dwStyle = WS_CHILD | WS_VISIBLE | CBRS_TOP)
    {
        m_dwControlBarStyle = dwStyle & CBRS_ALIGN_ANY;
        m_dwPaneStyle = dwStyle;
        const DWORD win = WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_LIST |
            CCS_NORESIZE | CCS_NOPARENTALIGN | CCS_NODIVIDER | TBSTYLE_TRANSPARENT;
        const CRect rc(0, 0, 0, 0);
        if (!CWnd::CreateEx(0, TOOLBARCLASSNAMEW, nullptr, win, rc, pParentWnd, AFX_IDW_TOOLBAR)) return FALSE;
        SendSelf(TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON));
        SendSelf(TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_HIDECLIPPEDBUTTONS);
        SendSelf(TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(GetImages()->m_hImageList));
        SendSelf(TB_SETDISABLEDIMAGELIST, 0, reinterpret_cast<LPARAM>(GetImages()->m_hDisabledImageList));
        return TRUE;
    }

    int  GetCount() const { return static_cast<int>(SendSelf(TB_BUTTONCOUNT)); }
    void RemoveButton(int i) { SendSelf(TB_DELETEBUTTON, static_cast<WPARAM>(i)); m_tips.clear(); }
    void InsertSeparator(int i = -1) { TBBUTTON b{}; b.fsStyle = BTNS_SEP; b.iBitmap = WdsDpiScale(8, m_hWnd); SendSelf(TB_INSERTBUTTONW, i < 0 ? GetCount() : i, reinterpret_cast<LPARAM>(&b)); }
    void InsertButton(const CMFCToolBarButton& btn, int i = -1)
    {
        TBBUTTON b{};
        b.iBitmap = btn.m_iImage;
        b.idCommand = static_cast<int>(btn.m_nID);
        b.fsState = (btn.m_nStyle & TBBS_DISABLED) ? 0 : TBSTATE_ENABLED;
        b.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
        if (!btn.m_strText.IsEmpty())
        {
            std::wstring wstr = btn.m_strText.GetString();
            wstr.push_back(L'\0');
            LRESULT idx = SendSelf(TB_ADDSTRINGW, 0, reinterpret_cast<LPARAM>(wstr.c_str()));
            b.iString = (idx != -1) ? idx : -1;
            m_tips[btn.m_nID] = btn.m_strText.GetString();
        }
        SendSelf(TB_INSERTBUTTONW, i < 0 ? GetCount() : i, reinterpret_cast<LPARAM>(&b));
    }
    void AdjustLayout()
    {
        SendSelf(TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(GetImages()->m_hImageList));
        SendSelf(TB_SETDISABLEDIMAGELIST, 0, reinterpret_cast<LPARAM>(GetImages()->m_hDisabledImageList));
        SendSelf(TB_SETBITMAPSIZE, 0, MAKELPARAM(GetImages()->m_cx, GetImages()->m_cy));
        SendSelf(TB_SETBUTTONSIZE, 0, MAKELPARAM(s_ButtonSize().cx, s_ButtonSize().cy));
        SendSelf(TB_AUTOSIZE);
        CFrameWnd* pFrame = DYNAMIC_DOWNCAST(CFrameWnd, GetParent());
        if (pFrame) pFrame->RecalcLayout();
        g_idleCmdUiUpdate = [this] { if (CFrameWnd* f = DYNAMIC_DOWNCAST(CFrameWnd, GetParent())) OnUpdateCmdUI(f); };
        if (pFrame) OnUpdateCmdUI(pFrame);
    }
    int  CommandToIndex(UINT nID) const { return static_cast<int>(SendSelf(TB_COMMANDTOINDEX, nID)); }
    BOOL GetItemRect(int i, LPRECT rc) const { return static_cast<BOOL>(SendSelf(TB_GETITEMRECT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(rc))); }
    CSize GetButtonSize() const
    {
        if (GetCount() == 0) return s_ButtonSize();
        const DWORD r = static_cast<DWORD>(SendSelf(TB_GETBUTTONSIZE));
        const CSize sz(LOWORD(r), HIWORD(r));
        return sz.cx > 0 && sz.cy > 0 ? sz : s_ButtonSize();
    }
    int  GetRowHeight() const { return GetButtonSize().cy + WdsDpiScale(2, m_hWnd); }
    void SetHeight(int h) { m_height = h; }

    CSize CalcFixedLayout(BOOL, BOOL) override
    {
        CRect rcParent; if (GetParent()) GetParent()->GetClientRect(&rcParent);
        const int h = GetRowHeight();
        return CSize(rcParent.Width(), h);
    }

    void OnUpdateCmdUI(CFrameWnd* pTarget)
    {
        struct CToolBarCmdUI : CCmdUI
        {
            CMFCToolBar* pBar = nullptr;
            void Enable(BOOL bOn) override { pBar->SendSelf(TB_ENABLEBUTTON, m_nID, MAKELONG(bOn, 0)); }
            void SetCheck(int nCheck) override { pBar->SendSelf(TB_CHECKBUTTON, m_nID, MAKELONG(nCheck != 0, 0)); }
            void SetRadio(BOOL bOn) override { SetCheck(bOn); }
            void SetText(LPCWSTR) override {}
        };
        const int n = GetCount();
        CToolBarCmdUI state; state.pBar = this; state.m_nIndexMax = static_cast<UINT>(n);
        for (int i = 0; i < n; ++i)
        {
            TBBUTTON b{}; SendSelf(TB_GETBUTTON, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&b));
            if (b.idCommand == 0 || (b.fsStyle & BTNS_SEP)) continue;
            state.m_nID = static_cast<UINT>(b.idCommand);
            state.m_nIndex = static_cast<UINT>(i);
            state.DoUpdate(pTarget, TRUE);
        }
    }

    void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
    {
        auto* cd = reinterpret_cast<LPNMTBCUSTOMDRAW>(pNMHDR);
        switch (cd->nmcd.dwDrawStage)
        {
        case CDDS_PREPAINT:
        {
            CRect rcClient; GetClientRect(&rcClient);
            CMFCVisualManager::GetInstance()->OnFillBarBackground(CDC::FromHandle(cd->nmcd.hdc), this, rcClient, rcClient, FALSE);
            *pResult = CDRF_NOTIFYITEMDRAW;
            break;
        }
        case CDDS_ITEMPREPAINT:
            cd->clrText = afxGlobalData.clrBtnText;
            *pResult = TBCDRF_USECDCOLORS | CDRF_DODEFAULT;
            break;
        default:
            *pResult = CDRF_DODEFAULT;
            break;
        }
    }
    void OnGetInfoTip(NMHDR* pNMHDR, LRESULT* pResult)
    {
        auto* tip = reinterpret_cast<LPNMTBGETINFOTIPW>(pNMHDR);
        if (const auto it = m_tips.find(static_cast<UINT>(tip->iItem)); it != m_tips.end())
            wcsncpy_s(tip->pszText, tip->cchTextMax, it->second.c_str(), _TRUNCATE);
        *pResult = 0;
    }

    static const AFX_MSGMAP* __stdcall _GetBaseMessageMap() { return CBasePane::GetThisMessageMap(); }
    static const AFX_MSGMAP* GetThisMessageMap()
    {
        static const AFX_MSGMAP_ENTRY entries[] = {
            { WM_NOTIFY, NM_CUSTOMDRAW, WdsReflectID, WdsReflectID, reinterpret_cast<AFX_PMSG>(static_cast<void (CMFCToolBar::*)(NMHDR*,LRESULT*)>(&CMFCToolBar::OnCustomDraw)), &WdsThunk_Notify, nullptr },
            { WM_NOTIFY, TBN_GETINFOTIPW, WdsReflectID, WdsReflectID, reinterpret_cast<AFX_PMSG>(static_cast<void (CMFCToolBar::*)(NMHDR*,LRESULT*)>(&CMFCToolBar::OnGetInfoTip)), &WdsThunk_Notify, nullptr },
            { 0, 0, 0, 0, nullptr, nullptr, nullptr }
        };
        static const AFX_MSGMAP map = { &CMFCToolBar::_GetBaseMessageMap, entries };
        return &map;
    }
    const AFX_MSGMAP* GetMessageMap() const override { return GetThisMessageMap(); }

private:
    int m_height = 0;
    std::map<UINT, std::wstring> m_tips;
};
SHIM_IMPLEMENT_DYNAMIC_INLINE(CMFCToolBar, RUNTIME_CLASS(CBasePane))

// -----------------------------------------------------------------------------
//  CMFCStatusBar (custom drawn, dark-mode aware via the visual manager)
// -----------------------------------------------------------------------------
class CMFCStatusBar : public CBasePane
{
public:
    static const CRuntimeClass classCMFCStatusBar;
    CRuntimeClass* GetRuntimeClass() const override;

    struct Pane { UINT id = 0; UINT style = 0; int width = 0; std::wstring text; COLORREF bg = CLR_NONE; };

    CMFCStatusBar() = default;

    BOOL Create(CWnd* pParentWnd, DWORD = WS_CHILD | WS_VISIBLE | CBRS_BOTTOM, UINT nID = AFX_IDW_STATUS_BAR)
    {
        m_dwControlBarStyle = CBRS_BOTTOM;
        m_font.Attach(::GetStockObject(DEFAULT_GUI_FONT));
        static const wchar_t kCls[] = L"WdsStatusBar32";
        WNDCLASSEXW wc{ sizeof(wc) };
        wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = AfxWndProc;
        wc.hInstance = AfxGetInstanceHandle();
        wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kCls;
        RegisterClassExW(&wc);
        const CRect rc(0, 0, 0, 0);
        if (!CWnd::CreateEx(0, kCls, nullptr, WS_CHILD | WS_VISIBLE, rc, pParentWnd, nID)) return FALSE;
        if (CFrameWnd* f = DYNAMIC_DOWNCAST(CFrameWnd, pParentWnd)) f->RegisterControlBar(this, CBRS_BOTTOM);
        return TRUE;
    }
    BOOL SetIndicators(const UINT* lpIDArray, int nIDCount)
    {
        m_panes.clear();
        for (int i = 0; i < nIDCount; ++i)
        {
            Pane p; p.id = lpIDArray[i];
            CString s; s.LoadString(lpIDArray[i]); p.text = s.GetString();
            p.width = 100;
            m_panes.push_back(std::move(p));
        }
        Invalidate();
        return TRUE;
    }
    int  GetCount() const { return static_cast<int>(m_panes.size()); }
    void SetPaneStyle(int i, UINT style) { if (i >= 0 && i < GetCount()) { m_panes[i].style = style; Invalidate(); } }
    UINT GetPaneStyle(int i) const { return (i >= 0 && i < GetCount()) ? m_panes[i].style : 0; }
    void SetPaneText(int i, LPCWSTR text) { if (i >= 0 && i < GetCount()) { m_panes[i].text = text ? text : L""; Invalidate(); } }
    void SetPaneWidth(int i, int cx) { if (i >= 0 && i < GetCount()) { m_panes[i].width = cx; Invalidate(); } }
    void SetPaneBackgroundColor(int i, COLORREF cr) { if (i >= 0 && i < GetCount()) { m_panes[i].bg = cr; Invalidate(); } }
    void GetItemRect(int i, LPRECT rc) const { const auto rects = LayoutPanes(); if (i >= 0 && i < static_cast<int>(rects.size())) *rc = rects[i]; else *rc = CRect(); }
    void GetItemRect(int i, CRect& rc) const { GetItemRect(i, static_cast<LPRECT>(&rc)); }

    CSize CalcFixedLayout(BOOL, BOOL) override
    {
        CRect rcParent; if (GetParent()) GetParent()->GetClientRect(&rcParent);
        return CSize(rcParent.Width(), DpiScaleHeight());
    }

    BOOL OnEraseBkgnd(CDC*) { return TRUE; }
    void OnSize(UINT, int, int) { Invalidate(); }
    void OnPaint()
    {
        CPaintDC dc(this);
        CMemDC mem(dc, this);
        CDC* pDC = &mem.GetDC();
        CRect rcClient; GetClientRect(&rcClient);
        pDC->FillSolidRect(rcClient, afxGlobalData.clrBarFace);
        CGdiObject* pOld = pDC->SelectObject(&m_font);
        pDC->SetBkMode(TRANSPARENT);

        const auto rects = LayoutPanes();
        for (int i = 0; i < GetCount(); ++i)
        {
            const Pane& p = m_panes[i];
            CRect rc = rects[i];
            const COLORREF bg = (p.bg != CLR_NONE) ? p.bg : afxGlobalData.clrBarFace;
            pDC->FillSolidRect(rc, bg);
            CMFCVisualManager::GetInstance()->OnDrawStatusBarPaneBorder(pDC, this, rc, p.id, p.style);
            CRect rcText = rc; rcText.DeflateRect(4, 0, 4, 0);
            pDC->SetTextColor(afxGlobalData.clrBtnText);
            pDC->DrawText(p.text.c_str(), static_cast<int>(p.text.size()), &rcText, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX);
        }
        pDC->SelectObject(pOld);
    }

    static const AFX_MSGMAP* __stdcall _GetBaseMessageMap() { return CBasePane::GetThisMessageMap(); }
    static const AFX_MSGMAP* GetThisMessageMap()
    {
        static const AFX_MSGMAP_ENTRY entries[] = {
            { WM_PAINT, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<void (CMFCStatusBar::*)()>(&CMFCStatusBar::OnPaint)), &WdsThunk_Void, nullptr },
            { WM_ERASEBKGND, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<BOOL (CMFCStatusBar::*)(CDC*)>(&CMFCStatusBar::OnEraseBkgnd)), &WdsThunk_EraseBkgnd, nullptr },
            { WM_SIZE, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<void (CMFCStatusBar::*)(UINT,int,int)>(&CMFCStatusBar::OnSize)), &WdsThunk_Size, nullptr },
            { 0, 0, 0, 0, nullptr, nullptr, nullptr }
        };
        static const AFX_MSGMAP map = { &CMFCStatusBar::_GetBaseMessageMap, entries };
        return &map;
    }
    const AFX_MSGMAP* GetMessageMap() const override { return GetThisMessageMap(); }

private:
    int DpiScaleHeight() const
    {
        const HDC hdc = ::GetDC(m_hWnd); const int dpi = hdc ? ::GetDeviceCaps(hdc, LOGPIXELSY) : 96; if (hdc) ::ReleaseDC(m_hWnd, hdc);
        return MulDiv(22, dpi, 96);
    }
    std::vector<CRect> LayoutPanes() const
    {
        std::vector<CRect> rects(m_panes.size());
        if (!::IsWindow(m_hWnd)) return rects;
        CRect rc; GetClientRect(&rc);
        int fixedTotal = 0, stretchCount = 0;
        for (const auto& p : m_panes) { if (p.style & SBPS_STRETCH) ++stretchCount; else fixedTotal += p.width; }
        const int stretchWidth = std::max(0, (rc.Width() - fixedTotal) / std::max(1, stretchCount));
        int x = rc.left;
        for (size_t i = 0; i < m_panes.size(); ++i)
        {
            const int w = (m_panes[i].style & SBPS_STRETCH) ? stretchWidth : m_panes[i].width;
            rects[i] = CRect(x, rc.top, x + w, rc.bottom);
            x += w;
        }
        if (!rects.empty()) rects.back().right = rc.right;   // last pane snaps to edge
        return rects;
    }
    std::vector<Pane> m_panes;
    CFont m_font;
};
SHIM_IMPLEMENT_DYNAMIC_INLINE(CMFCStatusBar, RUNTIME_CLASS(CBasePane))

// -----------------------------------------------------------------------------
//  CMFCBaseTabCtrl / CMFCTabCtrl
// -----------------------------------------------------------------------------
class CMFCBaseTabCtrl : public CWnd
{
public:
    static const CRuntimeClass classCMFCBaseTabCtrl;
    CRuntimeClass* GetRuntimeClass() const override;

    enum Style { STYLE_3D = 0, STYLE_3D_SCROLLED, STYLE_3D_ONENOTE, STYLE_3D_VS2005, STYLE_3D_ROUNDED,
                 STYLE_3D_ROUNDED_SCROLL, STYLE_FLAT, STYLE_FLAT_SHARED_HORZ_SCROLL };
    enum Location { LOCATION_BOTTOM = 0, LOCATION_TOP };

    CMFCBaseTabCtrl() = default;

    void SetLocation(Location loc) { m_location = loc; if (::IsWindow(m_hWnd)) { OnTabLocationChanged(); OnTabLayoutChanged(); Invalidate(FALSE); } }
    Location GetLocation() const { return m_location; }
    void ModifyTabStyle(Style s) { m_style = s; if (::IsWindow(m_hWnd)) Invalidate(FALSE); }
    void EnableTabSwap(BOOL) {}
    void SetDrawFrame(BOOL) {}
    void SetScrollButtons() {}
    void SetActiveTabBoldFont(BOOL = TRUE) {}
    void SetActiveTabColor(COLORREF cr) { m_clrActiveTab = cr; if (::IsWindow(m_hWnd)) Invalidate(FALSE); }
    void SetTabBorderSize(int n) { m_tabBorderSize = n; if (::IsWindow(m_hWnd)) Invalidate(FALSE); }

    int  GetTabsNum() const { return static_cast<int>(m_tabs.size()); }
    int  GetActiveTab() const { return m_activeTab; }
    CWnd* GetTabWnd(int i) const { return (i >= 0 && i < GetTabsNum()) ? m_tabs[i].pWnd : nullptr; }
    BOOL IsTabVisible(int i) const { return (i >= 0 && i < GetTabsNum()) ? m_tabs[i].visible : FALSE; }
    void GetTabLabel(int i, CString& str) const { if (i >= 0 && i < GetTabsNum()) str = m_tabs[i].label.c_str(); }
    void SetTabLabel(int i, LPCWSTR label)
    {
        if (i >= 0 && i < GetTabsNum())
        {
            m_tabs[i].label = label ? label : L"";
            OnTabLabelChanged(i);
            if (::IsWindow(m_hWnd)) Invalidate(FALSE);
        }
    }

protected:
    struct TabInfo { CWnd* pWnd = nullptr; std::wstring label; bool visible = true; CRect rcTab; };
    std::vector<TabInfo> m_tabs;
    int m_activeTab = -1;
    Style m_style = STYLE_3D_VS2005;
    Location m_location = LOCATION_BOTTOM;
    COLORREF m_clrActiveTab = CLR_NONE;
    int m_tabBorderSize = 1;
    virtual void OnTabLabelChanged(int) {}
    virtual void OnTabLocationChanged() {}
    virtual void OnTabLayoutChanged() {}

public:
    CMFCButton m_btnScrollFirst, m_btnScrollLast, m_btnScrollLeft, m_btnScrollRight;
    BOOL m_bScroll = FALSE;
};
SHIM_IMPLEMENT_DYNAMIC_INLINE(CMFCBaseTabCtrl, RUNTIME_CLASS(CWnd))

class CMFCTabCtrl : public CMFCBaseTabCtrl
{
public:
    static const CRuntimeClass classCMFCTabCtrl;
    CRuntimeClass* GetRuntimeClass() const override;
    CMFCTabCtrl() = default;

    BOOL Create(Style style, const RECT& rect, CWnd* pParentWnd, UINT nID, int = 0, BOOL = FALSE)
    {
        m_style = style;
        INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_TAB_CLASSES };
        ::InitCommonControlsEx(&icc);
        DWORD tabStyle = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
            TCS_TABS | TCS_SINGLELINE | TCS_RAGGEDRIGHT;
        if (m_location == LOCATION_BOTTOM)
            tabStyle |= TCS_BOTTOM;
        if (!CWnd::CreateEx(0, WC_TABCONTROLW, nullptr, tabStyle, rect, pParentWnd, nID)) return FALSE;
        SendSelf(TCM_SETUNICODEFORMAT, TRUE, 0);
        SetFont(CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT))));
        UpdateNativePadding();
        RebuildNativeTabs();
        return TRUE;
    }

    int AddTab(CWnd* pTabWnd, LPCWSTR lpszTabLabel, UINT = static_cast<UINT>(-1), BOOL = TRUE)
    {
        TabInfo t; t.pWnd = pTabWnd; t.label = lpszTabLabel ? lpszTabLabel : L""; t.visible = true;
        m_tabs.push_back(std::move(t));
        if (m_activeTab < 0) m_activeTab = static_cast<int>(m_tabs.size()) - 1;
        RebuildNativeTabs();
        LayoutPanes();
        Invalidate(FALSE);
        return static_cast<int>(m_tabs.size()) - 1;
    }

    void SetActiveTab(int i)
    {
        ActivateTab(i, true, true);
    }

    bool ActivateAdjacentTab(bool forward, bool wrap = true)
    {
        if (m_visibleToLogical.empty()) return false;
        const int native = NativeIndexFromLogical(m_activeTab);
        if (native < 0) return false;

        int next = native + (forward ? 1 : -1);
        if (next < 0 || next >= static_cast<int>(m_visibleToLogical.size()))
        {
            if (!wrap) return false;
            next = forward ? 0 : static_cast<int>(m_visibleToLogical.size()) - 1;
        }

        return ActivateTab(m_visibleToLogical[static_cast<size_t>(next)], true, true);
    }

    BOOL PreTranslateMessage(MSG* pMsg) override
    {
        if (pMsg != nullptr && pMsg->hwnd == m_hWnd && IsKeyboardMessage(pMsg->message) && !IsPlainTabTraversal(*pMsg))
        {
            if (ForwardKeyboardMessageToActiveTab(*pMsg)) return TRUE;
        }

        return CMFCBaseTabCtrl::PreTranslateMessage(pMsg);
    }

    void ShowTab(int i, BOOL show)
    {
        if (i < 0 || i >= GetTabsNum()) return;
        m_tabs[i].visible = (show != FALSE);
        if (!show && m_activeTab == i)
        {
            m_activeTab = -1;
            for (int k = 0; k < GetTabsNum(); ++k) if (m_tabs[k].visible) { m_activeTab = k; break; }
        }
        else if (show && m_activeTab < 0) m_activeTab = i;
        RebuildNativeTabs();
        LayoutPanes();
        Invalidate(FALSE);
    }

    void OnSize(UINT, int, int) { Default(); LayoutPanes(); Invalidate(FALSE); }
    BOOL OnEraseBkgnd(CDC*) { return TRUE; }

    void OnLButtonDown(UINT, CPoint)
    {
        Default();
        RedirectFocusAwayFromTabControl();
    }

    void OnLButtonUp(UINT, CPoint)
    {
        Default();
        RedirectFocusAwayFromTabControl();
    }

    void OnSetFocus(CWnd*)
    {
        RedirectFocusAwayFromTabControl();
        Invalidate(FALSE);
    }

    void OnKillFocus(CWnd*)
    {
        Invalidate(FALSE);
    }

    void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
    {
        MSG msg{};
        msg.hwnd = m_hWnd;
        msg.message = WM_KEYDOWN;
        msg.wParam = nChar;
        msg.lParam = MAKELPARAM(nRepCnt, nFlags);
        if (!IsPlainTabTraversal(msg) && ForwardKeyboardMessageToActiveTab(msg)) return;
        Default();
    }

    void OnNativeSelChange(NMHDR*, LRESULT* pResult)
    {
        const int native = static_cast<int>(SendSelf(TCM_GETCURSEL));
        if (native >= 0 && native < static_cast<int>(m_visibleToLogical.size()))
        {
            ActivateTab(m_visibleToLogical[static_cast<size_t>(native)], true, false);
        }
        if (pResult) *pResult = 0;
    }

    void OnPaint()
    {
        CPaintDC paintDC(this);
        CMemDC memDC(paintDC, this);
        CDC& dc = memDC.GetDC();

        CRect rcClient; GetClientRect(&rcClient);
        COLORREF clrDark, clrBlack, clrHighlight, clrFace, clrDarkShadow, clrLight; CBrush* pbrFace; CBrush* pbrBlack;
        CMFCVisualManager::GetInstance()->GetTabFrameColors(this, clrDark, clrBlack, clrHighlight, clrFace, clrDarkShadow, clrLight, pbrFace, pbrBlack);

        const int tabH = TabStripHeight();
        const bool bottomTabs = (m_location == LOCATION_BOTTOM);
        const bool labelOnlyTabs = IsLabelOnlyTabControl();
        const bool dark = CMFCVisualManager::GetInstance()->IsDark() || IsDarkColor(clrFace) || IsDarkColor(clrHighlight);
        const COLORREF paneBg = (m_clrActiveTab != CLR_NONE) ? m_clrActiveTab : clrHighlight;
        const COLORREF controlBg = labelOnlyTabs ? afxGlobalData.clrBtnFace : paneBg;
        dc.FillSolidRect(rcClient, controlBg);

        CRect rcStrip = bottomTabs ?
            CRect(rcClient.left, std::max(rcClient.top, rcClient.bottom - tabH), rcClient.right, rcClient.bottom) :
            CRect(rcClient.left, rcClient.top, rcClient.right, std::min(rcClient.bottom, rcClient.top + tabH));
        const COLORREF stripBg = dark ? RGB(30, 30, 30) : (labelOnlyTabs ? afxGlobalData.clrBtnFace : clrFace);
        const COLORREF stripBorder = dark ? RGB(95, 95, 95) : clrDark;
        const COLORREF activeTabBg = dark ? RGB(245, 245, 245) : (labelOnlyTabs ? afxGlobalData.clrBtnFace : RGB(255, 255, 255));
        const COLORREF inactiveTabBg = dark ? RGB(31, 31, 31) : BlendColor(clrFace, RGB(0, 0, 0), 0.05);
        const COLORREF activeText = dark ? RGB(0, 0, 0) : afxGlobalData.clrWindowText;
        const COLORREF inactiveText = dark ? RGB(235, 235, 235) : afxGlobalData.clrWindowText;
        dc.FillSolidRect(rcStrip, stripBg);
        dc.FillSolidRect(rcStrip.left, bottomTabs ? rcStrip.bottom - 1 : rcStrip.top, rcStrip.Width(), 1, stripBorder);
        dc.FillSolidRect(rcStrip.left, bottomTabs ? rcStrip.top : rcStrip.bottom - 1, rcStrip.Width(), 1, stripBorder);

        CFont* pFont = GetFont();
        if (pFont == nullptr || pFont->GetSafeHandle() == nullptr)
        {
            pFont = CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
        }
        CGdiObject* pOld = dc.SelectObject(pFont);
        CFont boldFont;
        LOGFONTW lf{};
        CFont* pBoldFont = nullptr;
        if (pFont->GetLogFont(&lf) != 0)
        {
            lf.lfWeight = FW_BOLD;
            if (boldFont.CreateFontIndirect(&lf)) pBoldFont = &boldFont;
        }
        dc.SetBkMode(TRANSPARENT);

        struct TabPaintInfo
        {
            int logical = -1;
            CRect rect;
        };

        std::vector<TabPaintInfo> inactiveTabs;
        inactiveTabs.reserve(m_visibleToLogical.size());
        TabPaintInfo activeTab;
        bool hasActiveTab = false;

        for (int native = 0; native < static_cast<int>(m_visibleToLogical.size()); ++native)
        {
            const int logical = m_visibleToLogical[static_cast<size_t>(native)];
            if (logical < 0 || logical >= GetTabsNum()) continue;

            CRect rcTab;
            if (!GetNativeItemRect(native, rcTab)) continue;

            const int verticalInset = WdsDpiScale(labelOnlyTabs ? 2 : 7, m_hWnd);
            const int tabVisualHeight = labelOnlyTabs ?
                std::max(0, rcStrip.Height() - verticalInset) :
                std::min(WdsDpiScale(18, m_hWnd), std::max(0, rcStrip.Height() - WdsDpiScale(8, m_hWnd)));
            if (bottomTabs)
            {
                rcTab.bottom = rcStrip.bottom - verticalInset;
                rcTab.top = std::max(rcStrip.top, rcTab.bottom - tabVisualHeight);
            }
            else
            {
                rcTab.top = rcStrip.top + verticalInset;
                rcTab.bottom = std::min(rcTab.top + tabVisualHeight, rcStrip.bottom);
            }
            const int leftInset = WdsDpiScale(labelOnlyTabs ? 2 : 3, m_hWnd);
            const int overlap = WdsDpiScale(labelOnlyTabs ? 1 : 2, m_hWnd);
            rcTab.left = std::max(rcStrip.left + leftInset, rcTab.left - (native == 0 ? 0 : overlap));
            rcTab.right += WdsDpiScale(labelOnlyTabs ? 2 : 4, m_hWnd);
            m_tabs[logical].rcTab = rcTab;

            if (logical == m_activeTab)
            {
                activeTab.logical = logical;
                activeTab.rect = rcTab;
                hasActiveTab = true;
                continue;
            }

            inactiveTabs.push_back({ logical, rcTab });
        }

        auto drawTab = [&](const TabPaintInfo& tab, bool active)
        {
            if (tab.logical < 0 || tab.logical >= GetTabsNum()) return;
            CRect rcTab = tab.rect;
            DrawAngledTab(dc, rcTab, active, bottomTabs, active ? activeTabBg : inactiveTabBg, stripBorder);

            dc.SetTextColor(active ? activeText : inactiveText);
            CGdiObject* pTextOld = nullptr;
            if (active && pBoldFont != nullptr) pTextOld = dc.SelectObject(pBoldFont);
            CRect rcText = rcTab; rcText.DeflateRect(WdsDpiScale(5, m_hWnd), WdsDpiScale(1, m_hWnd), WdsDpiScale(5, m_hWnd), WdsDpiScale(1, m_hWnd));
            DrawTabText(dc, rcText, m_tabs[tab.logical].label);
            if (pTextOld != nullptr) dc.SelectObject(pTextOld);
        };

        for (const TabPaintInfo& tab : inactiveTabs)
        {
            drawTab(tab, false);
        }

        if (hasActiveTab)
        {
            drawTab(activeTab, true);
        }

        dc.SelectObject(pOld);
    }

    static const AFX_MSGMAP* __stdcall _GetBaseMessageMap() { return CMFCBaseTabCtrl::GetThisMessageMap(); }
    static const AFX_MSGMAP* GetThisMessageMap()
    {
        static const AFX_MSGMAP_ENTRY entries[] = {
            { WM_SIZE, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<void (CMFCTabCtrl::*)(UINT,int,int)>(&CMFCTabCtrl::OnSize)), &WdsThunk_Size, nullptr },
            { WM_ERASEBKGND, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<BOOL (CMFCTabCtrl::*)(CDC*)>(&CMFCTabCtrl::OnEraseBkgnd)), &WdsThunk_EraseBkgnd, nullptr },
            { WM_LBUTTONDOWN, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<void (CMFCTabCtrl::*)(UINT,CPoint)>(&CMFCTabCtrl::OnLButtonDown)), &WdsThunk_MouseBtn, nullptr },
            { WM_LBUTTONUP, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<void (CMFCTabCtrl::*)(UINT,CPoint)>(&CMFCTabCtrl::OnLButtonUp)), &WdsThunk_MouseBtn, nullptr },
            { WM_KEYDOWN, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<void (CMFCTabCtrl::*)(UINT,UINT,UINT)>(&CMFCTabCtrl::OnKeyDown)), &WdsThunk_Key, nullptr },
            { WM_SETFOCUS, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<void (CMFCTabCtrl::*)(CWnd*)>(&CMFCTabCtrl::OnSetFocus)), &WdsThunk_Focus, nullptr },
            { WM_KILLFOCUS, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<void (CMFCTabCtrl::*)(CWnd*)>(&CMFCTabCtrl::OnKillFocus)), &WdsThunk_Focus, nullptr },
            { WM_PAINT, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<void (CMFCTabCtrl::*)()>(&CMFCTabCtrl::OnPaint)), &WdsThunk_Void, nullptr },
            { WM_NOTIFY, TCN_SELCHANGE, WdsReflectID, WdsReflectID, reinterpret_cast<AFX_PMSG>(static_cast<void (CMFCTabCtrl::*)(NMHDR*,LRESULT*)>(&CMFCTabCtrl::OnNativeSelChange)), &WdsThunk_Notify, nullptr },
            { 0, 0, 0, 0, nullptr, nullptr, nullptr }
        };
        static const AFX_MSGMAP map = { &CMFCTabCtrl::_GetBaseMessageMap, entries };
        return &map;
    }
    const AFX_MSGMAP* GetMessageMap() const override { return GetThisMessageMap(); }

private:
    std::vector<int> m_visibleToLogical;

    static bool IsKeyboardMessage(UINT message)
    {
        return message == WM_KEYDOWN || message == WM_SYSKEYDOWN || message == WM_CHAR || message == WM_SYSCHAR;
    }

    static bool IsPlainTabTraversal(const MSG& msg)
    {
        if (msg.message != WM_KEYDOWN || msg.wParam != VK_TAB) return false;
        return (::GetKeyState(VK_CONTROL) & 0x8000) == 0 && (::GetKeyState(VK_MENU) & 0x8000) == 0;
    }

    static COLORREF BlendColor(COLORREF from, COLORREF to, double amount)
    {
        const auto ch = [amount](BYTE a, BYTE b) -> BYTE
        {
            return static_cast<BYTE>(std::clamp<int>(static_cast<int>(a + (b - a) * amount), 0, 255));
        };
        return RGB(ch(GetRValue(from), GetRValue(to)), ch(GetGValue(from), GetGValue(to)), ch(GetBValue(from), GetBValue(to)));
    }

    static bool IsDarkColor(COLORREF color)
    {
        const int luminance = GetRValue(color) * 299 + GetGValue(color) * 587 + GetBValue(color) * 114;
        return luminance < 128000;
    }

    void OnTabLabelChanged(int i) override
    {
        const int native = NativeIndexFromLogical(i);
        if (native >= 0)
        {
            TCITEMW item{};
            item.mask = TCIF_TEXT | TCIF_PARAM;
            item.pszText = const_cast<LPWSTR>(m_tabs[i].label.c_str());
            item.lParam = static_cast<LPARAM>(i);
            SendSelf(TCM_SETITEMW, static_cast<WPARAM>(native), reinterpret_cast<LPARAM>(&item));
        }
    }

    void OnTabLayoutChanged() override
    {
        LayoutPanes();
    }

    void OnTabLocationChanged() override
    {
        if (!::IsWindow(m_hWnd)) return;

        ModifyStyle(m_location == LOCATION_BOTTOM ? 0 : TCS_BOTTOM,
            m_location == LOCATION_BOTTOM ? TCS_BOTTOM : 0, SWP_FRAMECHANGED);
        LayoutPanes();
    }

    int NativeIndexFromLogical(int logical) const
    {
        for (int i = 0; i < static_cast<int>(m_visibleToLogical.size()); ++i)
        {
            if (m_visibleToLogical[static_cast<size_t>(i)] == logical) return i;
        }
        return -1;
    }

    void RebuildNativeTabs()
    {
        if (!::IsWindow(m_hWnd)) return;

        UpdateNativePadding();
        SendSelf(TCM_DELETEALLITEMS);
        m_visibleToLogical.clear();
        m_visibleToLogical.reserve(m_tabs.size());

        int native = 0;
        for (int logical = 0; logical < GetTabsNum(); ++logical)
        {
            if (!m_tabs[logical].visible)
            {
                m_tabs[logical].rcTab.SetRectEmpty();
                continue;
            }

            TCITEMW item{};
            item.mask = TCIF_TEXT | TCIF_PARAM;
            item.pszText = const_cast<LPWSTR>(m_tabs[logical].label.c_str());
            item.lParam = static_cast<LPARAM>(logical);
            SendSelf(TCM_INSERTITEMW, static_cast<WPARAM>(native), reinterpret_cast<LPARAM>(&item));
            m_visibleToLogical.push_back(logical);
            ++native;
        }

        SyncNativeSelection();
    }

    void UpdateNativePadding()
    {
        if (!::IsWindow(m_hWnd)) return;

        const bool labelOnlyTabs = IsLabelOnlyTabControl();
        SendSelf(TCM_SETPADDING, 0, MAKELPARAM(
            WdsDpiScale(labelOnlyTabs ? 8 : 12, m_hWnd),
            WdsDpiScale(labelOnlyTabs ? 2 : 3, m_hWnd)));
    }

    void SyncNativeSelection()
    {
        if (!::IsWindow(m_hWnd)) return;
        const int native = NativeIndexFromLogical(m_activeTab);
        if (native >= 0) SendSelf(TCM_SETCURSEL, static_cast<WPARAM>(native));
    }

    bool ActivateTab(int i, bool notifyParent, bool syncNative)
    {
        if (i < 0 || i >= GetTabsNum() || !m_tabs[i].visible) return false;
        m_activeTab = i;
        if (syncNative) SyncNativeSelection();
        LayoutPanes();
        if (notifyParent)
        {
            if (CWnd* p = GetParent())
            {
                ::SendMessageW(p->m_hWnd, AFX_WM_CHANGING_ACTIVE_TAB, static_cast<WPARAM>(i), 0);
                ::SendMessageW(p->m_hWnd, AFX_WM_CHANGE_ACTIVE_TAB, static_cast<WPARAM>(i), 0);
            }
        }
        Invalidate(FALSE);
        return true;
    }

    bool ForwardKeyboardMessageToActiveTab(const MSG& msg)
    {
        CWnd* p = GetTabWnd(m_activeTab);
        if (p == nullptr || !::IsWindow(p->m_hWnd) || !::IsWindowVisible(p->m_hWnd) || !::IsWindowEnabled(p->m_hWnd)) return false;

        p->SetFocus();
        HWND target = ::GetFocus();
        if (target == nullptr || (target != p->m_hWnd && !::IsChild(p->m_hWnd, target))) target = p->m_hWnd;
        ::SendMessageW(target, msg.message, msg.wParam, msg.lParam);
        return true;
    }

    bool FocusActiveTabWindow()
    {
        CWnd* p = GetTabWnd(m_activeTab);
        if (p == nullptr || !::IsWindow(p->m_hWnd) || !::IsWindowVisible(p->m_hWnd) || !::IsWindowEnabled(p->m_hWnd)) return false;

        p->SetFocus();
        const HWND focus = ::GetFocus();
        return focus == p->m_hWnd || (focus != nullptr && ::IsChild(p->m_hWnd, focus));
    }

    bool RedirectFocusAwayFromTabControl()
    {
        if (FocusActiveTabWindow()) return true;

        HWND parent = ::GetParent(m_hWnd);
        if (parent != nullptr && ::IsWindow(parent) && ::IsWindowVisible(parent) && ::IsWindowEnabled(parent))
        {
            ::SetFocus(parent);
            return ::GetFocus() != m_hWnd;
        }
        return false;
    }

    bool GetNativeItemRect(int native, CRect& rc) const
    {
        RECT r{};
        if (SendSelf(TCM_GETITEMRECT, static_cast<WPARAM>(native), reinterpret_cast<LPARAM>(&r)) == 0) return false;
        rc = r;
        return true;
    }

    bool IsLabelOnlyTabControl() const
    {
        return !std::any_of(m_tabs.begin(), m_tabs.end(), [](const TabInfo& tab)
        {
            return tab.visible && tab.pWnd != nullptr;
        });
    }

    int TabStripHeight() const
    {
        int height = 0;
        for (int native = 0; native < static_cast<int>(m_visibleToLogical.size()); ++native)
        {
            CRect rcTab;
            if (GetNativeItemRect(native, rcTab)) height = std::max(height, static_cast<int>(rcTab.Height()));
        }
        return std::max(WdsDpiScale(28, m_hWnd), height + WdsDpiScale(10, m_hWnd));
    }

    void DrawAngledTab(CDC& dc, const CRect& rc, bool active, bool bottomTabs, COLORREF fill, COLORREF border)
    {
        const int slant = std::min(WdsDpiScale(8, m_hWnd), std::max(WdsDpiScale(4, m_hWnd), rc.Width() / 5));
        POINT pts[4] = {};
        if (bottomTabs)
        {
            pts[0] = { rc.left, rc.top };
            pts[1] = { rc.left + slant, rc.bottom - 1 };
            pts[2] = { rc.right - slant, rc.bottom - 1 };
            pts[3] = { rc.right, rc.top };
        }
        else
        {
            pts[0] = { rc.left, rc.bottom - 1 };
            pts[1] = { rc.left + slant, rc.top };
            pts[2] = { rc.right - slant, rc.top };
            pts[3] = { rc.right, rc.bottom - 1 };
        }

        CBrush brush(fill);
        CBrush* oldBrush = dc.SelectObject(&brush);
        CGdiObject* oldFillPen = dc.SelectStockObject(NULL_PEN);
        dc.Polygon(pts, 4);
        dc.SelectObject(oldFillPen);
        dc.SelectObject(oldBrush);

        CPen pen(PS_SOLID, 1, border);
        CPen* oldPen = dc.SelectObject(&pen);
        if (active)
        {
            dc.MoveTo(pts[0]);
            dc.LineTo(pts[1]);
            dc.LineTo(pts[2]);
            dc.LineTo(pts[3]);
        }
        else
        {
            POINT outline[5] = { pts[0], pts[1], pts[2], pts[3], pts[0] };
            dc.Polyline(outline, 5);
        }
        dc.SelectObject(oldPen);
    }

    static void DrawTabText(CDC& dc, const CRect& rc, const std::wstring& text)
    {
        const int oldBk = dc.SetBkMode(TRANSPARENT);
        const CSize ext = dc.GetTextExtent(text.c_str(), static_cast<int>(text.size()));
        TEXTMETRICW tm{};
        dc.GetTextMetrics(&tm);

        const int x = static_cast<int>(rc.left) + std::max<int>(0, (static_cast<int>(rc.Width()) - ext.cx) / 2);
        const int y = static_cast<int>(rc.top) + std::max<int>(0, (static_cast<int>(rc.Height()) - tm.tmHeight) / 2);
        dc.ExtTextOut(x, y, ETO_CLIPPED, &rc, text.c_str(), static_cast<UINT>(text.size()), nullptr);
        dc.SetBkMode(oldBk);
    }

    void LayoutPanes()
    {
        if (!::IsWindow(m_hWnd)) return;
        CRect rc; GetClientRect(&rc);
        const int tabH = TabStripHeight();
        CRect rcPane = (m_location == LOCATION_BOTTOM) ?
            CRect(rc.left, rc.top, rc.right, std::max(rc.top, rc.bottom - tabH)) :
            CRect(rc.left, rc.top + tabH, rc.right, rc.bottom);
        for (int i = 0; i < GetTabsNum(); ++i)
        {
            CWnd* p = m_tabs[i].pWnd;
            if (p == nullptr || !::IsWindow(p->m_hWnd)) continue;
            if (i == m_activeTab) { p->MoveWindow(rcPane); p->ShowWindow(SW_SHOW); }
            else p->ShowWindow(SW_HIDE);
        }
    }
};
SHIM_IMPLEMENT_DYNAMIC_INLINE(CMFCTabCtrl, RUNTIME_CLASS(CMFCBaseTabCtrl))

// -----------------------------------------------------------------------------
//  CMFCPropertyPage / CMFCPropertySheet
// -----------------------------------------------------------------------------
inline std::wstring WdsDialogTemplateCaption(UINT nIDTemplate)
{
    const HRSRC hResource = ::FindResourceW(AfxGetResourceHandle(), MAKEINTRESOURCEW(nIDTemplate), RT_DIALOG);
    if (hResource == nullptr) return {};

    const HGLOBAL hGlobal = ::LoadResource(AfxGetResourceHandle(), hResource);
    auto* p = hGlobal != nullptr ? static_cast<const WORD*>(::LockResource(hGlobal)) : nullptr;
    if (p == nullptr) return {};

    p += (p[0] == 1 && p[1] == 0xFFFF) ? 13 : 9; // DLGTEMPLATE(EX) header through cy.
    auto skipResourceOrString = [](const WORD* q)
    {
        if (*q == 0) return q + 1;
        if (*q == 0xFFFF) return q + 2;
        while (*q++ != 0) {}
        return q;
    };

    p = skipResourceOrString(p); // menu
    p = skipResourceOrString(p); // window class
    if (*p == 0 || *p == 0xFFFF) return {};

    const WORD* start = p;
    while (*p++ != 0) {}
    return { reinterpret_cast<const wchar_t*>(start), static_cast<size_t>((p - 1) - start) };
}

class CMFCPropertySheet;

class CMFCPropertyPage : public CDialog
{
public:
    static const CRuntimeClass classCMFCPropertyPage;
    CRuntimeClass* GetRuntimeClass() const override;

    BOOL m_bModified = FALSE;

    CMFCPropertyPage() = default;
    explicit CMFCPropertyPage(UINT nIDTemplate, UINT = 0) { m_nIDTemplate = nIDTemplate; }

    void SetModified(BOOL bChanged = TRUE);
    virtual BOOL OnApply() { return TRUE; }
    virtual BOOL OnSetActive() { return TRUE; }
    virtual BOOL OnKillActive() { return UpdateData(TRUE); }
    BOOL OnInitDialog() override { return CDialog::OnInitDialog(); }
    void OnOK() override {}        // a page does not end its own dialog
    void OnCancel() override {}

    static const AFX_MSGMAP* __stdcall _GetBaseMessageMap() { return CDialog::GetThisMessageMap(); }
    static const AFX_MSGMAP* GetThisMessageMap()
    {
        static const AFX_MSGMAP_ENTRY entries[] = { { 0, 0, 0, 0, nullptr, nullptr, nullptr } };
        static const AFX_MSGMAP map = { &CMFCPropertyPage::_GetBaseMessageMap, entries };
        return &map;
    }
    const AFX_MSGMAP* GetMessageMap() const override { return GetThisMessageMap(); }
};
SHIM_IMPLEMENT_DYNAMIC_INLINE(CMFCPropertyPage, RUNTIME_CLASS(CDialog))

class CMFCPropertySheet : public CWnd
{
public:
    static const CRuntimeClass classCMFCPropertySheet;
    CRuntimeClass* GetRuntimeClass() const override;

    enum PropSheetLook { PropSheetLook_Tabs, PropSheetLook_OutlookBar, PropSheetLook_Tree,
                         PropSheetLook_OneNoteTabs, PropSheetLook_List, PropSheetLook_Buttons };
    PropSheetLook m_look = PropSheetLook_Tabs;

    CMFCPropertySheet() = default;
    explicit CMFCPropertySheet(LPCWSTR caption, CWnd* pParent = nullptr, UINT = 0) : m_pParent(pParent) { if (caption) m_caption = caption; }
    explicit CMFCPropertySheet(UINT nID, CWnd* pParent = nullptr, UINT = 0) : m_pParent(pParent) { CString s; s.LoadString(nID); m_caption = s.GetString(); }

    void AddPage(CMFCPropertyPage* p) { m_pages.push_back(p); }
    int  GetPageCount() const { return static_cast<int>(m_pages.size()); }
    int  GetActiveIndex() const { return m_tab.GetActiveTab(); }
    CMFCPropertyPage* GetActivePage() const { const int i = m_tab.GetActiveTab(); return (i >= 0 && i < GetPageCount()) ? m_pages[i] : nullptr; }
    BOOL SetActivePage(int i)
    {
        if (i < 0 || i >= GetPageCount()) return FALSE;
        if (m_tab.GetSafeHwnd() == nullptr)
        {
            m_pendingActivePage = i;
            return TRUE;
        }

        m_tab.SetActiveTab(i);
        ShowPage(i);
        return TRUE;
    }
    CMFCTabCtrl& GetTab() { return m_tab; }
    void EnableApply(BOOL b) { if (m_btnApply.GetSafeHwnd()) m_btnApply.EnableWindow(b); }

    virtual INT_PTR DoModal();
    virtual BOOL OnInitDialog();
    BOOL OnEraseBkgnd(CDC* pDC) { CRect rc; GetClientRect(&rc); pDC->FillSolidRect(rc, afxGlobalData.clrBtnFace); return TRUE; }
    HBRUSH OnCtlColor(CDC*, CWnd*, UINT) { return reinterpret_cast<HBRUSH>(Default()); }
    LRESULT OnTabChanged(WPARAM w, LPARAM) { ShowPage(static_cast<int>(w)); return 0; }
    void EndDialog(int nResult) { m_modalResult = nResult; }

    static const AFX_MSGMAP* __stdcall _GetBaseMessageMap() { return CWnd::GetThisMessageMap(); }
    static const AFX_MSGMAP* GetThisMessageMap()
    {
        static const AFX_MSGMAP_ENTRY entries[] = {
            { 0, 0, 0, 0, reinterpret_cast<AFX_PMSG>(static_cast<LRESULT (CMFCPropertySheet::*)(WPARAM,LPARAM)>(&CMFCPropertySheet::OnTabChanged)), &WdsThunk_Message, &AFX_WM_CHANGING_ACTIVE_TAB },
            { 0, 0, 0, 0, nullptr, nullptr, nullptr }
        };
        static const AFX_MSGMAP map = { &CMFCPropertySheet::_GetBaseMessageMap, entries };
        return &map;
    }
    const AFX_MSGMAP* GetMessageMap() const override { return GetThisMessageMap(); }

    BOOL OnCommand(WPARAM wParam, LPARAM lParam) override
    {
        const UINT id = LOWORD(wParam);
        if (id == IDOK)
        {
            for (auto* p : m_pages) if (p->GetSafeHwnd()) p->OnOK();
            EndDialog(IDOK);
            return TRUE;
        }
        if (id == IDCANCEL) { EndDialog(IDCANCEL); return TRUE; }
        if (id == ID_APPLY_NOW)
        {
            for (auto* p : m_pages) if (p->GetSafeHwnd() && p->m_bModified) { p->OnOK(); p->m_bModified = FALSE; }
            EnableApply(FALSE);
            return TRUE;
        }
        return CWnd::OnCommand(wParam, lParam);
    }

protected:
    BOOL EnsurePageCreated(int i)
    {
        if (i < 0 || i >= GetPageCount()) return FALSE;

        auto* page = m_pages[i];
        if (page->GetSafeHwnd() != nullptr) return TRUE;

        page->m_pParentWnd = this;
        const HWND h = ::CreateDialogParamW(AfxGetResourceHandle(), MAKEINTRESOURCEW(page->m_nIDTemplate),
            m_hWnd, AfxDlgProc, reinterpret_cast<LPARAM>(static_cast<CWnd*>(page)));
        if (h == nullptr) return FALSE;

        ::SetWindowLongPtrW(h, GWL_STYLE, (::GetWindowLongPtrW(h, GWL_STYLE) | WS_CHILD) &
            ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_DISABLED));

        if (!m_pageRect.IsRectEmpty())
        {
            page->SetWindowPos(nullptr, m_pageRect.left, m_pageRect.top,
                m_pageRect.Width(), m_pageRect.Height(), SWP_NOZORDER | SWP_NOACTIVATE);
        }

        page->ShowWindow(SW_HIDE);
        return TRUE;
    }

    void ShowPage(int active)
    {
        if (!EnsurePageCreated(active)) return;

        for (int i = 0; i < GetPageCount(); ++i)
            if (m_pages[i]->GetSafeHwnd())
            {
                const BOOL isActive = (i == active);
                m_pages[i]->EnableWindow(isActive);
                m_pages[i]->ShowWindow(isActive ? SW_SHOW : SW_HIDE);
            }

        if (m_currentPage != active)
        {
            m_currentPage = active;
            m_pages[active]->OnSetActive();
        }
    }

    std::vector<CMFCPropertyPage*> m_pages;
    CMFCTabCtrl m_tab;
    CButton m_btnOK, m_btnCancel, m_btnApply;
    CRect m_pageRect;
    std::wstring m_caption;
    CWnd* m_pParent = nullptr;
    int m_currentPage = -1;
    int m_pendingActivePage = 0;
    int m_modalResult = -1;
};
SHIM_IMPLEMENT_DYNAMIC_INLINE(CMFCPropertySheet, RUNTIME_CLASS(CWnd))

inline void CMFCPropertyPage::SetModified(BOOL bChanged)
{
    m_bModified = bChanged;
    if (auto* s = DYNAMIC_DOWNCAST(CMFCPropertySheet, GetParent())) s->EnableApply(bChanged);
}

inline BOOL CMFCPropertySheet::OnInitDialog()
{
    const int margin = WdsDpiScale(10, m_hWnd);
    const int tabH = WdsDpiScale(26, m_hWnd);
    const int btnW = WdsDpiScale(86, m_hWnd);
    const int btnH = WdsDpiScale(26, m_hWnd);
    const int btnGap = WdsDpiScale(8, m_hWnd);
    const int gap = WdsDpiScale(8, m_hWnd);

    // Read tab labels from the dialog templates, then create only one page for
    // initial sizing.  Initializing every property page here makes the settings
    // dialog feel sluggish compared with MFC's lazy property-page creation.
    int maxW = 100, maxH = 100;
    std::vector<std::wstring> captions;
    captions.reserve(m_pages.size());
    for (auto* page : m_pages)
    {
        captions.push_back(WdsDialogTemplateCaption(page->m_nIDTemplate));
    }

    for (int i = 0; i < GetPageCount(); ++i)
    {
        if (!EnsurePageCreated(i)) continue;

        CRect rcPage;
        m_pages[i]->GetWindowRect(&rcPage);
        maxW = std::max<int>(maxW, rcPage.Width());
        maxH = std::max<int>(maxH, rcPage.Height());
        break;
    }

    const int clientW = margin + maxW + margin;
    const int clientH = margin + tabH + gap + maxH + gap + btnH + margin;

    // Resize the sheet to fit
    RECT rcWin{ 0, 0, clientW, clientH };
    ::AdjustWindowRectEx(&rcWin, static_cast<DWORD>(GetStyle()), FALSE, static_cast<DWORD>(GetExStyle()));
    SetWindowPos(nullptr, 0, 0, rcWin.right - rcWin.left, rcWin.bottom - rcWin.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    // Tab strip (label-only)
    m_tab.SetLocation(CMFCBaseTabCtrl::LOCATION_TOP);
    m_tab.Create(CMFCBaseTabCtrl::STYLE_3D_VS2005, CRect(margin, margin, margin + maxW, margin + tabH), this, 0xCAFE);
    for (auto& cap : captions) m_tab.AddTab(nullptr, cap.c_str());

    // Position pages
    const int pageY = margin + tabH + gap;
    m_pageRect = CRect(margin, pageY, margin + maxW, pageY + maxH);
    for (auto* page : m_pages)
        if (page->GetSafeHwnd()) page->SetWindowPos(nullptr, m_pageRect.left, m_pageRect.top,
            m_pageRect.Width(), m_pageRect.Height(), SWP_NOZORDER | SWP_NOACTIVATE);

    // Buttons (bottom-right: OK, Cancel, Apply)
    const int btnY = pageY + maxH + gap;
    int bx = margin + maxW - btnW;
    m_btnApply.CreateEx(0, WC_BUTTONW, L"Apply", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, CRect(bx, btnY, bx + btnW, btnY + btnH), this, ID_APPLY_NOW);
    bx -= btnW + btnGap;
    m_btnCancel.CreateEx(0, WC_BUTTONW, L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, CRect(bx, btnY, bx + btnW, btnY + btnH), this, IDCANCEL);
    bx -= btnW + btnGap;
    m_btnOK.CreateEx(0, WC_BUTTONW, L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, CRect(bx, btnY, bx + btnW, btnY + btnH), this, IDOK);

    m_btnOK.SetFont(CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT))));
    m_btnCancel.SetFont(CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT))));
    m_btnApply.SetFont(CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT))));

    if (GetPageCount() > 0) SetActivePage(std::clamp(m_pendingActivePage, 0, GetPageCount() - 1));
    EnableApply(FALSE);
    return TRUE;
}

inline INT_PTR CMFCPropertySheet::DoModal()
{
    const HWND hOwner = m_pParent ? m_pParent->m_hWnd : (AfxGetMainWnd() ? AfxGetMainWnd()->m_hWnd : nullptr);
    const LPCWSTR cls = AfxRegisterWndClass(0, ::LoadCursorW(nullptr, IDC_ARROW), reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1));
    if (!CWnd::CreateEx(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, cls, m_caption.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, hOwner, nullptr))
        return -1;

    OnInitDialog();
    CenterWindow(CWnd::FromHandle(hOwner));
    ShowWindow(SW_SHOW);
    UpdateWindow();
    if (hOwner) ::EnableWindow(hOwner, FALSE);

    m_modalResult = -1;
    MSG msg;
    while (m_modalResult == -1 && ::GetMessageW(&msg, nullptr, 0, 0))
    {
        if (PreTranslateMessage(&msg)) continue;
        if (!::IsDialogMessageW(m_hWnd, &msg)) { ::TranslateMessage(&msg); ::DispatchMessageW(&msg); }
    }

    if (hOwner) { ::EnableWindow(hOwner, TRUE); ::SetForegroundWindow(hOwner); }
    DestroyWindow();
    return m_modalResult;
}

// -----------------------------------------------------------------------------
//  Standard MFC/AFX resource ids and assorted MFC constants
// -----------------------------------------------------------------------------
#ifndef AFX_IDS_APP_TITLE
inline constexpr UINT AFX_IDS_APP_TITLE = 0xE000;
#endif
#ifndef ID_SEPARATOR
inline constexpr UINT ID_SEPARATOR = 0;
#endif
#ifndef ID_APP_ABOUT
inline constexpr UINT ID_APP_ABOUT = 0xE140;
inline constexpr UINT ID_APP_EXIT  = 0xE141;
#endif
#ifndef ID_VIEW_TOOLBAR
inline constexpr UINT ID_VIEW_TOOLBAR    = 0xE800;
inline constexpr UINT ID_VIEW_STATUS_BAR = 0xE801;
#endif
#ifndef ID_FILE_NEW
inline constexpr UINT ID_FILE_NEW  = 0xE100;
inline constexpr UINT ID_FILE_OPEN = 0xE101;
inline constexpr UINT ID_FILE_SAVE = 0xE103;
#endif
#ifndef ID_EDIT_COPY
inline constexpr UINT ID_EDIT_COPY       = 0xE122;
inline constexpr UINT ID_EDIT_CUT        = 0xE123;
inline constexpr UINT ID_EDIT_PASTE      = 0xE125;
inline constexpr UINT ID_EDIT_SELECT_ALL = 0xE131;
#endif
#ifndef ID_HELP
inline constexpr UINT ID_HELP = 0x0009;
#endif
#ifndef ID_WIZBACK
inline constexpr UINT ID_WIZBACK   = 0x3023;
inline constexpr UINT ID_WIZNEXT   = 0x3024;
inline constexpr UINT ID_WIZFINISH = 0x3025;
#endif
#ifndef AFX_IDP_NO_ERROR_AVAILABLE
inline constexpr UINT AFX_IDP_NO_ERROR_AVAILABLE = 0xF001;
#endif
#ifndef WM_KICKIDLE
inline constexpr UINT WM_KICKIDLE = 0x036A;
#endif

//
// End of WinDirStatShim.h
//
