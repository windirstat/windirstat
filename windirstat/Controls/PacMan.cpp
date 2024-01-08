// pacman.cpp - Implementation of CPacman
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

#include "stdafx.h"
#include "SelectObject.h"
#include "pacman.h"

namespace
{
    constexpr ULONGLONG UPDATEINTERVAL = 40; // ms
    constexpr float MOUTHSPEED = 0.0030f;    // aperture alteration / ms
}

CPacman::CPacman()
    : m_lastUpdate(0)
      , m_bgcolor(::GetSysColor(COLOR_WINDOW))
      , m_speed(0.0005f)
      , m_readJobs(0)
      , m_position(0)
      , m_aperture(0)
      , m_moving(false)
      , m_toTheRight(true)
      , m_mouthOpening(false)
{
    Reset();
}

void CPacman::Reset()
{
    m_toTheRight   = true;
    m_position     = 0;
    m_mouthOpening = true;
    m_aperture     = 0;
}

void CPacman::SetBackgroundColor(COLORREF color)
{
    m_bgcolor = color;
}

void CPacman::SetSpeed(float speed)
{
    m_speed = speed;
}

void CPacman::Start(bool start)
{
    m_moving     = start;
    m_lastUpdate = GetTickCount64();
}

bool CPacman::Drive(ULONGLONG readJobs)
{
    m_readJobs = static_cast<float>(readJobs);

    if (!m_moving)
    {
        return false;
    }

    const ULONGLONG now   = GetTickCount64();
    const ULONGLONG delta = now - m_lastUpdate;

    if (delta < UPDATEINTERVAL)
    {
        return false;
    }

    m_lastUpdate = now;

    UpdatePosition(m_position, m_toTheRight, m_speed * delta);
    UpdatePosition(m_aperture, m_mouthOpening, MOUTHSPEED * delta);

    return true;
}

void CPacman::Draw(CDC* pdc, const CRect& rect) const
{
    pdc->FillSolidRect(rect, m_bgcolor);

    CRect rc = rect;
    rc.DeflateRect(5, 1);

    if (rc.Height() % 2 == 0)
    {
        rc.bottom--;
    }

    const int diameter = rc.Height();

    const int left = rc.left + static_cast<int>(m_position * (rc.Width() - diameter));
    rc.left        = left;
    rc.right       = left + diameter;

    CPen pen(PS_SOLID, 1, RGB(0, 0, 0));
    CSelectObject sopen(pdc, &pen);

    CBrush brush(CalculateColor());
    CSelectObject sobrush(pdc, &brush);

    CPoint ptStart;
    CPoint ptEnd;
    const int hmiddle = rc.top + diameter / 2;

    const int mouthcy      = static_cast<int>(m_aperture * m_aperture * diameter);
    const int upperMouthcy = mouthcy;
    const int lowerMouthcy = mouthcy;

    if (m_toTheRight)
    {
        ptStart.x = ptEnd.x = rc.right;
        ptStart.y = hmiddle - upperMouthcy;
        ptEnd.y   = hmiddle + lowerMouthcy;
    }
    else
    {
        ptStart.x = ptEnd.x = rc.left;
        ptStart.y = hmiddle + lowerMouthcy;
        ptEnd.y   = hmiddle - upperMouthcy;
    }

    pdc->Pie(rc, ptStart, ptEnd);
}

void CPacman::UpdatePosition(float& position, bool& up, float diff)
{
    ASSERT(diff >= 0.0);
    ASSERT(position >= 0.0);
    ASSERT(position <= 1.0);

    while (diff > 0.0)
    {
        if (up)
        {
            if (position + diff > 1.0)
            {
                diff     = position + diff - 1.0f;
                position = 1.0;
                up       = false;
            }
            else
            {
                position += diff;
                diff = 0;
            }
        }
        else
        {
            if (position - diff < 0.0)
            {
                diff     = -(position - diff);
                position = 0.0;
                up       = true;
            }
            else
            {
                position -= diff;
                diff = 0;
            }
        }
    }
}

COLORREF CPacman::CalculateColor() const
{
    static constexpr double pi2 = 3.1415926535897932384626433832795 / 2;

    ASSERT(m_readJobs >= 0);
    const double a = atan(m_readJobs / 18) / pi2;
    ASSERT(a >= 0.0);
    ASSERT(a <= 1.0);

    /*
        // a == 1 --> yellow
        // a == 0 --> green
    
        int red = (int)(a * 255);
    
        return RGB(red, 255, 0);
    */
    // a == 1 --> yellow
    // a == 0 --> bgcolor

    const int red   = static_cast<int>(a * 255 + (1 - a) * RGB_GET_RVALUE(m_bgcolor));
    const int green = static_cast<int>(a * 255 + (1 - a) * RGB_GET_GVALUE(m_bgcolor));
    const int blue  = static_cast<int>((1 - a) * RGB_GET_BVALUE(m_bgcolor));

    return RGB(red, green, blue);
}
