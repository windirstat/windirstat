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

// -----------------------------------------------------------------------------
//  Build and DPI helpers
// -----------------------------------------------------------------------------

inline constexpr bool IsDebugBuild = _ITERATOR_DEBUG_LEVEL != 0;

inline int GetWindowDpi(const HWND window = nullptr) noexcept
{
    // GetDpiForWindow is available on Windows 10 1607 and later. Resolve it at
    // runtime so the Win7 build remains loadable, while avoiding a GetDC pair
    // for every scaled coordinate on current Windows releases.
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    static const auto getDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (window != nullptr && getDpiForWindow != nullptr)
    {
        if (const UINT dpi = getDpiForWindow(window); dpi != 0)
            return static_cast<int>(dpi);
    }

    const HDC hdc = GetDC(window);
    const int dpi = hdc != nullptr ? GetDeviceCaps(hdc, LOGPIXELSX) : 96;
    if (hdc != nullptr) ReleaseDC(window, hdc);
    return dpi > 0 ? dpi : USER_DEFAULT_SCREEN_DPI;
}

inline int g_fontSizePercent = 100;
inline int g_toolBarSizePercent = 100;

inline int GetFontSizePercent() noexcept
{
    return g_fontSizePercent;
}

inline void SetFontSizePercent(const int percent) noexcept
{
    g_fontSizePercent = std::clamp(percent, 100, 200);
}

inline int GetToolBarSizePercent() noexcept
{
    return g_toolBarSizePercent;
}

inline void SetToolBarSizePercent(const int percent) noexcept
{
    g_toolBarSizePercent = std::clamp(percent, 100, 200);
}

int ResolveTextScalePercent(int configuredPercent) noexcept;

inline int ScaleForScreenDpi(const int value, const HWND window = nullptr) noexcept
{
    return MulDiv(value, GetWindowDpi(window), USER_DEFAULT_SCREEN_DPI);
}

inline int UnscaleForScreenDpi(const int value, const HWND window = nullptr) noexcept
{
    return MulDiv(value, USER_DEFAULT_SCREEN_DPI, GetWindowDpi(window));
}

inline int ScaleForToolBarDpi(const int value, const HWND window = nullptr) noexcept
{
    return MulDiv(value, GetWindowDpi(window) * GetToolBarSizePercent(), USER_DEFAULT_SCREEN_DPI * 100);
}

inline int ScaleForDpi(const int value, const HWND window = nullptr) noexcept
{
    return MulDiv(value, GetWindowDpi(window) * GetFontSizePercent(), USER_DEFAULT_SCREEN_DPI * 100);
}

inline int UnscaleForDpi(const int value, const HWND window = nullptr) noexcept
{
    return MulDiv(value, USER_DEFAULT_SCREEN_DPI * 100, GetWindowDpi(window) * GetFontSizePercent());
}

HFONT GetAppFont(HWND window = nullptr);
void ApplyAppFont(HWND window, int oldPercent = 0);
void ApplyMenuFont(HWND window);
void InitializeDialogFontAndSize(HWND dialog);

inline bool IsKeyDown(const int virtualKey) noexcept
{
    return (GetKeyState(virtualKey) & 0x8000) != 0;
}

// Forward declarations of helpers needed early.
class CWnd;
HINSTANCE GetAppInstance();
CWnd* GetMainWindow();
HWND GetMainWindowHandle() noexcept;

// Application command constants referenced by the shim itself (full set near EOF).
inline constexpr UINT ID_APPLY_NOW = 0x3021;

// -----------------------------------------------------------------------------
//  Resource strings
// -----------------------------------------------------------------------------
inline std::wstring LoadResourceString(const UINT id)
{
    const wchar_t* resource = nullptr;
    const int length = LoadStringW(GetAppInstance(), id, reinterpret_cast<LPWSTR>(&resource), 0);
    return resource != nullptr && length >= 0
        ? std::wstring(resource, static_cast<std::size_t>(length))
        : std::wstring();
}

inline std::wstring LoadResourceString(const HINSTANCE instance, const UINT id, const WORD language)
{
    const HRSRC resource = FindResourceExW(instance, RT_STRING,
        MAKEINTRESOURCEW((id >> 4) + 1), language);
    const DWORD resourceSize = resource != nullptr ? SizeofResource(instance, resource) : 0;
    const HGLOBAL loaded = resource != nullptr ? LoadResource(instance, resource) : nullptr;
    const auto* current = loaded != nullptr ? static_cast<const WORD*>(LockResource(loaded)) : nullptr;
    const auto* end = current != nullptr ? current + resourceSize / sizeof(WORD) : nullptr;

    for (UINT index = 0; current != nullptr && index <= (id & 0xF); ++index)
    {
        if (current >= end) return {};

        const WORD length = *current++;
        if (static_cast<std::size_t>(end - current) < length) return {};
        if (index == (id & 0xF))
            return std::wstring(reinterpret_cast<const wchar_t*>(current), length);
        current += length;
    }
    return {};
}

inline std::wstring LoadResourceString(const UINT id, const WORD language)
{
    return LoadResourceString(GetAppInstance(), id, language);
}

// -----------------------------------------------------------------------------
//  Geometry: CSize / CPoint / CRect
// -----------------------------------------------------------------------------
class CSize final : public SIZE
{
public:
    CSize() noexcept { cx = cy = 0; }
    CSize(const int x, const int y) noexcept { cx = x; cy = y; }
    CSize(const SIZE s) noexcept { cx = s.cx; cy = s.cy; }
    bool operator==(const CSize& s) const noexcept { return cx == s.cx && cy == s.cy; }
    bool operator==(const SIZE& s) const noexcept { return cx == s.cx && cy == s.cy; }
    CSize operator-(const SIZE s) const noexcept { return { cx - s.cx, cy - s.cy }; }
};

class CPoint final : public POINT
{
public:
    CPoint() noexcept { x = y = 0; }
    CPoint(const int xx, const int yy) noexcept { x = xx; y = yy; }
    CPoint(const POINT p) noexcept { x = p.x; y = p.y; }
    CPoint(const SIZE s) noexcept { x = s.cx; y = s.cy; }
    void Offset(const int dx, const int dy) noexcept { x += dx; y += dy; }
    bool operator==(const CPoint& p) const noexcept { return x == p.x && y == p.y; }
    bool operator==(const POINT& p) const noexcept { return x == p.x && y == p.y; }
    CPoint operator+(const SIZE s) const noexcept { return { x + s.cx, y + s.cy }; }
    CPoint operator+(const POINT p) const noexcept { return { x + p.x, y + p.y }; }
    CSize  operator-(const POINT p) const noexcept { return { x - p.x, y - p.y }; }
    CPoint operator-(const SIZE s) const noexcept { return { x - s.cx, y - s.cy }; }
};

class CRect final : public RECT
{
public:
    CRect() noexcept { left = top = right = bottom = 0; }
    CRect(const int l, const int t, const int r, const int b) noexcept { left = l; top = t; right = r; bottom = b; }
    CRect(const RECT& r) noexcept { left = r.left; top = r.top; right = r.right; bottom = r.bottom; }
    explicit CRect(const HWND window) noexcept : CRect() { if (!::GetWindowRect(window, this)) Clear(); }
    CRect(const POINT topLeft, const POINT bottomRight) noexcept { left = topLeft.x; top = topLeft.y; right = bottomRight.x; bottom = bottomRight.y; }

    int Width() const noexcept { return static_cast<int>(std::clamp<int64_t>(static_cast<int64_t>(right) - left, INT_MIN, INT_MAX)); }
    int Height() const noexcept { return static_cast<int>(std::clamp<int64_t>(static_cast<int64_t>(bottom) - top, INT_MIN, INT_MAX)); }
    CSize Size() const noexcept { return { Width(), Height() }; }
    CPoint TopLeft() const noexcept { return { left, top }; }
    CPoint Center() const noexcept { return { static_cast<int>((static_cast<int64_t>(left) + right) / 2), static_cast<int>((static_cast<int64_t>(top) + bottom) / 2) }; }
    bool IsEmpty() const noexcept { return left >= right || top >= bottom; }

    operator LPRECT() noexcept { return this; }
    operator LPCRECT() const noexcept { return this; }

    void SetBounds(const int l, const int t, const int r, const int b) noexcept { left = l; top = t; right = r; bottom = b; }
    void Clear() noexcept { left = top = right = bottom = 0; }
    void Offset(const int dx, const int dy) noexcept { OffsetRect(this, dx, dy); }
    void Offset(const POINT p) noexcept { OffsetRect(this, p.x, p.y); }
    void Inflate(const int dx, const int dy) noexcept { InflateRect(this, dx, dy); }
    void Deflate(const int dx, const int dy) noexcept { InflateRect(this, -dx, -dy); }
    void Deflate(const int l, const int t, const int r, const int b) noexcept { left += l; top += t; right -= r; bottom -= b; }
    void Deflate(const SIZE s) noexcept { InflateRect(this, -s.cx, -s.cy); }
    void Normalize() noexcept { if (left > right) std::swap(left, right); if (top > bottom) std::swap(top, bottom); }
    bool Contains(const POINT p) const noexcept { return PtInRect(this, p); }
    bool Intersect(const RECT& a, const RECT& b) noexcept { return IntersectRect(this, &a, &b); }
    bool Union(const RECT& a, const RECT& b) noexcept { return UnionRect(this, &a, &b); }

    bool operator==(const CRect& r) const noexcept { return EqualRect(this, &r) != 0; }
    bool operator==(const RECT& r) const noexcept { return EqualRect(this, &r) != 0; }
    CRect& operator=(const RECT& r) noexcept { left = r.left; top = r.top; right = r.right; bottom = r.bottom; return *this; }
    CRect operator+(const POINT p) const noexcept { CRect r(*this); OffsetRect(&r, p.x, p.y); return r; }
    CRect operator-(const POINT p) const noexcept { CRect r(*this); OffsetRect(&r, -p.x, -p.y); return r; }
};

// -----------------------------------------------------------------------------
//  Message-dispatch infrastructure
// -----------------------------------------------------------------------------
class CCmdTarget;
class CCmdUI;
class CDC;
class CMenu;
class CBitmap;
class CPen;
class CBrush;
class CFont;
class CRgn;
class GdiObject;

using MessageResult = std::optional<LRESULT>;
using RouteInvoker = MessageResult(*)(CCmdTarget& target, UINT message, WPARAM wParam, LPARAM lParam);

struct RouteEntry
{
    UINT message;
    UINT code;
    UINT firstId;
    UINT lastId;
    RouteInvoker invoke;
    const UINT* registeredMessage;
};

struct RouteTable
{
    const RouteTable* base = nullptr;
    std::span<const RouteEntry> entries;
};

constexpr bool HasSameRouteKey(const RouteEntry& left, const RouteEntry& right) noexcept
{
    return left.message == right.message && left.code == right.code &&
        left.firstId == right.firstId && left.lastId == right.lastId &&
        left.registeredMessage == right.registeredMessage;
}

inline void ValidateRoutes(const std::span<const RouteEntry> entries)
{
#ifndef NDEBUG
    for (const auto [i, entry] : std::views::enumerate(entries))
        for (const auto& other : entries.subspan(i + 1))
            assert(!HasSameRouteKey(entry, other));
#else
    (void)entries;
#endif
}

// Command invocation and command-UI update codes.
inline constexpr int CommandCode = 0;
inline constexpr int UpdateCode = -1;

constexpr UINT ReflectedId = static_cast<UINT>(-1);   // nID sentinel for reflected handlers

// -----------------------------------------------------------------------------
//  CCmdTarget — message-dispatch base + command routing
// -----------------------------------------------------------------------------
class CCmdTarget
{
public:
    CCmdTarget() = default;
    CCmdTarget(const CCmdTarget&) = delete;
    CCmdTarget& operator=(const CCmdTarget&) = delete;
    virtual ~CCmdTarget() = default;

    virtual const RouteTable* GetRouteTable() const { return GetStaticRouteTable(); }
    static const RouteTable* GetStaticRouteTable()
    {
        static constexpr RouteTable table{};
        return &table;
    }

    virtual bool RouteCommand(UINT nID, int nCode, void* pExtra, bool execute = true);
};

template<typename Derived, typename Base>
class MessageTarget : public Base
{
public:
    using Base::Base;

    static const RouteTable* GetStaticRouteTable()
    {
        static const RouteTable table = []
        {
            const auto entries = Derived::Routes();
            ValidateRoutes(entries);
            return RouteTable{ Base::GetStaticRouteTable(), entries };
        }();
        return &table;
    }

protected:
    const RouteTable* GetRouteTable() const override { return GetStaticRouteTable(); }
};

// -----------------------------------------------------------------------------
//  CCmdUI — command-UI updater
// -----------------------------------------------------------------------------
class WindowRef;

class MenuRef
{
public:
    HMENU m_hMenu = nullptr;
    MenuRef() = default;
    MenuRef(std::nullptr_t) noexcept {}
    explicit MenuRef(HMENU menu) noexcept : m_hMenu(menu) {}
    explicit operator bool() const noexcept { return m_hMenu != nullptr; }
    bool operator==(std::nullptr_t) const noexcept { return m_hMenu == nullptr; }
    HMENU Handle() const noexcept { return m_hMenu; }

    enum class ItemLookup { Position, Command };

    int GetItemCount() const noexcept;
    UINT GetItemId(int pos) const noexcept;
    UINT GetItemState(UINT id, UINT flags) const noexcept;
    std::wstring GetItemText(UINT pos) const;
    MenuRef GetSubMenu(int pos) const;
    bool Append(UINT flags, UINT_PTR id = 0, LPCWSTR psz = nullptr) const noexcept;
    bool Append(UINT flags, UINT_PTR id, const std::wstring& text) const noexcept { return Append(flags, id, text.c_str()); }
    bool Modify(UINT pos, UINT flags, UINT_PTR id, LPCWSTR psz) const noexcept;
    bool Modify(UINT pos, UINT flags, UINT_PTR id, const std::wstring& text) const noexcept { return Modify(pos, flags, id, text.c_str()); }
    bool Remove(UINT pos, UINT flags = MF_BYPOSITION) const noexcept;
    UINT EnableItem(UINT id, UINT flags) const noexcept;
    UINT CheckItem(UINT id, UINT flags) const noexcept;
    bool SetDefaultItem(UINT item) const noexcept;
    bool GetItemInfo(UINT item, MENUITEMINFOW* info, ItemLookup lookup = ItemLookup::Position) const;
    bool SetItemInfo(UINT item, const MENUITEMINFOW* info, ItemLookup lookup = ItemLookup::Position) const;
    UINT ShowPopup(UINT flags, int x, int y, WindowRef window, LPTPMPARAMS lptpm = nullptr) const;
    UINT ShowPopup(UINT flags, POINT pt, WindowRef window, LPTPMPARAMS lptpm = nullptr) const;
    void SetItemEnabled(int item, bool enable, ItemLookup lookup = ItemLookup::Position) const;
    bool IsItemEnabled(UINT item, ItemLookup lookup = ItemLookup::Position) const noexcept;

};

class CMenu final : public MenuRef
{
public:
    CMenu() = default;
    ~CMenu() { if (m_hMenu != nullptr) DestroyMenu(m_hMenu); }
    CMenu(const CMenu&) = delete;
    CMenu& operator=(const CMenu&) = delete;
    CMenu(CMenu&& other) noexcept : MenuRef(other.Detach()) {}
    CMenu& operator=(CMenu&& other) noexcept;
    HMENU Detach() noexcept { return std::exchange(m_hMenu, nullptr); }
    static CMenu CreatePopup() noexcept;
    static CMenu LoadResource(UINT id) noexcept;

private:
    explicit CMenu(HMENU menu) noexcept : MenuRef(menu) {}
};

class CCmdUI
{
public:
    UINT   m_nID = 0;
    UINT   m_nIndex = 0;
    MenuRef m_menu;
    bool   m_bEnableChanged = false;

    virtual void Enable(bool bOn = true);
    virtual void SetCheck(int nCheck = 1);
    virtual void SetRadio(bool bOn = true);
    virtual void SetText(LPCWSTR lpszText);
    virtual bool Update(CCmdTarget* pTarget, bool disableIfUnhandled);
};

// -----------------------------------------------------------------------------
//  GDI objects
// -----------------------------------------------------------------------------
class GdiObject
{
public:
    HGDIOBJ m_hObject = nullptr;

    GdiObject() = default;
    explicit GdiObject(const HGDIOBJ h) : m_hObject(h) {}
    ~GdiObject() { Reset(); }

    GdiObject(const GdiObject&) = delete;
    GdiObject& operator=(const GdiObject&) = delete;
    GdiObject(GdiObject&& other) noexcept : m_hObject(std::exchange(other.m_hObject, nullptr)) {}
    GdiObject& operator=(GdiObject&& other) noexcept
    {
        if (this != &other) { Reset(); m_hObject = std::exchange(other.m_hObject, nullptr); }
        return *this;
    }

    operator HGDIOBJ() const noexcept { return m_hObject; }
    explicit operator bool() const noexcept { return m_hObject != nullptr; }
    HGDIOBJ Handle() const noexcept { return m_hObject; }

    bool Attach(const HGDIOBJ h) noexcept { if (m_hObject) return false; m_hObject = h; return h != nullptr; }
    HGDIOBJ Detach() noexcept {
        const HGDIOBJ h = m_hObject; m_hObject = nullptr; return h;
    }
    bool Reset() noexcept
    {
        if (m_hObject == nullptr) return false;
        const bool r = DeleteObject(m_hObject);
        m_hObject = nullptr;
        return r;
    }
};

class CPen final : public GdiObject
{
public:
    CPen() = default;
    CPen(const int style, const int width, const COLORREF color) noexcept : GdiObject(CreatePen(style, width, color)) {}
};

class CBrush final : public GdiObject
{
public:
    CBrush() = default;
    explicit CBrush(const COLORREF color) noexcept : GdiObject(CreateSolidBrush(color)) {}
    bool CreateSolid(const COLORREF c) noexcept { Reset(); m_hObject = CreateSolidBrush(c); return m_hObject != nullptr; }
    operator HBRUSH() const noexcept { return static_cast<HBRUSH>(m_hObject); }
};

class CFont final : public GdiObject
{
public:
    CFont() = default;
    explicit CFont(const LOGFONTW& lf) noexcept : GdiObject(CreateFontIndirectW(&lf)) {}
    CFont(const HFONT source, const LONG weight) noexcept
    {
        LOGFONTW lf{};
        if (GetObjectW(source, static_cast<int>(sizeof(LOGFONTW)), &lf) != 0)
        {
            lf.lfWeight = weight;
            m_hObject = ::CreateFontIndirectW(&lf);
        }
    }
    CFont(const int height, const int weight, const LPCWSTR face, const BYTE outPrecision = OUT_DEFAULT_PRECIS,
        const BYTE quality = ANTIALIASED_QUALITY, const BYTE pitchAndFamily = DEFAULT_PITCH | FF_DONTCARE) noexcept
    {
        Create(height, weight, face, outPrecision, quality, pitchAndFamily);
    }
    bool Create(const int height, const int weight, const LPCWSTR face,
        const BYTE outPrecision = OUT_DEFAULT_PRECIS, const BYTE quality = ANTIALIASED_QUALITY,
        const BYTE pitchAndFamily = DEFAULT_PITCH | FF_DONTCARE) noexcept
    {
        Reset();
        m_hObject = CreateFontW(height, 0, 0, 0, weight, false, false, false, DEFAULT_CHARSET,
            outPrecision, CLIP_DEFAULT_PRECIS, quality, pitchAndFamily, face);
        return m_hObject != nullptr;
    }
    operator HFONT() const noexcept { return static_cast<HFONT>(m_hObject); }
};

class CBitmap final : public GdiObject
{
public:
    CBitmap() = default;
    explicit CBitmap(const HBITMAP bitmap) noexcept : GdiObject(bitmap) {}
    CBitmap(const int w, const int h, const UINT planes, const UINT bits, const void* bitmapBits) noexcept
        : GdiObject(CreateBitmap(w, h, planes, bits, bitmapBits)) {}
    CBitmap(const CDC* dc, const int width, const int height) noexcept { CreateCompatible(dc, width, height); }
    bool CreateCompatible(const CDC* pDC, int w, int h);
    std::optional<BITMAP> Info() const noexcept
    {
        BITMAP bitmap{};
        if (GetObjectW(m_hObject, static_cast<int>(sizeof(bitmap)), &bitmap) == 0) return std::nullopt;
        return bitmap;
    }
    operator HBITMAP() const noexcept { return static_cast<HBITMAP>(m_hObject); }
};

class CRgn final : public GdiObject
{
public:
    CRgn() = default;
    CRgn(const int left, const int top, const int right, const int bottom, const int ellipseWidth, const int ellipseHeight) noexcept
        : GdiObject(CreateRoundRectRgn(left, top, right, bottom, ellipseWidth, ellipseHeight)) {}
    CRgn(const RECT& r, const int ellipseWidth, const int ellipseHeight) noexcept
        : GdiObject(CreateRoundRectRgn(r.left, r.top, r.right, r.bottom, ellipseWidth, ellipseHeight)) {}
    operator HRGN() const noexcept { return static_cast<HRGN>(m_hObject); }
};

// -----------------------------------------------------------------------------
//  CDC
// -----------------------------------------------------------------------------
class CDC
{
public:
    HDC m_hDC = nullptr;

    CDC() = default;
    explicit CDC(const CDC* compatibleWith) noexcept
        : m_hDC(CreateCompatibleDC(compatibleWith ? compatibleWith->m_hDC : nullptr)),
        m_bOwned(m_hDC != nullptr) {}
    ~CDC() { if (m_bOwned && m_hDC) DeleteDC(m_hDC); }

    CDC(const CDC&) = delete;
    CDC& operator=(const CDC&) = delete;

    static CDC Borrow(const HDC dc) noexcept { return CDC(dc, false); }

    operator HDC() const noexcept { return m_hDC; }
    explicit operator bool() const noexcept { return m_hDC != nullptr; }
    HDC Handle() const noexcept { return m_hDC; }

    bool Attach(const HDC hDC) noexcept
    {
        if (m_hDC != nullptr) return false;
        m_hDC = hDC;
        m_bOwned = hDC != nullptr;
        return hDC != nullptr;
    }
    HDC Detach() noexcept {
        const HDC h = m_hDC; m_hDC = nullptr; m_bOwned = false; return h;
    }

    int SelectClipRgn(const CRgn* pRgn) noexcept { return ::SelectClipRgn(m_hDC, pRgn ? static_cast<HRGN>(pRgn->m_hObject) : nullptr); }

    // Attributes
    COLORREF SetTextColor(const COLORREF c) noexcept { return ::SetTextColor(m_hDC, c); }
    COLORREF GetTextColor() const noexcept { return ::GetTextColor(m_hDC); }
    COLORREF SetBkColor(const COLORREF c) noexcept { return ::SetBkColor(m_hDC, c); }
    COLORREF GetBkColor() const noexcept { return ::GetBkColor(m_hDC); }
    int SetBkMode(const int mode) noexcept { return ::SetBkMode(m_hDC, mode); }
    int GetDeviceCaps(const int n) const noexcept { return ::GetDeviceCaps(m_hDC, n); }

    CPoint SetViewportOrg(const int x, const int y) noexcept { POINT p{}; SetViewportOrgEx(m_hDC, x, y, &p); return p; }

    // Rect/region clipping
    std::optional<CRect> GetClipBox() const noexcept
    {
        CRect rect;
        if (::GetClipBox(m_hDC, &rect) == ERROR) return std::nullopt;
        return rect;
    }
    int IntersectClipRect(const RECT& rc) noexcept { return ::IntersectClipRect(m_hDC, rc.left, rc.top, rc.right, rc.bottom); }

    // Drawing primitives
    CPoint MoveTo(const int x, const int y) noexcept { POINT p{}; MoveToEx(m_hDC, x, y, &p); return p; }
    CPoint MoveTo(const POINT p) noexcept { return MoveTo(p.x, p.y); }
    bool LineTo(const int x, const int y) noexcept { return ::LineTo(m_hDC, x, y); }
    bool LineTo(const POINT p) noexcept { return ::LineTo(m_hDC, p.x, p.y); }
    bool Rectangle(const int l, const int t, const int r, const int b) noexcept { return ::Rectangle(m_hDC, l, t, r, b); }
    bool Rectangle(const RECT& rc) noexcept { return Rectangle(rc.left, rc.top, rc.right, rc.bottom); }
    bool RoundRect(const RECT& rc, const POINT pt) noexcept { return ::RoundRect(m_hDC, rc.left, rc.top, rc.right, rc.bottom, pt.x, pt.y); }
    bool Ellipse(const int l, const int t, const int r, const int b) noexcept { return ::Ellipse(m_hDC, l, t, r, b); }
    bool Ellipse(const RECT& rc) noexcept { return Ellipse(rc.left, rc.top, rc.right, rc.bottom); }
    bool Polygon(const POINT* p, const int n) noexcept { return ::Polygon(m_hDC, p, n); }
    bool Polyline(const POINT* p, const int n) noexcept { return ::Polyline(m_hDC, p, n); }
    void DrawTreeExpander(const CRect& nodeRect, bool expanded);

    void FillSolidRect(const RECT& rc, const COLORREF clr) noexcept
    {
        ::SetBkColor(m_hDC, clr);
        ExtTextOutW(m_hDC, 0, 0, ETO_OPAQUE, &rc, nullptr, 0, nullptr);
    }
    void FillSolidRect(const int x, const int y, const int cx, const int cy, const COLORREF clr) noexcept
    {
        FillSolidRect(RECT{ x, y, x + cx, y + cy }, clr);
    }
    void FrameRect(const LPCRECT rc, CBrush* pBrush) noexcept { ::FrameRect(m_hDC, rc, pBrush ? static_cast<HBRUSH>(pBrush->m_hObject) : nullptr); }
    void FrameRect(const RECT& rc, CBrush* pBrush) noexcept { FrameRect(&rc, pBrush); }
    void Draw3dRect(const RECT& rc, const COLORREF topLeft, const COLORREF bottomRight) noexcept
    {
        const int width = rc.right - rc.left;
        const int height = rc.bottom - rc.top;
        if (width <= 0 || height <= 0) return;

        FillSolidRect(rc.left, rc.top, width - 1, 1, topLeft);
        FillSolidRect(rc.left, rc.top, 1, height - 1, topLeft);
        FillSolidRect(rc.right - 1, rc.top, 1, height, bottomRight);
        FillSolidRect(rc.left, rc.bottom - 1, width, 1, bottomRight);
    }
    bool DrawEdge(const LPRECT rc, const UINT edge, const UINT flags) noexcept { return ::DrawEdge(m_hDC, rc, edge, flags); }
    void DrawFocusRect(const RECT& rc) noexcept { ::DrawFocusRect(m_hDC, &rc); }
    // Text
    int DrawText(const LPCWSTR psz, const int n, const LPRECT rc, const UINT fmt) noexcept { return ::DrawTextW(m_hDC, psz, n, rc, fmt); }
    int DrawText(const std::wstring_view text, const LPRECT rc, const UINT fmt) noexcept { return ::DrawTextW(m_hDC, text.data(), static_cast<int>(text.size()), rc, fmt); }
    bool TextOut(const int x, const int y, const std::wstring_view text) noexcept { return ::TextOutW(m_hDC, x, y, text.data(), static_cast<int>(text.size())); }
    CSize GetTextExtent(const LPCWSTR psz, const int n) const noexcept { SIZE s{}; GetTextExtentPoint32W(m_hDC, psz, n, &s); return s; }
    CSize GetTextExtent(const std::wstring_view text) const noexcept { return GetTextExtent(text.data(), static_cast<int>(text.size())); }
    std::optional<TEXTMETRICW> GetTextMetrics() const noexcept
    {
        TEXTMETRICW metrics{};
        if (!::GetTextMetricsW(m_hDC, &metrics)) return std::nullopt;
        return metrics;
    }

    // Blit
    bool BitBlt(const int x, const int y, const int cx, const int cy, const CDC* pSrc, const int xs, const int ys, const DWORD rop) noexcept { return ::BitBlt(m_hDC, x, y, cx, cy, pSrc ? pSrc->m_hDC : nullptr, xs, ys, rop); }
    bool PatBlt(const int x, const int y, const int cx, const int cy, const DWORD rop) noexcept { return ::PatBlt(m_hDC, x, y, cx, cy, rop); }
    bool AlphaBlend(const int x, const int y, const int cx, const int cy, const CDC* pSrc, const int xs, const int ys, const int cxs, const int cys, const BLENDFUNCTION bf) noexcept { return ::AlphaBlend(m_hDC, x, y, cx, cy, pSrc ? pSrc->m_hDC : nullptr, xs, ys, cxs, cys, bf); }
    COLORREF SetDCPenColor(const COLORREF c) noexcept { return ::SetDCPenColor(m_hDC, c); }
    COLORREF SetDCBrushColor(const COLORREF c) noexcept { return ::SetDCBrushColor(m_hDC, c); }
    HFONT GetCurrentFont() const noexcept { return static_cast<HFONT>(GetCurrentObject(m_hDC, OBJ_FONT)); }

protected:
    CDC(const HDC dc, const bool owned) noexcept : m_hDC(dc), m_bOwned(owned && dc != nullptr) {}
    bool m_bOwned = false;
};

inline bool CBitmap::CreateCompatible(const CDC* pDC, const int w, const int h)
{
    Reset(); m_hObject = CreateCompatibleBitmap(pDC ? pDC->m_hDC : nullptr, w, h); return m_hObject != nullptr;
}

class GdiObjectSelection final
{
public:
    GdiObjectSelection(const HDC dc, const HGDIOBJ object) noexcept
        : m_dc(dc), m_oldObject(dc != nullptr ? SelectObject(dc, object) : nullptr) {}

    GdiObjectSelection(const HDC dc, const GdiObject* object) noexcept
        : GdiObjectSelection(dc, object != nullptr ? object->Handle() : nullptr) {}

    GdiObjectSelection(CDC* dc, const HGDIOBJ object) noexcept
        : GdiObjectSelection(dc != nullptr ? dc->Handle() : nullptr, object) {}

    GdiObjectSelection(CDC* dc, const GdiObject* object) noexcept
        : GdiObjectSelection(dc, object != nullptr ? object->Handle() : nullptr) {}

    GdiObjectSelection(const GdiObjectSelection&) = delete;
    GdiObjectSelection& operator=(const GdiObjectSelection&) = delete;

    ~GdiObjectSelection() noexcept
    {
        if (m_dc != nullptr && m_oldObject != nullptr && m_oldObject != HGDI_ERROR)
            SelectObject(m_dc, m_oldObject);
    }

private:
    HDC m_dc = nullptr;
    HGDIOBJ m_oldObject = nullptr;
};

class StockObjectSelection final
{
public:
    StockObjectSelection(const HDC dc, const int index) noexcept
        : m_selection(dc, GetStockObject(index)) {}

    StockObjectSelection(CDC* dc, const int index) noexcept
        : StockObjectSelection(dc != nullptr ? dc->Handle() : nullptr, index) {}

    StockObjectSelection(const StockObjectSelection&) = delete;
    StockObjectSelection& operator=(const StockObjectSelection&) = delete;

private:
    GdiObjectSelection m_selection;
};

// Sets a DC attribute in the constructor and restores the previous
// value in the destructor (e.g. SetBkMode, SetTextColor, SetBkColor).
template <typename V, V (WINAPI* Setter)(HDC, V)>
class ScopedDcAttribute final
{
public:
    ScopedDcAttribute(HDC dc, const V value) noexcept
        : m_dc(dc), m_oldValue(dc != nullptr ? Setter(dc, value) : V{}) {}

    ScopedDcAttribute(CDC* pdc, const V value) noexcept
        : ScopedDcAttribute(pdc != nullptr ? pdc->Handle() : nullptr, value) {}

    ScopedDcAttribute(const ScopedDcAttribute&) = delete;
    ScopedDcAttribute& operator=(const ScopedDcAttribute&) = delete;
    ScopedDcAttribute(ScopedDcAttribute&&) = delete;
    ScopedDcAttribute& operator=(ScopedDcAttribute&&) = delete;

    ~ScopedDcAttribute() noexcept
    {
        if (m_dc != nullptr) Setter(m_dc, m_oldValue);
    }

private:
    HDC m_dc = nullptr;
    V m_oldValue{};
};

using ScopedBkMode = ScopedDcAttribute<int, SetBkMode>;
using ScopedTextColor = ScopedDcAttribute<COLORREF, SetTextColor>;
using ScopedBkColor = ScopedDcAttribute<COLORREF, SetBkColor>;

class ScopedDcState final
{
public:
    ScopedDcState(HDC dc) noexcept : m_dc(dc), m_save(dc != nullptr ? SaveDC(dc) : 0) {}
    ScopedDcState(CDC* dc) noexcept : ScopedDcState(dc != nullptr ? dc->Handle() : nullptr) {}

    ScopedDcState(const ScopedDcState&) = delete;
    ScopedDcState& operator=(const ScopedDcState&) = delete;
    ScopedDcState(ScopedDcState&&) = delete;
    ScopedDcState& operator=(ScopedDcState&&) = delete;

    ~ScopedDcState() noexcept
    {
        if (m_dc != nullptr && m_save != 0) RestoreDC(m_dc, m_save);
    }

private:
    HDC m_dc = nullptr;
    int m_save = 0;
};

template<typename T>
class ScopedValue final
{
public:
    ScopedValue(T& value, T replacement) noexcept
        : m_value(value), m_previous(std::exchange(value, std::move(replacement)))
    {
        static_assert(std::is_nothrow_move_constructible_v<T>);
        static_assert(std::is_nothrow_move_assignable_v<T>);
    }
    ~ScopedValue() noexcept { m_value = std::move(m_previous); }

    ScopedValue(const ScopedValue&) = delete;
    ScopedValue& operator=(const ScopedValue&) = delete;

private:
    T& m_value;
    T m_previous;
};

LRESULT CALLBACK FrameworkWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LPCWSTR RegisterWindowClass(UINT classStyle, HCURSOR hCursor = nullptr,
    HBRUSH hbrBackground = nullptr, HICON hIcon = nullptr);

struct WindowCreationScope;
inline thread_local WindowCreationScope* g_pWndInit = nullptr;
inline thread_local MSG g_currentMsg{};
inline thread_local bool g_waitCursorActive = false;

inline MSG MakeMessageSnapshot(const HWND window, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
    const DWORD position = GetMessagePos();
    return { window, message, wParam, lParam, static_cast<DWORD>(GetMessageTime()),
        POINT{ GET_X_LPARAM(position), GET_Y_LPARAM(position) } };
}

struct WindowCreationScope final
{
    explicit WindowCreationScope(CWnd* pWnd) : m_pWnd(pWnd), m_pPrevious(g_pWndInit) { g_pWndInit = this; }
    ~WindowCreationScope() { g_pWndInit = m_pPrevious; }

    WindowCreationScope(const WindowCreationScope&) = delete;
    WindowCreationScope& operator=(const WindowCreationScope&) = delete;

    CWnd* m_pWnd;
    WindowCreationScope* m_pPrevious;
    bool m_attached = false;
};

// Set for the duration of each WM_DRAWITEM dispatch so CListCtrl::GetItemRect /
// GetSubItemRect can compute column rects from the already-known item rect + header
// widths without sending any messages back to the (subclassed) list control.
struct DrawItemContext
{
    HWND hWnd{};
    int  item = -1;
    RECT rcItem{};
    int  colLeft[16]{};
    int  colRight[16]{};
    bool colValid[16]{};
    int  colCount = 0;
};
inline thread_local DrawItemContext g_drawItemCtx;

class WindowRef
{
public:
    HWND m_hWnd = nullptr;

    WindowRef() = default;
    WindowRef(std::nullptr_t) noexcept {}
    explicit WindowRef(HWND window) noexcept : m_hWnd(window) {}
    WindowRef(const CWnd* window) noexcept;
    operator HWND() const noexcept { return m_hWnd; }
    HWND Handle() const noexcept { return m_hWnd; }

    // ---- common operations ----
    bool DestroyWindow() const noexcept
    {
        return m_hWnd ? ::DestroyWindow(m_hWnd) : false;
    }
    bool ShowWindow(const int nCmdShow) const noexcept { return ::ShowWindow(m_hWnd, nCmdShow); }
    bool UpdateWindow() const noexcept { return ::UpdateWindow(m_hWnd); }
    void Invalidate(const bool bErase = true) const noexcept { ::InvalidateRect(m_hWnd, nullptr, bErase); }
    void InvalidateRect(const LPCRECT rc, const bool bErase = true) const noexcept { ::InvalidateRect(m_hWnd, rc, bErase); }
    bool RedrawWindow(const LPCRECT rc = nullptr, const CRgn* pRgn = nullptr, const UINT flags = RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE) const noexcept
    {
        return ::RedrawWindow(m_hWnd, rc, pRgn ? static_cast<HRGN>(pRgn->m_hObject) : nullptr, flags);
    }

    bool MoveWindow(const int x, const int y, const int cx, const int cy, const bool bRepaint = true) const noexcept { return ::MoveWindow(m_hWnd, x, y, cx, cy, bRepaint); }
    bool MoveWindow(const RECT& rc, const bool bRepaint = true) const noexcept { return ::MoveWindow(m_hWnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, bRepaint); }
    bool SetWindowPos(WindowRef after, const int x, const int y, const int cx, const int cy, const UINT flags) const noexcept { return ::SetWindowPos(m_hWnd, after.Handle(), x, y, cx, cy, flags); }

    CRect GetClientRect() const noexcept { CRect rect; ::GetClientRect(m_hWnd, &rect); return rect; }
    CRect GetWindowRect() const noexcept { return CRect(m_hWnd); }
    CPoint ToScreen(const POINT point) const noexcept
    {
        CPoint result(point);
        ::ClientToScreen(m_hWnd, &result);
        return result;
    }
    CRect ToScreen(const RECT& rect) const noexcept
    {
        CRect result(rect);
        ::ClientToScreen(m_hWnd, reinterpret_cast<LPPOINT>(&result));
        ::ClientToScreen(m_hWnd, reinterpret_cast<LPPOINT>(&result) + 1);
        return result;
    }
    CPoint ToClient(const POINT point) const noexcept
    {
        CPoint result(point);
        ::ScreenToClient(m_hWnd, &result);
        return result;
    }
    CRect ToClient(const RECT& rect) const noexcept
    {
        CRect result(rect);
        ::ScreenToClient(m_hWnd, reinterpret_cast<LPPOINT>(&result));
        ::ScreenToClient(m_hWnd, reinterpret_cast<LPPOINT>(&result) + 1);
        return result;
    }
    CRect GetChildWindowRect(const HWND window) const noexcept { return ToClient(CRect(window)); }
    std::optional<CPoint> GetClientCursorPos() const noexcept
    {
        CPoint point;
        if (!::GetCursorPos(&point) || !::ScreenToClient(m_hWnd, &point)) return std::nullopt;
        return point;
    }
    int ScaleForDpi(const int value) const noexcept { return ::ScaleForDpi(value, m_hWnd); }

    WindowRef GetParent() const { return WindowRef(::GetParent(m_hWnd)); }
    bool IsChild(const HWND hWnd) const noexcept { return hWnd != nullptr && ::IsChild(m_hWnd, hWnd); }
    WindowRef GetDlgItem(const int nID) const { return WindowRef(::GetDlgItem(m_hWnd, nID)); }
    WindowRef GetWindow(const UINT nCmd) const { return WindowRef(::GetWindow(m_hWnd, nCmd)); }
    WindowRef GetFocus() const { return WindowRef(::GetFocus()); }
    bool HasFocus() const noexcept { return ::GetFocus() == m_hWnd; }
    WindowRef ChildWindowFromPoint(const POINT pt) const { return WindowRef(::ChildWindowFromPoint(m_hWnd, pt)); }
    WindowRef ChildWindowFromPoint(const POINT pt, const UINT flags) const { return WindowRef(ChildWindowFromPointEx(m_hWnd, pt, flags)); }
    static WindowRef GetCapture() { return WindowRef(::GetCapture()); }
    bool HasCapture() const noexcept { return ::GetCapture() == m_hWnd; }

    LRESULT SendMessage(const UINT msg, const WPARAM wParam = 0, const LPARAM lParam = 0) const noexcept { return ::SendMessageW(m_hWnd, msg, wParam, lParam); }
    template <typename T>
    LRESULT SendMessage(const UINT msg, const WPARAM wParam, T* lParam) const noexcept
    {
        return SendMessage(msg, wParam, reinterpret_cast<LPARAM>(lParam));
    }
    bool PostMessage(const UINT msg, const WPARAM wParam = 0, const LPARAM lParam = 0) const noexcept { return ::PostMessageW(m_hWnd, msg, wParam, lParam); }
    void SetText(const LPCWSTR psz) const noexcept { SetWindowTextW(m_hWnd, psz); }
    void SetText(const std::wstring& text) const noexcept { SetWindowTextW(m_hWnd, text.c_str()); }
    std::wstring GetText() const
    {
        std::wstring text;
        text.resize_and_overwrite(static_cast<size_t>(GetWindowTextLengthW(m_hWnd)) + 1,
            [this](wchar_t* data, const size_t size) noexcept {
                return static_cast<size_t>(GetWindowTextW(m_hWnd, data, static_cast<int>(size)));
            });
        return text;
    }
    UINT GetButtonCheckState(const int id) const noexcept { return IsDlgButtonChecked(m_hWnd, id); }
    bool IsChecked(const int id) const noexcept { return IsDlgButtonChecked(m_hWnd, id) == BST_CHECKED; }
    void SetChecked(const int id, const bool checked) const noexcept { CheckDlgButton(m_hWnd, id, checked ? BST_CHECKED : BST_UNCHECKED); }
    std::wstring GetText(const int id) const { return GetDlgItem(id).GetText(); }
    void SetText(const int id, const std::wstring& text) const noexcept { SetDlgItemTextW(m_hWnd, id, text.c_str()); }
    void SetText(const int id, const LPCWSTR psz) const noexcept { SetDlgItemTextW(m_hWnd, id, psz); }
    void SetText(const int id, const auto number) const { SetText(id, std::to_wstring(number)); }

    int GetComboSelection(const int id) const noexcept { return static_cast<int>(SendDlgItemMessageW(m_hWnd, id, CB_GETCURSEL, 0, 0)); }
    void SetComboSelection(const int id, const int index) const noexcept { SendDlgItemMessageW(m_hWnd, id, CB_SETCURSEL, index, 0); }
    int GetCheckedRadioButton(const int first, const int last) const noexcept
    {
        for (int id = first; id <= last; ++id) if (IsDlgButtonChecked(m_hWnd, id) == BST_CHECKED) return id;
        return 0;
    }
    void SetCheckedRadioButton(const int first, const int last, const int id) const noexcept { CheckRadioButton(m_hWnd, first, last, id); }

    bool EnableWindow(const bool bEnable = true) const noexcept { return ::EnableWindow(m_hWnd, bEnable); }
    bool IsWindowEnabled() const noexcept { return ::IsWindowEnabled(m_hWnd); }
    bool IsWindowVisible() const noexcept { return ::IsWindowVisible(m_hWnd); }
    bool IsIconic() const noexcept { return ::IsIconic(m_hWnd); }
    HWND SetFocus() const noexcept { return ::SetFocus(m_hWnd); }
    HWND SetCapture() const noexcept { return ::SetCapture(m_hWnd); }
    bool BringWindowToTop() const noexcept { return ::BringWindowToTop(m_hWnd); }
    bool SetForegroundWindow() const noexcept { return ::SetForegroundWindow(m_hWnd); }

    LONG GetStyle() const noexcept { return static_cast<LONG>(GetWindowLongPtrW(m_hWnd, GWL_STYLE)); }
    bool ModifyStyle(const DWORD remove, const DWORD add, const UINT flags = 0)
    {
        const LONG_PTR s = GetWindowLongPtrW(m_hWnd, GWL_STYLE);
        const LONG_PTR n = (s & ~static_cast<LONG_PTR>(remove)) | add;
        if (s == n) return false;
        SetWindowLongPtrW(m_hWnd, GWL_STYLE, n);
        if (flags) ::SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | flags);
        return true;
    }
    bool ModifyStyleEx(const DWORD remove, const DWORD add, const UINT flags = 0)
    {
        const LONG_PTR s = GetWindowLongPtrW(m_hWnd, GWL_EXSTYLE);
        const LONG_PTR n = (s & ~static_cast<LONG_PTR>(remove)) | add;
        if (s == n) return false;
        SetWindowLongPtrW(m_hWnd, GWL_EXSTYLE, n);
        if (flags) ::SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | flags);
        return true;
    }

    UINT_PTR SetTimer(const UINT_PTR id, const UINT interval) const noexcept { return ::SetTimer(m_hWnd, id, interval, nullptr); }

    bool GetWindowPlacement(WINDOWPLACEMENT* p) const noexcept { return ::GetWindowPlacement(m_hWnd, p); }
    bool SetWindowPlacement(const WINDOWPLACEMENT* p) const noexcept { return ::SetWindowPlacement(m_hWnd, p); }

    HFONT GetFont() const noexcept { return reinterpret_cast<HFONT>(::SendMessageW(m_hWnd, WM_GETFONT, 0, 0)); }
    void SetFont(const HFONT font) const noexcept { ::SendMessageW(m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), true); }
    void SetRedraw(const bool bRedraw = true) const noexcept { ::SendMessageW(m_hWnd, WM_SETREDRAW, static_cast<WPARAM>(bRedraw), 0); }

    bool CopyTextToClipboard(std::wstring_view text) const;

    int  GetDlgCtrlID() const noexcept { return ::GetDlgCtrlID(m_hWnd); }
    HICON SetIcon(HICON hIcon, const bool bBig = true) const noexcept { return reinterpret_cast<HICON>(SendMessage(WM_SETICON, bBig ? ICON_BIG : ICON_SMALL, hIcon)); }
    static WindowRef GetDesktopWindow() { return WindowRef(::GetDesktopWindow()); }
    bool LockWindowUpdate() const noexcept { return ::LockWindowUpdate(m_hWnd); }
    void UnlockWindowUpdate() const noexcept { ::LockWindowUpdate(nullptr); }
    bool HideCaret() const noexcept { return ::HideCaret(m_hWnd); }

    MenuRef GetMenu() const;

    int GetScrollPos(const int bar) const noexcept { return ::GetScrollPos(m_hWnd, bar); }
    void ShowScrollBar(const UINT bar, const bool bShow = true) const noexcept { ::ShowScrollBar(m_hWnd, bar, bShow); }
    bool GetScrollInfo(const int bar, SCROLLINFO* psi, const UINT mask = SIF_ALL) const noexcept { psi->cbSize = sizeof(SCROLLINFO); psi->fMask = mask; return ::GetScrollInfo(m_hWnd, bar, psi); }
    int SetScrollInfo(const int bar, SCROLLINFO* psi, const bool bRedraw = true)
    {
        psi->cbSize = sizeof(SCROLLINFO); return ::SetScrollInfo(m_hWnd, bar, psi, bRedraw);
    }

    void CenterWindow(WindowRef alternate = {});

};
