// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"

//
// CPacman. Pacman animation.
//
class CPacman final
{
public:
    static void SetGlobalSuspendState(const bool suspend = true) { m_suspended = suspend; }
    CPacman() = default;
    void Reset();
    void Start() { m_moving = true; }
    void Stop() { m_done = true; }
    void UpdatePosition();
    void Draw(CDC* pdc, const CRect& rect, COLORREF backColor = ~COLORREF(),
        bool dots = false, std::optional<bool> suspended = {});

private:
    static void UpdatePosition(float& position, bool& up, float diff);
    static inline bool m_suspended = false;

    ULONGLONG m_lastUpdate = 0;  // TickCount
    ULONGLONG m_lastDraw = 0;    // Last time drawn
    float m_position = 0.0f;     // 0...1
    float m_aperture = 0.0f;     // 0...1
    bool m_done = false;         // Whether pacman should be done
    bool m_moving = false;       // Whether pacman is moving
    bool m_toTheRight = true;    // Moving right
    bool m_mouthOpening = true;  // Mouth is opening
};
