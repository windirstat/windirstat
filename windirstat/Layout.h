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

//
// CLayout. A poor men's dialog layout mechanism.
// Simple, flat, and sufficient for our purposes.
//
class CLayout final
{
    template <typename T>
    struct SControlInfoT
    {
        HWND control;
        T movex;
        T movey;
        T stretchx;
        T stretchy;

        CRect originalRectangle;

        SControlInfoT(HWND ctl, const T& x, const T& y, const T& w, const T& h)
            : control(ctl)
              , movex(x)
              , movey(y)
              , stretchx(w)
              , stretchy(h)
        {
        }
    };

    using SControlInfo = SControlInfoT<double>;

    class CSizeGripper final : public MessageTarget<CSizeGripper, CWnd>
    {
    public:
        const int m_width = ScaleForDpi(14);

        CSizeGripper() = default;
        void Create(CWnd* parent, CRect rc);

    private:
        static void DrawShadowLine(CDC* pdc, CPoint start, CPoint end);

    public:
        static std::span<const RouteEntry> Routes();

    protected:
        void OnPaint();
        LRESULT OnNcHitTest(CPoint point) const;
        bool OnEraseBkgnd(CDC* pDC) const;
    };

    class CPositioner final
    {
    public:
        CPositioner(int nNumWindows = 10);
        ~CPositioner();
        void SetWindowPos(HWND hWnd, int x, int y, int cx, int cy, UINT uFlags);

    private:
        HDWP m_wdp;
    };

public:
    CLayout(CWnd* dialog, RECT* placement);
    int AddControl(CWnd* control, double movex, double movey, double stretchx, double stretchy);
    void AddControl(UINT id, double movex, double movey, double stretchx, double stretchy);

    void OnInitDialog(bool centerWindow);
    void OnSize();
    void OnGetMinMaxInfo(MINMAXINFO* mmi) const;
    void OnDestroy() const;

protected:
    RECT* m_wp;
    CWnd* m_dialog;
    CSize m_originalDialogSize;
    std::vector<SControlInfo> m_control;
    CSizeGripper m_sizeGripper;
};

//
// CLayoutDialog. A class that provides automatic layout management
// for dialogs. Inherit from this class instead of CDialog to get automatic
// m_layout support with OnSize, OnGetMinMaxInfo, and OnDestroy handling.
//
class CLayoutDialog : public MessageTarget<CLayoutDialog, CDialog>
{
protected:
    CLayout m_layout;

    // Constructor that takes dialog ID and window placement
    CLayoutDialog(const UINT nIDTemplate, RECT* placement, CWnd* pParent = nullptr)
        : MessageTarget(nIDTemplate, pParent)
        , m_layout(this, placement)
    {
    }

    // Message handlers
public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnSize(UINT nType, int cx, int cy);
    void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    void OnDestroy();
};

inline std::span<const RouteEntry> CLayoutDialog::Routes()
{
    using ThisClass = CLayoutDialog;
    static constexpr std::array entries
    {
        Route::Window<&ThisClass::OnSize>(WM_SIZE),
        Route::Window<&ThisClass::OnGetMinMaxInfo>(WM_GETMINMAXINFO),
        Route::Window<&ThisClass::OnDestroy>(WM_DESTROY),
    };
    return entries;
}

inline std::span<const RouteEntry> CLayout::CSizeGripper::Routes()
{
    using ThisClass = CSizeGripper;
    static constexpr std::array entries
    {
        Route::Window<&ThisClass::OnPaint>(WM_PAINT),
        Route::Window<&ThisClass::OnNcHitTest>(WM_NCHITTEST),
        Route::Window<&ThisClass::OnEraseBkgnd>(WM_ERASEBKGND),
    };
    return entries;
}
