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
#include "CsvLoader.h"
#include "FileTreeView.h"
#include "FileTopControl.h"
#include "FileSearchControl.h"
#include "FileWatcherControl.h"
#include "FilePermsControl.h"
#include "FinderBasic.h"
#include "FinderNtfs.h"
#include "SearchDlg.h"
#include "ProgressDlg.h"
#include "Filtering.h"

void CWinDirStatModel::OnUpdateCentralHandler(CCmdUI* pCmdUI)
{
    struct commandFilter
    {
        bool allowNone = false; // allow display when nothing is selected
        bool allowMany = false; // allow display when multiple items are selected
        bool allowEarly = false; // allow display before processing is finished
        LOGICAL_FOCUS focus = LF_NONE; // restrict which views support this selection
        ITEMTYPE typesAllow = ITF_ANY; // only display if these types are allowed (bitmask)
        bool (*extra)(CItem*) = nullptr; // extra checks
    };

    // special conditions
    static auto model = this;
    static bool (*isZoomed)(CItem*) = [](CItem*) { return CWinDirStatModel::Get()->IsZoomed(); };
    static bool (*canZoomIn)(CItem*) = [](CItem* i) { return i != nullptr && (i = i->IsLeaf() ? i->GetParent() : i) != nullptr && i != model->GetZoomItem() && i->TmiGetSize() > 0; };
    static bool (*canZoomOut)(CItem*) = [](CItem*) { return model->GetZoomItem() != model->GetRootItem(); };
    static bool (*parentNotNull)(CItem*) = [](CItem* i) { return i != nullptr && i->GetParent() != nullptr; };
    static bool (*reselectAvail)(CItem*) = [](CItem*) { return model->IsReselectChildAvailable(); };
    static bool (*notRoot)(CItem*) = [](CItem* item) { return item != nullptr && !item->IsRootItem(); };
    static bool (*hasRecycleBin)(CItem*) = [](CItem* i) { return i != nullptr && !i->IsRootItem() && IsLocalDrive(i->GetPath()); };
    static bool (*isResumable)(CItem*) = [](CItem*) { return CMainFrame::Get()->IsScanSuspended(); };
    static bool (*isSuspendable)(CItem*) = [](CItem*) { return model->HasRootItem() && !model->IsRootDone() && !CMainFrame::Get()->IsScanSuspended(); };
    static bool (*isStoppable)(CItem*) = [](CItem*) { return model->HasRootItem() && !model->IsRootDone(); };
    static bool (*isHibernate)(CItem*) = [](CItem*) { return IsElevationActive() && IsHibernateEnabled(); };
    static bool (*isElevated)(CItem*) = [](CItem*) { return IsElevationActive(); };
    static bool (*isElevationPossible)(CItem*) = [](CItem*) { return IsElevationPossible(); };
    static bool (*isDupeTabVisible)(CItem*) = [](CItem*) { return CMainFrame::Get()->GetFileTabbedView()->IsDupeTabVisible(); };
    static bool (*isPermsTabVisible)(CItem*) = [](CItem*) { return CMainFrame::Get()->GetFileTabbedView()->IsPermsTabVisible(); };
    static bool (*isDriveOrDirOrFile)(CItem*) = [](CItem* i) { return i != nullptr && i->IsTypeOrFlag(IT_DRIVE, IT_DIRECTORY, IT_FILE); };
    static bool (*isVhdFile)(CItem*) = [](CItem* i) { return i != nullptr && IsElevationActive() && (!i->IsTypeOrFlag(IT_FILE) || i->HasExtension(L".vhdx")); };
    static bool (*isStorageSenseAvailable)(CItem*) = [](CItem*) { return IsStorageSenseAvailable(); };

    static std::unordered_map<UINT, const commandFilter> filters
    {
        // ID                           none   many   early  focus        types
        { ID_CLEANUP_DELETE,          { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE, notRoot } },
        { ID_CLEANUP_DELETE_BIN,      { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE, hasRecycleBin } },
        { ID_CLEANUP_DISK_CLEANUP,    { true,  true,  false, LF_NONE,     ITF_ANY, isElevationPossible } },
        { ID_CLEANUP_STORAGE_SENSE,   { true,  true,  false, LF_NONE,     ITF_ANY, isStorageSenseAvailable } },
        { ID_CLEANUP_MOVE_TO,         { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE, notRoot } },
        { ID_CLEANUP_REMOVE_PROGRAMS, { true,  true,  false, LF_NONE,     ITF_ANY } },
        { ID_CLEANUP_DISM_ANALYZE,    { true,  true,  true,  LF_NONE,     ITF_ANY, isElevationPossible } },
        { ID_CLEANUP_DISM_NORMAL,     { true,  true,  false, LF_NONE,     ITF_ANY, isElevationPossible } },
        { ID_CLEANUP_DISM_RESET,      { true,  true,  false, LF_NONE,     ITF_ANY, isElevationPossible } },
        { ID_CLEANUP_EMPTY_BIN,       { true,  true,  false, LF_NONE,     ITF_ANY } },
        { ID_CLEANUP_EMPTY_FOLDER,    { true,  true,  false, LF_NONE,     IT_DIRECTORY, notRoot } },
        { ID_CLEANUP_REMOVE_EMPTY,    { false, true,  false, LF_FILETREE, IT_DRIVE | IT_DIRECTORY } },
        { ID_CLEANUP_EXPLORER_SELECT, { false, true,  true,  LF_NONE,     IT_DIRECTORY | IT_FILE } },
        { ID_CLEANUP_HIBERNATE,       { true,  true,  false, LF_NONE,     ITF_ANY, isHibernate } },
        { ID_CLEANUP_OPEN_IN_CONSOLE, { false, true,  true,  LF_NONE,     IT_DRIVE | IT_DIRECTORY | IT_FILE } },
        { ID_CLEANUP_OPEN_IN_PWSH,    { false, true,  true,  LF_NONE,     IT_DRIVE | IT_DIRECTORY | IT_FILE } },
        { ID_CLEANUP_OPEN_SELECTED,   { false, true,  true,  LF_NONE,     IT_MYCOMPUTER | IT_DRIVE | IT_DIRECTORY | IT_FILE } },
        { ID_CLEANUP_PROPERTIES,      { false, true,  true,  LF_NONE,     IT_MYCOMPUTER | IT_DRIVE | IT_DIRECTORY | IT_FILE } },
        { ID_CLEANUP_OPTIMIZE_VHD,    { false, true,  false, LF_NONE,     IT_DRIVE | IT_DIRECTORY | IT_FILE, isVhdFile } },
        { ID_CLEANUP_REMOVE_LOCAL,    { true,  true,  false, LF_NONE,     ITF_ANY, isElevated } },
        { ID_CLEANUP_REMOVE_MOTW,     { false, true,  false, LF_NONE,     IT_DRIVE | IT_DIRECTORY | IT_FILE } },
        { ID_CLEANUP_SPARSIFY_FILE,   { false, true,  false, LF_NONE,     IT_FILE } },
        { ID_CLEANUP_REMOVE_ROAMING,  { true,  true,  false, LF_NONE,     ITF_ANY, isElevated } },
        { ID_CLEANUP_REMOVE_SHADOW,   { true,  true,  false, LF_NONE,     ITF_ANY, isElevated } },
        { ID_COMPRESS_LZNT1,          { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE } },
        { ID_COMPRESS_LZX,            { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE } },
        { ID_COMPRESS_NONE,           { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE } },
        { ID_COMPRESS_XPRESS16K,      { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE } },
        { ID_COMPRESS_XPRESS4K,       { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE } },
        { ID_COMPRESS_XPRESS8K,       { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE } },
        { ID_COMPUTE_HASH,            { false, false, true,  LF_NONE,     IT_FILE } },
        { ID_EDIT_COPY_CLIPBOARD,     { false, true,  true,  LF_NONE,     IT_DRIVE | IT_DIRECTORY | IT_FILE } },
        { ID_FILTER,                  { true,  true,  true,  LF_NONE,     ITF_ANY } },
        { ID_FILTER_EXCLUDE_ITEM,     { false, true,  false, LF_NONE,     ITF_ANY, isDriveOrDirOrFile } },
        { ID_INDICATOR_DISK,          { true,  true,  false, LF_NONE,     ITF_ANY } },
        { ID_INDICATOR_IDLE,          { true,  true,  true,  LF_NONE,     ITF_ANY } },
        { ID_INDICATOR_RAM,           { true,  true,  true,  LF_NONE,     ITF_ANY } },
        { ID_INDICATOR_SIZE,          { true,  true,  false, LF_NONE,     ITF_ANY } },
        { ID_POPUP_CANCEL,            { true,  true,  true,  LF_NONE,     ITF_ANY } },
        { ID_REFRESH_ALL,             { true,  true,  false, LF_NONE,     ITF_ANY } },
        { ID_REFRESH_SELECTED,        { false, true,  false, LF_NONE,     IT_MYCOMPUTER | IT_DRIVE | IT_DIRECTORY | IT_FILE } },
        { ID_SAVE_DUPLICATES,         { true,  true,  false, LF_NONE,     ITF_ANY, isDupeTabVisible } },
        { ID_SAVE_PERMISSIONS,        { true,  true,  false, LF_NONE,     ITF_ANY, isPermsTabVisible } },
        { ID_SAVE_RESULTS,            { true,  true,  false, LF_NONE,     ITF_ANY } },
        { ID_SCAN_RESUME,             { true,  true,  true,  LF_NONE,     ITF_ANY, isResumable } },
        { ID_SCAN_STOP,               { true,  true,  true,  LF_NONE,     ITF_ANY, isStoppable } },
        { ID_SCAN_SUSPEND,            { true,  true,  true,  LF_NONE,     ITF_ANY, isSuspendable } },
        { ID_SEARCH,                  { true,  true,  false, LF_NONE,     ITF_ANY } },
        { ID_TOOLS_SET_DATES,         { true,  true,  false, LF_FILETREE, IT_DRIVE | IT_DIRECTORY } },
        { ID_TREEMAP_RESELECT_CHILD,  { true,  true,  false, LF_FILETREE, ITF_ANY, reselectAvail } },
        { ID_TREEMAP_SELECT_PARENT,   { false, false, false, LF_FILETREE, ITF_ANY, parentNotNull } },
        { ID_TREEMAP_ZOOMRESET,       { true,  true,  false, LF_FILETREE, ITF_ANY, isZoomed } },
        { ID_TREEMAP_ZOOMIN,          { false, false, false, LF_FILETREE, IT_MYCOMPUTER | IT_DRIVE | IT_DIRECTORY | IT_FILE, canZoomIn } },
        { ID_TREEMAP_ZOOMOUT,         { true,  true,  false, LF_FILETREE, ITF_ANY, canZoomOut } },
        { ID_VIEW_SHOWFREESPACE,      { true,  true,  false, LF_NONE,     ITF_ANY } },
        { ID_VIEW_SHOWUNKNOWN,        { true,  true,  false, LF_NONE,     ITF_ANY } }
    };

    const auto it = filters.find(pCmdUI->m_nID);
    if (it == filters.end())
    {
        ASSERT(FALSE);
        return;
    }

    const auto& filter = it->second;
    bool allow = filter.focus == LF_NONE || (CMainFrame::Get()->GetLogicalFocus() & filter.focus) > 0;
    allow &= filter.allowEarly || IsScanSettled();
    if (!allow) { pCmdUI->Enable(false); return; }

    const auto items = (!filter.allowNone || filter.extra != nullptr) ? GetAllSelected() : std::vector<CItem*>{};
    allow &= filter.allowNone || !items.empty();
    allow &= filter.allowMany || items.size() <= 1;
    if (items.empty() && filter.extra != nullptr) allow &= filter.extra(nullptr);
    for (const auto& item : items)
    {
        if (!allow) break;
        allow &= filter.typesAllow == ITF_ANY || (!item->IsTypeOrFlag(ITF_RESERVED) && item->IsTypeOrFlag(filter.typesAllow));
        allow &= filter.extra == nullptr || filter.extra(item);
    }
    pCmdUI->Enable(allow);
}

void CWinDirStatModel::OnUpdateCompressionHandler(CCmdUI* pCmdUI)
{
    // Defer to standard update handler for initial value
    OnUpdateCentralHandler(pCmdUI);
    if (pCmdUI->m_pMenu == nullptr) return;

    // See if each path supports available compression options
    bool allow = IsMenuEnabled(pCmdUI->m_pMenu, pCmdUI->m_nID, true);
    for (const auto& item : GetAllSelected())
    {
        allow &= CompressFileAllowed(item->GetVolumeRoot()->GetPath(),
            CompressionIdToAlg(pCmdUI->m_nID));
    }
    pCmdUI->Enable(allow);
}

#define ON_COMMAND_UPDATE_WRAPPER(x,y) ON_COMMAND(x, y) ON_UPDATE_COMMAND_UI(x, OnUpdateCentralHandler)
BEGIN_MESSAGE_MAP(CWinDirStatModel, CCmdTarget)
    ON_COMMAND_UPDATE_WRAPPER(ID_REFRESH_SELECTED, OnRefreshSelected)
    ON_COMMAND_UPDATE_WRAPPER(ID_REFRESH_ALL, OnRefreshAll)
    ON_COMMAND(ID_LOAD_RESULTS, OnLoadResults)
    ON_COMMAND_UPDATE_WRAPPER(ID_SAVE_RESULTS, OnSaveResults)
    ON_COMMAND_UPDATE_WRAPPER(ID_SAVE_DUPLICATES, OnSaveDuplicates)
    ON_COMMAND_UPDATE_WRAPPER(ID_SAVE_PERMISSIONS, OnSavePermissions)
    ON_COMMAND_UPDATE_WRAPPER(ID_EDIT_COPY_CLIPBOARD, OnEditCopy)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_EMPTY_BIN, OnCleanupEmptyRecycleBin)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_MOVE_TO, OnCleanupMoveTo)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWFREESPACE, OnUpdateViewShowFreeSpace)
    ON_COMMAND(ID_VIEW_SHOWFREESPACE, OnViewShowFreeSpace)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWUNKNOWN, OnUpdateViewShowUnknown)
    ON_COMMAND(ID_VIEW_SHOWUNKNOWN, OnViewShowUnknown)
    ON_COMMAND_UPDATE_WRAPPER(ID_TREEMAP_ZOOMIN, OnTreeMapZoomIn)
    ON_COMMAND_UPDATE_WRAPPER(ID_TREEMAP_ZOOMOUT, OnTreeMapZoomOut)
    ON_COMMAND_UPDATE_WRAPPER(ID_TREEMAP_ZOOMRESET, OnTreeMapZoomReset)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_EXPLORER_SELECT, OnExplorerSelect)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_OPEN_IN_CONSOLE, OnCommandPromptHere)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_OPEN_IN_PWSH, OnPowerShellHere)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_DELETE_BIN, OnCleanupDeleteToBin)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_DELETE, OnCleanupDelete)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_EMPTY_FOLDER, OnCleanupEmptyFolder)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_REMOVE_EMPTY, OnCleanupRemoveEmpty)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_REMOVE_SHADOW, OnRemoveShadowCopies)
    ON_COMMAND_UPDATE_WRAPPER(ID_SEARCH, OnSearch)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_DISM_ANALYZE, OnExecuteDismAnalyze)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_DISM_NORMAL, OnExecuteDism)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_DISM_RESET, OnExecuteDismReset)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_HIBERNATE, OnDisableHibernateFile)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_REMOVE_ROAMING, OnRemoveRoamingProfiles)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_REMOVE_LOCAL, OnRemoveLocalProfiles)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_DISK_CLEANUP, OnExecuteDiskCleanupUtility)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_STORAGE_SENSE, OnLaunchStorageSense)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_REMOVE_PROGRAMS, OnExecuteProgramsFeatures)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_REMOVE_MOTW, OnRemoveMarkOfTheWebTags)
    ON_UPDATE_COMMAND_UI(ID_CLEANUP_CREATE_HARDLINK, OnUpdateCreateHardlink)
    ON_COMMAND(ID_CLEANUP_CREATE_HARDLINK, OnCreateHardlink)
    ON_UPDATE_COMMAND_UI_RANGE(ID_USERDEFINEDCLEANUP0, ID_USERDEFINEDCLEANUP9, OnUpdateUserDefinedCleanup)
    ON_COMMAND_RANGE(ID_USERDEFINEDCLEANUP0, ID_USERDEFINEDCLEANUP9, OnUserDefinedCleanup)
    ON_COMMAND_UPDATE_WRAPPER(ID_TREEMAP_SELECT_PARENT, OnTreeMapSelectParent)
    ON_COMMAND_UPDATE_WRAPPER(ID_TREEMAP_RESELECT_CHILD, OnTreeMapReselectChild)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_OPEN_SELECTED, OnCleanupOpenTarget)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_PROPERTIES, OnCleanupProperties)
    ON_COMMAND_UPDATE_WRAPPER(ID_COMPUTE_HASH, OnComputeHash)
    ON_UPDATE_COMMAND_UI_RANGE(ID_COMPRESS_NONE, ID_COMPRESS_LZX, OnUpdateCompressionHandler)
    ON_COMMAND_RANGE(ID_COMPRESS_NONE, ID_COMPRESS_LZX, OnCleanupCompress)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_OPTIMIZE_VHD, OnCleanupOptimizeVhd)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_SPARSIFY_FILE, OnCleanupSparsifyFile)
    ON_COMMAND_UPDATE_WRAPPER(ID_TOOLS_SET_DATES, OnToolsSetDates)
    ON_COMMAND_UPDATE_WRAPPER(ID_SCAN_RESUME, OnScanResume)
    ON_COMMAND_UPDATE_WRAPPER(ID_SCAN_SUSPEND, OnScanSuspend)
    ON_COMMAND_UPDATE_WRAPPER(ID_SCAN_STOP, OnScanStop)
    ON_COMMAND_UPDATE_WRAPPER(ID_POPUP_CANCEL, OnPopupCancel)
    ON_COMMAND_UPDATE_WRAPPER(ID_FILTER_EXCLUDE_ITEM, OnFilterExcludeItem)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_RAM, OnUpdateCentralHandler)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_DISK, OnUpdateCentralHandler)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_IDLE, OnUpdateCentralHandler)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_SIZE, OnUpdateCentralHandler)
    ON_UPDATE_COMMAND_UI(ID_CLEANUP_DISK_CLEANUP, OnUpdateCentralHandler)
    ON_COMMAND_RANGE(CONTENT_MENU_MINCMD, CONTENT_MENU_MAXCMD, OnContextMenuExplore)
END_MESSAGE_MAP()

void CWinDirStatModel::OnFilterExcludeItem()
{
    const auto& selected = GetAllSelected();
    for (const auto* item : selected)
    {
        const bool isFile = item->IsTypeOrFlag(IT_FILE);
        const std::wstring value = isFile ? item->GetName() : item->GetPath();
        std::wstring& current = isFile ?
            COptions::FilteringExcludeFiles.Obj() : COptions::FilteringExcludeDirs.Obj();
        if (!current.empty() && current.back() != L'\n') current += L"\r\n";
        current += value;
    }

    CFiltering::CompileFilters();
    RefreshItem(selected);
}

void CWinDirStatModel::OnCleanupSparsifyFile()
{
    // Only sparsify files (no recursion)
    const auto& itemsSelected = GetAllSelected();
    CProgressDlg(itemsSelected.size(), CProgressDlg::Flags::None, AfxGetMainWnd(), [&](CProgressDlg* pdlg)
    {
        for (const auto* item : itemsSelected)
        {
            if (pdlg->IsCancelled()) break;

            if (!SparsifyFile(item->GetPathLong()))
            {
                DisplayError(TranslateError());
            }

            pdlg->Increment();
        }
    }).DoModal();

    RefreshItem(itemsSelected);
}

void CWinDirStatModel::OnRefreshSelected()
{
    // Optimize refresh selected when done on root item
    const auto& selected = GetAllSelected();
    if (selected.size() == 1 && selected.front() == GetRootItem()) OnRefreshAll();
    else RefreshItem(selected);
}

void CWinDirStatModel::OnRefreshAll()
{
    StartScan(Get()->GetScanPathSpec());
}

void CWinDirStatModel::OnSaveResults()
{
    // Request the file path from the user
    const std::wstring fileSelectString = std::format(L"{} (*.csv;*.json)|*.csv;*.json|{} (*.*)|*.*||",
        Localization::Lookup(IDS_FILE_FILTER), Localization::Lookup(IDS_ALL_FILES));
    CFileDialog dlg(FALSE, L"csv", nullptr, OFN_EXPLORER | OFN_DONTADDTORECENT, fileSelectString.c_str());
    if (dlg.DoModal() != IDOK) return;

    CProgressDlg(0, CProgressDlg::Flags::NoCancel, AfxGetMainWnd(), [&](CProgressDlg*)
    {
        SaveResults(dlg.GetPathName().GetString(), GetRootItem());
    }).DoModal();
}

void CWinDirStatModel::OnSaveDuplicates()
{
    // Request the file path from the user
    const std::wstring fileSelectString = std::format(L"{} (*.csv;*.json)|*.csv;*.json|{} (*.*)|*.*||",
        Localization::Lookup(IDS_FILE_FILTER), Localization::Lookup(IDS_ALL_FILES));
    CFileDialog dlg(FALSE, L"csv", nullptr, OFN_EXPLORER | OFN_DONTADDTORECENT, fileSelectString.c_str());
    if (dlg.DoModal() != IDOK) return;

    CProgressDlg(0, CProgressDlg::Flags::NoCancel, AfxGetMainWnd(), [&](CProgressDlg*)
    {
        SaveDuplicates(dlg.GetPathName().GetString(), CFileDupeControl::Get()->GetRootItem());
    }).DoModal();
}

void CWinDirStatModel::OnSavePermissions()
{
    // Request the file path from the user
    const std::wstring fileSelectString = std::format(L"{} (*.csv;*.json)|*.csv;*.json|{} (*.*)|*.*||",
        Localization::Lookup(IDS_FILE_FILTER), Localization::Lookup(IDS_ALL_FILES));
    CFileDialog dlg(FALSE, L"csv", nullptr, OFN_EXPLORER | OFN_DONTADDTORECENT, fileSelectString.c_str());
    if (dlg.DoModal() != IDOK) return;

    CProgressDlg(0, CProgressDlg::Flags::NoCancel, AfxGetMainWnd(), [&](CProgressDlg*)
    {
        SavePermissions(dlg.GetPathName().GetString(), CFilePermsControl::Get()->GetPermItems());
    }).DoModal();
}

void CWinDirStatModel::OnLoadResults()
{
    // Request the file path from the user
    const std::wstring fileSelectString = std::format(L"{} (*.csv;*.json)|*.csv;*.json|{} (*.*)|*.*||",
        Localization::Lookup(IDS_FILE_FILTER), Localization::Lookup(IDS_ALL_FILES));
    CFileDialog dlg(TRUE, L"csv", nullptr, OFN_EXPLORER | OFN_DONTADDTORECENT | OFN_PATHMUSTEXIST, fileSelectString.c_str());
    if (dlg.DoModal() != IDOK) return;

    CItem* newroot = nullptr;
    CProgressDlg(0, CProgressDlg::Flags::NoCancel, AfxGetMainWnd(), [&](CProgressDlg*)
    {
        newroot = LoadResults(dlg.GetPathName().GetString());
    }).DoModal();

    if (newroot != nullptr) Get()->OpenLoadedScan(newroot);
}

void CWinDirStatModel::OnEditCopy()
{
    // create concatenated paths
    std::wstring paths;
    for (const auto & item : GetAllSelected())
    {
        if (!paths.empty()) paths += L"\r\n";
        paths += item->GetPath();
    }

    CMainFrame::Get()->CopyToClipboard(paths);
}

void CWinDirStatModel::OnCleanupEmptyRecycleBin()
{
    if (!ConfirmOperation(IDS_MENU_EMPTY_BIN, COptions::ShowEmptyRecycleBinPrompt)) return;

    CProgressDlg(0, CProgressDlg::Flags::NoCancel, AfxGetMainWnd(), [](CProgressDlg*)
    {
        SHEmptyRecycleBin(*AfxGetMainWnd(), nullptr,
            SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
    }).DoModal();

    // locate all drive items in order to refresh recyclers
    std::vector<CItem*> toRefresh;
    for (const auto& drive : GetRootItem()->GetDriveItems())
    {
        if (CItem* recycler = drive->FindRecyclerItem(); recycler != nullptr)
        {
            toRefresh.push_back(recycler);
        }
    }

    // refresh recyclers
    if (!toRefresh.empty()) Get()->StartScanningEngine(toRefresh);
}

void CWinDirStatModel::OnRemoveShadowCopies()
{
    ULONGLONG count = 0, bytesUsed = 0;
    QueryShadowCopies(count, bytesUsed);
    if (count == 0 || !ConfirmOperation(IDS_MENU_REMOVE_SHADOW, COptions::ShowRemoveShadowCopiesPrompt)) return;

    CProgressDlg(static_cast<size_t>(count), CProgressDlg::Flags::None, AfxGetMainWnd(), [](CProgressDlg* pdlg)
    {
        RemoveWmiInstances(L"Win32_ShadowCopy", pdlg);
    }).DoModal();

    GetRootItem()->UpdateFreeSpaceItem();
}

void CWinDirStatModel::OnUpdateViewShowFreeSpace(CCmdUI* pCmdUI)
{
    OnUpdateCentralHandler(pCmdUI);
    pCmdUI->SetCheck(COptions::ShowFreeSpace);
}

void CWinDirStatModel::OnViewShowFreeSpace()
{
    for (const auto& drive : GetRootItem()->GetDriveItems())
    {
        if (COptions::ShowFreeSpace)
        {
            const CItem* free = drive->FindFreeSpaceItem();
            ASSERT(free != nullptr);

            if (GetZoomItem() == free)
            {
                m_zoomItem = free->GetParent();
            }

            drive->RemoveFreeSpaceItem();
        }
        else
        {
            drive->CreateFreeSpaceItem();
        }
    }

    // Toggle value
    COptions::ShowFreeSpace = !COptions::ShowFreeSpace;

    // Force recalculation and graph refresh
    StartScanningEngine({});
}

void CWinDirStatModel::OnUpdateViewShowUnknown(CCmdUI* pCmdUI)
{
    OnUpdateCentralHandler(pCmdUI);
    pCmdUI->SetCheck(COptions::ShowUnknown);
}

void CWinDirStatModel::OnViewShowUnknown()
{
    for (const auto& drive : GetRootItem()->GetDriveItems())
    {
        if (COptions::ShowUnknown)
        {
            const CItem* unknown = drive->FindUnknownItem();
            ASSERT(unknown != nullptr);

            if (GetZoomItem() == unknown)
            {
                m_zoomItem = unknown->GetParent();
            }

            drive->RemoveUnknownItem();
        }
        else
        {
            drive->CreateUnknownItem();
        }
    }

    // Toggle value
    COptions::ShowUnknown = !COptions::ShowUnknown;

    // Force recalculation and graph refresh
    StartScanningEngine({});
}

void CWinDirStatModel::OnTreeMapZoomIn()
{
    const auto & item = CFileTreeControl::Get()->GetFirstSelectedItem<CItem>();
    if (item != nullptr)
    {
        SetZoomItem(item->IsRootItem() ? GetRootItem() :
            item->IsTypeOrFlag(IT_FILE) ? item->GetParent() : item);
        if (!CMainFrame::Get()->IsVisualizationShown())
            CMainFrame::Get()->RestoreVisualizationPane(true);
    }
}

void CWinDirStatModel::OnTreeMapZoomOut()
{
    CItem* zoomItem = GetZoomItem();
    if (zoomItem != nullptr && zoomItem->GetParent() != nullptr)
    {
        SetZoomItem(zoomItem->GetParent());
        if (!CMainFrame::Get()->IsVisualizationShown())
            CMainFrame::Get()->RestoreVisualizationPane(true);
    }
}

void CWinDirStatModel::OnTreeMapZoomReset()
{
    if (IsZoomed())
    {
        SetZoomItem(GetRootItem());
    }
}

void CWinDirStatModel::OnExplorerSelect()
{
    // accumulate a unique set of paths
    const auto& items = GetAllSelected();
    std::unordered_set<std::wstring>paths;
    for (const auto& item : items)
    {
        // use function to determine parent to address non-drive rooted paths
        std::filesystem::path target(item->GetPath());
        paths.insert(target.parent_path());
    }

    for (const auto& path : paths)
    {
        // create path pidl
        SmartPointer parent(CoTaskMemFree, static_cast<LPITEMIDLIST>(nullptr));
        parent = ILCreateFromPath(path.c_str());

        // ignore unresolvable (e.g., deleted) files
        if (parent == nullptr)
        {
            ASSERT(FALSE);
            return;
        }

        // structures to hold and track pidls for children
        std::vector<SmartPointer<LPITEMIDLIST, decltype(&CoTaskMemFree)>> pidlCleanup;
        std::vector<LPITEMIDLIST> pidl;

        // create list of children from paths
        for (auto & item : items)
        {
            // not processing this path yet
            std::filesystem::path target(item->GetPath());
            if (target.parent_path() == path)
            {
                pidl.push_back(ILCreateFromPath(item->GetPath().c_str()));
                pidlCleanup.emplace_back(CoTaskMemFree, pidl.back());
            }
        }

        // attempt to open the items in the shell
        (void) SHOpenFolderAndSelectItems(parent, static_cast<UINT>(pidl.size()),
            const_cast<LPCITEMIDLIST*>(pidl.data()), 0);
    }
}

void CWinDirStatModel::OnCommandPromptHere()
{
    // accumulate a unique set of paths
    const auto& items = GetAllSelected();
    std::unordered_set<std::wstring>paths;
    for (const auto& item : items)
    {
        paths.insert(item->GetFolderPath());
    }

    // launch a command prompt for each path
    const std::wstring cmd = GetCOMSPEC();
    for (const auto& path : paths)
    {
        // If using command prompt, use pushd to force a drive mount
        std::wstring uncmod = path.starts_with(L"\\\\") ? std::format(L"&& PUSHD \"{}\" && CLS", path) : L"";
        std::wstring params = std::format(L"/K TITLE {} - \"{}\" {}", wds::strWinDirStat, path, uncmod);

        // Launch command prompt
        ShellExecuteWrapper(cmd, params, L"open", *AfxGetMainWnd(), path);
    }
}

void CWinDirStatModel::OnPowerShellHere()
{
    // locate PWSH
    static std::wstring pwsh(MAX_PATH, L'\0');
    if (!pwsh.front()) for (const auto exe : { L"pwsh.exe", L"powershell.exe" })
    {
        if (SearchPath(nullptr, exe, nullptr,
            static_cast<DWORD>(pwsh.size()), pwsh.data(), nullptr) > 0)
        {
            pwsh.resize(wcslen(pwsh.data()));
            break;
        }
    }

    // accumulate a unique set of paths
    const auto& items = GetAllSelected();
    std::unordered_set<std::wstring>paths;
    for (const auto& item : items)
    {
        paths.insert(item->GetFolderPath());
    }

    // launch a command prompt for each path
    for (const auto& path : paths)
    {
        ShellExecuteWrapper(pwsh, L"", L"open", *AfxGetMainWnd(), path);
    }
}

void CWinDirStatModel::OnCleanupDeleteToBin()
{
    DeletePhysicalItems(GetAllSelected(), true);
}

void CWinDirStatModel::OnCleanupDelete()
{
    DeletePhysicalItems(GetAllSelected(), false);
}

void CWinDirStatModel::OnCleanupEmptyFolder()
{
    DeletePhysicalItems(GetAllSelected(), false, true);
}

void CWinDirStatModel::OnCleanupMoveTo()
{
    const auto& items = GetAllSelected();
    if (items.empty()) return;

    // Show folder browser dialog to get destination directory
    CFolderPickerDialog dlg(nullptr, OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_DONTADDTORECENT);
    dlg.m_ofn.lpstrTitle = wds::strWinDirStat;

    if (dlg.DoModal() != IDOK) return;
    const std::wstring destFolder = dlg.GetPathName().GetString();

    // Verify destination exists
    if (!FolderExists(destFolder)) return;

    // Show progress dialog and move files
    CProgressDlg(0, CProgressDlg::Flags::None, AfxGetMainWnd(), [&](const CProgressDlg* pdlg)
    {
        // Create file operation object
        CComPtr<IFileOperation> fileOperation;
        CComPtr<IShellItem> destShellItem;
        const auto flags = FOFX_SHOWELEVATIONPROMPT | FOF_NOCONFIRMATION | (COptions::ShowMicrosoftProgress ? 0 : FOF_NO_UI);
        if (FAILED(::CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fileOperation))) ||
            FAILED(fileOperation->SetOwnerWindow(*pdlg)) ||
            FAILED(fileOperation->SetOperationFlags(flags)) ||
            FAILED(SHCreateItemFromParsingName(destFolder.c_str(), nullptr, IID_PPV_ARGS(&destShellItem))))
        {
            return;
        }

        if (const CComPtr<IShellItemArray> psia = CreateShellItemArray(items))
            fileOperation->MoveItems(psia, destShellItem);

        // Do all moves
        const HRESULT res = fileOperation->PerformOperations();
        if (res != S_OK) VTRACE(L"File Operation Failed: {}", TranslateError(res));
    }).DoModal();

    // Refresh the parent items of the moved files
    std::vector<CItem*> refresh;
    for (const auto& item : items)
    {
        if (item->GetParent() != nullptr &&
            std::ranges::find(refresh, item->GetParent()) == refresh.end())
        {
            refresh.push_back(item->GetParent());
        }
    }

    // Add in destination directory as a refresh location if present
    if (const auto destItem = GetRootItem()->FindItemByPath(destFolder); destItem != nullptr)
    {
        refresh.push_back(destItem);
    }

    if (!refresh.empty())
    {
        RefreshItem(refresh);
    }
}

void CWinDirStatModel::OnSearch()
{
    SearchDlg search;
    search.DoModal();
}

void CWinDirStatModel::OnDisableHibernateFile()
{
    if (!ConfirmOperation(IDS_MENU_DISABLE_HIBERNATE, COptions::ShowDisableHibernatePrompt)) return;

    DisableHibernate();

    // See if there is a hibernate file on any drive to refresh
    for (const auto& drive : GetRootItem()->GetDriveItems())
    {
        for (const auto& child : drive->GetChildren())
        {
            if (_wcsicmp(child->GetNameView().data(), L"hiberfil.sys") == 0)
            {
                StartScanningEngine({ child });
            }
        }
    }
}

void CWinDirStatModel::OnRemoveRoamingProfiles()
{
    RemoveLocalProfiles(L"RoamingConfigured = TRUE");
}

void CWinDirStatModel::OnRemoveLocalProfiles()
{
    RemoveLocalProfiles(L"RoamingConfigured = FALSE AND Loaded = FALSE AND Special = FALSE");
}

void CWinDirStatModel::RemoveLocalProfiles(const std::wstring_view whereClause)
{
    const auto paths = QueryWmiStringProperty(L"Win32_UserProfile", L"LocalPath", whereClause.data());
    if (paths.empty()) return;

    const auto result = CMessageBoxDlg::Show(Localization::Lookup(IDS_DELETE_WARNING), paths,
        {}, false, MB_YESNO | MB_ICONWARNING, AfxGetMainWnd(), { 600, 400 },
        Localization::Lookup(IDS_DELETE_TITLE));
    if (result.nID != IDYES) return;

    CProgressDlg(paths.size(), CProgressDlg::Flags::None, AfxGetMainWnd(), [&](CProgressDlg* pdlg)
    {
        RemoveWmiInstances(L"Win32_UserProfile", pdlg, whereClause.data());
    }).DoModal();

    GetRootItem()->UpdateFreeSpaceItem();
    SmartPointer profilePath(CoTaskMemFree, static_cast<PWSTR>(nullptr));
    if (SHGetKnownFolderPath(FOLDERID_UserProfiles, 0, nullptr, &profilePath) == S_OK && profilePath)
    {
        if (CItem* profileItem = GetRootItem()->FindItemByPath(profilePath.Get()); profileItem != nullptr)
        {
            RefreshItem(profileItem);
        }
    }
}

void CWinDirStatModel::OnExecuteDiskCleanupUtility()
{
    ShellExecuteWrapper(L"CLEANMGR.EXE");
}

void CWinDirStatModel::OnLaunchStorageSense()
{
    ShellExecuteWrapper(L"ms-settings:storagesense");
}

void CWinDirStatModel::OnExecuteProgramsFeatures()
{
    ShellExecuteWrapper(L"appwiz.cpl");
}

void CWinDirStatModel::OnExecuteDismAnalyze()
{
    ExecuteCommandInConsole(L"DISM.EXE /Online /Cleanup-Image /AnalyzeComponentStore", L"DISM");
}

void CWinDirStatModel::OnExecuteDismReset()
{
    if (!ConfirmOperation(IDS_MENU_DISM, COptions::ShowDismResetPrompt, L"/StartComponentCleanup /ResetBase")) return;

    ExecuteCommandInConsole(L"DISM.EXE /Online /Cleanup-Image /StartComponentCleanup /ResetBase", L"DISM");
}

void CWinDirStatModel::OnExecuteDism()
{
    if (!ConfirmOperation(IDS_MENU_DISM, COptions::ShowDismCleanupPrompt, L"/StartComponentCleanup")) return;

    ExecuteCommandInConsole(L"DISM.EXE /Online /Cleanup-Image /StartComponentCleanup", L"DISM");
}

void CWinDirStatModel::OnUpdateUserDefinedCleanup(CCmdUI* pCmdUI)
{
    const int i = pCmdUI->m_nID - ID_USERDEFINEDCLEANUP0;
    if (!IsScanSettled())
    {
        return pCmdUI->Enable(FALSE);
    }

    const auto & items = GetAllSelected();
    bool allowControl = (FileTreeHasFocus() || DupeListHasFocus() || TopListHasFocus()) &&
        COptions::UserDefinedCleanups.at(i).Enabled && !items.empty();
    if (allowControl) for (const auto & item : items)
    {
        allowControl &= UserDefinedCleanupWorksForItem(&COptions::UserDefinedCleanups[i], item);
    }

    pCmdUI->Enable(allowControl);
}

void CWinDirStatModel::OnUserDefinedCleanup(const UINT id)
{
    if (!IsScanSettled())
    {
        return;
    }

    USERDEFINEDCLEANUP* udc = &COptions::UserDefinedCleanups[id - ID_USERDEFINEDCLEANUP0];
    const auto & items = GetAllSelected();
    std::vector<CItem*> refreshQueue;
    for (const auto & item : items)
    {
        ASSERT(UserDefinedCleanupWorksForItem(udc, item));
        if (!UserDefinedCleanupWorksForItem(udc, item))
        {
            return;
        }

        try
        {
            AskForConfirmation(udc, item);
            PerformUserDefinedCleanup(udc, item);
            RefreshAfterUserDefinedCleanup(udc, item, refreshQueue);
        }
        catch (...)
        {
            // error would have been displayed already
        }
    }

    // process refresh queue
    if (!refreshQueue.empty())
    {
        RefreshItem(refreshQueue);
    }
}

void CWinDirStatModel::OnTreeMapSelectParent()
{
    const auto & item = CFileTreeControl::Get()->GetFirstSelectedItem<CItem>();
    if (item == nullptr || item->GetParent() == nullptr) return;

    PushReselectChild(item);
    CFileTreeControl::Get()->SelectItem(item->GetParent(), true, true, true);
    NotifyPanes(MODEL_CHANGE_SELECTION_REFRESH);
}

void CWinDirStatModel::OnTreeMapReselectChild()
{
    const CItem* item = PopReselectChild();
    if (item == nullptr) return;

    CFileTreeControl::Get()->ExpandPathToItem(item); // ensure item is visible before selecting
    CFileTreeControl::Get()->SelectItem(item, true, true, true);
    NotifyPanes(MODEL_CHANGE_SELECTION_REFRESH);
}

void CWinDirStatModel::OnCleanupOpenTarget()
{
    for (const auto & item : GetAllSelected())
    {
        OpenItem(item);
    }
}

void CWinDirStatModel::OnCleanupProperties()
{
    const auto& selected = GetAllSelected();
    const CComPtr<IShellItemArray> psia = CreateShellItemArray(selected);
    if (!psia) return;

    CComPtr<IDataObject> pDataObj;
    if (SUCCEEDED(psia->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(&pDataObj))) &&
        SUCCEEDED(SHMultiFileProperties(pDataObj, 0))) return;

    for (const auto& item : selected)
        OpenItem(item, L"properties");
}

void CWinDirStatModel::OnComputeHash()
{
    // Compute the hash in the message thread
    std::wstring hashResult;
    const auto& items = GetAllSelected();
    const ULONGLONG logicalSize = items.front()->GetSizeLogical();
    const size_t totalBlocks = static_cast<size_t>(logicalSize / wds::Mi + (logicalSize % wds::Mi != 0));
    CProgressDlg(totalBlocks, CProgressDlg::Flags::PercentageOnly, AfxGetMainWnd(), [&](CProgressDlg* pdlg)
    {
        hashResult = ComputeFileHashes(items.front()->GetPath(), pdlg);
    }).DoModal();

    if (!hashResult.empty())
    {
        // Display result in message box
        CMessageBoxDlg dlg(hashResult, wds::strWinDirStat, MB_OK | MB_ICONINFORMATION);
        dlg.SetWidthAuto();
        dlg.DoModal();
    }
}

CompressionAlgorithm CWinDirStatModel::CompressionIdToAlg(const UINT id)
{
    switch (id)
    {
        case ID_COMPRESS_NONE: return CompressionAlgorithm::NONE;
        case ID_COMPRESS_LZNT1: return  CompressionAlgorithm::LZNT1;
        case ID_COMPRESS_XPRESS4K: return  CompressionAlgorithm::XPRESS4K;
        case ID_COMPRESS_XPRESS8K: return  CompressionAlgorithm::XPRESS8K;
        case ID_COMPRESS_XPRESS16K: return  CompressionAlgorithm::XPRESS16K;
        case ID_COMPRESS_LZX: return  CompressionAlgorithm::LZX;
        default: return CompressionAlgorithm::NONE;
    }
}

void CWinDirStatModel::OnCleanupCompress(UINT id)
{
    CWaitCursor wc;
    const auto& itemsSelected = GetAllSelected();
    const auto& items = CItem::GetItemsRecursive(itemsSelected);

    // Show progress dialog and compress files
    const auto alg = CompressionIdToAlg(id);
    CProgressDlg(items.size(), CProgressDlg::Flags::None, AfxGetMainWnd(), [&](CProgressDlg* pdlg)
    {
        for (const auto & item : items)
        {
            if (pdlg->IsCancelled()) break;
            CompressFile(item->GetPathLong(), alg);
            pdlg->Increment();
        }
    }).DoModal();

    // Refresh items after compression
    RefreshItem(itemsSelected);
}

void CWinDirStatModel::OnCleanupOptimizeVhd()
{
    CWaitCursor wc;
    const auto& itemsSelected = GetAllSelected();
    const auto& items = CItem::GetItemsRecursive(itemsSelected, [](const CItem* item) {
        return item->IsTypeOrFlag(IT_FILE) && item->HasExtension(L".vhdx"); });

    // Show progress dialog and optimize VHD files
    CProgressDlg(items.size(), CProgressDlg::Flags::None, AfxGetMainWnd(), [&](CProgressDlg* pdlg)
    {
        for (const auto item : items)
        {
            if (pdlg->IsCancelled()) break;
            OptimizeVhd(item->GetPathLong());
            pdlg->Increment();
        }
    }).DoModal();

    // Refresh items after optimization
    RefreshItem(itemsSelected);
}

void CWinDirStatModel::OnScanSuspend()
{
    // Wait for system to fully shutdown
    for (auto& queue : m_queues | std::views::values)
        ProcessMessagesUntilSignaled([&queue] { queue.SuspendExecution(); });

    // Freeze the shared item clock only after every scan worker is idle.
    CItem::SuspendScanClock();

    // Mark as suspended
    if (CMainFrame::Get() != nullptr)
        CMainFrame::Get()->SuspendState(true);
}

void CWinDirStatModel::OnScanResume()
{
    // Resume the shared clock before allowing any scan worker to continue.
    CItem::ResumeScanClock();

    for (auto& queue : m_queues | std::views::values)
        queue.ResumeExecution();

    if (CMainFrame::Get() != nullptr)
        CMainFrame::Get()->SuspendState(false);
}

void CWinDirStatModel::OnScanStop()
{
    StopScanningEngine(Stop);
}

void CWinDirStatModel::StopScanningEngine(StopReason stopReason)
{
    // Interrupt blocking I/O (e.g. ReadFile on large files) in worker threads
    // so they reach WaitIfSuspended promptly. Without this, SuspendExecution
    // hangs indefinitely waiting for AllThreadsIdling() while a thread reads.
    for (auto& queue : m_queues | std::views::values)
        queue.CancelThreadIo();

    // Request for all threads to stop processing
    for (auto& queue : m_queues | std::views::values)
        ProcessMessagesUntilSignaled([&queue] { queue.SuspendExecution(); });

    // Stop m_queues from executing
    for (auto& queue : m_queues | std::views::values)
        ProcessMessagesUntilSignaled([&queue, &stopReason] { queue.CancelExecution(stopReason); });

    // Wait for wrapper thread to complete
    if (m_thread.joinable())
    {
        CWaitCursor waitCursor;
        ProcessMessagesUntilSignaled([this] { m_thread.join(); });
        m_thread = {};
        m_queues.clear();
    }

    // Resume the shared clock if a scan is stopped or replaced while suspended.
    CItem::ResumeScanClock();
}

void CWinDirStatModel::OnContextMenuExplore(UINT nID)
{
    // get list of paths from items
    const auto selected = GetAllSelected();
    std::vector<std::wstring> paths;
    paths.reserve(selected.size());
    for (const auto& item : selected)
        paths.emplace_back(item->GetPath());

    // query current context menu
    if (paths.empty()) return;

    // Keep OLE alive on this thread so shell clipboard verbs can use delayed rendering.
    if (thread_local SmartPointer oleInit([](PVOID) noexcept { OleUninitialize(); }, PVOID{});
        oleInit == nullptr && SUCCEEDED(OleInitialize(nullptr))) oleInit = reinterpret_cast<PVOID>(TRUE);

        const CComPtr contextMenu = GetContextMenu(CMainFrame::Get()->GetSafeHwnd(), paths);
    if (contextMenu == nullptr) return;

    // create placeholder menu
    CMenu menu;
    if (menu.CreatePopupMenu() == 0) return;
    if (FAILED(contextMenu->QueryContextMenu(menu.GetSafeHmenu(), 0,
        CONTENT_MENU_MINCMD, CONTENT_MENU_MAXCMD, CMF_NORMAL))) return;

    // launch command associated with passed item identifier
    CMINVOKECOMMANDINFOEX info = {};
    info.cbSize = sizeof(CMINVOKECOMMANDINFOEX);
    info.fMask = CMIC_MASK_UNICODE;
    info.hwnd = CMainFrame::Get()->GetSafeHwnd();
    info.lpVerb = MAKEINTRESOURCEA(nID - 1);
    info.lpVerbW = MAKEINTRESOURCEW(nID - 1);
    info.nShow = SW_SHOWNORMAL;
    contextMenu->InvokeCommand(reinterpret_cast<LPCMINVOKECOMMANDINFO>(&info));
}

void CWinDirStatModel::StartScanningEngine(std::vector<CItem*> items)
{
    // Stop any previous executions
    CWaitCursor wc;
    StopScanningEngine();

    // Address conflicts with currently zoomed/selected items
    const auto zoomItem = GetZoomItem();
    for (const auto item : items)
    {
        // Abort if bad entry detected
        if (item == nullptr)
        {
            return;
        }

        // Bring the zoom out if it would be invalidated
        if (item->IsAncestorOf(zoomItem))
        {
            SetZoomItem(item);
        }
    }

    // Clear any reselection options since they may be invalidated
    ClearReselectChildStack();

    // Do not attempt to update visualizations while scanning
    CMainFrame::Get()->GetVisualizationPane()->SuspendRecalculationDrawing(true);

    // If scanning drive(s) just rescan the child nodes
    if (items.size() == 1 && items.front()->IsTypeOrFlag(IT_MYCOMPUTER))
    {
        items.front()->ResetScanStartTime();
        items = items.front()->GetChildren();
    }

    // Prune descendants: if both an ancestor and a descendant are in the list,
    // remove any descendant since it will be rescanned as part of the ancestor scan
    std::erase_if(items, [&](const CItem* item) {
        return std::ranges::any_of(items, [item](const CItem* other) {
            return other != item && other->IsAncestorOf(item);
        });
    });

    // Remove items in UI thread so we do not conflict with the timer updates
    const auto selectedItems = GetAllSelected();
    using VisualInfo = struct { bool wasExpanded; bool isSelected; };
    std::unordered_map<CItem*, VisualInfo> visualInfo;
    for (auto item : std::vector(items))
    {
        // Clear items from duplicates and top list;
        CFileDupeControl::Get()->RemoveItem(item);
        CFileTopControl::Get()->RemoveItem(item);
        CFileSearchControl::Get()->RemoveItem(item);

        // Record current visual arrangement to reapply afterward
        if (item->IsVisible())
        {
            visualInfo[item].isSelected = std::ranges::find(selectedItems, item) != selectedItems.end();
            visualInfo[item].wasExpanded = item->IsExpanded();
        }

        // Skip pruning if it is a new element
        if (!item->IsDone()) continue;

        // Remove item from tree
        item->ExtensionDataProcessChildren(true);
        item->UpwardRecalcLastChange();
        item->UpwardSubtractSizePhysical(item->GetSizePhysicalRaw());
        item->UpwardSubtractSizeLogical(item->GetSizeLogical());
        item->UpwardSubtractFiles(item->GetFilesCount());
        item->UpwardSubtractFolders(item->GetFoldersCount());
        item->RemoveAllChildren();
        item->UpwardSetUndone();

        // Child removal will collapse the item, so re-expand it
        if (const auto iter = visualInfo.find(item);
            iter != visualInfo.end() && item->IsVisible())
            item->SetExpanded(iter->second.wasExpanded);

        // Handle if item to be refreshed has been removed or filtered
        if (CFiltering::IsFilteredOut(item) ||
            item->IsTypeOrFlag(IT_FILE, IT_DIRECTORY, IT_DRIVE) &&
            !FinderBasic::DoesFileExist(item->GetFolderPath(),
                item->IsTypeOrFlag(IT_FILE) ? item->GetName() : std::wstring()))
        {
            // Remove item from list so we do not rescan it
            std::erase(items, item);

            if (item->IsRootItem())
            {
                Get()->UnlinkRoot();
                // No worker is launched to release this scan's suspension.
                CMainFrame::Get()->GetVisualizationPane()->SuspendRecalculationDrawing(false);
                return;
            }

            // Handle non-root item by removing from parent
            item->GetParent()->UpwardSubtractFiles(item->IsTypeOrFlag(IT_FILE) ? 1 : 0);
            item->GetParent()->UpwardSubtractFolders(item->IsTypeOrFlag(IT_FILE) ? 0 : 1);
            item->GetParent()->RemoveChild(item);
        }
    }
    CWinDirStatModel::InvalidateSelectionCache();

    // Refresh filter cutoffs immediately before scanning in case settings
    // were compiled long ago (e.g. dialog left open before clicking scan).
    CFiltering::CompileFilters();

    // Start a thread so we do not hang the message loop during inserts.
    // Lambda captures assume the model exists for the duration of the scan.
    m_thread = std::jthread([this,items, visualInfo] () mutable
    {
        // Add items to processing queue
        for (const auto & item : items)
        {
            // Skip any items we should not follow
            if (!item->IsTypeOrFlag(ITF_ROOTITEM) && !CDirStatApp::Get()->IsFollowingAllowed(item->GetReparseTag()))
            {
                continue;
            }

            item->UpwardAddReadJobs(1);
            item->UpwardSetUndone();

            // Create status progress bar
            CMainFrame::Get()->InvokeInMessageThread([]
            {
                CMainFrame::Get()->UpdateProgress();
            });

            // Separate into separate m_queues per volume
            m_queues[item->GetVolumeRoot()->GetPath()].Push(item);
        }

        // Create subordinate threads if there is work to do
        std::unordered_map<std::wstring, FinderNtfsContext> queueContextNtfs;
        std::unordered_map<std::wstring, FinderBasicContext> queueContextBasic;
        for (auto& queue : m_queues)
        {
            queueContextNtfs.try_emplace(queue.first);
            queueContextBasic.try_emplace(queue.first);

            auto* queuePtr = &queue.second;
            auto* ntfsCtx = &queueContextNtfs[queue.first];
            auto* basicCtx = &queueContextBasic[queue.first];
            queue.second.StartThreads(COptions::ScanningThreads, [queuePtr, ntfsCtx, basicCtx]()
            {
                CItem::ScanItems(queuePtr, *ntfsCtx, *basicCtx);
            });
        }

        // Ensure toolbar buttons reflect scanning status
        CMainFrame::Get()->InvokeInMessageThread([]
        {
            CDirStatApp::Get()->OnIdle(0);
            CMainFrame::Get()->Invalidate();
        });

        // Wait for all threads to run out of work
        StopReason stopReason = Default;
        for (auto& queue : m_queues | std::views::values)
            stopReason = static_cast<StopReason>(queue.WaitForCompletion());

        // If new scan or closing, complete scan UI cleanup before the old
        // tree is torn down.
        if (stopReason == Abort)
        {
            CMainFrame::Get()->InvokeInMessageThread([]
            {
                CMainFrame::Get()->SetProgressComplete();
                // Preserve the current layout. A replacement scan expands All Files
                // before aborting this worker and must remain expanded.
                CMainFrame::Get()->GetVisualizationPane()->SuspendRecalculationDrawing(false);
            });
            return;
        }

        // Restore unknown and freespace items
        for (const auto& item : items)
        {
            if (!item->IsTypeOrFlag(IT_DRIVE)) continue;

            if (COptions::ShowFreeSpace)
            {
                item->CreateFreeSpaceItem();
            }
            if (COptions::ShowUnknown)
            {
                item->CreateUnknownItem();
            }
        }

        // Handle hardlink counting for the drive
        auto drives = GetRootItem()->GetDriveItems();
        if (COptions::ProcessHardlinks) std::for_each(std::execution::par, drives.begin(), drives.end(), [](auto* drive)
        {
            // Create hardlink item if it doesn't exist
            if (drive->FindHardlinksItem() == nullptr)
            {
                drive->CreateHardlinksItem();
            }

            drive->DoHardlinkAdjustment();
        });
        else std::for_each(std::execution::par, drives.begin(), drives.end(), [](auto* drive)
        {
            // Remove hardlink item if processing is disabled
            if (drive->FindHardlinksItem() != nullptr)
            {
                drive->RemoveHardlinksItem();
            }
        });
        // After hardlink adjustment GetProgressPos() holds the corrected scan-based size.
        // Resync the progress range to this value so position and range share exactly the
        // same calculation basis — they will converge to 100%, not drop to 99%.
        // Using GetProgressRange() (GetFreeDiskSpace) here would still be a mismatch.
        {
            const ULONGLONG correctedRange = GetRootItem()->GetProgressPos();
            CMainFrame::Get()->InvokeInMessageThread([correctedRange]
            {
                CMainFrame::Get()->UpdateProgressRange(correctedRange);
            });
        }

        // Sorting and other finalization tasks
        CItem::ScanItemsFinalize(GetRootItem());
        Get()->RebuildExtensionData();

        // Handle quiet save mode if path is set
        if (const auto savePath = CDirStatApp::Get()->GetSaveToPath(); !savePath.empty())
        {
            // Get the model and root item
            const auto* model = CWinDirStatModel::Get();
            if (!model->HasRootItem()) ExitProcess(1);

            // Run scan and exit with success == 0 or failure == 1
            ExitProcess(SaveResults(savePath, model->GetRootItem()) ? 0 : 1);
        }

        // Handle quiet save duplicates mode if path is set
        if (const auto dupeSavePath = CDirStatApp::Get()->GetSaveDupesToPath(); !dupeSavePath.empty())
        {
            // Get the duplicate root item
            CFileDupeControl::Get()->SortItems();
            const auto* dupeRoot = CFileDupeControl::Get()->GetRootItem();
            if (dupeRoot == nullptr) ExitProcess(1);

            // Run scan and exit with success == 0 or failure == 1
            ExitProcess(SaveDuplicates(dupeSavePath, dupeRoot) ? 0 : 1);
        }

        // Handle quiet save permissions mode if path is set
        if (const auto permsSavePath = CDirStatApp::Get()->GetSavePermsToPath(); !permsSavePath.empty())
        {
            // Scan the tree headlessly and exit with success == 0 or failure == 1
            if (!HasRootItem()) ExitProcess(1);
            const auto rows = CFilePermsControl::ScanTree(GetRootItem());
            const std::vector<const CItemPerm*> ptrs(rows.begin(), rows.end());
            ExitProcess(SavePermissions(permsSavePath, ptrs) ? 0 : 1);
        }

        // Invoke a UI thread to do updates
        CMainFrame::Get()->InvokeInMessageThread([&]
        {
            CMainFrame::Get()->LockWindowUpdate();
            Get()->NotifyPanes();
            CMainFrame::Get()->SetProgressComplete();
            CMainFrame::Get()->ApplyPaneVisibility(true);
            CMainFrame::Get()->GetVisualizationPane()->SuspendRecalculationDrawing(false);
            CMainFrame::Get()->UnlockWindowUpdate();

            // Restore pre-scan visual orientation
            for (const auto& item : visualInfo | std::views::keys)
            {
                if (GetFocusControl()->FindTreeItem(item) == -1 || !item->IsVisible()) continue;

                // Restore selection if previously set
                if (visualInfo[item].isSelected) GetFocusControl()->SelectItem(item, false, true);
            }
        });

        // Force heap cleanup after scan
        (void) _heapmin();
    });
}

void CWinDirStatModel::OnRemoveMarkOfTheWebTags()
{
    if (!ConfirmOperation(IDS_MENU_REMOVE_MOTW, COptions::ShowRemoveMotwPrompt)) return;

    CWaitCursor wc;
    const auto& itemsSelected = GetAllSelected();
    const auto& items = CItem::GetItemsRecursive(itemsSelected);

    CProgressDlg(items.size(), CProgressDlg::Flags::None, AfxGetMainWnd(), [&](CProgressDlg* pdlg)
    {
        for (const auto item : items)
        {
            if (pdlg->IsCancelled()) break;
            DeleteFile((item->GetPathLong() + L":Zone.Identifier").c_str());
            pdlg->Increment();
        }
    }).DoModal();
}

void CWinDirStatModel::OnUpdateCreateHardlink(CCmdUI* pCmdUI)
{
    // Only allow when focused on duplicate list after scanning has settled
    if (!IsScanSettled() || !DupeListHasFocus())
    {
        return pCmdUI->Enable(FALSE);
    }

    // Get the selected tree list items directly
    const auto selected = GetAllSelected();
    if (selected.size() < 2)
    {
        return pCmdUI->Enable(FALSE);
    }

    // Validate all items are on same logical volume
    const auto drive = selected.front()->GetParentDrive();
    for (auto* item : selected)
    {
        if (!item->IsTypeOrFlag(IT_FILE) ||
            item->GetParentDrive() != drive)
        {
            return pCmdUI->Enable(FALSE);
        }
    }

    pCmdUI->Enable(TRUE);
}

void CWinDirStatModel::OnCreateHardlink()
{
    const auto selected = GetAllSelected();
    if (selected.size() < 2 || !ConfirmOperation(IDS_MENU_CREATE_HARDLINK, COptions::ShowCreateHardlinkPrompt,
        std::span<CItem* const>(selected).subspan(1))) return;
    for (const auto* item : selected)
    {
        if (item == selected.front()) continue;

        CreateHardlinkFromFile(selected.front()->GetPathLong(), item->GetPathLong());
    }

    // Refresh the target item to reflect the change
    RefreshItem(selected);
}

void CWinDirStatModel::OnToolsSetDates()
{
    if (!ConfirmOperation(IDS_MENU_SET_DATES, COptions::ShowSetDatesPrompt)) return;

    CWaitCursor wc;
    std::vector<CItem*> directories;
    auto stack = GetAllSelected();
    if (stack.empty()) stack = { GetRootItem() };
    while (!stack.empty())
    {
        CItem* item = stack.back();
        stack.pop_back();
        if (item->IsTypeOrFlag(IT_DIRECTORY))
        {
            directories.push_back(item);
        }
        if (item->HasChildren())
        {
            stack.insert(stack.end(), item->GetChildren().begin(), item->GetChildren().end());
        }
    }

    CProgressDlg(directories.size(), CProgressDlg::Flags::None, AfxGetMainWnd(), [&](CProgressDlg* pdlg)
    {
        for (CItem* item : directories)
        {
            if (pdlg->IsCancelled()) break;

            const FILETIME lastChange = item->GetLastChange();
            if (std::bit_cast<std::uint64_t>(lastChange) != 0)
            {
                std::error_code ec;
                std::filesystem::last_write_time(
                    item->GetPathLong(),
                    std::filesystem::file_time_type{std::chrono::file_clock::duration(std::bit_cast<std::int64_t>(lastChange))},
                    ec
                );
            }
            pdlg->Increment();
        }
    }).DoModal();
}

void CWinDirStatModel::OnCleanupRemoveEmpty()
{
    const auto& roots = GetAllSelected();
    if (roots.empty()) return;

    // Collect every directory whose entire subtree contains no files (GetFilesCount() == 0).
    // Such a directory is wholly empty, so all of its descendants qualify as well. Each item is
    // recorded before its children are pushed, so ancestors precede descendants; reversing then
    // yields a bottom-up order suitable for RemoveDirectory, which only removes empty folders and
    // so is guaranteed to find each parent empty once its children have been processed.
    std::vector<CItem*> emptyDirs;
    std::vector<CItem*> stack(roots.begin(), roots.end());
    std::unordered_set<CItem*> visited;
    for (CWaitCursor wc; !stack.empty();)
    {
        CItem* item = stack.back();
        stack.pop_back();
        if (!visited.insert(item).second) continue;
        if (item->IsTypeOrFlag(IT_DIRECTORY) && !item->IsRootItem() && item->GetFilesCount() == 0)
        {
            emptyDirs.push_back(item);
        }
        if (item->HasChildren())
        {
            stack.insert(stack.end(), item->GetChildren().begin(), item->GetChildren().end());
        }
    }

    if (emptyDirs.empty()) return;
    if (!ConfirmOperation(IDS_MENU_REMOVE_EMPTY, COptions::ShowRemoveEmptyFoldersPrompt, emptyDirs)) return;

    size_t deletedCount = 0;
    std::unordered_set<const CItem*> deletedDirs;
    std::unordered_set<CItem*> parentsToRefresh;
    std::reverse(emptyDirs.begin(), emptyDirs.end());
    CProgressDlg(emptyDirs.size(), CProgressDlg::Flags::None, AfxGetMainWnd(), [&](CProgressDlg* pdlg)
    {
        for (CItem* item : emptyDirs)
        {
            if (pdlg->IsCancelled()) break;

            if (RemoveDirectory(item->GetPathLong().c_str()))
            {
                deletedCount++;
                deletedDirs.insert(item);
                if (CItem* parent = item->GetParent())
                {
                    parentsToRefresh.insert(parent);
                }
                pdlg->Increment();
            }
        }
    }).DoModal();

    // Refresh parents of deleted items that were not themselves deleted
    std::erase_if(parentsToRefresh, [&](const CItem* parent) {
        return deletedDirs.contains(parent);
    });

    if (!parentsToRefresh.empty())
    {
        RefreshItem(std::vector<CItem*>(parentsToRefresh.begin(), parentsToRefresh.end()));
    }
    else if (deletedCount > 0)
    {
        RefreshItem(roots);
    }
}
