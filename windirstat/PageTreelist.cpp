// PageTreelist.cpp - Implementation of CPageTreelist
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
#include "windirstat.h"
#include "PageTreelist.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CPageTreelist, CPropertyPage)

CPageTreelist::CPageTreelist()
	: CPropertyPage(CPageTreelist::IDD)
{
}

CPageTreelist::~CPageTreelist()
{
}

void CPageTreelist::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_PACMANANIMATION, m_pacmanAnimation);
	DDX_Check(pDX, IDC_SHOWTIMESPENT, m_showTimeSpent);
	for(int i = 0; i < TREELISTCOLORCOUNT; i++)
	{
		DDX_Control(pDX, IDC_COLORBUTTON0 + i, m_colorButton[i]);
		if(pDX->m_bSaveAndValidate)
		{
			m_treelistColor[i]= m_colorButton[i].GetColor();
		}
		else
		{
			m_colorButton[i].SetColor(m_treelistColor[i]);
		}
	}
	DDX_Control(pDX, IDC_SLIDER, m_slider);
}


BEGIN_MESSAGE_MAP(CPageTreelist, CPropertyPage)
	ON_NOTIFY_RANGE(COLBN_CHANGED, IDC_COLORBUTTON0, IDC_COLORBUTTON7, OnColorChanged)
	ON_WM_VSCROLL()
	ON_BN_CLICKED(IDC_PACMANANIMATION, OnBnClickedPacmananimation)
	ON_BN_CLICKED(IDC_SHOWTIMESPENT, OnBnClickedShowTimeSpent)
END_MESSAGE_MAP()


BOOL CPageTreelist::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	m_pacmanAnimation = GetOptions()->IsPacmanAnimation();
	m_showTimeSpent = GetOptions()->IsShowTimeSpent();
	m_treelistColorCount = GetOptions()->GetTreelistColorCount();
	GetOptions()->GetTreelistColors(m_treelistColor);

	m_slider.SetRange(1, TREELISTCOLORCOUNT);
	m_slider.SetPos(m_treelistColorCount);

	EnableButtons();

	UpdateData(false);
	return TRUE;
}

void CPageTreelist::OnOK()
{
	UpdateData();
	GetOptions()->SetPacmanAnimation(m_pacmanAnimation);
	GetOptions()->SetShowTimeSpent(m_showTimeSpent);
	GetOptions()->SetTreelistColorCount(m_treelistColorCount);
	GetOptions()->SetTreelistColors(m_treelistColor);
	CPropertyPage::OnOK();
}

void CPageTreelist::OnBnClickedPacmananimation()
{
	SetModified();
}

void CPageTreelist::OnBnClickedShowTimeSpent()
{
	SetModified();
}

void CPageTreelist::OnColorChanged(UINT, NMHDR *, LRESULT *)
{
	SetModified();
}

void CPageTreelist::EnableButtons()
{
	int i = 0;
	for(i = 0; i < m_treelistColorCount; i++)
	{
		m_colorButton[i].EnableWindow(true);
	}
	for(; i < TREELISTCOLORCOUNT; i++)
	{
		m_colorButton[i].EnableWindow(false);
	}
}

void CPageTreelist::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if((CSliderCtrl *)pScrollBar == &m_slider)
	{
		int pos = m_slider.GetPos();
		ASSERT(pos > 0);
		ASSERT(pos <= TREELISTCOLORCOUNT);

		m_treelistColorCount = pos;
		EnableButtons();
		SetModified();
	}
	CPropertyPage::OnVScroll(nSBCode, nPos, pScrollBar);
}

// $Log$
// Revision 1.9  2006/10/10 01:41:50  assarbad
// - Added credits for Gerben Wieringa (Dutch translation)
// - Replaced Header tag by Id for the CVS tags in the source files ...
// - Started re-ordering of the files inside the project(s)/solution(s)
//
// Revision 1.8  2006/07/04 23:37:39  assarbad
// - Added my email address in the header, adjusted "Author" -> "Author(s)"
// - Added CVS Log keyword to those files not having it
// - Added the files which I forgot during last commit
//
// Revision 1.7  2006/07/04 22:49:20  assarbad
// - Replaced CVS keyword "Date" by "Header" in the file headers
//
// Revision 1.6  2006/07/04 20:45:22  assarbad
// - See changelog for the changes of todays previous check-ins as well as this one!
//
// Revision 1.5  2004/11/13 08:17:07  bseifert
// Remove blanks in Unicode Configuration names.
//
// Revision 1.4  2004/11/12 22:14:16  bseifert
// Eliminated CLR_NONE. Minor corrections.
//
// Revision 1.3  2004/11/05 16:53:06  assarbad
// Added Date and History tag where appropriate.
//
