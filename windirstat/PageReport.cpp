// PageReport.cpp	- Implementation of CPageReport
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

#include "stdafx.h"
#include "windirstat.h"
#include "PageReport.h"
#include ".\pagereport.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


IMPLEMENT_DYNAMIC(CPageReport, CPropertyPage)

CPageReport::CPageReport()
	: CPropertyPage(CPageReport::IDD)
	, m_subject(_T(""))
	, m_prefix(_T(""))
	, m_suffix(_T(""))
{
}

CPageReport::~CPageReport()
{
}

void CPageReport::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_SUBJECT, m_subject);
	DDX_Text(pDX, IDC_PREFIX, m_prefix);
	DDX_Text(pDX, IDC_SUFFIX, m_suffix);
	DDX_Control(pDX, IDC_RESET, m_reset);
}


BEGIN_MESSAGE_MAP(CPageReport, CPropertyPage)
	ON_BN_CLICKED(IDC_RESET, OnBnClickedReset)
	ON_EN_CHANGE(IDC_SUBJECT, OnEnChangeSubject)
	ON_EN_CHANGE(IDC_PREFIX, OnEnChangePrefix)
	ON_EN_CHANGE(IDC_SUFFIX, OnEnChangeSuffix)
END_MESSAGE_MAP()



BOOL CPageReport::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	m_subject= GetOptions()->GetReportSubject();
	m_prefix= GetOptions()->GetReportPrefix();
	m_suffix= GetOptions()->GetReportSuffix();

	ValuesAltered();

	UpdateData(false);
	return TRUE;
}

void CPageReport::ValuesAltered(bool altered)
{
	m_altered= altered;
	CString s = LoadString(m_altered ? IDS_RESETTODEFAULTS : IDS_BACKTOUSERSETTINGS);
	m_reset.SetWindowText(s);
}

void CPageReport::OnOK()
{
	UpdateData();

	GetOptions()->SetReportSubject(m_subject);
	GetOptions()->SetReportPrefix(m_prefix);
	GetOptions()->SetReportSuffix(m_suffix);

	CPropertyPage::OnOK();
}

void CPageReport::OnBnClickedReset()
{
	UpdateData();

	if (m_altered)
	{
		m_undoSubject= m_subject;
		m_undoPrefix= m_prefix;
		m_undoSuffix= m_suffix;

		m_subject= GetOptions()->GetReportDefaultSubject();
		m_prefix= GetOptions()->GetReportDefaultPrefix();
		m_suffix= GetOptions()->GetReportDefaultSuffix();
	}
	else
	{
		m_subject= m_undoSubject;
		m_prefix= m_undoPrefix;
		m_suffix= m_undoSuffix;
	}

	ValuesAltered(!m_altered);
	UpdateData(false);
	SetModified();
}

void CPageReport::OnEnChangeSubject()
{
	ValuesAltered();
	SetModified();
}

void CPageReport::OnEnChangePrefix()
{
	ValuesAltered();
	SetModified();
}

void CPageReport::OnEnChangeSuffix()
{
	ValuesAltered();
	SetModified();
}
