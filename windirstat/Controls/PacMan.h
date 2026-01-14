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

#pragma once

#include "pch.h"

//
// CPacman. Pacman animation.
//
class CPacman final
{
public:
    CPacman(COLORREF backColor = ~COLORREF());
    static void SetGlobalSuspendState(bool suspend = true);
    void Reset();
    void Start();
    void Stop();
    void UpdatePosition();
    void Draw(CDC* pdc, const CRect& rect);

private:
    static void UpdatePosition(float& position, bool& up, float diff);
    static bool m_suspended;

    COLORREF m_backColor;
    ULONGLONG m_lastUpdate = 0;  // TickCount
    ULONGLONG m_lastDraw = 0;    // Last time drawn
    float m_position = 0.0f;     // 0...1
    float m_aperture = 0.0f;     // 0...1
    bool m_done = false;         // Whether pacman should be done
    bool m_moving = false;       // Whether pacman is moving
    bool m_toTheRight = true;    // Moving right
    bool m_mouthOpening = true;  // Mouth is opening
};
