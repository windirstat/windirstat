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

#include "UiWindow.h"

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

class CButton : public MessageTarget<CButton, CWnd>
{
public:
    bool Create(const LPCWSTR lpszCaption, const DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, const UINT nID)
    {
        return CreateEx(0, WC_BUTTONW, lpszCaption, dwStyle, rect, pParentWnd, nID);
    }
    HICON SetIcon(HICON hIcon) { return reinterpret_cast<HICON>(SendNativeMessage(BM_SETIMAGE, IMAGE_ICON, hIcon)); }
    // Offset in 96-DPI units, also scaled with the application font size.
    void SetTextOffset(const CPoint& offset);
    static std::span<const RouteEntry> Routes();

private:
    bool OnCustomDraw(UINT id, NMHDR* header, LRESULT* result);
    CPoint m_textOffset;
    HDC m_textDrawDc = nullptr;
    CPoint m_textDrawOrigin;
};

class CEdit : public CWnd
{
public:
    bool Create(const DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, const UINT nID)
    {
        return CreateEx(0, WC_EDITW, nullptr, dwStyle, rect, pParentWnd, nID);
    }
    void SetSel(const int start, const int end) { SendNativeMessage(EM_SETSEL, static_cast<WPARAM>(start), static_cast<LPARAM>(end)); }
    std::pair<int, int> GetSel() const { const DWORD selection = static_cast<DWORD>(SendNativeMessage(EM_GETSEL)); return { LOWORD(selection), HIWORD(selection) }; }
};

class CComboBox final : public CWnd
{
public:
    bool Create(const DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, const UINT nID)
    {
        return CreateEx(0, WC_COMBOBOXW, nullptr, dwStyle, rect, pParentWnd, nID);
    }
    int  AddString(LPCWSTR psz) { return static_cast<int>(SendNativeMessage(CB_ADDSTRING, 0, psz)); }
    int  AddString(const std::wstring& s) { return AddString(s.c_str()); }
    int  DeleteString(const UINT i) { return static_cast<int>(SendNativeMessage(CB_DELETESTRING, static_cast<WPARAM>(i))); }
    int  GetCount() const { return static_cast<int>(SendNativeMessage(CB_GETCOUNT)); }
    int  GetCurSel() const { return static_cast<int>(SendNativeMessage(CB_GETCURSEL)); }
    int  SetCurSel(const int i) { return static_cast<int>(SendNativeMessage(CB_SETCURSEL, static_cast<WPARAM>(i))); }
    DWORD_PTR GetItemData(const int i) const { return static_cast<DWORD_PTR>(SendNativeMessage(CB_GETITEMDATA, static_cast<WPARAM>(i))); }
    int  SetItemData(const int i, const DWORD_PTR data) { return static_cast<int>(SendNativeMessage(CB_SETITEMDATA, static_cast<WPARAM>(i), static_cast<LPARAM>(data))); }
    bool GetDroppedState() const { return static_cast<bool>(SendNativeMessage(CB_GETDROPPEDSTATE)); }
    void ResetContent() { SendNativeMessage(CB_RESETCONTENT); }
    std::wstring GetItemText(const int i) const
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
    int  AddString(const std::wstring& s) { return AddString(s.c_str()); }
    int  InsertString(const int i, LPCWSTR psz) { return static_cast<int>(SendNativeMessage(LB_INSERTSTRING, static_cast<WPARAM>(i), psz)); }
    int  InsertString(const int i, const std::wstring& s) { return InsertString(i, s.c_str()); }
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
    std::pair<int, int> GetRange() const { PBRANGE range{}; SendNativeMessage(PBM_GETRANGE, true, &range); return { range.iLow, range.iHigh }; }
    void SetRange(const int nMin, const int nMax) { SendNativeMessage(PBM_SETRANGE32, static_cast<WPARAM>(nMin), static_cast<LPARAM>(nMax)); }
    COLORREF SetBkColor(const COLORREF c) { return static_cast<COLORREF>(SendNativeMessage(PBM_SETBKCOLOR, 0, static_cast<LPARAM>(c))); }
    void SetMarquee(const bool on, const int ms) { SendNativeMessage(PBM_SETMARQUEE, static_cast<WPARAM>(on), static_cast<LPARAM>(ms)); }
};

class CSliderCtrl final : public MessageTarget<CSliderCtrl, CWnd>
{
public:
    void SetRange(const int nMin, const int nMax, const bool bRedraw = false) { SendNativeMessage(TBM_SETRANGE, static_cast<WPARAM>(bRedraw), MAKELPARAM(nMin, nMax)); }
    int  GetPos() const { return static_cast<int>(SendNativeMessage(TBM_GETPOS)); }
    void SetPos(const int n) { SendNativeMessage(TBM_SETPOS, true, static_cast<LPARAM>(n)); }
    void SetPageSize(const int n) { SendNativeMessage(TBM_SETPAGESIZE, 0, static_cast<LPARAM>(n)); }
    static std::span<const RouteEntry> Routes();

private:
    bool OnCustomDraw(UINT id, NMHDR* header, LRESULT* result);
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
    std::wstring GetTextRange(const long cpMin, const long cpMax) const
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
    bool AddTool(WindowRef window, const LPCWSTR lpszText)
    {
        if (window == nullptr) return false;
        TTTOOLINFOW ti{}; ti.cbSize = sizeof(ti);
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = window.Handle();
        ti.uId = reinterpret_cast<UINT_PTR>(window.Handle());
        ti.lpszText = const_cast<LPWSTR>(lpszText);
        return static_cast<bool>(SendNativeMessage(TTM_ADDTOOLW, 0, &ti));
    }
    bool AddTool(WindowRef window, const std::wstring& text) { return AddTool(window, text.c_str()); }
    bool AddTool(WindowRef window, const UINT_PTR id, const RECT& rect, const LPCWSTR lpszText)
    {
        if (window == nullptr) return false;
        TTTOOLINFOW ti{}; ti.cbSize = sizeof(ti);
        ti.uFlags = TTF_SUBCLASS;
        ti.hwnd = window.Handle();
        ti.uId = id;
        ti.rect = rect;
        ti.lpszText = const_cast<LPWSTR>(lpszText);
        return static_cast<bool>(SendNativeMessage(TTM_ADDTOOLW, 0, &ti));
    }
    void UpdateTipText(WindowRef window, const std::wstring& text)
    {
        if (window == nullptr) return;
        TTTOOLINFOW ti{}; ti.cbSize = sizeof(ti);
        ti.uFlags = TTF_IDISHWND;
        ti.hwnd = window.Handle();
        ti.uId = reinterpret_cast<UINT_PTR>(window.Handle());
        ti.lpszText = const_cast<LPWSTR>(text.c_str());
        SendNativeMessage(TTM_UPDATETIPTEXTW, 0, &ti);
    }
    void Activate() { SendNativeMessage(TTM_ACTIVATE, true); }
    void Pop() { SendNativeMessage(TTM_POP); }
    void SetToolRect(WindowRef window, const UINT_PTR id, const RECT& rect)
    {
        if (window == nullptr) return;
        TTTOOLINFOW ti{}; ti.cbSize = sizeof(ti);
        ti.hwnd = window.Handle();
        ti.uId = id;
        ti.rect = rect;
        SendNativeMessage(TTM_NEWTOOLRECTW, 0, &ti);
    }
    void SetMaxTipWidth(const int w) { SendNativeMessage(TTM_SETMAXTIPWIDTH, 0, static_cast<LPARAM>(w)); }
    void RelayEvent(MSG* pMsg) { SendNativeMessage(TTM_RELAYEVENT, 0, pMsg); }
};

class CHeaderCtrl final : public WindowRef
{
public:
    using WindowRef::WindowRef;
    int  GetItemCount() const { return static_cast<int>(SendMessage(HDM_GETITEMCOUNT)); }
    bool GetItem(const int i, HDITEMW* p) const { return static_cast<bool>(SendMessage(HDM_GETITEMW, static_cast<WPARAM>(i), p)); }
    bool SetItem(const int i, const HDITEMW* p) noexcept { return static_cast<bool>(SendMessage(HDM_SETITEMW, static_cast<WPARAM>(i), p)); }
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
    bool GetItemRect(const int i, RECT& r, const UINT code = LVIR_BOUNDS) const
    {
        if (i == g_drawItemCtx.item && m_hWnd == g_drawItemCtx.hWnd
            && (code == LVIR_LABEL || code == LVIR_BOUNDS))
        {
            EnsureDrawColCache();
            if (g_drawItemCtx.colCount > 0 && g_drawItemCtx.colValid[0])
            {
                r.top = g_drawItemCtx.rcItem.top;
                r.bottom = g_drawItemCtx.rcItem.bottom;
                r.left = (code == LVIR_LABEL) ? g_drawItemCtx.colLeft[0] : g_drawItemCtx.rcItem.left;
                // LVIR_LABEL = column-0 width; LVIR_BOUNDS = full row
                r.right = (code == LVIR_LABEL)
                    ? g_drawItemCtx.colRight[0]
                    : g_drawItemCtx.rcItem.right;
                return true;
            }
        }
        // Fallback: set r.left = code before sending per ListView_GetItemRect convention
        r.left = static_cast<LONG>(code);
        return static_cast<bool>(SendNativeMessage(LVM_GETITEMRECT, static_cast<WPARAM>(i), &r));
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
    int  HitTest(const CPoint& pt, UINT* pFlags = nullptr) const { LVHITTESTINFO h{}; h.pt = pt; const int r = static_cast<int>(SendNativeMessage(LVM_HITTEST, 0, &h)); if (pFlags) *pFlags = h.flags; return r; }
    void RedrawItems(const int first, const int last) { SendNativeMessage(LVM_REDRAWITEMS, static_cast<WPARAM>(first), (LPARAM)last); }
    bool Scroll(const CSize& size) { return static_cast<bool>(SendNativeMessage(LVM_SCROLL, static_cast<WPARAM>(size.cx), (LPARAM)size.cy)); }
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
    int  InsertColumn(const int nCol, const std::wstring& text, const int fmt = LVCFMT_LEFT, const int width = -1, const int sub = -1)
    {
        return InsertColumn(nCol, text.c_str(), fmt, width, sub);
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

    CHeaderCtrl GetHeader() const
    {
        return CHeaderCtrl(reinterpret_cast<HWND>(SendNativeMessage(LVM_GETHEADER)));
    }

    int GetFirstSelectedIndex() const { return GetNextItem(-1, LVNI_SELECTED); }
    int GetNextSelectedIndex(const int index) const { return GetNextItem(index, LVNI_SELECTED); }

protected:

    // Cache visual header rectangles by logical subitem. HDM_GETITEMRECT accounts
    // for both user-reordered columns and horizontal scrolling; accumulating widths
    // by logical index does not.
    // Called at most once per WM_DRAWITEM dispatch; subsequent calls are no-ops.
    void EnsureDrawColCache() const
    {
        if (g_drawItemCtx.colCount != 0 || m_hWnd != g_drawItemCtx.hWnd) return;
        const HWND hdr = GetHeader().m_hWnd;
        if (hdr == nullptr) return;
        const int n = static_cast<int>(::SendMessageW(hdr, HDM_GETITEMCOUNT, 0, 0));
        constexpr int cap = static_cast<int>(std::size(g_drawItemCtx.colLeft));
        const int cols = std::clamp(n, 0, cap);
        POINT headerOrigin{};
        MapWindowPoints(hdr, m_hWnd, &headerOrigin, 1);
        for (const int c : std::views::iota(0, cols))
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
    const ScopedValue<bool> m_waitCursorActive{ g_waitCursorActive, true };
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
    CBufferedDC(CDC& target, WindowRef window)
        : CDC(&target), m_target(target), m_bitmap(&target, window.GetClientRect().Width(),
            window.GetClientRect().Height()), m_rect(window.GetClientRect())
    {
        if (!*this || !m_bitmap) return;
        const HGDIOBJ previousBitmap = SelectObject(m_hDC, m_bitmap);
        if (previousBitmap == nullptr || previousBitmap == HGDI_ERROR) return;
        m_previousBitmap = static_cast<HBITMAP>(previousBitmap);
        SetViewportOrg(-m_rect.left, -m_rect.top);
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
