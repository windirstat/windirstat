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

Setting<bool> COptions::ShowFreeSpace(L"appearance", L"showFreeSpace", false);
Setting<bool> COptions::ShowUnknown(L"appearance", L"showUnknown", false);
Setting<bool> COptions::ShowFileTypes(L"appearance", L"showFileTypes", true);
Setting<bool> COptions::ShowToolbar(L"appearance", L"showToolbar", true);
Setting<bool> COptions::ShowStatusbar(L"appearance", L"showStatusbar", true);
Setting<bool> COptions::ShowDeleteWarning(L"appearance", L"showDeleteWarning", true);
Setting<int> COptions::ConfigPage(L"appearance", L"configPage", true);
Setting<WINDOWPLACEMENT> COptions::MainWindowPlacement(L"appearance", L"mainWindowPlacement");
Setting<RECT> COptions::AboutWindowRect(L"appearance", L"aboutWindowRect");
Setting<RECT> COptions::DriveWindowRect(L"appearance", L"driveWindowRect");
Setting<double> COptions::MainSplitterPos(L"appearance", L"MainSplitterPos", -1.0, 0.0, 1.0);
Setting<double> COptions::SubSplitterPos(L"appearance", L"SubSplitterPos", -1.0, 0.0, 1.0);
Setting<int> COptions::LanguageId(L"appearance", L"language", 0);

Setting<std::vector<int>> COptions::TypesColumnWidths(L"appearance", L"typesColumnWidths");
Setting<std::vector<int>> COptions::TypesColumnOrder(L"appearance", L"typesColumnOrder");
Setting<std::vector<int>> COptions::FileTreeColumnWidths(L"appearance", L"fileTreeColumnWidths");
Setting<std::vector<int>> COptions::FileTreeColumnOrder(L"appearance", L"fileTreeColumnOrder");
Setting<std::vector<int>> COptions::DriveListColumnWidths(L"appearance", L"driveListColumnOrder");
Setting<std::vector<int>> COptions::DriveListColumnOrder(L"appearance", L"driveListColumnWidths");
Setting<std::vector<int>> COptions::DupeTreeColumnWidths(L"appearance", L"dupeTreeColumnWidths");
Setting<std::vector<int>> COptions::DupeTreeColumnOrder(L"appearance", L"dupeTreeColumnOrder");

Setting<int> COptions::SelectDrivesRadio(L"options", L"selectDrivesRadio", 0, 0, 2);
Setting<std::wstring> COptions::SelectDrivesFolder(L"options", L"selectDrivesFolder");
Setting<std::vector<std::wstring>> COptions::SelectDrivesDrives(L"options", L"selectDrivesDrives");
Setting<bool> COptions::ScanForDuplicates(L"options", L"scanForDuplicates", false);
Setting<bool> COptions::PacmanAnimation(L"options", L"pacmanAnimation", true);
Setting<bool> COptions::ShowTimeSpent(L"options", L"humanFormat", true);
Setting<bool> COptions::HumanFormat(L"options", L"showTimeSpent", true);
Setting<bool> COptions::SkipDuplicationDetectionCloudLinks(L"options", L"skipDuplicationDetectionCloudLinks", true);
Setting<bool> COptions::ExcludeVolumeMountPoints(L"options", L"excludeVolumeMountPoints", true);
Setting<bool> COptions::ExcludeJunctions(L"options", L"excludeJunctions", true);
Setting<bool> COptions::ExcludeSymbolicLinks(L"options", L"excludeSymbolicLinks", true);
Setting<bool> COptions::FollowVolumeMountPoints(L"options", L"followVolumeMountPoint", false);
Setting<bool> COptions::UseBackupRestore(L"options", L"useBackupRestore", true);
Setting<int> COptions::ScanningThreads(L"options", L"scanningThreads", 6, 1, 16);

Setting<bool> COptions::SkipHidden(L"options", L"skipHidden", false);
Setting<bool> COptions::SkipProtected(L"options", L"skipProtected", true);
Setting<bool> COptions::UseFallbackLocale(L"options", L"useFallbackLocale", false);
Setting<bool> COptions::ListGrid(L"options", L"treelistGrid", false);
Setting<bool> COptions::ListStripes(L"options", L"listStripes", false);
Setting<bool> COptions::ListFullRowSelection(L"options", L"listFullRowSelection", true);

Setting<bool> COptions::ShowColumnFolders(L"appearance", L"showColumnFolders", false);
Setting<bool> COptions::ShowColumnItems(L"appearance", L"showColumnItems", false);
Setting<bool> COptions::ShowColumnFiles(L"appearance", L"showColumnFiles", true);
Setting<bool> COptions::ShowColumnAttributes(L"appearance", L"showColumnAttributes", false);
Setting<bool> COptions::ShowColumnLastChange(L"appearance", L"showColumnLastChange", true);
Setting<bool> COptions::ShowColumnOwner(L"appearance", L"showColumnOwner", false);
Setting<bool> COptions::ShowColumnSizePhysical(L"appearance", L"showColumnSizePhysical", true);
Setting<bool> COptions::ShowColumnSizeLogical(L"appearance", L"showColumnSizeLogical", true);

Setting<COLORREF> COptions::TreeListColor0(L"options", L"treeListColor0", RGB(64, 64, 140));
Setting<COLORREF> COptions::TreeListColor1(L"options", L"treeListColor1", RGB(140, 64, 64));
Setting<COLORREF> COptions::TreeListColor2(L"options", L"treeListColor2", RGB(64, 140, 64));
Setting<COLORREF> COptions::TreeListColor3(L"options", L"treeListColor3", RGB(140, 140, 64));
Setting<COLORREF> COptions::TreeListColor4(L"options", L"treeListColor4", RGB(0, 0, 255));
Setting<COLORREF> COptions::TreeListColor5(L"options", L"treeListColor5", RGB(255, 0, 0));
Setting<COLORREF> COptions::TreeListColor6(L"options", L"treeListColor6", RGB(0, 255, 0));
Setting<COLORREF> COptions::TreeListColor7(L"options", L"treeListColor7", RGB(255, 255, 0));
Setting<int> COptions::TreeListColorCount(L"options", L"treeListColorCount", 8);

Setting<bool> COptions::ShowTreemap(L"appearance", L"showTreemap", true);
Setting<int> COptions::TreeMapStyle(L"treemap", L"treemapStyle", CTreemap::GetDefaultOptions().style, 0, 1);
Setting<int> COptions::TreeMapBrightness(L"treemap", L"brightness", CTreemap::GetDefaultOptions().GetBrightnessPercent(), 0, 100);
Setting<int> COptions::TreeMapHeightFactor(L"treemap", L"heightFactor", CTreemap::GetDefaultOptions().GetHeightPercent(), 0, 100);
Setting<int> COptions::TreeMapScaleFactor(L"treemap", L"scaleFactor", CTreemap::GetDefaultOptions().GetScaleFactorPercent(), 0, 100);
Setting<int> COptions::TreeMapAmbientLightPercent(L"treemap", L"ambientLight", CTreemap::GetDefaultOptions().GetAmbientLightPercent(), 0, 100);
Setting<int> COptions::TreeMapLightSourceX(L"treemap", L"lightSourceX", CTreemap::GetDefaultOptions().GetLightSourceXPercent(), -200, 200);
Setting<int> COptions::TreeMapLightSourceY(L"treemap", L"lightSourceY", CTreemap::GetDefaultOptions().GetLightSourceYPercent(), -200, 200);
Setting<bool> COptions::TreeMapGrid(L"treemap", L"treemapGrid", (CTreemap::GetDefaultOptions().grid));
Setting<COLORREF> COptions::TreeMapGridColor(L"treemap", L"treemapGridColor", CTreemap::GetDefaultOptions().gridColor);
Setting<COLORREF> COptions::TreeMapHighlightColor(L"treemap", L"treemapHighlightColor", RGB(255, 255, 255));

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
