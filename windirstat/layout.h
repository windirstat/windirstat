// layout.h - Declaration of CLayout
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
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

#pragma once

#include <limits>

//
// CLayout. A poor men's dialog layout mechanism.
// Simple, flat, and sufficient for our purposes.
//
class CLayout
{
    template <typename T>
    struct SControlInfoT
    {
        CWnd* control;
        T movex;
        T movey;
        T stretchx;
        T stretchy;

        CRect originalRectangle;

        SControlInfoT()
            : control(nullptr)
              , movex(std::numeric_limits<T>::quiet_NaN())
              , movey(std::numeric_limits<T>::quiet_NaN())
              , stretchx(std::numeric_limits<T>::quiet_NaN())
              , stretchy(std::numeric_limits<T>::quiet_NaN())
        {
        }

        SControlInfoT(CWnd* ctl, T& x, T& y, T& w, T& h)
            : control(ctl)
              , movex(x)
              , movey(y)
              , stretchx(w)
              , stretchy(h)
        {
        }

    private:
        SControlInfoT(SControlInfoT&) = default; // hide copy ctor
    };

    using SControlInfo = SControlInfoT<double>;

    class CSizeGripper : public CWnd
    {
    public:
        static const int _width;

        CSizeGripper() = default;
        void Create(CWnd* parent, CRect rc);

    private:
        static void DrawShadowLine(CDC* pdc, CPoint start, CPoint end);

        DECLARE_MESSAGE_MAP()
        afx_msg void OnPaint();
        afx_msg LRESULT OnNcHitTest(CPoint point);
    };

    class CPositioner
    {
    public:
        CPositioner(int nNumWindows = 10);
        virtual ~CPositioner();
        void SetWindowPos(HWND hWnd, int x, int y, int cx, int cy, UINT uFlags);

    private:
        HDWP m_wdp;
    };

public:
    CLayout(CWnd* dialog, LPCWSTR name);
    int AddControl(CWnd* control, double movex, double movey, double stretchx, double stretchy);
    void AddControl(UINT id, double movex, double movey, double stretchx, double stretchy);

    void OnInitDialog(bool centerWindow);
    void OnSize();
    void OnGetMinMaxInfo(MINMAXINFO* mmi) const;
    void OnDestroy() const;

protected:
    CWnd* m_dialog;
    CStringW m_name;
    CSize m_originalDialogSize;
    CArray<SControlInfo, SControlInfo&> m_control;
    CSizeGripper m_sizeGripper;
};
