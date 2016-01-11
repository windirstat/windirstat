// selectobject.h - Declaration and implementation of Device Context helper classes.
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2016 WinDirStat team (windirstat.info)
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

#ifndef __WDS_SELECTOBJECT_H__
#define __WDS_SELECTOBJECT_H__
#pragma once
#include "stdafx.h"

class CSelectObject
{
public:
    CSelectObject(CDC *pdc, CGdiObject *pObject)
    { m_pOldObject = pdc->SelectObject(pObject); m_pdc = pdc; }
    ~CSelectObject()
    { m_pdc->SelectObject(m_pOldObject); }
protected:
    CDC *m_pdc;
    CGdiObject *m_pOldObject;
};

class CSelectStockObject
{
public:
    CSelectStockObject(CDC *pdc, int nIndex)
    { m_pOldObject = pdc->SelectStockObject(nIndex); m_pdc = pdc; }
    ~CSelectStockObject()
    { m_pdc->SelectObject(m_pOldObject); }
protected:
    CDC *m_pdc;
    CGdiObject *m_pOldObject;
};

class CSetBkMode
{
public:
    CSetBkMode(CDC *pdc, int mode)
    { m_pdc = pdc; m_oldMode = pdc->SetBkMode(mode); }
    ~CSetBkMode()
    { m_pdc->SetBkMode(m_oldMode); }
protected:
    CDC *m_pdc;
    int m_oldMode;
};

class CSetTextColor
{
public:
    CSetTextColor(CDC *pdc, COLORREF color)
    { m_pdc = pdc; m_oldColor = pdc->SetTextColor(color); }
    ~CSetTextColor()
    { m_pdc->SetTextColor(m_oldColor); }
protected:
    CDC *m_pdc;
    COLORREF m_oldColor;
};

class CSetBkColor
{
public:
    CSetBkColor(CDC *pdc, COLORREF color)
    { m_pdc = pdc; m_oldColor = pdc->SetBkColor(color); }
    ~CSetBkColor()
    { m_pdc->SetBkColor(m_oldColor); }
protected:
    CDC *m_pdc;
    COLORREF m_oldColor;
};

class CSaveDC
{
public:
    CSaveDC(CDC *pdc) { m_pdc = pdc; m_save = pdc->SaveDC(); }
    ~CSaveDC() { m_pdc->RestoreDC(m_save); }
protected:
    CDC *m_pdc;
    int m_save;
};

inline BOOL CreateRectRgn(CRgn& rgn, CRect rc)
{
    return rgn.CreateRectRgn(rc.left, rc.top, rc.right, rc.bottom);
}

inline COLORREF MakeShadowColor(COLORREF c, int percent)
{
    return RGB(
        RGB_GET_RVALUE(c) * percent / 100,
        RGB_GET_GVALUE(c) * percent / 100,
        RGB_GET_BVALUE(c) * percent / 100
    );
}

#endif // __WDS_SELECTOBJECT_H__
