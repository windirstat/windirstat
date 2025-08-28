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

#include "stdafx.h"
#include "PacMan.h"

namespace
{
    constexpr ULONGLONG HIDE_THRESHOLD = 750; // ms
    constexpr float MOUTHSPEED = 0.003f;      // aperture alteration / ms
    constexpr float PACMANSPEED = 0.09f;      // pixels / ms
}

CPacman::CPacman() :
    m_Font(L"Arial", 6.0f, Gdiplus::FontStyleBold),
    m_Bgcolor(GetSysColor(COLOR_WINDOW))
{
    Reset();
}

void CPacman::Reset()
{
    m_LastUpdate   = 0;
    m_LastDraw     = 0;
    m_ToTheRight   = true;
    m_Position     = 0;
    m_MouthOpening = true;
    m_Aperture     = 0;
    m_Done         = false;
}

bool CPacman::m_Suspended = false;

void CPacman::SetGlobalSuspendState(const bool suspend)
{
    m_Suspended = suspend;
}

void CPacman::SetBackgroundColor(const COLORREF color)
{
    m_Bgcolor = color;
}

void CPacman::Start()
{
    m_Moving = true;
}

void CPacman::Stop()
{
    m_Done = true;
}

void CPacman::UpdatePosition()
{
    m_LastUpdate = GetTickCount64();
    if (m_LastDraw == 0) m_LastDraw = m_LastUpdate;
    m_Moving = true;
    m_Done = false;
}

void CPacman::Draw(const CDC* pdc, const CRect& rect)
{
    const ULONGLONG now = GetTickCount64();
    if (m_Suspended)
    {
        // Rebase time based if suspended
        m_LastUpdate = now;
        m_LastDraw = now;
    }

    // See if we should still consider ourselves movies
    if (now - m_LastUpdate > HIDE_THRESHOLD) m_Moving = false;

    // Update position
    if (m_Moving)
    {
        const float delta = static_cast<float>(now - m_LastDraw);
        UpdatePosition(m_Position, m_ToTheRight, PACMANSPEED * delta / static_cast<float>(rect.Width()));
        UpdatePosition(m_Aperture, m_MouthOpening, MOUTHSPEED * delta);
    }

    // Record time for next draw comparison
    m_LastDraw = now;

    // Calculate rectangle to display graphic
    CRect rc(rect);
    rc.DeflateRect(5, 1);
    rc.bottom -= rc.Height() % 2;
    rc.left += static_cast<int>(m_Position * (rc.Width() - rc.Height() / 2.0f));
    rc.right = rc.left + rc.Height();
    const Gdiplus::Rect grect(rc.left, rc.top, rc.Width(), rc.Height());

    // Determine the share of the figure
    const float slice = m_Aperture * 90.0f;
    const Gdiplus::REAL sweepAngle = 360.0f - slice;
    Gdiplus::REAL startAngle = m_Aperture * slice / 2.0f;
    if (!m_ToTheRight) startAngle += 180.0f;

    // Draw the background (use non gdi+ for performance)
    const CBrush bgBrush(m_Bgcolor);
    FillRect(*pdc, &rect, bgBrush);
    if (m_Done) return;

    // Create pens and brushes
    const Gdiplus::Pen blackPen(Gdiplus::Color(0xFF, 0x00, 0x00, 0x00), 1);
    const Gdiplus::SolidBrush yellowPen(Gdiplus::Color(0xFF, 0xFC, 0xC9, 0x2F));

    // Draw filled shape if we started and recently updated
    Gdiplus::Graphics graphics(pdc->GetSafeHdc());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.FillPie(&yellowPen, grect, startAngle, sweepAngle);
    graphics.DrawPie(&blackPen, grect, startAngle, sweepAngle);
    if (m_Moving) return;

    // Draw sleepy graphic
    const Gdiplus::SolidBrush blackBrush(Gdiplus::Color(0xFF, 0, 0, 0));
    graphics.DrawString(L"z",1, &m_Font, {rc.left + 5.0f, rc.top - 3.0f}, &blackBrush);
    graphics.DrawString(L"z", 1, &m_Font, { rc.left + 10.0f, rc.top - 4.5f }, &blackBrush);
    graphics.DrawString(L"z", 1, &m_Font, { rc.left + 15.0f, rc.top - 6.0f }, &blackBrush);
}

void CPacman::UpdatePosition(float& position, bool& up, const float diff)
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
