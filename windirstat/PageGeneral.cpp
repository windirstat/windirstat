// PageGeneral.cpp	- Implementation of CPageGeneral
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2004 Bernhard Seifert
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
//
// Last modified: $Date$

#include "stdafx.h"
#include "windirstat.h"
#include "mainframe.h"		// COptionsPropertySheet
#include ".\pagegeneral.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CPageGeneral, CPropertyPage)

CPageGeneral::CPageGeneral()
	: CPropertyPage(CPageGeneral::IDD)
{
}

CPageGeneral::~CPageGeneral()
{
}

COptionsPropertySheet *CPageGeneral::GetSheet()
{
	COptionsPropertySheet *sheet = DYNAMIC_DOWNCAST(COptionsPropertySheet, GetParent());
	ASSERT(sheet != NULL);
	return sheet;
}

void CPageGeneral::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_HUMANFORMAT, m_humanFormat);
	DDX_Check(pDX, IDC_FOLLOWMOUNTPOINTS, m_followMountPoints);
	DDX_Check(pDX, IDC_FOLLOWJUNCTIONS, m_followJunctionPoints);
	DDX_Control(pDX, IDC_COMBO, m_combo);
	DDX_Control(pDX, IDC_FOLLOWMOUNTPOINTS, m_ctlFollowMountPoints);
	DDX_Control(pDX, IDC_FOLLOWJUNCTIONS, m_ctlFollowJunctionPoints);
	DDX_Check(pDX, IDC_SHOWGRID, m_listGrid);
	DDX_Check(pDX, IDC_SHOWSTRIPES, m_listStripes);
	DDX_Check(pDX, IDC_FULLROWSELECTION, m_listFullRowSelection);
}


BEGIN_MESSAGE_MAP(CPageGeneral, CPropertyPage)
	ON_BN_CLICKED(IDC_HUMANFORMAT, OnBnClickedHumanformat)
	ON_BN_CLICKED(IDC_FOLLOWMOUNTPOINTS, OnBnClickedFollowmountpoints)
	ON_BN_CLICKED(IDC_FOLLOWJUNCTIONS, OnBnClickedFollowjunctionpoints)
	ON_CBN_SELENDOK(IDC_COMBO, OnCbnSelendokCombo)
	ON_BN_CLICKED(IDC_SHOWGRID, OnBnClickedListGrid)
	ON_BN_CLICKED(IDC_SHOWSTRIPES, OnBnClickedListStripes)
	ON_BN_CLICKED(IDC_FULLROWSELECTION, OnBnClickedListFullRowSelection)
END_MESSAGE_MAP()


BOOL CPageGeneral::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	m_humanFormat= GetOptions()->IsHumanFormat();
	m_listGrid= GetOptions()->IsListGrid();
	m_listStripes= GetOptions()->IsListStripes();
	m_listFullRowSelection= GetOptions()->IsListFullRowSelection();

	m_followMountPoints= GetOptions()->IsFollowMountPoints();
	m_followJunctionPoints= GetOptions()->IsFollowJunctionPoints();

	CVolumeApi va;
	if (!va.IsSupported())
	{
		m_followMountPoints= false;	// Otherwise we would see pacman only.
		m_ctlFollowMountPoints.ShowWindow(SW_HIDE); // Ignorance is bliss.
		// The same for junction points
		m_followJunctionPoints = false;	// Otherwise we would see pacman only.
		m_ctlFollowJunctionPoints.ShowWindow(SW_HIDE); // Ignorance is bliss.
	}

	int k= m_combo.AddString(GetLocaleLanguage(GetApp()->GetBuiltInLanguage()));
	m_combo.SetItemData(k, GetApp()->GetBuiltInLanguage());

	CArray<LANGID, LANGID> langid;
	GetApp()->GetAvailableResourceDllLangids(langid);

	for (int i=0; i < langid.GetSize(); i++)
	{
		k= m_combo.AddString(GetLocaleLanguage(langid[i]));
		m_combo.SetItemData(k, langid[i]);
	}

	m_originalLanguage= 0;
	for (i=0; i < m_combo.GetCount(); i++)
	{
		if (m_combo.GetItemData(i) == CLanguageOptions::GetLanguage())
		{
			m_combo.SetCurSel(i);
			m_originalLanguage= i;
			break;
		}
	}

	UpdateData(false);
	return TRUE;
}

void CPageGeneral::OnOK()
{
	UpdateData();
	GetOptions()->SetHumanFormat(m_humanFormat);
	GetOptions()->SetFollowMountPoints(m_followMountPoints);
	GetOptions()->SetFollowJunctionPoints(m_followJunctionPoints);
	GetOptions()->SetListGrid(m_listGrid);
	GetOptions()->SetListStripes(m_listStripes);
	GetOptions()->SetListFullRowSelection(m_listFullRowSelection);

	LANGID id= (LANGID)m_combo.GetItemData(m_combo.GetCurSel());
	CLanguageOptions::SetLanguage(id);

	CPropertyPage::OnOK();
}

void CPageGeneral::OnBnClickedHumanformat()
{
	SetModified();
}

void CPageGeneral::OnBnClickedFollowmountpoints()
{
	SetModified();
}

void CPageGeneral::OnBnClickedFollowjunctionpoints()
{
	SetModified();
}

void CPageGeneral::OnBnClickedListGrid()
{
	SetModified();
}

void CPageGeneral::OnBnClickedListStripes()
{
	SetModified();
}

void CPageGeneral::OnBnClickedListFullRowSelection()
{
	SetModified();
}

void CPageGeneral::OnCbnSelendokCombo()
{
	int i= m_combo.GetCurSel();
	GetSheet()->SetLanguageChanged(i != m_originalLanguage);
	SetModified();
}

// $Log$
// Revision 1.7  2004/11/12 22:14:16  bseifert
// Eliminated CLR_NONE. Minor corrections.
//
// Revision 1.6  2004/11/05 16:53:06  assarbad
// Added Date and History tag where appropriate.
//
