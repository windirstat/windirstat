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
#include "FilePermsControl.h"
#include "FinderBasic.h"
#include "FinderMtp.h"
#include "FinderNtfs.h"
#include "SearchDlg.h"
#include "ProgressDlg.h"
#include "Filtering.h"

static std::optional<std::wstring> ChooseReportPath(const CDialog::FilePickerMode mode)
{
    return CDialog::PickFile(mode,
        std::format(L"{} (*.csv;*.json)|*.csv;*.json|{} (*.*)|*.*||",
        Localization::Lookup(IDS_FILE_FILTER), Localization::Lookup(IDS_ALL_FILES)));
}
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
    static bool (*isZoomed)(CItem*) = [](CItem*) { return Get()->IsZoomed(); };
    static bool (*canZoomIn)(CItem*) = [](CItem* i) { return i != nullptr && (i = i->IsLeaf() ? i->GetParent() : i) != nullptr && i != model->GetZoomItem() && i->TmiGetSize() > 0; };
    static bool (*canZoomOut)(CItem*) = [](CItem*) { return model->GetZoomItem() != model->GetRootItem(); };
    static bool (*parentNotNull)(CItem*) = [](CItem* i) { return i != nullptr && i->GetParent() != nullptr; };
    static bool (*reselectAvail)(CItem*) = [](CItem*) { return model->IsReselectChildAvailable(); };

    // Define item capability checks for shell and filesystem-specific commands.
    static bool (*isShellChild)(CItem*) = [](CItem* item)
    {
        return item != nullptr && !item->IsRootItem() && !item->IsMtpRoot() && item->HasShellIdentity();
    };
    static bool (*hasShellIdentity)(CItem*) = [](CItem* item)
    {
        return item != nullptr && item->HasShellIdentity();
    };
    static bool (*filesystemOnly)(CItem*) = [](CItem* item)
    {
        if (item == nullptr) item = model->GetRootItem();
        return item != nullptr && item->SupportsFilesystemApis();
    };
    static bool (*isRefreshable)(CItem*) = [](CItem* item)
    {
        return item != nullptr && (!item->IsTypeOrFlag(ITF_MTP) ||
            (item->HasShellIdentity() && !item->IsTypeOrFlag(IT_FILE)));
    };
    static bool (*hasRecycleBin)(CItem*) = [](CItem* i)
    {
        return i != nullptr && i->SupportsFilesystemApis() && !i->IsRootItem() && IsLocalDrive(i->GetPath());
    };
    static bool (*isResumable)(CItem*) = [](CItem*) { return CMainFrame::Get()->IsScanSuspended(); };
    static bool (*isSuspendable)(CItem*) = [](CItem*) { return model->HasRootItem() && !model->IsRootDone() && !CMainFrame::Get()->IsScanSuspended(); };
    static bool (*isStoppable)(CItem*) = [](CItem*) { return model->HasRootItem() && !model->IsRootDone(); };
    static bool (*isHibernate)(CItem*) = [](CItem*) { return IsElevationActive() && IsHibernateEnabled(); };
    static bool (*isElevated)(CItem*) = [](CItem*) { return IsElevationActive(); };
    static bool (*isElevationPossible)(CItem*) = [](CItem*) { return IsElevationPossible(); };
    static bool (*isDupeTabVisible)(CItem*) = [](CItem*) { return CMainFrame::Get()->GetFileTabbedView()->IsDupeTabVisible(); };
    static bool (*isPermsTabVisible)(CItem*) = [](CItem*) { return CMainFrame::Get()->GetFileTabbedView()->IsPermsTabVisible(); };
    static bool (*isDriveOrDirOrFile)(CItem*) = [](CItem* i) { return i != nullptr && i->IsTypeOrFlag(IT_DRIVE, IT_DIRECTORY, IT_FILE); };
    static bool (*isVhdFile)(CItem*) = [](CItem* i)
    {
        return i != nullptr && i->SupportsFilesystemApis() && IsElevationActive() &&
            (!i->IsTypeOrFlag(IT_FILE) || i->HasExtension(L".vhdx"));
    };
    static bool (*isStorageSenseAvailable)(CItem*) = [](CItem*) { return IsStorageSenseAvailable(); };
    static constexpr ITEMTYPE shellTypes = IT_MYCOMPUTER | IT_DRIVE | IT_DIRECTORY | IT_FILE;

    // Map each command to its selection rules and shell or filesystem capability requirements.
    static std::unordered_map<UINT, const commandFilter> filters
    {
        // ID                           none   many   early  focus        types
        { ID_CLEANUP_DELETE,          { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE, isShellChild } },
        { ID_CLEANUP_DELETE_BIN,      { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE, hasRecycleBin } },
        { ID_CLEANUP_DISK_CLEANUP,    { true,  true,  false, LF_NONE,     ITF_ANY, isElevationPossible } },
        { ID_CLEANUP_STORAGE_SENSE,   { true,  true,  false, LF_NONE,     ITF_ANY, isStorageSenseAvailable } },
        { ID_CLEANUP_MOVE_TO,         { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE, isShellChild } },
        { ID_CLEANUP_REMOVE_PROGRAMS, { true,  true,  false, LF_NONE,     ITF_ANY } },
        { ID_CLEANUP_DISM_ANALYZE,    { true,  true,  true,  LF_NONE,     ITF_ANY, isElevationPossible } },
        { ID_CLEANUP_DISM_NORMAL,     { true,  true,  false, LF_NONE,     ITF_ANY, isElevationPossible } },
        { ID_CLEANUP_DISM_RESET,      { true,  true,  false, LF_NONE,     ITF_ANY, isElevationPossible } },
        { ID_CLEANUP_EMPTY_BIN,       { true,  true,  false, LF_NONE,     ITF_ANY } },
        { ID_CLEANUP_EMPTY_FOLDER,    { true,  true,  false, LF_NONE,     IT_DIRECTORY, isShellChild } },
        { ID_CLEANUP_REMOVE_EMPTY,    { false, true,  false, LF_FILETREE, IT_DRIVE | IT_DIRECTORY, filesystemOnly } },
        { ID_CLEANUP_EXPLORER_SELECT, { false, true,  true,  LF_NONE,     IT_DIRECTORY | IT_FILE, hasShellIdentity } },
        { ID_CLEANUP_HIBERNATE,       { true,  true,  false, LF_NONE,     ITF_ANY, isHibernate } },
        { ID_CLEANUP_OPEN_IN_CONSOLE, { false, true,  true,  LF_NONE,     IT_DRIVE | IT_DIRECTORY | IT_FILE, filesystemOnly } },
        { ID_CLEANUP_OPEN_IN_PWSH,    { false, true,  true,  LF_NONE,     IT_DRIVE | IT_DIRECTORY | IT_FILE, filesystemOnly } },
        { ID_CLEANUP_OPEN_SELECTED,   { false, true,  true,  LF_NONE,     shellTypes, hasShellIdentity } },
        { ID_CLEANUP_PROPERTIES,      { false, true,  true,  LF_NONE,     shellTypes, hasShellIdentity } },
        { ID_CLEANUP_OPTIMIZE_VHD,    { false, true,  false, LF_NONE,     IT_DRIVE | IT_DIRECTORY | IT_FILE, isVhdFile } },
        { ID_CLEANUP_REMOVE_LOCAL,    { true,  true,  false, LF_NONE,     ITF_ANY, isElevated } },
        { ID_CLEANUP_REMOVE_MOTW,     { false, true,  false, LF_NONE,     IT_DRIVE | IT_DIRECTORY | IT_FILE, filesystemOnly } },
        { ID_CLEANUP_SPARSIFY_FILE,   { false, true,  false, LF_NONE,     IT_FILE, filesystemOnly } },
        { ID_CLEANUP_REMOVE_ROAMING,  { true,  true,  false, LF_NONE,     ITF_ANY, isElevated } },
        { ID_CLEANUP_REMOVE_SHADOW,   { true,  true,  false, LF_NONE,     ITF_ANY, isElevated } },
        { ID_COMPRESS_LZNT1,          { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE, filesystemOnly } },
        { ID_COMPRESS_LZX,            { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE, filesystemOnly } },
        { ID_COMPRESS_NONE,           { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE, filesystemOnly } },
        { ID_COMPRESS_XPRESS16K,      { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE, filesystemOnly } },
        { ID_COMPRESS_XPRESS4K,       { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE, filesystemOnly } },
        { ID_COMPRESS_XPRESS8K,       { false, true,  false, LF_NONE,     IT_DIRECTORY | IT_FILE, filesystemOnly } },
        { ID_COMPUTE_HASH,            { false, false, true,  LF_NONE,     IT_FILE, hasShellIdentity } },
        { ID_EDIT_COPY_CLIPBOARD,     { false, true,  true,  LF_NONE,     IT_DRIVE | IT_DIRECTORY | IT_FILE } },
        { ID_FILTER,                  { true,  true,  true,  LF_NONE,     ITF_ANY } },
        { ID_FILTER_EXCLUDE_ITEM,     { false, true,  false, LF_NONE,     ITF_ANY, isDriveOrDirOrFile } },
        { ID_INDICATOR_DISK,          { true,  true,  false, LF_NONE,     ITF_ANY } },
        { ID_INDICATOR_IDLE,          { true,  true,  true,  LF_NONE,     ITF_ANY } },
        { ID_INDICATOR_RAM,           { true,  true,  true,  LF_NONE,     ITF_ANY } },
        { ID_INDICATOR_SIZE,          { true,  true,  false, LF_NONE,     ITF_ANY } },
        { ID_POPUP_CANCEL,            { true,  true,  true,  LF_NONE,     ITF_ANY } },
        { ID_REFRESH_ALL,             { true,  true,  false, LF_NONE,     ITF_ANY } },
        { ID_REFRESH_SELECTED,        { false, true,  false, LF_NONE,     shellTypes, isRefreshable } },
        { ID_SAVE_DUPLICATES,         { true,  true,  false, LF_NONE,     ITF_ANY, isDupeTabVisible } },
        { ID_SAVE_PERMISSIONS,        { true,  true,  false, LF_NONE,     ITF_ANY, isPermsTabVisible } },
        { ID_SAVE_RESULTS,            { true,  true,  false, LF_NONE,     ITF_ANY } },
        { ID_SCAN_RESUME,             { true,  true,  true,  LF_NONE,     ITF_ANY, isResumable } },
        { ID_SCAN_STOP,               { true,  true,  true,  LF_NONE,     ITF_ANY, isStoppable } },
        { ID_SCAN_SUSPEND,            { true,  true,  true,  LF_NONE,     ITF_ANY, isSuspendable } },
        { ID_SEARCH,                  { true,  true,  false, LF_NONE,     ITF_ANY } },
        { ID_TOOLS_SET_DATES,         { true,  true,  false, LF_FILETREE, IT_DRIVE | IT_DIRECTORY, filesystemOnly } },
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
        assert(false);
        return;
    }

    const auto& [allowNone, allowMany, allowEarly, focus, typesAllow, extra] = it->second;
    bool allow = focus == LF_NONE || (CMainFrame::Get()->GetLogicalFocus() & focus) > 0;
    allow &= allowEarly || IsScanSettled();
    if (!allow) { pCmdUI->Enable(false); return; }

    const auto items = !allowNone || extra != nullptr ?
        GetSelectedItemsView() : std::span<CItem* const>{};
    allow &= allowNone || !items.empty();
    allow &= allowMany || items.size() <= 1;
    if (items.empty() && extra != nullptr) allow &= extra(nullptr);
    for (const auto& item : items)
    {
        if (!allow) break;
        allow &= typesAllow == ITF_ANY || (!item->IsTypeOrFlag(ITF_RESERVED) && item->IsTypeOrFlag(typesAllow));
        allow &= extra == nullptr || extra(item);
    }
    pCmdUI->Enable(allow);
}

void CWinDirStatModel::OnUpdateCompressionHandler(CCmdUI* pCmdUI)
{
    // Defer to standard update handler for initial value
    OnUpdateCentralHandler(pCmdUI);
    if (pCmdUI->m_pMenu == nullptr) return;

    // See if each path supports available compression options
    bool allow = pCmdUI->m_pMenu->IsItemEnabled(pCmdUI->m_nID, CMenu::ItemLookup::Command);
    for (const auto& item : GetSelectedItemsView())
    {
        allow &= CompressFileAllowed(item->GetVolumeRoot()->GetPath(),
            CompressionIdToAlg(pCmdUI->m_nID));
    }
    pCmdUI->Enable(allow);
}

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
    CProgressDlg(itemsSelected.size(), CProgressDlg::Flags::None, GetMainWindow(), [&](CProgressDlg* pdlg)
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
    }).ShowModal();

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

void CWinDirStatModel::OnSaveResults() const
{
    // Request the file path from the user
    const auto path = ChooseReportPath(CDialog::FilePickerMode::Save);
    if (!path) return;

    CProgressDlg(0, CProgressDlg::Flags::NoCancel, GetMainWindow(), [&](CProgressDlg*)
    {
        SaveResults(*path, GetRootItem());
    }).ShowModal();
}

void CWinDirStatModel::OnSaveDuplicates()
{
    // Request the file path from the user
    const auto path = ChooseReportPath(CDialog::FilePickerMode::Save);
    if (!path) return;

    CProgressDlg(0, CProgressDlg::Flags::NoCancel, GetMainWindow(), [&](CProgressDlg*)
    {
        SaveDuplicates(*path, CFileDupeControl::Get()->GetRootItem());
    }).ShowModal();
}

void CWinDirStatModel::OnSavePermissions()
{
    // Request the file path from the user
    const auto path = ChooseReportPath(CDialog::FilePickerMode::Save);
    if (!path) return;

    CProgressDlg(0, CProgressDlg::Flags::NoCancel, GetMainWindow(), [&](CProgressDlg*)
    {
        SavePermissions(*path, CFilePermsControl::Get()->GetPermItems());
    }).ShowModal();
}

void CWinDirStatModel::OnLoadResults()
{
    // Request the file path from the user
    const auto path = ChooseReportPath(CDialog::FilePickerMode::Open);
    if (!path) return;

    CItem* newroot = nullptr;
    CProgressDlg(0, CProgressDlg::Flags::NoCancel, GetMainWindow(), [&](CProgressDlg*)
    {
        newroot = LoadResults(*path);
    }).ShowModal();

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

    if (!CMainFrame::Get()->CopyTextToClipboard(paths)) DisplayError(TranslateError());
}

void CWinDirStatModel::OnCleanupEmptyRecycleBin() const
{
    if (!ConfirmOperation(IDS_MENU_EMPTY_BIN, COptions::ShowEmptyRecycleBinPrompt)) return;

    CProgressDlg(0, CProgressDlg::Flags::NoCancel, GetMainWindow(), [](CProgressDlg*)
    {
        SHEmptyRecycleBin(GetMainWindowHandle(), nullptr,
            SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
    }).ShowModal();

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

void CWinDirStatModel::OnRemoveShadowCopies() const
{
    ULONGLONG count = 0, bytesUsed = 0;
    QueryShadowCopies(count, bytesUsed);
    if (count == 0 || !ConfirmOperation(IDS_MENU_REMOVE_SHADOW, COptions::ShowRemoveShadowCopiesPrompt)) return;

    CProgressDlg(static_cast<size_t>(count), CProgressDlg::Flags::None, GetMainWindow(), [](CProgressDlg* pdlg)
    {
        RemoveWmiInstances(L"Win32_ShadowCopy", pdlg);
    }).ShowModal();

    GetRootItem()->UpdateFreeSpaceItem();
}

void CWinDirStatModel::OnUpdateViewShowFreeSpace(CCmdUI* pCmdUI)
{
    OnUpdateCentralHandler(pCmdUI);
    pCmdUI->SetCheck(COptions::ShowFreeSpace);
}

void CWinDirStatModel::OnViewShowFreeSpace()
{
    for (CItem* root : GetRootItem()->GetSpaceItems())
    {
        if (COptions::ShowFreeSpace)
        {
            const CItem* free = root->FindFreeSpaceItem();
            assert(free != nullptr);

            if (GetZoomItem() == free)
            {
                m_zoomItem = free->GetParent();
            }

            root->RemoveFreeSpaceItem();
        }
        else
        {
            root->CreateFreeSpaceItem();
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
    for (CItem* root : GetRootItem()->GetSpaceItems())
    {
        if (COptions::ShowUnknown)
        {
            const CItem* unknown = root->FindUnknownItem();
            assert(unknown != nullptr);

            if (GetZoomItem() == unknown)
            {
                m_zoomItem = unknown->GetParent();
            }

            root->RemoveUnknownItem();
        }
        else
        {
            root->CreateUnknownItem();
        }
    }

    // Toggle value
    COptions::ShowUnknown = !COptions::ShowUnknown;

    // Force recalculation and graph refresh
    StartScanningEngine({});
}

void CWinDirStatModel::OnTreeMapZoomIn()
{
    auto* item = CFileTreeControl::Get()->GetFirstSelectedItem<CItem>();
    if (item != nullptr) item = item->GetLinkedItem();
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
    const CItem* zoomItem = GetZoomItem();
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
    // Group child PIDLs by shell parent so each containing folder opens once.
    using PidlHolder = SmartPointer<PIDLIST_ABSOLUTE, decltype(&CoTaskMemFree)>;
    struct SelectionGroup
    {
        PCIDLIST_ABSOLUTE parent;
        std::vector<PCUITEMID_CHILD> children;
    };

    std::vector<PidlHolder> pidls;
    std::vector<SelectionGroup> groups;

    // Resolve each selected item and retain its parent and child PIDLs through shell invocation.
    for (const auto* item : GetAllSelected())
    {
        PidlHolder absolute(CoTaskMemFree, CreateShellPidl(item));
        PidlHolder parent(CoTaskMemFree, absolute != nullptr ? ILCloneFull(absolute) : nullptr);
        if (parent == nullptr || !ILRemoveLastID(parent)) continue;

        auto group = std::ranges::find_if(groups, [&](const SelectionGroup& candidate)
        {
            return ILIsEqual(candidate.parent, parent);
        });
        if (group == groups.end())
        {
            groups.push_back({ parent, {} });
            pidls.emplace_back(CoTaskMemFree, parent.Detach());
            group = std::prev(groups.end());
        }

        group->children.push_back(ILFindLastID(absolute));
        pidls.emplace_back(CoTaskMemFree, absolute.Detach());
    }

    // Open every resolved parent folder with all of its selected children highlighted.
    for (auto& group : groups)
    {
        (void) SHOpenFolderAndSelectItems(group.parent, static_cast<UINT>(group.children.size()),
            group.children.data(), 0);
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
        ShellExecuteWrapper(cmd, params, L"open", GetMainWindowHandle(), path);
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
        ShellExecuteWrapper(pwsh, L"", L"open", GetMainWindowHandle(), path);
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
    const auto destination = CDialog::PickFolder();
    if (!destination) return;
    const std::wstring& destFolder = *destination;

    // Verify destination exists
    if (!FolderExists(destFolder)) return;

    // Show progress dialog and move files
    CProgressDlg(0, CProgressDlg::Flags::None, GetMainWindow(), [&](const CProgressDlg* pdlg)
    {
        // Initialize an STA for the shell file operation on this worker thread.
        const ComApartmentScope com;
        if (!com) return;

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

        const CComPtr<IShellItemArray> psia = CreateShellItemArray(items, true);
        if (psia == nullptr || FAILED(fileOperation->MoveItems(psia, destShellItem))) return;

        // Do all moves
        const HRESULT res = fileOperation->PerformOperations();
        if (res != S_OK) VTRACE(L"File Operation Failed: {}", TranslateError(res));
    }).ShowModal();

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
    search.ShowModal();
}

void CWinDirStatModel::OnDisableHibernateFile()
{
    if (!ConfirmOperation(IDS_MENU_DISABLE_HIBERNATE, COptions::ShowDisableHibernatePrompt)) return;

    DisableHibernate();

    // See if there is a hibernate file on any drive to refresh
    std::vector<CItem*> refresh;
    for (const auto& drive : GetRootItem()->GetDriveItems())
    {
        for (const auto& child : drive->GetChildren())
        {
            if (_wcsicmp(child->GetNameView().data(), L"hiberfil.sys") == 0)
            {
                refresh.push_back(child);
            }
        }
    }
    if (!refresh.empty()) StartScanningEngine(std::move(refresh));
}

void CWinDirStatModel::OnRemoveRoamingProfiles() const
{
    RemoveLocalProfiles(L"RoamingConfigured = TRUE");
}

void CWinDirStatModel::OnRemoveLocalProfiles()
{
    RemoveLocalProfiles(L"RoamingConfigured = FALSE AND Loaded = FALSE AND Special = FALSE");
}

void CWinDirStatModel::RemoveLocalProfiles(const std::wstring_view whereClause) const
{
    const auto paths = QueryWmiStringProperty(L"Win32_UserProfile", L"LocalPath", whereClause.data());
    if (paths.empty()) return;

    const auto result = CMessageBoxDlg::Show(Localization::Lookup(IDS_DELETE_WARNING), paths,
        {}, false, MB_YESNO | MB_ICONWARNING, GetMainWindow(), { 600, 400 },
        Localization::Lookup(IDS_DELETE_TITLE));
    if (result.nID != IDYES) return;

    CProgressDlg(paths.size(), CProgressDlg::Flags::None, GetMainWindow(), [&](CProgressDlg* pdlg)
    {
        RemoveWmiInstances(L"Win32_UserProfile", pdlg, whereClause.data());
    }).ShowModal();

    GetRootItem()->UpdateFreeSpaceItem();
    CComHeapPtr<wchar_t> profilePath;
    if (SHGetKnownFolderPath(FOLDERID_UserProfiles, 0, nullptr, &profilePath) == S_OK && profilePath)
    {
        if (CItem* profileItem = GetRootItem()->FindItemByPath(static_cast<wchar_t*>(profilePath));
            profileItem != nullptr)
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
    const UINT i = pCmdUI->m_nID - ID_USERDEFINEDCLEANUP0;
    if (!IsScanSettled() || i >= COptions::UserDefinedCleanups.size()) return pCmdUI->Enable(false);

    const auto& items = GetSelectedItemsView();
    auto& udc = COptions::UserDefinedCleanups[i];
    const bool allowControl = (FileTreeHasFocus() || DupeListHasFocus() || TopListHasFocus()) &&
        udc.Enabled && !items.empty() && std::ranges::all_of(items,
            [&](const auto& item) { return UserDefinedCleanupWorksForItem(&udc, item); });

    pCmdUI->Enable(allowControl);
}

void CWinDirStatModel::OnUserDefinedCleanup(const UINT id)
{
    RunUserDefinedCleanup(id - ID_USERDEFINEDCLEANUP0);
}

void CWinDirStatModel::RunUserDefinedCleanup(const size_t index)
{
    if (!IsScanSettled() || index >= COptions::UserDefinedCleanups.size() ||
        !COptions::UserDefinedCleanups[index].Enabled) return;

    USERDEFINEDCLEANUP* udc = &COptions::UserDefinedCleanups[index];
    const auto & items = GetAllSelected();
    const bool worksForAll = std::ranges::all_of(items,
        [&](const auto& item) { return UserDefinedCleanupWorksForItem(udc, item); });
    assert(worksForAll);

    std::wstring detail = udc->Title.Obj();
    if (udc->RecurseIntoSubdirectories)
        detail = std::format(L"{}; {}", detail, GetLocalizedMenuText(IDS_PAGE_CLEANUPS_RECURSE));
    if (items.empty() || !worksForAll ||
        !ConfirmOperation(IDS_USER_DEFINED_CLEANUP, udc->AskForConfirmation, items, detail)) return;

    std::vector<CItem*> refreshQueue;
    for (const auto & item : items)
    {
        try
        {
            PerformUserDefinedCleanup(udc, item);
            RefreshAfterUserDefinedCleanup(udc, item, refreshQueue);
        }
        catch (...)
        {
            // error would have been displayed already
        }
    }

    // process refresh queue
    if (!refreshQueue.empty()) RefreshItem(refreshQueue);
}

void CWinDirStatModel::OnTreeMapSelectParent()
{
    auto* item = CFileTreeControl::Get()->GetFirstSelectedItem<CItem>();
    if (item != nullptr) item = item->GetLinkedItem();
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

    // Open every property sheet individually when the selection contains virtual MTP items.
    if (std::ranges::any_of(selected, [](const CItem* item) { return item->IsTypeOrFlag(ITF_MTP); }))
    {
        for (const auto& item : selected) OpenItem(item, L"properties");
        return;
    }

    // Show one shared property sheet for selections that support shell aggregation.
    const CComPtr<IShellItemArray> psia = CreateShellItemArray(selected, true);
    CComPtr<IDataObject> pDataObj;
    if (psia && SUCCEEDED(psia->BindToHandler(nullptr, BHID_DataObject, IID_PPV_ARGS(&pDataObj))) &&
        SUCCEEDED(SHMultiFileProperties(pDataObj, 0))) return;

    // Fall back to opening a property sheet for each item when aggregation fails.
    for (const auto& item : selected) OpenItem(item, L"properties");
}

void CWinDirStatModel::OnComputeHash()
{
    // Compute the hashes in a worker thread
    std::wstring hashResult;
    CItem* const item = GetAllSelected().front();
    const ULONGLONG logicalSize = item->GetSizeLogical();
    const size_t totalBlocks = static_cast<size_t>(logicalSize / wds::Mi + (logicalSize % wds::Mi != 0));
    CProgressDlg(totalBlocks, CProgressDlg::Flags::PercentageOnly, GetMainWindow(), [&](CProgressDlg* pdlg)
    {
        hashResult = ComputeFileHashes(item, pdlg);
    }).ShowModal();

    if (!hashResult.empty())
    {
        // Display result in message box
        CMessageBoxDlg dlg(hashResult, wds::strWinDirStat, MB_OK | MB_ICONINFORMATION);
        dlg.SetWidthAuto();
        dlg.ShowModal();
    }
}

CompressionAlgorithm CWinDirStatModel::CompressionIdToAlg(const UINT id)
{
    switch (id)
    {
        case ID_COMPRESS_NONE: return NONE;
        case ID_COMPRESS_LZNT1: return LZNT1;
        case ID_COMPRESS_XPRESS4K: return XPRESS4K;
        case ID_COMPRESS_XPRESS8K: return XPRESS8K;
        case ID_COMPRESS_XPRESS16K: return XPRESS16K;
        case ID_COMPRESS_LZX: return LZX;
        default: return NONE;
    }
}

void CWinDirStatModel::OnCleanupCompress(const UINT id)
{
    CWaitCursor wc;
    const auto& itemsSelected = GetAllSelected();
    const auto& items = CItem::GetItemsRecursive(itemsSelected);

    // Show progress dialog and compress files
    const auto alg = CompressionIdToAlg(id);
    CProgressDlg(items.size(), CProgressDlg::Flags::None, GetMainWindow(), [&](CProgressDlg* pdlg)
    {
        for (const auto & item : items)
        {
            if (pdlg->IsCancelled()) break;
            CompressFile(item->GetPathLong(), alg);
            pdlg->Increment();
        }
    }).ShowModal();

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
    CProgressDlg(items.size(), CProgressDlg::Flags::None, GetMainWindow(), [&](CProgressDlg* pdlg)
    {
        for (const auto item : items)
        {
            if (pdlg->IsCancelled()) break;
            OptimizeVhd(item->GetPathLong());
            pdlg->Increment();
        }
    }).ShowModal();

    // Refresh items after optimization
    RefreshItem(itemsSelected);
}

void CWinDirStatModel::OnScanSuspend()
{
    // Wait for system to fully shutdown
    for (auto& queue : m_queues | std::views::values)
        CWinApp::RunTaskWithUiUpdates([&queue] { queue.SuspendExecution(); });

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
        CWinApp::RunTaskWithUiUpdates([&queue] { queue.SuspendExecution(); });

    // Stop m_queues from executing
    for (auto& queue : m_queues | std::views::values)
        CWinApp::RunTaskWithUiUpdates([&queue, &stopReason] { queue.CancelExecution(stopReason); });

    // Wait for wrapper thread to complete
    if (m_thread.joinable())
    {
        CWaitCursor waitCursor;
        CWinApp::RunTaskWithUiUpdates([this] { m_thread.join(); });
        m_thread = {};
        m_queues.clear();
    }

    // Resume the shared clock if a scan is stopped or replaced while suspended.
    CItem::ResumeScanClock();
}

void CWinDirStatModel::OnContextMenuExplore(const UINT nID)
{
    const auto selected = GetAllSelected();
    if (selected.empty()) return;

    // Keep OLE alive on this thread so shell clipboard verbs can use delayed rendering.
    if (thread_local SmartPointer oleInit([](PVOID) noexcept { OleUninitialize(); }, PVOID{});
        oleInit == nullptr && SUCCEEDED(OleInitialize(nullptr))) oleInit = reinterpret_cast<PVOID>(1);

    // Query the shell context menu for the selected filesystem or MTP items.
    const CComPtr contextMenu = GetContextMenu(selected);
    if (contextMenu == nullptr) return;

    // create placeholder menu
    const CMenu menu = CMenu::CreatePopup();
    if (!menu) return;
    if (FAILED(contextMenu->QueryContextMenu(menu.Handle(), 0,
        CONTENT_MENU_MINCMD, CONTENT_MENU_MAXCMD, CMF_NORMAL))) return;

    // launch command associated with passed item identifier
    CMINVOKECOMMANDINFOEX info = {};
    info.cbSize = sizeof(CMINVOKECOMMANDINFOEX);
    info.fMask = CMIC_MASK_UNICODE;
    info.hwnd = CMainFrame::Get()->Handle();
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

    // Resolve hardlink references before their derived snapshot can be discarded.
    for (auto*& item : items)
        if (item != nullptr && item->IsTypeOrFlag(IT_HLINKS_FILE)) item = item->GetLinkedItem();
    std::erase_if(items, [](const CItem* item)
    {
        return item != nullptr && item->IsTypeOrFlag(IT_HLINKS, IT_HLINKS_SET, IT_HLINKS_IDX);
    });
    std::unordered_set<CItem*> uniqueItems;
    std::erase_if(items, [&](CItem* item) { return !uniqueItems.insert(item).second; });

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

    // Prune descendants: if both an ancestor and a descendant are in the list,
    // remove any descendant since it will be rescanned as part of the ancestor scan
    std::erase_if(items, [&](const CItem* item) {
        return std::ranges::any_of(items, [item](const CItem* other) {
            return other != item && other->IsAncestorOf(item);
        });
    });

    // If scanning drive(s) just rescan the child nodes
    if (items.size() == 1 && items.front()->IsTypeOrFlag(IT_MYCOMPUTER))
    {
        items.front()->ResetScanStartTime();
        items = items.front()->GetChildren();
    }

    const auto selectedItems = GetAllSelected();
    std::unordered_set<CItem*> doneItems;
    for (auto* item : items)
        if (item->IsDone()) doneItems.insert(item);

    // Hardlink results are a derived snapshot and cannot outlive mutations to their target drive.
    std::unordered_set<CItem*> affectedDrives;
    for (auto* item : items)
        if (CItem* drive = item->GetParentDrive(); drive != nullptr) affectedDrives.insert(drive);
    for (auto* drive : affectedDrives)
    {
        if (const CItem* hardlinks = drive->FindHardlinksItem();
            hardlinks != nullptr && hardlinks->IsAncestorOf(GetZoomItem())) SetZoomItem(drive);
        drive->RemoveHardlinksItem();
    }

    // Remove items in UI thread so we do not conflict with the timer updates
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
        if (!doneItems.contains(item)) continue;

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
        bool exists = true;
        if (item->IsTypeOrFlag(IT_FILE, IT_DIRECTORY, IT_DRIVE))
        {
            // Resolve existence through the matching filesystem or MTP backend.
            exists = item->IsTypeOrFlag(ITF_MTP) ? FinderMtp::DoesFileExist(item) :
                FinderBasic::DoesFileExist(item->GetFolderPath(),
                    item->IsTypeOrFlag(IT_FILE) ? item->GetName() : std::wstring());
        }
        if (CFiltering::IsFilteredOut(item) || !exists)
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
            CItem* const parent = item->GetParent();
            if (GetZoomItem() == item) SetZoomItem(parent);
            // Direct multi-root branches are not included in the synthetic root's folder count.
            if (!parent->IsTypeOrFlag(IT_MYCOMPUTER))
            {
                parent->UpwardSubtractFiles(item->IsTypeOrFlag(IT_FILE) ? 1 : 0);
                parent->UpwardSubtractFolders(item->IsTypeOrFlag(IT_FILE) ? 0 : 1);
            }
            parent->RemoveChild(item);
        }
    }
    InvalidateSelectionCache();

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
            queueContextBasic.try_emplace(queue.first, queue.first);

            auto* queuePtr = &queue.second;
            auto* ntfsCtx = &queueContextNtfs[queue.first];
            auto* basicCtx = &queueContextBasic[queue.first];

            // Use one worker per MTP volume while retaining configured parallelism for filesystems.
            const unsigned int threads = FinderMtp::IsPath(queue.first) ? 1 : COptions::ScanningThreads;
            queue.second.StartThreads(threads, [queuePtr, ntfsCtx, basicCtx]
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
            if (!item->SupportsSpaceItems()) continue;

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
            // Existing snapshots belong to drives untouched by this scan.
            if (drive->FindHardlinksItem() != nullptr) return;
            drive->CreateHardlinksItem();
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
            const auto* model = Get();
            if (!model->HasRootItem()) ExitProcess(1);

            // Run scan and exit with success == 0 or failure == 1
            ExitProcess(SaveResults(savePath, model->GetRootItem()) ? 0 : 1);
        }

        // Handle quiet save duplicates mode if path is set
        if (const auto dupeSavePath = CDirStatApp::Get()->GetSaveDupesToPath(); !dupeSavePath.empty())
        {
            // Get the duplicate root item
            CMainFrame::Get()->InvokeInMessageThread([]
            {
                CFileDupeControl::Get()->SortItems();
            });
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

        // Defer heap cleanup until the timer observes that this thread has exited.
        m_heapMinPending.store(true, std::memory_order_relaxed);
    });
}

void CWinDirStatModel::OnRemoveMarkOfTheWebTags()
{
    if (!ConfirmOperation(IDS_MENU_REMOVE_MOTW, COptions::ShowRemoveMotwPrompt)) return;

    CWaitCursor wc;
    const auto& itemsSelected = GetAllSelected();
    const auto& items = CItem::GetItemsRecursive(itemsSelected);

    CProgressDlg(items.size(), CProgressDlg::Flags::None, GetMainWindow(), [&](CProgressDlg* pdlg)
    {
        for (const auto item : items)
        {
            if (pdlg->IsCancelled()) break;
            DeleteFile((item->GetPathLong() + L":Zone.Identifier").c_str());
            pdlg->Increment();
        }
    }).ShowModal();
}

void CWinDirStatModel::OnUpdateCreateHardlink(CCmdUI* pCmdUI)
{
    // Only allow when focused on duplicate list after scanning has settled
    if (!IsScanSettled() || !DupeListHasFocus())
    {
        return pCmdUI->Enable(false);
    }

    // Get the selected tree list items directly
    const auto selected = GetSelectedItemsView();
    if (selected.size() < 2)
    {
        return pCmdUI->Enable(false);
    }

    // Validate all items are on same logical volume
    const auto drive = selected.front()->GetParentDrive();
    for (const auto* item : selected)
    {
        // Exclude virtual items because hard links require filesystem files.
        if (!item->SupportsFilesystemApis() || !item->IsTypeOrFlag(IT_FILE) ||
            item->GetParentDrive() != drive)
        {
            return pCmdUI->Enable(false);
        }
    }

    pCmdUI->Enable(true);
}

void CWinDirStatModel::OnCreateHardlink()
{
    const auto selected = GetAllSelected();
    if (selected.size() < 2 || !ConfirmOperation(IDS_MENU_CREATE_HARDLINK, COptions::ShowCreateHardlinkPrompt,
        std::span(selected).subspan(1))) return;
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

    // Collect filesystem directories while excluding virtual shell items.
    CWaitCursor wc;
    std::vector<CItem*> directories;
    auto stack = GetAllSelected();
    if (stack.empty()) stack = { GetRootItem() };
    while (!stack.empty())
    {
        CItem* item = stack.back();
        stack.pop_back();
        if (!item->SupportsFilesystemApis()) continue;
        if (item->IsTypeOrFlag(IT_DIRECTORY))
        {
            directories.push_back(item);
        }
        if (item->HasChildren())
        {
            stack.insert(stack.end(), item->GetChildren().begin(), item->GetChildren().end());
        }
    }

    CProgressDlg(directories.size(), CProgressDlg::Flags::None, GetMainWindow(), [&](CProgressDlg* pdlg)
    {
        for (const CItem* item : directories)
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
    }).ShowModal();
}

void CWinDirStatModel::OnCleanupRemoveEmpty()
{
    const auto& roots = GetAllSelected();
    if (roots.empty()) return;

    const auto isUnsafeDirectory = [](const CItem* item) noexcept
    {
        if (!item->IsTypeOrFlag(IT_DIRECTORY)) return false;

        const DWORD attributes = item->GetAttributes();
        return attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
    };

    // Collect every filesystem directory whose entire subtree contains no files (GetFilesCount() == 0).
    // Do not enter reparse-point branches: excluded links have no modeled files, while followed
    // links expose directories outside the selected physical tree. Each item is recorded before
    // its children are pushed, so reversing the result yields the required bottom-up order.
    std::vector<CItem*> emptyDirs;
    std::vector<CItem*> stack;
    for (CItem* root : roots)
    {
        if (!root->SupportsFilesystemApis()) continue;
        const CItem* ancestor = root;
        while (ancestor != nullptr && !isUnsafeDirectory(ancestor)) ancestor = ancestor->GetParent();
        if (ancestor == nullptr) stack.push_back(root);
    }
    std::unordered_set<CItem*> visited;
    for (CWaitCursor wc; !stack.empty();)
    {
        CItem* item = stack.back();
        stack.pop_back();
        if (!visited.insert(item).second || isUnsafeDirectory(item)) continue;
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
    std::ranges::reverse(emptyDirs);
    CProgressDlg(emptyDirs.size(), CProgressDlg::Flags::None, GetMainWindow(), [&](CProgressDlg* pdlg)
    {
        for (const CItem* item : emptyDirs)
        {
            if (pdlg->IsCancelled()) break;

            const std::wstring path = item->GetPathLong();
            const DWORD attributes = GetFileAttributes(path.c_str());
            const DWORD typeAttributes = attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT);
            if (attributes == INVALID_FILE_ATTRIBUTES || typeAttributes != FILE_ATTRIBUTE_DIRECTORY) continue;

            if (RemoveDirectory(path.c_str()))
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
    }).ShowModal();

    // Refresh parents of deleted items that were not themselves deleted
    std::erase_if(parentsToRefresh, [&](const CItem* parent) {
        return deletedDirs.contains(parent);
    });

    if (!parentsToRefresh.empty())
    {
        RefreshItem(std::vector(parentsToRefresh.begin(), parentsToRefresh.end()));
    }
    else if (deletedCount > 0)
    {
        RefreshItem(roots);
    }
}
