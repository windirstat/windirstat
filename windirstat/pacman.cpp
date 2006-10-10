// pacman.cpp - Implementation of CPacman
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2006 Oliver Schneider (assarbad.net)
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
// Author(s): - bseifert -> bseifert@users.sourceforge.net, bseifert@daccord.net
//            - assarbad -> http://assarbad.net/en/contact
//
// $Id$

#include "stdafx.h"
#include "pacman.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	DWORD UPDATEINTERVAL = 40;	// ms
	double MOUTHSPEED = 0.0030;	// aperture alteration / ms
}

CPacman::CPacman()
	: m_readJobs(0)
	, m_speed(0.0005)
	, m_moving(false)
	, m_lastUpdate(0)
	, m_isWindows9x(PlatformIsWindows9x())
	, m_bgcolor(GetSysColor(COLOR_WINDOW))
{
	Reset();
}

void CPacman::Reset()
{
	m_toTheRight = true;
	m_position = 0;
	m_mouthOpening = true;
	m_aperture = 0;
}

void CPacman::SetBackgroundColor(COLORREF color)
{
	m_bgcolor = color;
}

void CPacman::SetSpeed(double speed)
{
	m_speed = speed;
}

void CPacman::Start(bool start)
{
	m_moving = start;
	m_lastUpdate = GetTickCount();
}

bool CPacman::Drive(ULONGLONG readJobs)
{
	m_readJobs = (double)readJobs;

	if(!m_moving)
	{
		return false;
	}

	DWORD now = GetTickCount();
	DWORD delta = now - m_lastUpdate;

	if(delta < UPDATEINTERVAL)
	{
		return false;
	}

	m_lastUpdate = now;

	UpdatePosition(m_position, m_toTheRight, m_speed * delta);
	UpdatePosition(m_aperture, m_mouthOpening, MOUTHSPEED * delta);

	return true;
}

void CPacman::Draw(CDC *pdc, const CRect& rect)
{
	pdc->FillSolidRect(rect, m_bgcolor);
	
	CRect rc = rect;
	rc.DeflateRect(5, 1);

	if(rc.Height() % 2 == 0)
	{
		rc.bottom--;
	}

	int diameter = rc.Height();

	int left = rc.left + (int)(m_position * (rc.Width() - diameter));
	rc.left = left;
	rc.right = left + diameter;

	CPen pen(PS_SOLID, 1, RGB(0,0,0));
	CSelectObject sopen(pdc, &pen);

	CBrush brush(CalculateColor());
	CSelectObject sobrush(pdc, &brush);

	CPoint ptStart;
	CPoint ptEnd;
	int hmiddle = rc.top + diameter / 2;

	int mouthcy = (int)(m_aperture * m_aperture * diameter);
	int upperMouthcy = mouthcy;
	int lowerMouthcy = mouthcy;

	// It's the sad truth, that CDC::Pie() behaves different on
	// Windows 9x than on NT.
	if(!m_isWindows9x)
	{
		lowerMouthcy++;
	}

	if(m_toTheRight)
	{
		ptStart.x = ptEnd.x = rc.right;
		ptStart.y	= hmiddle - upperMouthcy;
		ptEnd.y		= hmiddle + lowerMouthcy;
	}
	else
	{
		ptStart.x = ptEnd.x = rc.left;
		ptStart.y	= hmiddle + lowerMouthcy;
		ptEnd.y		= hmiddle - upperMouthcy;
	}

	pdc->Pie(rc, ptStart, ptEnd);
}


void CPacman::UpdatePosition(double& position, bool& up, double diff)
{
	ASSERT(diff >= 0.0);
	ASSERT(position >= 0.0);
	ASSERT(position <= 1.0);

	while(diff > 0.0)
	{
		if(up)
		{
			if(position + diff > 1.0)
			{
				diff = position + diff - 1.0;
				position = 1.0;
				up = false;
			}
			else
			{
				position += diff;
				diff = 0;
			}
		}
		else
		{
			if(position - diff < 0.0)
			{
				diff = - (position - diff);
				position = 0.0;
				up = true;
			}
			else
			{
				position -= diff;
				diff = 0;
			}
		}
	}
}

COLORREF CPacman::CalculateColor()
{
	static const double pi2 = (3.1415926535897932384626433832795 / 2);

	ASSERT(m_readJobs >= 0);
	double a = atan(m_readJobs / 18) / pi2;
	ASSERT(a >= 0.0);
	ASSERT(a <= 1.0);

/*
	// a == 1 --> yellow
	// a == 0 --> green

	int red = (int)(a * 255);

	return RGB(red, 255, 0);
*/
	// a == 1 --> yellow
	// a == 0 --> bgcolor

	int red		= (int)(a * 255 + (1 - a) * GetRValue(m_bgcolor));
	int green	= (int)(a * 255 + (1 - a) * GetGValue(m_bgcolor));
	int blue	= (int)(          (1 - a) * GetBValue(m_bgcolor));

	return RGB(red, green, blue);
}

// $Log$
// Revision 1.10  2006/10/10 01:41:50  assarbad
// - Added credits for Gerben Wieringa (Dutch translation)
// - Replaced Header tag by Id for the CVS tags in the source files ...
// - Started re-ordering of the files inside the project(s)/solution(s)
//
// Revision 1.9  2006/07/04 23:37:40  assarbad
// - Added my email address in the header, adjusted "Author" -> "Author(s)"
// - Added CVS Log keyword to those files not having it
// - Added the files which I forgot during last commit
//
// Revision 1.8  2006/07/04 22:49:21  assarbad
// - Replaced CVS keyword "Date" by "Header" in the file headers
//
// Revision 1.7  2006/07/04 20:45:23  assarbad
// - See changelog for the changes of todays previous check-ins as well as this one!
//
// Revision 1.6  2004/11/05 16:53:07  assarbad
// Added Date and History tag where appropriate.
//
