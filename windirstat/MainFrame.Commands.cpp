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
#include "TreeMapView.h"
#include "FlameGraphView.h"
#include "SunburstView.h"
#include "FileTabbedView.h"
#include "FileTreeView.h"
#include "DrawTextCache.h"
#include "ExtensionView.h"
#include "PageAdvanced.h"
#include "PageFiltering.h"
#include "PageCleanups.h"
#include "PageFileTree.h"
#include "PageTreeMap.h"
#include "PagePermissions.h"
#include "PageGeneral.h"
#include "PagePrompts.h"
#include "ProgressDlg.h"

constexpr auto ID_STATUSPANE_IDLE_INDEX = 0;
constexpr auto ID_STATUSPANE_SIZE_INDEX = 1;
constexpr auto ID_STATUSPANE_RAM_INDEX = 2;
constexpr auto ID_INACTIVE_GRAPH_PANE = 0xEB00; // Outside MFC's control-bar and splitter-pane ID ranges.

static void SetNativeMenuRadio(CCmdUI* command, const bool checked)
{
    command->SetCheck(checked);
    if (command->m_pMenu == nullptr || command->m_pSubMenu != nullptr) return;

    MENUITEMINFO info{ .cbSize = sizeof(MENUITEMINFO), .fMask = MIIM_FTYPE | MIIM_CHECKMARKS };
    if (!command->m_pMenu->GetMenuItemInfo(command->m_nIndex, &info, TRUE)) return;

    info.fType |= MFT_RADIOCHECK;
    info.hbmpChecked = nullptr;
    info.hbmpUnchecked = nullptr;
    command->m_pMenu->SetMenuItemInfo(command->m_nIndex, &info, TRUE);
}

void CMainFrame::OnInitMenuPopup(CMenu* pPopupMenu, const UINT nIndex, const BOOL bSysMenu)
{
    CFrameWndEx::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);

    if (const auto [explorerMenu, explorerMenuPos] = LocateNamedMenu(pPopupMenu,
        Localization::Lookup(IDS_MENU_EXPLORER_MENU), false); explorerMenu != nullptr)
    {
        // Add placeholder only
        if (explorerMenu->GetMenuItemCount() == 0)
        {
            explorerMenu->AppendMenu(MF_STRING | MF_DISABLED | MF_GRAYED, 0,
                Localization::Lookup(IDS_PROGRESS).c_str());
        }

        SetMenuItem(pPopupMenu, explorerMenuPos, true);
    }

    // If the menu being opened is populate it
    if (pPopupMenu->GetMenuItemCount() == 1 && pPopupMenu->GetMenuItemID(0) == 0)
    {
        while (pPopupMenu->GetMenuItemCount() > 0)
        {
            pPopupMenu->DeleteMenu(0, MF_BYPOSITION);
        }

        std::vector<std::wstring> paths;
        for (const auto& item : CWinDirStatModel::Get()->GetAllSelected())
        {
            paths.push_back(item->GetPath());
        }

        if (const CComPtr contextMenu = GetContextMenu(GetSafeHwnd(), paths);
            contextMenu != nullptr)
        {
            (void) contextMenu->QueryContextMenu(pPopupMenu->GetSafeHmenu(), 0,
                CONTENT_MENU_MINCMD, CONTENT_MENU_MAXCMD, CMF_NORMAL);
        }
    }

    // update cleanup menu if this is the cleanup submenu
    if (pPopupMenu->GetMenuState(ID_CLEANUP_EMPTY_BIN, MF_BYCOMMAND) != static_cast<UINT>(-1))
    {
        UpdateCleanupMenu(pPopupMenu);
    }

    // update tools menu - check if pPopupMenu is the Tools menu by looking for our operation submenus
    const auto [shadowCopyMenu, shadowCopyPos] = LocateNamedMenu(pPopupMenu, Localization::Lookup(IDS_MENU_SHADOW_COPY), false);
    if (shadowCopyMenu != nullptr)
    {
        UpdateToolsMenu(pPopupMenu);
    }
}

void CMainFrame::UpdateCleanupMenu(CMenu* menu, const bool triggerAsync)
{
    // Define menu items structure with cached values
    struct { ULONGLONG* count; ULONGLONG* bytes; UINT menuId; LPCWSTR prefix; } menuItems[] = {
        { &m_recycleBinItems, &m_recycleBinBytes, ID_CLEANUP_EMPTY_BIN, IDS_EMPTY_RECYCLEBIN.data() },
        { &m_shadowCopyCount, &m_shadowCopyBytes, ID_CLEANUP_REMOVE_SHADOW, IDS_MENU_REMOVE_SHADOW.data() }
    };

    // Update menu items using cached values (initially shows zeros or last cached values)
    for (const auto& [count, bytes, menuId, prefix] : menuItems)
    {
        const std::wstring label = Localization::Lookup(prefix) + ((*count == 1) ?
            Localization::Format(IDS_ONEITEMs, FormatBytes(*bytes)) :
            Localization::Format(IDS_sITEMSs, FormatCount(*count), FormatBytes(*bytes)));

        const UINT state = menu->GetMenuState(menuId, MF_BYCOMMAND);
        menu->ModifyMenu(menuId, MF_BYCOMMAND | MF_STRING, menuId, label.c_str());
        menu->EnableMenuItem(menuId, state);
    }

    UpdateDynamicMenuItems(menu);

    // Launch a detached thread to perform the queries
    if (triggerAsync) std::thread([this]()
    {
        // Query recycle bin and shadow copies
        QueryRecycleBin(m_recycleBinItems, m_recycleBinBytes);
        QueryShadowCopies(m_shadowCopyCount, m_shadowCopyBytes);

        // Use InvokeInMessageThread to update the menu on the UI thread
        InvokeInMessageThread([this]()
        {
            // Check if the menu is still valid and visible
            const auto [menuObj, menuPos] = LocateNamedMenu(GetMenu(), Localization::Lookup(IDS_MENU_CLEANUP), false);
            if (menuObj == nullptr || menuObj->GetMenuItemCount() <= 0) return;

            // Update menu items with the newly retrieved values
            UpdateCleanupMenu(menuObj, false);
        });
    }).detach();
}

void CMainFrame::QueryRecycleBin(ULONGLONG& items, ULONGLONG& bytes)
{
    items = 0;
    bytes = 0;

    for (const std::wstring & drive : GetDriveList({DRIVE_FIXED, DRIVE_REMOVABLE, DRIVE_RAMDISK}))
    {
        SHQUERYRBINFO qbi{ .cbSize = sizeof(qbi) };
        if (FAILED(::SHQueryRecycleBin((drive + L"\\").c_str(), &qbi)))
        {
            continue;
        }

        items += qbi.i64NumItems;
        bytes += qbi.i64Size;
    }
}

std::pair<CMenu*,int> CMainFrame::LocateNamedMenu(const CMenu* menu, const std::wstring & subMenuText, const bool removeItems) const
{
    // locate submenu
    CMenu* subMenu = nullptr;
    int subMenuPos = -1;
    for (const int i : std::views::iota(0, menu->GetMenuItemCount()))
    {
        CStringW menuString;
        if (menu->GetMenuString(i, menuString, MF_BYPOSITION) > 0 &&
            _wcsicmp(menuString, subMenuText.c_str()) == 0)
        {
            subMenu = menu->GetSubMenu(i);
            subMenuPos = i;
            break;
        }
    }

    // cleanup old items
    if (removeItems && subMenu != nullptr) while (subMenu->GetMenuItemCount() > 0)
        subMenu->DeleteMenu(0, MF_BYPOSITION);
    return { subMenu, subMenuPos };
}

void CMainFrame::UpdateDynamicMenuItems(CMenu* menu) const
{
    const auto& items = CWinDirStatModel::Get()->GetAllSelected();
    const bool scanReady = CWinDirStatModel::Get()->IsScanSettled();

    // get list of paths from items
    std::vector<std::wstring> paths;
    for (auto& item : items) paths.push_back(item->GetPath());

    // locate compress menu
    auto [compressMenu, compressMenuPos] = CMainFrame::LocateNamedMenu(menu, Localization::Lookup(IDS_MENU_COMPRESS_MENU), false);
    if (compressMenu && compressMenuPos >= 0)
    {
        // Check if any submenu items are enabled
        const int menuItemCount = compressMenu->GetMenuItemCount();
        const bool anyEnabled = std::ranges::any_of(std::views::iota(0, menuItemCount), [&](const int i)
        {
            CCmdUI state;
            state.m_nIndex = i;
            state.m_nIndexMax = menuItemCount;
            state.m_nID = compressMenu->GetMenuItemID(i);
            state.m_pMenu = compressMenu;
            state.DoUpdate(const_cast<CMainFrame*>(this), FALSE);
            return IsMenuEnabled(compressMenu, i);
        });

        SetMenuItem(menu, compressMenuPos, anyEnabled);
    }

    auto[customMenu, customMenuPos] = LocateNamedMenu(menu, Localization::Lookup(IDS_USER_DEFINED_CLEANUP));
    for (UINT iCurrent = 0; customMenu != nullptr && iCurrent < COptions::UserDefinedCleanups.size(); iCurrent++)
    {
        auto& udc = COptions::UserDefinedCleanups[iCurrent];
        if (!udc.Enabled) continue;

        std::wstring string = std::vformat(Localization::Lookup(IDS_UDCsCTRLd),
            std::make_wformat_args(udc.Title.Obj(), iCurrent));

        bool udcValid = scanReady && GetLogicalFocus() == LF_FILETREE && !items.empty();
        if (udcValid) for (const auto& item : items)
        {
            udcValid &= CWinDirStatModel::Get()->UserDefinedCleanupWorksForItem(&udc, item);
        }

        customMenu->AppendMenu(MF_STRING, ID_USERDEFINEDCLEANUP0 + iCurrent, string.c_str());
        SetMenuItem(customMenu, ID_USERDEFINEDCLEANUP0 + iCurrent, udcValid, true);
    }

    // conditionally disable menu if empty
    if (customMenu) SetMenuItem(menu,
        customMenuPos, customMenu->GetMenuItemCount() > 0 && scanReady);
}

void CMainFrame::OnAdvancedShadowCopy(const UINT nID)
{
    const WCHAR driveLetter = wds::strAlpha[nID - ID_TOOLS_SHADOW_COPY_BASE];
    const std::wstring drive = std::format(L"{:c}:", driveLetter);

    bool success = false;
    CProgressDlg dlg(0, CProgressDlg::Flags::NoCancel, this, [&](CProgressDlg*)
    {
        success = CreateShadowCopy(drive);
    });
    dlg.DoModal();

    if (!success)
    {
        const std::wstring msg = Localization::Format(IDS_SHADOW_COPY_FAILED, GetDrive(drive));
        WdsMessageBox(*this, msg, wds::strWinDirStat, MB_ICONERROR | MB_OK);
    }
}

void CMainFrame::OnAdvancedDefrag(const UINT nID)
{
    const WCHAR driveLetter = wds::strAlpha[nID - ID_TOOLS_DEFRAG_BASE];
    ExecuteCommandInConsole(std::format(L"DEFRAG.EXE {:c}: /O", driveLetter), L"DEFRAG");
}

void CMainFrame::OnAdvancedChkdsk(const UINT nID)
{
    const WCHAR driveLetter = wds::strAlpha[nID - ID_TOOLS_CHKDSK_BASE];
    ExecuteCommandInConsole(std::format(L"CHKDSK.EXE {:c}: /F", driveLetter), L"CHKDSK");
}

void CMainFrame::UpdateToolsMenu(CMenu* menu)
{
    // menu is the Tools popup menu itself
    // Find each operation submenu and populate with drives
    auto [shadowCopyMenu, shadowCopyPos] = LocateNamedMenu(menu, Localization::Lookup(IDS_MENU_SHADOW_COPY), true);
    auto [defragMenu, defragPos] = LocateNamedMenu(menu, Localization::Lookup(IDS_MENU_DEFRAGMENT), true);
    auto [chkdskMenu, chkdskPos] = LocateNamedMenu(menu, Localization::Lookup(IDS_MENU_CHKDSK), true);

    // Get available local drives and conditionally enable based on elevation
    const auto drives = GetDriveList({DRIVE_FIXED, DRIVE_REMOVABLE, DRIVE_RAMDISK});
    SetMenuItem(menu, shadowCopyPos, IsElevationActive() && !drives.empty());
    SetMenuItem(menu, defragPos, IsElevationPossible() && !drives.empty());
    SetMenuItem(menu, chkdskPos, IsElevationPossible() && !drives.empty());

    for (const auto& drive : drives)
    {
        // Get volume label for display
        const std::wstring volumeName = GetVolumeName(drive);
        const std::wstring displayName = volumeName.empty()
            ? GetDrive(drive) : std::format(L"{:.2} ({})", drive, volumeName);

        const int driveIndex = std::toupper(drive[0]) - L'A';
        shadowCopyMenu->AppendMenu(MF_STRING, ID_TOOLS_SHADOW_COPY_BASE + driveIndex, displayName.c_str());
        defragMenu->AppendMenu(MF_STRING, ID_TOOLS_DEFRAG_BASE + driveIndex, displayName.c_str());
        chkdskMenu->AppendMenu(MF_STRING, ID_TOOLS_CHKDSK_BASE + driveIndex, displayName.c_str());
    }
}

void CMainFrame::SetLogicalFocus(const LOGICAL_FOCUS lf)
{
    if (lf != m_logicalFocus)
    {
        m_logicalFocus = lf;
        UpdatePaneText();

        CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_SELECTION_STYLE);
    }
}

LOGICAL_FOCUS CMainFrame::GetLogicalFocus() const
{
    return m_logicalFocus;
}

void CMainFrame::MoveFocus(const LOGICAL_FOCUS logicalFocus)
{
    switch (logicalFocus)
    {
        case LF_EXTLIST: GetExtensionView()->SetFocus(); break;
        case LF_DUPELIST: GetFileDupeView()->SetFocus(); break;
        case LF_TOPLIST: GetFileTopView()->SetFocus(); break;
        case LF_SEARCHLIST: GetFileSearchView()->SetFocus(); break;
        case LF_WATCHERLIST: GetFileWatcherView()->SetFocus(); break;
        case LF_PERMSLIST: GetFilePermsView()->SetFocus(); break;
        case LF_STORAGEANALYTICS:
        {
            GetFileTabbedView()->SetActiveStorageAnalyticsView();
            GetFileTabbedView()->SetFocus();
            break;
        }
        case LF_FILETREE:
        {
            GetFileTabbedView()->SetActiveFileTreeView();
            GetFileTreeView()->SetFocus();
            break;
        }
        case LF_NONE:
        {
            SetLogicalFocus(LF_NONE);
            SetFocus();
        }
    }
}

void CMainFrame::UpdatePaneText()
{
    const auto focus = GetLogicalFocus();
    std::wstring fileSelectionText = !CWinDirStatModel::Get()->IsScanRunning() ?
        Localization::Lookup(IDS_IDLEMESSAGE) : wds::strEmpty;
    ULONGLONG size = MAXULONGLONG;

    // Allow override on hover (check active graph pane)
    if (const auto hoverInfo = GetActiveGraphPane()->GetHoverInfo(); !hoverInfo.path.empty())
    {
        fileSelectionText = hoverInfo.path;
        size = hoverInfo.size;
    }

    // Only get the data if the scan model is not actively updating
    else if (CWinDirStatModel::Get()->IsScanSettled())
    {
        if (focus != LF_EXTLIST)
        {
            const auto& items = CWinDirStatModel::Get()->GetAllSelected();
            if (items.size() == 1)
            {
                // If single item selected, show full path
                const auto path = items.front()->GetPath();
                if (!path.empty()) fileSelectionText = path;
            }
            else if (items.size() > 1)
            {
                // If multiple items are selected, show the statistics of selected items, files, and folders
                ULONGLONG totalFiles = 0;
                ULONGLONG totalFolders = 0;
                for (const auto& item : items)
                {
                    if (item->IsTypeOrFlag(IT_FILE)) totalFiles++;
                    if (item->IsTypeOrFlag(IT_DIRECTORY)) totalFolders++;
                    totalFiles += item->GetFilesCount();
                    totalFolders += item->GetFoldersCount();
                }
                fileSelectionText = Localization::Format(IDS_ITEMSs_SELECTED_FILESs_FOLDERSs,
                    FormatCount(items.size()), FormatCount(totalFiles), FormatCount(totalFolders));
            }

            for (size = 0; const auto& item : items)
            {
                size += COptions::TreeMapUseLogical ? item->GetSizeLogical() : item->GetSizePhysical();
            }

        }
        else if (fileSelectionText.empty())
        {
            fileSelectionText = wds::chrStar + CWinDirStatModel::Get()->GetHighlightExtension();
        }
    }

    // Update select physical size
    const CClientDC dc(this);
    SetStatusPaneText(dc, ID_STATUSPANE_IDLE_INDEX, fileSelectionText);
    SetStatusPaneText(dc, ID_STATUSPANE_SIZE_INDEX, (size == MAXULONGLONG) ? wds::strEmpty :
        std::format(L"{}: \u2211 {}", Localization::Lookup(COptions::TreeMapUseLogical ? IDS_COL_SIZE_LOGICAL : IDS_COL_SIZE_PHYSICAL), FormatBytes(size)), 175);
    SetStatusPaneText(dc, ID_STATUSPANE_RAM_INDEX, CDirStatApp::GetCurrentProcessMemoryInfo(), 175);
}

void CMainFrame::OnUpdateEnableControl(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(true);
}

void CMainFrame::OnSize(const UINT nType, const int cx, const int cy)
{
    CFrameWndEx::OnSize(nType, cx, cy);

    if (!IsWindow(m_wndStatusBar.m_hWnd))
    {
        return;
    }

    CRect rc;
    m_wndStatusBar.GetItemRect(ID_STATUSPANE_IDLE_INDEX, rc);

    if (m_progress.m_hWnd != nullptr)
    {
        CRect progRc = rc;
        progRc.DeflateRect(DpiRest(3, &m_wndStatusBar), DpiRest(4, &m_wndStatusBar),
            DpiRest(5, &m_wndStatusBar), DpiRest(4, &m_wndStatusBar));
        m_progress.MoveWindow(progRc);
    }
    else if (m_pacman.m_hWnd != nullptr)
    {
        m_pacman.MoveWindow(rc);
    }
}

/////////////////////////////////////////////////////////////////////////////

void CMainFrame::OnUpdateViewShowTreeMap(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!CWinDirStatModel::Get()->IsScanRunning());
    SetNativeMenuRadio(pCmdUI, GetGraphPaneType() == GraphPane::TreeMap);
}

void CMainFrame::OnUpdateTreeMapUseLogical(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!CWinDirStatModel::Get()->IsScanRunning());
    SetNativeMenuRadio(pCmdUI, COptions::TreeMapUseLogical);
}

void CMainFrame::OnUpdateTreeMapUsePhysical(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!CWinDirStatModel::Get()->IsScanRunning());
    SetNativeMenuRadio(pCmdUI, !COptions::TreeMapUseLogical);
}

void CMainFrame::OnUpdateViewAbsolutePercentages(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(COptions::UseAbsolutePercentages);
}

void CMainFrame::OnUpdateViewShowFileTypes(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(GetExtensionView()->IsShowTypes());
}

void CMainFrame::OnUpdateViewGroupUnregisteredTypes(CCmdUI* pCmdUI)
{
    const CWinDirStatModel* model = CWinDirStatModel::Get();
    pCmdUI->Enable(GetExtensionView()->IsShowTypes() && model->IsScanSettled());
    pCmdUI->SetCheck(COptions::GroupUnregisteredTypes);
}

void CMainFrame::OnUpdateViewShowWatcher(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(GetFileTabbedView()->IsWatcherTabVisible());
}

GraphPane CMainFrame::GetGraphPaneType() const
{
    return DecodeGraphPane(COptions::GraphPaneStyle);
}

void CMainFrame::SelectGraphPane(const GraphPane pane)
{
    // Accelerators still dispatch WM_COMMAND while the corresponding menu
    // item is disabled. Do not switch away from the pane suspended for a scan.
    if (CWinDirStatModel::Get()->IsScanRunning()) return;
    if (GetGraphPaneType() == pane && IsActiveGraphPaneShown()) return;

    COptions::GraphPaneStyle = EncodeGraphPane(pane);
    ShowActiveGraphPane(true);
    RebuildLayout();
}

void CMainFrame::OnViewTreeMap()
{
    SelectGraphPane(GraphPane::TreeMap);
}

static_assert(ID_VIEW_TREEMAP_ROWS + static_cast<int>(TreeMapLayout::Style::Squarified)
        == ID_VIEW_TREEMAP_SQUARIFIED
    && ID_VIEW_TREEMAP_ROWS + static_cast<int>(TreeMapLayout::Style::Hilbert) == ID_VIEW_TREEMAP_HILBERT
    && ID_VIEW_TREEMAP_ROWS + static_cast<int>(TreeMapLayout::Style::Moore) == ID_VIEW_TREEMAP_MOORE);

void CMainFrame::OnViewTreeMapStyle(const UINT commandId)
{
    if (CWinDirStatModel::Get()->IsScanRunning()) return;

    const int styleValue = commandId - ID_VIEW_TREEMAP_ROWS;
    ASSERT(styleValue >= static_cast<int>(TreeMapLayout::Style::Rows)
        && styleValue <= static_cast<int>(TreeMapLayout::Style::Moore));
    CTreeMap::Options options = COptions::TreeMapOptions;
    options.style = static_cast<TreeMapLayout::Style>(styleValue);
    COptions::SetTreeMapOptions(options);
    SelectGraphPane(GraphPane::TreeMap);
}

void CMainFrame::OnUpdateViewTreeMapStyle(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!CWinDirStatModel::Get()->IsScanRunning());
    const auto style = static_cast<TreeMapLayout::Style>(pCmdUI->m_nID - ID_VIEW_TREEMAP_ROWS);
    SetNativeMenuRadio(pCmdUI, GetGraphPaneType() == GraphPane::TreeMap
        && COptions::TreeMapOptions.style == style);
}

void CMainFrame::OnViewFlameGraph()
{
    SelectGraphPane(GraphPane::FlameGraph);
}

void CMainFrame::OnUpdateViewFlameGraph(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!CWinDirStatModel::Get()->IsScanRunning());
    SetNativeMenuRadio(pCmdUI, GetGraphPaneType() == GraphPane::FlameGraph);
}

void CMainFrame::OnViewSunburst()
{
    SelectGraphPane(GraphPane::Sunburst);
}

void CMainFrame::OnUpdateViewSunburst(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(!CWinDirStatModel::Get()->IsScanRunning());
    SetNativeMenuRadio(pCmdUI, GetGraphPaneType() == GraphPane::Sunburst);
}

static void SortItemRecursive(CItem* item)
{
    if (item == nullptr || item->IsLeaf()) return;
    COptions::TreeMapUseLogical ? item->SortItemsBySizeLogical() : item->SortItemsBySizePhysical();
    for (CItem* child : item->GetChildren())
    {
        SortItemRecursive(child);
    }
}

void CMainFrame::OnViewTreeMapUseLogical()
{
    if (!COptions::TreeMapUseLogical)
    {
        COptions::TreeMapUseLogical = true;
        CItem* root = CWinDirStatModel::Get()->GetRootItem();
        if (root)
        {
            SortItemRecursive(root);
            CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_SIZE_MODE);
        }
        UpdatePaneText();
    }
}

void CMainFrame::OnViewTreeMapUsePhysical()
{
    if (COptions::TreeMapUseLogical)
    {
        COptions::TreeMapUseLogical = false;
        CItem* root = CWinDirStatModel::Get()->GetRootItem();
        if (root)
        {
            SortItemRecursive(root);
            CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_SIZE_MODE);
        }
        UpdatePaneText();
    }
}

void CMainFrame::OnViewAbsolutePercentages()
{
    COptions::UseAbsolutePercentages = !COptions::UseAbsolutePercentages.Obj();
    GetFileTreeView()->RefreshPercentages();
}

void CMainFrame::OnViewShowFileTypes()
{
    GetExtensionView()->ShowTypes(!GetExtensionView()->IsShowTypes());
    if (GetExtensionView()->IsShowTypes())
    {
        RestoreExtensionView();
    }
    else
    {
        MinimizeExtensionView();
    }
}

void CMainFrame::OnViewGroupUnregisteredTypes()
{
    COptions::GroupUnregisteredTypes = !COptions::GroupUnregisteredTypes;

    // Recolor extensions so the unregistered group shares one color, then refresh the list and graph
    CWinDirStatModel::Get()->RebuildExtensionData();
    GetExtensionView()->OnUpdate(nullptr, MODEL_CHANGE_NONE, nullptr);
    CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_TREEMAP_STYLE);
}

void CMainFrame::OnViewShowExtensionsOnTreeMap()
{
    if (GetGraphPaneType() != GraphPane::TreeMap) return;

    COptions::TreeMapShowExtensions = !static_cast<bool>(COptions::TreeMapShowExtensions);
    COptions::TreeMapOptions.showExtensions = COptions::TreeMapShowExtensions;
    CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_TREEMAP_STYLE);
}

void CMainFrame::OnUpdateViewShowExtensionsOnTreeMap(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetGraphPaneType() == GraphPane::TreeMap
        && !CWinDirStatModel::Get()->IsScanRunning());
    pCmdUI->SetCheck(COptions::TreeMapOptions.showExtensions);
}

void CMainFrame::OnViewShowFolderFramesOnTreeMap()
{
    if (GetGraphPaneType() != GraphPane::TreeMap) return;

    COptions::TreeMapShowFolderFrames = !static_cast<bool>(COptions::TreeMapShowFolderFrames);
    COptions::TreeMapOptions.showFolderFrames = COptions::TreeMapShowFolderFrames;
    CWinDirStatModel::Get()->NotifyPanes(MODEL_CHANGE_TREEMAP_STYLE);
}

void CMainFrame::OnUpdateViewShowFolderFramesOnTreeMap(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(GetGraphPaneType() == GraphPane::TreeMap
        && !CWinDirStatModel::Get()->IsScanRunning());
    pCmdUI->SetCheck(COptions::TreeMapOptions.showFolderFrames);
}

//
// CToolBarLabel. A non-interactive, text-only toolbar entry used to
// caption the contextual watcher button group.
//
class CToolBarLabel final : public CMFCToolBarButton
{
    DECLARE_DYNCREATE(CToolBarLabel)

public:
    CToolBarLabel() = default;
    CToolBarLabel(const UINT id, const std::wstring& text)
        : CMFCToolBarButton(id, -1, text.c_str())
    {
        m_bText = TRUE;
        m_bImage = FALSE;
        m_nStyle = TBBS_DISABLED;
    }

    SIZE OnCalculateSize(CDC* pDC, const CSize& sizeDefault, BOOL /*bHorz*/) override
    {
        // Pad the measured text by its height to keep the caption visually separated
        CFont* oldFont = pDC->SelectObject(&GetGlobalData()->fontRegular);
        const CSize textSize = pDC->GetTextExtent(m_strText);
        pDC->SelectObject(oldFont);
        return { textSize.cx + textSize.cy, sizeDefault.cy };
    }

    void OnDraw(CDC* pDC, const CRect& rect, CMFCToolBarImages* /*pImages*/, BOOL /*bHorz*/,
        BOOL /*bCustomizeMode*/, BOOL /*bHighlight*/, BOOL /*bDrawBorder*/, BOOL /*bGrayDisabledButtons*/) override
    {
        CRect textRect(rect);
        CFont* oldFont = pDC->SelectObject(&GetGlobalData()->fontRegular);
        pDC->SetBkMode(TRANSPARENT);
        pDC->SetTextColor(Icons::NeutralRef());
        pDC->DrawText(m_strText, textRect, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
        pDC->SelectObject(oldFont);
    }
};

IMPLEMENT_DYNCREATE(CToolBarLabel, CMFCToolBarButton)

static void PaintWatcherAutoScroll(Gdiplus::Graphics& g)
{
    const bool enabled = COptions::WatcherAutoScroll;
    Icons::PaintCharacter(g, L'⤓', enabled ? RGB(0, 156, 221) : Icons::NeutralRef());
    if (!enabled)
    {
        Gdiplus::Pen slash(Icons::C(204, 0, 0), 6);
        slash.SetStartCap(Gdiplus::LineCapRound);
        slash.SetEndCap(Gdiplus::LineCapRound);
        g.DrawLine(&slash, 12, 52, 52, 12);
    }
}

void CMainFrame::RebuildToolBar()
{
    const auto imageSize = COptions::LargeToolBar ? 32 : 20;
    const auto scale = COptions::LargeToolBar ? (32.0f / 20.0f) : 1.0f;

    // Remove all existing buttons
    if (CDirStatApp::Get()->m_pMainWnd == nullptr) return;
    while (m_wndToolBar.GetCount() > 0)
        m_wndToolBar.RemoveButton(0);

    // Clear the shared image list and resize buttons to match the new icon size
    CMFCToolBar::GetImages()->Clear();
    CMFCToolBar::SetSizes(
        { static_cast<LONG>(m_defaultButtonSize.cx * scale),
          static_cast<LONG>(m_defaultButtonSize.cy * scale)},
        { imageSize, imageSize });

    using Painter = std::function<void(Gdiplus::Graphics&)>;
    static const std::vector<std::tuple<UINT, std::wstring_view, Painter>> toolbarButtons =
    {
        { ID_FILE_SELECT,             IDS_FILE_SELECT,             Icons::PaintFileSelect},
        { ID_SEPARATOR,               {},{}},
        { ID_SCAN_RESUME,             IDS_RESUME,                  Icons::Char(L'▶', RGB( 50, 205,  50))},
        { ID_SCAN_SUSPEND,            IDS_SUSPEND,                 Icons::PaintPause},
        { ID_SCAN_STOP,               IDS_STOP,                    Icons::Char(L'■', RGB(220,  20,  60))},
        { ID_SEPARATOR,               {},{}},
        { ID_REFRESH_ALL,             IDS_REFRESH_ALL,             Icons::Char(L'↻', RGB(  0, 156, 221))},
        { ID_REFRESH_SELECTED,        IDS_REFRESH_SELECTED,        Icons::PaintRefreshSelected},
        { ID_SEPARATOR,               {},{}},
        { ID_SEARCH,                  IDS_SEARCH_TITLE,            Icons::Char(L'⌕', Icons::NeutralRef())},
        { ID_FILTER,                  IDS_PAGE_FILTERING_TITLE,    [](auto& g){ Icons::PaintFilter(g, CFiltering::IsFilterActive()); } },
        { ID_SEPARATOR,               {},{}},
        { ID_CLEANUP_OPEN_SELECTED,   IDS_CLEANUP_OPEN_SELECTED,   Icons::PaintOpenSelected},
        { ID_CLEANUP_EXPLORER_SELECT, IDS_CLEANUP_EXPLORER_SELECT, Icons::PaintExplorerSelect},
        { ID_EDIT_COPY_CLIPBOARD,     IDS_EDIT_COPY_CLIPBOARD,     Icons::PaintEditCopyClipboard},
        { ID_CLEANUP_OPEN_IN_CONSOLE, IDS_CLEANUP_OPEN_IN_CONSOLE, Icons::PaintOpenInConsole},
        { ID_CLEANUP_PROPERTIES,      IDS_CLEANUP_PROPERTIES,      Icons::PaintProperties},
        { ID_SEPARATOR,               {},{}},
        { ID_CLEANUP_DELETE_BIN,      IDS_CLEANUP_DELETE_BIN,      Icons::PaintDeleteBin},
        { ID_CLEANUP_DELETE,          IDS_CLEANUP_DELETE,          Icons::PaintDelete},
        { ID_SEPARATOR,               {},{}},
        { ID_TREEMAP_ZOOMIN,          IDS_TREEMAP_ZOOMIN,          [](auto& g) { Icons::PaintMagnifier(g, true);}},
        { ID_TREEMAP_ZOOMOUT,         IDS_TREEMAP_ZOOMOUT,         [](auto& g){ Icons::PaintMagnifier(g, false);}},
        { ID_SEPARATOR,               {},{}},
        { ID_VIEW_WINDOW_LAYOUT,      IDS_WINDOW_LAYOUT,           Icons::PaintWindowLayout},
        { ID_SEPARATOR,               {},{}},
        { ID_CONFIGURE,               IDS_MENU_SETTINGS,           Icons::PaintGear},
        { ID_HELP_MANUAL,             IDS_HELP_MANUAL,             Icons::PaintHelp},
        { ID_SEPARATOR,               {},{}},
        { ID_WATCHER_LABEL,           IDS_WATCHER,                 {}},
        { ID_WATCHER_START,           {},                          Icons::Char(L'▶', RGB( 50, 205,  50))},
        { ID_WATCHER_PAUSE,           {},                          Icons::PaintPause},
        { ID_WATCHER_AUTOSCROLL,      {},                          PaintWatcherAutoScroll},
        { ID_WATCHER_CLEAR,           {},                          Icons::PaintDelete},
    };

    for (const auto& [id, text, painter] : toolbarButtons)
    {
        if (id == ID_SEPARATOR)
        {
            m_wndToolBar.InsertSeparator();
            continue;
        }

        if (id == ID_WATCHER_LABEL)
        {
            m_wndToolBar.InsertButton(CToolBarLabel(id, Localization::Lookup(text) + L":"));
            continue;
        }

        int index = 0;
        if (painter)
        {
            CBitmap bitmap;
            bitmap.Attach(Icons::MakeBitmap(imageSize, painter));
            index = CMFCToolBar::GetImages()->AddImage(bitmap, TRUE);
        }

        CMFCToolBarButton button(id, index, nullptr, TRUE, TRUE);
        button.m_bText = FALSE;
        button.m_nStyle = TBBS_DISABLED;
        if (!text.empty()) button.m_strText = Localization::Lookup(text).c_str();
        m_wndToolBar.InsertButton(button);
    }

    // The watcher buttons are contextual and only shown while its tab is active
    SetWatcherToolBarButtons(m_fileTabbedView != nullptr &&
        m_fileTabbedView->IsFileWatcherViewTabActive());

    m_wndToolBar.AdjustLayout();
}

void CMainFrame::SetWatcherToolBarButtons(const bool visible)
{
    if (m_wndToolBar.GetSafeHwnd() == nullptr) return;

    // The group spans the separator before the caption label through the last button
    const int labelIndex = m_wndToolBar.CommandToIndex(ID_WATCHER_LABEL);
    if (labelIndex < 1) return;

    bool changed = false;
    for (const int index : std::views::iota(labelIndex - 1, labelIndex + 5))
    {
        CMFCToolBarButton* button = m_wndToolBar.GetButton(index);
        if (button == nullptr || (button->IsVisible() != FALSE) == visible) continue;
        button->SetVisible(visible);
        changed = true;
    }

    // Recompute button locations and repaint; a size-only adjustment does
    // not refresh the layout when the docked toolbar extents are unchanged
    if (changed) m_wndToolBar.AdjustLayout();
}

void CMainFrame::OnWatcherStart()
{
    CFileWatcherControl::Get()->StartMonitoring();
}

void CMainFrame::OnUpdateWatcherStart(CCmdUI* pCmdUI)
{
    const auto* watcher = CFileWatcherControl::Get();
    pCmdUI->Enable(watcher != nullptr && !watcher->IsMonitoring() &&
        CWinDirStatModel::Get()->HasRootItem());
}

void CMainFrame::OnWatcherPause()
{
    CFileWatcherControl::Get()->StopMonitoring();
}

void CMainFrame::OnUpdateWatcherPause(CCmdUI* pCmdUI)
{
    const auto* watcher = CFileWatcherControl::Get();
    pCmdUI->Enable(watcher != nullptr && watcher->IsMonitoring());
}

void CMainFrame::OnWatcherAutoScroll()
{
    COptions::WatcherAutoScroll = !COptions::WatcherAutoScroll;
    RebuildToolBar();
}

void CMainFrame::OnUpdateWatcherAutoScroll(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(TRUE);
    pCmdUI->SetCheck(FALSE);
}

void CMainFrame::OnWatcherClear()
{
    CFileWatcherControl::Get()->ClearResults();
}

void CMainFrame::OnUpdateWatcherClear(CCmdUI* pCmdUI)
{
    const auto* watcher = CFileWatcherControl::Get();
    pCmdUI->Enable(watcher != nullptr && watcher->GetItemCount() > 0);
}

void CMainFrame::OnViewLargeToolBar()
{
    COptions::LargeToolBar = !COptions::LargeToolBar;
    RebuildToolBar();
}

void CMainFrame::OnUpdateViewLargeToolBar(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(COptions::LargeToolBar);
    pCmdUI->Enable((m_wndToolBar.GetStyle() & WS_VISIBLE) != 0);
}

void CMainFrame::OnConfigure()
{
    const bool restart = COptionsPropertySheet::ShowSettings();

    // Rebuild the toolbar so icons (e.g. the filter indicator) reflect the new settings
    RebuildToolBar();

    // Save settings in case the application exits abnormally
    PersistedSetting::WritePersistedProperties();

    if (restart)
    {
        CDirStatApp::Get()->RestartApplication();
    }
}

void CMainFrame::OnSysColorChange()
{
    GetFileTreeView()->SysColorChanged();
    GetExtensionView()->SysColorChanged();
    DrawTextCache::Get().ClearCache();

    // Redraw menus for dark mode
    DarkMode::SetAppDarkMode();
    RedrawWindow();
}

UINT CMainFrame::OnPowerBroadcast(UINT, LPARAM)
{
    OnSysColorChange();
    return TRUE;
}

LRESULT CMainFrame::OnUahDrawMenu(WPARAM wParam, LPARAM lParam)
{
    return DarkMode::HandleMenuMessage(GetCurrentMessage()->message, wParam, lParam, *this);
}

void CMainFrame::OnNcPaint()
{
    // Update the bottom of the menu bar that is not properly painted
    CFrameWndEx::OnNcPaint();
    DarkMode::DrawMenuClientArea(*this);
}

BOOL CMainFrame::OnNcActivate(BOOL bActive)
{
    // Update the bottom of the menu bar that is not properly painted
    const auto ret = CFrameWndEx::OnNcActivate(bActive);
    DarkMode::DrawMenuClientArea(*this);
    return ret;
}

BOOL CMainFrame::LoadFrame(const UINT nIDResource, const DWORD dwDefaultStyle, CWnd* pParentWnd, CCreateContext* pContext)
{
    if (!CFrameWndEx::LoadFrame(nIDResource, dwDefaultStyle, pParentWnd, pContext))
    {
        return FALSE;
    }

    Localization::UpdateMenu(*GetMenu());
    Localization::UpdateDialogs(*this);
    SetTitle(GetAppTitle().c_str());

    return TRUE;
}

void CMainFrame::OnToolsWatcher()
{
    const bool visible = !GetFileTabbedView()->IsWatcherTabVisible();
    GetFileTabbedView()->SetWatcherTabVisibility(visible);
    if (visible)
    {
        GetFileTabbedView()->SetActiveWatcherView();
    }
}

void CMainFrame::OnToolsPermissions()
{
    GetFileTabbedView()->SetPermsTabVisibility(!GetFileTabbedView()->IsPermsTabVisible());

    // Re-check visibility: a cancelled scan leaves the tab hidden, so don't activate it
    if (GetFileTabbedView()->IsPermsTabVisible())
    {
        GetFileTabbedView()->SetActivePermsView();
    }
}

void CMainFrame::OnUpdateToolsPermissions(CCmdUI* pCmdUI)
{
    // Only allow launching a scan once the file tree has been fully populated
    const auto* model = CWinDirStatModel::Get();
    pCmdUI->SetCheck(GetFileTabbedView()->IsPermsTabVisible());
    pCmdUI->Enable(GetFileTabbedView()->IsPermsTabVisible() || model->IsScanSettled());
}

void CMainFrame::OnToolsStorageAnalytics()
{
    GetFileTabbedView()->SetStorageAnalyticsTabVisibility(!GetFileTabbedView()->IsStorageAnalyticsTabVisible());

    if (GetFileTabbedView()->IsStorageAnalyticsTabVisible())
    {
        GetFileTabbedView()->SetActiveStorageAnalyticsView();
    }
}

void CMainFrame::OnUpdateToolsStorageAnalytics(CCmdUI* pCmdUI)
{
    const auto* model = CWinDirStatModel::Get();
    pCmdUI->SetCheck(GetFileTabbedView()->IsStorageAnalyticsTabVisible());
    pCmdUI->Enable(GetFileTabbedView()->IsStorageAnalyticsTabVisible() || model->IsScanSettled());
}

void CMainFrame::OnViewWindowLayout()
{
    const int idx = m_wndToolBar.CommandToIndex(ID_VIEW_WINDOW_LAYOUT);
    CRect btnRect;
    m_wndToolBar.GetItemRect(idx, &btnRect);
    m_wndToolBar.ClientToScreen(&btnRect);
    m_layoutPopup.ShowAtButton(btnRect);
}

void CMainFrame::ConfigureSplitterCallbacks(int topo, int perm)
{
    m_splitter.ClearPaneTracking();
    m_subSplitter.ClearPaneTracking();

    auto showGraph = [this](bool visible) { ShowActiveGraphPane(visible); };
    auto showFileTypes = [this](bool visible) { GetExtensionView()->ShowTypes(visible); };

    switch (topo)
    {
    case LT_ROWS_SUB_COLS:
        m_splitter.TrackPane(perm == 0 ? 1 : 0, showGraph,
            [this, perm]() { m_splitter.SetSplitterPos(perm == 0 ? 1.0 : 0.0); });
        m_subSplitter.TrackPane(1, showFileTypes,
            [this]() { m_subSplitter.SetSplitterPos(1.0); });
        break;

    case LT_COLS_THREE:
        switch (perm)
        {
        case 0: // [FTV|graph] | ExtV
            m_splitter.TrackPane(1, showFileTypes,
                [this]() { m_splitter.SetSplitterPos(1.0); });
            m_subSplitter.TrackPane(1, showGraph,
                [this]() { m_subSplitter.SetSplitterPos(1.0); });
            break;
        case 1: // [graph|FTV] | ExtV
            m_splitter.TrackPane(1, showFileTypes,
                [this]() { m_splitter.SetSplitterPos(1.0); });
            m_subSplitter.TrackPane(0, showGraph,
                [this]() { m_subSplitter.SetSplitterPos(0.0); });
            break;
        case 2: // FTV | [ExtV|graph]
            m_subSplitter.TrackPane(0, showFileTypes,
                [this]() { m_subSplitter.SetSplitterPos(0.0); });
            m_subSplitter.TrackPane(1, showGraph,
                [this]() { m_subSplitter.SetSplitterPos(1.0); });
            break;
        case 3: // graph | [ExtV|FTV]
            m_splitter.TrackPane(0, showGraph,
                [this]() { m_splitter.SetSplitterPos(0.0); });
            m_subSplitter.TrackPane(0, showFileTypes,
                [this]() { m_subSplitter.SetSplitterPos(0.0); });
            break;
        }
        break;

    case LT_COLS_SUB_ROWS:
        m_splitter.TrackPane(1, showFileTypes,
            [this]() { m_splitter.SetSplitterPos(1.0); });
        m_subSplitter.TrackPane(perm == 0 ? 0 : 1, showGraph,
            [this, perm]() { m_subSplitter.SetSplitterPos(perm == 0 ? 0.0 : 1.0); });
        break;

    case LT_COLS_TM_FULL:
    {
        const int graphPane = (perm == 0 || perm == 1) ? 0 : 1;
        const int extPane = (perm == 0 || perm == 2) ? 0 : 1;
        m_splitter.TrackPane(graphPane, showGraph,
            [this, graphPane]() { m_splitter.SetSplitterPos(graphPane == 0 ? 0.0 : 1.0); });
        m_subSplitter.TrackPane(extPane, showFileTypes,
            [this, extPane]() { m_subSplitter.SetSplitterPos(extPane == 0 ? 0.0 : 1.0); });
        break;
    }
    }
}

void CMainFrame::BuildSplitterLayout(int topo, int perm, HWND hFTV, HWND hExtV, HWND hGraph)
{
    auto AttachView = [](CWdsSplitterWnd& splitter, int row, int col, HWND hView)
    {
        ::SetParent(hView, splitter.GetSafeHwnd());
        ::SetWindowLongPtr(hView, GWLP_ID, splitter.IdFromRowCol(row, col));
    };

    switch (topo)
    {
    case LT_ROWS_SUB_COLS:
        m_splitter.CreateStatic(this, 2, 1);
        if (perm == 0) // top: [FTV|ExtV], bottom: graph
        {
            m_subSplitter.CreateStatic(&m_splitter, 1, 2, WS_CHILD | WS_VISIBLE | WS_BORDER,
                                       m_splitter.IdFromRowCol(0, 0));
            AttachView(m_subSplitter, 0, 0, hFTV);
            AttachView(m_subSplitter, 0, 1, hExtV);
            AttachView(m_splitter, 1, 0, hGraph);
        }
        else // perm == 1: graph top, [FTV|ExtV] bottom
        {
            AttachView(m_splitter, 0, 0, hGraph);
            m_subSplitter.CreateStatic(&m_splitter, 1, 2, WS_CHILD | WS_VISIBLE | WS_BORDER,
                                       m_splitter.IdFromRowCol(1, 0));
            AttachView(m_subSplitter, 0, 0, hFTV);
            AttachView(m_subSplitter, 0, 1, hExtV);
        }
        break;

    case LT_COLS_THREE:
        m_splitter.CreateStatic(this, 1, 2);
        if (perm == 0) // [FTV|graph] in col 0, ExtV in col 1
        {
            m_subSplitter.CreateStatic(&m_splitter, 1, 2, WS_CHILD | WS_VISIBLE | WS_BORDER,
                                       m_splitter.IdFromRowCol(0, 0));
            AttachView(m_subSplitter, 0, 0, hFTV);
            AttachView(m_subSplitter, 0, 1, hGraph);
            AttachView(m_splitter, 0, 1, hExtV);
        }
        else if (perm == 1) // [graph|FTV] in col 0, ExtV in col 1
        {
            m_subSplitter.CreateStatic(&m_splitter, 1, 2, WS_CHILD | WS_VISIBLE | WS_BORDER,
                                       m_splitter.IdFromRowCol(0, 0));
            AttachView(m_subSplitter, 0, 0, hGraph);
            AttachView(m_subSplitter, 0, 1, hFTV);
            AttachView(m_splitter, 0, 1, hExtV);
        }
        else if (perm == 2) // FTV in col 0, [ExtV|graph] in col 1
        {
            AttachView(m_splitter, 0, 0, hFTV);
            m_subSplitter.CreateStatic(&m_splitter, 1, 2, WS_CHILD | WS_VISIBLE | WS_BORDER,
                                       m_splitter.IdFromRowCol(0, 1));
            AttachView(m_subSplitter, 0, 0, hExtV);
            AttachView(m_subSplitter, 0, 1, hGraph);
        }
        else // perm 3: graph in col 0, [ExtV|FTV] in col 1
        {
            AttachView(m_splitter, 0, 0, hGraph);
            m_subSplitter.CreateStatic(&m_splitter, 1, 2, WS_CHILD | WS_VISIBLE | WS_BORDER,
                                       m_splitter.IdFromRowCol(0, 1));
            AttachView(m_subSplitter, 0, 0, hExtV);
            AttachView(m_subSplitter, 0, 1, hFTV);
        }
        break;

    case LT_COLS_SUB_ROWS:
        m_splitter.CreateStatic(this, 1, 2);
        m_subSplitter.CreateStatic(&m_splitter, 2, 1, WS_CHILD | WS_VISIBLE | WS_BORDER,
                                   m_splitter.IdFromRowCol(0, 0));
        if (perm == 0) // left: graph/FTV; right: ExtV
        {
            AttachView(m_subSplitter, 0, 0, hGraph);
            AttachView(m_subSplitter, 1, 0, hFTV);
        }
        else // perm 1: left: FTV/graph; right: ExtV
        {
            AttachView(m_subSplitter, 0, 0, hFTV);
            AttachView(m_subSplitter, 1, 0, hGraph);
        }
        AttachView(m_splitter, 0, 1, hExtV);
        break;

    case LT_COLS_TM_FULL:
    {
        m_splitter.CreateStatic(this, 1, 2);
        const int graphCol = (perm == 0 || perm == 1) ? 0 : 1;
        const int extRow = (perm == 0 || perm == 2) ? 0 : 1; // ExtV on top (perm 0/2) or bottom (perm 1/3)
        AttachView(m_splitter, 0, graphCol, hGraph);
        m_subSplitter.CreateStatic(&m_splitter, 2, 1, WS_CHILD | WS_VISIBLE | WS_BORDER,
                                   m_splitter.IdFromRowCol(0, 1 - graphCol));
        AttachView(m_subSplitter, extRow, 0, hExtV);
        AttachView(m_subSplitter, 1 - extRow, 0, hFTV);
        break;
    }
    }
}

void CMainFrame::RebuildLayout(bool resetPositions)
{
    int topo = COptions::LayoutTopology;
    int perm = COptions::LayoutPermutation;
    const bool isDefault = (topo == LT_ROWS_SUB_COLS && perm == 0);
    if (topo == 1 || (!isDefault && CLayoutPopup::LayoutIndex(topo, perm) == 0))
    {
        topo = LT_ROWS_SUB_COLS;
        perm = 0;
        COptions::LayoutTopology = topo;
        COptions::LayoutPermutation = perm;
    }

    // Capture view HWNDs; reparent to frame so DestroyWindow below doesn't kill them.
    const HWND hFTV   = GetFileTabbedView()->GetSafeHwnd();
    const HWND hExtV  = GetExtensionView()->GetSafeHwnd();
    const HWND hTMV   = GetTreeMapView()->GetSafeHwnd();
    const HWND hFGV   = GetFlameGraphView()->GetSafeHwnd();
    const HWND hSBV   = GetSunburstView()->GetSafeHwnd();
    const HWND hFrame = GetSafeHwnd();
    HWND hActiveGraph = hTMV;
    switch (GetGraphPaneType())
    {
    case GraphPane::TreeMap: break;
    case GraphPane::FlameGraph: hActiveGraph = hFGV; break;
    case GraphPane::Sunburst: hActiveGraph = hSBV; break;
    }
    ::SetParent(hFTV,  hFrame);
    ::SetParent(hExtV, hFrame);
    ::SetParent(hTMV,  hFrame);
    ::SetParent(hFGV,  hFrame);
    ::SetParent(hSBV,  hFrame);
    const std::array graphWindows{ hTMV, hFGV, hSBV };
    const std::array<CGraphView*, 3> graphViews{
        GetTreeMapView(), GetFlameGraphView(), GetSunburstView()
    };
    for (const HWND graphWindow : graphWindows)
    {
        if (graphWindow != hActiveGraph)
            ::SetWindowLongPtr(graphWindow, GWLP_ID, ID_INACTIVE_GRAPH_PANE);
    }

    if (m_splitter.GetSafeHwnd())
        m_splitter.DestroyWindow();

    if (resetPositions)
    {
        COptions::MainSplitterPos = -1.0;
        COptions::SubSplitterPos  = -1.0;
    }

    BuildSplitterLayout(topo, perm, hFTV, hExtV, hActiveGraph);
    ::ShowWindow(hActiveGraph, SW_SHOW);
    for (std::size_t index = 0; index < graphWindows.size(); ++index)
    {
        if (graphWindows[index] != hActiveGraph)
        {
            ::ShowWindow(graphWindows[index], SW_HIDE);
            // Hidden panes otherwise retain a full-window bitmap and layout.
            // Rebuild them on demand instead of keeping three large caches.
            graphViews[index]->TrimRenderCache();
        }
    }
    ::ShowWindow(hExtV, SW_SHOW);

    ConfigureSplitterCallbacks(topo, perm);
    m_splitter.SetStorage(COptions::MainSplitterPos.Ptr());
    m_subSplitter.SetStorage(COptions::SubSplitterPos.Ptr());
    RecalcLayout();

    switch (topo)
    {
    case LT_ROWS_SUB_COLS:
        m_splitter.RestoreSplitterPos(0.5);
        m_subSplitter.RestoreSplitterPos(0.75);
        break;
    case LT_COLS_THREE:
        if (perm == 0 || perm == 1)
        {
            m_splitter.RestoreSplitterPos(0.80);
            m_subSplitter.RestoreSplitterPos(0.50);
        }
        else // perm 2, 3: first col = 40%, sub in col 1 gets 60%
        {
            m_splitter.RestoreSplitterPos(0.40);
            m_subSplitter.RestoreSplitterPos(1.0 / 3.0);
        }
        break;
    case LT_COLS_SUB_ROWS:
        m_splitter.RestoreSplitterPos(0.75);
        m_subSplitter.RestoreSplitterPos(0.50);
        break;
    case LT_COLS_TM_FULL:
        m_splitter.RestoreSplitterPos(0.50);
        m_subSplitter.RestoreSplitterPos(0.50);
        break;
    }

    if (!GetExtensionView()->IsShowTypes())
        MinimizeExtensionView();
    if (!IsActiveGraphPaneShown())
        MinimizeGraphPane();

    DarkMode::AdjustControls(GetSafeHwnd());
    GetFileTabbedView()->RedrawWindow();
    GetActiveGraphPane()->RedrawWindow();
    GetExtensionView()->RedrawWindow();
}
