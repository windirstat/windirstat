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
#include "Property.h"

LPCWSTR COptions::OptionsGeneral = L"Options";
LPCWSTR COptions::OptionsTreeMap = L"TreeMapView";
LPCWSTR COptions::OptionsFileTree = L"FileTreeView";
LPCWSTR COptions::OptionsDupeTree = L"DupeView";
LPCWSTR COptions::OptionsExtView = L"ExtView";
LPCWSTR COptions::OptionsTopView = L"TopView";
LPCWSTR COptions::OptionsSearch = L"SearchView";
LPCWSTR COptions::OptionsDriveSelect = L"DriveSelect";

Setting<bool> COptions::AutomaticallyResizeColumns(OptionsGeneral, L"AutomaticallyResizeColumns", true);
Setting<bool> COptions::ExcludeJunctions(OptionsGeneral, L"ExcludeJunctions", true);
Setting<bool> COptions::ExcludeSymbolicLinksDirectory(OptionsGeneral, L"ExcludeSymbolicLinksDirectory", true);
Setting<bool> COptions::ExcludeVolumeMountPoints(OptionsGeneral, L"ExcludeVolumeMountPoints", true);
Setting<bool> COptions::ExcludeHiddenDirectory(OptionsGeneral, L"ExcludeHiddenDirectory", false);
Setting<bool> COptions::ExcludeProtectedDirectory(OptionsGeneral, L"ExcludeProtectedDirectory", false);
Setting<bool> COptions::ExcludeSymbolicLinksFile(OptionsGeneral, L"ExcludeSymbolicLinksFile", true);
Setting<bool> COptions::ExcludeHiddenFile(OptionsGeneral, L"ExcludeHiddenFile", false);
Setting<bool> COptions::ExcludeProtectedFile(OptionsGeneral, L"ExcludeProtectedFile", false);
Setting<bool> COptions::FilteringUseRegex(OptionsGeneral, L"FilteringUseRegex", false);
Setting<bool> COptions::FollowVolumeMountPoints(OptionsGeneral, L"FollowVolumeMountPoints", false);
Setting<bool> COptions::UseSizeSuffixes(OptionsGeneral, L"UseSizeSuffixes", true);
Setting<bool> COptions::ListFullRowSelection(OptionsGeneral, L"ListFullRowSelection", true);
Setting<bool> COptions::ListGrid(OptionsGeneral, L"ListGrid", false);
Setting<bool> COptions::ListStripes(OptionsGeneral, L"ListStripes", false);
Setting<bool> COptions::PacmanAnimation(OptionsGeneral, L"PacmanAnimation", true);
Setting<bool> COptions::ScanForDuplicates(OptionsDupeTree, L"ScanForDuplicates", false);
Setting<bool> COptions::SearchWholePhrase(OptionsSearch, L"SearchWholePhrase", false);
Setting<bool> COptions::SearchRegex(OptionsSearch, L"SearchRegex", false);
Setting<bool> COptions::SearchCase(OptionsSearch, L"SearchCase", false);
Setting<int> COptions::SearchMaxResults(OptionsSearch, L"SearchMaxResults", 10000, 1, 1000000);
Setting<bool> COptions::ShowColumnAttributes(OptionsFileTree, L"ShowColumnAttributes", false);
Setting<bool> COptions::ShowColumnFiles(OptionsFileTree, L"ShowColumnFiles", true);
Setting<bool> COptions::ShowColumnFolders(OptionsFileTree, L"ShowColumnFolders", false);
Setting<bool> COptions::ShowColumnItems(OptionsFileTree, L"ShowColumnItems", false);
Setting<bool> COptions::ShowColumnLastChange(OptionsFileTree, L"ShowColumnLastChange", true);
Setting<bool> COptions::ShowColumnOwner(OptionsFileTree, L"ShowColumnOwner", false);
Setting<bool> COptions::ShowColumnSizeLogical(OptionsFileTree, L"ShowColumnSizeLogical", true);
Setting<bool> COptions::ShowColumnSizePhysical(OptionsFileTree, L"ShowColumnSizePhysical", true);
Setting<bool> COptions::ShowDeleteWarning(OptionsGeneral, L"ShowDeleteWarning", true);
Setting<bool> COptions::ShowElevationPrompt(OptionsGeneral, L"ShowElevationPrompt", true);
Setting<bool> COptions::ShowMicrosoftProgress(OptionsGeneral, L"ShowMicrosoftProgress", false);
Setting<bool> COptions::ShowFileTypes(OptionsGeneral, L"ShowFileTypes", true);
Setting<bool> COptions::ShowFreeSpace(OptionsGeneral, L"ShowFreeSpace", false);
Setting<bool> COptions::ShowStatusBar(OptionsGeneral, L"ShowStatusBar", true);
Setting<bool> COptions::ShowTimeSpent(OptionsFileTree, L"ShowTimeSpent", true);
Setting<bool> COptions::ShowToolBar(OptionsGeneral, L"ShowToolBar", true);
Setting<bool> COptions::ShowTreeMap(OptionsTreeMap, L"ShowTreeMap", true);
Setting<bool> COptions::ShowUnknown(OptionsGeneral, L"ShowUnknown", false);
Setting<bool> COptions::SkipDupeDetectionCloudLinks(OptionsGeneral, L"SkipDupeDetectionCloudLinks", true);
Setting<bool> COptions::ShowDupeDetectionCloudLinksWarning(OptionsGeneral, L"ShowDupeDetectionCloudLinksWarning", true);
Setting<bool> COptions::AutoElevate(OptionsGeneral, L"AutoElevate", false);
Setting<bool> COptions::AutoMapDrivesWhenElevated(OptionsGeneral, L"AutoMapDrivesWhenElevated", true);
Setting<bool> COptions::TreeMapGrid(OptionsTreeMap, L"TreeMapGrid", (CTreeMap::GetDefaults().grid));
Setting<bool> COptions::TreeMapUseLogical(OptionsTreeMap, L"TreeMapUseLogicalSize", false);
Setting<bool> COptions::UseBackupRestore(OptionsGeneral, L"UseBackupRestore", true);
Setting<bool> COptions::UseDrawTextCache(OptionsGeneral, L"UseDrawTextCache", true);
Setting<bool> COptions::UseFastScanEngine(OptionsGeneral, L"UseFastScanEngine", true);
Setting<bool> COptions::UseWindowsLocaleSetting(OptionsGeneral, L"UseWindowsLocaleSetting", true);
Setting<bool> COptions::ProcessHardlinks(OptionsGeneral, L"ProcessHardlinks", true);
Setting<COLORREF> COptions::FileTreeColor0(OptionsFileTree, L"FileTreeColor0", RGB(64, 64, 140));
Setting<COLORREF> COptions::FileTreeColor1(OptionsFileTree, L"FileTreeColor1", RGB(140, 64, 64));
Setting<COLORREF> COptions::FileTreeColor2(OptionsFileTree, L"FileTreeColor2", RGB(64, 140, 64));
Setting<COLORREF> COptions::FileTreeColor3(OptionsFileTree, L"FileTreeColor3", RGB(140, 140, 64));
Setting<COLORREF> COptions::FileTreeColor4(OptionsFileTree, L"FileTreeColor4", RGB(0, 0, 255));
Setting<COLORREF> COptions::FileTreeColor5(OptionsFileTree, L"FileTreeColor5", RGB(255, 0, 0));
Setting<COLORREF> COptions::FileTreeColor6(OptionsFileTree, L"FileTreeColor6", RGB(0, 255, 0));
Setting<COLORREF> COptions::FileTreeColor7(OptionsFileTree, L"FileTreeColor7", RGB(255, 255, 0));
Setting<COLORREF> COptions::TreeMapGridColor(OptionsTreeMap, L"TreeMapGridColor", CTreeMap::GetDefaults().gridColor);
Setting<COLORREF> COptions::TreeMapHighlightColor(OptionsTreeMap, L"TreeMapHighlightColor", RGB(255, 255, 255));
Setting<double> COptions::MainSplitterPos(OptionsGeneral, L"MainSplitterPos", -1.0, 0.0, 1.0);
Setting<double> COptions::SubSplitterPos(OptionsGeneral, L"SubSplitterPos", -1.0, 0.0, 1.0);
Setting<int> COptions::ConfigPage(OptionsGeneral, L"ConfigPage", 0);
Setting<int> COptions::DarkMode(OptionsGeneral, L"DarkMode", DM_USE_WINDOWS, DM_DISABLED, DM_USE_WINDOWS);
Setting<int> COptions::LanguageId(OptionsGeneral, L"LanguageId", 0);
Setting<int> COptions::LargeFileCount(OptionsGeneral, L"LargeFileCount", 50, 0, 10000);
Setting<int> COptions::ScanningThreads(OptionsGeneral, L"ScanningThreads", 4, 1, 16);
Setting<int> COptions::SelectDrivesRadio(OptionsDriveSelect, L"SelectDrivesRadio", 0, 0, 2);
Setting<int> COptions::FileTreeColorCount(OptionsFileTree, L"FileTreeColorCount", 8);
Setting<int> COptions::FilteringSizeMinimum(OptionsGeneral, L"FilteringSizeMinimum", 0);
Setting<int> COptions::FilteringSizeUnits(OptionsGeneral, L"FilteringSizeUnits", 0);
Setting<int> COptions::TreeMapAmbientLightPercent(OptionsTreeMap, L"TreeMapAmbientLightPercent", CTreeMap::GetDefaults().GetAmbientLightPercent(), 0, 100);
Setting<int> COptions::TreeMapBrightness(OptionsTreeMap, L"TreeMapBrightness", CTreeMap::GetDefaults().GetBrightnessPercent(), 0, 100);
Setting<int> COptions::TreeMapHeightFactor(OptionsTreeMap, L"TreeMapHeightFactor", CTreeMap::GetDefaults().GetHeightPercent(), 0, 100);
Setting<int> COptions::TreeMapLightSourceX(OptionsTreeMap, L"TreeMapLightSourceX", CTreeMap::GetDefaults().GetLightSourceXPercent(), -200, 200);
Setting<int> COptions::TreeMapLightSourceY(OptionsTreeMap, L"TreeMapLightSourceY", CTreeMap::GetDefaults().GetLightSourceYPercent(), -200, 200);
Setting<int> COptions::TreeMapScaleFactor(OptionsTreeMap, L"TreeMapScaleFactor", CTreeMap::GetDefaults().GetScaleFactorPercent(), 0, 100);
Setting<int> COptions::TreeMapStyle(OptionsTreeMap, L"TreeMapStyle", CTreeMap::GetDefaults().style, 0, 1);
Setting<int> COptions::FolderHistoryCount(OptionsDriveSelect, L"FolderHistoryCount", 10, 0, 100);
Setting<RECT> COptions::AboutWindowRect(OptionsGeneral, L"AboutWindowRect");
Setting<RECT> COptions::DriveSelectWindowRect(OptionsDriveSelect, L"WindowRect");
Setting<RECT> COptions::SearchWindowRect(OptionsSearch, L"WindowRect");
Setting<std::vector<int>> COptions::DriveListColumnOrder(OptionsDriveSelect, L"ColumnOrder");
Setting<std::vector<int>> COptions::DriveListColumnWidths(OptionsDriveSelect, L"ColumnWidths");
Setting<std::vector<int>> COptions::DupeViewColumnOrder(OptionsDupeTree, L"ColumnOrder");
Setting<std::vector<int>> COptions::DupeViewColumnWidths(OptionsDupeTree, L"ColumnWidths");
Setting<std::vector<int>> COptions::FileTreeColumnOrder(OptionsFileTree, L"ColumnOrder");
Setting<std::vector<int>> COptions::FileTreeColumnWidths(OptionsFileTree, L"ColumnWidths");
Setting<std::vector<int>> COptions::ExtViewColumnOrder(OptionsExtView, L"ColumnOrder");
Setting<std::vector<int>> COptions::ExtViewColumnWidths(OptionsExtView, L"ColumnWidths");
Setting<std::vector<int>> COptions::TopViewColumnOrder(OptionsTopView, L"ColumnOrder");
Setting<std::vector<int>> COptions::TopViewColumnWidths(OptionsTopView, L"ColumnWidths");
Setting<std::vector<int>> COptions::SearchViewColumnOrder(OptionsSearch, L"ColumnOrder");
Setting<std::vector<int>> COptions::SearchViewColumnWidths(OptionsSearch, L"ColumnWidths");
Setting<std::vector<std::wstring>> COptions::SelectDrivesDrives(OptionsDriveSelect, L"SelectDrivesDrives");
Setting<std::vector<std::wstring>> COptions::SelectDrivesFolder(OptionsDriveSelect, L"SelectDrivesFolder");
Setting<std::wstring> COptions::SearchTerm(OptionsSearch, L"SearchTerm");
Setting<std::wstring> COptions::FilteringExcludeDirs(OptionsDriveSelect, L"FilteringExcludeDirs");
Setting<std::wstring> COptions::FilteringExcludeFiles(OptionsDriveSelect, L"FilteringExcludeFiles");
Setting<WINDOWPLACEMENT> COptions::MainWindowPlacement(OptionsGeneral, L"MainWindowPlacement");

CTreeMap::Options COptions::TreeMapOptions;
std::vector<USERDEFINEDCLEANUP> COptions::UserDefinedCleanups;
std::vector<std::wregex> COptions::FilteringExcludeDirsRegex;
std::vector<std::wregex> COptions::FilteringExcludeFilesRegex;
ULONGLONG COptions::FilteringSizeMinimumCalculated;

void COptions::SanitizeRect(RECT& rect)
{
    CRect rc(rect);
    constexpr int visible = 30;

    rc.NormalizeRect();

    CRect rcDesktop;
    CWnd::GetDesktopWindow()->GetWindowRect(rcDesktop);

    if (rc.Width() > rcDesktop.Width())
    {
        rc.right = rc.left + rcDesktop.Width();
    }
    if (rc.Height() > rcDesktop.Height())
    {
        rc.bottom = rc.top + rcDesktop.Height();
    }

    if (rc.left < 0)
    {
        rc.OffsetRect(-rc.left, 0);
    }
    if (rc.left > rcDesktop.right - visible)
    {
        rc.OffsetRect(-visible, 0);
    }

    if (rc.top < 0)
    {
        rc.OffsetRect(-rc.top, 0);
    }
    if (rc.top > rcDesktop.bottom - visible)
    {
        rc.OffsetRect(0, -visible);
    }

    rect = rc;
}

void COptions::SetTreeMapOptions(const CTreeMap::Options& options)
{
    TreeMapOptions = options;

    TreeMapStyle = static_cast<int>(TreeMapOptions.style);
    TreeMapGrid = TreeMapOptions.grid;
    TreeMapGridColor = TreeMapOptions.gridColor;
    TreeMapBrightness = TreeMapOptions.GetBrightnessPercent();
    TreeMapHeightFactor = TreeMapOptions.GetHeightPercent();
    TreeMapScaleFactor = TreeMapOptions.GetScaleFactorPercent();
    TreeMapAmbientLightPercent = TreeMapOptions.GetAmbientLightPercent();
    TreeMapLightSourceX = TreeMapOptions.GetLightSourceXPercent();
    TreeMapLightSourceY = TreeMapOptions.GetLightSourceYPercent();

    CDirStatDoc::Get()->UpdateAllViews(nullptr, HINT_TREEMAPSTYLECHANGED);
}

void COptions::CompileFilters()
{
    FilteringExcludeDirsRegex.clear();
    FilteringExcludeFilesRegex.clear();

    for (const auto & [optionString, optionRegex] : {
        std::pair{FilteringExcludeDirs.Obj(), std::ref(FilteringExcludeDirsRegex)},
        std::pair{FilteringExcludeFiles.Obj(), std::ref(FilteringExcludeFilesRegex)}})
    {
        for (auto& token : SplitString(optionString, L'\n'))
        {
            try
            {
                while (!token.empty() && (token.back() == L'\r' || token.back() == L'\\')) token.pop_back();
                optionRegex.get().emplace_back(FilteringUseRegex ? token : GlobToRegex(token),
                    std::regex_constants::icase | std::regex_constants::optimize);
            }
            catch (const std::regex_error&)
            {
                DisplayError(Localization::Lookup(IDS_PAGE_FILTERING_INVALID_FILTER) + L" " + token);
            }
        }
    }

    // Calculate the total number of bytes to test as a scan minimum
    FilteringSizeMinimumCalculated = static_cast<ULONGLONG>(FilteringSizeMinimum) * (1ull << (10 * FilteringSizeUnits));
}

void COptions::PreProcessPersistedSettings()
{
    // Reserve space so the copy/move constructors are not called
    UserDefinedCleanups.reserve(USERDEFINEDCLEANUPCOUNT);
    for (const int i : std::views::iota(0, USERDEFINEDCLEANUPCOUNT))
    {
        UserDefinedCleanups.emplace_back(L"Cleanups\\UserDefinedCleanup" + std::format(L"{:02}", i));
    }
}

void COptions::PostProcessPersistedSettings()
{
    // Adjust windows for sanity
    SanitizeRect(MainWindowPlacement.Obj().rcNormalPosition);
    SanitizeRect(AboutWindowRect.Obj());
    SanitizeRect(DriveSelectWindowRect.Obj());
    SanitizeRect(SearchWindowRect.Obj());

    // Compile filters, if any
    CompileFilters();

    // Set up the language for the environment
    if (const auto& languages = Localization::GetLanguageList();
        std::ranges::find(languages, static_cast<LANGID>(LanguageId)) == languages.end())
    {
        const LANGID specified = GetUserDefaultLangID();
        const LANGID generic = MAKELANGID(PRIMARYLANGID(specified), SUBLANG_NEUTRAL);
        LanguageId =
            languages.contains(specified) ? specified :
            languages.contains(generic) ? generic : MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL);
    }
    Localization::LoadResource(static_cast<LANGID>(LanguageId));

    // Load treemap settings
    TreeMapOptions.style = static_cast<CTreeMap::STYLE>(static_cast<int>(TreeMapStyle));
    TreeMapOptions.grid = TreeMapGrid;
    TreeMapOptions.gridColor = TreeMapGridColor;
    TreeMapOptions.SetBrightnessPercent(TreeMapBrightness);
    TreeMapOptions.SetHeightPercent(TreeMapHeightFactor);
    TreeMapOptions.SetScaleFactorPercent(TreeMapScaleFactor);
    TreeMapOptions.SetAmbientLightPercent(TreeMapAmbientLightPercent);
    TreeMapOptions.SetLightSourceXPercent(TreeMapLightSourceX);
    TreeMapOptions.SetLightSourceYPercent(TreeMapLightSourceY);

    // Adjust Title to language default Title
    for (const int i : std::views::iota(0, USERDEFINEDCLEANUPCOUNT))
    {
        if (UserDefinedCleanups[i].Title.Obj().empty() || UserDefinedCleanups[i].VirginTitle)
        {
            UserDefinedCleanups[i].Title = Localization::Format(IDS_USER_DEFINED_CLEANUPd, i);
        }
    }
}

LCID COptions::GetLocaleForFormatting()
{
    return UseWindowsLocaleSetting ? LOCALE_USER_DEFAULT :
        MAKELCID(LanguageId, SORT_DEFAULT);
}

void COptions::LoadAppSettings()
{
    PreProcessPersistedSettings();
    PersistedSetting::ReadPersistedProperties();
    PostProcessPersistedSettings();
}
