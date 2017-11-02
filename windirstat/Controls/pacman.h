// pacman.h - Declaration of CPacman
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2017 WinDirStat Team (windirstat.net)
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

#ifndef __WDS_PACMAN_H__
#define __WDS_PACMAN_H__
#pragma once

//
// CPacman. Pacman animation.
//
class CPacman
{
public:
    CPacman();
    void SetBackgroundColor(COLORREF color);
    void SetSpeed(double speed);
    void Reset();
    void Start(bool start);
    bool Drive(ULONGLONG readJobs); // return: true -> should be redrawn.
    void Draw(CDC *pdc, const CRect& rc);

private:
    void UpdatePosition(double& position, bool& up, double diff);
    COLORREF CalculateColor();

    COLORREF m_bgcolor;     // Background color
    double m_speed;         // Speed in full width / ms
    bool m_moving;          // Whether pacman is moving
    double m_readJobs;      // # of read jobs determines our color
    bool m_toTheRight;      // moving right
    double m_position;      // 0...1
    bool m_mouthOpening;    // Mouth is opening
    double m_aperture;      // 0...1
    DWORD m_lastUpdate;     // TickCount
};

#endif // __WDS_PACMAN_H__
