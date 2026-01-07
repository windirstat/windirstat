// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

#include "pch.h"
#include "IconHandler.h"

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
    BOOL IsIdleMessage(MSG* pMsg) override;

    static bool InPortableMode();
    bool SetPortableMode(bool enable, bool onlyOpen = false);

    bool IsFollowingAllowed(DWORD reparseTag = 0) const;

    COLORREF AltColor() const;           // Coloring of compressed items
    COLORREF AltEncryptionColor() const; // Coloring of encrypted items

    static std::wstring GetCurrentProcessMemoryInfo();
    CIconHandler* GetIconHandler();

    static void LaunchHelp();
    void RestartApplication(bool resetPreferences = false);

    static void LegacyUninstall();
    static std::tuple<ULONGLONG, ULONGLONG> GetFreeDiskSpace(const std::wstring& pszRootPath);
    static CDirStatApp* Get() { return &s_singleton; }
    std::wstring GetSaveToCsvPath() const { return m_saveToCsvPath; }
    std::wstring GetSaveDupesToCsvPath() const { return m_saveDupesToCsvPath; }

protected:

    // Get the alternative color from Explorer configuration
    COLORREF GetAlternativeColor(COLORREF clrDefault, const std::wstring& which) const;

    CSingleDocTemplate* m_pDocTemplate{nullptr}; // MFC voodoo.

    CIconHandler m_iconList;          // Our central icon list
    COLORREF m_altColor;              // Coloring of compressed items
    COLORREF m_altEncryptionColor;    // Coloring of encrypted items
    std::wstring m_saveToCsvPath;     // Path to save csv file to
    std::wstring m_saveDupesToCsvPath; // Path to save duplicates csv file to
    static CDirStatApp s_singleton;    // Singleton application instance
#ifdef _DEBUG
    CWDSTracerConsole m_vtraceConsole;
#endif

    DECLARE_MESSAGE_MAP()
    afx_msg void OnFileOpen();
    afx_msg void OnRunElevated();
    afx_msg void OnFilter();
    afx_msg void OnUpdateRunElevated(CCmdUI* pCmdUI);
    afx_msg void OnHelpManual();
    afx_msg void OnReportBug();
    afx_msg void OnAppAbout();
};
