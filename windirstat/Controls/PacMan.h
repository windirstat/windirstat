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

//
// CPacman. Pacman animation.
//
class CPacman final
{
public:
    CPacman();
    void SetBackgroundColor(COLORREF color);
    void SetSpeed(float speed);
    void Reset();
    void Start(bool start);
    bool Drive(ULONGLONG readJobs); // return: true -> should be redrawn.
    void Draw(CDC* pdc, const CRect& rect) const;

private:
    void UpdatePosition(float& position, bool& up, float diff);
    COLORREF CalculateColor() const;

    ULONGLONG m_lastUpdate; // TickCount
    COLORREF m_bgcolor;     // Background color
    float m_speed;         // Speed in full width / ms
    float m_readJobs;      // # of read jobs determines our color
    float m_position;      // 0...1
    float m_aperture;      // 0...1
    bool m_moving;          // Whether pacman is moving
    bool m_toTheRight;      // moving right
    bool m_mouthOpening;    // Mouth is opening
};
