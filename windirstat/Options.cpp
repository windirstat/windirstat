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
#include <common/CommonHelpers.h>
#include "Options.h"
#include "Property.h"

#include <format>

Setting<bool> COptions::ShowFreeSpace(L"persistence", L"showFreeSpace", false);
Setting<bool> COptions::ShowUnknown(L"persistence", L"showUnknown", false);
Setting<bool> COptions::ShowFileTypes(L"persistence", L"showFileTypes", true);
Setting<bool> COptions::ShowToolbar(L"persistence", L"showToolbar", true);
Setting<bool> COptions::ShowTreemap(L"persistence", L"showTreemap", true);
Setting<bool> COptions::ShowStatusbar(L"persistence", L"showStatusbar", true);
Setting<bool> COptions::ShowDeleteWarning(L"persistence", L"showDeleteWarning", true);
Setting<int> COptions::ConfigPage(L"persistence", L"configPage", true);
Setting<WINDOWPLACEMENT> COptions::MainWindowPlacement(L"persistence", L"mainWindowPlacement");
Setting<RECT> COptions::AboutWindowRect(L"persistence", L"aboutWindowRect");
Setting<RECT> COptions::DriveWindowRect(L"persistence", L"driveWindowRect");
Setting<int> COptions::SelectDrivesRadio(L"persistence", L"selectDrivesRadio", 0, 0, 2);
Setting<std::wstring> COptions::SelectDrivesFolder(L"persistence", L"selectDrivesFolder");
Setting<std::vector<std::wstring>> COptions::SelectDrivesDrives(L"persistence", L"selectDrivesDrives");
Setting<double> COptions::MainSplitterPos(L"persistence", L"MainSplitterPos", -1, 0, 1);
Setting<double> COptions::SubSplitterPos(L"persistence", L"SubSplitterPos", -1, 0, 1);
Setting<int> COptions::LanguageId(L"persistence", L"language", LANGIDFROMLCID(GetUserDefaultLCID()));

Setting<std::vector<int>> COptions::TypesColumnWidths(L"persistence", L"typesColumnWidths");
Setting<std::vector<int>> COptions::TypesColumnOrder(L"persistence", L"typesColumnOrder");
Setting<std::vector<int>> COptions::TreeListColumnWidths(L"persistence", L"treelistColumnWidths");
Setting<std::vector<int>> COptions::TreeListColumnOrder(L"persistence", L"treelistColumnOrder");
Setting<std::vector<int>> COptions::DriveListColumnWidths(L"persistence", L"drivelistColumnOrder");
Setting<std::vector<int>> COptions::DriveListColumnOrder(L"persistence", L"drivelistColumnWidths");

Setting<bool> COptions::PacmanAnimation(L"options", L"pacmanAnimation", true);
Setting<bool> COptions::ShowTimeSpent(L"options", L"humanFormat", true);
Setting<bool> COptions::HumanFormat(L"options", L"showTimeSpent", true);
Setting<bool> COptions::FollowMountPoints(L"options", L"followMountPoint", false);
Setting<bool> COptions::FollowJunctionPoints(L"options", L"followJunctionPoints", false);
Setting<bool> COptions::UseBackupRestore(L"options", L"useBackupRestore", true);
Setting<bool> COptions::ShowUncompressedFileSizes(L"options", L"showUncompressedFileSizes", false);
Setting<int> COptions::ScanningThreads(L"options", L"scanningThreads", 4, 1, 16);

Setting<bool> COptions::SkipHidden(L"options", L"skipHidden", false);
Setting<bool> COptions::UseWdsLocale(L"options", L"useWdsLocale", false);
Setting<bool> COptions::ListGrid(L"options", L"treelistGrid", false);
Setting<bool> COptions::ListStripes(L"options", L"listStripes", false);
Setting<bool> COptions::ListFullRowSelection(L"options", L"listFullRowSelection", true);

Setting<bool> COptions::ShowColumnSubdirs(L"options", L"showColumnSubdirs", true);
Setting<bool> COptions::ShowColumnItems(L"options", L"showUnknownhowColumnItems", true);
Setting<bool> COptions::ShowColumnFiles(L"options", L"showColumnFiles", true);
Setting<bool> COptions::ShowColumnAttributes(L"options", L"showColumnAttributes", true);
Setting<bool> COptions::ShowColumnLastChange(L"options", L"showColumnLastChange", true);
Setting<bool> COptions::ShowColumnOwner(L"options", L"showColumnOwner", false);

Setting<COLORREF> COptions::TreeListColor0(L"options", L"treelistColor0", RGB(64, 64, 140));
Setting<COLORREF> COptions::TreeListColor1(L"options", L"treelistColor1", RGB(140, 64, 64));
Setting<COLORREF> COptions::TreeListColor2(L"options", L"treelistColor2", RGB(64, 140, 64));
Setting<COLORREF> COptions::TreeListColor3(L"options", L"treelistColor3", RGB(140, 140, 64));
Setting<COLORREF> COptions::TreeListColor4(L"options", L"treelistColor4", RGB(0, 0, 255));
Setting<COLORREF> COptions::TreeListColor5(L"options", L"treelistColor5", RGB(255, 0, 0));
Setting<COLORREF> COptions::TreeListColor6(L"options", L"treelistColor6", RGB(0, 255, 0));
Setting<COLORREF> COptions::TreeListColor7(L"options", L"treelistColor7", RGB(255, 255, 0));
Setting<int> COptions::TreeListColorCount(L"options", L"treelistColorCount", 8);

Setting<int> COptions::TreeMapStyle(L"options", L"treemapStyle", CTreemap::GetDefaultOptions().style, 0, 1);
Setting<int> COptions::TreeMapBrightness(L"options", L"brightness", CTreemap::GetDefaultOptions().GetBrightnessPercent(), 0, 100);
Setting<int> COptions::TreeMapHeightFactor(L"options", L"heightFactor", CTreemap::GetDefaultOptions().GetHeightPercent(), 0, 100);
Setting<int> COptions::TreeMapScaleFactor(L"options", L"scaleFactor", CTreemap::GetDefaultOptions().GetScaleFactorPercent(), 0, 100);
Setting<int> COptions::TreeMapAmbientLightPercent(L"options", L"ambientLight", CTreemap::GetDefaultOptions().GetAmbientLightPercent(), 0, 100);
Setting<int> COptions::TreeMapLightSourceX(L"options", L"lightSourceX", CTreemap::GetDefaultOptions().GetLightSourceXPercent(), -200, 200);
Setting<int> COptions::TreeMapLightSourceY(L"options", L"lightSourceY", CTreemap::GetDefaultOptions().GetLightSourceYPercent(), -200, 200);
Setting<bool> COptions::TreeMapGrid(L"options", L"treemapGrid", (CTreemap::GetDefaultOptions().grid));
Setting<COLORREF> COptions::TreeMapGridColor(L"options", L"treemapGridColor", CTreemap::GetDefaultOptions().gridColor);
Setting<COLORREF> COptions::TreeMapHighlightColor(L"options", L"treemapHighlightColor", RGB(255, 255, 255));

Setting<std::wstring> COptions::ReportSubject(L"options", L"reportSubject");
Setting<std::wstring> COptions::ReportSuffix(L"options", L"reportSuffix");
Setting<std::wstring> COptions::ReportPrefix(L"options", L"reportPrefix");

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

        // Add default title
        CStringW s;
        s.FormatMessage(LoadString(IDS_USERDEFINEDCLEANUPd), i);
        UserDefinedCleanups[i].title = std::wstring(s.GetString());
    }

    // Set defaults for reports
    ReportSubject.Obj() = LoadString(IDS_REPORT_DISKUSAGE).GetString();
    ReportSuffix.Obj() = LoadString(IDS_DISKUSAGEREPORTGENERATEDBYWINDIRSTAT).GetString();
    ReportPrefix.Obj() = LoadString(IDS_PLEASECHECKYOURDISKUSAGE).GetString();

}

void COptions::PostProcessPersistedSettings()
{
    // Adjust windows for sanity
    SanitizeRect(MainWindowPlacement.Obj().rcNormalPosition);
    SanitizeRect(AboutWindowRect.Obj());
    SanitizeRect(DriveWindowRect.Obj());

    // Validate locale is valie
    if (!IsValidLocale(MAKELCID(LanguageId, SORT_DEFAULT), LCID_INSTALLED))
    {
        LanguageId.Obj() = GetUserDefaultLangID();
    }

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
}

LANGID COptions::GetBuiltInLanguage()
{
    return MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
}

LANGID COptions::GetEffectiveLangId()
{
    if (UseWdsLocale)
    {
        return GetBuiltInLanguage();
    }

    return static_cast<LANGID>(LanguageId);
}

void COptions::LoadAppSettings()
{
    // Load settings from persisted store
    PreProcessPersistedSettings();
    PersistedSetting::ReadPersistedProperties();

    // Setup the language for the environment
    LANGID langid = static_cast<LANGID>(COptions::LanguageId.Obj());
    if (langid != GetBuiltInLanguage())
    {
        const CStringW resourceDllPath = CDirStatApp::FindResourceDllPathByLangid(langid);
        if (!resourceDllPath.IsEmpty())
        {
            // Load language resource DLL
            if (const HINSTANCE dll = ::LoadLibrary(resourceDllPath))
            {
                // Set default module handle for loading of resources
                AfxSetResourceHandle(dll);
                COptions::LanguageId = static_cast<int>(langid);
            }
            else
            {
                VTRACE(L"LoadLibrary(%s) failed: %u", resourceDllPath.GetString(), ::GetLastError());
            }
        }
    }

    // Post-process settings now that language is loaded
    PostProcessPersistedSettings();
}
