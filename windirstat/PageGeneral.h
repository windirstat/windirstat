// pagegeneral.h	- Declaration of CPageGeneral
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

#pragma once


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
	virtual void DoDataExchange(CDataExchange* pDX);

	BOOL m_humanFormat;

	DECLARE_MESSAGE_MAP()
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnBnClickedHumanformat();
public:
	BOOL m_followMountPoints;
	afx_msg void OnBnClickedFollowmountpoints();
	BOOL m_pacmanAnimation;
	afx_msg void OnBnClickedPacmananimation();
	CComboBox m_combo;
	CButton m_ctlFollowMountPoints;
};
