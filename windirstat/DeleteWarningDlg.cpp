// DeleteWarningDlg.cpp: implementation of CDeleteWarningDlg
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
#include ".\deletewarningdlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


IMPLEMENT_DYNAMIC(CDeleteWarningDlg, CDialog)

CDeleteWarningDlg::CDeleteWarningDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CDeleteWarningDlg::IDD, pParent)
	, m_fileName(_T(""))
	, m_dontShowAgain(false)
{
}

CDeleteWarningDlg::~CDeleteWarningDlg()
{
}

void CDeleteWarningDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_DONTSHOWAGAIN, m_dontShowAgain);
	DDX_Text(pDX, IDC_FILENAME, m_fileName);
}


BEGIN_MESSAGE_MAP(CDeleteWarningDlg, CDialog)
	ON_BN_CLICKED(IDNO, OnBnClickedNo)
	ON_BN_CLICKED(IDYES, OnBnClickedYes)
END_MESSAGE_MAP()


void CDeleteWarningDlg::OnBnClickedNo()
{
	UpdateData();
	EndDialog(IDNO);
}

void CDeleteWarningDlg::OnBnClickedYes()
{
	UpdateData();
	EndDialog(IDYES);
}

// $Log$
// Revision 1.3  2004/11/05 16:53:06  assarbad
// Added Date and History tag where appropriate.
//
