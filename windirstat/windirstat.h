// windirstat.h - Main header for the windirstat application
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

#pragma once

#include "resource.h" 
#include "myimagelist.h"
#include "osspecific.h"
#include "globalhelpers.h"
#include "options.h"
#include "mountpoints.h"
#include "helpmap.h"

typedef CMap<CString, LPCTSTR, COLORREF, COLORREF> CExtensionColorMap;	// ".bmp" -> color

class CMainFrame;
class CDirstatApp;

// Frequently used "globals"
CMainFrame *GetMainFrame();
CDirstatApp *GetApp();
CMyImageList *GetMyImageList();

// Other application related globals
CString GetAuthorEmail();
CString GetWinDirStatHomepage();
CString GetFeedbackEmail();

#define MyGetDiskFreeSpace GetApp()->GetFreeSpaceApi()->GetDiskFreeSpace

//
// CDirstatApp. The MFC application object. 
// Knows about RAM Usage, Mount points, Help files and the CMyImageList.
//
class CDirstatApp : public CWinApp
{
public:
	CDirstatApp();
	virtual BOOL InitInstance();
	virtual int ExitInstance();

	LANGID GetBuiltInLanguage() ;
	LANGID GetLangid();				// Language as selected in PageGeneral
	LANGID GetEffectiveLangid();	// Language to be used for date/time and number formatting

	void ReReadMountPoints();
	bool IsMountPoint(CString path);
	bool IsJunctionPoint(CString path);

	COLORREF AltColor();					// Coloring of compressed items
	COLORREF AltEncryptionColor();			// Coloring of encrypted items

	CString GetCurrentProcessMemoryInfo();

	CMyImageList *GetMyImageList();

	void UpdateRamUsage();
	
	void PeriodicalUpdateRamUsage();

	void DoContextHelp(DWORD topic);

	void GetAvailableResourceDllLangids(CArray<LANGID, LANGID>& arr);

	void RestartApplication();

	CGetCompressedFileSizeApi *GetComprSizeApi();
	CGetDiskFreeSpaceApi *GetFreeSpaceApi();

protected:
	CString FindResourceDllPathByLangid(LANGID& langid);
	CString FindHelpfilePathByLangid(LANGID langid);
	CString FindAuxiliaryFileByLangid(LPCTSTR prefix, LPCTSTR suffix, LANGID& langid, bool checkResource);
	bool ScanResourceDllName(LPCTSTR name, LANGID& langid);
	bool ScanAuxiliaryFileName(LPCTSTR prefix, LPCTSTR suffix, LPCTSTR name, LANGID& langid);
	#ifdef _DEBUG
		void TestScanResourceDllName();
	#endif
	bool IsCorrectResourceDll(LPCTSTR path);

	CString ConstructHelpFileName();
	
	bool UpdateMemoryInfo();

	// Get the alternative color from Explorer configuration
	COLORREF GetAlternativeColor(COLORREF clrDefault, LPCTSTR which);

	virtual BOOL OnIdle(LONG lCount);		// This is, where scanning is done.

	CSingleDocTemplate* m_pDocTemplate;		// MFC voodoo.

	LANGID m_langid;						// Language we are running
	CMountPoints m_mountPoints;				// Mount point information
	CMyImageList m_myImageList;				// Out central image list
	CPsapi m_psapi;							// Dynamically linked psapi.dll (for RAM usage)
	CGetCompressedFileSizeApi m_comprSize;	// Dynamically linked API GetCompressedFileSize()
	CGetDiskFreeSpaceApi m_freeSpace;		// For compatibility with W95 first release!
	ULONGLONG m_workingSet;					// Current working set (RAM usage)
	ULONGLONG m_pageFaults;					// Page faults so far (unused)
	DWORD m_lastPeriodicalRamUsageUpdate;	// Tick count
	COLORREF m_altColor;					// Coloring of compressed items
	COLORREF m_altEncryptionColor;			// Coloring of encrypted items

	DECLARE_MESSAGE_MAP()
	afx_msg void OnFileOpen();
	afx_msg void OnHelpManual();
	afx_msg void OnAppAbout();
	afx_msg void OnUpdateHelpReportbug(CCmdUI *pCmdUI);
	afx_msg void OnHelpReportbug();
};


// $Log$
// Revision 1.15  2006/07/04 20:45:23  assarbad
// - See changelog for the changes of todays previous check-ins as well as this one!
//
// Revision 1.14  2005/10/01 11:21:08  assarbad
// *** empty log message ***
//
// Revision 1.13  2004/12/19 10:52:39  bseifert
// Minor fixes.
//
// Revision 1.12  2004/11/28 14:40:06  assarbad
// - Extended CFileFindWDS to replace a global function
// - Now packing/unpacking the file attributes. This even spares a call to find encrypted/compressed files.
//
// Revision 1.11  2004/11/25 11:58:52  assarbad
// - Minor fixes (odd behavior of coloring in ANSI version, caching of the GetCompressedFileSize API)
//   for details see the changelog.txt
//
// Revision 1.10  2004/11/14 08:49:06  bseifert
// Date/Time/Number formatting now uses User-Locale. New option to force old behavior.
//
// Revision 1.9  2004/11/12 22:14:16  bseifert
// Eliminated CLR_NONE. Minor corrections.
//
// Revision 1.8  2004/11/10 01:03:00  assarbad
// - Style cleaning of the alternative coloring code for compressed/encrypted items
//
// Revision 1.7  2004/11/08 00:46:26  assarbad
// - Added feature to distinguish compressed and encrypted files/folders by color as in the Windows 2000/XP explorer.
//   Same rules apply. (Green = encrypted / Blue = compressed)
//
// Revision 1.6  2004/11/05 16:53:08  assarbad
// Added Date and History tag where appropriate.
//
