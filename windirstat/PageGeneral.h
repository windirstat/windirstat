// pagegeneral.h	- Declaration of CPageGeneral
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

#pragma once

class COptionsPropertySheet;

//
// CPageGeneral. "Settings" property page "General".
//
class CPageGeneral : public CPropertyPage
{
	DECLARE_DYNAMIC(CPageGeneral)
	enum { IDD = IDD_PAGE_GENERAL };

public:
	CPageGeneral();
	virtual ~CPageGeneral();

protected:
	COptionsPropertySheet *GetSheet();

	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK();

	BOOL m_followMountPoints;
	BOOL m_followJunctionPoints;
	BOOL m_humanFormat;
	BOOL m_listGrid;
	BOOL m_listStripes;
	BOOL m_listFullRowSelection;

	CComboBox m_combo;
	CButton m_ctlFollowMountPoints;
	CButton m_ctlFollowJunctionPoints;

	int m_originalLanguage;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedHumanformat();
	afx_msg void OnBnClickedFollowmountpoints();
	afx_msg void OnBnClickedFollowjunctionpoints();
	afx_msg void OnCbnSelendokCombo();
	afx_msg void OnBnClickedListGrid();
	afx_msg void OnBnClickedListStripes();
	afx_msg void OnBnClickedListFullRowSelection();
};
