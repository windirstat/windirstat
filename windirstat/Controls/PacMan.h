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

#include "stdafx.h"

//
// CPacman. Pacman animation.
//
class CPacman final
{
public:
    CPacman(COLORREF backColor = -1);
    static void SetGlobalSuspendState(bool suspend = true);
    void Reset();
    void Start();
    void Stop();
    void UpdatePosition();
    void Draw(CDC* pdc, const CRect& rect);

private:
    static void UpdatePosition(float& position, bool& up, float diff);
    static bool m_Suspended;

    Gdiplus::Font m_Font;
    COLORREF m_BackColor;
    ULONGLONG m_LastUpdate = 0;  // TickCount
    ULONGLONG m_LastDraw = 0;    // Last time drawn
    float m_Position = 0.0f;     // 0...1
    float m_Aperture = 0.0f;     // 0...1
    bool m_Done = false;         // Whether pacman should be done
    bool m_Moving = false;       // Whether pacman is moving
    bool m_ToTheRight = true;    // Moving right
    bool m_MouthOpening = true;  // Mouth is opening
};
