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
#include "FinderBasic.h"
#include "FinderNtfs.h"
#include "SearchDlg.h"
#include "ProgressDlg.h"

IMPLEMENT_DYNCREATE(CDirStatDoc, CDocument)

CDirStatDoc::CDirStatDoc() :
        m_showFreeSpace(COptions::ShowFreeSpace)
      , m_showUnknown(COptions::ShowUnknown)
{
    ASSERT(nullptr == s_singleton);
    s_singleton = this;


    VTRACE(L"sizeof(CItem) = {}", sizeof(CItem));
    VTRACE(L"sizeof(CTreeListItem) = {}", sizeof(CTreeListItem));
    VTRACE(L"sizeof(CWdsListItem) = {}", sizeof(CWdsListItem));
}

CDirStatDoc::~CDirStatDoc()
{
    delete m_rootItem;
    s_singleton = nullptr;
}

CDirStatDoc* CDirStatDoc::s_singleton = nullptr;

void CDirStatDoc::DeleteContents()
{
    CWaitCursor wc;

    // Wait for system to fully shutdown
    StopScanningEngine(Abort);

    // Stop watchers
    if (CFileWatcherControl::Get() != nullptr) CFileWatcherControl::Get()->StopMonitoring();

    // Clean out icon queue
    GetIconHandler()->ClearAsyncShellInfoQueue();

    // Reset extension data
    GetExtensionData()->clear();

    // Cleanup visual artifacts - controllers manage their own root items
    if (CFileTopControl::Get() != nullptr) CFileTopControl::Get()->DeleteAllItems();
    if (CFileTreeControl::Get() != nullptr) CFileTreeControl::Get()->DeleteAllItems();
    if (CFileDupeControl::Get() != nullptr) CFileDupeControl::Get()->DeleteAllItems();
    if (CFileSearchControl::Get() != nullptr) CFileSearchControl::Get()->DeleteAllItems();
    if (CFileWatcherControl::Get() != nullptr) CFileWatcherControl::Get()->DeleteAllItems();

    // Cleanup structures
    delete m_rootItem;
    m_rootItem = nullptr;
    m_zoomItem = nullptr;
}

BOOL CDirStatDoc::OnNewDocument()
{
    if (!CDocument::OnNewDocument())
    {
        return FALSE;
    }

    UpdateAllViews(nullptr, HINT_NEWROOT);
    return TRUE;
}

BOOL CDirStatDoc::OnOpenDocument(LPCWSTR lpszPathName)
{
    // Temporarily minimize extra views
    CMainFrame::Get()->MinimizeTreeMapView();
    CMainFrame::Get()->MinimizeExtensionView();

    // Decode list of folders to scan
    const std::wstring spec = lpszPathName;
    std::vector<std::wstring> selections = SplitString(spec);

    // Prepare for new root and delete any existing data
    CDocument::OnNewDocument();

    // Call base class to commit path to internal doc name string
    Get()->SetPathName(spec.c_str(), FALSE);

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
        SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree);
        SmartPointer<LPWSTR> ppszName(CoTaskMemFree);
        CComPtr<IShellItem> psi;
        if (FAILED(SHGetKnownFolderIDList(FOLDERID_ComputerFolder, 0, nullptr, &pidl)) ||
            SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&psi)) != S_OK ||
            FAILED(psi->GetDisplayName(SIGDN_NORMALDISPLAY, &ppszName)))
        {
            ASSERT(FALSE);
        }

        const LPCWSTR name = ppszName != nullptr ? const_cast<LPCWSTR>(*ppszName) : Localization::Lookup(IDS_THISPC).c_str();
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
    UpdateAllViews(nullptr, HINT_NEWROOT);
    StartScanningEngine(std::vector({ Get()->GetRootItem() }));
    return true;
}

BOOL CDirStatDoc::OnOpenDocument(CItem* newroot)
{
    CMainFrame::Get()->MinimizeTreeMapView();
    CMainFrame::Get()->MinimizeExtensionView();

    CDocument::OnNewDocument(); // --> DeleteContents()

    // Determine root spec string using GetNameView to avoid temporary allocations
    std::wstring spec(newroot->GetNameView());
    if (newroot->IsTypeOrFlag(IT_MYCOMPUTER))
    {
        std::vector<std::wstring> folders;
        std::ranges::transform(newroot->GetChildren(), std::back_inserter(folders),
            [](const CItem* obj) -> std::wstring { return GetDrive(obj->GetNameView()); });
        spec = JoinString(folders);
    }
    else if (newroot->IsTypeOrFlag(IT_DRIVE))
    {
        spec = GetDrive(newroot->GetNameView());
    }

    Get()->SetPathName(spec.c_str(), FALSE);

    m_rootItem = newroot;
    m_zoomItem = m_rootItem;

    UpdateAllViews(nullptr, HINT_NEWROOT);
    StartScanningEngine({});
    return true;
}

// We don't want MFC's AfxFullPath() logic, because lpszPathName
// is not a path. So we have overridden this.
//
void CDirStatDoc::SetPathName(LPCWSTR lpszPathName, BOOL /*bAddToMRU*/)
{
    // MRU would be fine but is not implemented yet.
    m_strPathName = lpszPathName;
    m_bEmbedded = FALSE;
    SetTitle(m_strPathName);

    ASSERT_VALID(this);
}

// Prefix the window title (with percentage or "Scanning")
//
void CDirStatDoc::SetTitlePrefix(const std::wstring& prefix) const
{
    static std::wstring suffix = IsElevationActive() ? std::format(L" ({})", Localization::Lookup(IDS_ADMIN)) : L"";
    std::wstring docName = std::format(L"{} {} {}", prefix, GetTitle().GetString(), suffix);
    docName = TrimString(docName);
    CMainFrame::Get()->UpdateFrameTitleForDocument(docName.empty() ? nullptr : docName.c_str());
}

COLORREF CDirStatDoc::GetCushionColor(const std::wstring & ext)
{
    const auto& record = GetExtensionData()->find(ext);
    ASSERT(record != GetExtensionData()->end());
    return record->second.color;
}

COLORREF CDirStatDoc::GetZoomColor() const
{
    return RGB(0, 0, 255);
}

CExtensionData* CDirStatDoc::GetExtensionData()
{
    return &m_extensionData;
}

SExtensionRecord* CDirStatDoc::GetExtensionDataRecord(const std::wstring& ext)
{
    std::scoped_lock guard(m_extensionMutex);
    return &m_extensionData[ext];
}

ULONGLONG CDirStatDoc::GetRootSize() const
{
    ASSERT(m_rootItem != nullptr);
    ASSERT(IsRootDone());
    return m_rootItem->GetSizePhysical();
}

// Starts a refresh of all mount points in our tree.
// Called when the user changes the follow mount points option.
//
void CDirStatDoc::RefreshReparsePointItems()
{
    CWaitCursor wc;
    
    if (CItem* root = GetRootItem(); nullptr != root)
    {
        RecurseRefreshReparsePoints(root);
    }
}

bool CDirStatDoc::HasRootItem() const
{
    return m_rootItem != nullptr;
}

bool CDirStatDoc::IsRootDone() const
{
    return HasRootItem() && m_rootItem->IsDone();
}

bool CDirStatDoc::IsScanRunning() const
{
    if (!m_thread.has_value()) return false;

    DWORD exitCode;
    GetExitCodeThread(const_cast<std::jthread&>(*m_thread).native_handle(), &exitCode);
    return (exitCode == STILL_ACTIVE);
}

CItem* CDirStatDoc::GetRootItem() const
{
    return m_rootItem;
}

CItem* CDirStatDoc::GetZoomItem() const
{
    return m_zoomItem;
}

bool CDirStatDoc::IsZoomed() const
{
    return GetZoomItem() != GetRootItem();
}

void CDirStatDoc::SetHighlightExtension(const std::wstring & ext)
{
    m_highlightExtension = ext;
    CMainFrame::Get()->UpdatePaneText();
}

std::wstring CDirStatDoc::GetHighlightExtension() const
{
    return m_highlightExtension;
}

// The root item has been deleted.
//
void CDirStatDoc::UnlinkRoot()
{
    CMainFrame::Get()->InvokeInMessageThread([this]
    {
        CMainFrame::Get()->MinimizeTreeMapView();
        CMainFrame::Get()->MinimizeExtensionView();
        DeleteContents();
        UpdateAllViews(nullptr, HINT_NEWROOT);
        CMainFrame::Get()->SetProgressComplete();
    });
}

// Determines whether a UDC works for a given item.
//
bool CDirStatDoc::UserDefinedCleanupWorksForItem(USERDEFINEDCLEANUP* udc, const CItem* item) const
{
    return item != nullptr && (
        (item->IsTypeOrFlag(IT_DRIVE) && udc->WorksForDrives) ||
        (item->IsTypeOrFlag(IT_DIRECTORY) && udc->WorksForDirectories) ||
        (item->IsTypeOrFlag(IT_FILE) && udc->WorksForFiles) ||
        (item->HasUncPath() && udc->WorksForUncPaths));
}

void CDirStatDoc::OpenItem(const CItem* item, const std::wstring & verb)
{
    ASSERT(item != nullptr);

    // Ignore if special reserved item
    if (item->IsTypeOrFlag(ITF_RESERVED)) return;

    // Determine path to feed into shell function
    SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree, nullptr);
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

void CDirStatDoc::RecurseRefreshReparsePoints(CItem* items) const
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

void CDirStatDoc::RebuildExtensionData()
{
    // Do initial extension information population if necessary
    if (m_extensionData.empty())
    {
        GetRootItem()->ExtensionDataProcessChildren();
    }

    // Collect iterators to the map entries to avoid copying keys
    std::vector<CExtensionData::iterator> sortedExtensions;
    sortedExtensions.reserve(m_extensionData.size());
    for (auto it = m_extensionData.begin(); it != m_extensionData.end(); ++it)
    {
        sortedExtensions.emplace_back(it);
    }

    // Sort the iterators based on total bytes in descending order
    std::ranges::sort(sortedExtensions, [](const auto& a_it, const auto& b_it)
    {
        return a_it->second.GetBytes() > b_it->second.GetBytes();
    });

    // Initialize colors if not already done
    static std::vector<COLORREF> colors;
    if (colors.empty())
    {
        CTreeMap::GetDefaultPalette(colors);
    }

    // Assign primary colors to extensions
    const auto extensionsSize = sortedExtensions.size();
    const auto primaryColorsMax = min(colors.size(), extensionsSize);
    for (const size_t i : std::views::iota(0u, primaryColorsMax))
    {
        sortedExtensions[i]->second.color = colors[i];
    }

    // Assign fallback colors to extensions
    const auto fallbackColor = colors.back();
    for (const size_t i : std::views::iota(primaryColorsMax, extensionsSize))
    {
        sortedExtensions[i]->second.color = fallbackColor;
    }
}

void CDirStatDoc::DeletePhysicalItems(const std::vector<CItem*>& items, const bool toTrashBin, const bool emptyOnly) const
{
    if (COptions::ShowDeleteWarning)
    {
        // Build list of file paths for the message box
        std::vector<std::wstring> filePaths;
        for (const auto& item : items)
        {
            filePaths.push_back(item->GetPath());
        }

        // Display the file deletion warning dialog
        CMessageBoxDlg warning(
            Localization::Lookup(emptyOnly ? IDS_EMPTY_FOLDER_WARNING : IDS_DELETE_WARNING),
            Localization::Lookup(IDS_DELETE_TITLE),
            MB_YESNO | MB_ICONWARNING, AfxGetMainWnd(), filePaths,
            Localization::Lookup(IDS_DONT_SHOW_AGAIN), false);

        // Change default width and display
        warning.SetInitialWindowSize({ 600, 400 });
        if (IDYES != warning.DoModal()) return;

        // Save off the deletion warning preference
        COptions::ShowDeleteWarning = !warning.IsCheckboxChecked();
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
    if (!toTrashBin) CProgressDlg(totalItems, false, AfxGetMainWnd(), [&](CProgressDlg* pdlg)
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
        CProgressDlg(0, false, AfxGetMainWnd(), [&](const CProgressDlg* pdlg)
            {
                // For trash bin operations, use IFileOperation directly
                auto flags = FOFX_SHOWELEVATIONPROMPT | (COptions::ShowMicrosoftProgress ? FOF_NOCONFIRMMKDIR : FOF_NO_UI);
                if (toTrashBin) flags |= FOFX_ADDUNDORECORD | FOFX_RECYCLEONDELETE;

                CComPtr<IFileOperation> fileOperation;
                if (FAILED(::CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fileOperation))) ||
                    FAILED(fileOperation->SetOwnerWindow(*pdlg)) ||
                    FAILED(fileOperation->SetOperationFlags(flags)))
                {
                    return;
                }

                // Add all items into a single deletion operation
                for (const auto& item : itemsToDelete)
                {
                    CComPtr<IShellItem> shellitem;
                    SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree, ILCreateFromPath(item->GetPath().c_str()));
                    if (pidl == nullptr || FAILED(SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&shellitem)))) continue;
                    fileOperation->DeleteItem(shellitem, nullptr);
                }

                // Do all deletions
                const HRESULT res = fileOperation->PerformOperations();
                if (res != S_OK) VTRACE(L"File Operation Failed: {}", TranslateError(res));
            }).DoModal();

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

void CDirStatDoc::SetZoomItem(CItem* item)
{
    m_zoomItem = item;
    UpdateAllViews(nullptr, HINT_ZOOMCHANGED);
}

// Starts a refresh of an item.
// If the physical item has been deleted,
// updates selection, zoom and working item accordingly.
//
void CDirStatDoc::RefreshItem(const std::vector<CItem*>& item) const
{
    Get()->StartScanningEngine(item);
}

// UDC confirmation dialog.
//
void CDirStatDoc::AskForConfirmation(USERDEFINEDCLEANUP* udc, const CItem* item)
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

void CDirStatDoc::PerformUserDefinedCleanup(USERDEFINEDCLEANUP* udc, const CItem* item)
{
    CWaitCursor wc;

    const std::wstring path = item->GetPath();

    // Verify that path still exists
    if (item->IsTypeOrFlag(IT_DIRECTORY, IT_DRIVE))
    {
        if (!FolderExists(path) && !DriveExists(path))
        {
            DisplayError(Localization::Format(IDS_DIRECTORYs_NOT_EXIST, path));
            throw;
        }
    }
    else
    {
        ASSERT(item->IsTypeOrFlag(IT_FILE));

        if (!::PathFileExists(path.c_str()))
        {
            DisplayError(Localization::Format(IDS_THEFILEsDOESNOTEXIST, path));
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

void CDirStatDoc::RefreshAfterUserDefinedCleanup(const USERDEFINEDCLEANUP* udc, CItem* item, std::vector<CItem*> & refreshQueue) const
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

void CDirStatDoc::RecursiveUserDefinedCleanup(USERDEFINEDCLEANUP* udc, const std::wstring& rootPath, const std::wstring& currentPath)
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

void CDirStatDoc::CallUserDefinedCleanup(const bool isDirectory, const std::wstring& format, const std::wstring& rootPath, const std::wstring& currentPath, const bool showConsoleWindow, const bool wait)
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
        DisplayError(Localization::Format(IDS_PROCESS_ERRORssss,
            app, cmdline, directory, TranslateError()));
        throw;
    }

    CloseHandle(pi.hThread);

    if (wait)
    {
        WaitForHandleWithRepainting(pi.hProcess);
    }

    CloseHandle(pi.hProcess);
}

std::wstring CDirStatDoc::BuildUserDefinedCleanupCommandLine(const std::wstring & format, const std::wstring & rootPath, const std::wstring & currentPath)
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

void CDirStatDoc::PushReselectChild(CItem* item)
{
    m_reselectChildStack.push_back(item);
}

CItem* CDirStatDoc::PopReselectChild()
{
    if (m_reselectChildStack.empty()) return nullptr;
    CItem* item = m_reselectChildStack.back();
    m_reselectChildStack.pop_back();
    return item;
}

void CDirStatDoc::ClearReselectChildStack()
{
    m_reselectChildStack.clear();
}

bool CDirStatDoc::IsReselectChildAvailable() const
{
    return !m_reselectChildStack.empty();
}

bool CDirStatDoc::FileTreeHasFocus()
{
    return LF_FILETREE == CMainFrame::Get()->GetLogicalFocus();
}

bool CDirStatDoc::DupeListHasFocus()
{
    return LF_DUPELIST == CMainFrame::Get()->GetLogicalFocus();
}

bool CDirStatDoc::TopListHasFocus()
{
    return LF_TOPLIST == CMainFrame::Get()->GetLogicalFocus();
}

bool CDirStatDoc::SearchListHasFocus()
{
    return LF_SEARCHLIST == CMainFrame::Get()->GetLogicalFocus();
}

bool CDirStatDoc::WatcherListHasFocus()
{
    return LF_WATCHERLIST == CMainFrame::Get()->GetLogicalFocus();
}

CTreeListControl* CDirStatDoc::GetFocusControl()
{
    if (DupeListHasFocus()) return CFileDupeControl::Get();
    if (TopListHasFocus()) return CFileTopControl::Get();
    if (SearchListHasFocus()) return CFileSearchControl::Get();
    if (WatcherListHasFocus()) return CFileWatcherControl::Get();
    return CFileTreeControl::Get();
}

void CDirStatDoc::UpdateAllViews(CView* pSender, VIEW_HINT hint, CItem* pHint)
{
    InvalidateSelectionCache();

    CDocument::UpdateAllViews(pSender, static_cast<LONG>(hint), reinterpret_cast<CObject*>(pHint));
}

std::vector<CItem*> CDirStatDoc::GetAllSelected()
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
        if (item->GetLinkedItem()) continue;
        
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

void CDirStatDoc::InvalidateSelectionCache()
{
    m_selectionCacheValid = false;
}

void CDirStatDoc::OnUpdateCentralHandler(CCmdUI* pCmdUI)
{
    struct commandFilter
    {
        bool allowNone = false; // allow display when nothing is selected
        bool allowMany = false; // allow display when multiple items are selected
        bool allowEarly = false; // allow display before processing is finished
        LOGICAL_FOCUS focus = LF_NONE; // restrict which views support this selection
        std::vector<ITEMTYPE> typesAllow{ ITF_ANY }; // only display if these types are allowed
        bool (*extra)(CItem*) = nullptr; // extra checks
    };

    // special conditions
    static auto doc = this;
    static bool (*canZoomOut)(CItem*) = [](CItem*) { return doc->GetZoomItem() != doc->GetRootItem(); };
    static bool (*parentNotNull)(CItem*) = [](CItem* item) { return item != nullptr && item->GetParent() != nullptr; };
    static bool (*reselectAvail)(CItem*) = [](CItem*) { return doc->IsReselectChildAvailable(); };
    static bool (*notRoot)(CItem*) = [](CItem* item) { return item != nullptr && !item->IsRootItem(); };
    static bool (*hasRecycleBin)(CItem*) = [](CItem* item) { return item != nullptr && !item->IsRootItem() && IsLocalDrive(item->GetPath()); };
    static bool (*isResumable)(CItem*) = [](CItem*) { return CMainFrame::Get()->IsScanSuspended(); };
    static bool (*isSuspendable)(CItem*) = [](CItem*) { return doc->HasRootItem() && !doc->IsRootDone() && !CMainFrame::Get()->IsScanSuspended(); };
    static bool (*isStoppable)(CItem*) = [](CItem*) { return doc->HasRootItem() && !doc->IsRootDone(); };
    static bool (*isHibernate)(CItem*) = [](CItem*) { return IsElevationActive() && IsHibernateEnabled(); };
    static bool (*isElevated)(CItem*) = [](CItem*) { return IsElevationActive(); };
    static bool (*isElevationAvailable)(CItem*) = [](CItem*) { return IsElevationActive(); };
    static bool (*isDupeTabVisible)(CItem*) = [](CItem*) { return CMainFrame::Get()->GetFileTabbedView()->IsDupeTabVisible(); };
    static bool (*isVhdFile)(CItem*) = [](CItem* item) { return item != nullptr && IsElevationActive() && (!item->IsTypeOrFlag(IT_FILE) || item->GetExtension() == L".vhdx"); };
    
    static std::unordered_map<UINT, const commandFilter> filters
    {
        // ID                           none   many   early  focus        types
        { ID_CLEANUP_DELETE,          { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE }, notRoot } },
        { ID_CLEANUP_DELETE_BIN,      { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE }, hasRecycleBin } },
        { ID_CLEANUP_DISK_CLEANUP  ,  { true,  true,  false, LF_NONE,     { ITF_ANY }, isElevationAvailable } },
        { ID_CLEANUP_MOVE_TO,         { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE }, notRoot } },
        { ID_CLEANUP_REMOVE_PROGRAMS, { true,  true,  false, LF_NONE,     { ITF_ANY } } },
        { ID_CLEANUP_DISM_ANALYZE,    { true,  true,  true,  LF_NONE,     { ITF_ANY }, isElevationAvailable } },
        { ID_CLEANUP_DISM_NORMAL,     { true,  true,  false, LF_NONE,     { ITF_ANY }, isElevationAvailable } },
        { ID_CLEANUP_DISM_RESET,      { true,  true,  false, LF_NONE,     { ITF_ANY }, isElevationAvailable } },
        { ID_CLEANUP_EMPTY_BIN,       { true,  true,  false, LF_NONE,     { ITF_ANY } } },
        { ID_CLEANUP_EMPTY_FOLDER,    { true,  true,  false, LF_NONE,     { IT_DIRECTORY }, notRoot } },
        { ID_CLEANUP_EXPLORER_SELECT, { false, true,  true,  LF_NONE,     { IT_DIRECTORY, IT_FILE } } },
        { ID_CLEANUP_HIBERNATE,       { true,  true,  false, LF_NONE,     { ITF_ANY }, isHibernate } },
        { ID_CLEANUP_OPEN_IN_CONSOLE, { false, true,  true,  LF_NONE,     { IT_DRIVE, IT_DIRECTORY, IT_FILE } } },
        { ID_CLEANUP_OPEN_IN_PWSH,    { false, true,  true,  LF_NONE,     { IT_DRIVE, IT_DIRECTORY, IT_FILE } } },
        { ID_CLEANUP_OPEN_SELECTED,   { false, true,  true,  LF_NONE,     { IT_MYCOMPUTER, IT_DRIVE, IT_DIRECTORY, IT_FILE } } },
        { ID_CLEANUP_PROPERTIES,      { false, true,  true,  LF_NONE,     { IT_MYCOMPUTER, IT_DRIVE, IT_DIRECTORY, IT_FILE } } },
        { ID_CLEANUP_OPTIMIZE_VHD,    { false, true,  false, LF_NONE,     { IT_DRIVE, IT_DIRECTORY, IT_FILE }, isVhdFile } },
        { ID_CLEANUP_REMOVE_LOCAL,    { true,  true,  false, LF_NONE,     { ITF_ANY }, isElevated } },
        { ID_CLEANUP_REMOVE_MOTW,     { false, true,  false, LF_NONE,     { IT_DRIVE, IT_DIRECTORY, IT_FILE } } },
        { ID_CLEANUP_REMOVE_ROAMING,  { true,  true,  false, LF_NONE,     { ITF_ANY }, isElevated } },
        { ID_CLEANUP_REMOVE_SHADOW,   { true,  true,  false, LF_NONE,     { ITF_ANY }, isElevated } },
        { ID_COMPRESS_LZNT1,          { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE } } },
        { ID_COMPRESS_LZX,            { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE } } },
        { ID_COMPRESS_NONE,           { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE } } },
        { ID_COMPRESS_XPRESS16K,      { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE } } },
        { ID_COMPRESS_XPRESS4K,       { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE } } },
        { ID_COMPRESS_XPRESS8K,       { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE } } },
        { ID_COMPUTE_HASH,            { false, false, true,  LF_NONE,     { IT_FILE } } },
        { ID_EDIT_COPY_CLIPBOARD,     { false, true,  true,  LF_NONE,     { IT_DRIVE, IT_DIRECTORY, IT_FILE } } },
        { ID_FILTER,                  { true,  true,  true,  LF_NONE,     { ITF_ANY } } },
        { ID_INDICATOR_DISK,          { true,  true,  false, LF_NONE,     { ITF_ANY } } },
        { ID_INDICATOR_IDLE,          { true,  true,  true,  LF_NONE,     { ITF_ANY } } },
        { ID_INDICATOR_RAM,           { true,  true,  true,  LF_NONE,     { ITF_ANY } } },
        { ID_INDICATOR_SIZE,          { true,  true,  false, LF_NONE,     { ITF_ANY } } },
        { ID_REFRESH_ALL,             { true,  true,  false, LF_NONE,     { ITF_ANY } } },
        { ID_REFRESH_SELECTED,        { false, true,  false, LF_NONE,     { IT_MYCOMPUTER, IT_DRIVE, IT_DIRECTORY, IT_FILE } } },
        { ID_SAVE_DUPLICATES,         { true,  true,  false, LF_NONE,     { ITF_ANY }, isDupeTabVisible } },
        { ID_SAVE_RESULTS,            { true,  true,  false, LF_NONE,     { ITF_ANY } } },
        { ID_SCAN_RESUME,             { true,  true,  true,  LF_NONE,     { ITF_ANY }, isResumable } },
        { ID_SCAN_STOP,               { true,  true,  true,  LF_NONE,     { ITF_ANY }, isStoppable } },
        { ID_SCAN_SUSPEND,            { true,  true,  true,  LF_NONE,     { ITF_ANY }, isSuspendable } },
        { ID_SEARCH,                  { true,  true,  false, LF_NONE,     { ITF_ANY } } },
        { ID_TREEMAP_RESELECT_CHILD,  { true,  true,  true,  LF_FILETREE, { ITF_ANY }, reselectAvail } },
        { ID_TREEMAP_SELECT_PARENT,   { false, false, true,  LF_FILETREE, { ITF_ANY }, parentNotNull } },
        { ID_TREEMAP_ZOOMIN,          { false, false, false, LF_FILETREE, { IT_DRIVE , IT_DIRECTORY } } },
        { ID_TREEMAP_ZOOMOUT,         { true,  true,  false, LF_FILETREE, { ITF_ANY }, canZoomOut } },
        { ID_VIEW_SHOWFREESPACE,      { true,  true,  false, LF_NONE,     { ITF_ANY } } },
        { ID_VIEW_SHOWUNKNOWN,        { true,  true,  false, LF_NONE,     { ITF_ANY } } }
    };

    const auto it = filters.find(pCmdUI->m_nID);
    if (it == filters.end())
    {
        ASSERT(FALSE);
        return;
    }

    const auto& filter = it->second;
    const auto& items = (filter.allowNone && filter.extra == nullptr) ?
        std::vector<CItem*>{} : GetAllSelected();

    bool allow = true;
    allow &= filter.focus == LF_NONE || (CMainFrame::Get()->GetLogicalFocus() & filter.focus) > 0;
    allow &= filter.allowNone || !items.empty();
    allow &= filter.allowMany || items.size() <= 1;
    allow &= filter.allowEarly || (IsRootDone() && !IsScanRunning());
    if (items.empty() && filter.extra != nullptr) allow &= filter.extra(nullptr);
    for (const auto& item : items)
    {
        allow &= filter.typesAllow.front() == ITF_ANY || !item->IsTypeOrFlag(ITF_RESERVED);
        allow &= (filter.extra == nullptr) || filter.extra(item);
        allow &= std::ranges::any_of(filter.typesAllow,
            [item](ITEMTYPE type) { return item->IsTypeOrFlag(type); });
    }

    pCmdUI->Enable(allow);
}

void CDirStatDoc::OnUpdateCompressionHandler(CCmdUI* pCmdUI)
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
BEGIN_MESSAGE_MAP(CDirStatDoc, CDocument)
    ON_COMMAND_UPDATE_WRAPPER(ID_REFRESH_SELECTED, OnRefreshSelected)
    ON_COMMAND_UPDATE_WRAPPER(ID_REFRESH_ALL, OnRefreshAll)
    ON_COMMAND(ID_LOAD_RESULTS, OnLoadResults)
    ON_COMMAND_UPDATE_WRAPPER(ID_SAVE_RESULTS, OnSaveResults)
    ON_COMMAND_UPDATE_WRAPPER(ID_SAVE_DUPLICATES, OnSaveDuplicates)
    ON_COMMAND_UPDATE_WRAPPER(ID_EDIT_COPY_CLIPBOARD, OnEditCopy)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_EMPTY_BIN, OnCleanupEmptyRecycleBin)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_MOVE_TO, OnCleanupMoveTo)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWFREESPACE, OnUpdateViewShowFreeSpace)
    ON_COMMAND(ID_VIEW_SHOWFREESPACE, OnViewShowFreeSpace)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWUNKNOWN, OnUpdateViewShowUnknown)
    ON_COMMAND(ID_VIEW_SHOWUNKNOWN, OnViewShowUnknown)
    ON_COMMAND_UPDATE_WRAPPER(ID_TREEMAP_ZOOMIN, OnTreeMapZoomIn)
    ON_COMMAND_UPDATE_WRAPPER(ID_TREEMAP_ZOOMOUT, OnTreeMapZoomOut)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_EXPLORER_SELECT, OnExplorerSelect)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_OPEN_IN_CONSOLE, OnCommandPromptHere)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_OPEN_IN_PWSH, OnPowerShellHere)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_DELETE_BIN, OnCleanupDeleteToBin)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_DELETE, OnCleanupDelete)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_EMPTY_FOLDER, OnCleanupEmptyFolder)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_REMOVE_SHADOW, OnRemoveShadowCopies)
    ON_COMMAND_UPDATE_WRAPPER(ID_SEARCH, OnSearch)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_DISM_ANALYZE, OnExecuteDismAnalyze)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_DISM_NORMAL, OnExecuteDism)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_DISM_RESET, OnExecuteDismReset)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_HIBERNATE, OnDisableHibernateFile)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_REMOVE_ROAMING, OnRemoveRoamingProfiles)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_REMOVE_LOCAL, OnRemoveLocalProfiles)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_DISK_CLEANUP, OnExecuteDiskCleanupUtility)
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
    ON_COMMAND_UPDATE_WRAPPER(ID_SCAN_RESUME, OnScanResume)
    ON_COMMAND_UPDATE_WRAPPER(ID_SCAN_SUSPEND, OnScanSuspend)
    ON_COMMAND_UPDATE_WRAPPER(ID_SCAN_STOP, OnScanStop)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_RAM, OnUpdateCentralHandler)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_DISK, OnUpdateCentralHandler)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_IDLE, OnUpdateCentralHandler)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_SIZE, OnUpdateCentralHandler)    
    ON_UPDATE_COMMAND_UI(ID_CLEANUP_DISK_CLEANUP, OnUpdateCentralHandler)
    ON_COMMAND_RANGE(CONTENT_MENU_MINCMD, CONTENT_MENU_MAXCMD, OnContextMenuExplore)
END_MESSAGE_MAP()

void CDirStatDoc::OnRefreshSelected()
{
    // Optimize refresh selected when done on root item
    const auto& selected = GetAllSelected();
    if (selected.size() == 1 && selected.at(0) == GetRootItem()) OnRefreshAll();
    else RefreshItem(selected);
}

void CDirStatDoc::OnRefreshAll()
{
    OnOpenDocument(Get()->GetPathName().GetString());
}

void CDirStatDoc::OnSaveResults()
{
    // Request the file path from the user
    const std::wstring fileSelectString = std::format(L"{} (*.csv)|*.csv|{} (*.*)|*.*||",
        Localization::Lookup(IDS_CSV_FILES), Localization::Lookup(IDS_ALL_FILES));
    CFileDialog dlg(FALSE, L"csv", nullptr, OFN_EXPLORER | OFN_DONTADDTORECENT, fileSelectString.c_str());
    if (dlg.DoModal() != IDOK) return;

    CProgressDlg(0, true, AfxGetMainWnd(), [&](CProgressDlg*)
    {
        SaveResults(dlg.GetPathName().GetString(), GetRootItem());
    }).DoModal();
}

void CDirStatDoc::OnSaveDuplicates()
{
    // Request the file path from the user
    const std::wstring fileSelectString = std::format(L"{} (*.csv)|*.csv|{} (*.*)|*.*||",
        Localization::Lookup(IDS_CSV_FILES), Localization::Lookup(IDS_ALL_FILES));
    CFileDialog dlg(FALSE, L"csv", nullptr, OFN_EXPLORER | OFN_DONTADDTORECENT, fileSelectString.c_str());
    if (dlg.DoModal() != IDOK) return;

    CProgressDlg(0, true, AfxGetMainWnd(), [&](CProgressDlg*)
    {
        SaveDuplicates(dlg.GetPathName().GetString(), CFileDupeControl::Get()->GetRootItem());
    }).DoModal();
}

void CDirStatDoc::OnLoadResults()
{
    // Request the file path from the user
    const std::wstring fileSelectString = std::format(L"{} (*.csv)|*.csv|{} (*.*)|*.*||",
        Localization::Lookup(IDS_CSV_FILES), Localization::Lookup(IDS_ALL_FILES));
    CFileDialog dlg(TRUE, L"csv", nullptr, OFN_EXPLORER | OFN_DONTADDTORECENT | OFN_PATHMUSTEXIST, fileSelectString.c_str());
    if (dlg.DoModal() != IDOK) return;

    CItem* newroot = nullptr;
    CProgressDlg(0, true, AfxGetMainWnd(), [&](CProgressDlg*)
    {
        newroot = LoadResults(dlg.GetPathName().GetString());
    }).DoModal();

    if (newroot != nullptr) Get()->OnOpenDocument(newroot);
}

void CDirStatDoc::OnEditCopy()
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

void CDirStatDoc::OnCleanupEmptyRecycleBin()
{
    CProgressDlg(0, true, AfxGetMainWnd(), [](CProgressDlg*)
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

void CDirStatDoc::OnRemoveShadowCopies()
{
    // Show progress dialog and compress files
    ULONGLONG count = 0, bytesUsed = 0;
    QueryShadowCopies(count, bytesUsed);

    CProgressDlg(static_cast<size_t>(count), false, AfxGetMainWnd(), [](CProgressDlg* pdlg)
    {
        RemoveWmiInstances(L"Win32_ShadowCopy", pdlg);
    }).DoModal();

    GetRootItem()->UpdateFreeSpaceItem();
}

void CDirStatDoc::OnUpdateViewShowFreeSpace(CCmdUI* pCmdUI)
{
    OnUpdateCentralHandler(pCmdUI);
    pCmdUI->SetCheck(m_showFreeSpace);
}

void CDirStatDoc::OnViewShowFreeSpace()
{
    for (const auto& drive : GetRootItem()->GetDriveItems())
    {
        if (m_showFreeSpace)
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
    m_showFreeSpace = !m_showFreeSpace;
    COptions::ShowFreeSpace = m_showFreeSpace;

    // Force recalculation and graph refresh
    StartScanningEngine({});
}

void CDirStatDoc::OnUpdateViewShowUnknown(CCmdUI* pCmdUI)
{
    OnUpdateCentralHandler(pCmdUI);
    pCmdUI->SetCheck(m_showUnknown);
}

void CDirStatDoc::OnViewShowUnknown()
{
    for (const auto& drive : GetRootItem()->GetDriveItems())
    {
        if (m_showUnknown)
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
    m_showUnknown = !m_showUnknown;
    COptions::ShowUnknown = m_showUnknown;

    // Force recalculation and graph refresh
    StartScanningEngine({});
}

void CDirStatDoc::OnTreeMapZoomIn()
{
    const auto & item = CFileTreeControl::Get()->GetFirstSelectedItem<CItem>();
    if (item != nullptr)
    {
        SetZoomItem(item);
    }
}

void CDirStatDoc::OnTreeMapZoomOut()
{
    if (GetZoomItem() != nullptr)
    {
        SetZoomItem(GetZoomItem()->GetParent());
    }
}

void CDirStatDoc::OnExplorerSelect()
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
        SmartPointer<LPITEMIDLIST> parent(CoTaskMemFree);
        parent = ILCreateFromPath(path.c_str());

        // ignore unresolvable (e.g., deleted) files
        if (parent == nullptr)
        {
            ASSERT(FALSE);
            return;
        }

        // structures to hold and track pidls for children
        std::vector<SmartPointer<LPITEMIDLIST>> pidlCleanup;
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

void CDirStatDoc::OnCommandPromptHere()
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

void CDirStatDoc::OnPowerShellHere()
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

void CDirStatDoc::OnCleanupDeleteToBin()
{
    DeletePhysicalItems(GetAllSelected(), true);
}

void CDirStatDoc::OnCleanupDelete()
{
    DeletePhysicalItems(GetAllSelected(), false);
}

void CDirStatDoc::OnCleanupEmptyFolder()
{
    DeletePhysicalItems(GetAllSelected(), false, true);
}

void CDirStatDoc::OnCleanupMoveTo()
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
    CProgressDlg(0, false, AfxGetMainWnd(), [&](const CProgressDlg* pdlg)
    {
        // Create file operation object
        CComPtr<IFileOperation> fileOperation;
        CComPtr<IShellItem> destShellItem;
        const auto flags = FOFX_SHOWELEVATIONPROMPT | (COptions::ShowMicrosoftProgress ? FOF_NOCONFIRMMKDIR : FOF_NO_UI);
        if (FAILED(::CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fileOperation))) ||
            FAILED(fileOperation->SetOwnerWindow(*pdlg)) ||
            FAILED(fileOperation->SetOperationFlags(flags)) ||
            FAILED(SHCreateItemFromParsingName(destFolder.c_str(), nullptr, IID_PPV_ARGS(&destShellItem))))
        {
            return;
        }

        // Add all items to the move operation
        for (const auto& item : items)
        {
            CComPtr<IShellItem> sourceShellItem;
            SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree, ILCreateFromPath(item->GetPath().c_str()));
            if (pidl == nullptr || FAILED(SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&sourceShellItem))))
            {
                continue;
            }

            fileOperation->MoveItem(sourceShellItem, destShellItem, nullptr, nullptr);
        }

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

void CDirStatDoc::OnSearch()
{
    SearchDlg search;
    search.DoModal();
}

void CDirStatDoc::OnDisableHibernateFile()
{
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

void CDirStatDoc::OnRemoveRoamingProfiles()
{
    CProgressDlg(0, false, AfxGetMainWnd(), [&](CProgressDlg* pdlg)
    {
        RemoveWmiInstances(L"Win32_UserProfile", pdlg,
            L"RoamingConfigured = TRUE");
    }).DoModal();

    GetRootItem()->UpdateFreeSpaceItem();
}

void CDirStatDoc::OnRemoveLocalProfiles()
{
    CProgressDlg(0, false, AfxGetMainWnd(), [&](CProgressDlg* pdlg)
    {
        RemoveWmiInstances(L"Win32_UserProfile", pdlg,
            L"RoamingConfigured = FALSE AND Loaded = FALSE AND Special = FALSE");
    }).DoModal();

    GetRootItem()->UpdateFreeSpaceItem();
}

void CDirStatDoc::OnExecuteDiskCleanupUtility()
{
    ShellExecuteWrapper(L"CLEANMGR.EXE");
}

void CDirStatDoc::OnExecuteProgramsFeatures()
{
    ShellExecuteWrapper(L"appwiz.cpl");
}

void CDirStatDoc::OnExecuteDismAnalyze()
{
    ExecuteCommandInConsole(L"DISM.EXE /Online /Cleanup-Image /AnalyzeComponentStore", L"DISM");
}

void CDirStatDoc::OnExecuteDismReset()
{
    ExecuteCommandInConsole(L"DISM.EXE /Online /Cleanup-Image /StartComponentCleanup /ResetBase", L"DISM");
}

void CDirStatDoc::OnExecuteDism()
{
    ExecuteCommandInConsole(L"DISM.EXE /Online /Cleanup-Image /StartComponentCleanup", L"DISM");
}

void CDirStatDoc::OnUpdateUserDefinedCleanup(CCmdUI* pCmdUI)
{
    const int i = pCmdUI->m_nID - ID_USERDEFINEDCLEANUP0;
    const auto & items = GetAllSelected();
    bool allowControl = (FileTreeHasFocus() || DupeListHasFocus() || TopListHasFocus()) &&
        COptions::UserDefinedCleanups.at(i).Enabled && !items.empty();
    if (allowControl) for (const auto & item : items)
    {
        allowControl &= UserDefinedCleanupWorksForItem(&COptions::UserDefinedCleanups[i], item);
    }

    pCmdUI->Enable(allowControl);
}

void CDirStatDoc::OnUserDefinedCleanup(const UINT id)
{
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

void CDirStatDoc::OnTreeMapSelectParent()
{
    const auto & item = CFileTreeControl::Get()->GetFirstSelectedItem<CItem>();
    PushReselectChild(item);
    CFileTreeControl::Get()->SelectItem(item->GetParent(), true, true);
    UpdateAllViews(nullptr, HINT_SELECTIONREFRESH);
}

void CDirStatDoc::OnTreeMapReselectChild()
{
    const CItem* item = PopReselectChild();
    CFileTreeControl::Get()->SelectItem(item, true, true);
    UpdateAllViews(nullptr, HINT_SELECTIONREFRESH);
}

void CDirStatDoc::OnCleanupOpenTarget()
{
    for (const auto & item : GetAllSelected())
    {
        OpenItem(item);
    }
}

void CDirStatDoc::OnCleanupProperties()
{
    for (const auto & item : GetAllSelected())
    {
        OpenItem(item, L"properties");
    }
}

void CDirStatDoc::OnComputeHash()
{
    // Compute the hash in the message thread
    std::wstring hashResult;
    const auto& items = GetAllSelected();
    CProgressDlg(0, false, AfxGetMainWnd(), [&](CProgressDlg*)
    {
        hashResult = ComputeFileHashes(items.front()->GetPath());
    }).DoModal();

    // Display result in message box
    CMessageBoxDlg dlg(hashResult, wds::strWinDirStat, MB_OK | MB_ICONINFORMATION);
    dlg.SetWidthAuto();
    dlg.DoModal();
}

constexpr CompressionAlgorithm CDirStatDoc::CompressionIdToAlg(const UINT id)
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

void CDirStatDoc::OnCleanupCompress(UINT id)
{
    CWaitCursor wc;
    const auto& itemsSelected = GetAllSelected();
    const auto& items = CItem::GetItemsRecursive(itemsSelected);
   
    // Show progress dialog and compress files
    const auto alg = CompressionIdToAlg(id);
    CProgressDlg(items.size(), false, AfxGetMainWnd(), [&](CProgressDlg* pdlg)
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

void CDirStatDoc::OnCleanupOptimizeVhd()
{
    CWaitCursor wc;
    const auto& itemsSelected = GetAllSelected();
    const auto& items = CItem::GetItemsRecursive(itemsSelected, [](const CItem* item) {
        return item->IsTypeOrFlag(IT_FILE) && item->GetExtension() == L".vhdx"; });

    // Show progress dialog and optimize VHD files
    CProgressDlg(items.size(), false, AfxGetMainWnd(), [&](CProgressDlg* pdlg)
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

void CDirStatDoc::OnScanSuspend()
{
    // Wait for system to fully shutdown
    for (auto& queue : m_queues | std::views::values)
        ProcessMessagesUntilSignaled([&queue] { queue.SuspendExecution(); });

    // Mark as suspended
    if (CMainFrame::Get() != nullptr)
        CMainFrame::Get()->SuspendState(true);
}

void CDirStatDoc::OnScanResume()
{
    for (auto& queue : m_queues | std::views::values)
        queue.ResumeExecution();

    if (CMainFrame::Get() != nullptr)
        CMainFrame::Get()->SuspendState(false);
}

void CDirStatDoc::OnScanStop()
{
    StopScanningEngine(Stop);
}

void CDirStatDoc::StopScanningEngine(StopReason stopReason)
{
    // Request for all threads to stop processing
    for (auto& queue : m_queues | std::views::values)
        ProcessMessagesUntilSignaled([&queue] { queue.SuspendExecution(); });

    // Stop m_queues from executing
    for (auto& queue : m_queues | std::views::values)
        ProcessMessagesUntilSignaled([&queue, &stopReason] { queue.CancelExecution(stopReason); });

    // Wait for wrapper thread to complete
    if (m_thread.has_value())
    {
        CWaitCursor waitCursor;
        ProcessMessagesUntilSignaled([this] { m_thread->join(); });
        m_thread.reset();
        m_queues.clear();
    }
}

void CDirStatDoc::OnContextMenuExplore(UINT nID)
{
    // get list of paths from items
    std::vector<std::wstring> paths;
    for (const auto& item : GetAllSelected())
        paths.push_back(item->GetPath());

    // query current context menu
    if (paths.empty()) return;
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

void CDirStatDoc::StartScanningEngine(std::vector<CItem*> items)
{
    // Stop any previous executions
    CWaitCursor wc;
    StopScanningEngine();

    // Address conflicts with currently zoomed/selected items
    const auto zoomItem = GetZoomItem();
    for (const auto& item : std::vector(items))
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

    // Do not attempt to update graph while scanning
    CMainFrame::Get()->GetTreeMapView()->SuspendRecalculationDrawing(true);

    // If scanning drive(s) just rescan the child nodes
    if (items.size() == 1 && items.at(0)->IsTypeOrFlag(IT_MYCOMPUTER))
    {
        items.at(0)->ResetScanStartTime();
        items = items.at(0)->GetChildren();
    }

    // Remove items in UI thread so we do not conflict with the timer updates
    const auto selectedItems = GetAllSelected();
    using VisualInfo = struct { int scrollPosition; bool wasExpanded; bool isSelected; };
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
            visualInfo[item].scrollPosition = item->GetScrollPosition();
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

        // Handle if item to be refreshed has been removed
        if (item->IsTypeOrFlag(IT_FILE, IT_DIRECTORY, IT_DRIVE) &&
            !FinderBasic::DoesFileExist(item->GetFolderPath(),
                item->IsTypeOrFlag(IT_FILE) ? item->GetName() : std::wstring()))
        {
            // Remove item from list so we do not rescan it
            std::erase(items, item);

            if (item->IsRootItem())
            {
                Get()->UnlinkRoot();
                return;
            }

            // Handle non-root item by removing from parent
            item->GetParent()->UpwardSubtractFiles(item->IsTypeOrFlag(IT_FILE) ? 1 : 0);
            item->GetParent()->UpwardSubtractFolders(item->IsTypeOrFlag(IT_FILE) ? 0 : 1);
            item->GetParent()->RemoveChild(item);
        }
    }
    CDirStatDoc::InvalidateSelectionCache();

    // Start a thread so we do not hang the message loop during inserts
    // Lambda captures assume document exists for duration of thread
    m_thread.emplace([this,items, visualInfo] () mutable
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

        // If new scan or closing, indicate done and exit early
        if (stopReason == Abort)
        {
            CMainFrame::Get()->InvokeInMessageThread([&]
            {
                CMainFrame::Get()->SetProgressComplete();
            });
            return;
        }

        // Sorting and other finalization tasks
        CItem::ScanItemsFinalize(GetRootItem());
        Get()->RebuildExtensionData();

        // Handle quiet save mode if path is set
        if (const auto csvPath = CDirStatApp::Get()->GetSaveToCsvPath(); !csvPath.empty())
        {
            // Get the document and root item
            const auto* doc = CDirStatDoc::Get();
            if (doc == nullptr || !doc->HasRootItem()) ExitProcess(1);

            // Run scan and exit with success == 0 or failure == 1
            ExitProcess(SaveResults(csvPath, doc->GetRootItem()) ? 0 : 1);
        }

        // Handle quiet save duplicates mode if path is set
        if (const auto dupeCsvPath = CDirStatApp::Get()->GetSaveDupesToCsvPath(); !dupeCsvPath.empty())
        {
            // Get the duplicate root item
            CFileDupeControl::Get()->SortItems();
            const auto* dupeRoot = CFileDupeControl::Get()->GetRootItem();
            if (dupeRoot == nullptr) ExitProcess(1);

            // Run scan and exit with success == 0 or failure == 1
            ExitProcess(SaveDuplicates(dupeCsvPath, dupeRoot) ? 0 : 1);
        }

        // Invoke a UI thread to do updates
        CMainFrame::Get()->InvokeInMessageThread([&]
        {
            CMainFrame::Get()->LockWindowUpdate();
            Get()->UpdateAllViews(nullptr);
            CMainFrame::Get()->SetProgressComplete();
            CMainFrame::Get()->RestoreExtensionView();
            CMainFrame::Get()->RestoreTreeMapView();
            CMainFrame::Get()->GetTreeMapView()->SuspendRecalculationDrawing(false);
            CMainFrame::Get()->UnlockWindowUpdate();

            // Restore pre-scan visual orientation
            for (const auto& item : visualInfo | std::views::keys)
            {
                if (GetFocusControl()->FindTreeItem(item) == -1 || !item->IsVisible()) continue;

                // Restore scroll position and selection if previously set
                item->SetScrollPosition(visualInfo[item].scrollPosition);
                if (visualInfo[item].isSelected) GetFocusControl()->SelectItem(item, false, true);
            }
        });

        // Force heap cleanup after scan
        (void) _heapmin();
    });
}

void CDirStatDoc::OnRemoveMarkOfTheWebTags()
{
    CWaitCursor wc;
    const auto& itemsSelected = GetAllSelected();
    const auto& items = CItem::GetItemsRecursive(itemsSelected);

    CProgressDlg(items.size(), false, AfxGetMainWnd(), [&](CProgressDlg* pdlg)
    {
        for (const auto item : items)
        {
            if (pdlg->IsCancelled()) break;
            DeleteFile((item->GetPathLong() + L":Zone.Identifier").c_str());
            pdlg->Increment();
        }
    }).DoModal();
}

void CDirStatDoc::OnUpdateCreateHardlink(CCmdUI* pCmdUI)
{
    // Only allow when focused on duplicate list
    if (!DupeListHasFocus())
    {
        pCmdUI->Enable(FALSE);
        return;
    }

    // Get selected items from the duplicate control
    const auto& control = CFileDupeControl::Get();
    if (control == nullptr)
    {
        pCmdUI->Enable(FALSE);
        return;
    }

    // Get the selected tree list items directly
    const auto selectedItems = control->GetAllSelected<CTreeListItem>(true);
    
    // Must have exactly 2 items selected
    if (selectedItems.size() != 2)
    {
        pCmdUI->Enable(FALSE);
        return;
    }

    // Both items must be files (have a linked item) with the same parent
    auto* item1 = selectedItems[0];
    auto* item2 = selectedItems[1];
    
    const bool bothAreFiles = item1->GetLinkedItem() != nullptr && 
                              item2->GetLinkedItem() != nullptr;
    const bool sameParent = item1->GetParent() == item2->GetParent() && 
                            item1->GetParent() != nullptr;
    
    pCmdUI->Enable(bothAreFiles && sameParent);
}

void CDirStatDoc::OnCreateHardlink()
{
    // Get selected items from the duplicate control
    const auto& control = CFileDupeControl::Get();
    if (control == nullptr) return;

    const auto selectedItems = control->GetAllSelected<CTreeListItem>(true);
    if (selectedItems.size() != 2) return;

    // Get the linked CItem objects
    const CItem* item1 = selectedItems[0]->GetLinkedItem();
    CItem* item2 = selectedItems[1]->GetLinkedItem();
    
    if (item1 == nullptr || item2 == nullptr) return;

    // Determine which file to keep and which to replace
    // Use the first selected item as source, second as target to be replaced
    const std::wstring& sourcePath = item1->GetPath();
    const std::wstring& targetPath = item2->GetPath();

    // Attempt to create the hardlink
    if (CreateHardlinkFromFile(sourcePath, targetPath))
    {
        // Refresh the target item to reflect the change
        RefreshItem(item2);
    }
}
