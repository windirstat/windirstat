// PageGeneral.cpp	- Implementation of CPageGeneral
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
#include ".\pagegeneral.h"


IMPLEMENT_DYNAMIC(CPageGeneral, CPropertyPage)

CPageGeneral::CPageGeneral()
	: CPropertyPage(CPageGeneral::IDD)
	, m_humanFormat(FALSE)
	, m_followMountPoints(FALSE)
	, m_pacmanAnimation(FALSE)
{
}

CPageGeneral::~CPageGeneral()
{
}

void CPageGeneral::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_HUMANFORMAT, m_humanFormat);
	DDX_Check(pDX, IDC_FOLLOWMOUNTPOINTS, m_followMountPoints);
	DDX_Check(pDX, IDC_PACMANANIMATION, m_pacmanAnimation);
	DDX_Check(pDX, IDC_SHOWTIMESPENT, m_showTimeSpent);
	DDX_Control(pDX, IDC_COMBO, m_combo);
	DDX_Control(pDX, IDC_FOLLOWMOUNTPOINTS, m_ctlFollowMountPoints);
}


BEGIN_MESSAGE_MAP(CPageGeneral, CPropertyPage)
	ON_BN_CLICKED(IDC_HUMANFORMAT, OnBnClickedHumanformat)
	ON_BN_CLICKED(IDC_FOLLOWMOUNTPOINTS, OnBnClickedFollowmountpoints)
	ON_BN_CLICKED(IDC_PACMANANIMATION, OnBnClickedPacmananimation)
	ON_BN_CLICKED(IDC_SHOWTIMESPENT, OnBnClickedShowTimeSpent)
END_MESSAGE_MAP()


BOOL CPageGeneral::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	m_humanFormat= GetOptions()->IsHumanFormat();
	m_pacmanAnimation= GetOptions()->IsPacmanAnimation();
	m_showTimeSpent= GetOptions()->IsShowTimeSpent();

	m_followMountPoints= GetOptions()->IsFollowMountPoints();
	CVolumeApi va;
	if (!va.IsSupported())
	{
		m_followMountPoints= false;	// Otherwise we would see pacman only.
		m_ctlFollowMountPoints.ShowWindow(SW_HIDE); // Ignorance is bliss.
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

	for (i=0; i < m_combo.GetCount(); i++)
	{
		if (m_combo.GetItemData(i) == GetApp()->GetLangid())
		{
			m_combo.SetCurSel(i);
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
	GetOptions()->SetPacmanAnimation(m_pacmanAnimation);
	GetOptions()->SetShowTimeSpent(m_showTimeSpent);

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

void CPageGeneral::OnBnClickedPacmananimation()
{
	SetModified();
}

void CPageGeneral::OnBnClickedShowTimeSpent()
{
	SetModified();
}
