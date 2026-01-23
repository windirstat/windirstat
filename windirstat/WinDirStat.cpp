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

#include "pch.h"
#include "SelectDrivesDlg.h"
#include "AboutDlg.h"
#include "TreeMapView.h"
#include "PageFiltering.h"
#include "CsvLoader.h"

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

CDirStatApp CDirStatApp::s_singleton;

CDirStatApp::CDirStatApp()
{
    m_altColor = GetAlternativeColor(RGB(0x3A, 0x99, 0xE8), L"AltColor");
    m_altEncryptionColor = GetAlternativeColor(RGB(0x00, 0x80, 0x00), L"AltEncryptionColor");
}

CIconHandler* CDirStatApp::GetIconHandler()
{
    m_iconList.Initialize();
    return &m_iconList;
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
        DisplayError(Localization::Format(IDS_PROCESS_FAILEDss, GetAppFileName(), TranslateError()));
        return;
    }

    // If resetting preference, hard exit to prevent saving settings
    if (resetPreferences)
    {
        ExitProcess(0);
    }

    // We _send_ the WM_CLOSE here to ensure that all COptions settings
    // like column widths and so on are saved before the new instance is resumed.
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
    ULARGE_INTEGER u64total = {.QuadPart = 0};
    ULARGE_INTEGER u64free = {.QuadPart = 0};

    if (GetDiskFreeSpaceEx(pszRootPath.c_str(), nullptr, &u64total, &u64free) == 0)
    {
        VTRACE(L"GetDiskFreeSpaceEx({}) failed.", pszRootPath.c_str());
    }

    ASSERT(u64free.QuadPart <= u64total.QuadPart);
    return { u64total.QuadPart, u64free.QuadPart };
}

bool CDirStatApp::IsFollowingAllowed(const DWORD reparseTag) const
{
    if (reparseTag == 0) return true;
    return reparseTag == IO_REPARSE_TAG_MOUNT_POINT && !COptions::ExcludeVolumeMountPoints ||
        reparseTag == IO_REPARSE_TAG_SYMLINK && !COptions::ExcludeSymbolicLinksDirectory ||
        reparseTag == IO_REPARSE_TAG_JUNCTION_POINT && !COptions::ExcludeJunctions ||
        (reparseTag != IO_REPARSE_TAG_MOUNT_POINT && reparseTag != IO_REPARSE_TAG_SYMLINK && reparseTag != IO_REPARSE_TAG_JUNCTION_POINT);
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
    return m_altColor;
}

COLORREF CDirStatApp::AltEncryptionColor() const
{
    // Return property value
    return m_altEncryptionColor;
}

std::wstring CDirStatApp::GetCurrentProcessMemoryInfo()
{
    // Fetch current working set
    PROCESS_MEMORY_COUNTERS pmc = { .cb = sizeof(pmc) };
    if (!::GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        return wds::strEmpty;
    }

    return Localization::Format(IDS_RAMUSAGEs, FormatBytes(pmc.WorkingSetSize));
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
        SetRegistryKey(Localization::LookupNeutral(AFX_IDS_APP_TITLE).c_str());
        return false;
    }

    // Attempt to remove file succeeded
    if (DeleteFile(ini.c_str()) != 0 || GetLastError() == ERROR_FILE_NOT_FOUND)
    {
        SetRegistryKey(Localization::LookupNeutral(AFX_IDS_APP_TITLE).c_str());
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
    std::wstring m_pendingFlag;
    const std::wstring saveToCSVFlag = L"savetocsv";
    const std::wstring saveDupesToCSVFlag = L"savedupestocsv";
    const std::wstring loadFromCSVFlag = L"loadfromcsv";
    const std::wstring legacyUninstallFlag = L"legacyuninstall";

public:

    void ParseParam(const WCHAR* pszParam, BOOL bFlag, BOOL bLast) override
    {
        UNREFERENCED_PARAMETER(bLast);

        // Normalize string for parsing
        std::wstring param{ pszParam };
        TrimString(param, wds::chrDoubleQuote);
        TrimString(param, wds::chrBackslash, true);

        // If we have a pending flag, this non-flag param is its value
        if (!m_pendingFlag.empty() && !bFlag)
        {
            if (m_pendingFlag == saveToCSVFlag)
            {
                CDirStatApp::Get()->m_saveToCsvPath = param;
                COptions::ScanForDuplicates = false;
            }
            else if (m_pendingFlag == saveDupesToCSVFlag)
            {
                CDirStatApp::Get()->m_saveDupesToCsvPath = param;
                COptions::ScanForDuplicates = true;
            }
            else if (m_pendingFlag == loadFromCSVFlag)
            {
                CDirStatApp::Get()->m_loadFromCsvPath = param;
            }
            
            m_pendingFlag.clear();
            return;
        }

        // Handle any non-flags as paths
        if (!bFlag)
        {
            for (const auto& paramSpilt : SplitString(param))
            {
                if (!m_strFileName.IsEmpty()) m_strFileName += wds::chrPipe;
                std::error_code ec;
                const std::wstring fullPath = std::filesystem::absolute(paramSpilt + L"\\", ec).wstring();
                if (FolderExists(fullPath)) m_strFileName += fullPath.c_str();
            }
            return;
        }

        // Handle flags
        param = MakeLower(param);
        if (param == saveToCSVFlag || param == saveDupesToCSVFlag || param == loadFromCSVFlag)
        {
            m_pendingFlag = param;
        }
        else if (param == legacyUninstallFlag)
        {
            CDirStatApp::LegacyUninstall();
        }
    }
};

BOOL CDirStatApp::InitInstance()
{
    // Prevent state saving
    m_bSaveState = FALSE;

    // Set process I/O priority to high for better disk scanning performance
    SetProcessIoPriorityHigh();

    // Load default language just to get bootstrapped
    Localization::LoadResource(MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL));

    // If a local config file is available, use that for settings
    SetPortableMode(true, true);

    COptions::LoadAppSettings();
    LoadStdProfileSettings(0);

    // Silently restart elevated conditionally before any expensive initialization
    if (IsElevationAvailable() && COptions::AutoElevate && !COptions::ShowElevationPrompt) // only if user doesn't want to be prompted
    {
        RunElevated(m_lpCmdLine);
    }

    // Set app to prefer dark mode
    DarkMode::SetAppDarkMode();

    CWinAppEx::InitInstance();
    InitShellManager();

    // Initialize visual controls
    constexpr INITCOMMONCONTROLSEX ctrls = { sizeof(INITCOMMONCONTROLSEX) , ICC_STANDARD_CLASSES };
    (void)InitCommonControlsEx(&ctrls);
    (void)OleInitialize(nullptr);
    (void)AfxOleInit();
    AfxEnableControlContainer();
    (void)AfxInitRichEdit2();

    // Initialize GDI Plus
    const Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    m_pDocTemplate = new CSingleDocTemplate(
        IDR_MAINFRAME,
        RUNTIME_CLASS(CDirStatDoc),
        RUNTIME_CLASS(CMainFrame),
        RUNTIME_CLASS(CTreeMapView));
    AddDocTemplate(m_pDocTemplate);

    // Parse command line arguments
    CWinDirStatCommandLineInfo cmdInfo;
    ParseCommandLine(cmdInfo);
    ProcessShellCommand(cmdInfo);

    // Check if we should hide the app window
    const bool hideApp = !m_saveToCsvPath.empty() || !m_saveDupesToCsvPath.empty();
    if (hideApp) m_nCmdShow = SW_HIDE;

    CMainFrame::Get()->InitialShowWindow();
    m_pMainWnd->Invalidate();
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

    // Enable reading of reparse data for cloud links
    CHAR(WINAPI * RtlSetProcessPlaceholderCompatibilityMode) (CHAR Mode) =
        reinterpret_cast<decltype(RtlSetProcessPlaceholderCompatibilityMode)>(
            static_cast<LPVOID>(GetProcAddress(GetModuleHandle(L"ntdll.dll"), "RtlSetProcessPlaceholderCompatibilityMode")));
    if (RtlSetProcessPlaceholderCompatibilityMode != nullptr)
    {
        constexpr CHAR PHCM_EXPOSE_PLACEHOLDERS = 2;
        RtlSetProcessPlaceholderCompatibilityMode(PHCM_EXPOSE_PLACEHOLDERS);
    }

    // Allow user to elevate if desired
    if (IsElevationAvailable() && COptions::ShowElevationPrompt && !hideApp)
    {
        CMessageBoxDlg elevationPrompt(Localization::Lookup(IDS_ELEVATION_QUESTION),
            Localization::LookupNeutral(AFX_IDS_APP_TITLE), MB_YESNO | MB_ICONQUESTION, m_pMainWnd, {},
            Localization::Lookup(IDS_DONT_SHOW_AGAIN), false);

        const INT_PTR result = elevationPrompt.DoModal();
        COptions::ShowElevationPrompt = !elevationPrompt.IsCheckboxChecked();
        if (result == IDYES)
        {
            if (!COptions::ShowElevationPrompt) COptions::AutoElevate = true;
            RunElevated(m_lpCmdLine);
            return FALSE;
        }
        else COptions::AutoElevate = false;
    }

    // Load from CSV if specified via command line
    if (!m_loadFromCsvPath.empty())
    {
        if (CItem* newroot = LoadResults(m_loadFromCsvPath); newroot != nullptr)
        {
            CDirStatDoc::Get()->OnOpenDocument(newroot);
        }
        return TRUE;
    }

    // Either open the file names or open file selection dialog
    cmdInfo.m_strFileName.IsEmpty() ? OnFileOpen() :
        (void)m_pDocTemplate->OpenDocumentFile(cmdInfo.m_strFileName, true);

    return TRUE;
}

BOOL CDirStatApp::IsIdleMessage(MSG* pMsg)
{
    // Treat WM_TIMER as an idle message to prevent excessive OnIdle calls
    // The timer is used for UI updates and should not trigger idle processing
    if (pMsg->message == WM_TIMER) return FALSE;
    if (pMsg->message == WM_MOUSEMOVE) return FALSE;
    return CWinAppEx::IsIdleMessage(pMsg);
}

void CDirStatApp::OnAppAbout()
{
    StartAboutDialog();
}

void CDirStatApp::OnFileOpen()
{
    CopyAllDriveMappings();

    if (CSelectDrivesDlg dlg; IDOK == dlg.DoModal())
    {
        const std::wstring path = JoinString(dlg.GetSelectedItems());
        m_pDocTemplate->OpenDocumentFile(path.c_str(), true);
    }
}

void CDirStatApp::OnUpdateRunElevated(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!IsElevationActive());
}

void CDirStatApp::OnRunElevated()
{
    RunElevated(CDirStatDoc::Get()->GetPathName().GetString());
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

void CDirStatApp::LegacyUninstall()
{
    namespace fs = std::filesystem;
    std::error_code ec;

    // Kill WinDirStat processes based on executable name
    if (SmartPointer<HANDLE> snap(CloseHandle, CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)); snap.IsValid())
    {
        const std::wstring exeName = wds::strWinDirStat;
        PROCESSENTRY32W pe{ .dwSize = sizeof(pe) };
        for (BOOL hasProcess = Process32First(snap, &pe); hasProcess; hasProcess = Process32NextW(snap, &pe))
        {
            if (_wcsnicmp(pe.szExeFile, exeName.c_str(), exeName.size()) != 0 ||
                pe.th32ProcessID == GetCurrentProcessId()) continue;

            SmartPointer<HANDLE> h(CloseHandle, OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID));
            if (h.IsValid()) TerminateProcess(h, 0);
        }
    }

    // Collect all registry keys from HKLM and HKU
    struct RegInfo { HKEY rootKey; std::wstring subKey; };
    std::vector<RegInfo> regKeys;

    // Add HKLM key
    regKeys.push_back({ HKEY_LOCAL_MACHINE, wds::strUninstall });

    // Add HKU keys for all users
    if (CRegKey key; key.Open(HKEY_USERS, nullptr, KEY_ENUMERATE_SUB_KEYS) == ERROR_SUCCESS)
    {
        std::array<WCHAR, SECURITY_MAX_SID_STRING_CHARACTERS> sidName;
        for (DWORD sidSize = static_cast<DWORD>(sidName.size()), i = 0;
            key.EnumKey(i, sidName.data(), &sidSize) == ERROR_SUCCESS;
            i++, sidSize = static_cast<DWORD>(sidName.size()))
        {
            regKeys.push_back({ HKEY_USERS, std::wstring(sidName.data()) + L"\\" + wds::strUninstall });
        }
    }

    // Process all registry keys - query InstallLocation, clean up files, and delete registry keys
    for (const auto& regInfo : regKeys)
    {
        CRegKey key;
        if (key.Open(regInfo.rootKey, regInfo.subKey.c_str(), KEY_READ) != ERROR_SUCCESS) continue;

        // Query InstallLocation
        std::array<WCHAR, MAX_PATH + 1> installPath;
        ULONG size = static_cast<ULONG>(installPath.size());
        if (key.QueryStringValue(L"InstallLocation", installPath.data(), &size) == ERROR_SUCCESS)
        {
            // Clean up installation directory
            if (fs::path dir(installPath.data()); fs::exists(dir))
            {
                for (auto& file : fs::directory_iterator(dir, ec))
                {
                    auto fname = MakeLower(file.path().filename().wstring());
                    if (fname.starts_with(L"wdsh") || fname.starts_with(L"wdsr") ||
                        fname.starts_with(L"windirstat") || fname == L"uninstall.exe")
                    {
                        fs::remove(file, ec);
                    }
                }
                if (fs::is_empty(dir, ec)) fs::remove(dir, ec);
            }
        }

        // Delete registry key
        SHDeleteKey(regInfo.rootKey, (regInfo.subKey + L"\\WinDirStat").c_str());
    }

    // Remove shortcuts and start menu items for all users
    constexpr auto startMenuLocation = L"Microsoft\\Windows\\Start Menu\\Programs\\WinDirStat";
    SmartPointer<PWSTR> usersPath(CoTaskMemFree, nullptr);
    if (SHGetKnownFolderPath(FOLDERID_UserProfiles, 0, nullptr, &usersPath) != S_OK) return;
    if (fs::path usersDir(static_cast<LPWSTR>(usersPath)); fs::exists(usersDir, ec))
    {
        for (auto& userDir : fs::directory_iterator(usersDir, ec))
        {
            if (!userDir.is_directory()) continue;

            fs::remove(userDir.path() / L"Desktop\\WinDirStat.lnk", ec);
            fs::remove_all(userDir.path() / L"AppData\\Roaming" / startMenuLocation, ec);
        }
    }

    // Remove ProgramData start menu items
    std::array<WCHAR, MAX_PATH + 1> programData;
    if (SHGetFolderPath(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, programData.data()) != S_OK) return;
    fs::remove_all(fs::path(programData.data()) / startMenuLocation, ec);
    ExitProcess(0);
}
