// WinDirStat.h - Main header for the windirstat application
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
// Copyright (C) 2010 Chris Wimmer
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

#pragma once

#include <Windows.h>
#include "resource.h"
#include "MyImageList.h"
#include "MountPoints.h"
#include "HelpMap.h"
#include <common/Constants.h>
#include <common/Tracer.h>

using CExtensionColorMap = CMap<CStringW, LPCWSTR, COLORREF, COLORREF>; // ".bmp" -> color

class CMainFrame;
class CDirstatApp;

// Frequently used "globals"
CMainFrame* GetMainFrame();
CDirstatApp* GetWDSApp();
CMyImageList* GetMyImageList();

// Other application related globals
CStringW GetAuthorEmail();
CStringW GetWinDirStatHomepage();

#   define WINDIRSTAT_EVENT_NAME_FMT L"WinDirStat_ElevationEvent_{72D223E3-1539-461D-980E-0863FE480E84}.%s.%s"

//
// CDirstatApp. The MFC application object.
// Knows about RAM Usage, Mount points, Help files and the CMyImageList.
//
class CDirstatApp final : public CWinApp
{
    using Inherited = CWinApp;

public:
    CDirstatApp();
    ~CDirstatApp() override;

    BOOL InitInstance() override;
    int ExitInstance() override;

    LANGID GetBuiltInLanguage();
    LANGID GetLangid() const;          // Language as selected in PageGeneral
    LANGID GetEffectiveLangid(); // Language to be used for date/time and number formatting

    void ReReadMountPoints();
    bool IsVolumeMountPoint(const CStringW& path);
    bool IsFolderJunction(DWORD attr);

    COLORREF AltColor() const;           // Coloring of compressed items
    COLORREF AltEncryptionColor() const; // Coloring of encrypted items

    CStringW GetCurrentProcessMemoryInfo();
    CMyImageList* GetMyImageList();
    void UpdateRamUsage();

    void PeriodicalUpdateRamUsage();

    void DoContextHelp(DWORD topic);

    void GetAvailableResourceDllLangids(CArray<LANGID, LANGID>& arr);

    static void RestartApplication();

    static bool getDiskFreeSpace(LPCWSTR pszRootPath, ULONGLONG& total, ULONGLONG& unused);

protected:
    CStringW FindResourceDllPathByLangid(LANGID& langid);
    CStringW FindHelpfilePathByLangid(LANGID langid);
    CStringW FindAuxiliaryFileByLangid(LPCWSTR prefix, LPCWSTR suffix, LANGID& langid, bool checkResource);
    bool ScanResourceDllName(LPCWSTR name, LANGID& langid);
    bool ScanAuxiliaryFileName(LPCWSTR prefix, LPCWSTR suffix, LPCWSTR name, LANGID& langid);
#   ifdef _DEBUG
    void TestScanResourceDllName();
#   endif
    bool IsCorrectResourceDll(LPCWSTR path);

    bool UpdateMemoryInfo();

    // Get the alternative color from Explorer configuration
    COLORREF GetAlternativeColor(COLORREF clrDefault, LPCWSTR which);

    BOOL OnIdle(LONG lCount) override; // This is, where scanning is done.
    static BOOL IsUACEnabled();

    CSingleDocTemplate* m_pDocTemplate; // MFC voodoo.

    LANGID m_langid;                          // Language we are running
    CReparsePoints m_mountPoints;             // Mount point information
    CMyImageList m_myImageList;               // Our central image list
    ULONGLONG m_workingSet;                   // Current working set (RAM usage)
    ULONGLONG m_pageFaults;                   // Page faults so far (unused)
    ULONGLONG m_lastPeriodicalRamUsageUpdate; // Tick count
    COLORREF m_altColor;                      // Coloring of compressed items
    COLORREF m_altEncryptionColor;            // Coloring of encrypted items
    HANDLE m_ElevationEvent;
    CStringW m_ElevationEventName;
#ifdef VTRACE_TO_CONSOLE
    CAutoPtr<CWDSTracerConsole> m_vtrace_console;
#endif // VTRACE_TO_CONSOLE

    DECLARE_MESSAGE_MAP()
    afx_msg void OnFileOpen();
    afx_msg void OnRunElevated();
    afx_msg void OnUpdateRunElevated(CCmdUI* pCmdUI);
    afx_msg void OnHelpManual();
    afx_msg void OnAppAbout();
};
