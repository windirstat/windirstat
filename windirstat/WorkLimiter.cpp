#include "stdafx.h"
#include "WorkLimiter.h"

CWorkLimiter::CWorkLimiter()
    : m_done()
      , m_tickLimit()
      , m_prevTicks()
{
}

void CWorkLimiter::Start(ULONGLONG ticks)
{
    const ULONGLONG start = CWorkLimiter::Now();
    m_done                = false;
    m_tickLimit           = start + ticks;
    m_prevTicks           = start;
}

bool CWorkLimiter::IsDone() const
{
    if (m_done) return true;

    // check remaining ticks
    const ULONGLONG now = CWorkLimiter::Now();
    // signed subtraction to deal with overflow
    const ULONGLONG remaining = m_tickLimit - now;
    if (remaining <= 0)
    {
        m_done = true;
        return true;
    }

    // check if there are any pending window messages
    const ULONGLONG elapsed = now - m_prevTicks;
    if (elapsed > 10)
    {
        m_prevTicks = now;
        MSG msg;
        if (PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE | PM_QS_INPUT))
        {
            m_done = true;
            return true;
        }
    }

    return false;
}

inline ULONGLONG CWorkLimiter::Now()
{
    return GetTickCount64();
}
