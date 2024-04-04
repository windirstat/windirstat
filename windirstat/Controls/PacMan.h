// pacman.h - Declaration of CPacman
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

#include <shared_mutex>

//
// CPacman. Pacman animation.
//
class CPacman final
{
public:
    CPacman();
    static void SetGlobalSuspendState(bool suspend = true);
    void SetBackgroundColor(COLORREF color);
    void SetSpeed(float speed);
    void Reset();
    void Start();
    void Stop();
    void UpdatePosition();
    void Draw(const CDC* pdc, const CRect& rect);

private:
    static void UpdatePosition(float& position, bool& up, float diff);
    static bool m_suspended;

    ULONGLONG m_lastUpdate = 0;  // TickCount
    ULONGLONG m_lastDraw = 0;    // Last time drawn
    COLORREF m_bgcolor;          // Background color
    float m_speed = 0.0006f;     // Speed in full width / ms
    float m_position = 0.0f;     // 0...1
    float m_aperture = 0.0f;     // 0...1
    bool m_done = false;         // Whether pacman should be done
    bool m_moving = false;       // Whether pacman is moving
    bool m_toTheRight = true;    // Moving right
    bool m_mouthOpening = false; // Mouth is opening
};
