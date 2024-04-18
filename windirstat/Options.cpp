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

#include <format>

#include "Localization.h"

Setting<bool> COptions::ExcludeJunctions(L"options", L"excludeJunctions", true);
Setting<bool> COptions::ExcludeSymbolicLinks(L"options", L"excludeSymbolicLinks", true);
Setting<bool> COptions::ExcludeVolumeMountPoints(L"options", L"excludeVolumeMountPoints", true);
Setting<bool> COptions::FollowVolumeMountPoints(L"options", L"followVolumeMountPoint", false);
Setting<bool> COptions::HumanFormat(L"general", L"showTimeSpent", true);
Setting<bool> COptions::ListFullRowSelection(L"general", L"listFullRowSelection", true);
Setting<bool> COptions::ListGrid(L"general", L"treelistGrid", false);
Setting<bool> COptions::ListStripes(L"general", L"listStripes", false);
Setting<bool> COptions::PacmanAnimation(L"general", L"pacmanAnimation", true);
Setting<bool> COptions::ScanForDuplicates(L"driveselect", L"scanForDuplicates", false);
Setting<bool> COptions::ShowColumnAttributes(L"filetree", L"showColumnAttributes", false);
Setting<bool> COptions::ShowColumnFiles(L"filetree", L"showColumnFiles", true);
Setting<bool> COptions::ShowColumnFolders(L"filetree", L"showColumnFolders", false);
Setting<bool> COptions::ShowColumnItems(L"filetree", L"showColumnItems", false);
Setting<bool> COptions::ShowColumnLastChange(L"filetree", L"showColumnLastChange", true);
Setting<bool> COptions::ShowColumnOwner(L"filetree", L"showColumnOwner", false);
Setting<bool> COptions::ShowColumnSizeLogical(L"filetree", L"showColumnSizeLogical", true);
Setting<bool> COptions::ShowColumnSizePhysical(L"filetree", L"showColumnSizePhysical", true);
Setting<bool> COptions::ShowDeleteWarning(L"general", L"showDeleteWarning", true);
Setting<bool> COptions::ShowFileTypes(L"general", L"showFileTypes", true);
Setting<bool> COptions::ShowFreeSpace(L"general", L"showFreeSpace", false);
Setting<bool> COptions::ShowStatusbar(L"general", L"showStatusbar", true);
Setting<bool> COptions::ShowTimeSpent(L"filetree", L"humanFormat", true);
Setting<bool> COptions::ShowToolbar(L"general", L"showToolbar", true);
Setting<bool> COptions::ShowTreemap(L"appearance", L"showTreemap", true);
Setting<bool> COptions::ShowUnknown(L"general", L"showUnknown", false);
Setting<bool> COptions::SkipDupeDetectionCloudLinks(L"options", L"skipDupeDetectionCloudLinks", true);
Setting<bool> COptions::SkipHidden(L"scans", L"skipHidden", false);
Setting<bool> COptions::SkipProtected(L"scans", L"skipProtected", true);
Setting<bool> COptions::TreeMapGrid(L"treemap", L"treemapGrid", (CTreemap::GetDefaultOptions().grid));
Setting<bool> COptions::UseBackupRestore(L"options", L"useBackupRestore", true);
Setting<bool> COptions::UseFallbackLocale(L"general", L"useFallbackLocale", false);
Setting<COLORREF> COptions::TreeListColor0(L"filetree", L"treeListColor0", RGB(64, 64, 140));
Setting<COLORREF> COptions::TreeListColor1(L"filetree", L"treeListColor1", RGB(140, 64, 64));
Setting<COLORREF> COptions::TreeListColor2(L"filetree", L"treeListColor2", RGB(64, 140, 64));
Setting<COLORREF> COptions::TreeListColor3(L"filetree", L"treeListColor3", RGB(140, 140, 64));
Setting<COLORREF> COptions::TreeListColor4(L"filetree", L"treeListColor4", RGB(0, 0, 255));
Setting<COLORREF> COptions::TreeListColor5(L"filetree", L"treeListColor5", RGB(255, 0, 0));
Setting<COLORREF> COptions::TreeListColor6(L"filetree", L"treeListColor6", RGB(0, 255, 0));
Setting<COLORREF> COptions::TreeListColor7(L"filetree", L"treeListColor7", RGB(255, 255, 0));
Setting<COLORREF> COptions::TreeMapGridColor(L"treemap", L"treemapGridColor", CTreemap::GetDefaultOptions().gridColor);
Setting<COLORREF> COptions::TreeMapHighlightColor(L"treemap", L"treemapHighlightColor", RGB(255, 255, 255));
Setting<double> COptions::MainSplitterPos(L"general", L"MainSplitterPos", -1.0, 0.0, 1.0);
Setting<double> COptions::SubSplitterPos(L"general", L"SubSplitterPos", -1.0, 0.0, 1.0);
Setting<int> COptions::ConfigPage(L"general", L"configPage", true);
Setting<int> COptions::LanguageId(L"general", L"language", 0);
Setting<int> COptions::ScanningThreads(L"options", L"scanningThreads", 6, 1, 16);
Setting<int> COptions::SelectDrivesRadio(L"driveselect", L"selectDrivesRadio", 0, 0, 2);
Setting<int> COptions::TreeListColorCount(L"filetree", L"treeListColorCount", 8);
Setting<int> COptions::TreeMapAmbientLightPercent(L"treemap", L"ambientLight", CTreemap::GetDefaultOptions().GetAmbientLightPercent(), 0, 100);
Setting<int> COptions::TreeMapBrightness(L"treemap", L"brightness", CTreemap::GetDefaultOptions().GetBrightnessPercent(), 0, 100);
Setting<int> COptions::TreeMapHeightFactor(L"treemap", L"heightFactor", CTreemap::GetDefaultOptions().GetHeightPercent(), 0, 100);
Setting<int> COptions::TreeMapLightSourceX(L"treemap", L"lightSourceX", CTreemap::GetDefaultOptions().GetLightSourceXPercent(), -200, 200);
Setting<int> COptions::TreeMapLightSourceY(L"treemap", L"lightSourceY", CTreemap::GetDefaultOptions().GetLightSourceYPercent(), -200, 200);
Setting<int> COptions::TreeMapScaleFactor(L"treemap", L"scaleFactor", CTreemap::GetDefaultOptions().GetScaleFactorPercent(), 0, 100);
Setting<int> COptions::TreeMapStyle(L"treemap", L"treemapStyle", CTreemap::GetDefaultOptions().style, 0, 1);
Setting<RECT> COptions::AboutWindowRect(L"general", L"aboutWindowRect");
Setting<RECT> COptions::DriveWindowRect(L"general", L"driveWindowRect");
Setting<std::vector<int>> COptions::DriveListColumnOrder(L"driveselect", L"driveListColumnWidths");
Setting<std::vector<int>> COptions::DriveListColumnWidths(L"driveselect", L"driveListColumnOrder");
Setting<std::vector<int>> COptions::DupeTreeColumnOrder(L"appearance", L"dupeTreeColumnOrder");
Setting<std::vector<int>> COptions::DupeTreeColumnWidths(L"appearance", L"dupeTreeColumnWidths");
Setting<std::vector<int>> COptions::FileTreeColumnOrder(L"filetree", L"fileTreeColumnOrder");
Setting<std::vector<int>> COptions::FileTreeColumnWidths(L"filetree", L"fileTreeColumnWidths");
Setting<std::vector<int>> COptions::TypesColumnOrder(L"appearance", L"typesColumnOrder");
Setting<std::vector<int>> COptions::TypesColumnWidths(L"appearance", L"typesColumnWidths");
Setting<std::vector<std::wstring>> COptions::SelectDrivesDrives(L"driveselect", L"selectDrivesDrives");
Setting<std::wstring> COptions::SelectDrivesFolder(L"driveselect", L"selectDrivesFolder");
Setting<WINDOWPLACEMENT> COptions::MainWindowPlacement(L"general", L"mainWindowPlacement");

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
        UserDefinedCleanups.emplace_back(L"cleanups\\userDefinedCleanup" + std::format(L"{:02}", i));
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
            s.FormatMessage(Localization::Lookup(IDS_USERDEFINEDCLEANUPd), i);
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
