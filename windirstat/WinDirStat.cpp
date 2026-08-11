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
#include "CsvLoader.h"

CIconHandler* GetIconHandler()
{
    return CDirStatApp::Get()->GetIconHandler();
}

// CDirStatApp

CDirStatApp CDirStatApp::s_singleton;

int APIENTRY wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
    InitCommonControls();
    CDirStatApp* app = CDirStatApp::Get();
    app->m_lpCmdLine = lpCmdLine;
    app->m_nCmdShow = nShowCmd;
    if (app->InitInstance()) return app->Run();
    if (app->m_pMainWnd != nullptr) app->m_pMainWnd->DestroyWindow();
    return app->ExitInstance();
}

CDirStatApp::CDirStatApp()
{
    m_altColor = GetAlternativeColor(RGB(0x3A, 0x99, 0xE8), L"AltColor");
    m_altEncryptionColor = GetAlternativeColor(RGB(0x00, 0x80, 0x00), L"AltEncryptionColor");
}

CDirStatApp::~CDirStatApp() = default;

CIconHandler* CDirStatApp::GetIconHandler()
{
    m_iconList.Initialize();
    return &m_iconList;
}

void CDirStatApp::RestartApplication(const bool resetPreferences)
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
    if (const bool success = CreateProcess(GetAppFileName().c_str(), nullptr, nullptr, nullptr, false,
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

    if (const DWORD dw = ResumeThread(pi.hThread); dw != 1)
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

    assert(u64free.QuadPart <= u64total.QuadPart);
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
    const std::wstring ini = GetAppFileName(L"ini");
    if (enable)
    {
        // Enable portable mode by creating the file
        const SmartPointer iniHandle(CloseHandle, CreateFile(ini.c_str(), GENERIC_WRITE | GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, onlyOpen ? OPEN_EXISTING : OPEN_ALWAYS , 0, nullptr));
        if (iniHandle != INVALID_HANDLE_VALUE)
        {
            // Open successful, setup settings to store to file
            PersistedSetting::UseIniStorage(ini);
            return true;
        }

        // Fallback to registry mode for any failures
        PersistedSetting::UseRegistryStorage();
        return false;
    }

    // Attempt to remove file succeeded
    if (DeleteFile(ini.c_str()) != 0 || GetLastError() == ERROR_FILE_NOT_FOUND)
    {
        PersistedSetting::UseRegistryStorage();
        return true;
    }

    // Deletion failed  - go back to ini mode
    PersistedSetting::UseIniStorage(ini);
    return false;
}

class CWinDirStatCommandLineInfo final
{
    std::wstring m_pendingFlag;
    std::wstring m_operationFlag;
    bool m_hasParsedParam = false;
    bool m_hasPathParam = false;
    bool m_malformedFlag = false;
    bool m_invalidPath = false;
    static constexpr std::wstring_view saveToFlag = L"saveto";
    static constexpr std::wstring_view saveDupesToFlag = L"savedupesto";
    static constexpr std::wstring_view savePermsToFlag = L"savepermsto";
    static constexpr std::wstring_view loadFromFlag = L"loadfrom";
    static constexpr std::wstring_view legacyUninstallFlag = L"legacyuninstall";
    std::wstring m_path;

public:
    CWinDirStatCommandLineInfo()
    {
        int argc = 0;
        const std::unique_ptr<wchar_t*, decltype(&LocalFree)> argv(
            CommandLineToArgvW(GetCommandLineW(), &argc), LocalFree);
        if (argv == nullptr) return;

        for (int i = 1; i < argc; ++i)
        {
            const wchar_t* param = argv.get()[i];
            const bool flag = param[0] == L'-' || param[0] == L'/';
            if (flag) ++param;
            ParseParam(param, flag, i == argc - 1);
        }
    }

    bool HasMalformedCommandLine() const noexcept
    {
        return m_malformedFlag || !m_pendingFlag.empty() ||
            (m_operationFlag == loadFromFlag && m_hasPathParam);
    }
    bool HasInvalidPath() const noexcept { return m_invalidPath; }
    bool IsLegacyUninstallRequested() const noexcept { return m_operationFlag == legacyUninstallFlag; }
    const std::wstring& GetPath() const noexcept { return m_path; }

private:
    void ParseParam(const WCHAR* pszParam, const bool bFlag, const bool bLast)
    {
        const bool hadPriorParam = m_hasParsedParam;
        m_hasParsedParam = true;

        // Normalize string for parsing
        std::wstring param{ pszParam };
        TrimString(param, wds::chrDoubleQuote);
        TrimString(param, wds::chrBackslash, true);

        // If we have a pending flag, this non-flag param is its value
        if (!m_pendingFlag.empty() && !bFlag)
        {
            if (param.empty())
            {
                m_malformedFlag = true;
            }
            else if (m_pendingFlag == saveToFlag)
            {
                CDirStatApp::Get()->m_saveToPath = param;
                COptions::ScanForDuplicates = false;
            }
            else if (m_pendingFlag == saveDupesToFlag)
            {
                CDirStatApp::Get()->m_saveDupesToPath = param;
                COptions::ScanForDuplicates = true;
            }
            else if (m_pendingFlag == savePermsToFlag)
            {
                CDirStatApp::Get()->m_savePermsToPath = param;
            }
            else if (m_pendingFlag == loadFromFlag)
            {
                CDirStatApp::Get()->m_loadFromPath = param;
            }

            m_pendingFlag.clear();
            return;
        }

        // Handle any non-flags as paths
        if (!bFlag)
        {
            m_hasPathParam = true;
            if (param.empty())
            {
                m_invalidPath = true;
                m_malformedFlag = true;
                return;
            }
            for (const auto& paramSpilt : SplitString(param))
            {
                if (paramSpilt.empty())
                {
                    m_invalidPath = true;
                    continue;
                }
                std::error_code ec;
                const std::wstring fullPath = std::filesystem::absolute(paramSpilt + L"\\", ec).wstring();
                if (!ec && FolderExists(fullPath))
                {
                    if (!m_path.empty()) m_path += wds::chrPipe;
                    m_path += fullPath;
                }
                else
                {
                    m_invalidPath = true;
                }
            }
            return;
        }

        // Handle flags
        // Value-taking flags require an immediate non-flag value.
        if (!m_pendingFlag.empty())
        {
            m_malformedFlag = true;
            return;
        }
        param = MakeLower(param);
        if (param == saveToFlag || param == saveDupesToFlag || param == savePermsToFlag || param == loadFromFlag)
        {
            if (!m_operationFlag.empty()) m_malformedFlag = true;
            else m_operationFlag = param;
            m_pendingFlag = param;
            if (bLast) m_malformedFlag = true;
        }
        else if (param == legacyUninstallFlag)
        {
            // Defer this destructive standalone action until parsing is validated.
            if (hadPriorParam || !bLast || !m_operationFlag.empty()) m_malformedFlag = true;
            else
            {
                m_operationFlag = param;
            }
        }
    }
};

bool CDirStatApp::InitInstance()
{
    // Restrict DLL search to System32 — prevents DLL hijacking from CWD or PATH
    if (const auto pSetDefaultDllDirectories = reinterpret_cast<decltype(&SetDefaultDllDirectories)>(
        GetProcAddress(GetModuleHandle(L"kernel32.dll"), "SetDefaultDllDirectories"));
        pSetDefaultDllDirectories && IsWindows8OrGreater())
    {
        pSetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    }

    // Load default language just to get bootstrapped
    Localization::LoadResource(MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL));

    // If a local config file is available, use that for settings
    SetPortableMode(true, true);

    COptions::LoadAppSettings();
    SetProcessPriority(COptions::ProcessPriority);

    // Silently restart elevated conditionally before any expensive initialization
    if (IsElevationAvailable() && COptions::AutoElevate && !COptions::ShowElevationPrompt) // only if user doesn't want to be prompted
    {
        RunElevated(m_lpCmdLine);
    }

    // Set app to prefer dark mode
    DarkMode::SetAppDarkMode();
    CWinApp::InitInstance();

    // Initialize visual controls
    constexpr INITCOMMONCONTROLSEX ctrls = { sizeof(INITCOMMONCONTROLSEX) , ICC_STANDARD_CLASSES };
    (void)CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    (void)InitCommonControlsEx(&ctrls);

    // Initialize GDI Plus
    const Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    const CWinDirStatCommandLineInfo cmdInfo;
    if (cmdInfo.HasMalformedCommandLine()) ExitProcess(1);

    if (cmdInfo.IsLegacyUninstallRequested())
    {
        LegacyUninstall();
        // Terminate even if cleanup returned early.
        ExitProcess(0);
    }

    // Check if we should hide the app window
    const bool hideApp = !m_saveToPath.empty() || !m_saveDupesToPath.empty() || !m_savePermsToPath.empty();
    if (hideApp && (cmdInfo.GetPath().empty() || cmdInfo.HasInvalidPath())) ExitProcess(1);
    if (hideApp) m_nCmdShow = SW_HIDE;

    m_model = std::make_unique<CWinDirStatModel>();

    m_pMainWnd = new CMainFrame;
    if (!static_cast<CMainFrame*>(m_pMainWnd)->CreateFromResource(IDR_MAINFRAME))
    {
        m_pMainWnd = nullptr;
        return false;
    }

    CWinDirStatModel::Get()->ResetScan();
    CMainFrame::Get()->InitialShowWindow();
    m_pMainWnd->ShowWindow(m_nCmdShow);
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
        if (const auto [nID, isChecked] =
            CMessageBoxDlg::Show(Localization::Lookup(IDS_ELEVATION_QUESTION), Localization::Lookup(IDS_DONT_SHOW_AGAIN),
                false, MB_YESNO | MB_ICONQUESTION, m_pMainWnd);
            (COptions::ShowElevationPrompt = !isChecked, nID == IDYES))
        {
            if (isChecked) COptions::AutoElevate = true;
            RunElevated(m_lpCmdLine);
            return false;
        }
    }

    // Load results if specified via command line
    if (!m_loadFromPath.empty())
    {
        if (CItem* newroot = LoadResults(m_loadFromPath); newroot != nullptr)
        {
            CWinDirStatModel::Get()->OpenLoadedScan(newroot);
        }
        return true;
    }

    // Reject unsupported quiet roots instead of leaving a hidden process idle.
    if (cmdInfo.GetPath().empty())
    {
        OnSelectScanRoots();
    }
    else if (!CWinDirStatModel::Get()->StartScan(cmdInfo.GetPath()))
    {
        if (hideApp) ExitProcess(1);
        OnSelectScanRoots();
    }

    return true;
}

bool CDirStatApp::IsIdleMessage(MSG* pMsg)
{
    // Treat WM_TIMER as an idle message to prevent excessive OnIdle calls
    // The timer is used for UI updates and should not trigger idle processing
    if (pMsg->message == WM_TIMER) return false;
    if (pMsg->message == WM_MOUSEMOVE || pMsg->message == WM_NCMOUSEMOVE) return false;
    return CWinApp::IsIdleMessage(pMsg);
}

void CDirStatApp::OnAppAbout()
{
    const auto dlg = std::make_unique<CAboutDlg>();
    dlg->ShowModal();
}

void CDirStatApp::OnSelectScanRoots()
{
    CopyAllDriveMappings();

    if (const auto dlg = std::make_unique<CSelectDrivesDlg>(); IDOK == dlg->ShowModal())
    {
        const std::wstring path = JoinString(dlg->GetSelectedItems());
        CWinDirStatModel::Get()->StartScan(path);
    }
}

void CDirStatApp::OnUpdateRunElevated(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!IsElevationActive());
}

void CDirStatApp::OnRunElevated()
{
    RunElevated(CWinDirStatModel::Get()->GetScanPathSpec());
}

void CDirStatApp::OnFilter()
{
    CSettingsSheet::ShowSettings(1); // 1 = Filtering tab
}

void CDirStatApp::LaunchHelp()
{
    ShellExecute(GetMainWindowHandle(), L"open", Localization::LookupNeutral(IDS_URL_HELP).c_str(),
        nullptr, nullptr, SW_SHOWNORMAL);
}

void CDirStatApp::OnHelpManual()
{
    LaunchHelp();
}

void CDirStatApp::OnReportBug()
{
    ShellExecute(GetMainWindowHandle(), L"open", Localization::LookupNeutral(IDS_URL_REPORT_BUG).c_str(),
        nullptr, nullptr, SW_SHOWNORMAL);
}

void CDirStatApp::LegacyUninstall()
{
    namespace fs = std::filesystem;
    std::error_code ec;

    // Kill WinDirStat processes based on executable name
    if (const SmartPointer snap(CloseHandle, CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)); snap.IsValid())
    {
        const std::wstring exeName = wds::strWinDirStat;
        PROCESSENTRY32W pe{ .dwSize = sizeof(pe) };
        for (bool hasProcess = Process32First(snap, &pe); hasProcess; hasProcess = Process32Next(snap, &pe))
        {
            if (_wcsnicmp(pe.szExeFile, exeName.c_str(), exeName.size()) != 0 ||
                pe.th32ProcessID == GetCurrentProcessId()) continue;

            SmartPointer h(CloseHandle, OpenProcess(PROCESS_TERMINATE, false, pe.th32ProcessID));
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
        std::array<WCHAR, MAX_PATH> installPath;
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
        SHDeleteKey(regInfo.rootKey, regInfo.subKey.c_str());
    }

    // Remove shortcuts and start menu items for all users
    constexpr auto startMenuLocation = L"Microsoft\\Windows\\Start Menu\\Programs\\WinDirStat";
    SmartPointer usersPath(CoTaskMemFree, static_cast<PWSTR>(nullptr));
    if (SHGetKnownFolderPath(FOLDERID_UserProfiles, 0, nullptr, &usersPath) != S_OK) return;
    if (const fs::path usersDir(static_cast<LPWSTR>(usersPath)); fs::exists(usersDir, ec))
    {
        for (auto& userDir : fs::directory_iterator(usersDir, ec))
        {
            if (!userDir.is_directory()) continue;

            fs::remove(userDir.path() / L"Desktop\\WinDirStat.lnk", ec);
            fs::remove_all(userDir.path() / L"AppData\\Roaming" / startMenuLocation, ec);
        }
    }

    // Remove ProgramData start menu items
    std::array<WCHAR, MAX_PATH> programData;
    if (SHGetFolderPath(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, programData.data()) != S_OK) return;
    fs::remove_all(fs::path(programData.data()) / startMenuLocation, ec);
    ExitProcess(0);
}
