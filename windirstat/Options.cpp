// Options.cpp - Implementation of COptions, COptions and CRegistryUser
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

#include "stdafx.h"
#include "WinDirStat.h"
#include "DirStatDoc.h"
#include "Options.h"
#include "Property.h"
#include "Localization.h"

#include <format>

Setting<bool> COptions::ExcludeJunctions(L"Options", L"ExcludeJunctions", true);
Setting<bool> COptions::ExcludeSymbolicLinks(L"Options", L"ExcludeSymbolicLinks", true);
Setting<bool> COptions::ExcludeVolumeMountPoints(L"Options", L"ExcludeVolumeMountPoints", true);
Setting<bool> COptions::FollowVolumeMountPoints(L"Options", L"FollowVolumeMountPoints", false);
Setting<bool> COptions::HumanFormat(L"General", L"HumanFormat", true);
Setting<bool> COptions::ListFullRowSelection(L"General", L"ListFullRowSelection", true);
Setting<bool> COptions::ListGrid(L"General", L"ListGrid", false);
Setting<bool> COptions::ListStripes(L"General", L"ListStripes", false);
Setting<bool> COptions::PacmanAnimation(L"General", L"PacmanAnimation", true);
Setting<bool> COptions::ScanForDuplicates(L"DriveSelect", L"ScanForDuplicates", false);
Setting<bool> COptions::ShowColumnAttributes(L"FileTree", L"ShowColumnAttributes", false);
Setting<bool> COptions::ShowColumnFiles(L"FileTree", L"ShowColumnFiles", true);
Setting<bool> COptions::ShowColumnFolders(L"FileTree", L"ShowColumnFolders", false);
Setting<bool> COptions::ShowColumnItems(L"FileTree", L"ShowColumnItems", false);
Setting<bool> COptions::ShowColumnLastChange(L"FileTree", L"ShowColumnLastChange", true);
Setting<bool> COptions::ShowColumnOwner(L"FileTree", L"ShowColumnOwner", false);
Setting<bool> COptions::ShowColumnSizeLogical(L"FileTree", L"ShowColumnSizeLogical", true);
Setting<bool> COptions::ShowColumnSizePhysical(L"FileTree", L"ShowColumnSizePhysical", true);
Setting<bool> COptions::ShowDeleteWarning(L"General", L"ShowDeleteWarning", true);
Setting<bool> COptions::ShowFileTypes(L"General", L"ShowFileTypes", true);
Setting<bool> COptions::ShowFreeSpace(L"General", L"ShowFreeSpace", false);
Setting<bool> COptions::ShowStatusbar(L"General", L"ShowStatusbar", true);
Setting<bool> COptions::ShowTimeSpent(L"FileTree", L"ShowTimeSpent", true);
Setting<bool> COptions::ShowToolbar(L"General", L"ShowToolbar", true);
Setting<bool> COptions::ShowTreemap(L"Appearance", L"ShowTreemap", true);
Setting<bool> COptions::ShowUnknown(L"General", L"ShowUnknown", false);
Setting<bool> COptions::SkipDupeDetectionCloudLinks(L"Options", L"SkipDupeDetectionCloudLinks", true);
Setting<bool> COptions::SkipHidden(L"scans", L"SkipHidden", false);
Setting<bool> COptions::SkipProtected(L"scans", L"SkipProtected", true);
Setting<bool> COptions::TreeMapGrid(L"TreeMap", L"TreeMapGrid", (CTreemap::GetDefaultOptions().grid));
Setting<bool> COptions::UseBackupRestore(L"Options", L"UseBackupRestore", true);
Setting<bool> COptions::UseFallbackLocale(L"General", L"UseFallbackLocale", false);
Setting<COLORREF> COptions::FileTreeColor0(L"FileTree", L"FileTreeColor0", RGB(64, 64, 140));
Setting<COLORREF> COptions::FileTreeColor1(L"FileTree", L"FileTreeColor1", RGB(140, 64, 64));
Setting<COLORREF> COptions::FileTreeColor2(L"FileTree", L"FileTreeColor2", RGB(64, 140, 64));
Setting<COLORREF> COptions::FileTreeColor3(L"FileTree", L"FileTreeColor3", RGB(140, 140, 64));
Setting<COLORREF> COptions::FileTreeColor4(L"FileTree", L"FileTreeColor4", RGB(0, 0, 255));
Setting<COLORREF> COptions::FileTreeColor5(L"FileTree", L"FileTreeColor5", RGB(255, 0, 0));
Setting<COLORREF> COptions::FileTreeColor6(L"FileTree", L"FileTreeColor6", RGB(0, 255, 0));
Setting<COLORREF> COptions::FileTreeColor7(L"FileTree", L"FileTreeColor7", RGB(255, 255, 0));
Setting<COLORREF> COptions::TreeMapGridColor(L"TreeMap", L"TreeMapGridColor", CTreemap::GetDefaultOptions().gridColor);
Setting<COLORREF> COptions::TreeMapHighlightColor(L"TreeMap", L"TreeMapHighlightColor", RGB(255, 255, 255));
Setting<double> COptions::MainSplitterPos(L"General", L"MainSplitterPos", -1.0, 0.0, 1.0);
Setting<double> COptions::SubSplitterPos(L"General", L"SubSplitterPos", -1.0, 0.0, 1.0);
Setting<int> COptions::ConfigPage(L"General", L"ConfigPage", true);
Setting<int> COptions::LanguageId(L"General", L"LanguageId", 0);
Setting<int> COptions::ScanningThreads(L"Options", L"ScanningThreads", 6, 1, 16);
Setting<int> COptions::SelectDrivesRadio(L"DriveSelect", L"SelectDrivesRadio", 0, 0, 2);
Setting<int> COptions::FileTreeColorCount(L"FileTree", L"FileTreeColorCount", 8);
Setting<int> COptions::TreeMapAmbientLightPercent(L"TreeMap", L"TreeMapAmbientLightPercent", CTreemap::GetDefaultOptions().GetAmbientLightPercent(), 0, 100);
Setting<int> COptions::TreeMapBrightness(L"TreeMap", L"TreeMapBrightness", CTreemap::GetDefaultOptions().GetBrightnessPercent(), 0, 100);
Setting<int> COptions::TreeMapHeightFactor(L"TreeMap", L"TreeMapHeightFactor", CTreemap::GetDefaultOptions().GetHeightPercent(), 0, 100);
Setting<int> COptions::TreeMapLightSourceX(L"TreeMap", L"TreeMapLightSourceX", CTreemap::GetDefaultOptions().GetLightSourceXPercent(), -200, 200);
Setting<int> COptions::TreeMapLightSourceY(L"TreeMap", L"TreeMapLightSourceY", CTreemap::GetDefaultOptions().GetLightSourceYPercent(), -200, 200);
Setting<int> COptions::TreeMapScaleFactor(L"TreeMap", L"TreeMapScaleFactor", CTreemap::GetDefaultOptions().GetScaleFactorPercent(), 0, 100);
Setting<int> COptions::TreeMapStyle(L"TreeMap", L"TreeMapStyle", CTreemap::GetDefaultOptions().style, 0, 1);
Setting<RECT> COptions::AboutWindowRect(L"Appearance", L"AboutWindowRect");
Setting<RECT> COptions::DriveWindowRect(L"Appearance", L"DriveWindowRect");
Setting<std::vector<int>> COptions::DriveListColumnOrder(L"DriveSelect", L"DriveListColumnOrder");
Setting<std::vector<int>> COptions::DriveListColumnWidths(L"DriveSelect", L"DriveListColumnWidths");
Setting<std::vector<int>> COptions::DupeTreeColumnOrder(L"Appearance", L"DupeTreeColumnOrder");
Setting<std::vector<int>> COptions::DupeTreeColumnWidths(L"Appearance", L"DupeTreeColumnWidths");
Setting<std::vector<int>> COptions::FileTreeColumnOrder(L"FileTree", L"FileTreeColumnOrder");
Setting<std::vector<int>> COptions::FileTreeColumnWidths(L"FileTree", L"FileTreeColumnWidths");
Setting<std::vector<int>> COptions::TypesColumnOrder(L"Appearance", L"TypesColumnOrder");
Setting<std::vector<int>> COptions::TypesColumnWidths(L"Appearance", L"TypesColumnWidths");
Setting<std::vector<std::wstring>> COptions::SelectDrivesDrives(L"DriveSelect", L"SelectDrivesDrives");
Setting<std::wstring> COptions::SelectDrivesFolder(L"DriveSelect", L"SelectDrivesFolder");
Setting<WINDOWPLACEMENT> COptions::MainWindowPlacement(L"General", L"MainWindowPlacement");

CTreemap::Options COptions::TreemapOptions;
std::vector<USERDEFINEDCLEANUP> COptions::UserDefinedCleanups;

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

void COptions::SetTreemapOptions(const CTreemap::Options& options)
{
    TreemapOptions = options;

    TreeMapStyle = static_cast<int>(TreemapOptions.style);
    TreeMapGrid = TreemapOptions.grid;
    TreeMapGridColor = TreemapOptions.gridColor;
    TreeMapBrightness = TreemapOptions.GetBrightnessPercent();
    TreeMapHeightFactor = TreemapOptions.GetHeightPercent();
    TreeMapScaleFactor = TreemapOptions.GetScaleFactorPercent();
    TreeMapAmbientLightPercent = TreemapOptions.GetAmbientLightPercent();
    TreeMapLightSourceX = TreemapOptions.GetLightSourceXPercent();
    TreeMapLightSourceY = TreemapOptions.GetLightSourceYPercent();

    GetDocument()->UpdateAllViews(nullptr, HINT_TREEMAPSTYLECHANGED);
}

void COptions::PreProcessPersistedSettings()
{
    // Reserve space so the copy/move constructors are not called
    UserDefinedCleanups.reserve(USERDEFINEDCLEANUPCOUNT);
    for (int i = 0; i < USERDEFINEDCLEANUPCOUNT; i++)
    {
        UserDefinedCleanups.emplace_back(L"Cleanups\\UserDefinedCleanup" + std::format(L"{:02}", i));
    }
}

void COptions::PostProcessPersistedSettings()
{
    // Adjust windows for sanity
    SanitizeRect(MainWindowPlacement.Obj().rcNormalPosition);
    SanitizeRect(AboutWindowRect.Obj());
    SanitizeRect(DriveWindowRect.Obj());

    // Setup the language for the environment
    const LANGID langid = static_cast<LANGID>(LanguageId);
    const auto& languages = Localization::GetLanguageList();
    if (std::ranges::find(languages, langid) == languages.end())
    {
        const LANGID best = MAKELANGID(PRIMARYLANGID(GetUserDefaultLangID()), SUBLANG_NEUTRAL);
        LanguageId.Obj() = (std::ranges::find(languages, best) != languages.end()) ? best : GetFallbackLanguage();
    }
    Localization::LoadResource(static_cast<LANGID>(LanguageId));

    // Load treemap settings 
    TreemapOptions.style = static_cast<CTreemap::STYLE>(static_cast<int>(TreeMapStyle));
    TreemapOptions.grid = TreeMapGrid;
    TreemapOptions.gridColor = TreeMapGridColor;
    TreemapOptions.SetBrightnessPercent(TreeMapBrightness);
    TreemapOptions.SetHeightPercent(TreeMapHeightFactor);
    TreemapOptions.SetScaleFactorPercent(TreeMapScaleFactor);
    TreemapOptions.SetAmbientLightPercent(TreeMapAmbientLightPercent);
    TreemapOptions.SetLightSourceXPercent(TreeMapLightSourceX);
    TreemapOptions.SetLightSourceYPercent(TreeMapLightSourceY);

    // Adjust title to language default title
    for (int i = 0; i < USERDEFINEDCLEANUPCOUNT; i++)
    {
        if (UserDefinedCleanups[i].title.Obj().empty() || UserDefinedCleanups[i].virginTitle)
        {
            CStringW s;
            s.FormatMessage(Localization::Lookup(IDS_USERDEFINEDCLEANUPd).c_str(), i);
            UserDefinedCleanups[i].title = std::wstring(s.GetString());
        }
    }
}

LANGID COptions::GetFallbackLanguage()
{
    return MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL);
}

LANGID COptions::GetEffectiveLangId()
{
    if (UseFallbackLocale)
    {
        return GetFallbackLanguage();
    }

    return static_cast<LANGID>(LanguageId);
}

void COptions::LoadAppSettings()
{
    PreProcessPersistedSettings();
    PersistedSetting::ReadPersistedProperties();
    PostProcessPersistedSettings();
}
