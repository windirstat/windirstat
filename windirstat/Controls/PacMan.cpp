// PacMan.cpp - Implementation of CPacman
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
#include "PacMan.h"

namespace
{
    constexpr ULONGLONG HIDE_THRESHOLD = 750;  // ms
    constexpr float MOUTHSPEED = 0.0030f;      // aperture alteration / ms
}

CPacman::CPacman()
    : m_lastUpdate(0)
      , m_lastDraw(0)
      , m_bgcolor(::GetSysColor(COLOR_WINDOW))
      , m_speed(0.0006f)
      , m_position(0)
      , m_aperture(0)
      , m_done(false)
      , m_moving(false)
      , m_toTheRight(true)
      , m_mouthOpening(false)
{
    Reset();
}

void CPacman::Reset()
{
    m_lastUpdate   = 0;
    m_lastDraw     = 0;
    m_toTheRight   = true;
    m_position     = 0;
    m_mouthOpening = true;
    m_aperture     = 0;
    m_done         = false;
}

bool CPacman::m_suspended = false;

void CPacman::SetGlobalSuspendState(bool suspend)
{
    m_suspended = suspend;
}

void CPacman::SetBackgroundColor(COLORREF color)
{
    m_bgcolor = color;
}

void CPacman::SetSpeed(float speed)
{
    m_speed = speed;
}

void CPacman::Start()
{
    m_moving = true;
}

void CPacman::Stop()
{
    m_done = true;
}

void CPacman::UpdatePosition()
{
    m_lastUpdate = GetTickCount64();
    if (m_lastDraw == 0) m_lastDraw = m_lastUpdate;
    m_moving = true;
    m_done = false;
}

void CPacman::Draw(const CDC* pdc, const CRect& rect)
{
    const ULONGLONG now = GetTickCount64();
    if (m_suspended)
    {
        // Rebase time based if suspended
        m_lastUpdate = now;
        m_lastDraw = now;
    }

    // See if we should still consider ourselves movies
    if (now - m_lastUpdate > HIDE_THRESHOLD) m_moving = false;

    // Update position
    const float delta = static_cast<float>(now - m_lastDraw);
    m_lastDraw = now;
    if (m_moving)
    {
        UpdatePosition(m_position, m_toTheRight, m_speed * delta);
        UpdatePosition(m_aperture, m_mouthOpening, MOUTHSPEED * delta);
    }

    // Calculate rectangle to display graphic
    CRect rc(rect);
    rc.DeflateRect(5, 1);
    rc.bottom -= rc.Height() % 2;
    rc.left += static_cast<int>(m_position * (rc.Width() - rc.Height()));
    rc.right = rc.left + rc.Height();
    const Gdiplus::Rect grect(rc.left, rc.top, rc.Width(), rc.Height());

    // Create pens and brushes
    Gdiplus::Color bgColor;
    bgColor.SetFromCOLORREF(m_bgcolor);
    const Gdiplus::SolidBrush bgPen(bgColor);
    static const Gdiplus::Pen blackPen(Gdiplus::Color(0xFF, 0x00, 0x00, 0x00), 1);
    static const Gdiplus::SolidBrush yellowPen(Gdiplus::Color(0xFF, 0xFC, 0xC9, 0x2F));

    // Determine the share of the figure
    const float slice = m_aperture * 90.0f;
    const Gdiplus::REAL sweepAngle = 360.0f - slice;
    Gdiplus::REAL startAngle = m_aperture * slice / 2.0f;
    if (!m_toTheRight) startAngle += 180.0f;

    // Draw the background
    Gdiplus::Graphics graphics(pdc->GetSafeHdc());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.FillRectangle(&bgPen, rect.left, rect.top, rect.Width(), rect.Height());
    if (m_done) return;

    // Draw filled shape if we started and recently updated
    graphics.FillPie(&yellowPen, grect, startAngle, sweepAngle);
    graphics.DrawPie(&blackPen, grect, startAngle, sweepAngle);
    if (m_moving) return;

    // Draw sleepy graphic
    static const Gdiplus::Font font(L"Arial", 6.0f, Gdiplus::FontStyleBold);
    static const Gdiplus::SolidBrush blackBrush(Gdiplus::Color(0xFF, 0, 0, 0));
    graphics.DrawString(L"z",1, &font, {rc.left + 5.0f, rc.top- 3.0f}, &blackBrush);
    graphics.DrawString(L"z", 1, &font, { rc.left + 10.0f, rc.top - 4.5f }, &blackBrush);
    graphics.DrawString(L"z", 1, &font, { rc.left + 15.0f, rc.top - 6.0f }, &blackBrush);
}

void CPacman::UpdatePosition(float& position, bool& up, float diff)
{
    ASSERT(diff >= 0.0f);
    ASSERT(position >= 0.0f);
    ASSERT(position <= 1.0f);

    if (!up) position = 2.0f - position;
    position += diff;
    position = std::fmodf(position, 2.0f);
    up = (position <= 1.0f);
    if (!up) position = 2.0f - position;
}
