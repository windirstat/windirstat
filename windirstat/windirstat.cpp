// windirstat.cpp	- Implementation of CDirstatApp and some globals
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
#include "mainframe.h"
#include "selectdrivesdlg.h"
#include "aboutdlg.h"
#include "reportbugdlg.h"
#include "modalsendmail.h"
#include "dirstatdoc.h"
#include "graphview.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


CMainFrame *GetMainFrame()
{
	// Not: return (CMainFrame *)AfxGetMainWnd();
	// because CWinApp::m_pMainWnd is set too late.
	return CMainFrame::GetTheFrame();
}

CDirstatApp *GetApp()
{
	return (CDirstatApp *)AfxGetApp();
}

CString GetAuthorEmail()
{
	return _T("bseifert@users.sourceforge.net");
}

CString GetWinDirStatHomepage()
{
	return _T("windirstat.sourceforge.net");
}

void AddRidge(const CRect& rc, double *surface, double h)
{
	/* 
	Unoptimized:

	if (rc.Width() > 0)
	{
		surface[2]+= 4 * h * (rc.right + rc.left) / (rc.right - rc.left);
		surface[0]-= 4 * h / (rc.right - rc.left);
	}

	if (rc.Height() > 0)
	{
		surface[3]+= 4 * h * (rc.bottom + rc.top) / (rc.bottom - rc.top);
		surface[1]-= 4 * h / (rc.bottom - rc.top);
	}
	*/
	
	// Optimized (gains 15 ms of 1030):

	int width= rc.Width();
	int height= rc.Height();

	ASSERT(width > 0 && height > 0);

	double h4= 4 * h;

	double wf= h4 / width;
	surface[2]+= wf * (rc.right + rc.left);
	surface[0]-= wf;

	double hf= h4 / height;
	surface[3]+= hf * (rc.bottom + rc.top);
	surface[1]-= hf;
}

void RenderRectangle(CDC *pdc, const CRect& rc, const double *surface, COLORREF col)
{
	if (GetOptions()->IsCushionShading())
	{
		RenderCushion(pdc, rc, surface, col);
	}
	else
	{
		pdc->FillSolidRect(rc, col);
	}
}

void RenderCushion(CDC *pdc, const CRect& rc, const double *surface, COLORREF col)
{
	// Cushion parameters
	const double Ia = GetOptions()->GetAmbientLight() / 100.0;

	// where is the light:
	static const double lx = -1;		// negative = left
	static const double ly = -1;		// negative = top
	static const double lz = 10;

	// Derived parameters
	const double Is = 1 - Ia;	// brightness

	static const double len= sqrt(lx*lx + ly*ly + lz*lz);
	static const double Lx = lx / len;
	static const double Ly = lx / len;
	static const double Lz = lz / len;

	const double colR= GetRValue(col);
	const double colG= GetGValue(col);
	const double colB= GetBValue(col);

	for (int iy = rc.top; iy < rc.bottom; iy++)
	for (int ix = rc.left; ix < rc.right; ix++)
	{
		double nx= -(2 * surface[0] * (ix + 0.5) + surface[2]);
		double ny= -(2 * surface[1] * (iy + 0.5) + surface[3]);
		double cosa= (nx*Lx + ny*Ly + Lz) / sqrt(nx*nx + ny*ny + 1.0);
		
		double brightness= Is * cosa;
		if (brightness < 0)
			brightness= 0;
		
		brightness+= Ia;
		ASSERT(brightness <= 1.0);

		brightness*= 2.5 / BASE_BRIGHTNESS;

		int red		= (int)(colR * brightness);
		int green	= (int)(colG * brightness);
		int blue	= (int)(colB * brightness);

		NormalizeColor(red, green, blue);

		pdc->SetPixel(ix, iy, RGB(red, green, blue));
	}
}

CMyImageList *GetMyImageList()
{
	return GetApp()->GetMyImageList();
}


// CDirstatApp

BEGIN_MESSAGE_MAP(CDirstatApp, CWinApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_HELP_MANUAL, OnHelpManual)
	ON_UPDATE_COMMAND_UI(ID_HELP_REPORTBUG, OnUpdateHelpReportbug)
	ON_COMMAND(ID_HELP_REPORTBUG, OnHelpReportbug)
END_MESSAGE_MAP()


CDirstatApp _theApp;

CDirstatApp::CDirstatApp()
{
	m_workingSet= 0;
	m_pageFaults= 0;

	#ifdef _DEBUG
		TestScanResourceDllName();
	#endif
}

CMyImageList *CDirstatApp::GetMyImageList()
{
	m_myImageList.Initialize();
	return &m_myImageList;
}

void CDirstatApp::UpdateRamUsage()
{
	CWinThread::OnIdle(0);
}

CString CDirstatApp::FindResourceDllPathByLangid(LANGID& langid)
{
	return FindAuxiliaryFileByLangid(_T("wdsr"), _T(".dll"), langid, true);
}

CString CDirstatApp::FindHelpfilePathByLangid(LANGID langid)
{
	CString s;
	if (langid == GetBuiltInLanguage())
	{
		// The english help file is named windirstat.chm.
		s= GetAppFolder() + _T("\\windirstat.chm");
		if (FileExists(s))
			return s;
	}

	// Help files for other languages are named wdshxxxx.chm (xxxx = LANGID).
	s= FindAuxiliaryFileByLangid(_T("wdsh"), _T(".chm"), langid, false);
	if (!s.IsEmpty())
		return s;

	// If our primary language is English, try windirstat.chm again.
	if (PRIMARYLANGID(langid) == PRIMARYLANGID(GetBuiltInLanguage()))
	{
		s= GetAppFolder() + _T("\\windirstat.chm");
		if (FileExists(s))
			return s;
	}

	// Not found.
	return _T("");
}

void CDirstatApp::GetAvailableResourceDllLangids(CArray<LANGID, LANGID>& arr)
{
	arr.RemoveAll();

	CFileFind finder;
	BOOL b= finder.FindFile(GetAppFolder() + _T("\\wdsr*.dll"));
	while (b)
	{
		b= finder.FindNextFile();
		if (finder.IsDirectory())
			continue;

		LANGID langid;
		if (ScanResourceDllName(finder.GetFileName(), langid) && IsCorrectResourceDll(finder.GetFilePath()))
			arr.Add(langid);
	}
}

bool CDirstatApp::ScanResourceDllName(LPCTSTR name, LANGID& langid)
{
	return ScanAuxiliaryFileName(_T("wdsr"), _T(".dll"), name, langid);
}

bool CDirstatApp::ScanAuxiliaryFileName(LPCTSTR prefix, LPCTSTR suffix, LPCTSTR name, LANGID& langid)
{
	ASSERT(lstrlen(prefix) == 4);	// "wdsr" or "wdsh"
	ASSERT(lstrlen(suffix) == 4);	// ".dll" or ".chm"

	CString s= name;	// "wdsr0a01.dll"
	s.MakeLower();
	if (s.Left(4) != prefix)
		return false;
	s= s.Mid(4);		// "0a01.dll"

	if (s.GetLength() != 8)
		return false;

	if (s.Mid(4) != suffix)
		return false;

	s= s.Left(4);		// "0a01"

	for (int i=0; i < 4; i++)
		if (!IsHexDigit(s[i]))
			return false;

	int id;
	VERIFY(1 == _stscanf(s, _T("%04x"), &id));
	langid= (LANGID)id;

	return true;
}

#ifdef _DEBUG
	void CDirstatApp::TestScanResourceDllName()
	{
		LANGID id;
		ASSERT(!ScanResourceDllName(_T(""), id));
		ASSERT(!ScanResourceDllName(_T("wdsr.dll"), id));
		ASSERT(!ScanResourceDllName(_T("wdsr123.dll"), id));
		ASSERT(!ScanResourceDllName(_T("wdsr12345.dll"), id));
		ASSERT(!ScanResourceDllName(_T("wdsr1234.exe"), id));
		ASSERT(ScanResourceDllName(_T("wdsr0123.dll"), id));
			ASSERT(id == 0x0123);
		ASSERT(ScanResourceDllName(_T("WDsRa13F.dll"), id));
			ASSERT(id == 0xa13f);
	}
#endif

CString CDirstatApp::FindAuxiliaryFileByLangid(LPCTSTR prefix, LPCTSTR suffix, LANGID& langid, bool checkResource)
{
	CString number;
	number.Format(_T("%04x"), langid);

	CString exactName;
	exactName.Format(_T("%s%s%s"), prefix, number, suffix);

	CString exactPath= GetAppFolder() + _T("\\") + exactName;
	if (FileExists(exactPath) && (!checkResource || IsCorrectResourceDll(exactPath)))
		return exactPath;

	CString search;
	search.Format(_T("%s*%s"), prefix, suffix);

	CFileFind finder;
	BOOL b= finder.FindFile(GetAppFolder() + _T("\\") + search);
	while (b)
	{
		b= finder.FindNextFile();
		if (finder.IsDirectory())
			continue;

		LANGID id;
		if (!ScanAuxiliaryFileName(prefix, suffix, finder.GetFileName(), id))
			continue;

		if (PRIMARYLANGID(id) == PRIMARYLANGID(langid) && (!checkResource || IsCorrectResourceDll(finder.GetFilePath())))
		{
			langid= id;
			return finder.GetFilePath();
		}
	}

	return _T("");
}

CString CDirstatApp::ConstructHelpFileName()
{
	return FindHelpfilePathByLangid(CLanguageOptions::GetLanguage());
}

bool CDirstatApp::IsCorrectResourceDll(LPCTSTR path)
{
	HMODULE module= LoadLibrary(path);
	if (module == NULL)
		return false;

	CString reference= LoadString(IDS_RESOURCEVERSION);
	
	int bufsize= reference.GetLength() * 2;
	CString s;
	int r= LoadString(module, IDS_RESOURCEVERSION, s.GetBuffer(bufsize), bufsize);
	s.ReleaseBuffer();

	FreeLibrary(module);

	if (r == 0 || s != reference)
		return false;

	return true;
}

void CDirstatApp::ReReadMountPoints()
{
	m_mountPoints.Initialize();
}

bool CDirstatApp::IsMountPoint(CString path)
{
	return m_mountPoints.IsMountPoint(path);
}

CString CDirstatApp::GetCurrentProcessMemoryInfo()
{
	UpdateMemoryInfo();

	if (m_workingSet == 0)
		return _T("");

	CString n= PadWidthBlanks(FormatBytes(m_workingSet), 11);

	CString s;
	s.FormatMessage(IDS_RAMUSAGEs, n);

	return s;
}

bool CDirstatApp::UpdateMemoryInfo()
{
	if (!m_psapi.IsSupported())
		return false;

	PROCESS_MEMORY_COUNTERS pmc;
	ZeroMemory(&pmc, sizeof(pmc));
	pmc.cb= sizeof(pmc);

	if (!m_psapi.GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
		return false;

	m_workingSet= pmc.WorkingSetSize;

	bool ret= false;
	if (pmc.PageFaultCount > m_pageFaults + 500)
		ret= true;

	m_pageFaults= pmc.PageFaultCount;

	return ret;
}

LANGID CDirstatApp::GetBuiltInLanguage() 
{ 
	return MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US); 
}

BOOL CDirstatApp::InitInstance()
{
	CWinApp::InitInstance();

	InitCommonControls();			// InitCommonControls() is necessary for Windows XP.
	VERIFY(AfxOleInit());			// For SHBrowseForFolder()
	AfxEnableControlContainer();	// For our rich edit controls in the about dialog
	VERIFY(AfxInitRichEdit());		// Rich edit control in out about box
	VERIFY(AfxInitRichEdit2());		// On NT, this helps.
	EnableHtmlHelp();

	SetRegistryKey(_T("Seifert"));
	LoadStdProfileSettings(4);

	m_langid= GetBuiltInLanguage(); 

	LANGID langid= CLanguageOptions::GetLanguage();
	if (langid != GetBuiltInLanguage())
	{
		CString resourceDllPath= FindResourceDllPathByLangid(langid);
		if (!resourceDllPath.IsEmpty())
		{
			HINSTANCE dll= LoadLibrary(resourceDllPath);
			if (dll != NULL)
			{
				AfxSetResourceHandle(dll);
				m_langid= langid;
			}
			else
			{
				TRACE(_T("LoadLibrary(%s) failed: %u\r\n"), resourceDllPath, GetLastError());
			}
		}
		// else: We use our built-in English resources.
	}

	GetOptions()->LoadFromRegistry();

	free((void*)m_pszHelpFilePath);
	m_pszHelpFilePath=_tcsdup(ConstructHelpFileName()); // ~CWinApp() will free this memory.

	m_pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CDirstatDoc),
		RUNTIME_CLASS(CMainFrame),
		RUNTIME_CLASS(CGraphView));
	if (!m_pDocTemplate)
		return FALSE;
	AddDocTemplate(m_pDocTemplate);
	
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	m_nCmdShow= SW_HIDE;
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	GetMainFrame()->InitialShowWindow();
	m_pMainWnd->UpdateWindow();

	// When called by setup.exe, windirstat remained in the
	// background, so we do a
	m_pMainWnd->BringWindowToTop();
	m_pMainWnd->SetForegroundWindow();

	if (cmdInfo.m_nShellCommand != CCommandLineInfo::FileOpen)
	{
		OnFileOpen();
	}

	return TRUE;
}

LANGID CDirstatApp::GetLangid()
{
	return m_langid;
}

void CDirstatApp::OnAppAbout()
{
	StartAboutDialog();
}

void CDirstatApp::OnFileOpen()
{
	CSelectDrivesDlg dlg;
	if (IDOK == dlg.DoModal())
	{
		CString path= CDirstatDoc::EncodeSelection((RADIO)dlg.m_radio, dlg.m_folderName, dlg.m_drives);
		m_pDocTemplate->OpenDocumentFile(path, true);
	}
}

BOOL CDirstatApp::OnIdle(LONG lCount)
{
	bool more= false;

	CDirstatDoc *doc= GetDocument();
	if (doc != NULL && !doc->Work(600)) 
		more= true;

	if (CWinApp::OnIdle(lCount))
		more= true;
	
	// The status bar (RAM usage) is updated only when count == 0.
	// That's why we call an extra OnIdle(0) here.
	if (CWinThread::OnIdle(0))
		more= true;
	
	return more;
}

void CDirstatApp::OnHelpManual()
{
	DoContextHelp(IDH_StartPage);
}

void CDirstatApp::DoContextHelp(DWORD topic)
{
	if (FileExists(m_pszHelpFilePath))
	{
		// I want a NULL parent window. So I don't use CWinApp::HtmlHelp().
		::HtmlHelp(NULL, m_pszHelpFilePath, HH_HELP_CONTEXT, topic);
	}
	else
	{
		CString msg;
		msg.FormatMessage(IDS_HELPFILEsCOULDNOTBEFOUND, _T("windirstat.chm"));
		AfxMessageBox(msg);
	}
}

void CDirstatApp::OnUpdateHelpReportbug(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(CModalSendMail::IsSendMailAvailable());
}

void CDirstatApp::OnHelpReportbug()
{
	CReportBugDlg dlg;
	if (IDOK == dlg.DoModal())
	{
		CModalSendMail msm;
		msm.SendMail(dlg.m_recipient, dlg.m_subject, dlg.m_body);
	}
}
