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
#include "TreeMap.h"
#include "Property.h"

class COptions;

constexpr auto USERDEFINEDCLEANUPCOUNT = 10;
constexpr auto TREELISTCOLORCOUNT = 8;
constexpr auto PERMSRULECOUNT = 5;

enum REFRESHPOLICY : std::uint8_t
{
    RP_NO_REFRESH,
    RP_REFRESH_THIS_ENTRY,
    RP_REFRESH_THIS_ENTRYS_PARENT
};

enum DARKMODE : std::uint8_t
{
    DM_DISABLED,
    DM_ENABLED,
    DM_USE_WINDOWS
};

enum class GraphPane : std::uint8_t
{
    TreeMap,
    FlameGraph,
    Sunburst,
};

// GraphPaneStyle historically stored treemap layouts (0, 1, 4, 5) alongside
// flame graph (2) and sunburst (3). Decode those values while keeping the
// runtime pane model independent from TreeMapLayout::Style.
[[nodiscard]] constexpr GraphPane DecodeGraphPane(const int persisted) noexcept
{
    if (persisted == 2) return GraphPane::FlameGraph;
    if (persisted == 3) return GraphPane::Sunburst;
    return GraphPane::TreeMap;
}

[[nodiscard]] constexpr int EncodeGraphPane(const GraphPane pane) noexcept
{
    switch (pane)
    {
    case GraphPane::TreeMap: return 0;
    case GraphPane::FlameGraph: return 2;
    case GraphPane::Sunburst: return 3;
    }
    return 0;
}

constexpr int MaxPersistedGraphPane = 5;

// Layout view types
enum LAYOUT_VIEW_TYPE : int
{
    VT_ABSENT    = -1,
    VT_ALLFILES  = 0,
    VT_FILETYPES = 1,
    VT_TREEMAP   = 2
};

// Layout topology: the structural arrangement of the two splitters
enum LAYOUT_TOPOLOGY : int
{
    LT_ROWS_SUB_COLS = 0, // Main = 2 rows; sub-splitter = 2 cols inside one row (default)
    LT_COLS_THREE    = 2, // Main = 2 cols; sub-splitter = 2 cols in col 0 (three-column)
    LT_COLS_SUB_ROWS = 3, // Main = 2 cols; sub-splitter = 2 rows in col 0
    LT_COLS_TM_FULL  = 4, // Main = 2 cols; treemap fills one col; sub-splitter = 2 rows in the other
};

struct USERDEFINEDCLEANUP
{
    USERDEFINEDCLEANUP() : USERDEFINEDCLEANUP(L"") {}
    USERDEFINEDCLEANUP(const std::wstring & sEntry) :
        Title(Setting<std::wstring>(sEntry, L"Title", L"")),
        CommandLine(Setting<std::wstring>(sEntry, L"CommandLine", L"")),
        Enabled(Setting(sEntry, L"Enable", false)),
        VirginTitle(Setting(sEntry, L"VirginTitle", true)),
        WorksForDrives(Setting(sEntry, L"WorksForDrives", false)),
        WorksForDirectories(Setting(sEntry, L"WorksForDirectories", false)),
        WorksForFiles(Setting(sEntry, L"WorksForFiles", false)),
        WorksForUncPaths(Setting(sEntry, L"WorksForUncPaths", false)),
        RecurseIntoSubdirectories(Setting(sEntry, L"RecurseIntoSubdirectories", false)),
        AskForConfirmation(Setting(sEntry, L"AskForConfirmation", false)),
        ShowConsoleWindow(Setting(sEntry, L"ShowConsoleWindow", false)),
        WaitForCompletion(Setting(sEntry, L"WaitForCompletion", false)),
        RefreshPolicy(Setting(sEntry, L"RefreshPolicy", 0)) {}

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
// COptions. Reads and writes all persistent settings
// like window position, column order etc.
//
class COptions final
{
    inline static LPCWSTR OptionsGeneral = L"Options";
    inline static LPCWSTR OptionsTreeMap = L"TreeMapView";
    inline static LPCWSTR OptionsFileTree = L"FileTreeView";
    inline static LPCWSTR OptionsDupeTree = L"DupeView";
    inline static LPCWSTR OptionsExtView = L"ExtView";
    inline static LPCWSTR OptionsTopView = L"TopView";
    inline static LPCWSTR OptionsSearch = L"SearchView";
    inline static LPCWSTR OptionsWatcher = L"Watcher";
    inline static LPCWSTR OptionsPerms = L"PermissionsView";
    inline static LPCWSTR OptionsDriveSelect = L"DriveSelect";

public:
    inline static Setting<bool> AutomaticallyResizeColumns{ OptionsGeneral, L"AutomaticallyResizeColumns", true };
    inline static Setting<bool> ExcludeJunctions{ OptionsGeneral, L"ExcludeJunctions", true };
    inline static Setting<bool> ExcludeSymbolicLinksDirectory{ OptionsGeneral, L"ExcludeSymbolicLinksDirectory", true };
    inline static Setting<bool> ExcludeVolumeMountPoints{ OptionsGeneral, L"ExcludeVolumeMountPoints", true };
    inline static Setting<bool> ExcludeHiddenDirectory{ OptionsGeneral, L"ExcludeHiddenDirectory", false };
    inline static Setting<bool> ExcludeProtectedDirectory{ OptionsGeneral, L"ExcludeProtectedDirectory", false };
    inline static Setting<bool> ExcludeSymbolicLinksFile{ OptionsGeneral, L"ExcludeSymbolicLinksFile", true };
    inline static Setting<bool> ExcludeHiddenFile{ OptionsGeneral, L"ExcludeHiddenFile", false };
    inline static Setting<bool> ExcludeProtectedFile{ OptionsGeneral, L"ExcludeProtectedFile", false };
    inline static Setting<bool> FilteringUseRegex{ OptionsGeneral, L"FilteringUseRegex", false };
    inline static Setting<bool> FollowVolumeMountPoints{ OptionsGeneral, L"FollowVolumeMountPoints", false };
    inline static Setting<bool> UseSizeSuffixes{ OptionsGeneral, L"UseSizeSuffixes", true };
    inline static Setting<bool> ListFullRowSelection{ OptionsGeneral, L"ListFullRowSelection", true };
    inline static Setting<bool> ListGrid{ OptionsGeneral, L"ListGrid", false };
    inline static Setting<bool> ListStripes{ OptionsGeneral, L"ListStripes", false };
    inline static Setting<bool> PacmanAnimation{ OptionsGeneral, L"PacmanAnimation", true };
    inline static Setting<bool> ScanForDuplicates{ OptionsDupeTree, L"ScanForDuplicates", false };
    inline static Setting<bool> SearchWholePhrase{ OptionsSearch, L"SearchWholePhrase", false };
    inline static Setting<bool> SearchRegex{ OptionsSearch, L"SearchRegex", false };
    inline static Setting<bool> SearchCase{ OptionsSearch, L"SearchCase", false };
    inline static Setting<int> SearchMaxResults{ OptionsSearch, L"SearchMaxResults", 10000, 1, 1000000 };
    inline static Setting<bool> ShowDeleteWarning{ OptionsGeneral, L"ShowDeleteWarning", true };
    inline static Setting<bool> ShowElevationPrompt{ OptionsGeneral, L"ShowElevationPrompt", true };
    inline static Setting<bool> ShowMicrosoftProgress{ OptionsGeneral, L"ShowMicrosoftProgress", false };
    inline static Setting<bool> ShowFileTypes{ OptionsGeneral, L"ShowFileTypes", true };
    inline static Setting<bool> GroupUnregisteredTypes{ OptionsGeneral, L"GroupUnregisteredTypes", false };
    inline static Setting<bool> ShowFreeSpace{ OptionsGeneral, L"ShowFreeSpace", false };
    inline static Setting<bool> ShowStatusBar{ OptionsGeneral, L"ShowStatusBar", true };
    inline static Setting<bool> ShowTimeSpent{ OptionsFileTree, L"ShowTimeSpent", true };
    inline static Setting<bool> ShowToolBar{ OptionsGeneral, L"ShowToolBar", true };
    inline static Setting<bool> LargeToolBar{ OptionsGeneral, L"LargeToolBar", false };
    inline static Setting<bool> ShowTreeMap{ OptionsTreeMap, L"ShowTreeMap", true };
    inline static Setting<bool> ShowUnknown{ OptionsGeneral, L"ShowUnknown", false };
    inline static Setting<bool> SkipDupeDetectionCloudLinks{ OptionsGeneral, L"SkipDupeDetectionCloudLinks", true };
    inline static Setting<bool> ShowDupeDetectionCloudLinksWarning{ OptionsGeneral, L"ShowDupeDetectionCloudLinksWarning", true };
    inline static Setting<bool> AutoElevate{ OptionsGeneral, L"AutoElevate", false };
    inline static Setting<bool> AutoMapDrivesWhenElevated{ OptionsGeneral, L"AutoMapDrivesWhenElevated", true };
    inline static Setting<bool> TreeMapGrid{ OptionsTreeMap, L"TreeMapGrid", (CTreeMap::GetDefaults().grid) };
    inline static Setting<bool> TreeMapShowExtensions{ OptionsTreeMap, L"TreeMapShowExtensions", (CTreeMap::GetDefaults().showExtensions) };
    inline static Setting<bool> TreeMapShowFolderFrames{ OptionsTreeMap, L"TreeMapShowFolderFrames", (CTreeMap::GetDefaults().showFolderFrames) };
    inline static Setting<bool> TreeMapUseLogical{ OptionsTreeMap, L"TreeMapUseLogicalSize", false };
    inline static Setting<bool> UseAbsolutePercentages{ OptionsFileTree, L"UseAbsolutePercentages", true };
    inline static Setting<bool> UseBackupRestore{ OptionsGeneral, L"UseBackupRestore", true };
    inline static Setting<bool> UseDrawTextCache{ OptionsGeneral, L"UseDrawTextCache", true };
    inline static Setting<bool> UseFastScanEngine{ OptionsGeneral, L"UseFastScanEngine", true };
    inline static Setting<bool> UseWindowsLocaleSetting{ OptionsGeneral, L"UseWindowsLocaleSetting", true };
    inline static Setting<bool> ProcessHardlinks{ OptionsGeneral, L"ProcessHardlinks", true };
    inline static Setting<COLORREF> FileTreeColors[TREELISTCOLORCOUNT] =
    {
        { OptionsFileTree, L"FileTreeColor0", RGB(64, 64, 140) },
        { OptionsFileTree, L"FileTreeColor1", RGB(140, 64, 64) },
        { OptionsFileTree, L"FileTreeColor2", RGB(64, 140, 64) },
        { OptionsFileTree, L"FileTreeColor3", RGB(140, 140, 64) },
        { OptionsFileTree, L"FileTreeColor4", RGB(0, 0, 255) },
        { OptionsFileTree, L"FileTreeColor5", RGB(255, 0, 0) },
        { OptionsFileTree, L"FileTreeColor6", RGB(0, 255, 0) },
        { OptionsFileTree, L"FileTreeColor7", RGB(255, 255, 0) }
    };
    inline static Setting<COLORREF> TreeMapGridColor{ OptionsTreeMap, L"TreeMapGridColor", CTreeMap::GetDefaults().gridColor };
    inline static Setting<COLORREF> TreeMapHighlightColor{ OptionsTreeMap, L"TreeMapHighlightColor", RGB(255, 255, 255) };
    inline static Setting<std::wstring> PermsColorAccount[PERMSRULECOUNT] =
    {
        { OptionsPerms, L"ColorAccount0", L"" },
        { OptionsPerms, L"ColorAccount1", L"" },
        { OptionsPerms, L"ColorAccount2", L"" },
        { OptionsPerms, L"ColorAccount3", L"" },
        { OptionsPerms, L"ColorAccount4", L"" }
    };
    // Level values are 0 (any) or 1 + PERMSLEVEL enumeration value (excluding Special)
    inline static Setting<int> PermsColorLevel[PERMSRULECOUNT] =
    {
        { OptionsPerms, L"ColorLevel0", 0, 0, 5 },
        { OptionsPerms, L"ColorLevel1", 0, 0, 5 },
        { OptionsPerms, L"ColorLevel2", 0, 0, 5 },
        { OptionsPerms, L"ColorLevel3", 0, 0, 5 },
        { OptionsPerms, L"ColorLevel4", 0, 0, 5 }
    };
    inline static Setting<COLORREF> PermsColor[PERMSRULECOUNT] =
    {
        { OptionsPerms, L"Color0", RGB(200, 0, 0) },
        { OptionsPerms, L"Color1", RGB(200, 100, 0) },
        { OptionsPerms, L"Color2", RGB(0, 100, 200) },
        { OptionsPerms, L"Color3", RGB(0, 150, 0) },
        { OptionsPerms, L"Color4", RGB(150, 0, 200) }
    };
    inline static Setting<std::wstring> PermsExcludeRegex{ OptionsPerms, L"ExcludeRegex", L"" };
    inline static Setting<double> MainSplitterPos{ OptionsGeneral, L"MainSplitterPos", -1.0, 0.0, 1.0 };
    inline static Setting<double> SubSplitterPos{ OptionsGeneral, L"SubSplitterPos", -1.0, 0.0, 1.0 };
    inline static Setting<int> ConfigPage{ OptionsGeneral, L"ConfigPage", 0 };
    inline static Setting<int> DarkMode{ OptionsGeneral, L"DarkMode", DM_USE_WINDOWS, DM_DISABLED, DM_USE_WINDOWS };
    inline static Setting<int> LanguageId{ OptionsGeneral, L"LanguageId", 0 };
    // 0=Low, 1=Normal, 2=High
    inline static Setting<int> ProcessPriority{ OptionsGeneral, L"ProcessPriority", 1, 0, 2 };
    inline static Setting<int> FileHashAlgorithm{ OptionsGeneral, L"FileHashAlgorithm", HASH_XXHASH, HASH_MD5, HASH_XXHASH };
    inline static Setting<int> LargeFileCount{ OptionsGeneral, L"LargeFileCount", 50, 0, 10000 };
    inline static Setting<int> MinimizeViewThreshold{ OptionsGeneral, L"MinimizeViewThreshold", 10, 1, 10000 };
    inline static Setting<int> ScanningThreads{ OptionsGeneral, L"ScanningThreads", 4, 1, 16 };
    inline static Setting<int> SelectDrivesRadio{ OptionsDriveSelect, L"SelectDrivesRadio", 0, 0, 2 };
    inline static Setting<int> SizeProportionIndent{ OptionsFileTree, L"SizeProportionIndent", 16, 0, 1000 };
    inline static Setting<int> FileTreeColorCount{ OptionsFileTree, L"FileTreeColorCount", 8, 1, TREELISTCOLORCOUNT };
    inline static Setting<int> FilteringSizeMinimum{ OptionsGeneral, L"FilteringSizeMinimum", 0 };
    inline static Setting<int> FilteringSizeUnits{ OptionsGeneral, L"FilteringSizeUnits", 0 };
    inline static Setting<int> FilteringMaxAgeDays{ OptionsGeneral, L"FilteringMaxAgeDays", 0 };
    inline static Setting<int> TreeMapAmbientLightPercent{ OptionsTreeMap, L"TreeMapAmbientLightPercent", CTreeMap::GetDefaults().GetAmbientLightPercent(), 0, 100 };
    inline static Setting<int> TreeMapBrightness{ OptionsTreeMap, L"TreeMapBrightness", CTreeMap::GetDefaults().GetBrightnessPercent(), 0, 100 };
    inline static Setting<int> TreeMapFolderFramesDrawThreshold{ OptionsTreeMap, L"TreeMapFolderFramesDrawThreshold", CTreeMap::GetDefaults().folderFramesDrawThreshold, 3, 128 };
    inline static Setting<int> TreeMapHeightFactor{ OptionsTreeMap, L"TreeMapHeightFactor", CTreeMap::GetDefaults().GetHeightPercent(), 0, 100 };
    inline static Setting<int> TreeMapLightSourceX{ OptionsTreeMap, L"TreeMapLightSourceX", CTreeMap::GetDefaults().GetLightSourceXPercent(), -200, 200 };
    inline static Setting<int> TreeMapLightSourceY{ OptionsTreeMap, L"TreeMapLightSourceY", CTreeMap::GetDefaults().GetLightSourceYPercent(), -200, 200 };
    inline static Setting<int> TreeMapScaleFactor{ OptionsTreeMap, L"TreeMapScaleFactor", CTreeMap::GetDefaults().GetScaleFactorPercent(), 0, 100 };
    inline static Setting<int> TreeMapStyle{ OptionsTreeMap, L"TreeMapStyle", static_cast<int>(CTreeMap::GetDefaults().style), static_cast<int>(TreeMapLayout::Style::Rows), static_cast<int>(TreeMapLayout::Style::Moore) };
    inline static Setting<int> GraphPaneStyle{ OptionsTreeMap, L"GraphPaneStyle", EncodeGraphPane(GraphPane::TreeMap), 0, MaxPersistedGraphPane };
    inline static Setting<int> TreeMapMaxDepth{ OptionsTreeMap, L"TreeMapMaxDepth", 6, 1, 64 };
    inline static Setting<int> FolderHistoryCount{ OptionsDriveSelect, L"FolderHistoryCount", 10, 0, 100 };
    inline static Setting<int> LayoutTopology{ OptionsGeneral, L"LayoutTopology", LT_ROWS_SUB_COLS, LT_ROWS_SUB_COLS, LT_COLS_TM_FULL };
    inline static Setting<int> LayoutPermutation{ OptionsGeneral, L"LayoutPermutation", 0, 0, 3 };
    inline static Setting<RECT> AboutWindowRect{ OptionsGeneral, L"AboutWindowRect" };
    inline static Setting<RECT> DriveSelectWindowRect{ OptionsDriveSelect, L"WindowRect" };
    inline static Setting<RECT> SearchWindowRect{ OptionsSearch, L"WindowRect" };
    inline static Setting<std::vector<int>> DriveListColumnOrder{ OptionsDriveSelect, L"ColumnOrder" };
    inline static Setting<std::vector<int>> DriveListColumnWidths{ OptionsDriveSelect, L"ColumnWidths" };
    inline static Setting<std::vector<int>> DriveListColumnVisibility{ OptionsDriveSelect, L"ColumnVisibility" };
    inline static Setting<std::vector<int>> DupeViewColumnOrder{ OptionsDupeTree, L"ColumnOrder" };
    inline static Setting<std::vector<int>> DupeViewColumnWidths{ OptionsDupeTree, L"ColumnWidths" };
    inline static Setting<std::vector<int>> DupeViewColumnVisibility{ OptionsDupeTree, L"ColumnVisibility" };
    inline static Setting<std::vector<int>> FileTreeColumnOrder{ OptionsFileTree, L"ColumnOrder" };
    inline static Setting<std::vector<int>> FileTreeColumnWidths{ OptionsFileTree, L"ColumnWidths" };
    inline static Setting<std::vector<int>> FileTreeColumnVisibility{ OptionsFileTree, L"ColumnVisibility" };
    inline static Setting<std::vector<int>> ExtViewColumnOrder{ OptionsExtView, L"ColumnOrder" };
    inline static Setting<std::vector<int>> ExtViewColumnWidths{ OptionsExtView, L"ColumnWidths" };
    inline static Setting<std::vector<int>> ExtViewColumnVisibility{ OptionsExtView, L"ColumnVisibility" };
    inline static Setting<std::vector<int>> TopViewColumnOrder{ OptionsTopView, L"ColumnOrder" };
    inline static Setting<std::vector<int>> TopViewColumnWidths{ OptionsTopView, L"ColumnWidths" };
    inline static Setting<std::vector<int>> TopViewColumnVisibility{ OptionsTopView, L"ColumnVisibility" };
    inline static Setting<std::vector<int>> SearchViewColumnOrder{ OptionsSearch, L"ColumnOrder" };
    inline static Setting<std::vector<int>> SearchViewColumnWidths{ OptionsSearch, L"ColumnWidths" };
    inline static Setting<std::vector<int>> SearchViewColumnVisibility{ OptionsSearch, L"ColumnVisibility" };
    inline static Setting<bool> WatcherAutoScroll{ OptionsWatcher, L"AutoScroll", true };
    inline static Setting<std::vector<int>> WatcherColumnOrder{ OptionsWatcher, L"ColumnOrder" };
    inline static Setting<std::vector<int>> WatcherColumnWidths{ OptionsWatcher, L"ColumnWidths" };
    inline static Setting<std::vector<int>> WatcherColumnVisibility{ OptionsWatcher, L"ColumnVisibility" };
    inline static Setting<std::vector<int>> PermsViewColumnOrder{ OptionsPerms, L"ColumnOrder" };
    inline static Setting<std::vector<int>> PermsViewColumnWidths{ OptionsPerms, L"ColumnWidths" };
    inline static Setting<std::vector<int>> PermsViewColumnVisibility{ OptionsPerms, L"ColumnVisibility" };
    inline static Setting<std::vector<std::wstring>> SelectDrivesDrives{ OptionsDriveSelect, L"SelectDrivesDrives" };
    inline static Setting<std::vector<std::wstring>> SelectDrivesFolder{ OptionsDriveSelect, L"SelectDrivesFolder" };
    inline static Setting<std::wstring> SearchTerm{ OptionsSearch, L"SearchTerm" };
    inline static Setting<std::wstring> FilteringExcludeDirs{ OptionsDriveSelect, L"FilteringExcludeDirs" };
    inline static Setting<std::wstring> FilteringExcludeFiles{ OptionsDriveSelect, L"FilteringExcludeFiles" };
    inline static Setting<std::wstring> FilteringIncludeDirs{ OptionsDriveSelect, L"FilteringIncludeDirs" };
    inline static Setting<std::wstring> FilteringIncludeFiles{ OptionsDriveSelect, L"FilteringIncludeFiles" };
    inline static Setting<WINDOWPLACEMENT> MainWindowPlacement{ OptionsGeneral, L"MainWindowPlacement" };

    inline static CTreeMap::Options TreeMapOptions;
    inline static std::vector<USERDEFINEDCLEANUP> UserDefinedCleanups;

    static void SanitizeRect(RECT& rect);
    static bool IsColumnVisible(const std::vector<int>& visibility, int subitem) noexcept;
    static void SetColumnVisible(std::vector<int>& visibility, int subitem, bool visible);
    static void LoadAppSettings();
    static void PreProcessPersistedSettings();
    static void PostProcessPersistedSettings();
    static void SetTreeMapOptions(const CTreeMap::Options& options);

    static LCID GetLocaleForFormatting();
};
