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
#include "langs.h"
#include "MyImageList.h"
#include "MountPoints.h"
#include "HelpMap.h"
#include <common/Constants.h>
#include <common/Tracer.h>

using CExtensionColorMap = CMap<CStringW, LPCWSTR, COLORREF, COLORREF>; // ".bmp" -> color

class CMainFrame;
class CDirStatApp;

// Frequently used "globals"
CMainFrame* GetMainFrame();
CDirStatApp* GetWDSApp();
CMyImageList* GetMyImageList();

// Other application related globals
CStringW GetAuthorEmail();
CStringW GetWinDirStatHomepage();

//
// CDirStatApp. The MFC application object.
// Knows about RAM Usage, Mount points, Help files and the CMyImageList.
//
class CDirStatApp final : public CWinApp
{
    using Inherited = CWinApp;

public:
    CDirStatApp();

    BOOL InitInstance() override;
    int ExitInstance() override;

    bool InPortableMode() const;
    bool SetPortableMode(bool enable, bool only_open = false);

    void ReReadMountPoints();
    bool IsVolumeMountPoint(const CStringW& path);
    bool IsFolderJunction(DWORD attr);

    COLORREF AltColor() const;           // Coloring of compressed items
    COLORREF AltEncryptionColor() const; // Coloring of encrypted items

    static CStringW GetCurrentProcessMemoryInfo();
    CMyImageList* GetMyImageList();

    void DoContextHelp(DWORD topic);

    static void RestartApplication();

    static bool getDiskFreeSpace(LPCWSTR pszRootPath, ULONGLONG& total, ULONGLONG& unused);

    // Language resource processing
    static CStringW FindHelpfilePathByLangid(LANGID langid);

protected:

    // Get the alternative color from Explorer configuration
    COLORREF GetAlternativeColor(COLORREF clrDefault, LPCWSTR which);

    CSingleDocTemplate* m_pDocTemplate; // MFC voodoo.

    LANGID m_langid;                          // Language we are running
    CReparsePoints m_mountPoints;             // Mount point information
    CMyImageList m_myImageList;               // Our central image list
    COLORREF m_altColor;                      // Coloring of compressed items
    COLORREF m_altEncryptionColor;            // Coloring of encrypted items
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
