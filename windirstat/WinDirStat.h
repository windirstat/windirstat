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

#include "stdafx.h"
#include "resource.h"
#include "Langs.h"
#include "IconHandler.h"
#include "MountPoints.h"
#include "Constants.h"
#include "Tracer.h"

class CMainFrame;
class CDirStatApp;

// Frequently used "globals"
CIconHandler* GetIconHandler();

//
// CDirStatApp. The MFC application object.
// Knows about RAM Usage, Mount points, Help files and the CIconHandler.
//
class CDirStatApp final : public CWinAppEx
{
public:

    CDirStatApp();
    BOOL InitInstance() override;
    BOOL LoadState(LPCTSTR, CFrameImpl*) override { return TRUE; }

    static bool InPortableMode();
    bool SetPortableMode(bool enable, bool onlyOpen = false);

    void ReReadMountPoints();
    bool IsFollowingAllowed(const std::wstring& longpath, DWORD attr = 1) const;
    CReparsePoints* GetReparseInfo() { return &m_ReparsePoints; }

    COLORREF AltColor() const;           // Coloring of compressed items
    COLORREF AltEncryptionColor() const; // Coloring of encrypted items

    static std::wstring GetCurrentProcessMemoryInfo();
    CIconHandler* GetIconHandler();

    static void LaunchHelp();
    void RestartApplication(bool resetPreferences = false);

    static std::tuple<ULONGLONG, ULONGLONG> GetFreeDiskSpace(const std::wstring& pszRootPath);
    static CDirStatApp* Get() { return &_singleton; }

protected:

    // Get the alternative color from Explorer configuration
    COLORREF GetAlternativeColor(COLORREF clrDefault, const std::wstring& which) const;

    CSingleDocTemplate* m_PDocTemplate{nullptr}; // MFC voodoo.

    CReparsePoints m_ReparsePoints;   // Mount point information
    CIconHandler m_IconList;          // Our central icon list
    COLORREF m_AltColor;              // Coloring of compressed items
    COLORREF m_AltEncryptionColor;    // Coloring of encrypted items
    static CDirStatApp _singleton;    // Singleton application instance
#ifdef VTRACE_TO_CONSOLE
    CAutoPtr<CWDSTracerConsole> m_VtraceConsole;
#endif // VTRACE_TO_CONSOLE

    DECLARE_MESSAGE_MAP()
    afx_msg void OnFileOpen();
    afx_msg void OnRunElevated();
    afx_msg void OnFilter();
    afx_msg void OnUpdateRunElevated(CCmdUI* pCmdUI);
    afx_msg void OnHelpManual();
    afx_msg void OnReportBug();
    afx_msg void OnAppAbout();
};
