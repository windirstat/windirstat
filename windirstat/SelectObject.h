// SelectObject.h - Declaration and implementation of Device Context helper classes.
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

// Example:
// Instead of writing
//     CGdiObject *old = pdc->SelectObject(&brush); ...; pdc->SelectObject(old);
// we can simply write
//     CSelectObject sobrush(pdc, &brush);
// and the destructor will reselect the old object.

#pragma once

#include "stdafx.h"

class CSelectObject final
{
public:
    CSelectObject(CDC* pdc, CGdiObject* pObject)
    {
        m_POldObject = pdc->SelectObject(pObject);
        m_Pdc        = pdc;
    }

    ~CSelectObject()
    {
        m_Pdc->SelectObject(m_POldObject);
    }

protected:
    CDC* m_Pdc;
    CGdiObject* m_POldObject;
};

class CSelectStockObject final
{
public:
    CSelectStockObject(CDC* pdc, const int nIndex)
    {
        m_POldObject = pdc->SelectStockObject(nIndex);
        m_Pdc = pdc;
    }

    ~CSelectStockObject()
    {
        m_Pdc->SelectObject(m_POldObject);
    }

protected:
    CDC* m_Pdc;
    CGdiObject* m_POldObject;
};

class CSetBkMode final
{
public:
    CSetBkMode(CDC* pdc, const int mode)
    {
        m_Pdc = pdc;
        m_OldMode = pdc->SetBkMode(mode);
    }

    ~CSetBkMode()
    {
        m_Pdc->SetBkMode(m_OldMode);
    }

protected:
    CDC* m_Pdc;
    int m_OldMode;
};

class CSetTextColor final
{
public:
    CSetTextColor(CDC* pdc, const COLORREF color)
    {
        m_Pdc = pdc;
        m_OldColor = pdc->SetTextColor(color);
    }

    ~CSetTextColor()
    {
        m_Pdc->SetTextColor(m_OldColor);
    }

protected:
    CDC* m_Pdc;
    COLORREF m_OldColor;
};

class CSetBkColor final
{
public:
    CSetBkColor(CDC* pdc, const COLORREF color)
    {
        m_Pdc = pdc;
        m_OldColor = pdc->SetBkColor(color);
    }

    ~CSetBkColor()
    {
        m_Pdc->SetBkColor(m_OldColor);
    }

protected:
    CDC* m_Pdc;
    COLORREF m_OldColor;
};

class CSaveDC final
{
public:
    CSaveDC(CDC* pdc)
    {
        m_Pdc= pdc;
        m_Save = pdc->SaveDC();
    }

    ~CSaveDC()
    {
        m_Pdc->RestoreDC(m_Save);
    }

protected:
    CDC* m_Pdc;
    int m_Save;
};

inline BOOL CreateRectRgn(CRgn& rgn, const CRect rc)
{
    return rgn.CreateRectRgn(rc.left, rc.top, rc.right, rc.bottom);
}

inline COLORREF MakeShadowColor(const COLORREF c, const int percent)
{
    return RGB(
        RGB_GET_RVALUE(c) * percent / 100,
        RGB_GET_GVALUE(c) * percent / 100,
        RGB_GET_BVALUE(c) * percent / 100
    );
}
