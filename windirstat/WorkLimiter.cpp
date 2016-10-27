#include "stdafx.h"
#include "WorkLimiter.h"

CWorkLimiter::CWorkLimiter()
	: m_done()
	, m_tickLimit()
	, m_prevTicks()
{
}

CWorkLimiter::~CWorkLimiter()
{
}

void CWorkLimiter::Start(DWORD ticks)
{
	DWORD start = Now();
	m_done = false;
	m_tickLimit = start + ticks;
	m_prevTicks = start;
}

bool CWorkLimiter::IsDone() const
{
	if (m_done) return true;

	// check remaining ticks
	DWORD now = Now();
	// signed subtraction to deal with overflow
	long remaining = m_tickLimit - now;
	if (remaining <= 0)
	{
		m_done = true;
		return true;
	}

	// check if there are any pending window messages
	long elapsed = now - m_prevTicks;
	if (elapsed > 10)
	{
		m_prevTicks = now;
		MSG msg;
		if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE | PM_QS_INPUT))
		{
			m_done = true;
			return true;
		}
	}

	return false;
}

void CWorkLimiter::DoFileWork()
{
}

DWORD CWorkLimiter::Now() const
{
	return ::GetTickCount();
}