// pagereport.h		- Declaration of CPageReport
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

#pragma once


class CPageReport: public CPropertyPage
{
	DECLARE_DYNAMIC(CPageReport)
	enum { IDD = IDD_PAGE_REPORT };

public:
	CPageReport();
	virtual ~CPageReport();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK();

	void ValuesAltered(bool altered = true);

	CString m_subject;
	CString m_prefix;
	CString m_suffix;

	bool m_altered;		// Values have been modified. Button reads "Reset to defaults"
	CString m_undoSubject;
	CString m_undoPrefix;
	CString m_undoSuffix;

	CButton m_reset;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedReset();
public:
	afx_msg void OnEnChangeSubject();
	afx_msg void OnEnChangePrefix();
	afx_msg void OnEnChangeSuffix();
};

// $Log$
// Revision 1.3  2004/11/05 16:53:06  assarbad
// Added Date and History tag where appropriate.
//
