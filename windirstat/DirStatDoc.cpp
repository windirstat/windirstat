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
#include "FinderBasic.h"
#include "FinderNtfs.h"
#include "SearchDlg.h"
#include "MessageBoxDlg.h"
#include "ProgressDlg.h"

IMPLEMENT_DYNCREATE(CDirStatDoc, CDocument)

CDirStatDoc::CDirStatDoc() :
        m_ShowFreeSpace(COptions::ShowFreeSpace)
      , m_ShowUnknown(COptions::ShowUnknown)
{
    ASSERT(nullptr == _theDocument);
    _theDocument = this;

    VTRACE(L"sizeof(CItem) = {}", sizeof(CItem));
    VTRACE(L"sizeof(CTreeListItem) = {}", sizeof(CTreeListItem));
    VTRACE(L"sizeof(CTreeMap::Item) = {}", sizeof(CTreeMap::Item));
    VTRACE(L"sizeof(COwnerDrawnListItem) = {}", sizeof(COwnerDrawnListItem));
}

CDirStatDoc::~CDirStatDoc()
{
    delete m_RootItem;
    _theDocument = nullptr;
}

CDirStatDoc* CDirStatDoc::_theDocument = nullptr;

// Encodes a selection from the CSelectDrivesDlg into a string which can be routed as a pseudo
// document "path" through MFC and finally arrives in OnOpenDocument().
//
std::wstring CDirStatDoc::EncodeSelection(const std::vector<std::wstring>& folders)
{
    return std::accumulate(folders.begin(), folders.end(), std::wstring(),
        [&](const std::wstring& a, const std::wstring& b) {
            return a.empty() ? b : a + wds::chrPipe + b;
        });
}

// The inverse of EncodeSelection
//
std::vector<std::wstring> CDirStatDoc::DecodeSelection(const std::wstring& encodedPath)
{
    std::vector<std::wstring> selections;
    for (const auto part : std::views::split(encodedPath, wds::chrPipe)) {
        std::wstring partString(part.begin(), part.end());
        selections.emplace_back(TrimString(partString));
    }

    return selections;
}

void CDirStatDoc::DeleteContents()
{
    CWaitCursor wc;

    // Wait for system to fully shutdown
    StopScanningEngine(Abort);

    // Clean out icon queue
    GetIconHandler()->ClearAsyncShellInfoQueue();

    // Reset extension data
    GetExtensionData()->clear();

    // Cleanup visual artifacts
    if (CFileTopControl::Get() != nullptr) CFileTopControl::Get()->DeleteAllItems();
    if (CFileTreeControl::Get() != nullptr) CFileTreeControl::Get()->DeleteAllItems();
    if (CFileDupeControl::Get() != nullptr) CFileDupeControl::Get()->DeleteAllItems();
    if (CFileSearchControl::Get() != nullptr) CFileSearchControl::Get()->DeleteAllItems();

    // Cleanup structures
    delete m_RootItemDupe;
    delete m_RootItemTop;
    delete m_RootItemSearch;
    delete m_RootItem;
    m_RootItemDupe = nullptr;
    m_RootItemTop = nullptr;
    m_RootItemSearch = nullptr;
    m_RootItem = nullptr;
    m_ZoomItem = nullptr;
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
    std::vector<std::wstring> selections = DecodeSelection(spec);

    // Prepare for new root and delete any existing data
    CDocument::OnNewDocument();

    // Call base class to commit path to internal doc name string
    Get()->SetPathName(spec.c_str(), FALSE);

    // Count number of drives for validation
    const auto driveCount = static_cast<size_t>(std::ranges::count_if(selections, [](const std::wstring& str) {
        return std::regex_match(str, std::wregex(LR"(^[A-Za-z]:[\\]?$)"));
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

        const LPCWSTR name = ppszName != nullptr ? const_cast<LPCWSTR>(*ppszName) : L"This PC";
        m_RootItem = new CItem(IT_MYCOMPUTER | ITF_ROOTITEM, name);
        for (const auto & rootFolder : selections)
        {
            const auto drive = new CItem(IT_DRIVE, rootFolder);
            m_RootItem->AddChild(drive);
        }
    }
    else
    {
        const ITEMTYPE type = driveCount == 1 ? IT_DRIVE : IT_DIRECTORY;
        m_RootItem = new CItem(type | ITF_ROOTITEM, selections.front());
        m_RootItem->UpdateStatsFromDisk();
    }

    // Restore zoom scope to be the root
    m_ZoomItem = m_RootItem;

    // Set new node for extra views
    m_RootItemDupe = new CItemDupe();
    m_RootItemTop = new CItemTop();
    m_RootItemSearch = new CItemSearch();

    // Update new root for display
    UpdateAllViews(nullptr, HINT_NEWROOT);
    StartScanningEngine(std::vector({ Get()->GetRootItem() }));
    return true;
}

BOOL CDirStatDoc::OnOpenDocument(CItem * newroot)
{
    CMainFrame::Get()->MinimizeTreeMapView();
    CMainFrame::Get()->MinimizeExtensionView();

    CDocument::OnNewDocument(); // --> DeleteContents()

    // Determine root spec string
    std::wstring spec = newroot->GetName();
    if (newroot->IsTypeOrFlag(IT_MYCOMPUTER))
    {
        std::vector<std::wstring> folders;
        std::ranges::transform(newroot->GetChildren(), std::back_inserter(folders),
            [](const CItem* obj) -> std::wstring { return obj->GetName().substr(0, 2); });
        spec = EncodeSelection(folders);
    }

    Get()->SetPathName(spec.c_str(), FALSE);

    m_RootItemDupe = new CItemDupe();
    m_RootItemTop = new CItemTop();
    m_RootItemSearch = new CItemSearch();
    m_RootItem = newroot;
    m_ZoomItem = m_RootItem;

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
    static std::wstring suffix = IsElevationActive() ? L" (Administrator)" : L"";;
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
    return &m_ExtensionData;
}

SExtensionRecord* CDirStatDoc::GetExtensionDataRecord(const std::wstring& ext)
{
    std::scoped_lock guard(m_ExtensionMutex);
    return &m_ExtensionData[ext];
}

ULONGLONG CDirStatDoc::GetRootSize() const
{
    ASSERT(m_RootItem != nullptr);
    ASSERT(IsRootDone());
    return m_RootItem->GetSizePhysical();
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
    return m_RootItem != nullptr;
}

bool CDirStatDoc::IsRootDone() const
{
    return HasRootItem() && m_RootItem->IsDone();
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
    return m_RootItem;
}

CItem* CDirStatDoc::GetZoomItem() const
{
    return m_ZoomItem;
}

CItemDupe* CDirStatDoc::GetRootItemDupe() const
{
    return m_RootItemDupe;
}

CItemTop* CDirStatDoc::GetRootItemTop() const
{
    return m_RootItemTop;
}

CItemSearch* CDirStatDoc::GetRootItemSearch() const
{
    return m_RootItemSearch;
}

bool CDirStatDoc::IsZoomed() const
{
    return GetZoomItem() != GetRootItem();
}

void CDirStatDoc::SetHighlightExtension(const std::wstring & ext)
{
    m_HighlightExtension = ext;
    CMainFrame::Get()->UpdatePaneText();
}

std::wstring CDirStatDoc::GetHighlightExtension() const
{
    return m_HighlightExtension;
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

    // Determine path to feed into shell function
    SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree, nullptr);
    if (item->IsTypeOrFlag(IT_MYCOMPUTER))
    {
        (void) SHGetSpecialFolderLocation(nullptr, CSIDL_DRIVES, &pidl);
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
    SHELLEXECUTEINFO sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.hwnd = *AfxGetMainWnd();
    sei.lpVerb = verb.empty() ? nullptr : verb.c_str();
    sei.fMask = SEE_MASK_INVOKEIDLIST | SEE_MASK_IDLIST | SEE_MASK_NOZONECHECKS;
    sei.lpIDList = pidl;
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteEx(&sei);
}

void CDirStatDoc::RecurseRefreshReparsePoints(CItem* item) const
{
    std::vector<CItem*> toRefresh;

    if (item == nullptr) return;
    std::stack<CItem*> reparseStack({item});
    while (!reparseStack.empty())
    {
        const auto& qitem = reparseStack.top();
        reparseStack.pop();

        if (!item->IsTypeOrFlag(IT_DIRECTORY, IT_DRIVE)) continue;
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
                reparseStack.push(child);
            }
        }
    }

    if (!toRefresh.empty())
    {
        RefreshItem(toRefresh);
    }
}

// Gets all items of type IT_DRIVE.
//
std::vector<CItem*> CDirStatDoc::GetDriveItems() const
{
    std::vector<CItem*> drives;
    CItem* root = GetRootItem();

    if (nullptr == root)
    {
        return drives;
    }

    if (root->IsTypeOrFlag(IT_MYCOMPUTER))
    {
        for (const auto& child : root->GetChildren())
        {
            ASSERT(child->IsTypeOrFlag(IT_DRIVE));
            drives.push_back(child);
        }
    }
    else if (root->IsTypeOrFlag(IT_DRIVE))
    {
        drives.push_back(root);
    }

    return drives;
}

void CDirStatDoc::RebuildExtensionData()
{
    // Do initial extension information population if necessary
    if (m_ExtensionData.empty())
    {
        GetRootItem()->ExtensionDataProcessChildren();
    }

    // Collect iterators to the map entries to avoid copying keys
    std::vector<CExtensionData::iterator> sortedExtensions;
    sortedExtensions.reserve(m_ExtensionData.size());
    for (auto it = m_ExtensionData.begin(); it != m_ExtensionData.end(); ++it)
    {
        sortedExtensions.emplace_back(it);
    }

    // Sort the iterators based on total bytes in descending order
    std::ranges::sort(sortedExtensions, [](const auto& a_it, const auto& b_it)
    {
        return a_it->second.bytes.load() > b_it->second.bytes.load();
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
    for (std::size_t i = 0; i < primaryColorsMax; ++i)
    {
        sortedExtensions[i]->second.color = colors[i];
    }

    // Assign fallback colors to extensions
    const auto fallbackColor = colors.back();
    for (std::size_t i = primaryColorsMax; i < extensionsSize; ++i)
    {
        sortedExtensions[i]->second.color = fallbackColor;
    }
}

// Deletes a file or directory via SHFileOperation.
// Return: false, if canceled
//
bool CDirStatDoc::DeletePhysicalItems(const std::vector<CItem*>& items, const bool toTrashBin, const bool emptyOnly) const
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
        warning.SetInitialWindowSize({ 600, 600 });
        if (IDYES != warning.DoModal())
        {
            return false;
        }

        // Save off the deletion warning preference
        COptions::ShowDeleteWarning = !warning.IsCheckboxChecked();
    }

    CProgressDlg(0, true, AfxGetMainWnd(), [&](const std::atomic<bool>&, std::atomic<size_t>&)
    {
        // Build list of items to delete
        std::vector<CItem*> itemsToDelete{ items };
        if (emptyOnly)
        {
            itemsToDelete.clear();
            for (const auto& item : items)
            {
                const auto& children = item->GetChildren();
                itemsToDelete.insert(itemsToDelete.end(), children.begin(), children.end());
            }
        }

        // Determine flags to use for deletion
        auto flags = FOF_NOCONFIRMATION | FOFX_SHOWELEVATIONPROMPT | FOF_NOERRORUI;
        if (toTrashBin)
        {
            flags |= (IsWindows8OrGreater() ? (FOFX_ADDUNDORECORD | FOFX_RECYCLEONDELETE) : FOF_ALLOWUNDO);
        }

        CComPtr<IFileOperation> fileOperation;
        if (FAILED(::CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&fileOperation))) ||
            FAILED(fileOperation->SetOperationFlags(flags)))
        {
            return;
        }

        // Add all items into a single deletion operation
        for (const auto& item : items)
        {
            CComPtr<IShellItem> shellitem;
            SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree, ILCreateFromPath(item->GetPath().c_str()));
            if (pidl == nullptr || FAILED(SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&shellitem)))) continue;
            fileOperation->DeleteItem(shellitem, nullptr);
        }

        // Do all deletions
        HRESULT res = fileOperation->PerformOperations();
        if (res != S_OK) VTRACE(L"File Operation Failed: {}", TranslateError(res));

        // Re-run deletion using native function to handle any long paths that were missed
        if (!toTrashBin)
        {
            // Create a list of files and directories that still exist to remove
            std::vector<const CItem*> filesToDelete;
            std::vector<const CItem*> dirsToDelete;
            std::stack<const CItem*> childStack;
            for (const auto& item : items) childStack.push(item);
            while (!childStack.empty())
            {
                const auto& item = childStack.top();
                childStack.pop();
                if (!FinderBasic::DoesFileExist(item->GetPath())) continue;

                if (item->IsTypeOrFlag(IT_DIRECTORY))
                {
                    dirsToDelete.emplace_back(item);
                    for (const auto& child : item->GetChildren())
                    {
                        childStack.push(child);
                    }
                }
                else if (item->IsTypeOrFlag(IT_FILE))
                {
                    filesToDelete.emplace_back(item);
                }
            }

            // First remove files and then attempt directories in order from deep to shallow
            for (const auto& file : filesToDelete) DeleteFile(file->GetPathLong().c_str());
            for (const auto& dir : dirsToDelete | std::views::reverse)
            {
                std::error_code ec;
                std::filesystem::remove_all(std::filesystem::path(dir->GetPathLong().c_str()), ec);
            }
        }
    }).DoModal();

    // Create a list of items and recycler directories to refresh
    std::vector<CItem*> refresh{ items };
    if (toTrashBin) for (const auto& item : items)
    {
        if (const auto & recycler = item->FindRecyclerItem(); recycler != nullptr &&
            std::ranges::find(refresh, recycler) == refresh.end())
        {
            refresh.push_back(recycler);
        }
    }

    // Refresh the items and recycler directories
    RefreshItem(items);
    return true;
}

void CDirStatDoc::SetZoomItem(CItem* item)
{
    m_ZoomItem = item;
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

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

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
    m_ReselectChildStack.AddHead(item);
}

CItem* CDirStatDoc::PopReselectChild()
{
    return m_ReselectChildStack.RemoveHead();
}

void CDirStatDoc::ClearReselectChildStack()
{
    m_ReselectChildStack.RemoveAll();
}

bool CDirStatDoc::IsReselectChildAvailable() const
{
    return !m_ReselectChildStack.IsEmpty();
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

CTreeListControl* CDirStatDoc::GetFocusControl()
{
    if (DupeListHasFocus()) return CFileDupeControl::Get();
    if (TopListHasFocus()) return CFileTopControl::Get();
    if (SearchListHasFocus()) return CFileSearchControl::Get();
    return CFileTreeControl::Get();
}

void CDirStatDoc::UpdateAllViews(CView* pSender, VIEW_HINT hint, CObject* pHint)
{
    InvalidateSelectionCache();

    CDocument::UpdateAllViews(pSender, static_cast<LONG>(hint), pHint);
}

std::vector<CItem*> CDirStatDoc::GetAllSelected()
{
    // Check if we can use cached results
    const auto currentFocus = CMainFrame::Get()->GetLogicalFocus();
    if (m_SelectionCacheValid && m_CachedFocus == currentFocus)
    {
        return m_CachedSelection;
    }

    // Query fresh selection data
    auto selection = GetFocusControl()->GetAllSelected<CItem>();

    // Update cache
    m_CachedFocus = currentFocus;
    m_CachedSelection = selection;
    m_SelectionCacheValid = true;

    return selection;
}

void CDirStatDoc::InvalidateSelectionCache()
{
    m_SelectionCacheValid = false;
}

void CDirStatDoc::OnUpdateCentralHandler(CCmdUI* pCmdUI)
{
    struct commandFilter
    {
        bool allowNone = false; // allow display when nothing is selected
        bool allowMany = false; // allow display when multiple items are selected
        bool allowEarly = false; // allow display before processing is finished
        LOGICAL_FOCUS focus = LF_NONE; // restrict which views support this selection
        std::vector<ITEMTYPE> typesAllow = { ITF_ANY }; // only display if these types are allowed
        bool (*extra)(CItem*) = [](CItem*) { return true; }; // extra checks
    };

    // special conditions
    static auto doc = this;
    static bool (*canZoomOut)(CItem*) = [](CItem*) { return doc->GetZoomItem() != doc->GetRootItem(); };
    static bool (*parentNotNull)(CItem*) = [](CItem* item) { return item != nullptr && item->GetParent() != nullptr; };
    static bool (*reselectAvail)(CItem*) = [](CItem*) { return doc->IsReselectChildAvailable(); };
    static bool (*notRoot)(CItem*) = [](CItem* item) { return item != nullptr && !item->IsRootItem(); };
    static bool (*isResumable)(CItem*) = [](CItem*) { return CMainFrame::Get()->IsScanSuspended(); };
    static bool (*isSuspendable)(CItem*) = [](CItem*) { return doc->HasRootItem() && !doc->IsRootDone() && !CMainFrame::Get()->IsScanSuspended(); };
    static bool (*isStoppable)(CItem*) = [](CItem*) { return doc->HasRootItem() && !doc->IsRootDone(); };
    static bool (*isHibernate)(CItem*) = [](CItem*) { return IsElevationActive() && IsHibernateEnabled(); };
    static bool (*isElevated)(CItem*) = [](CItem*) { return IsElevationActive(); };
    static bool (*isElevationAvailable)(CItem*) = [](CItem*) { return IsElevationActive(); };
    static bool (*isDupeTabVisible)(CItem*) = [](CItem*) { return CMainFrame::Get()->GetFileTabbedView()->IsDupeTabVisible(); };
    
    static std::unordered_map<UINT, const commandFilter> filters
    {
        // ID                           none   many   early  focus        types
        { ID_CLEANUP_DELETE,          { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE }, notRoot } },
        { ID_CLEANUP_DELETE_BIN,      { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE }, notRoot } },
        { ID_CLEANUP_DISK_CLEANUP  ,  { true,  true,  false, LF_NONE,     { ITF_ANY }, isElevationAvailable } },
        { ID_CLEANUP_REMOVE_PROGRAMS, { true,  true,  false, LF_NONE,     { ITF_ANY } } },        
        { ID_CLEANUP_DISM_ANALYZE,    { true,  true,  false, LF_NONE,     { ITF_ANY }, isElevationAvailable } },
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
        { ID_CLEANUP_REMOVE_LOCAL,    { true,  true,  false, LF_NONE,     { ITF_ANY }, isElevated } },
        { ID_CLEANUP_REMOVE_ROAMING,  { true,  true,  false, LF_NONE,     { ITF_ANY }, isElevated } },
        { ID_CLEANUP_REMOVE_SHADOW,   { true,  true,  false, LF_NONE,     { ITF_ANY }, isElevated } },
        { ID_COMPRESS_LZNT1,          { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE } } },
        { ID_COMPRESS_LZX,            { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE } } },
        { ID_COMPRESS_NONE,           { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE } } },
        { ID_COMPRESS_XPRESS16K,      { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE } } },
        { ID_COMPRESS_XPRESS4K,       { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE } } },
        { ID_COMPRESS_XPRESS8K,       { false, true,  false, LF_NONE,     { IT_DIRECTORY, IT_FILE } } },
        { ID_COMPUTE_HASH,            { false, false, false, LF_NONE,     { IT_FILE } } },
        { ID_EDIT_COPY_CLIPBOARD,     { false, true,  true,  LF_NONE,     { IT_DRIVE, IT_DIRECTORY, IT_FILE } } },
        { ID_FILTER,                  { true,  true,  true,  LF_NONE,     { ITF_ANY } } },
        { ID_INDICATOR_DISK,          { true,  true,  false, LF_NONE,     { ITF_ANY } } },
        { ID_INDICATOR_IDLE,          { true,  true,  true,  LF_NONE,     { ITF_ANY } } },
        { ID_INDICATOR_MEM,           { true,  true,  true,  LF_NONE,     { ITF_ANY } } },
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

    if (!filters.contains(pCmdUI->m_nID))
    {
        ASSERT(FALSE);
        return;
    }

    const auto& filter = filters[pCmdUI->m_nID];
    const auto& items = (filter.allowNone && filter.extra == nullptr) ?
        std::vector<CItem*>{} : GetAllSelected();

    bool allow = true;
    allow &= filter.focus == LF_NONE || (CMainFrame::Get()->GetLogicalFocus() & filter.focus) > 0;
    allow &= filter.allowNone || !items.empty();
    allow &= filter.allowMany || items.size() <= 1;
    allow &= filter.allowEarly || (IsRootDone() && !IsScanRunning());
    if (items.empty()) allow &= filter.extra(nullptr);
    for (const auto& item : items)
    {
        allow &= filter.extra(item);
        allow &= std::any_of(filter.typesAllow.begin(), filter.typesAllow.end(),
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
    const UINT flag = pCmdUI->m_pMenu->GetMenuState(pCmdUI->m_nID, MF_BYCOMMAND);
    bool allow = (flag & (MF_DISABLED | MF_GRAYED)) == 0;
    for (const auto& item : GetAllSelected())
    {
        allow &= CompressFileAllowed(item->GetPath(),
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
    ON_UPDATE_COMMAND_UI_RANGE(ID_USERDEFINEDCLEANUP0, ID_USERDEFINEDCLEANUP9, OnUpdateUserDefinedCleanup)
    ON_COMMAND_RANGE(ID_USERDEFINEDCLEANUP0, ID_USERDEFINEDCLEANUP9, OnUserDefinedCleanup)
    ON_COMMAND_UPDATE_WRAPPER(ID_TREEMAP_SELECT_PARENT, OnTreeMapSelectParent)
    ON_COMMAND_UPDATE_WRAPPER(ID_TREEMAP_RESELECT_CHILD, OnTreeMapReselectChild)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_OPEN_SELECTED, OnCleanupOpenTarget)
    ON_COMMAND_UPDATE_WRAPPER(ID_CLEANUP_PROPERTIES, OnCleanupProperties)
    ON_COMMAND_UPDATE_WRAPPER(ID_COMPUTE_HASH, OnComputeHash)
    ON_UPDATE_COMMAND_UI_RANGE(ID_COMPRESS_NONE, ID_COMPRESS_LZX, OnUpdateCompressionHandler)
    ON_COMMAND_RANGE(ID_COMPRESS_NONE, ID_COMPRESS_LZX, OnCleanupCompress)
    ON_COMMAND_UPDATE_WRAPPER(ID_SCAN_RESUME, OnScanResume)
    ON_COMMAND_UPDATE_WRAPPER(ID_SCAN_SUSPEND, OnScanSuspend)
    ON_COMMAND_UPDATE_WRAPPER(ID_SCAN_STOP, OnScanStop)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_MEM, OnUpdateCentralHandler)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_DISK, OnUpdateCentralHandler)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_IDLE, OnUpdateCentralHandler)
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
    std::wstring fileSelectString = std::format(L"{} (*.csv)|*.csv|{} (*.*)|*.*||",
        Localization::Lookup(IDS_CSV_FILES), Localization::Lookup(IDS_ALL_FILES));
    CFileDialog dlg(FALSE, L"csv", nullptr, OFN_EXPLORER | OFN_DONTADDTORECENT, fileSelectString.c_str());
    if (dlg.DoModal() != IDOK) return;

    CProgressDlg(0, true, AfxGetMainWnd(), [&](const std::atomic<bool>&, std::atomic<size_t>&)
    {
        SaveResults(dlg.GetPathName().GetString(), GetRootItem());
    }).DoModal();
}

void CDirStatDoc::OnSaveDuplicates()
{
    // Request the file path from the user
    std::wstring fileSelectString = std::format(L"{} (*.csv)|*.csv|{} (*.*)|*.*||",
        Localization::Lookup(IDS_CSV_FILES), Localization::Lookup(IDS_ALL_FILES));
    CFileDialog dlg(FALSE, L"csv", nullptr, OFN_EXPLORER | OFN_DONTADDTORECENT, fileSelectString.c_str());
    if (dlg.DoModal() != IDOK) return;

    CProgressDlg(0, true, AfxGetMainWnd(), [&](const std::atomic<bool>&, std::atomic<size_t>&)
    {
        SaveDuplicates(dlg.GetPathName().GetString(), GetRootItemDupe());
    }).DoModal();
}

void CDirStatDoc::OnLoadResults()
{
    // Request the file path from the user
    std::wstring fileSelectString = std::format(L"{} (*.csv)|*.csv|{} (*.*)|*.*||",
        Localization::Lookup(IDS_CSV_FILES), Localization::Lookup(IDS_ALL_FILES));
    CFileDialog dlg(TRUE, L"csv", nullptr, OFN_EXPLORER | OFN_DONTADDTORECENT | OFN_PATHMUSTEXIST, fileSelectString.c_str());
    if (dlg.DoModal() != IDOK) return;

    CItem* newroot = nullptr;
    CProgressDlg(0, true, AfxGetMainWnd(), [&](const std::atomic<bool>&, std::atomic<size_t>&)
    {
        newroot = LoadResults(dlg.GetPathName().GetString());
    }).DoModal();

    Get()->OnOpenDocument(newroot);
}

void CDirStatDoc::OnEditCopy()
{
    // create concatenated paths
    std::wstring paths;
    const auto & items = GetAllSelected();
    for (const auto & item : items)
    {
        if (!paths.empty()) paths += L"\r\n";
        paths += item->GetPath();
    }

    CMainFrame::Get()->CopyToClipboard(paths);
}

void CDirStatDoc::OnCleanupEmptyRecycleBin()
{
    CProgressDlg(0, true, AfxGetMainWnd(), [](const std::atomic<bool>&, std::atomic<size_t>&)
    {
        SHEmptyRecycleBin(*AfxGetMainWnd(), nullptr, 0);
    }).DoModal();

    // locate all drive items in order to refresh recyclers
    std::vector<CItem*> toRefresh;
    for (const auto& drive : GetDriveItems())
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

    CProgressDlg(static_cast<size_t>(count), false, AfxGetMainWnd(), [](const std::atomic<bool>& cancelRequested,
        std::atomic<size_t>& progress)
    {
        RemoveWmiInstances(L"Win32_ShadowCopy", progress, cancelRequested);
    }).DoModal();

    GetRootItem()->UpdateFreeSpaceItem();
}

void CDirStatDoc::OnUpdateViewShowFreeSpace(CCmdUI* pCmdUI)
{
    OnUpdateCentralHandler(pCmdUI);
    pCmdUI->SetCheck(m_ShowFreeSpace);
}

void CDirStatDoc::OnViewShowFreeSpace()
{
    for (const auto& drive : GetDriveItems())
    {
        if (m_ShowFreeSpace)
        {
            const CItem* free = drive->FindFreeSpaceItem();
            ASSERT(free != nullptr);

            if (GetZoomItem() == free)
            {
                m_ZoomItem = free->GetParent();
            }

            drive->RemoveFreeSpaceItem();
        }
        else
        {
            drive->CreateFreeSpaceItem();
        }
    }

    // Toggle value
    m_ShowFreeSpace = !m_ShowFreeSpace;
    COptions::ShowFreeSpace = m_ShowFreeSpace;

    // Force recalculation and graph refresh
    StartScanningEngine({});
}

void CDirStatDoc::OnUpdateViewShowUnknown(CCmdUI* pCmdUI)
{
    OnUpdateCentralHandler(pCmdUI);
    pCmdUI->SetCheck(m_ShowUnknown);
}

void CDirStatDoc::OnViewShowUnknown()
{
    for (const auto& drive : GetDriveItems())
    {
        if (m_ShowUnknown)
        {
            const CItem* unknown = drive->FindUnknownItem();
            ASSERT(unknown != nullptr);

            if (GetZoomItem() == unknown)
            {
                m_ZoomItem = unknown->GetParent();
            }

            drive->RemoveUnknownItem();
        }
        else
        {
            drive->CreateUnknownItem();
        }
    }

    // Toggle value
    m_ShowUnknown = !m_ShowUnknown;
    COptions::ShowUnknown = m_ShowUnknown;

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
        SHOpenFolderAndSelectItems(parent, static_cast<UINT>(pidl.size()), const_cast<LPCITEMIDLIST*>(pidl.data()), 0);
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
        ShellExecuteWrapper(cmd, L"", L"open", *AfxGetMainWnd(), path);
    }
}

void CDirStatDoc::OnPowerShellHere()
{
    // locate PWSH
    static std::wstring pwsh;
    if (pwsh.empty()) for (const auto exe : { L"pwsh.exe", L"powershell.exe" })
    {
        SmartPointer<HMODULE> lib(FreeLibrary, LoadLibrary(exe));
        if (lib == nullptr) continue;
        pwsh.resize(MAX_PATH);
        if (GetModuleFileNameW(lib, pwsh.data(),
            static_cast<DWORD>(pwsh.size())) > 0 && GetLastError() == ERROR_SUCCESS)
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

void CDirStatDoc::OnSearch()
{
    SearchDlg search;
    search.DoModal();
}

void CDirStatDoc::OnDisableHibernateFile()
{
    DisableHibernate();

    // See if there is a hibernate file on any drive to refresh
    for (const auto& drive : GetDriveItems())
    {
        for (const auto& child : drive->GetChildren())
        {
            if (_wcsicmp(child->GetName().c_str(), L"hiberfil.sys") == 0)
            {
                StartScanningEngine({ child });
            }
        }
    }
}

void CDirStatDoc::OnRemoveRoamingProfiles()
{
    CProgressDlg(0, false, AfxGetMainWnd(), [&](const std::atomic<bool>& cancelRequested, std::atomic<size_t>& progress)
    {
        RemoveWmiInstances(L"Win32_UserProfile", progress, cancelRequested,
            L"RoamingConfigured = TRUE");
    }).DoModal();

    GetRootItem()->UpdateFreeSpaceItem();
}

void CDirStatDoc::OnRemoveLocalProfiles()
{
    CProgressDlg(0, false, AfxGetMainWnd(), [&](const std::atomic<bool>&cancelRequested, std::atomic<size_t>& progress)
    {
        RemoveWmiInstances(L"Win32_UserProfile", progress, cancelRequested,
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
    const std::wstring cmd = std::format(LR"(/C "TITLE {} & DISM.EXE {} & PAUSE)",
        L"WinDirStat - DISM", L"/Online /Cleanup-Image /AnalyzeComponentStore");
    ShellExecuteWrapper(GetCOMSPEC(), cmd, L"runas");
}

void CDirStatDoc::OnExecuteDismReset()
{
    const std::wstring cmd = std::format(LR"(/C "TITLE {} & DISM.EXE {} & PAUSE)",
        L"WinDirStat - DISM", L"/Online /Cleanup-Image /StartComponentCleanup /ResetBase");
    ShellExecuteWrapper(GetCOMSPEC(), cmd, L"runas");
}

void CDirStatDoc::OnExecuteDism()
{
    const std::wstring cmd = std::format(LR"(/C "TITLE {} & DISM.EXE {} & PAUSE)",
        L"WinDirStat - DISM", L"/Online /Cleanup-Image /StartComponentCleanup");
    ShellExecuteWrapper(GetCOMSPEC(), cmd, L"runas");
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
    const auto & items = GetAllSelected();
    for (const auto & item : items)
    {
        OpenItem(item);
    }
}

void CDirStatDoc::OnCleanupProperties()
{
    const auto & items = GetAllSelected();
    for (const auto & item : items)
    {
        OpenItem(item, L"properties");
    }
}

void CDirStatDoc::OnComputeHash()
{
    // Compute the hash in the message thread
    const auto& items = GetAllSelected();
    const std::wstring hashResult = ComputeFileHashes(items.front()->GetPath());

    // Display result in message box
    CMessageBoxDlg dlg(hashResult, Localization::LookupNeutral(AFX_IDS_APP_TITLE), MB_OK | MB_ICONINFORMATION);
    dlg.SetInitialWindowSize(CSize(1000, 200));
    dlg.DoModal();
}

CompressionAlgorithm CDirStatDoc::CompressionIdToAlg(const UINT id)
{
    const std::unordered_map<UINT, CompressionAlgorithm> compressionMap =
    {
        { ID_COMPRESS_NONE, CompressionAlgorithm::NONE },
        { ID_COMPRESS_LZNT1, CompressionAlgorithm::LZNT1 },
        { ID_COMPRESS_XPRESS4K, CompressionAlgorithm::XPRESS4K },
        { ID_COMPRESS_XPRESS8K, CompressionAlgorithm::XPRESS8K },
        { ID_COMPRESS_XPRESS16K, CompressionAlgorithm::XPRESS16K },
        { ID_COMPRESS_LZX, CompressionAlgorithm::LZX }
    };

    return compressionMap.at(id);
}

void CDirStatDoc::OnCleanupCompress(UINT id)
{
    CWaitCursor wc;
    const auto& itemsSelected = GetAllSelected();
    std::vector<const CItem*> itemsToCompress;
    std::stack<const CItem*> childStack;
    for (const auto & item : itemsSelected) { childStack.push(item); }
    while (!childStack.empty())
    {
        const auto& item = childStack.top();
        childStack.pop();

        if (item->IsTypeOrFlag(IT_DIRECTORY))
        {
            for (const auto& child : item->GetChildren())
            {
                childStack.push(child);
            }
        }
        else if (item->IsTypeOrFlag(IT_FILE))
        {
            itemsToCompress.emplace_back(item);
        }
    }

    // Show progress dialog and compress files
    const auto alg = CompressionIdToAlg(id);
    CProgressDlg(itemsToCompress.size(), false, AfxGetMainWnd(), [&](const std::atomic<bool>& cancel,
        std::atomic<size_t>& current)
    {
        for (size_t i = 0; i < itemsToCompress.size() && !cancel; ++i)
        {
            current = i + 1;
            CompressFile(itemsToCompress[i]->GetPathLong(), alg);
        }
    }).DoModal();

    // Refresh items after compression
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
    for (auto& item : GetAllSelected())
        paths.push_back(item->GetPath());

    // query current context menu
    if (paths.empty()) return;
    CComPtr contextMenu = GetContextMenu(CMainFrame::Get()->GetSafeHwnd(), paths);
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

    // Start a thread so we do not hang the message loop
    // Lambda captures assume document exists for duration of thread
    m_thread.emplace([this,items] () mutable
    {
        // Wait for other threads to finish if this was scheduled in parallel
        static std::shared_mutex mutex;
        std::scoped_lock lock(mutex);

        // If scanning drive(s) just rescan the child nodes
        if (items.size() == 1 && items.at(0)->IsTypeOrFlag(IT_MYCOMPUTER))
        {
            items.at(0)->ResetScanStartTime();
            items = items.at(0)->GetChildren();
        }

        const auto selectedItems = GetAllSelected();
        using VisualInfo = struct { int scrollPosition; bool wasExpanded; bool isSelected; };
        std::unordered_map<CItem *,VisualInfo> visualInfo;
        CMainFrame::Get()->SetRedraw(FALSE);
        for (auto item : std::vector(items))
        {
            // Clear items from duplicates and top list;
            CFileDupeControl::Get()->RemoveItem(item);
            CFileTopControl::Get()->RemoveItem(item);
            CFileSearchControl::Get()->RemoveItem(item);
            CDirStatDoc::InvalidateSelectionCache();

            // Record current visual arrangement to reapply afterward
            if (item->IsVisible())
            {
                visualInfo[item].isSelected = std::ranges::find(selectedItems, item) != selectedItems.end();
                visualInfo[item].wasExpanded = item->IsExpanded();
                visualInfo[item].scrollPosition = item->GetScrollPosition();
            }

            // Skip pruning if it is a new element
            if (!item->IsDone()) continue;
            item->ExtensionDataProcessChildren(true);
            item->UpwardRecalcLastChange(true);
            item->UpwardSubtractSizePhysical(item->GetSizePhysical());
            item->UpwardSubtractSizeLogical(item->GetSizeLogical());
            item->UpwardSubtractFiles(item->GetFilesCount());
            item->UpwardSubtractFolders(item->GetFoldersCount());
            item->RemoveAllChildren();
            item->UpwardSetUndone();

            // Child removal will collapse the item, so re-expand it
            if (visualInfo.contains(item) && item->IsVisible())
                item->SetExpanded(visualInfo[item].wasExpanded);
  
            // Handle if item to be refreshed has been removed
            if (item->IsTypeOrFlag(IT_FILE, IT_DIRECTORY, IT_DRIVE) &&
                !FinderBasic::DoesFileExist(item->GetFolderPath(),
                    item->IsTypeOrFlag(IT_FILE) ? item->GetName() : std::wstring()))
            {
                // Remove item from list so we do not rescan it
                std::erase(items, item);

                if (item->IsRootItem())
                {
                    // Handle deleted root item; this must be launched
                    // asynchronously since it will end up calling this
                    // function and could potentially deadlock
                    std::jthread ([] ()
                    {
                        Get()->UnlinkRoot();
                    }).detach();
                    CMainFrame::Get()->SetRedraw(TRUE);
                    return;
                }

                // Handle non-root item by removing from parent
                item->UpwardSubtractFiles(item->IsTypeOrFlag(IT_FILE) ? 1 : 0);
                item->UpwardSubtractFolders(item->IsTypeOrFlag(IT_FILE) ? 0 : 1);
                item->GetParent()->RemoveChild(item);
            }
        }
        CMainFrame::Get()->SetRedraw(TRUE);

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

            // Separate into separate m_queues per drive
            const auto volume = GetVolumePathNameEx(item->GetPathLong());
            if (!volume.empty())
            {
                m_queues[volume].Push(item);
            }
            else ASSERT(FALSE);
        }

        // Create subordinate threads if there is work to do
        std::unordered_map<std::wstring, FinderNtfsContext> queueContextNtfs;
        std::unordered_map<std::wstring, FinderBasicContext> queueContextBasic;
        for (auto& queue : m_queues)
        {
            queueContextNtfs.emplace(queue.first, FinderNtfsContext{});
            queueContextBasic.emplace(queue.first, FinderBasicContext{});
            queue.second.StartThreads(COptions::ScanningThreads, [&]()
            {
                CItem::ScanItems(&queue.second, queueContextNtfs[queue.first], queueContextBasic[queue.first]);
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
        auto drives = GetDriveItems();
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
            auto* doc = CDirStatDoc::Get();
            if (doc == nullptr || !doc->HasRootItem()) ExitProcess(1);

            // Exit the application with appropriate code
            ExitProcess(SaveResults(csvPath, doc->GetRootItem()) ? 0 : 1);
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
    });
}
