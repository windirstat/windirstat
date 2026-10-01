// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "IconHandler.h"

class CMainFrame;
class CWinDirStatModel;
class CDirStatApp;

// Frequently used "globals"
CIconHandler* GetIconHandler();

//
// CDirStatApp. The application object.
// Knows about RAM Usage, Mount points, Help files and the CIconHandler.
//
class CDirStatApp final : public MessageTarget<CDirStatApp, CWinApp>
{
    friend class CWinDirStatCommandLineInfo;

public:

    CDirStatApp();
    ~CDirStatApp() override;
    bool InitInstance() override;
    bool IsIdleMessage(const MSG* pMsg) const noexcept override;

    static bool InPortableMode();
    DWORD SetPortableMode(bool enable, bool onlyOpen = false);

    bool IsFollowingAllowed(DWORD reparseTag = 0) const;

    COLORREF AltColor() const { return m_altColor; } // Coloring of compressed items
    COLORREF AltEncryptionColor() const { return m_altEncryptionColor; } // Coloring of encrypted items

    static std::wstring GetCurrentProcessMemoryInfo();
    CIconHandler* GetIconHandler();

    static void LaunchHelp();
    void RestartApplication(bool resetPreferences = false);

    static void LegacyUninstall();
    static std::tuple<ULONGLONG, ULONGLONG> GetFreeDiskSpace(const std::wstring& pszRootPath);
    static CDirStatApp* Get() { return &s_singleton; }
    std::wstring GetSaveToPath() const { return m_saveToPath; }
    std::wstring GetSaveDupesToPath() const { return m_saveDupesToPath; }
    std::wstring GetSavePermsToPath() const { return m_savePermsToPath; }

protected:

    // Get the alternative color from Explorer configuration
    COLORREF GetAlternativeColor(COLORREF clrDefault, const std::wstring& which) const;

    std::unique_ptr<CWinDirStatModel> m_model;
    std::unique_ptr<CMainFrame> m_mainFrame;

    CIconHandler m_iconList;        // Central icon list
    COLORREF m_altColor;            // Coloring of compressed items
    COLORREF m_altEncryptionColor;  // Coloring of encrypted items
    std::wstring m_loadFromPath;    // Path to load results from
    std::wstring m_saveToPath;      // Path to save results to
    std::wstring m_saveDupesToPath; // Path to save duplicates to
    std::wstring m_savePermsToPath; // Path to save permissions to
    static CDirStatApp s_singleton; // Singleton application instance

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnSelectScanRoots();
    void OnRunElevated();
    void OnFilter();
    void OnUpdateRunElevated(CCmdUI* pCmdUI);
    void OnHelpManual();
    void OnReportBug();
    void OnAppAbout();
};

inline std::span<const RouteEntry> CDirStatApp::Routes()
{
    static constexpr std::array entries
    {
        Route::Command<&OnAppAbout>(ID_APP_ABOUT),
        Route::Command<&OnSelectScanRoots>(ID_FILE_SELECT),
        Route::Command<&OnFilter>(ID_FILTER),
        Route::Command<&OnRunElevated>(ID_RUN_ELEVATED),
        Route::Update<&OnUpdateRunElevated>(ID_RUN_ELEVATED),
        Route::Command<&OnHelpManual>(ID_HELP_MANUAL),
        Route::Command<&OnReportBug>(ID_HELP_REPORTBUG),
    };
    return entries;
}
