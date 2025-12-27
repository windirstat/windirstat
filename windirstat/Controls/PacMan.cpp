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

#include "pch.h"
#include "PacMan.h"

namespace
{
    constexpr ULONGLONG HIDE_THRESHOLD = 750; // ms
    constexpr float MOUTHSPEED = 0.003f;      // aperture alteration / ms
    constexpr float PACMANSPEED = 0.09f;      // pixels / ms
}

CPacman::CPacman(const COLORREF backColor) :
    m_font(L"Arial", 6.0f, Gdiplus::FontStyleBold),
    m_backColor(backColor)
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

void CPacman::SetGlobalSuspendState(const bool suspend)
{
    m_suspended = suspend;
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

void CPacman::Draw(CDC* pdc, const CRect& rect)
{
    const ULONGLONG now = GetTickCount64();
    if (m_suspended)
    {
        // Rebase time if suspended
        m_lastUpdate = now;
        m_lastDraw = now;
    }

    // See if we should still consider ourselves moving
    if (now - m_lastUpdate > HIDE_THRESHOLD) m_moving = false;

    // Update position
    if (m_moving)
    {
        const float delta = static_cast<float>(now - m_lastDraw);
        UpdatePosition(m_position, m_toTheRight, PACMANSPEED * delta / static_cast<float>(rect.Width()));
        UpdatePosition(m_aperture, m_mouthOpening, MOUTHSPEED * delta);
    }

    // Record time for next draw comparison
    m_lastDraw = now;

    // Calculate rectangle to display graphic
    CRect rc(rect);
    rc.DeflateRect(5, 1);
    rc.bottom -= rc.Height() % 2;
    rc.left += static_cast<int>(m_position * (rc.Width() - rc.Height() / 2.0f));
    rc.right = rc.left + rc.Height();
    const Gdiplus::Rect grect(rc.left, rc.top, rc.Width(), rc.Height());

    // Determine the share of the figure
    const float slice = m_aperture * 90.0f;
    const Gdiplus::REAL sweepAngle = 360.0f - slice;
    Gdiplus::REAL startAngle = m_aperture * slice / 2.0f;
    if (!m_toTheRight) startAngle += 180.0f;

    // Fill background if requested
    if (m_backColor != -1)
    {
        pdc->FillSolidRect(rect, m_backColor);
    }
    if (m_done) return;

    // Create pens and brushes
    const Gdiplus::Pen blackPen(Gdiplus::Color(0xFF, 0x00, 0x00, 0x00), 1);
    const Gdiplus::SolidBrush yellowPen(Gdiplus::Color(0xFF, 0xFC, 0xC9, 0x2F));

    // Draw filled shape if we started and recently updated
    Gdiplus::Graphics graphics(pdc->GetSafeHdc());
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.FillPie(&yellowPen, grect, startAngle, sweepAngle);
    graphics.DrawPie(&blackPen, grect, startAngle, sweepAngle);
    if (m_moving) return;

    // Draw sleepy graphic
    const COLORREF zColor = DarkMode::WdsSysColor(COLOR_WINDOWTEXT);
    const Gdiplus::SolidBrush zBrush(Gdiplus::Color(0xFF, GetRValue(zColor), GetGValue(zColor), GetBValue(zColor)));
    graphics.DrawString(L"z",1, &m_font, {rc.left + 5.0f, rc.top - 3.0f}, &zBrush);
    graphics.DrawString(L"z", 1, &m_font, { rc.left + 10.0f, rc.top - 4.5f }, &zBrush);
    graphics.DrawString(L"z", 1, &m_font, { rc.left + 15.0f, rc.top - 6.0f }, &zBrush);
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
