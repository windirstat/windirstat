// layout.cpp	- Implementation of CLayout
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003 Bernhard Seifert
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
// Author: bseifert@users.sourceforge.net, bseifert@daccord.net

#include "stdafx.h"
#include "windirstat.h"
#include "options.h"
#include "layout.h"


CLayout::CLayout(CWnd *dialog, LPCTSTR name)
{
	ASSERT(dialog != NULL);
	m_dialog= dialog;
	m_name= name;

	// This is necessary because OnGetMinMaxInfo() will be called
	// before OnInitDialog!
	m_originalDialogSize.cx= 0;
	m_originalDialogSize.cy= 0;
}

void CLayout::AddControl(CWnd *control, double movex, double movey, double stretchx, double stretchy)
{
	SControlInfo info;
	
	info.control= control;
	info.movex= movex;
	info.movey= movey;
	info.stretchx= stretchx;
	info.stretchy= stretchy;
	
	m_control.Add(info);
}

void CLayout::AddControl(UINT id, double movex, double movey, double stretchx, double stretchy)
{
	AddControl(m_dialog->GetDlgItem(id), movex, movey, stretchx, stretchy);
}

void CLayout::OnInitDialog(bool centerWindow)
{
	m_dialog->SetIcon(GetApp()->LoadIcon(IDR_MAINFRAME), false);

	CRect rc;
	m_dialog->GetWindowRect(rc);
	m_originalDialogSize= rc.Size();

	for (int i=0; i < m_control.GetSize(); i++)
	{
		CRect rc;
		m_control[i].control->GetWindowRect(rc);
		m_dialog->ScreenToClient(rc);
		m_control[i].originalRectangle= rc;
	}

	CPersistence::GetDialogRectangle(m_name, rc);
	m_dialog->MoveWindow(rc);
	if (centerWindow)
		m_dialog->CenterWindow();
}

void CLayout::OnDestroy()
{
	CRect rc;
	m_dialog->GetWindowRect(rc);
	CPersistence::SetDialogRectangle(m_name, rc);
}

void CLayout::OnSize()
{
	CRect rc;
	m_dialog->GetWindowRect(rc);
	CSize newDialogSize= rc.Size();

	CSize diff= newDialogSize - m_originalDialogSize;

	for (int i=0; i < m_control.GetSize(); i++)
	{
		CRect rc= m_control[i].originalRectangle;

		CSize move(int(diff.cx * m_control[i].movex), int(diff.cy * m_control[i].movey));
		CRect stretch(0, 0, int(diff.cx * m_control[i].stretchx), int(diff.cy * m_control[i].stretchy));
		
		rc+= move;
		rc+= stretch;

		m_control[i].control->MoveWindow(rc);
	}
}

void CLayout::OnGetMinMaxInfo(MINMAXINFO *mmi)
{
	mmi->ptMinTrackSize.x= m_originalDialogSize.cx;
	mmi->ptMinTrackSize.y= m_originalDialogSize.cy;
}


