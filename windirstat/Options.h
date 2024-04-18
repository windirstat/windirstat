// Options.h - Declaration of CRegistryUser, COptions and COptions
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
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

class COptions;

constexpr auto USERDEFINEDCLEANUPCOUNT = 10;
constexpr auto TREELISTCOLORCOUNT = 8;

enum REFRESHPOLICY
{
    RP_NO_REFRESH,
    RP_REFRESH_THIS_ENTRY,
    RP_REFRESH_THIS_ENTRYS_PARENT
};

struct USERDEFINEDCLEANUP
{
    USERDEFINEDCLEANUP() : USERDEFINEDCLEANUP(L"") {}
    USERDEFINEDCLEANUP(const std::wstring & sEntry) :
        title(Setting<std::wstring>(sEntry, L"title", L"")),
        commandLine(Setting<std::wstring>(sEntry, L"commandLine", L"")),
        enabled(Setting<bool>(sEntry, L"enable", false)),
        virginTitle(Setting<bool>(sEntry, L"virginTitle", true)),
        worksForDrives(Setting<bool>(sEntry, L"worksForDrives", false)),
        worksForDirectories(Setting<bool>(sEntry, L"worksForDirectories", false)),
        worksForFiles(Setting<bool>(sEntry, L"worksForFiles", false)),
        worksForUncPaths(Setting<bool>(sEntry, L"worksForUncPaths", false)),
        recurseIntoSubdirectories(Setting<bool>(sEntry, L"recurseIntoSubdirectories", false)),
        askForConfirmation(Setting<bool>(sEntry, L"askForConfirmation", false)),
        showConsoleWindow(Setting<bool>(sEntry, L"showConsoleWindow", false)),
        waitForCompletion(Setting<bool>(sEntry, L"waitForCompletion", false)),
        refreshPolicy(Setting<int>(sEntry, L"refreshPolicy", 0)) {}

    Setting<std::wstring> title;
    Setting<std::wstring> commandLine;
    Setting<bool> enabled;
    Setting<bool> virginTitle;
    Setting<bool> worksForDrives;
    Setting<bool> worksForDirectories;
    Setting<bool> worksForFiles;
    Setting<bool> worksForUncPaths;
    Setting<bool> recurseIntoSubdirectories;
    Setting<bool> askForConfirmation;
    Setting<bool> showConsoleWindow;
    Setting<bool> waitForCompletion;
    Setting<int> refreshPolicy;

    // This will not transfer the property persistent settings but allows
    // this to be used as a generalized structure in the cleanup page
    USERDEFINEDCLEANUP(const USERDEFINEDCLEANUP& other) { *this = other; }
    USERDEFINEDCLEANUP& operator=(const USERDEFINEDCLEANUP& other)
    {
        title = other.title.Obj();
        commandLine = other.commandLine.Obj();
        enabled = other.enabled.Obj();
        virginTitle = other.virginTitle.Obj();
        worksForDrives = other.worksForDrives.Obj();
        worksForDirectories = other.worksForDirectories.Obj();
        worksForFiles = other.worksForFiles.Obj();
        worksForUncPaths = other.worksForUncPaths.Obj();
        recurseIntoSubdirectories = other.recurseIntoSubdirectories.Obj();
        askForConfirmation = other.askForConfirmation.Obj();
        showConsoleWindow = other.showConsoleWindow.Obj();
        waitForCompletion = other.waitForCompletion.Obj();
        refreshPolicy = other.refreshPolicy.Obj();
        return *this;
    }
};

//
// COptions. Reads from and writes all the persistent settings
// like window position, column order etc.
//
class COptions
{
public:

    static Setting<bool> ShowFreeSpace;
    static Setting<bool> ShowUnknown;
    static Setting<bool> ShowFileTypes;
    static Setting<bool> ShowToolbar;
    static Setting<bool> ShowTreemap;
    static Setting<bool> ShowStatusbar;
    static Setting<WINDOWPLACEMENT> MainWindowPlacement;
    static Setting<RECT> AboutWindowRect;
    static Setting<RECT> DriveWindowRect;

    static Setting<bool> ShowDeleteWarning;
    static Setting<int> ConfigPage;

    static Setting<int> SelectDrivesRadio;
    static Setting<std::wstring> SelectDrivesFolder;
    static Setting<std::vector<std::wstring>> SelectDrivesDrives;
    static Setting<bool> ScanForDuplicates;

    static Setting<std::vector<int>> TypesColumnWidths;
    static Setting<std::vector<int>> TypesColumnOrder;
    static Setting<std::vector<int>> FileTreeColumnWidths;
    static Setting<std::vector<int>> FileTreeColumnOrder;
    static Setting<std::vector<int>> DriveListColumnWidths;
    static Setting<std::vector<int>> DriveListColumnOrder;
    static Setting<std::vector<int>> DupeTreeColumnWidths;
    static Setting<std::vector<int>> DupeTreeColumnOrder;

    static Setting<bool> PacmanAnimation;
    static Setting<bool> ShowTimeSpent;
    static Setting<bool> HumanFormat;

    static Setting<bool> SkipDuplicationDetectionCloudLinks;
    static Setting<bool> ExcludeVolumeMountPoints;
    static Setting<bool> ExcludeJunctions;
    static Setting<bool> ExcludeSymbolicLinks;
    static Setting<int> FollowReparsePointMask;
    static Setting<bool> FollowVolumeMountPoints;
    static Setting<bool> UseBackupRestore;
    static Setting<int> ScanningThreads;

    static Setting<bool> SkipHidden;
    static Setting<bool> SkipProtected;
    static Setting<bool> UseFallbackLocale;
    static Setting<bool> ListGrid;
    static Setting<bool> ListStripes;
    static Setting<bool> ListFullRowSelection;

    static Setting<bool> ShowColumnFolders;
    static Setting<bool> ShowColumnItems;
    static Setting<bool> ShowColumnFiles;
    static Setting<bool> ShowColumnAttributes;
    static Setting<bool> ShowColumnLastChange;
    static Setting<bool> ShowColumnOwner;
    static Setting<bool> ShowColumnSizePhysical;
    static Setting<bool> ShowColumnSizeLogical;

    static Setting<COLORREF> TreeListColor0;
    static Setting<COLORREF> TreeListColor1;
    static Setting<COLORREF> TreeListColor2;
    static Setting<COLORREF> TreeListColor3;
    static Setting<COLORREF> TreeListColor4;
    static Setting<COLORREF> TreeListColor5;
    static Setting<COLORREF> TreeListColor6;
    static Setting<COLORREF> TreeListColor7;
    static Setting<int> TreeListColorCount;

    static Setting<int> TreeMapStyle;
    static Setting<int> TreeMapHeightFactor;
    static Setting<int> TreeMapBrightness;
    static Setting<int> TreeMapScaleFactor;
    static Setting<int> TreeMapAmbientLightPercent;
    static Setting<int> TreeMapLightSourceX;
    static Setting<int> TreeMapLightSourceY;
    static Setting<bool> TreeMapGrid;
    static Setting<COLORREF> TreeMapGridColor;
    static Setting<COLORREF> TreeMapHighlightColor;
    static Setting<double> MainSplitterPos;
    static Setting<double> SubSplitterPos;
    static Setting<int> LanguageId;

    static void SanitizeRect(RECT& rect);

    static void LoadAppSettings();
    static void PreProcessPersistedSettings();
    static void PostProcessPersistedSettings();

    static void SetTreemapOptions(const CTreemap::Options& options);

    static CTreemap::Options TreemapOptions;
    static std::vector<USERDEFINEDCLEANUP> UserDefinedCleanups;

    static LANGID GetFallbackLanguage();
    static LANGID GetEffectiveLangId(); // Language to be used for date/time and number formatting
};
