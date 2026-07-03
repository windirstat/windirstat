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
#include "TreeMapView.h"
#include "FileTopControl.h"
#include "FileSearchControl.h"
#include "FileWatcherControl.h"
#include "FilePermsControl.h"
#include "FinderBasic.h"
#include "FinderNtfs.h"
#include "SearchDlg.h"
#include "ProgressDlg.h"
#include "Filtering.h"

CWinDirStatModel::CWinDirStatModel()
{
    ASSERT(nullptr == s_singleton);
    s_singleton = this;

    VTRACE(L"sizeof(CItem) = {}", sizeof(CItem));
    VTRACE(L"sizeof(CTreeListItem) = {}", sizeof(CTreeListItem));
    VTRACE(L"sizeof(CWdsListItem) = {}", sizeof(CWdsListItem));
}

CWinDirStatModel::~CWinDirStatModel()
{
    delete m_rootItem;
    s_singleton = nullptr;
}

CWinDirStatModel* CWinDirStatModel::s_singleton = nullptr;

void CWinDirStatModel::ClearScanState()
{
    CWaitCursor wc;

    // Wait for system to fully shutdown
    StopScanningEngine(Abort);

    // Stop watchers
    if (CFileWatcherControl::Get() != nullptr) CFileWatcherControl::Get()->StopMonitoring();

    // Stop permissions scanner before the tree is torn down
    if (CFilePermsControl::Get() != nullptr) CFilePermsControl::Get()->StopScan();

    // Discard pending top-list entries that still point into the old tree.
    if (CFileTopControl::Get() != nullptr) CFileTopControl::Get()->ClearPendingItems();

    // Clean out icon queue
    GetIconHandler()->ClearAsyncShellInfoQueue();

    // Reset extension data
    GetExtensionData()->clear();
    m_registeredExtensions.clear();

    // Cleanup visual artifacts - controllers manage their own root items
    if (CFileTopControl::Get() != nullptr) CFileTopControl::Get()->DeleteAllItems();
    if (CFileTreeControl::Get() != nullptr) CFileTreeControl::Get()->DeleteAllItems();
    if (CFileDupeControl::Get() != nullptr) CFileDupeControl::Get()->DeleteAllItems();
    if (CFileSearchControl::Get() != nullptr) CFileSearchControl::Get()->DeleteAllItems();
    if (CFileWatcherControl::Get() != nullptr) CFileWatcherControl::Get()->DeleteAllItems();
    if (CFilePermsControl::Get() != nullptr) CFilePermsControl::Get()->DeleteAllItems();

    // Cleanup structures
    delete m_rootItem;
    m_rootItem = nullptr;
    m_zoomItem = nullptr;
}

BOOL CWinDirStatModel::ResetScan()
{
    ClearScanState();
    SetScanPathSpec(wds::strEmpty);
    NotifyPanes(MODEL_CHANGE_NEW_ROOT);
    return TRUE;
}

BOOL CWinDirStatModel::StartScan(const std::wstring& pathSpec)
{
    // Expand All Files view to full window during scan
    CMainFrame::Get()->ExpandFileTabbedView();

    // Decode list of folders to scan
    std::vector<std::wstring> selections = SplitString(pathSpec);

    // Prepare for new root and delete any existing data
    ClearScanState();

    // Persist the full scan spec, which may contain pipe-separated roots.
    Get()->SetScanPathSpec(pathSpec);

    // Count number of drives for validation
    const std::wregex driveMatch(LR"(^[A-Za-z]:[\\]?$)", std::regex_constants::optimize);
    const auto driveCount = static_cast<size_t>(std::ranges::count_if(selections, [&](const std::wstring& str) {
        return std::regex_match(str, driveMatch);
    }));

    // Return if no paths were passed
    if (selections.empty()) return true;

    // Return if multiple selections but they are not all drives
    if (selections.size() >= 2 && selections.size() != driveCount) return true;

    // Determine if we should add multiple drives under a single node
    if (selections.size() >= 2)
    {
        // Fetch the localized string for the root computer object
        SmartPointer pidl(CoTaskMemFree, static_cast<LPITEMIDLIST>(nullptr));
        SmartPointer ppszName(CoTaskMemFree, static_cast<LPWSTR>(nullptr));
        CComPtr<IShellItem> psi;
        if (FAILED(SHGetKnownFolderIDList(FOLDERID_ComputerFolder, 0, nullptr, &pidl)) ||
            SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&psi)) != S_OK ||
            FAILED(psi->GetDisplayName(SIGDN_NORMALDISPLAY, &ppszName)))
        {
            ASSERT(FALSE);
        }

        const std::wstring name = ppszName != nullptr ? *ppszName : Localization::Lookup(IDS_THISPC);
        m_rootItem = new CItem(IT_MYCOMPUTER | ITF_ROOTITEM, name);
        for (const auto& rootFolder : selections)
        {
            const auto drive = new CItem(IT_DRIVE, rootFolder);
            m_rootItem->AddChild(drive);
        }
    }
    else
    {
        const ITEMTYPE type = driveCount == 1 ? IT_DRIVE : IT_DIRECTORY;
        m_rootItem = new CItem(type | ITF_ROOTITEM, selections.front());
        m_rootItem->UpdateStatsFromDisk();
    }

    // Restore zoom scope to be the root
    m_zoomItem = m_rootItem;

    // Update new root for display
    NotifyPanes(MODEL_CHANGE_NEW_ROOT);
    StartScanningEngine(std::vector({ Get()->GetRootItem() }));
    return true;
}

BOOL CWinDirStatModel::OpenLoadedScan(CItem* loadedRoot)
{
    CMainFrame::Get()->ExpandFileTabbedView();

    ClearScanState();

    // Determine root spec string using GetNameView to avoid temporary allocations
    std::wstring spec(loadedRoot->GetNameView());
    if (loadedRoot->IsTypeOrFlag(IT_MYCOMPUTER))
    {
        std::vector<std::wstring> folders;
        std::ranges::transform(loadedRoot->GetChildren(), std::back_inserter(folders),
            [](const CItem* obj) -> std::wstring { return GetDrive(obj->GetNameView()); });
        spec = JoinString(folders);
    }
    else if (loadedRoot->IsTypeOrFlag(IT_DRIVE))
    {
        spec = GetDrive(loadedRoot->GetNameView());
    }

    Get()->SetScanPathSpec(spec);

    m_rootItem = loadedRoot;
    m_zoomItem = m_rootItem;

    // Populate the Largest Files list from the pre-built tree
    for (CItem* item : CItem::GetItemsRecursive({ m_rootItem }))
    {
        CFileTopControl::Get()->ProcessTop(item);
    }

    NotifyPanes(MODEL_CHANGE_NEW_ROOT);
    StartScanningEngine({});
    return true;
}

// The scan spec can be a pipe-separated list of folders, not a filesystem path.
//
void CWinDirStatModel::SetScanPathSpec(const std::wstring& pathSpec)
{
    // MRU would be fine but is not implemented yet.
    m_scanPathSpec = pathSpec;
    SetScanTitle(m_scanPathSpec);
}

// Prefix the window title (with percentage or "Scanning")
//
void CWinDirStatModel::SetScanTitlePrefix(const std::wstring& prefix) const
{
    static std::wstring suffix = IsElevationActive() ? std::format(L" ({})", Localization::Lookup(IDS_ADMIN)) : L"";
    std::wstring scanName = std::format(L"{} {} {}", prefix, GetScanTitle(), suffix);
    scanName = TrimString(scanName);
    CMainFrame::Get()->UpdateFrameTitleForScan(scanName.empty() ? nullptr : scanName.c_str());
}

COLORREF CWinDirStatModel::GetCushionColor(const std::wstring & ext)
{
    const auto& record = GetExtensionData()->find(ext);
    ASSERT(record != GetExtensionData()->end());
    return record->second.color;
}

COLORREF CWinDirStatModel::GetZoomColor() const
{
    return RGB(0, 0, 255);
}

CExtensionData* CWinDirStatModel::GetExtensionData()
{
    return &m_extensionData;
}

SExtensionRecord* CWinDirStatModel::GetExtensionDataRecord(const std::wstring& ext)
{
    std::scoped_lock guard(m_extensionMutex);
    return &m_extensionData[ext];
}

// Snapshots every ".ext" key directly under HKEY_CLASSES_ROOT. Rebuilt with the extension
// data so registration tests are lock-free set lookups
void CWinDirStatModel::RebuildRegisteredExtensions()
{
    if (!m_registeredExtensions.empty()) return;

    CRegKey classes;
    if (classes.Open(HKEY_CLASSES_ROOT, nullptr, KEY_READ) != ERROR_SUCCESS) return;

    wchar_t name[UCHAR_MAX + 1];
    for (DWORD i = 0, length = std::size(name); classes.EnumKey(i, name, &length) == ERROR_SUCCESS; ++i, length = std::size(name))
    {
        if (name[0] != L'.') continue;
        _wcslwr_s(name, std::size(name));
        m_registeredExtensions.emplace(name);
    }
}

// True if the extension was present in the HKEY_CLASSES_ROOT snapshot (registered on this PC)
bool CWinDirStatModel::IsExtensionRegistered(const std::wstring& ext) const
{
    return m_registeredExtensions.contains(ext);
}

ULONGLONG CWinDirStatModel::GetRootSize() const
{
    ASSERT(m_rootItem != nullptr);
    ASSERT(IsRootDone());
    return m_rootItem->GetSizePhysical();
}

// Starts a refresh of all mount points in our tree.
// Called when the user changes the follow mount points option.
//
void CWinDirStatModel::RefreshReparsePointItems()
{
    CWaitCursor wc;

    if (CItem* root = GetRootItem(); nullptr != root)
    {
        RecurseRefreshReparsePoints(root);
    }
}

bool CWinDirStatModel::HasRootItem() const
{
    return m_rootItem != nullptr;
}

bool CWinDirStatModel::IsRootDone() const
{
    return HasRootItem() && m_rootItem->IsDone();
}

bool CWinDirStatModel::IsScanRunning() const
{
    if (!m_thread.has_value()) return false;

    DWORD exitCode;
    GetExitCodeThread(const_cast<std::jthread&>(*m_thread).native_handle(), &exitCode);
    return (exitCode == STILL_ACTIVE);
}

bool CWinDirStatModel::IsScanSettled() const
{
    return IsRootDone() && !IsScanRunning();
}

CItem* CWinDirStatModel::GetRootItem() const
{
    return m_rootItem;
}

CItem* CWinDirStatModel::GetZoomItem() const
{
    return m_zoomItem;
}

bool CWinDirStatModel::IsZoomed() const
{
    return GetZoomItem() != GetRootItem();
}

void CWinDirStatModel::SetHighlightExtension(const std::wstring & ext, const bool unregistered)
{
    m_highlightExtension = ext;
    m_highlightUnregistered = unregistered;

    // Precompute the unregistered extensions once so the treemap highlight can test each
    // leaf with a single lock-free lookup instead of a per-leaf registry query
    m_highlightExtensions.clear();
    if (unregistered)
    {
        for (const auto& [key, rec] : m_extensionData)
        {
            if (!key.empty() && !IsExtensionRegistered(key)) m_highlightExtensions.insert(key);
        }
    }

    CMainFrame::Get()->UpdatePaneText();
}

std::wstring CWinDirStatModel::GetHighlightExtension() const
{
    return m_highlightExtension;
}

bool CWinDirStatModel::IsHighlightUnregistered() const
{
    return m_highlightUnregistered;
}

const std::unordered_set<std::wstring>& CWinDirStatModel::GetHighlightExtensions() const
{
    return m_highlightExtensions;
}

// The root item has been deleted.
//
void CWinDirStatModel::UnlinkRoot()
{
    CMainFrame::Get()->InvokeInMessageThread([this]
    {
        CMainFrame::Get()->ExpandFileTabbedView();
        ClearScanState();
        NotifyPanes(MODEL_CHANGE_NEW_ROOT);
        CMainFrame::Get()->SetProgressComplete();
    });
}

// Determines whether a UDC works for a given item.
//
bool CWinDirStatModel::UserDefinedCleanupWorksForItem(USERDEFINEDCLEANUP* udc, const CItem* item) const
{
    return item != nullptr && (
        (item->IsTypeOrFlag(IT_DRIVE) && udc->WorksForDrives) ||
        (item->IsTypeOrFlag(IT_DIRECTORY) && udc->WorksForDirectories) ||
        (item->IsTypeOrFlag(IT_FILE) && udc->WorksForFiles) ||
        (item->HasUncPath() && udc->WorksForUncPaths));
}

void CWinDirStatModel::OpenItem(const CItem* item, const std::wstring & verb)
{
    ASSERT(item != nullptr);

    // Ignore if special reserved item
    if (item->IsTypeOrFlag(ITF_RESERVED)) return;

    // Determine path to feed into shell function
    SmartPointer pidl(CoTaskMemFree, static_cast<LPITEMIDLIST>(nullptr));
    if (item->IsTypeOrFlag(IT_MYCOMPUTER))
    {
        SHGetKnownFolderIDList(FOLDERID_ComputerFolder, 0, nullptr, &pidl);
    }
    else
    {
        pidl = ILCreateFromPath(item->GetPath().c_str());
    }

    // Ignore unresolvable (e.g., deleted) files
    if (pidl == nullptr)
    {
        ASSERT(FALSE);
        return;
    }

    // Launch properties dialog
    SHELLEXECUTEINFO sei{};
    sei.cbSize = sizeof(sei);
    sei.hwnd = *AfxGetMainWnd();
    sei.lpVerb = verb.empty() ? nullptr : verb.c_str();
    sei.fMask = SEE_MASK_INVOKEIDLIST | SEE_MASK_IDLIST | SEE_MASK_NOZONECHECKS;
    sei.lpIDList = pidl;
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteEx(&sei);
}

void CWinDirStatModel::RecurseRefreshReparsePoints(CItem* items) const
{
    std::vector<CItem*> toRefresh;

    if (items == nullptr) return;
    std::vector reparseStack({items});
    while (!reparseStack.empty())
    {
        const auto qitem = reparseStack.back();
        reparseStack.pop_back();

        if (!qitem->IsTypeOrFlag(IT_DIRECTORY, IT_DRIVE)) continue;
        for (const auto& child : qitem->GetChildren())
        {
            if (!(child->IsTypeOrFlag(IT_DIRECTORY, IT_DRIVE) || child->IsTypeOrFlag(ITF_ROOTITEM)))
            {
                continue;
            }

            if (CDirStatApp::Get()->IsFollowingAllowed(child->GetReparseTag()))
            {
                toRefresh.push_back(child);
            }
            else
            {
                reparseStack.push_back(child);
            }
        }
    }

    if (!toRefresh.empty())
    {
        RefreshItem(toRefresh);
    }
}

void CWinDirStatModel::RebuildExtensionData()
{
    // Snapshot the registered extensions up front so all registration tests below (and on
    // the UI thread afterwards) are lock-free lookups against an immutable set
    RebuildRegisteredExtensions();

    // Do initial extension information population if necessary
    if (m_extensionData.empty())
    {
        GetRootItem()->ExtensionDataProcessChildren();
    }

    // Each registered extension is colored on its own; when grouping is enabled the
    // unregistered extensions are gathered so the whole group can share one color
    const bool group = COptions::GroupUnregisteredTypes;
    std::vector<CExtensionData::iterator> individual;
    std::vector<CExtensionData::iterator> unregistered;
    individual.reserve(m_extensionData.size());
    for (auto it = m_extensionData.begin(); it != m_extensionData.end(); ++it)
    {
        const bool grouped = group && !it->first.empty() && !IsExtensionRegistered(it->first);
        (grouped ? unregistered : individual).emplace_back(it);
    }

    // Rank color units by descending bytes; the unregistered group is a single unit
    // (marked with index 'groupUnit') sized by the sum of its members
    constexpr size_t groupUnit = SIZE_MAX;
    ULONGLONG unregisteredBytes = 0;
    for (const auto& it : unregistered) unregisteredBytes += it->second.GetBytes();

    std::vector<std::pair<ULONGLONG, size_t>> units;
    units.reserve(individual.size() + 1);
    for (size_t i = 0; i < individual.size(); ++i)
    {
        units.emplace_back(individual[i]->second.GetBytes(), i);
    }
    if (!unregistered.empty()) units.emplace_back(unregisteredBytes, groupUnit);

    // Sort the color units based on total bytes in descending order
    std::ranges::sort(units, [](const auto& a, const auto& b) { return a.first > b.first; });

    // Initialize colors if not already done
    static std::vector<COLORREF> colors;
    if (colors.empty())
    {
        CTreeMap::GetDefaultPalette(colors);
    }

    // Assign palette colors by rank: distinct primary colors first, then the shared fallback
    for (size_t rank = 0; rank < units.size(); ++rank)
    {
        const COLORREF color = colors[std::min(rank, colors.size() - 1)];
        if (units[rank].second == groupUnit)
        {
            for (const auto& it : unregistered) it->second.color = color;
        }
        else
        {
            individual[units[rank].second]->second.color = color;
        }
    }
}

void CWinDirStatModel::DeletePhysicalItems(const std::vector<CItem*>& items, const bool toTrashBin, const bool emptyOnly, const bool skipWarning) const
{
    if (COptions::ShowDeleteWarning && !skipWarning)
    {
        // Build list of file paths for the message box
        std::vector<std::wstring> filePaths;
        filePaths.reserve(items.size());
        for (const auto& item : items) filePaths.emplace_back(item->GetPath());

        // Display the file deletion warning dialog with custom width and height
        if (![&]() -> bool {
            const auto result = CMessageBoxDlg::Show(Localization::Lookup(emptyOnly ? IDS_EMPTY_FOLDER_WARNING : IDS_DELETE_WARNING), filePaths,
                Localization::Lookup(IDS_DONT_SHOW_AGAIN), false, MB_YESNO | MB_ICONWARNING, AfxGetMainWnd(), { 600, 400 }, Localization::Lookup(IDS_DELETE_TITLE));

            if (result.nID != IDYES) return false;

            // Save off the deletion warning preference
            COptions::ShowDeleteWarning = !result.isChecked;
            return true;
        }()) return;
    }

    // Clear active selections
    if (!emptyOnly)
    {
        GetFocusControl()->DeselectAll();
        Get()->InvalidateSelectionCache();
    }

    // Build list of items to delete
    std::vector itemsToDelete{ items };
    if (emptyOnly)
    {
        auto childrenView = items | std::views::transform(&CItem::GetChildren) | std::views::join;
        itemsToDelete.assign(childrenView.begin(), childrenView.end());
    }

    // Calculate total item count for progress tracking
    size_t totalItems = 0;
    for (const auto& item : itemsToDelete)
    {
        totalItems += static_cast<size_t>(1 + item->GetItemsCount());
    }

    bool cancelled = false;
    if (!toTrashBin && !COptions::ShowMicrosoftProgress) CProgressDlg(totalItems, CProgressDlg::Flags::None, AfxGetMainWnd(), [&](CProgressDlg* pdlg)
        {
            // Collect items depth-first and separate into files and directories
            std::vector<const CItem*> files;
            std::vector<const CItem*> directories;
            for (std::vector<CItem*> stack = itemsToDelete; !stack.empty();)
            {
                const CItem* item = stack.back(); stack.pop_back();

                // Sort into files or directories
                (item->IsTypeOrFlag(IT_DIRECTORY) ? directories : files).push_back(item);

                // Add children to stack
                if (item->HasChildren() && !item->IsTypeOrFlag(ITRP_MASK))
                {
                    const auto& children = item->GetChildren();
                    stack.insert(stack.end(), children.begin(), children.end());
                }
            }

            // Delete files in parallel
            std::for_each(std::execution::par, files.begin(), files.end(), [&](const CItem* item)
                {
                    if (pdlg->IsCancelled() || cancelled)
                    {
                        cancelled = true;
                        return;
                    }

                    DeleteFileForce(item->GetPathLong(), item->GetAttributes());
                    pdlg->Increment();
                });

            // Check if cancelled during parallel deletion
            if (cancelled) return;

            // Delete directories in reverse order (children before parents)
            for (const auto& item : directories | std::views::reverse)
            {
                if (pdlg->IsCancelled())
                {
                    cancelled = true;
                    return;
                }

                RemoveDirectory(item->GetPathLong().c_str());
                pdlg->Increment();
            }

            // Check if top-level items still exist
            std::vector<CItem*> remainingItems;
            for (const auto& item : itemsToDelete)
            {
                if (GetFileAttributes(item->GetPathLong().c_str()) != INVALID_FILE_ATTRIBUTES)
                {
                    remainingItems.push_back(item);
                }
            }
            itemsToDelete = std::move(remainingItems);
        }).DoModal();

    if (!cancelled && !itemsToDelete.empty())
    {
        DWORD flags = FOFX_SHOWELEVATIONPROMPT | FOF_NOCONFIRMATION;
        if (toTrashBin) flags |= FOFX_ADDUNDORECORD | FOFX_RECYCLEONDELETE;

        const auto doDelete = [&](HWND hwnd, DWORD opFlags)
        {
            CComPtr<IFileOperation> fileOperation;
            if (FAILED(::CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fileOperation))) ||
                FAILED(fileOperation->SetOwnerWindow(hwnd)) ||
                FAILED(fileOperation->SetOperationFlags(opFlags)))
                return;
            if (const CComPtr<IShellItemArray> psia = CreateShellItemArray(itemsToDelete))
                fileOperation->DeleteItems(psia);
            const HRESULT res = fileOperation->PerformOperations();
            if (res != S_OK) VTRACE(L"File Operation Failed: {}", TranslateError(res));
        };

        if (COptions::ShowMicrosoftProgress)
            doDelete(*AfxGetMainWnd(), flags);
        else
            CProgressDlg(0, CProgressDlg::Flags::None, AfxGetMainWnd(), [&](const CProgressDlg* pdlg)
                { doDelete(*pdlg, flags | FOF_NO_UI); }).DoModal();
    }

    // Create a recycler directories to refresh
    std::unordered_set<CItem*> recyclers;
    if (toTrashBin) for (const auto& item : items)
    {
        const auto& recycler = item->FindRecyclerItem();
        if (recycler != nullptr) recyclers.insert(recycler);
    }

    // Refresh the items and recycler directories
    std::vector<CItem*> itemsToRefresh = items;
    itemsToRefresh.insert(itemsToRefresh.end(), recyclers.begin(), recyclers.end());
    RefreshItem(itemsToRefresh);
}

void CWinDirStatModel::SetZoomItem(CItem* item)
{
    m_zoomItem = item;
    CTreeListControl* pControl = GetFocusControl();
    pControl->Invalidate();
    pControl->UpdateWindow();
    NotifyPanes(MODEL_CHANGE_ZOOM);
}

// Starts a refresh of an item.
// If the physical item has been deleted,
// updates selection, zoom and working item accordingly.
//
void CWinDirStatModel::RefreshItem(const std::vector<CItem*>& item) const
{
    Get()->StartScanningEngine(item);
}

// UDC confirmation dialog.
//
void CWinDirStatModel::AskForConfirmation(USERDEFINEDCLEANUP* udc, const CItem* item)
{
    if (!udc->AskForConfirmation)
    {
        return;
    }

    const std::wstring msg = Localization::Format(udc->RecurseIntoSubdirectories ?
        Localization::Lookup(IDS_RUDC_CONFIRMATIONss) : Localization::Lookup(IDS_UDC_CONFIRMATIONss),
        udc->Title.Obj(), item->GetPath());
    if (IDYES != WdsMessageBox(msg, MB_YESNO))
    {
        AfxThrowUserException();
    }
}

void CWinDirStatModel::PerformUserDefinedCleanup(USERDEFINEDCLEANUP* udc, const CItem* item)
{
    CWaitCursor wc;

    const std::wstring path = item->GetPath();

    // Verify that path still exists
    if (item->IsTypeOrFlag(IT_DIRECTORY, IT_DRIVE))
    {
        if (!FolderExists(path) && !DriveExists(path))
        {
            DisplayError(Localization::Format(IDS_PATHs_NOT_EXIST, path));
            throw;
        }
    }
    else
    {
        ASSERT(item->IsTypeOrFlag(IT_FILE));

        if (!::PathFileExists(path.c_str()))
        {
            DisplayError(Localization::Format(IDS_PATHs_NOT_EXIST, path));
            throw;
        }
    }

    if (udc->RecurseIntoSubdirectories)
    {
        ASSERT(item->IsTypeOrFlag(IT_DRIVE, IT_DIRECTORY));

        RecursiveUserDefinedCleanup(udc, path, path);
    }
    else
    {
        CallUserDefinedCleanup(item->IsTypeOrFlag(IT_DIRECTORY, IT_DRIVE), udc->CommandLine.Obj(), path, path, udc->ShowConsoleWindow, udc->WaitForCompletion);
    }
}

void CWinDirStatModel::RefreshAfterUserDefinedCleanup(const USERDEFINEDCLEANUP* udc, CItem* item, std::vector<CItem*> & refreshQueue) const
{
    // Do not refresh if we are not set to wait
    if (!udc->WaitForCompletion.Obj())
    {
        return;
    }

    switch (static_cast<REFRESHPOLICY>(udc->RefreshPolicy.Obj()))
    {
    case RP_NO_REFRESH:
        break;

    case RP_REFRESH_THIS_ENTRY:
        {
            refreshQueue.push_back(item);
        }
        break;

    case RP_REFRESH_THIS_ENTRYS_PARENT:
        {
            refreshQueue.push_back(nullptr == item->GetParent() ? item : item->GetParent());
        }
        break;
    }
}

void CWinDirStatModel::RecursiveUserDefinedCleanup(USERDEFINEDCLEANUP* udc, const std::wstring& rootPath, const std::wstring& currentPath)
{
    // (Depth first.)

    FinderBasic finder;
    for (BOOL b = finder.FindFile(currentPath); b; b = finder.FindNext())
    {
        if (!finder.IsDirectory())
        {
            continue;
        }
        if (!CDirStatApp::Get()->IsFollowingAllowed(finder.GetReparseTag()))
        {
            continue;
        }

        RecursiveUserDefinedCleanup(udc, rootPath, finder.GetFilePath());
    }

    CallUserDefinedCleanup(true, udc->CommandLine.Obj(), rootPath, currentPath, udc->ShowConsoleWindow, true);
}

void CWinDirStatModel::CallUserDefinedCleanup(const bool isDirectory, const std::wstring& format, const std::wstring& rootPath, const std::wstring& currentPath, const bool showConsoleWindow, const bool wait)
{
    const std::wstring userCommandLine = BuildUserDefinedCleanupCommandLine(format, rootPath, currentPath);

    const std::wstring app = GetCOMSPEC();
    std::wstring cmdline = GetBaseNameFromPath(app) + L" /C " + userCommandLine;
    const std::wstring directory = isDirectory ? currentPath : GetFolderNameFromPath(currentPath);

    STARTUPINFO si { sizeof(si) };
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = showConsoleWindow ? SW_SHOWNORMAL : SW_HIDE;

    PROCESS_INFORMATION pi{};
    if (CreateProcess(app.c_str(), cmdline.data(), nullptr, nullptr, false,
        0, nullptr, directory.c_str(), &si, &pi) == 0)
    {
        DisplayError(Localization::Format(IDS_PROCESS_FAILEDss, app, TranslateError()));
        throw;
    }

    CloseHandle(pi.hThread);

    if (wait)
    {
        WaitForHandleWithRepainting(pi.hProcess);
    }

    CloseHandle(pi.hProcess);
}

std::wstring CWinDirStatModel::BuildUserDefinedCleanupCommandLine(const std::wstring & format, const std::wstring & rootPath, const std::wstring & currentPath)
{
    const std::wstring rootName    = GetBaseNameFromPath(rootPath);
    const std::wstring currentName = GetBaseNameFromPath(currentPath);

    std::wstring s = format;

    // Because file names can contain "%", we first replace our placeholders with
    // strings which contain a forbidden character.
    ReplaceString(s, L"%p", L">p");
    ReplaceString(s, L"%n", L">n");
    ReplaceString(s, L"%sp", L">sp");
    ReplaceString(s, L"%sn", L">sn");

    // Now substitute
    ReplaceString(s, L">p", rootPath);
    ReplaceString(s, L">n", rootName);
    ReplaceString(s, L">sp", currentPath);
    ReplaceString(s,L">sn", currentName);

    return s;
}

void CWinDirStatModel::PushReselectChild(CItem* item)
{
    m_reselectChildStack.push_back(item);
}

CItem* CWinDirStatModel::PopReselectChild()
{
    if (m_reselectChildStack.empty()) return nullptr;
    CItem* item = m_reselectChildStack.back();
    m_reselectChildStack.pop_back();
    return item;
}

void CWinDirStatModel::ClearReselectChildStack()
{
    m_reselectChildStack.clear();
}

bool CWinDirStatModel::IsReselectChildAvailable() const
{
    return !m_reselectChildStack.empty();
}

bool CWinDirStatModel::FileTreeHasFocus()
{
    return LF_FILETREE == CMainFrame::Get()->GetLogicalFocus();
}

bool CWinDirStatModel::DupeListHasFocus()
{
    return LF_DUPELIST == CMainFrame::Get()->GetLogicalFocus();
}

bool CWinDirStatModel::TopListHasFocus()
{
    return LF_TOPLIST == CMainFrame::Get()->GetLogicalFocus();
}

bool CWinDirStatModel::SearchListHasFocus()
{
    return LF_SEARCHLIST == CMainFrame::Get()->GetLogicalFocus();
}

bool CWinDirStatModel::WatcherListHasFocus()
{
    return LF_WATCHERLIST == CMainFrame::Get()->GetLogicalFocus();
}

bool CWinDirStatModel::PermsListHasFocus()
{
    return LF_PERMSLIST == CMainFrame::Get()->GetLogicalFocus();
}

CTreeListControl* CWinDirStatModel::GetFocusControl()
{
    if (DupeListHasFocus()) return CFileDupeControl::Get();
    if (TopListHasFocus()) return CFileTopControl::Get();
    if (SearchListHasFocus()) return CFileSearchControl::Get();
    if (WatcherListHasFocus()) return CFileWatcherControl::Get();
    if (PermsListHasFocus()) return CFilePermsControl::Get();
    return CFileTreeControl::Get();
}

void CWinDirStatModel::NotifyPanes(MODEL_CHANGE change, CItem* item)
{
    NotifyPanesExcept(nullptr, change, item);
}

void CWinDirStatModel::NotifyPanesExcept(CWnd* sender, MODEL_CHANGE change, CItem* item)
{
    InvalidateSelectionCache();

    if (CMainFrame::Get() != nullptr)
    {
        CMainFrame::Get()->UpdateAllPanes(sender, change, item);
    }
}

std::vector<CItem*> CWinDirStatModel::GetAllSelected()
{
    // Check if we can use cached results
    const auto currentFocus = CMainFrame::Get()->GetLogicalFocus();
    if (m_selectionCacheValid && m_cachedFocus == currentFocus)
    {
        return m_cachedSelection;
    }

    // Query fresh selection data
    auto selection = GetFocusControl()->GetAllSelected<CItem>();

    // Expand any visual items if the direct item fetch did not work
    if (selection.empty()) for (const auto item : GetFocusControl()->GetAllSelected<CTreeListItem>(true))
    {
        // If item has no direct link but has children, expand to children
        // Skip root container nodes (no parent) - they should not proxy as a full selection
        if (item->GetLinkedItem()) continue;
        if (item->GetParent() == nullptr) continue;

        for (const auto i : std::views::iota(0, item->GetTreeListChildCount()))
        {
            if (const auto linkedItem = item->GetTreeListChild(i)->GetLinkedItem())
            {
                selection.push_back(linkedItem);
            }
        }
    }

    // Update cache
    m_cachedFocus = currentFocus;
    m_cachedSelection = selection;
    m_selectionCacheValid = true;

    return selection;
}

void CWinDirStatModel::InvalidateSelectionCache()
{
    m_selectionCacheValid = false;
}
