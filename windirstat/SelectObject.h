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

// Example:
// Instead of writing
//     CGdiObject *old = pdc->SelectObject(&brush); ...; pdc->SelectObject(old);
// we can simply write
//     CSelectObject sobrush(pdc, &brush);
// and the destructor will reselect the old object.

#pragma once

#include "pch.h"

class CSelectObject final
{
public:
    CSelectObject(CDC* pdc, CGdiObject* pObject)
    {
        m_pOldObject = pdc->SelectObject(pObject);
        m_pdc        = pdc;
    }

    CSelectObject(const CSelectObject&) = delete;
    CSelectObject& operator=(const CSelectObject&) = delete;
    CSelectObject(CSelectObject&&) = delete;
    CSelectObject& operator=(CSelectObject&&) = delete;

    ~CSelectObject()
    {
        if (m_pdc != nullptr && m_pOldObject != nullptr)
        {
            m_pdc->SelectObject(m_pOldObject);
        }
    }

protected:
    CDC* m_pdc = nullptr;
    CGdiObject* m_pOldObject = nullptr;
};

class CSelectStockObject final
{
public:
    CSelectStockObject(CDC* pdc, const int nIndex)
    {
        m_pOldObject = pdc->SelectStockObject(nIndex);
        m_pdc = pdc;
    }

    CSelectStockObject(const CSelectStockObject&) = delete;
    CSelectStockObject& operator=(const CSelectStockObject&) = delete;
    CSelectStockObject(CSelectStockObject&&) = delete;
    CSelectStockObject& operator=(CSelectStockObject&&) = delete;

    ~CSelectStockObject()
    {
        if (m_pdc != nullptr && m_pOldObject != nullptr)
        {
            m_pdc->SelectObject(m_pOldObject);
        }
    }

protected:
    CDC* m_pdc = nullptr;
    CGdiObject* m_pOldObject = nullptr;
};

class CSetBkMode final
{
public:
    CSetBkMode(CDC* pdc, const int mode)
    {
        m_pdc = pdc;
        m_oldMode = pdc->SetBkMode(mode);
    }

    CSetBkMode(const CSetBkMode&) = delete;
    CSetBkMode& operator=(const CSetBkMode&) = delete;
    CSetBkMode(CSetBkMode&&) = delete;
    CSetBkMode& operator=(CSetBkMode&&) = delete;

    ~CSetBkMode()
    {
        if (m_pdc != nullptr)
        {
            m_pdc->SetBkMode(m_oldMode);
        }
    }

protected:
    CDC* m_pdc = nullptr;
    int m_oldMode = 0;
};

class CSetTextColor final
{
public:
    CSetTextColor(CDC* pdc, const COLORREF color)
    {
        m_pdc = pdc;
        m_oldColor = pdc->SetTextColor(color);
    }

    CSetTextColor(const CSetTextColor&) = delete;
    CSetTextColor& operator=(const CSetTextColor&) = delete;
    CSetTextColor(CSetTextColor&&) = delete;
    CSetTextColor& operator=(CSetTextColor&&) = delete;

    ~CSetTextColor()
    {
        if (m_pdc != nullptr)
        {
            m_pdc->SetTextColor(m_oldColor);
        }
    }

protected:
    CDC* m_pdc = nullptr;
    COLORREF m_oldColor = CLR_NONE;
};

class CSetBkColor final
{
public:
    CSetBkColor(CDC* pdc, const COLORREF color)
    {
        m_pdc = pdc;
        m_oldColor = pdc->SetBkColor(color);
    }

    CSetBkColor(const CSetBkColor&) = delete;
    CSetBkColor& operator=(const CSetBkColor&) = delete;
    CSetBkColor(CSetBkColor&&) = delete;
    CSetBkColor& operator=(CSetBkColor&&) = delete;

    ~CSetBkColor()
    {
        if (m_pdc != nullptr)
        {
            m_pdc->SetBkColor(m_oldColor);
        }
    }

protected:
    CDC* m_pdc = nullptr;
    COLORREF m_oldColor = CLR_NONE;
};

class CSaveDC final
{
public:
    CSaveDC(CDC* pdc)
    {
        m_pdc= pdc;
        m_save = pdc->SaveDC();
    }

    CSaveDC(const CSaveDC&) = delete;
    CSaveDC& operator=(const CSaveDC&) = delete;
    CSaveDC(CSaveDC&&) = delete;
    CSaveDC& operator=(CSaveDC&&) = delete;

    ~CSaveDC()
    {
        if (m_pdc != nullptr)
        {
            m_pdc->RestoreDC(m_save);
        }
    }

protected:
    CDC* m_pdc = nullptr;
    int m_save = 0;
};

inline BOOL CreateRectRgn(CRgn& rgn, const CRect rc)
{
    return rgn.CreateRectRgn(rc.left, rc.top, rc.right, rc.bottom);
}

inline COLORREF MakeShadowColor(const COLORREF c, const int percent)
{
    return RGB(
        GetRValue(c) * percent / 100,
        GetGValue(c) * percent / 100,
        GetBValue(c) * percent / 100
    );
}
