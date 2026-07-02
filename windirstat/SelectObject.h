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
    CSelectObject(CDC* pdc, CGdiObject* pObject) noexcept
    {
        m_pOldObject = pdc->SelectObject(pObject);
        m_pdc        = pdc;
    }

    CSelectObject(const CSelectObject&) = delete;
    CSelectObject& operator=(const CSelectObject&) = delete;
    CSelectObject(CSelectObject&&) = delete;
    CSelectObject& operator=(CSelectObject&&) = delete;

    ~CSelectObject() noexcept
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
    CSelectStockObject(CDC* pdc, const int nIndex) noexcept
    {
        m_pOldObject = pdc->SelectStockObject(nIndex);
        m_pdc = pdc;
    }

    CSelectStockObject(const CSelectStockObject&) = delete;
    CSelectStockObject& operator=(const CSelectStockObject&) = delete;
    CSelectStockObject(CSelectStockObject&&) = delete;
    CSelectStockObject& operator=(CSelectStockObject&&) = delete;

    ~CSelectStockObject() noexcept
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

// Sets a DC attribute in the constructor and restores the previous
// value in the destructor (e.g. SetBkMode, SetTextColor, SetBkColor).
template <typename V, V (CDC::* Setter)(V)>
class CSetDCAttribute final
{
public:
    CSetDCAttribute(CDC* pdc, const V value) noexcept : m_pdc(pdc)
    {
        m_oldValue = (pdc->*Setter)(value);
    }

    CSetDCAttribute(const CSetDCAttribute&) = delete;
    CSetDCAttribute& operator=(const CSetDCAttribute&) = delete;
    CSetDCAttribute(CSetDCAttribute&&) = delete;
    CSetDCAttribute& operator=(CSetDCAttribute&&) = delete;

    ~CSetDCAttribute() noexcept
    {
        if (m_pdc != nullptr)
        {
            (m_pdc->*Setter)(m_oldValue);
        }
    }

protected:
    CDC* m_pdc = nullptr;
    V m_oldValue{};
};

using CSetBkMode = CSetDCAttribute<int, &CDC::SetBkMode>;
using CSetTextColor = CSetDCAttribute<COLORREF, &CDC::SetTextColor>;
using CSetBkColor = CSetDCAttribute<COLORREF, &CDC::SetBkColor>;

class CSaveDC final
{
public:
    CSaveDC(CDC* pdc) noexcept
    {
        m_pdc = pdc;
        m_save = pdc->SaveDC();
    }

    CSaveDC(const CSaveDC&) = delete;
    CSaveDC& operator=(const CSaveDC&) = delete;
    CSaveDC(CSaveDC&&) = delete;
    CSaveDC& operator=(CSaveDC&&) = delete;

    ~CSaveDC() noexcept
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
