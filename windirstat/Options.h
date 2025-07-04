// Options.h - Declaration of CRegistryUser, COptions and COptions
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

#pragma once

#include "TreeMap.h"
#include "Property.h"

#include <regex>

class COptions;

constexpr auto USERDEFINEDCLEANUPCOUNT = 10;
constexpr auto TREELISTCOLORCOUNT = 8;

enum REFRESHPOLICY : std::uint8_t
{
    RP_NO_REFRESH,
    RP_REFRESH_THIS_ENTRY,
    RP_REFRESH_THIS_ENTRYS_PARENT
};

struct USERDEFINEDCLEANUP
{
    USERDEFINEDCLEANUP() : USERDEFINEDCLEANUP(L"") {}
    USERDEFINEDCLEANUP(const std::wstring & sEntry) :
        Title(Setting<std::wstring>(sEntry, L"Title", L"")),
        CommandLine(Setting<std::wstring>(sEntry, L"CommandLine", L"")),
        Enabled(Setting<bool>(sEntry, L"Enable", false)),
        VirginTitle(Setting<bool>(sEntry, L"VirginTitle", true)),
        WorksForDrives(Setting<bool>(sEntry, L"WorksForDrives", false)),
        WorksForDirectories(Setting<bool>(sEntry, L"WorksForDirectories", false)),
        WorksForFiles(Setting<bool>(sEntry, L"WorksForFiles", false)),
        WorksForUncPaths(Setting<bool>(sEntry, L"WorksForUncPaths", false)),
        RecurseIntoSubdirectories(Setting<bool>(sEntry, L"RecurseIntoSubdirectories", false)),
        AskForConfirmation(Setting<bool>(sEntry, L"AskForConfirmation", false)),
        ShowConsoleWindow(Setting<bool>(sEntry, L"ShowConsoleWindow", false)),
        WaitForCompletion(Setting<bool>(sEntry, L"WaitForCompletion", false)),
        RefreshPolicy(Setting<int>(sEntry, L"RefreshPolicy", 0)) {}

    Setting<std::wstring> Title;
    Setting<std::wstring> CommandLine;
    Setting<bool> Enabled;
    Setting<bool> VirginTitle;
    Setting<bool> WorksForDrives;
    Setting<bool> WorksForDirectories;
    Setting<bool> WorksForFiles;
    Setting<bool> WorksForUncPaths;
    Setting<bool> RecurseIntoSubdirectories;
    Setting<bool> AskForConfirmation;
    Setting<bool> ShowConsoleWindow;
    Setting<bool> WaitForCompletion;
    Setting<int> RefreshPolicy;

    // This will not transfer the property persistent settings but allows
    // this to be used as a generalized structure in the cleanup page
    USERDEFINEDCLEANUP(const USERDEFINEDCLEANUP& other) { *this = other; }
    USERDEFINEDCLEANUP& operator=(const USERDEFINEDCLEANUP& other)
    {
        Title = other.Title.Obj();
        CommandLine = other.CommandLine.Obj();
        Enabled = other.Enabled.Obj();
        VirginTitle = other.VirginTitle.Obj();
        WorksForDrives = other.WorksForDrives.Obj();
        WorksForDirectories = other.WorksForDirectories.Obj();
        WorksForFiles = other.WorksForFiles.Obj();
        WorksForUncPaths = other.WorksForUncPaths.Obj();
        RecurseIntoSubdirectories = other.RecurseIntoSubdirectories.Obj();
        AskForConfirmation = other.AskForConfirmation.Obj();
        ShowConsoleWindow = other.ShowConsoleWindow.Obj();
        WaitForCompletion = other.WaitForCompletion.Obj();
        RefreshPolicy = other.RefreshPolicy.Obj();
        return *this;
    }
};

//
// COptions. Reads from and writes all the persistent settings
// like window position, column order etc.
//
class COptions final
{
    static LPCWSTR OptionsGeneral;
    static LPCWSTR OptionsTreeMap;
    static LPCWSTR OptionsFileTree;
    static LPCWSTR OptionsDupeTree;
    static LPCWSTR OptionsExtView;
    static LPCWSTR OptionsTopView;
    static LPCWSTR OptionsDriveSelect;

public:

    static Setting<bool> AutomaticallyResizeColumns;
    static Setting<bool> ExcludeJunctions;
    static Setting<bool> ExcludeSymbolicLinksDirectory;
    static Setting<bool> ExcludeVolumeMountPoints;
    static Setting<bool> ExcludeHiddenDirectory;
    static Setting<bool> ExcludeProtectedDirectory;
    static Setting<bool> ExcludeSymbolicLinksFile;
    static Setting<bool> ExcludeHiddenFile;
    static Setting<bool> ExcludeProtectedFile;
    static Setting<bool> FilteringUseRegex;
    static Setting<bool> FollowVolumeMountPoints;
    static Setting<bool> UseSizeSuffixes;
    static Setting<bool> ListFullRowSelection;
    static Setting<bool> ListGrid;
    static Setting<bool> ListStripes;
    static Setting<bool> PacmanAnimation;
    static Setting<bool> ScanForDuplicates;
    static Setting<bool> ShowColumnAttributes;
    static Setting<bool> ShowColumnFiles;
    static Setting<bool> ShowColumnFolders;
    static Setting<bool> ShowColumnItems;
    static Setting<bool> ShowColumnLastChange;
    static Setting<bool> ShowColumnOwner;
    static Setting<bool> ShowColumnSizeLogical;
    static Setting<bool> ShowColumnSizePhysical;
    static Setting<bool> ShowDeleteWarning;
    static Setting<bool> ShowFileTypes;
    static Setting<bool> ShowFreeSpace;
    static Setting<bool> ShowStatusBar;
    static Setting<bool> ShowTimeSpent;
    static Setting<bool> ShowToolBar;
    static Setting<bool> ShowTreeMap;
    static Setting<bool> ShowUnknown;
    static Setting<bool> SkipDupeDetectionCloudLinks;
    static Setting<bool> SkipDupeDetectionCloudLinksWarning;
    static Setting<bool> TreeMapGrid;
    static Setting<bool> TreeMapUseLogical;
    static Setting<bool> UseBackupRestore;
    static Setting<bool> UseWindowsLocaleSetting;
    static Setting<COLORREF> FileTreeColor0;
    static Setting<COLORREF> FileTreeColor1;
    static Setting<COLORREF> FileTreeColor2;
    static Setting<COLORREF> FileTreeColor3;
    static Setting<COLORREF> FileTreeColor4;
    static Setting<COLORREF> FileTreeColor5;
    static Setting<COLORREF> FileTreeColor6;
    static Setting<COLORREF> FileTreeColor7;
    static Setting<COLORREF> TreeMapGridColor;
    static Setting<COLORREF> TreeMapHighlightColor;
    static Setting<double> MainSplitterPos;
    static Setting<double> SubSplitterPos;
    static Setting<int> ConfigPage;
    static Setting<int> FollowReparsePointMask;
    static Setting<int> LanguageId;
    static Setting<int> LargeFileCount;
    static Setting<int> ScanningThreads;
    static Setting<int> SelectDrivesRadio;
    static Setting<int> FileTreeColorCount;
    static Setting<int> FilteringSizeMinimum;
    static Setting<int> FilteringSizeUnits;
    static Setting<int> TreeMapAmbientLightPercent;
    static Setting<int> TreeMapBrightness;
    static Setting<int> TreeMapHeightFactor;
    static Setting<int> TreeMapLightSourceX;
    static Setting<int> TreeMapLightSourceY;
    static Setting<int> TreeMapScaleFactor;
    static Setting<int> TreeMapStyle;
    static Setting<RECT> AboutWindowRect;
    static Setting<RECT> DriveSelectWindowRect;
    static Setting<std::vector<int>> DriveListColumnOrder;
    static Setting<std::vector<int>> DriveListColumnWidths;
    static Setting<std::vector<int>> DupeViewColumnOrder;
    static Setting<std::vector<int>> DupeViewColumnWidths;
    static Setting<std::vector<int>> FileTreeColumnOrder;
    static Setting<std::vector<int>> FileTreeColumnWidths;
    static Setting<std::vector<int>> ExtViewColumnOrder;
    static Setting<std::vector<int>> ExtViewColumnWidths;
    static Setting<std::vector<int>> TopViewColumnOrder;
    static Setting<std::vector<int>> TopViewColumnWidths;
    static Setting<std::vector<std::wstring>> SelectDrivesDrives;
    static Setting<std::wstring> FilteringExcludeDirs;
    static Setting<std::wstring> FilteringExcludeFiles;
    static Setting<std::wstring> SelectDrivesFolder;
    static Setting<WINDOWPLACEMENT> MainWindowPlacement;

    static CTreeMap::Options TreeMapOptions;
    static std::vector<USERDEFINEDCLEANUP> UserDefinedCleanups;
    static std::vector<std::wregex> FilteringExcludeDirsRegex;
    static std::vector<std::wregex> FilteringExcludeFilesRegex;
    static ULONGLONG FilteringSizeMinimumCalculated;

    static void SanitizeRect(RECT& rect);
    static void LoadAppSettings();
    static void PreProcessPersistedSettings();
    static void PostProcessPersistedSettings();
    static void SetTreeMapOptions(const CTreeMap::Options& options);
    static void CompileFilters();

    static LCID GetLocaleForFormatting();
};
