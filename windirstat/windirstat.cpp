// windirstat.cpp	- Implementation of CDirstatApp and some globals
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
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

CString GetFeedbackEmail()
{
	return _T("windirstat-feedback@lists.sourceforge.net");
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
	m_lastPeriodicalRamUsageUpdate= GetTickCount();
	m_altEncryptionColor=	GetAlternativeColor(RGB(0x00, 0x80, 0x00), _T("AltEncryptionColor"));
	m_altColor=				GetAlternativeColor(RGB(0x00, 0x00, 0xFF), _T("AltColor"));

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

void CDirstatApp::PeriodicalUpdateRamUsage()
{
	if (GetTickCount() - m_lastPeriodicalRamUsageUpdate > 1200)
	{
		UpdateRamUsage();
		m_lastPeriodicalRamUsageUpdate= GetTickCount();
	}
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

	// Else, try windirstat.chm again.
	s= GetAppFolder() + _T("\\windirstat.chm");
	if (FileExists(s))
		return s;

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

void CDirstatApp::RestartApplication()
{
	// First, try to create the suspended process
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb= sizeof(si);

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	BOOL success = CreateProcess(GetAppFileName(), NULL, NULL, NULL, false, CREATE_SUSPENDED, NULL, NULL, &si, &pi);
	if (!success)
	{
		CString s;
		s.FormatMessage(IDS_CREATEPROCESSsFAILEDs, GetAppFileName(), MdGetWinerrorText(GetLastError()));
		AfxMessageBox(s);
		return;
	}

	// We _send_ the WM_CLOSE here to ensure that all CPersistence-Settings
	// like column widths an so on are saved before the new instance is resumed.
	// This will post a WM_QUIT message.
	GetMainFrame()->SendMessage(WM_CLOSE);

	DWORD dw= ::ResumeThread(pi.hThread);
	if (dw != 1)
		TRACE(_T("ResumeThread() didn't return 1\r\n"));

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
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
	HMODULE module = LoadLibrary(path);
	if (module == NULL)
		return false;

	CString reference = LoadString(IDS_RESOURCEVERSION);
	
	int bufsize = reference.GetLength() * 2;
	CString s;
	int r = LoadString(module, IDS_RESOURCEVERSION, s.GetBuffer(bufsize), bufsize);
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

bool CDirstatApp::IsJunctionPoint(CString path)
{
	return m_mountPoints.IsJunctionPoint(path);
}

// Get the alternative colors for compressed and encrypted files/folders.
// This function uses either the value defined in the Explorer configuration
// or the default color values.
COLORREF CDirstatApp::GetAlternativeColor(COLORREF clrDefault, LPCTSTR which)
{
	const LPCTSTR explorerKey = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer");
	COLORREF x; DWORD cbValue = sizeof(x); CRegKey key;

	// Open the explorer key
	key.Open(HKEY_CURRENT_USER, explorerKey, KEY_READ);

	// Try to read the REG_BINARY value
	if (ERROR_SUCCESS == key.QueryBinaryValue(which, &x, &cbValue))
	{
		// Return the read value upon success
		return x;
	}
	else
	{
		// Return the default upon failure
		return clrDefault;
	}
}

COLORREF CDirstatApp::AltColor()
{
	// Return property value
	return m_altColor;
}

COLORREF CDirstatApp::AltEncryptionColor()
{
	// Return property value
	return m_altEncryptionColor;
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

CGetCompressedFileSizeApi *CDirstatApp::GetComprSizeApi()
{
	return &m_comprSize;
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

	LANGID langid = CLanguageOptions::GetLanguage();
	if (langid != GetBuiltInLanguage())
	{
		CString resourceDllPath = FindResourceDllPathByLangid(langid);
		if (!resourceDllPath.IsEmpty())
		{
			// Load language resource DLL
			HINSTANCE dll = LoadLibrary(resourceDllPath);
			if (dll != NULL)
			{
				// Set default module handle for loading of resources
				AfxSetResourceHandle(dll);
				m_langid = langid;
			}
			else
			{
				TRACE(_T("LoadLibrary(%s) failed: %u\r\n"), resourceDllPath, GetLastError());
			}
		}
		// else: We use our built-in English resources.

		CLanguageOptions::SetLanguage(m_langid);
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

int CDirstatApp::ExitInstance()
{
	return CWinApp::ExitInstance();
}

LANGID CDirstatApp::GetLangid()
{
	return m_langid;
}

LANGID CDirstatApp::GetEffectiveLangid()
{
	if (GetOptions()->IsUseWdsLocale())
		return GetLangid();
	else
		return GetUserDefaultLangID();
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

// $Log$
// Revision 1.16  2005/04/17 12:27:21  assarbad
// - For details see changelog of 2005-04-17
//
// Revision 1.15  2005/04/10 16:49:30  assarbad
// - Some smaller fixes including moving the resource string version into the rc2 files
//
// Revision 1.14  2004/12/19 10:52:39  bseifert
// Minor fixes.
//
// Revision 1.13  2004/11/28 14:40:06  assarbad
// - Extended CFileFindWDS to replace a global function
// - Now packing/unpacking the file attributes. This even spares a call to find encrypted/compressed files.
//
// Revision 1.12  2004/11/25 11:58:52  assarbad
// - Minor fixes (odd behavior of coloring in ANSI version, caching of the GetCompressedFileSize API)
//   for details see the changelog.txt
//
// Revision 1.11  2004/11/14 08:49:06  bseifert
// Date/Time/Number formatting now uses User-Locale. New option to force old behavior.
//
// Revision 1.10  2004/11/12 22:14:16  bseifert
// Eliminated CLR_NONE. Minor corrections.
//
// Revision 1.9  2004/11/10 01:03:00  assarbad
// - Style cleaning of the alternative coloring code for compressed/encrypted items
//
// Revision 1.8  2004/11/08 00:46:26  assarbad
// - Added feature to distinguish compressed and encrypted files/folders by color as in the Windows 2000/XP explorer.
//   Same rules apply. (Green = encrypted / Blue = compressed)
//
// Revision 1.7  2004/11/05 16:53:08  assarbad
// Added Date and History tag where appropriate.
//
