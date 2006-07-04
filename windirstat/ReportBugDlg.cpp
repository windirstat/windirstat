// ReportBugDlg.cpp - Implementation of CReportBugDlg
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
// Author: bseifert@users.sourceforge.net, bseifert@daccord.net
//
// Last modified: $Date$

#include "stdafx.h"
#include "windirstat.h"
#include "aboutdlg.h"
#include "getosplatformstring.h"
#include "ReportBugDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	enum
	{
		SEV_CRITICAL,
		SEV_GRAVE,
		SEV_NORMAL,
		SEV_WISH,
		SEV_FEEDBACK
	};
}


IMPLEMENT_DYNAMIC(CReportBugDlg, CDialog)

CReportBugDlg::CReportBugDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CReportBugDlg::IDD, pParent)
	, m_from(strEmpty)
	, m_to(strEmpty)
	, m_application(strEmpty)
	, m_platform(strEmpty)
	, m_hint(strEmpty)
	, m_severity(SEV_FEEDBACK)
	, m_inAWord(strEmpty)
	, m_text(strEmpty)
	, m_layout(this, TEXT("rbdlg"))
{
}

CReportBugDlg::~CReportBugDlg()
{
}

void CReportBugDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_FROM, m_from);
	DDX_Text(pDX, IDC_TO, m_to);
	DDX_Text(pDX, IDC_APPLICATION, m_application);
	DDX_Text(pDX, IDC_PLATFORM, m_platform);
	DDX_Text(pDX, IDC_HINT, m_hint);
	DDX_Radio(pDX, IDC_CRITICAL, m_severity);
	DDX_Text(pDX, IDC_INAWORD, m_inAWord);
	DDX_Text(pDX, IDC_TEXT, m_text);
}


BEGIN_MESSAGE_MAP(CReportBugDlg, CDialog)
	ON_BN_CLICKED(IDC_CRITICAL, OnBnClickedCritical)
	ON_BN_CLICKED(IDC_GRAVE, OnBnClickedGrave)
	ON_BN_CLICKED(IDC_NORMAL, OnBnClickedNormal)
	ON_BN_CLICKED(IDC_WISH, OnBnClickedWish)
	ON_BN_CLICKED(IDC_FEEDBACK, OnBnClickedFeedback)
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
	ON_WM_DESTROY()
END_MESSAGE_MAP()



BOOL CReportBugDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_layout.AddControl(IDC_STATIC_TO,		1, 0, 0, 0);
	m_layout.AddControl(IDC_STATIC_PLATFORM,1, 0, 0, 0);
	m_layout.AddControl(IDC_TO,				1, 0, 0, 0);
	m_layout.AddControl(IDC_PLATFORM,		1, 0, 0, 0);
	m_layout.AddControl(IDC_STATIC_SEVERITY,0, 0, 1, 0);
	m_layout.AddControl(IDC_CRITICAL,		0, 0, 0, 0);
	m_layout.AddControl(IDC_GRAVE,			0.25, 0, 0, 0);
	m_layout.AddControl(IDC_NORMAL,			0.50, 0, 0, 0);
	m_layout.AddControl(IDC_WISH,			0.75, 0, 0, 0);
	m_layout.AddControl(IDC_FEEDBACK,		1.0,  0, 0, 0);
	m_layout.AddControl(IDC_INAWORD,		0, 0, 1, 0);
	m_layout.AddControl(IDC_TEXT,			0, 0, 1, 1);
	m_layout.AddControl(IDC_STATIC_OKHINT,	0, 1, 0, 0);
	m_layout.AddControl(IDOK,				0.5, 1, 0, 0);
	m_layout.AddControl(IDCANCEL,			1, 1, 0, 0);

	m_layout.OnInitDialog(true);

	m_from = GetUserName();
	m_to = GetFeedbackEmail();
	m_application = CAboutDlg::GetAppVersion();
	m_platform = GetOsPlatformString();

	UpdateData(false);
	OnSeverityClick();
	return TRUE;
}

void CReportBugDlg::OnBnClickedCritical()
{
	OnSeverityClick();
}

void CReportBugDlg::OnBnClickedGrave()
{
	OnSeverityClick();
}

void CReportBugDlg::OnBnClickedNormal()
{
	OnSeverityClick();
}

void CReportBugDlg::OnBnClickedWish()
{
	OnSeverityClick();
}

void CReportBugDlg::OnBnClickedFeedback()
{
	OnSeverityClick();
}

void CReportBugDlg::OnSeverityClick()
{
	UpdateData();
	switch (m_severity)
	{
	case SEV_CRITICAL:
	case SEV_GRAVE:
	case SEV_NORMAL:
		{
			m_hint.LoadString(IDS_BUGREPORTHINT);
		}
		break;
	case SEV_WISH:
		{
			m_hint.Empty();
		}
		break;
	case SEV_FEEDBACK:
		{
			m_hint.LoadString(IDS_FEEDBACKHINT);
		}
		break;
	default:
		{
			ASSERT(0);
		}
	}
	
	UpdateData(false);
}


void CReportBugDlg::OnOK()
{
	UpdateData();

	m_recipient = m_to;
	m_subject.Format(TEXT("%s - %s: %s"), m_application, GetSeverityString(), m_inAWord);
	m_body.Empty();

	CString s;
	
	s.FormatMessage(IDS_FROMsPLATFORMs, m_from, m_platform);
	m_body += s;

	s = m_text;
	s.TrimLeft();
	s.TrimRight();
	s += TEXT("\r\n");
	m_body += s;

	CDialog::OnOK();
}

CString CReportBugDlg::GetSeverityString()
{
	CString s;

	switch (m_severity)
	{
	case SEV_CRITICAL:
		{
			s.LoadString(IDS_SEV_CRITICAL);
		}
		break;
	case SEV_GRAVE:
		{
			s.LoadString(IDS_SEV_GRAVE);
		}
		break;
	case SEV_NORMAL:
		{
			s.LoadString(IDS_SEV_NORMAL);
		}
		break;
	case SEV_WISH:
		{
			s.LoadString(IDS_SEV_WISH);
		}
		break;
	case SEV_FEEDBACK:
		{
			s.LoadString(IDS_SEV_FEEDBACK);
		}
		break;
	default:
		{
			ASSERT(0);
		}
	}

	return s;
}

void CReportBugDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);
	m_layout.OnSize();
}

void CReportBugDlg::OnGetMinMaxInfo(MINMAXINFO* mmi)
{
	m_layout.OnGetMinMaxInfo(mmi);
	CDialog::OnGetMinMaxInfo(mmi);
}

void CReportBugDlg::OnDestroy()
{
	m_layout.OnDestroy();
	CDialog::OnDestroy();
}

// $Log$
// Revision 1.9  2006/07/04 20:45:22  assarbad
// - See changelog for the changes of todays previous check-ins as well as this one!
//
// Revision 1.8  2005/04/10 16:49:30  assarbad
// - Some smaller fixes including moving the resource string version into the rc2 files
//
// Revision 1.7  2004/12/25 13:41:47  bseifert
// Synced help files.
//
// Revision 1.6  2004/12/19 10:52:39  bseifert
// Minor fixes.
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
