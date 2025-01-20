// WinDirStat.cpp - Implementation of CDirStatApp and some globals
//
// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
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

#include "stdafx.h"
#include "WinDirStat.h"
#include "MainFrame.h"
#include "SelectDrivesDlg.h"
#include "AboutDlg.h"
#include "DirStatDoc.h"
#include "TreeMapView.h"
#include "GlobalHelpers.h"
#include "Localization.h"
#include "PageFiltering.h"
#include "SmartPointer.h"

CIconHandler* GetIconHandler()
{
    return CDirStatApp::Get()->GetIconHandler();
}

// CDirStatApp

BEGIN_MESSAGE_MAP(CDirStatApp, CWinAppEx)
    ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
    ON_COMMAND(ID_FILE_SELECT, OnFileOpen)
    ON_COMMAND(ID_FILTER, OnFilter)
    ON_COMMAND(ID_RUN_ELEVATED, OnRunElevated)
    ON_UPDATE_COMMAND_UI(ID_RUN_ELEVATED, OnUpdateRunElevated)
    ON_COMMAND(ID_HELP_MANUAL, OnHelpManual)
    ON_COMMAND(ID_HELP_REPORTBUG, OnReportBug)
END_MESSAGE_MAP()

CDirStatApp CDirStatApp::_singleton;

CDirStatApp::CDirStatApp()
{
#ifdef VTRACE_TO_CONSOLE
    m_VtraceConsole.Attach(new CWDSTracerConsole);
#endif

    m_AltColor = GetAlternativeColor(RGB(0x00, 0x00, 0xFF), L"AltColor");
    m_AltEncryptionColor = GetAlternativeColor(RGB(0x00, 0x80, 0x00), L"AltEncryptionColor");
}

CIconHandler* CDirStatApp::GetIconHandler()
{
    m_IconList.Initialize();
    return &m_IconList;
}

void CDirStatApp::RestartApplication(bool resetPreferences)
{
    // Clear preferences if requested
    if (resetPreferences)
    {
        // Cleanup registry preferences
        RegDeleteTree(HKEY_CURRENT_USER, L"Software\\WinDirStat");

        // Enable portable mode by creating the file
        if (InPortableMode())
        {
            const std::wstring ini = GetAppFileName(L"ini");
            DeleteFile(ini.c_str());
            SetPortableMode(true);
        }
    }

    // First, try to create the suspended process
    STARTUPINFO si = { .cb = sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (const BOOL success = CreateProcess(GetAppFileName().c_str(), nullptr, nullptr, nullptr, false,
        resetPreferences ? 0 : CREATE_SUSPENDED, nullptr, nullptr, &si, &pi); !success)
    {
        DisplayError(Localization::Format(IDS_CREATEPROCESSsFAILEDs, GetAppFileName(), TranslateError()));
        return;
    }

    // If resetting preference, hard exit to prevent saving settings
    if (resetPreferences)
    {
        ExitProcess(0);
    }

    // We _send_ the WM_CLOSE here to ensure that all COptions-Settings
    // like column widths an so on are saved before the new instance is resumed.
    // This will post a WM_QUIT message.
    (void)CMainFrame::Get()->SendMessage(WM_CLOSE);

    if (const DWORD dw = ::ResumeThread(pi.hThread); dw != 1)
    {
        VTRACE(L"ResumeThread() didn't return 1");
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

std::tuple<ULONGLONG, ULONGLONG> CDirStatApp::GetFreeDiskSpace(const std::wstring & pszRootPath)
{
    ULARGE_INTEGER u64total = {{0, 0}};
    ULARGE_INTEGER u64free = {{0, 0}};

    if (GetDiskFreeSpaceEx(pszRootPath.c_str(), nullptr, &u64total, &u64free) == 0)
    {
        VTRACE(L"GetDiskFreeSpaceEx({}) failed.", pszRootPath.c_str());
    }

    ASSERT(u64free.QuadPart <= u64total.QuadPart);
    return { u64total.QuadPart, u64free.QuadPart };
}

void CDirStatApp::ReReadMountPoints()
{
    m_ReparsePoints.Initialize();
}

bool CDirStatApp::IsFollowingAllowed(const std::wstring& longpath, const DWORD attr) const
{
    // Allow following if not a reparse point, is a reparse point without exclusion controls,
    // or is a reparse point with exclusion controls but are not excluded
    return !CReparsePoints::IsReparsePoint(attr) ||
        !CReparsePoints::IsReparseType(longpath, { IO_REPARSE_TAG_SYMLINK, IO_REPARSE_TAG_MOUNT_POINT }) ||
        !COptions::ExcludeVolumeMountPoints && m_ReparsePoints.IsVolumeMountPoint(longpath, attr) ||
        !COptions::ExcludeJunctions && m_ReparsePoints.IsJunction(longpath, attr) ||
        !COptions::ExcludeSymbolicLinksDirectory && CReparsePoints::IsSymbolicLink(longpath, attr);
}

// Get the alternative colors for compressed and encrypted files/folders.
// This function uses either the value defined in the Explorer configuration
// or the default color values.
COLORREF CDirStatApp::GetAlternativeColor(const COLORREF clrDefault, const std::wstring & which) const
{
    // Open the explorer key
    CRegKey key;
    key.Open(HKEY_CURRENT_USER, wds::strExplorerKey, KEY_READ);

    // Try to read the REG_BINARY value
    COLORREF x;
    DWORD cbValue = sizeof(x);
    if (ERROR_SUCCESS == key.QueryBinaryValue(which.c_str(), &x, &cbValue))
    {
        // Return the read value upon success
        return x;
    }

    // Return the default upon failure
    return clrDefault;
}

COLORREF CDirStatApp::AltColor() const
{
    // Return property value
    return m_AltColor;
}

COLORREF CDirStatApp::AltEncryptionColor() const
{
    // Return property value
    return m_AltEncryptionColor;
}

std::wstring CDirStatApp::GetCurrentProcessMemoryInfo()
{
    // Fetch current working set
    PROCESS_MEMORY_COUNTERS pmc = { .cb = sizeof(pmc) };
    if (!::GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        return wds::strEmpty;
    }

    static std::wstring memformat = L"     " + Localization::Lookup(IDS_RAMUSAGEs);
    return Localization::Format(memformat, FormatBytes(pmc.WorkingSetSize));
}

bool CDirStatApp::InPortableMode()
{
    return GetFileAttributes(GetAppFileName(L"ini").c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool CDirStatApp::SetPortableMode(const bool enable, const bool onlyOpen)
{
    // If portable mode is Enabled, then just ensure the full path is used
    const std::wstring ini = GetAppFileName(L"ini");
    if (ini == m_pszProfileName &&
        enable == InPortableMode())
    {
        return true;
    }

    // Cleanup previous configuration
    if (m_pszRegistryKey != nullptr) free(const_cast<LPVOID>(static_cast<LPCVOID>(m_pszRegistryKey)));
    if (m_pszProfileName != nullptr) free(const_cast<LPVOID>(static_cast<LPCVOID>(m_pszProfileName)));
    m_pszProfileName = nullptr;
    m_pszRegistryKey = nullptr;
    
    if (enable)
    {
        // Enable portable mode by creating the file
        SmartPointer<HANDLE> iniHandle(CloseHandle, CreateFile(ini.c_str(), GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ,
            nullptr, onlyOpen ? OPEN_EXISTING : OPEN_ALWAYS , 0, nullptr));
        if (iniHandle != INVALID_HANDLE_VALUE)
        {
            // Open successful, setup settings to store to file
            m_pszProfileName = _wcsdup(ini.c_str());
            return true;
        }

        // Fallback to registry mode for any failures
        SetRegistryKey(Localization::Lookup(IDS_APP_TITLE).c_str());
        return false;
    }

    // Attempt to remove file succeeded
    if (DeleteFile(ini.c_str()) != 0 || GetLastError() == ERROR_FILE_NOT_FOUND)
    {
        SetRegistryKey(Localization::Lookup(IDS_APP_TITLE).c_str());
        return true;
    }

    // Deletion failed  - go back to ini mode
    m_pszProfileName = _wcsdup(ini.c_str());
    return false;
}

CString AFXGetRegPath(LPCTSTR lpszPostFix, LPCTSTR)
{
    // This overrides an internal MFC function that causes CWinAppEx
    // to malfunction when operated in portable mode
    return CString(L"Software\\WinDirStat\\WinDirStat\\") + lpszPostFix + L"\\";
}

 class CWinDirStatCommandLineInfo final : public CCommandLineInfo
{
public:

    DWORD m_ParentPid = 0;
    std::vector<std::wstring> m_PathsToOpen;

    void ParseParam(const WCHAR* pszParam, BOOL bFlag, BOOL bLast) override
    {
        UNREFERENCED_PARAMETER(bLast);

        // Normalize case for parsing
        std::wstring param{ pszParam };
        MakeLower(param);

        // Handle any non-flags as paths
        if (!bFlag)
        {
            if (!m_strFileName.IsEmpty()) m_strFileName += wds::chrPipe;
            m_strFileName += TrimString(param, wds::chrBackslash).c_str();
            return;
        }

        const std::wstring ParentPidFlag = L"parentpid:";
        if (param.starts_with(ParentPidFlag))
        {
            m_ParentPid = std::stoul(param.substr(ParentPidFlag.size()));
        }
    }
};

BOOL CDirStatApp::InitInstance()
{
    // Prevent state saving
    m_bSaveState = FALSE;

    CWinAppEx::InitInstance();
    InitShellManager();

    // Load default language just to get bootstrapped
    Localization::LoadResource(MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL));

    // Initialize visual controls
    constexpr INITCOMMONCONTROLSEX ctrls = { sizeof(INITCOMMONCONTROLSEX) , ICC_STANDARD_CLASSES };
    (void)InitCommonControlsEx(&ctrls);
    (void)OleInitialize(nullptr);
    (void)AfxOleInit();
    AfxEnableControlContainer();
    (void)AfxInitRichEdit2();

    // Initialize GPI Plus
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // If a local config file is available, use that for settings
    SetPortableMode(true, true);

    COptions::LoadAppSettings();
    LoadStdProfileSettings(0);

    m_PDocTemplate = new CSingleDocTemplate(
        IDR_MAINFRAME,
        RUNTIME_CLASS(CDirStatDoc),
        RUNTIME_CLASS(CMainFrame),
        RUNTIME_CLASS(CTreeMapView));
    if (!m_PDocTemplate)
    {
        return FALSE;
    }
    AddDocTemplate(m_PDocTemplate);

    // Parse command line arguments
    CWinDirStatCommandLineInfo cmdInfo;
    ParseCommandLine(cmdInfo);
    ProcessShellCommand(cmdInfo);

    CMainFrame::Get()->InitialShowWindow();
    m_pMainWnd->UpdateWindow();

    // When called by setup.exe, WinDirStat remained in the
    // background, so force it to the foreground
    m_pMainWnd->BringWindowToTop();
    m_pMainWnd->SetForegroundWindow();

    // Attempt to enable backup / restore privileges if running as admin
    if (COptions::UseBackupRestore && !EnableReadPrivileges())
    {
        VTRACE(L"Failed to enable additional privileges.");
    }

    // If launches with a parent pid flag, close that process
    if (cmdInfo.m_ParentPid != 0)
    {
        if (SmartPointer<HANDLE> handle(CloseHandle, OpenProcess(PROCESS_TERMINATE, FALSE, cmdInfo.m_ParentPid)); handle != nullptr)
        {
            TerminateProcess(handle, 0);
        }
    }

    // Either open the file names or open file selection dialog
    cmdInfo.m_strFileName.IsEmpty() ? OnFileOpen() :
        (void) m_PDocTemplate->OpenDocumentFile(cmdInfo.m_strFileName, true);

    return TRUE;
}

BOOL CDirStatApp::IsIdleMessage(MSG* pMsg)
{
    if (pMsg->message == WM_TIMER) return FALSE;
    return CWinApp::IsIdleMessage(pMsg);
}

void CDirStatApp::OnAppAbout()
{
    StartAboutDialog();
}

void CDirStatApp::OnFileOpen()
{
    CSelectDrivesDlg dlg;
    if (IDOK == dlg.DoModal())
    {
        const std::wstring path = CDirStatDoc::EncodeSelection(dlg.GetSelectedItems());
        m_PDocTemplate->OpenDocumentFile(path.c_str(), true);
    }
}

void CDirStatApp::OnUpdateRunElevated(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!IsAdmin());
}

void CDirStatApp::OnRunElevated()
{
    // For the configuration to launch, include the parent process so we can
    // terminate it once launched from the child process
    const std::wstring sAppName = GetAppFileName();
    const std::wstring launchConfig = std::format(LR"(/ParentPid {} "{}")", GetCurrentProcessId(), CDirStatDoc::GetDocument()->GetPathName().GetString());

    SHELLEXECUTEINFO shellInfo;
    ZeroMemory(&shellInfo, sizeof(shellInfo));
    shellInfo.cbSize = sizeof(shellInfo);
    shellInfo.fMask  = SEE_MASK_DEFAULT;
    shellInfo.lpFile = sAppName.c_str();
    shellInfo.lpVerb = L"runas";
    shellInfo.nShow  = SW_NORMAL;
    shellInfo.lpParameters = launchConfig.c_str();

    if (!::ShellExecuteEx(&shellInfo))
    {
        VTRACE(L"ShellExecuteEx failed to elevate: {:#08X}", GetLastError());
    }
}

void CDirStatApp::OnFilter()
{
    COptionsPropertySheet sheet;
    CPageFiltering filtering;
    sheet.AddPage(&filtering);
    sheet.DoModal();
}

void CDirStatApp::LaunchHelp()
{
    ShellExecute(*AfxGetMainWnd(), L"open", Localization::LookupNeutral(IDS_URL_HELP).c_str(),
        nullptr, nullptr, SW_SHOWNORMAL);
}

void CDirStatApp::OnHelpManual()
{
    LaunchHelp();
}

void CDirStatApp::OnReportBug()
{
    ShellExecute(*AfxGetMainWnd(), L"open", Localization::LookupNeutral(IDS_URL_REPORT_BUG).c_str(),
        nullptr, nullptr, SW_SHOWNORMAL);
}
