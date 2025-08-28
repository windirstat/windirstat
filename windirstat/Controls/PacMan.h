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

#pragma once

#include "stdafx.h"

//
// CPacman. Pacman animation.
//
class CPacman final
{
public:
    CPacman();
    static void SetGlobalSuspendState(bool suspend = true);
    void SetBackgroundColor(COLORREF color);
    void Reset();
    void Start();
    void Stop();
    void UpdatePosition();
    void Draw(const CDC* pdc, const CRect& rect);

private:
    static void UpdatePosition(float& position, bool& up, float diff);
    static bool m_Suspended;

    Gdiplus::Font m_Font;
    ULONGLONG m_LastUpdate = 0;  // TickCount
    ULONGLONG m_LastDraw = 0;    // Last time drawn
    COLORREF m_Bgcolor;          // Background color
    float m_Position = 0.0f;     // 0...1
    float m_Aperture = 0.0f;     // 0...1
    bool m_Done = false;         // Whether pacman should be done
    bool m_Moving = false;       // Whether pacman is moving
    bool m_ToTheRight = true;    // Moving right
    bool m_MouthOpening = false; // Mouth is opening
};
