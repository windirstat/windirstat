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
#include "Filtering.h"
#include "Property.h"

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
    TreeMapShowExtensions = TreeMapOptions.showExtensions;
    TreeMapShowFolderFrames = TreeMapOptions.showFolderFrames;
    TreeMapFolderFramesDrawThreshold = TreeMapOptions.folderFramesDrawThreshold;
    TreeMapGridColor = TreeMapOptions.gridColor;
    TreeMapBrightness = TreeMapOptions.GetBrightnessPercent();
    TreeMapHeightFactor = TreeMapOptions.GetHeightPercent();
    TreeMapScaleFactor = TreeMapOptions.GetScaleFactorPercent();
    TreeMapAmbientLightPercent = TreeMapOptions.GetAmbientLightPercent();
    TreeMapLightSourceX = TreeMapOptions.GetLightSourceXPercent();
    TreeMapLightSourceY = TreeMapOptions.GetLightSourceYPercent();

    CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_TREEMAP_STYLE);
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
    // File-tree visibility is also consumed by non-UI exports, so initialize its defaults before any view exists.
    if (auto& visibility = FileTreeColumnVisibility.Obj(); visibility.empty())
    {
        visibility = { 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0 };
    }

    // Adjust windows for sanity
    SanitizeRect(MainWindowPlacement.Obj().rcNormalPosition);
    SanitizeRect(AboutWindowRect.Obj());
    SanitizeRect(DriveSelectWindowRect.Obj());
    SanitizeRect(SearchWindowRect.Obj());

    // Compile filters, if any
    CFiltering::CompileFilters();

    // Set up the language for the environment
    if (const auto& languages = Localization::GetLanguageList();
        !languages.contains(static_cast<LANGID>(LanguageId)))
    {
        const LANGID specified = GetUserDefaultLangID();
        const LANGID generic = MAKELANGID(PRIMARYLANGID(specified), SUBLANG_NEUTRAL);
        LanguageId =
            languages.contains(specified) ? specified :
            languages.contains(generic) ? generic : MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL);
    }
    Localization::LoadResource(static_cast<LANGID>(LanguageId));

    // Load treemap settings
    TreeMapOptions.style = static_cast<TreeMapLayout::Style>(static_cast<int>(TreeMapStyle));
    TreeMapOptions.grid = TreeMapGrid;
    TreeMapOptions.showExtensions = TreeMapShowExtensions;
    TreeMapOptions.showFolderFrames = TreeMapShowFolderFrames;
    TreeMapOptions.folderFramesDrawThreshold = TreeMapFolderFramesDrawThreshold;
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

bool COptions::IsColumnVisible(const std::vector<int>& visibility, const int subitem) noexcept
{
    return subitem >= 0 &&
        (subitem >= static_cast<int>(visibility.size()) || visibility[subitem] != 0);
}

void COptions::SetColumnVisible(std::vector<int>& visibility, const int subitem, const bool visible)
{
    if (subitem < 0) return;

    if (subitem >= static_cast<int>(visibility.size()))
    {
        visibility.resize(subitem + 1, 1);
    }
    visibility[subitem] = visible;
}

LCID COptions::GetLocaleForFormatting()
{
    return UseWindowsLocaleSetting ? LOCALE_USER_DEFAULT :
        MAKELCID(LanguageId, SORT_DEFAULT);
}

void COptions::LoadAppSettings()
{
    // Odr-use the final inline Setting to initialize and register all Settings before loading persisted values.
    static_cast<void>(MainWindowPlacement.Obj());
    PreProcessPersistedSettings();
    PersistedSetting::ReadPersistedProperties();
    PostProcessPersistedSettings();
}
