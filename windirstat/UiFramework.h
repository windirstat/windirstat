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

inline int GetWindowDpi(const HWND window) noexcept
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
    CRect(const POINT p, const SIZE s) noexcept { left = p.x; top = p.y; right = p.x + s.cx; bottom = p.y + s.cy; }

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
    bool Intersect(const LPCRECT a, const LPCRECT b) noexcept { return IntersectRect(this, a, b); }
    bool Union(const LPCRECT a, const LPCRECT b) noexcept { return UnionRect(this, a, b); }

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

using RouteInvoker = LRESULT(*)(CCmdTarget& target, UINT message, WPARAM wParam, LPARAM lParam, bool& handled);

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
    for (size_t i = 0; i < entries.size(); ++i)
        for (size_t j = i + 1; j < entries.size(); ++j)
            assert(!HasSameRouteKey(entries[i], entries[j]));
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
class CCmdUI
{
public:
    UINT   m_nID = 0;
    UINT   m_nIndex = 0;
    CMenu* m_pMenu = nullptr;
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
    CPen(const int nPenStyle, const int nWidth, const LOGBRUSH* pLogBrush, const int nStyleCount = 0,
        const DWORD* lpStyle = nullptr)
    {
        m_hObject = ExtCreatePen(static_cast<DWORD>(nPenStyle), static_cast<DWORD>(nWidth), pLogBrush,
            static_cast<DWORD>(nStyleCount), lpStyle);
    }

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
        if (GetObjectW(source, static_cast<int>(sizeof(LOGFONTW)), &lf) == 0) return;
        lf.lfWeight = weight;
        m_hObject = CreateFontIndirectW(&lf);
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
    CBitmap(CDC* dc, const int width, const int height) noexcept { CreateCompatible(dc, width, height); }
    bool CreateCompatible(CDC* pDC, int w, int h);
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
    explicit CDC(CDC* compatibleWith) noexcept
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

    int SelectClipRgn(CRgn* pRgn) noexcept { return ::SelectClipRgn(m_hDC, pRgn ? static_cast<HRGN>(pRgn->m_hObject) : nullptr); }

    // Attributes
    COLORREF SetTextColor(const COLORREF c) noexcept { return ::SetTextColor(m_hDC, c); }
    COLORREF GetTextColor() const noexcept { return ::GetTextColor(m_hDC); }
    COLORREF SetBkColor(const COLORREF c) noexcept { return ::SetBkColor(m_hDC, c); }
    COLORREF GetBkColor() const noexcept { return ::GetBkColor(m_hDC); }
    int SetBkMode(const int mode) noexcept { return ::SetBkMode(m_hDC, mode); }
    int GetDeviceCaps(const int n) const noexcept { return ::GetDeviceCaps(m_hDC, n); }

    CPoint SetViewportOrg(const int x, const int y) noexcept { POINT p{}; SetViewportOrgEx(m_hDC, x, y, &p); return p; }

    // Rect/region clipping
    std::optional<CRect> ClipBox() const noexcept
    {
        CRect rect;
        if (::GetClipBox(m_hDC, &rect) == ERROR) return std::nullopt;
        return rect;
    }
    int IntersectClipRect(const LPCRECT rc) noexcept { return ::IntersectClipRect(m_hDC, rc->left, rc->top, rc->right, rc->bottom); }

    // Drawing primitives
    CPoint MoveTo(const int x, const int y) noexcept { POINT p{}; MoveToEx(m_hDC, x, y, &p); return p; }
    CPoint MoveTo(const POINT p) noexcept { return MoveTo(p.x, p.y); }
    bool LineTo(const int x, const int y) noexcept { return ::LineTo(m_hDC, x, y); }
    bool LineTo(const POINT p) noexcept { return ::LineTo(m_hDC, p.x, p.y); }
    bool Rectangle(const int l, const int t, const int r, const int b) noexcept { return ::Rectangle(m_hDC, l, t, r, b); }
    bool Rectangle(const LPCRECT rc) noexcept { return ::Rectangle(m_hDC, rc->left, rc->top, rc->right, rc->bottom); }
    bool RoundRect(const int l, const int t, const int r, const int b, const int w, const int h) noexcept { return ::RoundRect(m_hDC, l, t, r, b, w, h); }
    bool RoundRect(const LPCRECT rc, const POINT pt) noexcept { return ::RoundRect(m_hDC, rc->left, rc->top, rc->right, rc->bottom, pt.x, pt.y); }
    bool Ellipse(const int l, const int t, const int r, const int b) noexcept { return ::Ellipse(m_hDC, l, t, r, b); }
    bool Ellipse(const LPCRECT rc) noexcept { return Ellipse(rc->left, rc->top, rc->right, rc->bottom); }
    bool Polygon(const POINT* p, const int n) noexcept { return ::Polygon(m_hDC, p, n); }
    bool Polyline(const POINT* p, const int n) noexcept { return ::Polyline(m_hDC, p, n); }
    void DrawTreeConnector(const CRect& nodeRect, COLORREF background, bool toTop, bool toBottom,
        bool toRight, bool showPlus = false, bool showMinus = false);

    void FillSolidRect(const LPCRECT rc, const COLORREF clr) noexcept
    {
        ::SetBkColor(m_hDC, clr);
        ExtTextOutW(m_hDC, 0, 0, ETO_OPAQUE, rc, nullptr, 0, nullptr);
    }
    void FillSolidRect(const int x, const int y, const int cx, const int cy, const COLORREF clr) noexcept
    {
        const RECT rc{ x, y, x + cx, y + cy };
        FillSolidRect(&rc, clr);
    }
    void FrameRect(const LPCRECT rc, CBrush* pBrush) noexcept { ::FrameRect(m_hDC, rc, pBrush ? static_cast<HBRUSH>(pBrush->m_hObject) : nullptr); }
    void Draw3dRect(const LPCRECT rc, const COLORREF topLeft, const COLORREF bottomRight) noexcept
    {
        const int width = rc->right - rc->left;
        const int height = rc->bottom - rc->top;
        if (width <= 0 || height <= 0) return;

        FillSolidRect(rc->left, rc->top, width - 1, 1, topLeft);
        FillSolidRect(rc->left, rc->top, 1, height - 1, topLeft);
        FillSolidRect(rc->right - 1, rc->top, 1, height, bottomRight);
        FillSolidRect(rc->left, rc->bottom - 1, width, 1, bottomRight);
    }
    bool DrawEdge(const LPRECT rc, const UINT edge, const UINT flags) noexcept { return ::DrawEdge(m_hDC, rc, edge, flags); }
    void DrawFocusRect(const LPCRECT rc) noexcept { ::DrawFocusRect(m_hDC, rc); }
    // Text
    int DrawText(const LPCWSTR psz, const int n, const LPRECT rc, const UINT fmt) noexcept { return ::DrawTextW(m_hDC, psz, n, rc, fmt); }
    int DrawText(const std::wstring_view text, const LPRECT rc, const UINT fmt) noexcept { return ::DrawTextW(m_hDC, text.data(), static_cast<int>(text.size()), rc, fmt); }
    bool TextOut(const int x, const int y, const std::wstring_view text) noexcept { return ::TextOutW(m_hDC, x, y, text.data(), static_cast<int>(text.size())); }
    CSize GetTextExtent(const LPCWSTR psz, const int n) const noexcept { SIZE s{}; GetTextExtentPoint32W(m_hDC, psz, n, &s); return s; }
    std::optional<TEXTMETRICW> TextMetrics() const noexcept
    {
        TEXTMETRICW metrics{};
        if (!::GetTextMetricsW(m_hDC, &metrics)) return std::nullopt;
        return metrics;
    }

    // Blit
    bool BitBlt(const int x, const int y, const int cx, const int cy, CDC* pSrc, const int xs, const int ys, const DWORD rop) noexcept { return ::BitBlt(m_hDC, x, y, cx, cy, pSrc ? pSrc->m_hDC : nullptr, xs, ys, rop); }
    bool PatBlt(const int x, const int y, const int cx, const int cy, const DWORD rop) noexcept { return ::PatBlt(m_hDC, x, y, cx, cy, rop); }
    bool AlphaBlend(const int x, const int y, const int cx, const int cy, CDC* pSrc, const int xs, const int ys, const int cxs, const int cys, const BLENDFUNCTION bf) noexcept { return ::AlphaBlend(m_hDC, x, y, cx, cy, pSrc ? pSrc->m_hDC : nullptr, xs, ys, cxs, cys, bf); }
    COLORREF SetDCPenColor(const COLORREF c) noexcept { return ::SetDCPenColor(m_hDC, c); }
    COLORREF SetDCBrushColor(const COLORREF c) noexcept { return ::SetDCBrushColor(m_hDC, c); }
    HFONT GetCurrentFont() const noexcept { return static_cast<HFONT>(GetCurrentObject(m_hDC, OBJ_FONT)); }

protected:
    CDC(const HDC dc, const bool owned) noexcept : m_hDC(dc), m_bOwned(owned && dc != nullptr) {}
    bool m_bOwned = false;
};

inline bool CBitmap::CreateCompatible(CDC* pDC, const int w, const int h)
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

struct NotificationRoute { NMHDR* pNMHDR; LRESULT* pResult; UINT id; };

LRESULT CALLBACK FrameworkWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LPCWSTR RegisterWindowClass(UINT classStyle, HCURSOR hCursor = nullptr,
    HBRUSH hbrBackground = nullptr, HICON hIcon = nullptr);

struct WindowCreationScope;
inline thread_local WindowCreationScope* g_pWndInit = nullptr;
inline thread_local MSG g_currentMsg{};

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

class CWnd : public CCmdTarget
{
public:
    HWND m_hWnd = nullptr;

    CWnd() = default;
    explicit CWnd(const HWND h) : m_hWnd(h) {}
    ~CWnd() override
    {
        const HWND hWnd = m_hWnd;
        if (hWnd == nullptr || FindAttached(hWnd) != this) return;

        if (m_pfnSuper != nullptr)
        {
            if (reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hWnd, GWLP_WNDPROC)) == FrameworkWindowProc)
            {
                const auto previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hWnd, GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(m_pfnSuper)));
                if (previous == FrameworkWindowProc)
                {
                    RemovePropW(hWnd, kSuperProp());
                    m_pfnSuper = nullptr;
                    Detach();
                    return;
                }
                if (previous != nullptr)
                    SetWindowLongPtrW(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
            }
            Detach();
            return;
        }

        if (IsWindow(hWnd)) ::DestroyWindow(hWnd);
        if (m_hWnd == hWnd) Detach();
    }

    operator HWND() const noexcept { return m_hWnd; }
    HWND Handle() const noexcept { return m_hWnd; }
    virtual bool IsSplitterWindow() const noexcept { return false; }

    // ---- handle maps ----
    static CWnd* FindAttached(const HWND hWnd)
    {
        return hWnd != nullptr ? static_cast<CWnd*>(GetPropW(hWnd, kProp())) : nullptr;
    }
    static CWnd* FromHandle(HWND hWnd);
    bool Attach(const HWND hWnd)
    {
        if (hWnd == nullptr) return false;
        if (m_hWnd != nullptr) return m_hWnd == hWnd && FindAttached(hWnd) == this;
        if (const CWnd* pExisting = FindAttached(hWnd); pExisting != nullptr && pExisting != this) return false;
        if (!SetPropW(hWnd, kProp(), this)) return false;

        m_hWnd = hWnd;
        m_ownerThreadId = GetWindowThreadProcessId(hWnd, nullptr);
        return true;
    }
    HWND Detach()
    {
        const HWND h = m_hWnd;
        if (h != nullptr && FindAttached(h) == this) RemovePropW(h, kProp());
        m_hWnd = nullptr;
        m_ownerThreadId = 0;
        return h;
    }

    // ---- creation ----
    virtual bool PreCreateWindow(CREATESTRUCT& cs);
    virtual void PostNcDestroy();
    virtual bool PreprocessMessage(MSG*) { return false; }
    virtual void OnFontSizeChanged(int, int) {}
    bool InitializeDialogControls(UINT resourceId);

    bool CreateEx(const DWORD dwExStyle, const LPCWSTR lpszClassName, const LPCWSTR lpszWindowName, const DWORD dwStyle,
        const int x, const int y, const int nWidth, const int nHeight, const HWND hwndParent, const HMENU nIDorHMenu, const LPVOID lpParam = nullptr)
    {
        if (m_hWnd != nullptr) return false;

        CREATESTRUCT cs{};
        cs.dwExStyle = dwExStyle; cs.lpszClass = lpszClassName; cs.lpszName = lpszWindowName;
        cs.style = dwStyle; cs.x = x; cs.y = y; cs.cx = nWidth; cs.cy = nHeight;
        cs.hwndParent = hwndParent; cs.hMenu = nIDorHMenu;
        cs.hInstance = GetAppInstance(); cs.lpCreateParams = lpParam;
        try
        {
            if (!PreCreateWindow(cs))
            {
                PostNcDestroy();
                return false;
            }
        }
        catch (...)
        {
            PostNcDestroy();
            throw;
        }
        const WindowCreationScope createScope(this);
        const HWND h = CreateWindowExW(cs.dwExStyle, cs.lpszClass, cs.lpszName, cs.style,
            cs.x, cs.y, cs.cx, cs.cy, cs.hwndParent, cs.hMenu, cs.hInstance, cs.lpCreateParams);
        if (h == nullptr)
        {
            if (!createScope.m_attached) PostNcDestroy();
            return false;
        }
        // If the window used our FrameworkWindowProc class, the creation hook already attached it.
        // Otherwise it is a system/common-control class — subclass it so our message dispatch runs
        // (this is how MFC routes WM_ERASEBKGND, custom-draw, etc. to control-derived classes).
        if (m_hWnd == nullptr)
        {
            try
            {
                if (SubclassNativeWindow(h))
                {
                    SetFont(GetAppFont(m_hWnd));
                    return true;
                }
            }
            catch (...)
            {
                ::DestroyWindow(h);
                PostNcDestroy();
                throw;
            }

            ::DestroyWindow(h);
            PostNcDestroy();
            return false;
        }
        SetFont(GetAppFont(m_hWnd));
        return true;
    }
    bool CreateEx(const DWORD dwExStyle, const LPCWSTR lpszClassName, const LPCWSTR lpszWindowName, const DWORD dwStyle,
        const RECT& rect, CWnd* pParentWnd, const UINT nID, const LPVOID lpParam = nullptr)
    {
        return CreateEx(dwExStyle, lpszClassName, lpszWindowName, dwStyle,
            rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
            pParentWnd ? pParentWnd->m_hWnd : nullptr, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(nID)), lpParam);
    }
    virtual bool Create(const LPCWSTR lpszClassName, const LPCWSTR lpszWindowName, const DWORD dwStyle,
        const RECT& rect, CWnd* pParentWnd, const UINT nID)
    {
        return CreateEx(0, lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID);
    }

    bool SubclassDlgItem(const int id, CWnd* parent)
    {
        return parent != nullptr && SubclassNativeWindow(::GetDlgItem(parent->m_hWnd, id));
    }

    bool SubclassNativeWindow(const HWND hWnd)
    {
        if (!IsWindow(hWnd)) return false;
        if (m_hWnd != nullptr) return m_hWnd == hWnd && FindAttached(hWnd) == this;
        if (FindAttached(hWnd) != nullptr) return false;
        if (GetPropW(hWnd, kSuperProp()) != nullptr) return false;

        SetLastError(ERROR_SUCCESS);
        const auto old = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hWnd, GWLP_WNDPROC));
        if (old == nullptr || old == FrameworkWindowProc) return false;

        SetLastError(ERROR_SUCCESS);
        const auto previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hWnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(FrameworkWindowProc)));
        if (previous == nullptr)
        {
            if (GetWindowLongPtrW(hWnd, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(FrameworkWindowProc))
                SetWindowLongPtrW(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(old));
            return false;
        }

        if (!SetPropW(hWnd, kSuperProp(), reinterpret_cast<HANDLE>(previous)) ||
            !SetPropW(hWnd, kProp(), this))
        {
            RemovePropW(hWnd, kProp());
            RemovePropW(hWnd, kSuperProp());
            SetWindowLongPtrW(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
            return false;
        }

        m_hWnd = hWnd;
        m_ownerThreadId = GetWindowThreadProcessId(hWnd, nullptr);
        m_pfnSuper = previous;
        return true;

    }

    // ---- message dispatch ----
    LRESULT DispatchWindowMessage(const UINT msg, const WPARAM wParam, const LPARAM lParam) noexcept
    {
        const ScopedValue currentMessage(g_currentMsg, MakeMessageSnapshot(m_hWnd, msg, wParam, lParam));

        LRESULT r = 0;
        try
        {
            if (!RouteWindowMessage(msg, wParam, lParam, &r)) r = DefWindowProc(msg, wParam, lParam);
        }
        catch (...)
        {
            if (msg == WM_PAINT) ValidateRect(m_hWnd, nullptr);
            r = msg == WM_CREATE ? -1 : 0;
        }

        if (msg == WM_NCDESTROY)
        {
            RemovePropW(m_hWnd, kSuperProp());
            Detach();
            m_pfnSuper = nullptr;
            try { PostNcDestroy(); }
            catch (...) {}
        }
        return r;
    }
    bool RouteWindowMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* pResult);
    virtual bool OnCommand(WPARAM wParam, LPARAM lParam);
    virtual bool OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
    bool RouteReflectedCommand(int code);
    bool RouteReflectedNotification(NMHDR* pNMHDR, LRESULT* pResult);

    LRESULT DefWindowProc(const UINT msg, const WPARAM wParam, const LPARAM lParam)
    {
        return m_pfnSuper ? CallWindowProcW(m_pfnSuper, m_hWnd, msg, wParam, lParam) :
            ::DefWindowProcW(m_hWnd, msg, wParam, lParam);
    }
    LRESULT CallDefaultHandler() { return DefWindowProc(g_currentMsg.message, g_currentMsg.wParam, g_currentMsg.lParam); }
    static const MSG& CurrentMessage() noexcept { return g_currentMsg; }

    // ---- common operations ----
    bool DestroyWindow()
    {
        return m_hWnd ? ::DestroyWindow(m_hWnd) : false;
    }
    bool ShowWindow(const int nCmdShow) noexcept { return ::ShowWindow(m_hWnd, nCmdShow); }
    bool UpdateWindow() noexcept { return ::UpdateWindow(m_hWnd); }
    void Invalidate(const bool bErase = true) noexcept { ::InvalidateRect(m_hWnd, nullptr, bErase); }
    void InvalidateRect(const LPCRECT rc, const bool bErase = true) noexcept { ::InvalidateRect(m_hWnd, rc, bErase); }
    bool RedrawWindow(const LPCRECT rc = nullptr, CRgn* pRgn = nullptr, const UINT flags = RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE) noexcept
    {
        return ::RedrawWindow(m_hWnd, rc, pRgn ? static_cast<HRGN>(pRgn->m_hObject) : nullptr, flags);
    }

    bool MoveWindow(const int x, const int y, const int cx, const int cy, const bool bRepaint = true) noexcept { return ::MoveWindow(m_hWnd, x, y, cx, cy, bRepaint); }
    bool MoveWindow(const LPCRECT rc, const bool bRepaint = true) noexcept { return ::MoveWindow(m_hWnd, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top, bRepaint); }
    bool SetWindowPos(const CWnd* pAfter, const int x, const int y, const int cx, const int cy, const UINT flags) noexcept { return ::SetWindowPos(m_hWnd, pAfter ? pAfter->m_hWnd : nullptr, x, y, cx, cy, flags); }

    CRect ClientRect() const noexcept { CRect rect; ::GetClientRect(m_hWnd, &rect); return rect; }
    CRect WindowRect() const noexcept { return CRect(m_hWnd); }
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
    CRect WindowRectInClient(const HWND window) const noexcept { return ToClient(CRect(window)); }
    std::optional<CPoint> ClientCursorPosition() const noexcept
    {
        CPoint point;
        if (!::GetCursorPos(&point) || !::ScreenToClient(m_hWnd, &point)) return std::nullopt;
        return point;
    }
    int ScaleForDpi(const int value) const noexcept { return ::ScaleForDpi(value, m_hWnd); }
    int UnscaleForDpi(const int value) const noexcept { return ::UnscaleForDpi(value, m_hWnd); }

    CWnd* GetParent() const { return FromHandle(::GetParent(m_hWnd)); }
    bool IsChild(const CWnd* pWnd) const noexcept { return pWnd != nullptr && ::IsChild(m_hWnd, pWnd->m_hWnd); }
    CWnd* GetDlgItem(const int nID) const { return FromHandle(::GetDlgItem(m_hWnd, nID)); }
    CWnd* GetWindow(const UINT nCmd) const { return FromHandle(::GetWindow(m_hWnd, nCmd)); }
    CWnd* GetFocus() const { return FromHandle(::GetFocus()); }
    CWnd* ChildWindowFromPoint(const POINT pt) const { return FromHandle(::ChildWindowFromPoint(m_hWnd, pt)); }
    CWnd* ChildWindowFromPoint(const POINT pt, const UINT flags) const { return FromHandle(ChildWindowFromPointEx(m_hWnd, pt, flags)); }
    static CWnd* GetCapture() { return FromHandle(::GetCapture()); }

    LRESULT SendMessage(const UINT msg, const WPARAM wParam = 0, const LPARAM lParam = 0) const noexcept { return ::SendMessageW(m_hWnd, msg, wParam, lParam); }
    template <typename T>
    LRESULT SendMessage(const UINT msg, const WPARAM wParam, T* lParam) const noexcept
    {
        return SendMessage(msg, wParam, reinterpret_cast<LPARAM>(lParam));
    }
    bool PostMessage(const UINT msg, const WPARAM wParam = 0, const LPARAM lParam = 0) const noexcept { return ::PostMessageW(m_hWnd, msg, wParam, lParam); }
    // Like SendMessage but, when we have subclassed this window, calls the previous wndproc
    // On the window's own thread: bypass SendMessage dispatch via CallWindowProcW (fast path).
    // From any other thread: use SendMessage so the owning thread's message loop services it,
    // matching real MFC's CWnd::SendMessage which always goes through ::SendMessage.
    LRESULT SendNativeMessage(const UINT msg, const WPARAM wParam = 0, const LPARAM lParam = 0) const
    {
        return m_pfnSuper && m_ownerThreadId == GetCurrentThreadId() ?
            CallWindowProcW(m_pfnSuper, m_hWnd, msg, wParam, lParam) :
            ::SendMessageW(m_hWnd, msg, wParam, lParam);
    }
    template <typename T>
    LRESULT SendNativeMessage(const UINT msg, const WPARAM wParam, T* lParam) const
    {
        return SendNativeMessage(msg, wParam, reinterpret_cast<LPARAM>(lParam));
    }

    void SetText(const LPCWSTR psz) noexcept { SetWindowTextW(m_hWnd, psz); }
    std::wstring Text() const
    {
        std::wstring text(static_cast<size_t>(GetWindowTextLengthW(m_hWnd)) + 1, L'\0');
        text.resize(static_cast<size_t>(GetWindowTextW(m_hWnd, text.data(), static_cast<int>(text.size()))));
        return text;
    }
    UINT ButtonCheckState(const int id) const noexcept { return IsDlgButtonChecked(m_hWnd, id); }
    bool IsChecked(const int id) const noexcept { return IsDlgButtonChecked(m_hWnd, id) == BST_CHECKED; }
    void SetChecked(const int id, const bool checked) noexcept { CheckDlgButton(m_hWnd, id, checked ? BST_CHECKED : BST_UNCHECKED); }
    std::wstring GetText(const int id) const { if (const CWnd* control = GetDlgItem(id)) return control->Text(); return {}; }
    void SetText(const int id, const std::wstring& text) noexcept { SetDlgItemTextW(m_hWnd, id, text.c_str()); }
    int ComboSelection(const int id) const noexcept { return static_cast<int>(SendDlgItemMessageW(m_hWnd, id, CB_GETCURSEL, 0, 0)); }
    void SetComboSelection(const int id, const int index) noexcept { SendDlgItemMessageW(m_hWnd, id, CB_SETCURSEL, index, 0); }
    int CheckedRadioButton(const int first, const int last) const noexcept
    {
        for (int id = first; id <= last; ++id) if (IsDlgButtonChecked(m_hWnd, id) == BST_CHECKED) return id;
        return 0;
    }
    void SetCheckedRadioButton(const int first, const int last, const int id) noexcept { CheckRadioButton(m_hWnd, first, last, id); }

    bool EnableWindow(const bool bEnable = true) noexcept { return ::EnableWindow(m_hWnd, bEnable); }
    bool IsWindowEnabled() const noexcept { return ::IsWindowEnabled(m_hWnd); }
    bool IsWindowVisible() const noexcept { return ::IsWindowVisible(m_hWnd); }
    bool IsIconic() const noexcept { return ::IsIconic(m_hWnd); }
    bool IsZoomed() const noexcept { return ::IsZoomed(m_hWnd); }
    HWND SetFocus() noexcept { return ::SetFocus(m_hWnd); }
    HWND SetCapture() noexcept { return ::SetCapture(m_hWnd); }
    bool BringWindowToTop() noexcept { return ::BringWindowToTop(m_hWnd); }
    bool SetForegroundWindow() noexcept { return ::SetForegroundWindow(m_hWnd); }

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

    UINT_PTR SetTimer(const UINT_PTR id, const UINT interval) noexcept { return ::SetTimer(m_hWnd, id, interval, nullptr); }

    bool GetWindowPlacement(WINDOWPLACEMENT* p) const noexcept { return ::GetWindowPlacement(m_hWnd, p); }
    bool SetWindowPlacement(const WINDOWPLACEMENT* p) noexcept { return ::SetWindowPlacement(m_hWnd, p); }

    HFONT GetFont() const { return reinterpret_cast<HFONT>(::SendMessageW(m_hWnd, WM_GETFONT, 0, 0)); }
    void SetFont(const HFONT font) { ::SendMessageW(m_hWnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), true); }
    void SetRedraw(const bool bRedraw = true) { ::SendMessageW(m_hWnd, WM_SETREDRAW, static_cast<WPARAM>(bRedraw), 0); }

    bool CopyTextToClipboard(std::wstring_view text) const;

    int  GetDlgCtrlID() const noexcept { return ::GetDlgCtrlID(m_hWnd); }
    HICON SetIcon(HICON hIcon, const bool bBig = true) { return reinterpret_cast<HICON>(SendMessage(WM_SETICON, bBig ? ICON_BIG : ICON_SMALL, hIcon)); }
    static CWnd* GetDesktopWindow() { return FromHandle(::GetDesktopWindow()); }
    bool LockWindowUpdate() noexcept { return ::LockWindowUpdate(m_hWnd); }
    void UnlockWindowUpdate() noexcept { ::LockWindowUpdate(nullptr); }
    bool HideCaret() noexcept { return ::HideCaret(m_hWnd); }

    CMenu* GetMenu() const;

    int GetScrollPos(const int bar) const noexcept { return ::GetScrollPos(m_hWnd, bar); }
    void ShowScrollBar(const UINT bar, const bool bShow = true) noexcept { ::ShowScrollBar(m_hWnd, bar, bShow); }
    bool GetScrollInfo(const int bar, SCROLLINFO* psi, const UINT mask = SIF_ALL) const noexcept { psi->cbSize = sizeof(SCROLLINFO); psi->fMask = mask; return ::GetScrollInfo(m_hWnd, bar, psi); }
    int SetScrollInfo(const int bar, SCROLLINFO* psi, const bool bRedraw = true)
    {
        psi->cbSize = sizeof(SCROLLINFO); return ::SetScrollInfo(m_hWnd, bar, psi, bRedraw);
    }

    void CenterWindow(CWnd* pAlternate = nullptr);

    // ---- default message handlers (call CallDefaultHandler()) ----
    int  OnCreate(LPCREATESTRUCT) { return static_cast<int>(CallDefaultHandler()); }
    void OnDestroy() { CallDefaultHandler(); }
    void OnPaint() { CallDefaultHandler(); }
    void OnClose() { CallDefaultHandler(); }
    void OnNcPaint() { CallDefaultHandler(); }
    void OnSize(UINT, int, int) { CallDefaultHandler(); }
    bool OnEraseBkgnd(CDC*) { return static_cast<bool>(CallDefaultHandler()); }
    void OnLButtonDown(UINT, CPoint) { CallDefaultHandler(); }
    void OnLButtonUp(UINT, CPoint) { CallDefaultHandler(); }
    void OnLButtonDblClk(UINT, CPoint) { CallDefaultHandler(); }
    void OnMButtonDown(UINT, CPoint) { CallDefaultHandler(); }
    void OnMouseMove(UINT, CPoint) { CallDefaultHandler(); }
    void OnMouseLeave() { CallDefaultHandler(); }
    bool OnMouseWheel(UINT, short, CPoint) { return static_cast<bool>(CallDefaultHandler()); }
    void OnKeyDown(UINT, UINT, UINT) { CallDefaultHandler(); }
    void OnChar(UINT, UINT, UINT) { CallDefaultHandler(); }
    void OnSetFocus(CWnd*) { CallDefaultHandler(); }
    void OnKillFocus(CWnd*) { CallDefaultHandler(); }
    void OnContextMenu(CWnd*, CPoint) { CallDefaultHandler(); }
    void OnTimer(UINT_PTR) { CallDefaultHandler(); }
    void OnInitMenuPopup(CMenu*, UINT, bool) { CallDefaultHandler(); }
    void OnSysColorChange() { CallDefaultHandler(); }
    UINT OnPowerBroadcast(UINT, LPARAM) { return static_cast<UINT>(CallDefaultHandler()); }
    bool OnNcActivate(bool) { return static_cast<bool>(CallDefaultHandler()); }
    HBRUSH OnCtlColor(CDC*, CWnd*, UINT) { return reinterpret_cast<HBRUSH>(CallDefaultHandler()); }
    LRESULT OnNcHitTest(CPoint) { return CallDefaultHandler(); }
    void OnGetMinMaxInfo(MINMAXINFO*) { CallDefaultHandler(); }
    void OnEnable(bool) { CallDefaultHandler(); }
    bool OnSetCursor(CWnd*, UINT, UINT) { return static_cast<bool>(CallDefaultHandler()); }
    void OnActivateApp(bool, DWORD) { CallDefaultHandler(); }
    void OnCaptureChanged(CWnd*) { CallDefaultHandler(); }
    void OnSettingChange(UINT, LPCTSTR) { CallDefaultHandler(); }
    void OnShowWindow(bool, UINT) { CallDefaultHandler(); }
    void OnHScroll(UINT, UINT, CWnd*) { CallDefaultHandler(); }
    void OnVScroll(UINT, UINT, CWnd*) { CallDefaultHandler(); }
    UINT OnGetDlgCode() { return static_cast<UINT>(CallDefaultHandler()); }
    void OnNcCalcSize(bool, NCCALCSIZE_PARAMS*) { CallDefaultHandler(); }
    virtual int OnMouseActivate(CWnd*, UINT, UINT) { return static_cast<int>(CallDefaultHandler()); }
    bool IsTopParentActive() const
    {
        HWND hWnd = m_hWnd;
        while (hWnd != nullptr && (GetWindowLongW(hWnd, GWL_STYLE) & WS_CHILD))
            hWnd = ::GetParent(hWnd);
        return hWnd != nullptr && hWnd == GetActiveWindow();
    }

    // owner-draw reflection targets
    virtual void DrawItem(LPDRAWITEMSTRUCT) {}
    // docked-bar sizing (frame layout queries this)
    virtual CSize PreferredSize() { return WindowRect().Size(); }

protected:
    WNDPROC m_pfnSuper = nullptr;
    DWORD   m_ownerThreadId = 0;
    static LPCWSTR kProp() { return L"_WdsShimCWnd"; }
    static LPCWSTR kSuperProp() { return L"_WdsShimSuperProc"; }
    friend LRESULT CALLBACK FrameworkWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

template<typename Handle, typename Object>
struct TemporaryHandleCache final
{
    Handle mruKey = nullptr;
    Object* mruVal = nullptr;
    std::unordered_map<Handle, Object> map;

    ~TemporaryHandleCache() { Clear(); }

    template<typename Initialize>
    Object* Get(const Handle handle, Initialize&& initialize)
    {
        if (handle != mruKey)
        {
            mruVal = &map.try_emplace(handle).first->second;
            mruKey = handle;
        }
        std::invoke(std::forward<Initialize>(initialize), *mruVal, handle);
        return mruVal;
    }

    void Clear()
    {
        mruKey = nullptr;
        mruVal = nullptr;
        for (auto& value : map | std::views::values) value.Detach();
        map.clear();
    }
};

inline TemporaryHandleCache<HWND, CWnd>& WindowHandleCache()
{
    thread_local TemporaryHandleCache<HWND, CWnd> cache;
    return cache;
}

inline CWnd* CWnd::FromHandle(const HWND hWnd)
{
    if (hWnd == nullptr) return nullptr;
    if (CWnd* window = FindAttached(hWnd)) return window;
    return WindowHandleCache().Get(hWnd,
        [](CWnd& window, const HWND handle) { window.m_hWnd = handle; });
}

// Global scope helpers expected by the codebase that take CWnd via HWND.

// ---- Device-context wrappers tied to a window ----
class CClientDC final : public CDC
{
public:
    explicit CClientDC(CWnd* window)
        : CDC(GetDC(window ? window->m_hWnd : nullptr), false), m_hWndDC(window ? window->m_hWnd : nullptr) {}
    ~CClientDC() { if (m_hDC) ReleaseDC(m_hWndDC, Detach()); }
private:
    HWND m_hWndDC;
};
class CWindowDC final : public CDC
{
public:
    explicit CWindowDC(CWnd* window)
        : CDC(GetWindowDC(window ? window->m_hWnd : nullptr), false), m_hWndDC(window ? window->m_hWnd : nullptr) {}
    ~CWindowDC() { if (m_hDC) ReleaseDC(m_hWndDC, Detach()); }
private:
    HWND m_hWndDC;
};
class CPaintDC final : public CDC
{
public:
    PAINTSTRUCT m_ps{};
    explicit CPaintDC(CWnd* pWnd) : m_hWndDC(pWnd ? pWnd->m_hWnd : nullptr) { Attach(BeginPaint(m_hWndDC, &m_ps)); }
    ~CPaintDC() { if (m_hDC) { EndPaint(m_hWndDC, &m_ps); Detach(); } }
private:
    HWND m_hWndDC;
};

// -----------------------------------------------------------------------------
//  CMenu
// -----------------------------------------------------------------------------
class CMenu final
{
public:
    HMENU m_hMenu = nullptr;

    CMenu() = default;
    ~CMenu();

    CMenu(const CMenu&) = delete;
    CMenu& operator=(const CMenu&) = delete;
    CMenu(CMenu&& other) noexcept;
    CMenu& operator=(CMenu&& other) noexcept;

    explicit operator bool() const noexcept;
    HMENU Handle() const noexcept { return m_hMenu; }
    HMENU Detach() noexcept;

    enum class ItemLookup { Position, Command };

    static CMenu CreatePopup() noexcept;
    static CMenu LoadResource(UINT id) noexcept;
    static CMenu* FromHandle(HMENU menu);

    int ItemCount() const noexcept;
    UINT ItemIdAt(int pos) const noexcept;
    UINT ItemState(UINT id, UINT flags) const noexcept;
    std::wstring ItemTextAt(UINT pos) const;
    CMenu* SubmenuAt(int pos) const;
    bool Append(UINT flags, UINT_PTR id = 0, LPCWSTR psz = nullptr) noexcept;
    bool Modify(UINT pos, UINT flags, UINT_PTR id, LPCWSTR psz) noexcept;
    bool Remove(UINT pos, UINT flags) noexcept;
    UINT EnableItem(UINT id, UINT flags) noexcept;
    UINT CheckItem(UINT id, UINT flags) noexcept;
    bool SetDefaultItem(UINT item) noexcept;
    bool GetItemInfo(UINT item, MENUITEMINFOW* info, ItemLookup lookup = ItemLookup::Position) const;
    bool SetItemInfo(UINT item, const MENUITEMINFOW* info, ItemLookup lookup = ItemLookup::Position);
    UINT ShowPopup(UINT flags, int x, int y, CWnd* pWnd) const;
    UINT ShowPopupEx(UINT flags, int x, int y, CWnd* pWnd, LPTPMPARAMS lptpm = nullptr) const;
    void SetItemEnabled(int item, bool enable, ItemLookup lookup = ItemLookup::Position);
    bool IsItemEnabled(UINT item, ItemLookup lookup = ItemLookup::Position) const noexcept;

private:
    explicit CMenu(HMENU menu) noexcept;
    bool m_bAutoDestroy = false;
};

void ClearTemporaryHandleCaches();

// -----------------------------------------------------------------------------
//  Type-safe message handlers (defined now that CDC/CWnd/CMenu exist)
// -----------------------------------------------------------------------------
template<typename T> struct MemberFunctionTraits;
template<typename Return, typename Class, typename... Args>
struct MemberFunctionTraits<Return(Class::*)(Args...)>
{
    using ClassType = Class;
    using Signature = Return(Args...);
};
template<typename Return, typename Class, typename... Args>
struct MemberFunctionTraits<Return(Class::*)(Args...) const>
    : MemberFunctionTraits<Return(Class::*)(Args...)>
{};
template<typename Return, typename Class, typename... Args>
struct MemberFunctionTraits<Return(Class::*)(Args...) noexcept>
    : MemberFunctionTraits<Return(Class::*)(Args...)>
{};
template<typename Return, typename Class, typename... Args>
struct MemberFunctionTraits<Return(Class::*)(Args...) const noexcept>
    : MemberFunctionTraits<Return(Class::*)(Args...)>
{};

template<typename> inline constexpr bool InvalidMessageHandler = false;

template<auto Handler>
LRESULT InvokeCommandHandler(CCmdTarget& target, UINT, const WPARAM wParam, LPARAM, bool&)
{
    using Traits = MemberFunctionTraits<decltype(Handler)>;
    using Class = Traits::ClassType;
    using Signature = Traits::Signature;
    auto& object = static_cast<Class&>(target);

    if constexpr (std::is_same_v<Signature, void()>) std::invoke(Handler, object);
    else if constexpr (std::is_same_v<Signature, void(UINT)>) std::invoke(Handler, object, static_cast<UINT>(wParam));
    else static_assert(InvalidMessageHandler<Signature>, "Unsupported command-handler signature");
    return 0;
}

template<auto Handler>
LRESULT InvokeUpdateHandler(CCmdTarget& target, UINT, WPARAM, const LPARAM lParam, bool&)
{
    using Class = MemberFunctionTraits<decltype(Handler)>::ClassType;
    static_assert(std::is_same_v<typename MemberFunctionTraits<decltype(Handler)>::Signature, void(CCmdUI*)>);
    std::invoke(Handler, static_cast<Class&>(target), reinterpret_cast<CCmdUI*>(lParam));
    return 0;
}

template<auto Handler>
LRESULT InvokeNotifyHandler(CCmdTarget& target, UINT, WPARAM, const LPARAM lParam, bool& handled)
{
    using Traits = MemberFunctionTraits<decltype(Handler)>;
    using Class = Traits::ClassType;
    using Signature = Traits::Signature;
    auto& object = static_cast<Class&>(target);
    auto* notify = reinterpret_cast<NotificationRoute*>(lParam);

    if constexpr (std::is_same_v<Signature, void(NMHDR*, LRESULT*)>)
        std::invoke(Handler, object, notify->pNMHDR, notify->pResult);
    else if constexpr (std::is_same_v<Signature, void(UINT, NMHDR*, LRESULT*)>)
        std::invoke(Handler, object, notify->id, notify->pNMHDR, notify->pResult);
    else if constexpr (std::is_same_v<Signature, bool(UINT, NMHDR*, LRESULT*)>)
        handled = std::invoke(Handler, object, notify->id, notify->pNMHDR, notify->pResult);
    else
        static_assert(InvalidMessageHandler<Signature>, "Unsupported notification-handler signature");
    return 0;
}

template<auto Handler>
LRESULT InvokeWindowHandler(CCmdTarget& target, const UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    using Traits = MemberFunctionTraits<decltype(Handler)>;
    using Class = Traits::ClassType;
    using Signature = Traits::Signature;
    auto& object = static_cast<Class&>(target);
    LRESULT result = 0;

    if constexpr (std::is_same_v<Signature, void()>)
        std::invoke(Handler, object);
    else if constexpr (std::is_same_v<Signature, int(LPCREATESTRUCT)>)
        result = std::invoke(Handler, object, reinterpret_cast<LPCREATESTRUCT>(lParam));
    else if constexpr (std::is_same_v<Signature, void(UINT, int, int)>)
        std::invoke(Handler, object, static_cast<UINT>(wParam), static_cast<int>(LOWORD(lParam)),
            static_cast<int>(HIWORD(lParam)));
    else if constexpr (std::is_same_v<Signature, bool(CDC*)>)
    {
        auto dc = CDC::Borrow(reinterpret_cast<HDC>(wParam));
        result = std::invoke(Handler, object, &dc);
    }
    else if constexpr (std::is_same_v<Signature, void(UINT, CPoint)>)
        std::invoke(Handler, object, static_cast<UINT>(wParam),
            CPoint(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))));
    else if constexpr (std::is_same_v<Signature, bool(UINT, short, CPoint)>)
    {
        const bool handledResult = std::invoke(Handler, object, LOWORD(wParam), static_cast<short>(HIWORD(wParam)),
            CPoint(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))));
        handled = handledResult;
        result = handledResult;
    }
    else if constexpr (std::is_same_v<Signature, void(UINT, UINT, UINT)>)
        std::invoke(Handler, object, static_cast<UINT>(wParam), static_cast<UINT>(lParam & 0xFFFF),
            static_cast<UINT>((lParam >> 16) & 0xFFFF));
    else if constexpr (std::is_same_v<Signature, void(CWnd*)>)
    {
        const auto hwnd = reinterpret_cast<HWND>(message == WM_CAPTURECHANGED ? lParam : wParam);
        std::invoke(Handler, object, CWnd::FromHandle(hwnd));
    }
    else if constexpr (std::is_same_v<Signature, void(CWnd*, CPoint)>)
        std::invoke(Handler, object, CWnd::FromHandle(reinterpret_cast<HWND>(wParam)),
            CPoint(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))));
    else if constexpr (std::is_same_v<Signature, void(UINT_PTR)>)
        std::invoke(Handler, object, static_cast<UINT_PTR>(wParam));
    else if constexpr (std::is_same_v<Signature, void(CMenu*, UINT, bool)>)
        std::invoke(Handler, object, CMenu::FromHandle(reinterpret_cast<HMENU>(wParam)),
            static_cast<UINT>(LOWORD(lParam)), static_cast<bool>(HIWORD(lParam)));
    else if constexpr (std::is_same_v<Signature, UINT(UINT, LPARAM)>)
        result = static_cast<LRESULT>(std::invoke(Handler, object, static_cast<UINT>(wParam), lParam));
    else if constexpr (std::is_same_v<Signature, bool(bool)>)
        result = static_cast<LRESULT>(std::invoke(Handler, object, static_cast<bool>(wParam)));
    else if constexpr (std::is_same_v<Signature, HBRUSH(CDC*, CWnd*, UINT)>)
    {
        auto dc = CDC::Borrow(reinterpret_cast<HDC>(wParam));
        const HBRUSH brush = std::invoke(Handler, object, &dc,
            CWnd::FromHandle(reinterpret_cast<HWND>(lParam)), message - WM_CTLCOLORMSGBOX);
        if (brush == nullptr) handled = false;
        result = reinterpret_cast<LRESULT>(brush);
    }
    else if constexpr (std::is_same_v<Signature, LRESULT(CPoint)>)
        result = std::invoke(Handler, object,
            CPoint(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))));
    else if constexpr (std::is_same_v<Signature, void(MINMAXINFO*)>)
        std::invoke(Handler, object, reinterpret_cast<MINMAXINFO*>(lParam));
    else if constexpr (std::is_same_v<Signature, void(bool)>)
        std::invoke(Handler, object, static_cast<bool>(wParam));
    else if constexpr (std::is_same_v<Signature, bool(CWnd*, UINT, UINT)>)
    {
        const bool handledResult = std::invoke(Handler, object, CWnd::FromHandle(reinterpret_cast<HWND>(wParam)),
            LOWORD(lParam), HIWORD(lParam));
        handled = handledResult;
        result = handledResult;
    }
    else if constexpr (std::is_same_v<Signature, void(bool, DWORD)>)
        std::invoke(Handler, object, static_cast<bool>(wParam), static_cast<DWORD>(lParam));
    else if constexpr (std::is_same_v<Signature, void(bool, UINT)>)
        std::invoke(Handler, object, static_cast<bool>(wParam), static_cast<UINT>(lParam));
    else if constexpr (std::is_same_v<Signature, void(UINT, LPCTSTR)>)
        std::invoke(Handler, object, static_cast<UINT>(wParam), reinterpret_cast<LPCTSTR>(lParam));
    else if constexpr (std::is_same_v<Signature, void(UINT, UINT, CWnd*)>)
    {
        CWnd* scrollBar = lParam ? CWnd::FromHandle(reinterpret_cast<HWND>(lParam)) : nullptr;
        std::invoke(Handler, object, LOWORD(wParam), HIWORD(wParam), scrollBar);
    }
    else if constexpr (std::is_same_v<Signature, UINT()>)
        result = static_cast<LRESULT>(std::invoke(Handler, object));
    else if constexpr (std::is_same_v<Signature, void(bool, NCCALCSIZE_PARAMS*)>)
        std::invoke(Handler, object, static_cast<bool>(wParam), reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam));
    else if constexpr (std::is_same_v<Signature, int(CWnd*, UINT, UINT)>)
        result = static_cast<LRESULT>(std::invoke(Handler, object,
            CWnd::FromHandle(reinterpret_cast<HWND>(wParam)), LOWORD(lParam), HIWORD(lParam)));
    else if constexpr (std::is_same_v<Signature, LRESULT(WPARAM, LPARAM)>)
        result = std::invoke(Handler, object, wParam, lParam);
    else
        static_assert(InvalidMessageHandler<Signature>, "Unsupported window-message handler signature");

    return result;
}

namespace Route
{
    template<auto Handler>
    constexpr RouteEntry Control(UINT code, UINT firstId, UINT lastId);

    template<auto Handler>
    constexpr RouteEntry Command(const UINT firstId, const UINT lastId)
    {
        return Control<Handler>(static_cast<UINT>(CommandCode), firstId, lastId);
    }

    template<auto Handler>
    constexpr RouteEntry Command(const UINT id)
    {
        return Command<Handler>(id, id);
    }

    template<auto Handler>
    constexpr RouteEntry Update(const UINT firstId, const UINT lastId)
    {
        return { WM_COMMAND, static_cast<UINT>(UpdateCode), firstId, lastId,
            &InvokeUpdateHandler<Handler>, nullptr };
    }

    template<auto Handler>
    constexpr RouteEntry Update(const UINT id)
    {
        return Update<Handler>(id, id);
    }

    template<auto Handler>
    constexpr RouteEntry Control(const UINT code, const UINT firstId, const UINT lastId)
    {
        return { WM_COMMAND, code, firstId, lastId, &InvokeCommandHandler<Handler>, nullptr };
    }

    template<auto Handler>
    constexpr RouteEntry Control(const UINT code, const UINT id)
    {
        return Control<Handler>(code, id, id);
    }

    template<auto Handler>
    constexpr RouteEntry Window(const UINT message)
    {
        return { message, 0, 0, 0, &InvokeWindowHandler<Handler>, nullptr };
    }

    template<auto Handler>
    constexpr RouteEntry Registered(const UINT& message)
    {
        return { 0, 0, 0, 0, &InvokeWindowHandler<Handler>, &message };
    }

    template<auto Handler>
    constexpr RouteEntry Notify(const UINT code, const UINT firstId, const UINT lastId)
    {
        return { WM_NOTIFY, code, firstId, lastId, &InvokeNotifyHandler<Handler>, nullptr };
    }

    template<auto Handler>
    constexpr RouteEntry Notify(const UINT code, const UINT id)
    {
        return Notify<Handler>(code, id, id);
    }

    template<auto Handler>
    constexpr RouteEntry ReflectNotify(const UINT code)
    {
        return Notify<Handler>(code, ReflectedId, ReflectedId);
    }

    template<auto Handler>
    constexpr RouteEntry ReflectControl(const UINT code)
    {
        return Control<Handler>(code, ReflectedId, ReflectedId);
    }
}

// -----------------------------------------------------------------------------
//  Dispatch implementations
// -----------------------------------------------------------------------------
inline bool CCmdTarget::RouteCommand(const UINT nID, const int nCode, void* pExtra, const bool execute)
{
    for (const RouteTable* table = GetRouteTable(); table != nullptr; table = table->base)
    {
        for (const RouteEntry& entry : table->entries)
        {
            if (entry.message != WM_COMMAND) continue;
            if (entry.firstId == ReflectedId) continue;
            if (entry.code != static_cast<UINT>(nCode)) continue;
            if (nID < entry.firstId || nID > entry.lastId) continue;
            if (!execute) return true;
            bool bHandled = true;
            entry.invoke(*this, WM_COMMAND, static_cast<WPARAM>(nID), reinterpret_cast<LPARAM>(pExtra), bHandled);
            if (bHandled) return true;
        }
    }
    return false;
}

inline bool CWnd::RouteWindowMessage(const UINT msg, const WPARAM wParam, const LPARAM lParam, LRESULT* pResult)
{
    if (msg == WM_COMMAND)
    {
        if (OnCommand(wParam, lParam)) { if (pResult) *pResult = 0; return true; }
        return false;
    }
    if (msg == WM_NOTIFY)
    {
        if (LRESULT r = 0; OnNotify(wParam, lParam, &r))
        { if (pResult) *pResult = r; return true; }
        return false;
    }
    if (msg == WM_DRAWITEM)
    {
        if (auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam); dis->CtlType != ODT_MENU && dis->hwndItem)
            if (CWnd* pChild = FindAttached(dis->hwndItem))
            {
                // Publish item rect so GetItemRect/GetSubItemRect can avoid SendMessage.
                DrawItemContext context{};
                context.hWnd = dis->hwndItem;
                context.item = static_cast<int>(dis->itemID);
                context.rcItem = dis->rcItem;
                const ScopedValue drawContext(g_drawItemCtx, context);
                pChild->DrawItem(dis);
                if (pResult) *pResult = true; return true;
            }
    }

    if (msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC)
    {
        for (const RouteTable* table = GetRouteTable(); table != nullptr; table = table->base)
            for (const RouteEntry& entry : table->entries)
            {
                if (entry.message != WM_CTLCOLOR) continue;
                bool b = true;
                if (const LRESULT r = entry.invoke(*this, msg, wParam, lParam, b); b)
                { if (pResult) *pResult = r; return true; }
            }
        return false;
    }
    for (const RouteTable* table = GetRouteTable(); table != nullptr; table = table->base)
        for (const RouteEntry& entry : table->entries)
        {
            bool match;
            if (entry.registeredMessage) match = (*entry.registeredMessage != 0 && *entry.registeredMessage == msg);
            else
                match = entry.message == msg && entry.message != WM_COMMAND && entry.message != WM_NOTIFY &&
                entry.message != WM_CTLCOLOR;
            if (!match) continue;
            bool b = true;
            if (const LRESULT r = entry.invoke(*this, msg, wParam, lParam, b); b)
            { if (pResult) *pResult = r; return true; }
        }
    return false;
}

inline bool CWnd::OnCommand(const WPARAM wParam, const LPARAM lParam)
{
    const UINT nID = LOWORD(wParam);
    const auto hWndCtrl = reinterpret_cast<HWND>(lParam);
    const int nCode = hWndCtrl == nullptr ? CommandCode : HIWORD(wParam);
    if (hWndCtrl != nullptr)
    {
        if (CWnd* pCtrl = FindAttached(hWndCtrl);
            pCtrl && pCtrl != this && pCtrl->RouteReflectedCommand(nCode)) return true;
    }
    if (nID == 0) return false;

    if (hWndCtrl == nullptr)
    {
        struct CCommandState final : CCmdUI
        {
            bool m_bEnabled = true;
            void Enable(const bool bOn) override
            {
                m_bEnableChanged = true;
                m_bEnabled = bOn;
            }
        } state;
        state.m_nID = nID;
        if (RouteCommand(nID, UpdateCode, &state) &&
            state.m_bEnableChanged && !state.m_bEnabled)
        {
            return true;
        }
    }

    if (RouteCommand(nID, nCode, nullptr)) return true;

    return false;
}

inline bool CWnd::RouteReflectedCommand(const int code)
{
    for (const RouteTable* table = GetRouteTable(); table != nullptr; table = table->base)
        for (const RouteEntry& entry : table->entries)
            if (entry.message == WM_COMMAND && entry.firstId == ReflectedId && std::cmp_equal(entry.code, code))
            {
                bool b = true; entry.invoke(*this, WM_COMMAND, 0, 0, b); if (b) return true;
            }
    return false;
}

inline bool CWnd::OnNotify(WPARAM, const LPARAM lParam, LRESULT* pResult)
{
    const auto pNMHDR = reinterpret_cast<NMHDR*>(lParam);
    if (pNMHDR == nullptr) return false;
    if (pNMHDR->hwndFrom)
    {
        if (CWnd* pCtrl = FindAttached(pNMHDR->hwndFrom);
            pCtrl && pCtrl != this && pCtrl->RouteReflectedNotification(pNMHDR, pResult)) return true;
    }
    NotificationRoute n{ pNMHDR, pResult, static_cast<UINT>(pNMHDR->idFrom) };
    for (const RouteTable* table = GetRouteTable(); table != nullptr; table = table->base)
        for (const RouteEntry& entry : table->entries)
        {
            if (entry.message != WM_NOTIFY || entry.firstId == ReflectedId) continue;
            if (entry.code != pNMHDR->code) continue;
            const UINT idFrom = static_cast<UINT>(pNMHDR->idFrom);
            if (idFrom < entry.firstId || idFrom > entry.lastId) continue;
            bool b = true;
            entry.invoke(*this, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&n), b);
            if (b) return true;
        }
    return false;
}

inline bool CWnd::RouteReflectedNotification(NMHDR* pNMHDR, LRESULT* pResult)
{
    NotificationRoute n{ pNMHDR, pResult, static_cast<UINT>(pNMHDR->idFrom) };
    for (const RouteTable* table = GetRouteTable(); table != nullptr; table = table->base)
        for (const RouteEntry& entry : table->entries)
            if (entry.message == WM_NOTIFY && entry.firstId == ReflectedId && entry.code == pNMHDR->code)
            {
                bool b = true; entry.invoke(*this, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&n), b); if (b) return true;
            }
    return false;
}

inline LRESULT CALLBACK FrameworkWindowProc(const HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
    CWnd* pWnd = CWnd::FindAttached(hWnd);
    if (pWnd == nullptr && msg == WM_NCCREATE && g_pWndInit != nullptr && !g_pWndInit->m_attached &&
        g_pWndInit->m_pWnd->m_hWnd == nullptr)
    {
        pWnd = g_pWndInit->m_pWnd;
        if (!pWnd->Attach(hWnd)) return false;
        g_pWndInit->m_attached = true;
    }
    if (pWnd == nullptr)
    {
        const auto super = reinterpret_cast<WNDPROC>(GetPropW(hWnd, CWnd::kSuperProp()));
        if (super == nullptr) return DefWindowProcW(hWnd, msg, wParam, lParam);

        const LRESULT result = CallWindowProcW(super, hWnd, msg, wParam, lParam);
        if (msg == WM_NCDESTROY) RemovePropW(hWnd, CWnd::kSuperProp());
        return result;
    }
    return pWnd->DispatchWindowMessage(msg, wParam, lParam);
}

inline bool CWnd::PreCreateWindow(CREATESTRUCT& cs)
{
    if (cs.lpszClass == nullptr)
    {
        static const LPCWSTR defaultClass = RegisterWindowClass(CS_DBLCLKS);
        cs.lpszClass = defaultClass;
    }
    return true;
}
inline void CWnd::PostNcDestroy() {}

// -----------------------------------------------------------------------------
//  Standard controls
// -----------------------------------------------------------------------------
class CStatic : public CWnd
{
public:
    bool Create(const LPCWSTR lpszText, const DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, const UINT nID = 0xffff)
    {
        return CreateEx(0, WC_STATICW, lpszText, dwStyle, rect, pParentWnd, nID);
    }
    HICON   SetIcon(HICON hIcon) { return reinterpret_cast<HICON>(SendNativeMessage(STM_SETICON, reinterpret_cast<WPARAM>(hIcon))); }
};

class CButton : public CWnd
{
public:
    bool Create(const LPCWSTR lpszCaption, const DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, const UINT nID)
    {
        return CreateEx(0, WC_BUTTONW, lpszCaption, dwStyle, rect, pParentWnd, nID);
    }
    void SetCheck(const int nCheck) { SendNativeMessage(BM_SETCHECK, static_cast<WPARAM>(nCheck)); }
    HICON SetIcon(HICON hIcon) { return reinterpret_cast<HICON>(SendNativeMessage(BM_SETIMAGE, IMAGE_ICON, hIcon)); }
};

class CEdit : public CWnd
{
public:
    bool Create(const DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, const UINT nID)
    {
        return CreateEx(0, WC_EDITW, nullptr, dwStyle, rect, pParentWnd, nID);
    }
    void SetSel(const int start, const int end) { SendNativeMessage(EM_SETSEL, static_cast<WPARAM>(start), static_cast<LPARAM>(end)); }
    std::pair<int, int> Selection() const { const DWORD selection = static_cast<DWORD>(SendNativeMessage(EM_GETSEL)); return { LOWORD(selection), HIWORD(selection) }; }
};

class CComboBox final : public CWnd
{
public:
    bool Create(const DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, const UINT nID)
    {
        return CreateEx(0, WC_COMBOBOXW, nullptr, dwStyle, rect, pParentWnd, nID);
    }
    int  AddString(LPCWSTR psz) { return static_cast<int>(SendNativeMessage(CB_ADDSTRING, 0, psz)); }
    int  DeleteString(const UINT i) { return static_cast<int>(SendNativeMessage(CB_DELETESTRING, static_cast<WPARAM>(i))); }
    int  GetCount() const { return static_cast<int>(SendNativeMessage(CB_GETCOUNT)); }
    int  GetCurSel() const { return static_cast<int>(SendNativeMessage(CB_GETCURSEL)); }
    int  SetCurSel(const int i) { return static_cast<int>(SendNativeMessage(CB_SETCURSEL, static_cast<WPARAM>(i))); }
    DWORD_PTR GetItemData(const int i) const { return static_cast<DWORD_PTR>(SendNativeMessage(CB_GETITEMDATA, static_cast<WPARAM>(i))); }
    int  SetItemData(const int i, const DWORD_PTR data) { return static_cast<int>(SendNativeMessage(CB_SETITEMDATA, static_cast<WPARAM>(i), static_cast<LPARAM>(data))); }
    bool GetDroppedState() const { return static_cast<bool>(SendNativeMessage(CB_GETDROPPEDSTATE)); }
    std::wstring ItemText(const int i) const
    {
        const int length = static_cast<int>(SendNativeMessage(CB_GETLBTEXTLEN, static_cast<WPARAM>(i)));
        if (length == CB_ERR) return {};
        std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
        const int copied = static_cast<int>(SendNativeMessage(CB_GETLBTEXT, static_cast<WPARAM>(i), text.data()));
        text.resize(static_cast<std::size_t>(std::max(copied, 0)));
        return text;
    }
};

class CListBox final : public CWnd
{
public:
    int  AddString(LPCWSTR psz) { return static_cast<int>(SendNativeMessage(LB_ADDSTRING, 0, psz)); }
    int  InsertString(const int i, LPCWSTR psz) { return static_cast<int>(SendNativeMessage(LB_INSERTSTRING, static_cast<WPARAM>(i), psz)); }
    int  DeleteString(const UINT i) { return static_cast<int>(SendNativeMessage(LB_DELETESTRING, static_cast<WPARAM>(i))); }
    int  GetCurSel() const { return static_cast<int>(SendNativeMessage(LB_GETCURSEL)); }
    int  SetCurSel(const int i) { return static_cast<int>(SendNativeMessage(LB_SETCURSEL, static_cast<WPARAM>(i))); }

};

class CProgressCtrl : public CWnd
{
public:
    bool Create(const DWORD dwStyle, const RECT& rect, CWnd* pParent, const UINT nID)
    {
        return CreateEx(0, PROGRESS_CLASSW, nullptr, dwStyle, rect, pParent, nID);
    }
    int  SetPos(const int n) { return static_cast<int>(SendNativeMessage(PBM_SETPOS, static_cast<WPARAM>(n))); }
    int  GetPos() const { return static_cast<int>(SendNativeMessage(PBM_GETPOS)); }
    std::pair<int, int> Range() const { PBRANGE range{}; SendNativeMessage(PBM_GETRANGE, true, &range); return { range.iLow, range.iHigh }; }
    COLORREF SetBkColor(const COLORREF c) { return static_cast<COLORREF>(SendNativeMessage(PBM_SETBKCOLOR, 0, static_cast<LPARAM>(c))); }
    void SetMarquee(const bool on, const int ms) { SendNativeMessage(PBM_SETMARQUEE, static_cast<WPARAM>(on), static_cast<LPARAM>(ms)); }
};

class CSliderCtrl final : public CWnd
{
public:
    void SetRange(const int nMin, const int nMax, const bool bRedraw = false) { SendNativeMessage(TBM_SETRANGE, static_cast<WPARAM>(bRedraw), MAKELPARAM(nMin, nMax)); }
    int  GetPos() const { return static_cast<int>(SendNativeMessage(TBM_GETPOS)); }
    void SetPos(const int n) { SendNativeMessage(TBM_SETPOS, true, static_cast<LPARAM>(n)); }
    void SetPageSize(const int n) { SendNativeMessage(TBM_SETPAGESIZE, 0, static_cast<LPARAM>(n)); }
};

class CRichEditCtrl final : public CWnd
{
public:
    bool Create(const DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, const UINT nID)
    {
        static const HMODULE richEdit = LoadLibraryW(L"Msftedit.dll");
        if (richEdit == nullptr) return false;
        return CreateEx(0, MSFTEDIT_CLASS, nullptr, dwStyle, rect, pParentWnd, nID);
    }
    DWORD SetEventMask(const DWORD mask) { return static_cast<DWORD>(SendNativeMessage(EM_SETEVENTMASK, 0, static_cast<LPARAM>(mask))); }
    void  SetSel(const long s, const long e)
    {
        CHARRANGE cr{ s, e }; SendNativeMessage(EM_EXSETSEL, 0, &cr);
    }
    void  SetBackgroundColor(const COLORREF color) { SendNativeMessage(EM_SETBKGNDCOLOR, false, static_cast<LPARAM>(color)); }
    void  EnableAutoUrlDetection() { SendNativeMessage(EM_AUTOURLDETECT, true); }
    DWORD SetOptions(const WORD op, const DWORD mask) { return static_cast<DWORD>(SendNativeMessage(EM_SETOPTIONS, op, static_cast<LPARAM>(mask))); }
    void  HideSelection() { SendNativeMessage(EM_HIDESELECTION, true, false); }
    bool  SetDefaultCharFormat(CHARFORMAT2W& cf) { return static_cast<bool>(SendNativeMessage(EM_SETCHARFORMAT, SCF_DEFAULT, &cf)); }
    std::wstring TextRange(const long cpMin, const long cpMax) const
    {
        std::wstring text(static_cast<std::size_t>(std::max(0L, cpMax - cpMin)) + 1, L'\0');
        TEXTRANGEW range{ .chrg = { cpMin, cpMax }, .lpstrText = text.data() };
        const long copied = static_cast<long>(SendNativeMessage(EM_GETTEXTRANGE, 0, &range));
        text.resize(static_cast<std::size_t>(std::max(0L, copied)));
        return text;
    }
};

class CToolTipCtrl final : public CWnd
{
public:
    bool Create(CWnd* pParentWnd)
    {
        return CreateEx(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
            CRect(), pParentWnd, 0);
    }
    bool AddTool(CWnd* pWnd, const LPCWSTR lpszText)
    {
        if (pWnd == nullptr) return false;
        TTTOOLINFOW ti{}; ti.cbSize = sizeof(ti);
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = pWnd->m_hWnd;
        ti.uId = reinterpret_cast<UINT_PTR>(pWnd->m_hWnd);
        ti.lpszText = const_cast<LPWSTR>(lpszText);
        return static_cast<bool>(SendNativeMessage(TTM_ADDTOOLW, 0, &ti));
    }
    bool AddTool(CWnd* pWnd, const UINT_PTR id, const RECT& rect, const LPCWSTR lpszText)
    {
        if (pWnd == nullptr) return false;
        TTTOOLINFOW ti{}; ti.cbSize = sizeof(ti);
        ti.uFlags = TTF_SUBCLASS;
        ti.hwnd = pWnd->m_hWnd;
        ti.uId = id;
        ti.rect = rect;
        ti.lpszText = const_cast<LPWSTR>(lpszText);
        return static_cast<bool>(SendNativeMessage(TTM_ADDTOOLW, 0, &ti));
    }
    void Activate() { SendNativeMessage(TTM_ACTIVATE, true); }
    void Pop() { SendNativeMessage(TTM_POP); }
    void SetToolRect(CWnd* pWnd, const UINT_PTR id, const RECT& rect)
    {
        if (pWnd == nullptr) return;
        TTTOOLINFOW ti{}; ti.cbSize = sizeof(ti);
        ti.hwnd = pWnd->m_hWnd;
        ti.uId = id;
        ti.rect = rect;
        SendNativeMessage(TTM_NEWTOOLRECTW, 0, &ti);
    }
    void SetMaxTipWidth(const int w) { SendNativeMessage(TTM_SETMAXTIPWIDTH, 0, static_cast<LPARAM>(w)); }
    void RelayEvent(MSG* pMsg) { SendNativeMessage(TTM_RELAYEVENT, 0, pMsg); }
};

class CHeaderCtrl final : public CWnd
{
public:
    int  GetItemCount() const { return static_cast<int>(SendNativeMessage(HDM_GETITEMCOUNT)); }
    bool GetItem(const int i, HDITEMW* p) const { return static_cast<bool>(SendNativeMessage(HDM_GETITEMW, static_cast<WPARAM>(i), p)); }
    bool SetItem(const int i, HDITEMW* p) { return static_cast<bool>(SendNativeMessage(HDM_SETITEMW, static_cast<WPARAM>(i), p)); }
};

class CListCtrl : public CWnd
{
public:
    int FindItem(const LVFINDINFOW* info, const int start = -1) const
    {
        return static_cast<int>(SendNativeMessage(LVM_FINDITEMW, static_cast<WPARAM>(start), info));
    }
    bool Create(const DWORD dwStyle, const RECT& rect, CWnd* pParent, const UINT nID)
    {
        return CreateEx(0, WC_LISTVIEWW, nullptr, dwStyle, rect, pParent, nID);
    }

    int  GetItemCount() const { return static_cast<int>(SendNativeMessage(LVM_GETITEMCOUNT)); }

    UINT GetItemState(const int i, const UINT mask) const { return static_cast<UINT>(SendNativeMessage(LVM_GETITEMSTATE, static_cast<WPARAM>(i), (LPARAM)mask)); }
    bool SetItemState(const int i, const UINT state, const UINT mask)
    {
        LVITEMW it{}; it.state = state; it.stateMask = mask; return static_cast<bool>(SendNativeMessage(LVM_SETITEMSTATE, static_cast<WPARAM>(i), &it));
    }
    int  GetNextItem(const int i, const int flags) const { return static_cast<int>(SendNativeMessage(LVM_GETNEXTITEM, static_cast<WPARAM>(i), (LPARAM)flags)); }
    int  GetSelectionMark() const { return static_cast<int>(SendNativeMessage(LVM_GETSELECTIONMARK)); }
    int  SetSelectionMark(const int i) { return static_cast<int>(SendNativeMessage(LVM_SETSELECTIONMARK, 0, (LPARAM)i)); }
    UINT GetSelectedCount() const { return static_cast<UINT>(SendNativeMessage(LVM_GETSELECTEDCOUNT)); }
    int  GetTopIndex() const { return static_cast<int>(SendNativeMessage(LVM_GETTOPINDEX)); }
    int  GetCountPerPage() const { return static_cast<int>(SendNativeMessage(LVM_GETCOUNTPERPAGE)); }
    bool EnsureVisible(const int i, const bool partial) { return static_cast<bool>(SendNativeMessage(LVM_ENSUREVISIBLE, static_cast<WPARAM>(i), (LPARAM)partial)); }
    bool GetItemRect(const int i, LPRECT r, const UINT code) const
    {
        if (r == nullptr) return false;
        if (i == g_drawItemCtx.item && m_hWnd == g_drawItemCtx.hWnd
            && (code == LVIR_LABEL || code == LVIR_BOUNDS))
        {
            EnsureDrawColCache();
            if (g_drawItemCtx.colCount > 0 && g_drawItemCtx.colValid[0])
            {
                r->top = g_drawItemCtx.rcItem.top;
                r->bottom = g_drawItemCtx.rcItem.bottom;
                r->left = (code == LVIR_LABEL) ? g_drawItemCtx.colLeft[0] : g_drawItemCtx.rcItem.left;
                // LVIR_LABEL = column-0 width; LVIR_BOUNDS = full row
                r->right = (code == LVIR_LABEL)
                    ? g_drawItemCtx.colRight[0]
                    : g_drawItemCtx.rcItem.right;
                return true;
            }
        }
        // Fallback: set r->left = code before sending per ListView_GetItemRect convention
        r->left = static_cast<LONG>(code);
        return static_cast<bool>(SendNativeMessage(LVM_GETITEMRECT, static_cast<WPARAM>(i), r));
    }
    bool GetSubItemRect(const int i, const int sub, const int code, RECT& r) const
    {
        if (i == g_drawItemCtx.item && m_hWnd == g_drawItemCtx.hWnd && code == LVIR_LABEL)
        {
            EnsureDrawColCache();
            if (sub >= 0 && sub < g_drawItemCtx.colCount && g_drawItemCtx.colValid[sub])
            {
                r.top = g_drawItemCtx.rcItem.top;
                r.bottom = g_drawItemCtx.rcItem.bottom;
                r.left = g_drawItemCtx.colLeft[sub];
                r.right = g_drawItemCtx.colRight[sub];
                return true;
            }
        }
        // Fallback: set left = code, top = sub before sending per ListView_GetSubItemRect convention
        r.left = static_cast<LONG>(code); r.top = sub; return static_cast<bool>(SendNativeMessage(LVM_GETSUBITEMRECT, static_cast<WPARAM>(i), &r));
    }
    int  HitTest(LVHITTESTINFO* p) const { return static_cast<int>(SendNativeMessage(LVM_HITTEST, 0, p)); }
    int  HitTest(const CPoint pt, UINT* pFlags = nullptr) const { LVHITTESTINFO h{}; h.pt = pt; const int r = static_cast<int>(SendNativeMessage(LVM_HITTEST, 0, &h)); if (pFlags) *pFlags = h.flags; return r; }
    void RedrawItems(const int first, const int last) { SendNativeMessage(LVM_REDRAWITEMS, static_cast<WPARAM>(first), (LPARAM)last); }
    bool Scroll(const CSize size) { return static_cast<bool>(SendNativeMessage(LVM_SCROLL, static_cast<WPARAM>(size.cx), (LPARAM)size.cy)); }
    DWORD SetExtendedStyle(const DWORD ex) { return static_cast<DWORD>(SendNativeMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)ex)); }
    DWORD GetExtendedStyle() const { return static_cast<DWORD>(SendNativeMessage(LVM_GETEXTENDEDLISTVIEWSTYLE)); }
    bool SetBkColor(const COLORREF color) { return static_cast<bool>(SendNativeMessage(LVM_SETBKCOLOR, 0, static_cast<LPARAM>(color))); }
    bool SetTextBkColor(const COLORREF color) { return static_cast<bool>(SendNativeMessage(LVM_SETTEXTBKCOLOR, 0, static_cast<LPARAM>(color))); }
    bool SetTextColor(const COLORREF color) { return static_cast<bool>(SendNativeMessage(LVM_SETTEXTCOLOR, 0, static_cast<LPARAM>(color))); }
    bool SetItemCountEx(const int n, const DWORD flags = LVSICF_NOINVALIDATEALL) { return static_cast<bool>(SendNativeMessage(LVM_SETITEMCOUNT, static_cast<WPARAM>(n), (LPARAM)flags)); }
    void SetRowHeight(const int height)
    {
        const HIMAGELIST images = ImageList_Create(1, height, ILC_COLOR, 1, 1);
        SendNativeMessage(LVM_SETIMAGELIST, LVSIL_SMALL, images);
        SendNativeMessage(LVM_SETIMAGELIST, LVSIL_SMALL);
        if (images != nullptr) ImageList_Destroy(images);
    }

    int  InsertColumn(const int nCol, const LPCWSTR psz, const int fmt = LVCFMT_LEFT, const int width = -1, const int sub = -1)
    {
        LVCOLUMNW c{}; c.mask = LVCF_TEXT | LVCF_FMT; c.pszText = const_cast<LPWSTR>(psz); c.fmt = fmt;
        if (width != -1) { c.mask |= LVCF_WIDTH; c.cx = width; }
        if (sub != -1) { c.mask |= LVCF_SUBITEM; c.iSubItem = sub; }
        return static_cast<int>(SendNativeMessage(LVM_INSERTCOLUMNW, static_cast<WPARAM>(nCol), &c));
    }
    bool DeleteColumn(const int nCol) { return static_cast<bool>(SendNativeMessage(LVM_DELETECOLUMN, static_cast<WPARAM>(nCol))); }
    bool GetColumn(const int nCol, LVCOLUMNW* p) const { return static_cast<bool>(SendNativeMessage(LVM_GETCOLUMN, static_cast<WPARAM>(nCol), p)); }
    bool SetColumn(const int nCol, const LVCOLUMNW* p) { return static_cast<bool>(SendNativeMessage(LVM_SETCOLUMN, static_cast<WPARAM>(nCol), p)); }
    int  GetColumnWidth(const int nCol) const { return static_cast<int>(SendNativeMessage(LVM_GETCOLUMNWIDTH, static_cast<WPARAM>(nCol))); }
    bool SetColumnWidth(const int nCol, const int cx) { return static_cast<bool>(SendNativeMessage(LVM_SETCOLUMNWIDTH, static_cast<WPARAM>(nCol), (LPARAM)cx)); }
    bool GetColumnOrder(std::span<int> order) const
    {
        return static_cast<bool>(SendNativeMessage(LVM_GETCOLUMNORDERARRAY, order.size(), order.data()));
    }
    bool SetColumnOrder(std::span<const int> order)
    {
        return static_cast<bool>(SendNativeMessage(LVM_SETCOLUMNORDERARRAY, order.size(), order.data()));
    }

    CHeaderCtrl& Header()
    {
        EnsureHeaderHandle();
        return m_header;
    }
    const CHeaderCtrl& Header() const
    {
        EnsureHeaderHandle();
        return m_header;
    }

    int FirstSelectedIndex() const { return GetNextItem(-1, LVNI_SELECTED); }
    int NextSelectedIndex(const int index) const { return GetNextItem(index, LVNI_SELECTED); }

protected:
    void EnsureHeaderHandle() const
    {
        if (m_header.m_hWnd != nullptr) return;
        m_header.m_hWnd = reinterpret_cast<HWND>(m_pfnSuper
            ? CallWindowProcW(m_pfnSuper, m_hWnd, LVM_GETHEADER, 0, 0)
            : ::SendMessageW(m_hWnd, LVM_GETHEADER, 0, 0));
    }

    // Cache visual header rectangles by logical subitem. HDM_GETITEMRECT accounts
    // for both user-reordered columns and horizontal scrolling; accumulating widths
    // by logical index does not.
    // Called at most once per WM_DRAWITEM dispatch; subsequent calls are no-ops.
    void EnsureDrawColCache() const
    {
        if (g_drawItemCtx.colCount != 0 || m_hWnd != g_drawItemCtx.hWnd) return;
        const HWND hdr = Header().m_hWnd;
        if (hdr == nullptr) return;
        const int n = static_cast<int>(::SendMessageW(hdr, HDM_GETITEMCOUNT, 0, 0));
        constexpr int cap = static_cast<int>(std::size(g_drawItemCtx.colLeft));
        const int cols = std::clamp(n, 0, cap);
        POINT headerOrigin{};
        MapWindowPoints(hdr, m_hWnd, &headerOrigin, 1);
        for (int c = 0; c < cols; ++c)
        {
            RECT columnRect{};
            if (::SendMessageW(hdr, HDM_GETITEMRECT, static_cast<WPARAM>(c), reinterpret_cast<LPARAM>(&columnRect)) != 0)
            {
                g_drawItemCtx.colLeft[c] = headerOrigin.x + columnRect.left;
                g_drawItemCtx.colRight[c] = headerOrigin.x + columnRect.right;
                g_drawItemCtx.colValid[c] = true;
            }
            else
            {
                g_drawItemCtx.colValid[c] = false;
            }
        }
        g_drawItemCtx.colCount = cols;
    }

    mutable CHeaderCtrl m_header;
};

// -----------------------------------------------------------------------------
//  CWaitCursor
// -----------------------------------------------------------------------------
class CWaitCursor final
{
public:
    CWaitCursor() : m_previous(SetCursor(LoadCursorW(nullptr, IDC_WAIT))) {}
    ~CWaitCursor() { SetCursor(m_previous); }
private:
    HCURSOR m_previous = nullptr;
};

class ScopedRedrawPause final
{
public:
    explicit ScopedRedrawPause(CWnd* window) : m_window(window) { m_window->SetRedraw(false); }
    ~ScopedRedrawPause() { m_window->SetRedraw(true); m_window->Invalidate(); }

    ScopedRedrawPause(const ScopedRedrawPause&) = delete;
    ScopedRedrawPause& operator=(const ScopedRedrawPause&) = delete;

private:
    CWnd* m_window;
};

class CBufferedDC final : public CDC
{
public:
    CBufferedDC(CDC& target, CWnd* window) :
        CBufferedDC(target, window != nullptr ? window->ClientRect() : CRect()) {}
    CBufferedDC(CDC& target, const CRect& rect)
        : CDC(&target), m_target(target), m_bitmap(&target, rect.Width(), rect.Height()), m_rect(rect)
    {
        if (!*this || !m_bitmap) return;
        const HGDIOBJ previousBitmap = SelectObject(m_hDC, m_bitmap);
        if (previousBitmap == nullptr || previousBitmap == HGDI_ERROR) return;
        m_previousBitmap = static_cast<HBITMAP>(previousBitmap);
        SetViewportOrg(-rect.left, -rect.top);
    }
    ~CBufferedDC()
    {
        if (m_previousBitmap == nullptr) return;
        SetViewportOrg(0, 0);
        m_target.BitBlt(m_rect.left, m_rect.top, m_rect.Width(), m_rect.Height(), this, 0, 0, SRCCOPY);
        SelectObject(m_hDC, m_previousBitmap);
    }

    CBufferedDC(const CBufferedDC&) = delete;
    CBufferedDC& operator=(const CBufferedDC&) = delete;

private:
    CDC& m_target;
    CBitmap m_bitmap;
    HBITMAP m_previousBitmap = nullptr;
    CRect m_rect;
};

// -----------------------------------------------------------------------------
//  Native common-dialog helpers
// -----------------------------------------------------------------------------
inline HWND GetDialogOwner(CWnd* pParentWnd = nullptr)
{
    HWND hOwner = pParentWnd ? pParentWnd->Handle() : GetActiveWindow();
    if (hOwner == nullptr)
    {
        if (const CWnd* pMainWnd = GetMainWindow()) hOwner = pMainWnd->Handle();
    }
    if (hOwner == nullptr) return nullptr;

    if (const HWND hRoot = GetAncestor(hOwner, GA_ROOT); hRoot != nullptr) hOwner = hRoot;
    if (const HWND hPopup = GetLastActivePopup(hOwner);
        hPopup != nullptr && IsWindowVisible(hPopup) && IsWindowEnabled(hPopup))
    {
        hOwner = hPopup;
    }
    return hOwner;
}

// -----------------------------------------------------------------------------
//  CDialog / CDialog + DDX
// -----------------------------------------------------------------------------
INT_PTR CALLBACK FrameworkDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
inline bool PreTranslateWindowTree(HWND hWndStop, MSG* pMsg);

inline thread_local std::vector<CWnd*> g_modalPreTranslateStack;
inline thread_local HHOOK g_modalPreTranslateHook = nullptr;

inline LRESULT CALLBACK ModalMessageHookProc(const int code, const WPARAM wParam, const LPARAM lParam)
{
    if (code >= 0 && lParam != 0 && !g_modalPreTranslateStack.empty())
    {
        const auto pMsg = reinterpret_cast<MSG*>(lParam);
        if (const CWnd* pDlg = g_modalPreTranslateStack.back();
            pDlg != nullptr && pDlg->Handle() != nullptr &&
            pMsg->hwnd != nullptr &&
            (pMsg->hwnd == pDlg->m_hWnd || IsChild(pDlg->m_hWnd, pMsg->hwnd)) &&
            PreTranslateWindowTree(pDlg->m_hWnd, pMsg))
        {
            return true;
        }
    }

    return CallNextHookEx(g_modalPreTranslateHook, code, wParam, lParam);
}

class CDialog : public MessageTarget<CDialog, CWnd>
{
public:
    enum class FilePickerMode : char { Open, Save };

    explicit CDialog(const UINT nIDTemplate, CWnd* pParent = nullptr) : m_nIDTemplate(nIDTemplate), m_pParentWnd(pParent) {}

    static std::optional<std::wstring> PickFile(FilePickerMode mode, std::wstring filter);
    static std::optional<std::wstring> PickFolder(CWnd* parent = nullptr);
    static std::optional<COLORREF> PickColor(COLORREF initial);

    virtual INT_PTR ShowModal()
    {
        if (m_hWnd != nullptr) return -1;
        struct ModalPreTranslateScope final
        {
            explicit ModalPreTranslateScope(CWnd* dialog)
            {
                if (g_modalPreTranslateHook == nullptr)
                    g_modalPreTranslateHook = SetWindowsHookExW(WH_MSGFILTER,
                        ModalMessageHookProc, nullptr, GetCurrentThreadId());
                g_modalPreTranslateStack.push_back(dialog);
            }
            ~ModalPreTranslateScope()
            {
                if (!g_modalPreTranslateStack.empty()) g_modalPreTranslateStack.pop_back();
                if (!g_modalPreTranslateStack.empty() || g_modalPreTranslateHook == nullptr) return;
                UnhookWindowsHookEx(g_modalPreTranslateHook);
                g_modalPreTranslateHook = nullptr;
            }
            ModalPreTranslateScope(const ModalPreTranslateScope&) = delete;
            ModalPreTranslateScope& operator=(const ModalPreTranslateScope&) = delete;
        };

        const HWND hParent = GetDialogOwner(m_pParentWnd);
        const ModalPreTranslateScope modalPreTranslate(this);
        const INT_PTR result = DialogBoxParamW(GetAppInstance(), MAKEINTRESOURCEW(m_nIDTemplate),
            hParent, FrameworkDialogProc, reinterpret_cast<LPARAM>(static_cast<CWnd*>(this)));
        ClearTemporaryHandleCaches();
        return result;
    }

    bool PreprocessMessage(MSG* pMsg) override
    {
        if (pMsg != nullptr && pMsg->message == WM_KEYDOWN && pMsg->wParam == 'C' && IsKeyDown(VK_CONTROL))
        {
            if (const auto point = ClientCursorPosition())
            {
                wchar_t className[16]{};
                if (const CWnd* control = ChildWindowFromPoint(*point, CWP_SKIPINVISIBLE);
                    control != nullptr && control != this &&
                    GetClassNameW(control->m_hWnd, className, static_cast<int>(std::size(className))) != 0 &&
                    _wcsicmp(className, WC_STATIC) == 0)
                {
                    if (const std::wstring value = control->Text();
                        !value.empty() && CopyTextToClipboard(value)) return true;
                }
            }
        }

        return CWnd::PreprocessMessage(pMsg);
    }

    virtual bool OnInitDialog()
    {
        if (InitializeDialogControls(m_nIDTemplate)) return true;
        CloseModal(-1);
        return false;
    }
    virtual void OnOK() { CloseModal(IDOK); }
    virtual void OnCancel() { CloseModal(IDCANCEL); }
    void CloseModal(const int result) { EndDialog(m_hWnd, result); }

    static std::span<const RouteEntry> Routes()
    {
        using ThisClass = CDialog;
        static constexpr std::array entries
        {
            Route::Command<&ThisClass::OnOK>(IDOK),
            Route::Command<&ThisClass::OnCancel>(IDCANCEL),
        };
        return entries;
    }

    UINT  m_nIDTemplate = 0;
    CWnd* m_pParentWnd = nullptr;
};

inline INT_PTR CALLBACK FrameworkDialogProc(HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
    // Make CurrentMessage()/the WM_CTLCOLOR thunk see the real message (needed for
    // dark-mode OnCtlColor, which derives the control type from the current message).
    const ScopedValue currentMessage(g_currentMsg, MakeMessageSnapshot(hWnd, msg, wParam, lParam));

    if (msg == WM_INITDIALOG)
    {
        const auto pDlg = static_cast<CDialog*>(reinterpret_cast<CWnd*>(lParam));
        if (pDlg == nullptr) return false;

        const bool destroyOnFailure = (GetWindowLongPtrW(hWnd, GWL_STYLE) & WS_CHILD) != 0;

        const auto abortDialog = [destroyOnFailure, hWnd]
            {
                if (destroyOnFailure) DestroyWindow(hWnd);
                else EndDialog(hWnd, -1);
            };
        if (!pDlg->Attach(hWnd))
        {
            abortDialog();
            return false;
        }

        try
        {
            InitializeDialogFontAndSize(hWnd);
            return pDlg->OnInitDialog();
        }
        catch (...)
        {
            abortDialog();
            return false;
        }
    }
    CWnd* pDlg = CWnd::FindAttached(hWnd);
    if (pDlg == nullptr) return false;
    LRESULT lResult = 0;
    bool handled = false;
    try { handled = pDlg->RouteWindowMessage(msg, wParam, lParam, &lResult); }
    catch (...) {}
    if (msg == WM_NCDESTROY)
    {
        pDlg->Detach();
        try { pDlg->PostNcDestroy(); }
        catch (...) {}
        return false;
    }
    if (handled)
    {
        if (msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC)
        {
            return reinterpret_cast<INT_PTR>(reinterpret_cast<void*>(lResult));
        }
        SetWindowLongPtrW(hWnd, DWLP_MSGRESULT, lResult);
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
//  Application object
// -----------------------------------------------------------------------------
class CWinApp;

inline CWinApp* g_pApp = nullptr;
inline thread_local std::function<void()> g_idleCmdUiUpdate;   // set by the frame to refresh toolbar UI

class CWinApp : public MessageTarget<CWinApp, CCmdTarget>
{
public:
    static std::span<const RouteEntry> Routes();
    CWnd* m_pMainWnd = nullptr;
    MSG m_msgCur{};
    LPWSTR m_lpCmdLine = nullptr;
    int m_nCmdShow = SW_SHOWNORMAL;

    CWinApp() { g_pApp = this; }
    ~CWinApp() override { if (g_pApp == this) g_pApp = nullptr; }

    virtual bool InitInstance() { return true; }
    virtual int ExitInstance() { return static_cast<int>(m_msgCur.wParam); }
    virtual bool OnIdle(const LONG lCount)
    {
        if (lCount != 0) return false;
        if (g_idleCmdUiUpdate) g_idleCmdUiUpdate();
        ClearTemporaryHandleCaches();
        return true;
    }
    virtual bool IsIdleMessage(MSG* pMsg)
    {
        return !(pMsg->message == WM_MOUSEMOVE || pMsg->message == WM_NCMOUSEMOVE ||
            pMsg->message == WM_PAINT || pMsg->message == 0x0118 /*WM_SYSTIMER*/);
    }
    virtual bool PreprocessMessage(MSG* pMsg);
    virtual bool PumpMessage();
    virtual int Run();

    static void RunTaskWithUiUpdates(const std::function<void()>& task);
    static void WaitForHandleWithUiUpdates(HANDLE handle, DWORD timeout = INFINITE) noexcept;

    void OnAppExit()
    {
        if (m_pMainWnd != nullptr) m_pMainWnd->SendMessage(WM_CLOSE);
    }

};

// ---- message pump --------------------------------------------------------------
inline bool PreTranslateWindowTree(const HWND hWndStop, MSG* pMsg)
{
    for (HWND hWnd = pMsg->hwnd; hWnd != nullptr; hWnd = GetParent(hWnd))
    {
        if (CWnd* pWnd = CWnd::FindAttached(hWnd))
            if (pWnd->PreprocessMessage(pMsg)) return true;
        if (hWnd == hWndStop) break;
    }
    return false;
}
inline bool CWinApp::PreprocessMessage(MSG* pMsg)
{
    return PreTranslateWindowTree(m_pMainWnd ? m_pMainWnd->m_hWnd : nullptr, pMsg);
}
inline bool CWinApp::PumpMessage()
{
    if (const int result = GetMessageW(&m_msgCur, nullptr, 0, 0); result <= 0)
    {
        if (result < 0)
        {
            m_msgCur = {};
            m_msgCur.message = WM_QUIT;
            m_msgCur.wParam = static_cast<WPARAM>(-1);
        }
        return false;
    }
    if (!PreprocessMessage(&m_msgCur))
    {
        TranslateMessage(&m_msgCur);
        DispatchMessageW(&m_msgCur);
    }
    return true;
}
inline int CWinApp::Run()
{
    bool bIdle = true;
    LONG lIdleCount = 0;
    for (;;)
    {
        while (bIdle && !PeekMessageW(&m_msgCur, nullptr, 0, 0, PM_NOREMOVE))
        {
            if (!OnIdle(lIdleCount++)) bIdle = false;
        }
        do
        {
            if (!PumpMessage()) return ExitInstance();
            if (IsIdleMessage(&m_msgCur)) { bIdle = true; lIdleCount = 0; }
        } while (PeekMessageW(&m_msgCur, nullptr, 0, 0, PM_NOREMOVE));
    }
}

// ---- Application globals -------------------------------------------------------
inline CWnd* GetMainWindow() { return g_pApp ? g_pApp->m_pMainWnd : nullptr; }
inline HWND GetMainWindowHandle() noexcept
{
    const CWnd* mainWindow = GetMainWindow();
    return mainWindow != nullptr ? mainWindow->Handle() : nullptr;
}
inline HINSTANCE GetAppInstance() { return GetModuleHandleW(nullptr); }

// -----------------------------------------------------------------------------
//  Frame / control-bar / splitter constants
// -----------------------------------------------------------------------------
inline constexpr UINT WDS_PANE_ID_BASE = 0xE900;
inline constexpr UINT WDS_TOOLBAR_ID = 0xE81B;
inline constexpr UINT WDS_STATUS_BAR_ID = 0xE801;

class CSplitterWnd;

// -----------------------------------------------------------------------------
//  CFrameWnd
// -----------------------------------------------------------------------------
class CFrameWnd : public MessageTarget<CFrameWnd, CWnd>
{
public:
    ~CFrameWnd() override
    {
        if (m_hAccelTable != nullptr) DestroyAcceleratorTable(m_hAccelTable);
    }

    bool RouteCommand(const UINT nID, const int nCode, void* pExtra, const bool execute = true) override
    {
        if (CWnd* focus = GetFocus(); focus != nullptr && focus != this && IsChild(focus))
        {
            for (CWnd* target = focus; target != nullptr && target != this; target = target->GetParent())
                if (target->RouteCommand(nID, nCode, pExtra, execute)) return true;
        }

        if (CWnd::RouteCommand(nID, nCode, pExtra, execute)) return true;
        if (CCmdTarget* target = GetCommandTarget();
            target != nullptr && target->RouteCommand(nID, nCode, pExtra, execute)) return true;
        return g_pApp != nullptr && g_pApp->RouteCommand(nID, nCode, pExtra, execute);
    }

    virtual bool CreateFromResource(UINT nIDResource);
    virtual void UpdateLayout();
    void SetDocumentTitle(const LPCWSTR lpszDocName)
    {
        const std::wstring prefix = m_strTitle.empty() ? L"WinDirStat" : m_strTitle;
        const std::wstring t = lpszDocName && lpszDocName[0] != L'\0' ?
            prefix + L" - " + lpszDocName : prefix;
        if (m_hWnd) SetWindowTextW(m_hWnd, t.c_str());
    }

    void SetTitle(const LPCWSTR title) { m_strTitle = title; SetText(title); }

    void SetTopBar(CWnd* bar) { m_topBar = bar; UpdateLayout(); }
    void SetBottomBar(CWnd* bar) { m_bottomBar = bar; UpdateLayout(); }
    void UpdateMenuCommands(CMenu* pMenu, bool bSysMenu);

    static std::span<const RouteEntry> Routes();

protected:
    virtual bool OnCreateClient() { return true; }
    virtual CCmdTarget* GetCommandTarget() const { return nullptr; }

    bool PreCreateWindow(CREATESTRUCT& cs) override;
    void PostNcDestroy() override
    {
        if (CWinApp* pApp = g_pApp; pApp != nullptr && pApp->m_pMainWnd == this) pApp->m_pMainWnd = nullptr;
        delete this;
    }
    bool PreprocessMessage(MSG* pMsg) override;

    int OnCreate(const LPCREATESTRUCT lpcs)
    {
        if (CWnd::OnCreate(lpcs) == -1) return -1;
        return OnCreateClient() ? 0 : -1;
    }
    void OnSize(const UINT nType, const int cx, const int cy) { UpdateLayout(); CWnd::OnSize(nType, cx, cy); }
    void OnDestroy()
    {
        CWnd::OnDestroy();
        if (const CWinApp* pApp = g_pApp; pApp != nullptr && pApp->m_pMainWnd == this)
            PostQuitMessage(0);
    }
    void OnInitMenuPopup(CMenu* pMenu, UINT /*nIndex*/, const bool bSysMenu) { UpdateMenuCommands(pMenu, bSysMenu); }

    // Default handling for the standard ID_VIEW_TOOLBAR / ID_VIEW_STATUS_BAR menu commands.
    void OnBarCheck(UINT nID);
    void OnUpdateControlBarMenu(CCmdUI* pCmdUI) const;

private:
    HACCEL m_hAccelTable = nullptr;
    std::wstring m_strTitle;
    CWnd* m_topBar = nullptr;
    CWnd* m_bottomBar = nullptr;
};

inline bool CFrameWnd::PreprocessMessage(MSG* pMsg)
{
    if (m_hAccelTable && ::TranslateAccelerator(m_hWnd, m_hAccelTable, pMsg))
        return true;
    return CWnd::PreprocessMessage(pMsg);
}

inline bool CFrameWnd::PreCreateWindow(CREATESTRUCT& cs)
{
    if (cs.lpszName) m_strTitle = cs.lpszName;
    if (cs.lpszClass == nullptr)
        cs.lpszClass = RegisterWindowClass(CS_DBLCLKS, LoadCursorW(nullptr, IDC_ARROW),
            reinterpret_cast<HBRUSH>(COLOR_3DFACE + 1),
            LoadIconW(GetAppInstance(), MAKEINTRESOURCEW(128 /*IDR_MAINFRAME*/)));
    return true;
}

inline bool CFrameWnd::CreateFromResource(const UINT nIDResource)
{
    struct FrameResourceScope final
    {
        ~FrameResourceScope()
        {
            if (hMenu != nullptr && IsMenu(hMenu)) DestroyMenu(hMenu);
            if (hAccel != nullptr) DestroyAcceleratorTable(hAccel);
        }

        HMENU hMenu;
        HACCEL hAccel;
    } resources{
        LoadMenuW(GetAppInstance(), MAKEINTRESOURCEW(nIDResource)),
        LoadAcceleratorsW(GetAppInstance(), MAKEINTRESOURCEW(nIDResource))
    };

    if (const std::wstring strTitle = LoadResourceString(nIDResource);
        !CreateEx(0, nullptr, strTitle.empty() ? nullptr : strTitle.c_str(),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, resources.hMenu))
    {
        return false;
    }

    m_hAccelTable = resources.hAccel;
    resources.hAccel = nullptr;
    resources.hMenu = nullptr;
    UpdateLayout();
    return true;
}

inline void CFrameWnd::UpdateLayout()
{
    if (!IsWindow(m_hWnd)) return;
    const CRect rc = ClientRect();
    int top = rc.top, bottom = rc.bottom;
    if (m_topBar != nullptr && IsWindow(m_topBar->m_hWnd) && m_topBar->IsWindowVisible())
    {
        const CSize size = m_topBar->PreferredSize();
        m_topBar->SetWindowPos(nullptr, rc.left, top, rc.Width(), size.cy, SWP_NOZORDER | SWP_NOACTIVATE);
        top += size.cy;
    }
    if (m_bottomBar != nullptr && IsWindow(m_bottomBar->m_hWnd) && m_bottomBar->IsWindowVisible())
    {
        const CSize size = m_bottomBar->PreferredSize();
        m_bottomBar->SetWindowPos(nullptr, rc.left, bottom - size.cy, rc.Width(), size.cy, SWP_NOZORDER | SWP_NOACTIVATE);
        bottom -= size.cy;
    }
    if (CWnd* client = FromHandle(::GetDlgItem(m_hWnd, WDS_PANE_ID_BASE));
        client != nullptr && IsWindow(client->m_hWnd))
        client->SetWindowPos(nullptr, rc.left, top, rc.Width(), bottom - top, SWP_NOZORDER | SWP_NOACTIVATE);
}

// -----------------------------------------------------------------------------
//  CSplitterWnd (static splitter)
// -----------------------------------------------------------------------------

class CSplitterWnd : public MessageTarget<CSplitterWnd, CWnd>
{
public:
    static constexpr int PaneBorderSize = 2;
    static constexpr int SplitterSize = 7;

    bool IsSplitterWindow() const noexcept override { return true; }

    bool CreateStatic(CWnd* pParentWnd, const int nRows, const int nCols,
        const DWORD dwStyle = WS_CHILD | WS_VISIBLE, const UINT nID = WDS_PANE_ID_BASE)
    {
        if ((nRows != 1 && nCols != 1) || nRows < 1 || nRows > 2 || nCols < 1 || nCols > 2)
            return false;

        m_rowSizes.assign(nRows, 0);
        m_columnSizes.assign(nCols, 0);
        const CRect rc(0, 0, 0, 0);
        const LPCWSTR cls = RegisterWindowClass(CS_DBLCLKS, LoadCursorW(nullptr, IDC_ARROW));
        return CreateEx(0, cls, nullptr, (dwStyle & ~WS_BORDER) | WS_CLIPCHILDREN, rc, pParentWnd, nID);
    }
    template<typename View>
    bool CreateView(const int row, const int col, const SIZE sizeInit)
    {
        static_assert(std::is_base_of_v<CWnd, View>);
        if (!IsValidPane(row, col)) return false;
        auto* view = new View;
        const CRect rc(0, 0, sizeInit.cx, sizeInit.cy);
        if (!view->Create(nullptr, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            rc, this, PaneId(row, col)))
        {
            // Dynamically created views release themselves from PostNcDestroy,
            // including when WM_CREATE fails after the HWND has been attached.
            return false;
        }
        return true;
    }
    CWnd* PaneAt(const int row, const int col) const
    {
        return IsValidPane(row, col) ? FromHandle(::GetDlgItem(m_hWnd, PaneId(row, col))) : nullptr;
    }
    int PaneId(const int row, const int col) const { return WDS_PANE_ID_BASE + row * 16 + col; }
    int RowCount() const { return static_cast<int>(m_rowSizes.size()); }
    int ColumnCount() const { return static_cast<int>(m_columnSizes.size()); }
    int RowSize(const int row) const { return IsValidRow(row) ? m_rowSizes[row] : 0; }
    void SetRowSize(const int row, const int size) { if (IsValidRow(row)) m_rowSizes[row] = size; }
    int ColumnSize(const int column) const { return IsValidColumn(column) ? m_columnSizes[column] : 0; }
    void SetColumnSize(const int column, const int size) { if (IsValidColumn(column)) m_columnSizes[column] = size; }
    void ResetPanes() { m_rowSizes.clear(); m_columnSizes.clear(); }

    virtual void UpdateLayout();

    static std::span<const RouteEntry> Routes()
    {
        using ThisClass = CSplitterWnd;
        static constexpr std::array entries
        {
            Route::Window<&ThisClass::OnSize>(WM_SIZE),
            Route::Window<&ThisClass::OnEraseBkgnd>(WM_ERASEBKGND),
            Route::Window<&ThisClass::OnPaint>(WM_PAINT),
            Route::Window<&ThisClass::OnLButtonDown>(WM_LBUTTONDOWN),
            Route::Window<&ThisClass::OnLButtonUp>(WM_LBUTTONUP),
            Route::Window<&ThisClass::OnMouseMove>(WM_MOUSEMOVE),
            Route::Window<&ThisClass::OnCaptureChanged>(WM_CAPTURECHANGED),
            Route::Window<&ThisClass::OnCancelMode>(WM_CANCELMODE),
            Route::Window<&ThisClass::OnSetCursor>(WM_SETCURSOR),
        };
        return entries;
    }

protected:
    virtual void StopTracking(bool bAccept);

    // message handlers
    void OnSize(UINT, int, int) { UpdateLayout(); }
    bool OnEraseBkgnd(CDC*) { return true; }
    void OnPaint();
    void OnLButtonDown(UINT nFlags, CPoint pt);
    void OnLButtonUp(UINT nFlags, CPoint pt);
    void OnMouseMove(UINT nFlags, CPoint pt);
    void OnCaptureChanged(CWnd* pWnd);
    void OnCancelMode();
    bool OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);

private:
    bool IsValidRow(const int row) const { return row >= 0 && std::cmp_less(row, m_rowSizes.size()); }
    bool IsValidColumn(const int column) const
    {
        return column >= 0 && std::cmp_less(column, m_columnSizes.size());
    }
    bool IsValidPane(const int row, const int column) const { return IsValidRow(row) && IsValidColumn(column); }

    CRect WorkRect() const;
    CRect TrackerRect(int pos) const;
    void DrawBackground(CDC& dc, CRect rect) const;
    void DrawTrackerRect(CDC& dc, const CRect& rect) const;
    void DrawTracker();
    void FinishTracking(bool bAccept, bool releaseCapture);

    std::vector<int> m_rowSizes;
    std::vector<int> m_columnSizes;
    bool m_bTracking = false;
    bool m_bTrackingColumn = false;
    bool m_bTrackerVisible = false;
    int m_nTrackPos = 0;
    CRect m_rectTracker;
};

inline void CSplitterWnd::UpdateLayout()
{
    if (!IsWindow(m_hWnd) || m_rowSizes.empty() || m_columnSizes.empty()) return;
    CRect rcWork = ClientRect();
    rcWork.Deflate(PaneBorderSize, PaneBorderSize);
    const auto movePane = [&](const int row, const int column, CRect rect)
    {
        CWnd* pane = PaneAt(row, column);
        if (pane == nullptr || !IsWindow(pane->m_hWnd)) return;
        if (pane->IsSplitterWindow()) rect.Inflate(PaneBorderSize, PaneBorderSize);
        pane->MoveWindow(rect);
    };
    if (m_columnSizes.size() > 1)
    {
        const int total = rcWork.Width();
        constexpr int bar = SplitterSize;
        int w0 = m_columnSizes[0];
        if (w0 <= 0) w0 = (total - bar) / 2;
        w0 = std::clamp(w0, 0, std::max(0, total - bar));
        const int w1 = std::max(0, total - bar - w0);
        m_columnSizes[0] = w0; m_columnSizes[1] = w1;
        movePane(0, 0, CRect(rcWork.left, rcWork.top, rcWork.left + w0, rcWork.bottom));
        movePane(0, 1, CRect(rcWork.left + w0 + bar, rcWork.top, rcWork.right, rcWork.bottom));
    }
    else if (m_rowSizes.size() > 1)
    {
        const int total = rcWork.Height();
        constexpr int bar = SplitterSize;
        int h0 = m_rowSizes[0];
        if (h0 <= 0) h0 = (total - bar) / 2;
        h0 = std::clamp(h0, 0, std::max(0, total - bar));
        const int h1 = std::max(0, total - bar - h0);
        m_rowSizes[0] = h0; m_rowSizes[1] = h1;
        movePane(0, 0, CRect(rcWork.left, rcWork.top, rcWork.right, rcWork.top + h0));
        movePane(1, 0, CRect(rcWork.left, rcWork.top + h0 + bar, rcWork.right, rcWork.bottom));
    }
    else movePane(0, 0, rcWork);
    Invalidate(false);
}
inline CRect CSplitterWnd::WorkRect() const
{
    CRect rc = ClientRect();
    rc.Deflate(PaneBorderSize, PaneBorderSize);
    return rc;
}

inline CRect CSplitterWnd::TrackerRect(const int pos) const
{
    const CRect rcWork = WorkRect();
    if (m_bTrackingColumn)
    {
        const int x = rcWork.left + std::clamp(pos, 0, std::max(0, rcWork.Width() - SplitterSize));
        return CRect(x, rcWork.top, x + SplitterSize, rcWork.bottom);
    }

    const int y = rcWork.top + std::clamp(pos, 0, std::max(0, rcWork.Height() - SplitterSize));
    return CRect(rcWork.left, y, rcWork.right, y + SplitterSize);
}

inline void CSplitterWnd::DrawTrackerRect(CDC& dc, const CRect& rect) const
{
    if (!rect.IsEmpty())
        dc.PatBlt(rect.left, rect.top, rect.Width(), rect.Height(), DSTINVERT);
}

inline void CSplitterWnd::DrawTracker()
{
    if (!IsWindow(m_hWnd) || m_rectTracker.IsEmpty())
        return;

    CClientDC dc(this);
    DrawTrackerRect(dc, m_rectTracker);
}

inline void CSplitterWnd::FinishTracking(const bool bAccept, const bool releaseCapture)
{
    if (!m_bTracking)
        return;

    const bool trackingColumn = m_bTrackingColumn;
    const int trackPos = m_nTrackPos;

    if (m_bTrackerVisible)
        DrawTracker();

    m_bTracking = false;
    m_bTrackerVisible = false;
    m_rectTracker.Clear();

    if (releaseCapture && ::GetCapture() == m_hWnd)
        ReleaseCapture();

    if (!bAccept)
    {
        Invalidate(false);
        return;
    }

    if (trackingColumn && m_columnSizes.size() > 1)
        m_columnSizes[0] = trackPos;
    else if (!trackingColumn && m_rowSizes.size() > 1)
        m_rowSizes[0] = trackPos;

    UpdateLayout();
}

inline void CSplitterWnd::StopTracking(const bool bAccept) { FinishTracking(bAccept, true); }
inline void CSplitterWnd::OnLButtonDown(UINT, const CPoint pt)
{
    if (m_bTracking)
        StopTracking(false);

    const CRect rcWork = WorkRect();
    bool onBar = false;
    bool trackingColumn = false;
    int trackPos = 0;

    if (m_columnSizes.size() > 1)
    {
        trackingColumn = true;
        trackPos = std::clamp(m_columnSizes[0], 0, std::max(0, rcWork.Width() - SplitterSize));
        const int x = rcWork.left + trackPos;
        onBar = (pt.x >= x && pt.x <= x + SplitterSize);
    }
    else if (m_rowSizes.size() > 1)
    {
        trackPos = std::clamp(m_rowSizes[0], 0, std::max(0, rcWork.Height() - SplitterSize));
        const int y = rcWork.top + trackPos;
        onBar = (pt.y >= y && pt.y <= y + SplitterSize);
    }

    if (!onBar)
        return;

    m_bTracking = true;
    m_bTrackingColumn = trackingColumn;
    m_nTrackPos = trackPos;
    m_rectTracker = TrackerRect(m_nTrackPos);
    m_bTrackerVisible = !m_rectTracker.IsEmpty();
    SetCapture();
    if (m_bTrackerVisible)
        DrawTracker();
}
inline void CSplitterWnd::OnMouseMove(UINT, const CPoint pt)
{
    if (!m_bTracking) return;

    const CRect rcWork = WorkRect();
    const int trackPos = m_bTrackingColumn ?
        std::clamp(static_cast<int>(pt.x - rcWork.left), 0, std::max(0, rcWork.Width() - SplitterSize)) :
        std::clamp(static_cast<int>(pt.y - rcWork.top), 0, std::max(0, rcWork.Height() - SplitterSize));
    if (trackPos == m_nTrackPos)
        return;

    if (m_bTrackerVisible)
        DrawTracker();

    m_nTrackPos = trackPos;
    m_rectTracker = TrackerRect(m_nTrackPos);
    m_bTrackerVisible = !m_rectTracker.IsEmpty();
    if (m_bTrackerVisible)
        DrawTracker();
}
inline void CSplitterWnd::OnLButtonUp(UINT, CPoint) { if (m_bTracking) StopTracking(true); }
inline void CSplitterWnd::OnCaptureChanged(CWnd* pWnd)
{
    if (m_bTracking && (pWnd == nullptr || pWnd->m_hWnd != m_hWnd))
        FinishTracking(false, false);
}
inline void CSplitterWnd::OnCancelMode() { StopTracking(false); }
inline bool CSplitterWnd::OnSetCursor(CWnd*, const UINT nHitTest, UINT)
{
    if (nHitTest == HTCLIENT)
    {
        const auto pt = ClientCursorPosition();
        if (!pt) return static_cast<bool>(CallDefaultHandler());
        const CRect rcWork = WorkRect();
        if (m_columnSizes.size() > 1)
        {
            const int x = rcWork.left + m_columnSizes[0];
            if (pt->x >= x && pt->x <= x + SplitterSize)
            {
                SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                return true;
            }
        }
        else if (m_rowSizes.size() > 1)
        {
            const int y = rcWork.top + m_rowSizes[0];
            if (pt->y >= y && pt->y <= y + SplitterSize)
            {
                SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
                return true;
            }
        }
    }
    return static_cast<bool>(CallDefaultHandler());
}

// -----------------------------------------------------------------------------
//  Theme helpers
// -----------------------------------------------------------------------------
inline constexpr UINT WM_WDS_TAB_CHANGING = WM_APP + 0x100;
inline constexpr UINT WM_WDS_TAB_CHANGED = WM_APP + 0x101;

inline COLORREF BlendColor(const COLORREF from, const COLORREF to, double amount)
{
    const auto ch = [amount](const BYTE a, const BYTE b) -> BYTE
        {
            return static_cast<BYTE>(std::clamp<int>(static_cast<int>(a + (b - a) * amount), 0, 255));
        };
    return RGB(ch(GetRValue(from), GetRValue(to)), ch(GetGValue(from), GetGValue(to)), ch(GetBValue(from), GetBValue(to)));
}

// -----------------------------------------------------------------------------
//  CToolBar
// -----------------------------------------------------------------------------
class CToolBarButton
{
public:
    CToolBarButton(const UINT id, int image, std::wstring text = {})
        : m_id(id), m_image(image), m_text(std::move(text)) {}
    CToolBarButton(const UINT id, std::wstring text) : m_id(id), m_text(std::move(text)) {}

    UINT m_id;
    std::optional<int> m_image;
    std::wstring m_text;
};

class CToolBar : public MessageTarget<CToolBar, CWnd>
{
public:
    ~CToolBar() override
    {
        if (m_imageList != nullptr) ImageList_Destroy(m_imageList);
        if (m_disabledImageList != nullptr) ImageList_Destroy(m_disabledImageList);
    }

    void SetMetrics(const SIZE buttonSize, const int imageSize)
    {
        SetButtonSize(buttonSize);
        if (m_imageList != nullptr && !::ImageList_RemoveAll(m_imageList))
        {
            ImageList_Destroy(m_imageList);
            m_imageList = nullptr;
        }
        if (m_disabledImageList != nullptr && !::ImageList_RemoveAll(m_disabledImageList))
        {
            ImageList_Destroy(m_disabledImageList);
            m_disabledImageList = nullptr;
        }

        if (imageSize != m_imageSize)
        {
            m_imageSize = imageSize;
            RecreateImageLists();
        }
    }
    int AddImage(const CBitmap& bmp);

    bool Create(CFrameWnd* parent)
    {
        constexpr DWORD style = WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | TBSTYLE_LIST |
            CCS_NORESIZE | CCS_NOPARENTALIGN | CCS_NODIVIDER | TBSTYLE_TRANSPARENT;
        const CRect rect(0, 0, 0, 0);
        if (!CreateEx(0, TOOLBARCLASSNAMEW, nullptr, style, rect, parent, WDS_TOOLBAR_ID)) return false;
        SendNativeMessage(TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON));
        SendNativeMessage(TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS | TBSTYLE_EX_HIDECLIPPEDBUTTONS);
        SendNativeMessage(TB_SETIMAGELIST, 0, m_imageList);
        SendNativeMessage(TB_SETDISABLEDIMAGELIST, 0, m_disabledImageList);
        parent->SetTopBar(this);
        return true;
    }

    int ButtonCount() const { return static_cast<int>(SendNativeMessage(TB_BUTTONCOUNT)); }
    void ClearButtons()
    {
        for (int i = ButtonCount(); i-- > 0;) SendNativeMessage(TB_DELETEBUTTON, static_cast<WPARAM>(i));
        m_tips.clear();
    }
    void AddSeparator()
    {
        TBBUTTON b{};
        b.fsStyle = BTNS_SEP;
        b.iBitmap = MulDiv(m_imageSize, 8, 20);
        SendNativeMessage(TB_INSERTBUTTONW, ButtonCount(), &b);
    }
    void AddButton(const CToolBarButton& btn)
    {
        TBBUTTON b{};
        b.iBitmap = btn.m_image.value_or(I_IMAGENONE);
        b.idCommand = static_cast<int>(btn.m_id);
        b.fsState = 0;
        b.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
        if (!btn.m_image) b.fsStyle |= BTNS_SHOWTEXT;
        if (!btn.m_text.empty())
        {
            std::wstring wstr = btn.m_text;
            wstr.push_back(L'\0');
            const LRESULT idx = SendNativeMessage(TB_ADDSTRINGW, 0, wstr.c_str());
            b.iString = (idx != -1) ? idx : -1;
            m_tips[btn.m_id] = btn.m_text;
        }
        SendNativeMessage(TB_INSERTBUTTONW, ButtonCount(), &b);
    }
    void SetButtonSize(const SIZE buttonSize) noexcept { m_buttonSize = buttonSize; }
    void UpdateLayout()
    {
        SendNativeMessage(TB_SETIMAGELIST, 0, m_imageList);
        SendNativeMessage(TB_SETDISABLEDIMAGELIST, 0, m_disabledImageList);
        SendNativeMessage(TB_SETBITMAPSIZE, 0, MAKELPARAM(m_imageSize, m_imageSize));
        SendNativeMessage(TB_SETBUTTONSIZE, 0, MAKELPARAM(m_buttonSize.cx, m_buttonSize.cy));
        SendNativeMessage(TB_AUTOSIZE);
        auto* frame = static_cast<CFrameWnd*>(GetParent());
        frame->UpdateLayout();
        const auto toolbar = this;
        const HWND hWnd = m_hWnd;
        g_idleCmdUiUpdate = [toolbar, hWnd]
            {
                if (FindAttached(hWnd) != toolbar) return;
                toolbar->OnUpdateCmdUI(static_cast<CFrameWnd*>(toolbar->GetParent()));

            };
        OnUpdateCmdUI(frame);
        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    }
    int ButtonIndexForCommand(const UINT id) const { return static_cast<int>(SendNativeMessage(TB_COMMANDTOINDEX, id)); }
    bool GetButtonRect(const int index, LPRECT rect) const
    {
        return static_cast<bool>(SendNativeMessage(TB_GETITEMRECT, static_cast<WPARAM>(index),
            rect));
    }
    bool SetButtonVisible(const int index, const bool visible)
    {
        TBBUTTON button{};
        if (index < 0 || index >= ButtonCount() ||
            !SendNativeMessage(TB_GETBUTTON, static_cast<WPARAM>(index), &button))
            return false;

        const bool hidden = SendNativeMessage(TB_ISBUTTONHIDDEN, button.idCommand) != 0;
        if (hidden == !visible) return false;
        SendNativeMessage(TB_HIDEBUTTON, button.idCommand, MAKELPARAM(!visible, 0));
        return true;
    }
    CSize ButtonSize() const
    {
        if (ButtonCount() == 0) return m_buttonSize;
        const DWORD r = static_cast<DWORD>(SendNativeMessage(TB_GETBUTTONSIZE));
        const CSize sz(LOWORD(r), HIWORD(r));
        return sz.cx > 0 && sz.cy > 0 ? sz : m_buttonSize;
    }
    CSize PreferredSize() override
    {
        const CRect rcParent = GetParent()->ClientRect();
        return CSize(rcParent.Width(), ButtonSize().cy + ::ScaleForScreenDpi(2, m_hWnd));
    }

    void OnUpdateCmdUI(CFrameWnd* pTarget)
    {
        struct CToolBarCmdUI : CCmdUI
        {
            CToolBar* pBar = nullptr;
            void Enable(const bool bOn) override { pBar->SendNativeMessage(TB_ENABLEBUTTON, m_nID, MAKELONG(bOn, 0)); }
            void SetCheck(const int nCheck) override { pBar->SendNativeMessage(TB_CHECKBUTTON, m_nID, MAKELONG(nCheck != 0, 0)); }
            void SetRadio(const bool bOn) override { SetCheck(bOn); }
            void SetText(LPCWSTR) override {}
        };
        const int n = ButtonCount();
        CToolBarCmdUI state; state.pBar = this;
        for (int i = 0; i < n; ++i)
        {
            TBBUTTON b{}; SendNativeMessage(TB_GETBUTTON, static_cast<WPARAM>(i), &b);
            if (b.idCommand == 0 || (b.fsStyle & BTNS_SEP)) continue;
            state.m_nID = static_cast<UINT>(b.idCommand);
            state.m_nIndex = static_cast<UINT>(i);
            state.Update(pTarget, true);
        }
    }

    static std::span<const RouteEntry> Routes()
    {
        using ThisClass = CToolBar;
        static constexpr std::array entries
        {
            Route::ReflectNotify<&ThisClass::OnCustomDraw>(NM_CUSTOMDRAW),
            Route::ReflectNotify<&ThisClass::OnGetInfoTip>(TBN_GETINFOTIPW),
        };
        return entries;
    }

private:
    void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult) const;
    void OnGetInfoTip(NMHDR* pNMHDR, LRESULT* pResult)
    {
        const auto* tip = reinterpret_cast<LPNMTBGETINFOTIPW>(pNMHDR);
        if (const auto it = m_tips.find(static_cast<UINT>(tip->iItem)); it != m_tips.end())
            wcsncpy_s(tip->pszText, tip->cchTextMax, it->second.c_str(), _TRUNCATE);
        *pResult = 0;
    }

    void RecreateImageLists()
    {
        if (m_imageList) ImageList_Destroy(m_imageList);
        if (m_disabledImageList) ImageList_Destroy(m_disabledImageList);
        m_imageList = nullptr;
        m_disabledImageList = nullptr;

        m_imageList = ImageList_Create(m_imageSize, m_imageSize, ILC_COLOR32, 0, 16);
        if (m_imageList) ImageList_SetBkColor(m_imageList, CLR_NONE);

        m_disabledImageList = ImageList_Create(m_imageSize, m_imageSize, ILC_COLOR32, 0, 16);
        if (m_disabledImageList) ImageList_SetBkColor(m_disabledImageList, CLR_NONE);
    }

    HIMAGELIST m_imageList = nullptr;
    HIMAGELIST m_disabledImageList = nullptr;
    int m_imageSize = 16;
    CSize m_buttonSize{ 23, 22 };
    std::map<UINT, std::wstring> m_tips;
};

// -----------------------------------------------------------------------------
//  CStatusBar (custom drawn)
// -----------------------------------------------------------------------------
class CStatusBar : public MessageTarget<CStatusBar, CWnd>
{
public:
    enum class PaneId : char { Idle, Size, Ram };

    bool Create(CFrameWnd* pParentWnd)
    {
        static constexpr wchar_t kCls[] = L"WdsStatusBar32";
        WNDCLASSEXW wc{ sizeof(wc) };
        wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = FrameworkWindowProc;
        wc.hInstance = GetAppInstance();
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kCls;
        RegisterClassExW(&wc);
        const CRect rc(0, 0, 0, 0);
        if (!CreateEx(0, kCls, nullptr, WS_CHILD | WS_VISIBLE, rc, pParentWnd, WDS_STATUS_BAR_ID)) return false;
        pParentWnd->SetBottomBar(this);
        return true;
    }
    void SetPaneContent(const PaneId pane, const std::wstring_view text, const int width)
    {
        auto& target = m_panes[static_cast<size_t>(pane)];
        if (target.text == text && target.width == width) return;
        target.text.assign(text);
        target.width = width;
        Invalidate();
    }
    void SetBackgroundColor(const COLORREF color) { m_background = color; Invalidate(); }
    CRect PaneRect(const PaneId pane) const
    {
        return LayoutPanes()[static_cast<size_t>(pane)];
    }

    CSize PreferredSize() override
    {
        const CRect rcParent = GetParent()->ClientRect();
        return CSize(rcParent.Width(), ::ScaleForDpi(22, m_hWnd));
    }

    static void DrawPaneBorder(CDC& dc, CRect rect);

    static std::span<const RouteEntry> Routes()
    {
        using ThisClass = CStatusBar;
        static constexpr std::array entries
        {
            Route::Window<&ThisClass::OnPaint>(WM_PAINT),
            Route::Window<&ThisClass::OnEraseBkgnd>(WM_ERASEBKGND),
            Route::Window<&ThisClass::OnSize>(WM_SIZE),
        };
        return entries;
    }

private:
    struct Pane { int width = 100; std::wstring text; };

    bool OnEraseBkgnd(CDC*) { return true; }
    void OnSize(UINT, int, int) { Invalidate(); }
    void OnPaint();

    std::array<CRect, 3> LayoutPanes() const
    {
        std::array<CRect, 3> rects{};
        if (!IsWindow(m_hWnd)) return rects;
        const CRect rc = ClientRect();
        const int fixedTotal = m_panes[1].width + m_panes[2].width;
        const int stretchWidth = std::max(0, rc.Width() - fixedTotal);
        int x = rc.left;
        for (size_t i = 0; i < m_panes.size(); ++i)
        {
            const int w = i == 0 ? stretchWidth : m_panes[i].width;
            rects[i] = CRect(x, rc.top, x + w, rc.bottom);
            x += w;
        }
        rects.back().right = rc.right;   // last pane snaps to edge
        return rects;
    }
    std::array<Pane, 3> m_panes;
    COLORREF m_background = CLR_NONE;
};

// -----------------------------------------------------------------------------
//  CTabControl
// -----------------------------------------------------------------------------
class CTabControl : public MessageTarget<CTabControl, CWnd>
{
public:
    enum class Location : char { Bottom, Top };

    bool Create(const RECT& rect, CWnd* pParentWnd, UINT nID, bool focusTabStrip = false);
    int AddTab(CWnd* window, std::wstring_view label);
    bool SelectTab(const int i) { return ActivateTab(i, true); }
    void SetTabVisible(int i, bool show);

    void SetLocation(Location loc);
    void SetContentBackgroundColor(COLORREF color);
    int TabCount() const { return static_cast<int>(m_tabs.size()); }
    int ActiveTab() const { return m_activeTab; }
    CWnd* TabWindow(const int index) const { return index >= 0 && index < TabCount() ? m_tabs[index].window : nullptr; }
    bool IsTabVisible(const int index) const { return index >= 0 && index < TabCount() && m_tabs[index].visible; }
    std::wstring_view TabLabel(int index) const;
    void SetTabLabel(int i, std::wstring_view label);

    bool PreprocessMessage(MSG* pMsg) override;
    static std::span<const RouteEntry> Routes();

protected:
    LRESULT OnNcHitTest(CPoint point);
    void OnFontSizeChanged(int oldPercent, int newPercent) override;
    void OnSize(UINT, int, int);
    bool OnEraseBkgnd(CDC*) { return true; }
    void OnLButtonDown(UINT, CPoint point);
    void OnLButtonUp(UINT, CPoint);
    void OnSetFocus(CWnd*);
    void OnKillFocus(CWnd*);
    void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    void OnNativeSelChange(NMHDR*, LRESULT* pResult);
    void OnPaint();

private:
    struct TabInfo { CWnd* window = nullptr; std::wstring label; bool visible = true; CRect paintedRect; };

    static bool IsPlainTabTraversal(const MSG& msg);
    static bool IsDarkColor(COLORREF color);
    int NativeIndexFromLogical(int logical) const;
    void RebuildNativeTabs();
    void UpdateNativePadding();
    void SyncNativeSelection();
    bool ActivateTab(int i, bool syncNative);
    void NotifyParentOfTabChange(int activeTab) const;
    bool ShouldMoveFocusOnTabActivation(int previousActiveTab) const;
    bool ForwardKeyboardMessageToActiveTab(const MSG& msg);
    bool FocusActiveTabWindow();
    bool RedirectFocusAwayFromTabControl();
    bool GetNativeItemRect(int native, CRect& rc) const;
    CRect TabStripRect() const;
    void UpdatePaintedTabRects(const CRect& rcStrip, bool bottomTabs, bool labelOnlyTabs);
    bool UsesLabelOnlyTabs() const;
    int TabStripHeight() const { return ::ScaleForDpi(22, m_hWnd); }
    void LayoutPanes();

    std::vector<TabInfo> m_tabs;
    std::vector<int> m_visibleToLogical;
    int m_activeTab = -1;
    Location m_location = Location::Bottom;
    COLORREF m_paneBackgroundColor = CLR_NONE;
    bool m_handledPaintedTabMouseDown = false;
    bool m_focusTabStrip = false;
};

// -----------------------------------------------------------------------------
//  CPropertyPage / CPropertySheet
// -----------------------------------------------------------------------------
class CPropertySheet;

class CPropertyPage : public CDialog
{
public:
    explicit CPropertyPage(const UINT templateId) : CDialog(templateId) {}

    bool Create(CWnd* parent);

    void SetModified(bool bChanged = true);
    void OnOK() override {}        // a page does not end its own dialog
    void OnCancel() override {}

private:
    friend class CPropertySheet;
    bool m_bModified = false;
};

class CPropertySheet : public MessageTarget<CPropertySheet, CWnd>
{
public:
    explicit CPropertySheet(const LPCWSTR caption) : m_caption(caption) {}

    template<typename Page, typename... Args>
        requires std::derived_from<Page, CPropertyPage>
    void AddPage(Args&&... args)
    {
        m_pages.push_back(std::make_unique<Page>(std::forward<Args>(args)...));
    }
    int PageCount() const { return static_cast<int>(m_pages.size()); }
    int ActivePageIndex() const { return m_currentPage >= 0 ? m_currentPage : m_tab.ActiveTab(); }
    bool SelectPage(int i);
    CTabControl& TabControl() { return m_tab; }

    void UpdateApplyButton();

    virtual INT_PTR ShowModal();

    static std::span<const RouteEntry> Routes();

protected:
    virtual bool OnInitDialog();
    bool PreprocessMessage(MSG* pMsg) override;
    bool OnEraseBkgnd(CDC* pDC) const;
    HBRUSH OnCtlColor(CDC*, CWnd*, UINT) { return reinterpret_cast<HBRUSH>(CallDefaultHandler()); }
    LRESULT OnTabChanged(WPARAM w, LPARAM);
    void OnClose() { RequestModalExit(IDCANCEL); }
    void RequestModalExit(const int result) { m_modalResult = result; }
    bool OnCommand(WPARAM wParam, LPARAM lParam) override;

private:
    bool EnsurePageCreated(int i);
    bool ActivatePage(int active);
    void SyncTabSelection(int active);
    void ApplyPages();

    std::vector<std::unique_ptr<CPropertyPage>> m_pages;
    CTabControl m_tab;
    HWND m_applyButton = nullptr;
    CRect m_pageRect;
    std::wstring m_caption;
    int m_currentPage = -1;
    int m_pendingActivePage = 0;
    int m_modalResult = -1;
    bool m_syncingTabSelection = false;
};

// -----------------------------------------------------------------------------
//  Application command and resource IDs
// -----------------------------------------------------------------------------
inline constexpr UINT IDS_APP_TITLE = 0xE000;
inline constexpr UINT ID_SEPARATOR = 0;
inline constexpr UINT ID_APP_ABOUT = 0xE140;
inline constexpr UINT ID_APP_EXIT = 0xE141;
inline constexpr UINT ID_VIEW_TOOLBAR = 0xE800;
inline constexpr UINT ID_VIEW_STATUS_BAR = 0xE801;

// CWinApp's routes are defined here because ID_APP_EXIT must be visible first.
// It is inline because this header is included by multiple translation units.
inline std::span<const RouteEntry> CWinApp::Routes()
{
    using ThisClass = CWinApp;
    static constexpr std::array entries
    {
        Route::Command<&ThisClass::OnAppExit>(ID_APP_EXIT),
    };
    return entries;
}

// CFrameWnd's routes handle ID_VIEW_TOOLBAR and ID_VIEW_STATUS_BAR.
// It is inline for the same reason as CWinApp's table above.
inline std::span<const RouteEntry> CFrameWnd::Routes()
{
    using ThisClass = CFrameWnd;
    static constexpr std::array entries
    {
        Route::Command<&ThisClass::OnBarCheck>(ID_VIEW_TOOLBAR, ID_VIEW_STATUS_BAR),
        Route::Update<&ThisClass::OnUpdateControlBarMenu>(ID_VIEW_TOOLBAR, ID_VIEW_STATUS_BAR),
    };
    return entries;
}

inline void CFrameWnd::OnBarCheck(const UINT nID)
{
    CWnd* bar = nID == ID_VIEW_TOOLBAR ? m_topBar : nID == ID_VIEW_STATUS_BAR ? m_bottomBar : nullptr;
    if (bar == nullptr) return;
    bar->ShowWindow(bar->IsWindowVisible() ? SW_HIDE : SW_SHOW);
    UpdateLayout();
}

inline void CFrameWnd::OnUpdateControlBarMenu(CCmdUI* pCmdUI) const
{
    const CWnd* bar = pCmdUI->m_nID == ID_VIEW_TOOLBAR ? m_topBar :
        pCmdUI->m_nID == ID_VIEW_STATUS_BAR ? m_bottomBar : nullptr;
    pCmdUI->Enable(bar != nullptr);
    if (bar != nullptr) pCmdUI->SetCheck(bar->IsWindowVisible() ? 1 : 0);
}
