// DirStatDoc.cpp - Implementation of CDirStatDoc
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
#include "CsvLoader.h"
#include "deletewarningdlg.h"
#include "DirStatDoc.h"
#include "FileTreeView.h"
#include "GlobalHelpers.h"
#include "TreeMapView.h"
#include "Item.h"
#include "Localization.h"
#include "MainFrame.h"
#include "ModalShellApi.h"
#include "WinDirStat.h"
#include <common/CommonHelpers.h>
#include <common/MdExceptions.h>
#include <common/SmartPointer.h>

#include <functional>
#include <unordered_map>
#include <string>
#include <unordered_set>
#include <vector>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <stack>

CDirStatDoc* _theDocument;

CDirStatDoc* GetDocument()
{
    return _theDocument;
}

IMPLEMENT_DYNCREATE(CDirStatDoc, CDocument)

CDirStatDoc::CDirStatDoc() :
        m_ShowFreeSpace(COptions::ShowFreeSpace)
      , m_ShowUnknown(COptions::ShowUnknown)
{
    ASSERT(nullptr == _theDocument);
    _theDocument = this;

    VTRACE(L"sizeof(CItem) = {}", sizeof(CItem));
}

CDirStatDoc::~CDirStatDoc()
{
    delete m_RootItem;
    _theDocument = nullptr;
}

// Encodes a selection from the CSelectDrivesDlg into a string which can be routed as a pseudo
// document "path" through MFC and finally arrives in OnOpenDocument().
//
std::wstring CDirStatDoc::EncodeSelection(const RADIO radio, const std::wstring& folder, const std::vector<std::wstring>& drives)
{
    std::wstring ret;
    switch (radio)
    {
    case RADIO_ALLLOCALDRIVES:
    case RADIO_SOMEDRIVES:
        {
            for (std::size_t i = 0; i < drives.size(); i++)
            {
                if (i > 0)
                {
                    ret += GetEncodingSeparator();
                }
                ret += drives[i];
            }
        }
        break;

    case RADIO_AFOLDER:
        {
            ret = folder;
        }
        break;
    }
    return ret;
}

// The inverse of EncodeSelection
//
void CDirStatDoc::DecodeSelection(const std::wstring& s, std::wstring& folder, std::vector<std::wstring>& drives)
{
    folder.clear();
    drives.clear();

    // s is either something like "C:\programme"
    // or something like "C:|D:|E:".

    std::vector<std::wstring> selections;
    std::size_t i = 0;

    while (i < s.size())
    {
        std::wstring token;
        while (i < s.size() && s[i] != GetEncodingSeparator())
        {
            token += s[i++];
        }

        TrimString(token);
        ASSERT(!token.empty());
        selections.emplace_back(token);

        if (i < s.size())
        {
            i++;
        }
    }

    if (selections.size() > 1)
    {
        for (const auto & selection : selections)
        {
            ASSERT(2 == selection.size());
            ASSERT(wds::chrColon == selection[1]);
            drives.emplace_back(selection + L"\\");
        }
    }
    else if (!selections.empty())
    {
        std::wstring f = selections[0];
        if (2 == f.size() && wds::chrColon == f[1])
        {
            drives.emplace_back(f + L'\\');
        }
        else
        {
            // Remove trailing backslash, if any and not drive-root.
            if (!f.empty() && wds::chrBackslash == f.back() && (f.size() != 3 || f[1] != wds::chrColon))
            {
                f = f.substr(0, f.size() - 1);
            }

            folder = f;
        }
    }
}

WCHAR CDirStatDoc::GetEncodingSeparator()
{
    return wds::chrPipe; // This character must be one, which is not allowed in file names.
}

void CDirStatDoc::DeleteContents()
{
    CWaitCursor wc;

    // Wait for system to fully shutdown
    StopScanningEngine();

    // Cleanup structures
    delete m_RootItemDupe;
    delete m_RootItem;
    m_RootItemDupe = nullptr;
    m_RootItem = nullptr;
    m_ZoomItem = nullptr;
    CDirStatApp::Get()->ReReadMountPoints();
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
    // Temporary minimize extra reviews
    CMainFrame::Get()->MinimizeTreeMapView();
    CMainFrame::Get()->MinimizeExtensionView();

    // Prepare for new root and delete any existing data
    CDocument::OnNewDocument();

    // Decode list of folders to scan
    const std::wstring spec = lpszPathName;
    std::wstring folder;
    std::vector<std::wstring> drives;
    DecodeSelection(spec, folder, drives);

    // Return if no drives or folder were passed
    if (drives.empty() && folder.empty()) return true;

    // Determine if we should add multiple drives under a single node
    std::vector<std::wstring> rootFolders;
    if (drives.empty())
    {
        ASSERT(!folder.empty());
        m_ShowMyComputer = false;
        rootFolders.emplace_back(folder);
    }
    else
    {
        m_ShowMyComputer = drives.size() > 1;
        for (const auto & drive : drives)
        {
            rootFolders.emplace_back(drive);
        }
    }

    std::vector<CItem*> driveItems;

    if (m_ShowMyComputer)
    {
        m_RootItem = new CItem(IT_MYCOMPUTER | ITF_ROOTITEM, Localization::Lookup(IDS_MYCOMPUTER));
        for (const auto & rootFolder : rootFolders)
        {
            const auto drive = new CItem(IT_DRIVE, rootFolder);
            driveItems.emplace_back(drive);
            m_RootItem->AddChild(drive);
        }
    }
    else
    {
        const ITEMTYPE type = IsDrive(rootFolders[0]) ? IT_DRIVE : IT_DIRECTORY;
        m_RootItem = new CItem(type | ITF_ROOTITEM, rootFolders[0]);
        if (m_RootItem->IsType(IT_DRIVE))
        {
            driveItems.emplace_back(m_RootItem);
        }
        m_RootItem->UpdateStatsFromDisk();
    }
    m_ZoomItem = m_RootItem;

    // Set new node for duplicate view
    m_RootItemDupe = new CItemDupe();

    // Update new root for display
    UpdateAllViews(nullptr, HINT_NEWROOT);
    StartScanningEngine(std::vector({ GetDocument()->GetRootItem() }));
    return true;
}

BOOL CDirStatDoc::OnOpenDocument(CItem * newroot)
{
    CMainFrame::Get()->MinimizeTreeMapView();
    CMainFrame::Get()->MinimizeExtensionView();

    CDocument::OnNewDocument(); // --> DeleteContents()

    m_RootItemDupe = new CItemDupe();
    m_RootItem = newroot;
    m_ZoomItem = m_RootItem;

    UpdateAllViews(nullptr, HINT_NEWROOT);
    StartScanningEngine({});
    return true;
}

// We don't want MFCs AfxFullPath()-Logic, because lpszPathName
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

// Prefix the window Title (with percentage or "Scanning")
//
void CDirStatDoc::SetTitlePrefix(const std::wstring& prefix) const
{
    static std::wstring suffix = IsAdmin() ? L" (Administrator)" : L"";
    std::wstring docName = std::format(L"{} {} {}", prefix, GetTitle().GetString(), suffix);
    docName = TrimString(docName);
    CMainFrame::Get()->UpdateFrameTitleForDocument(docName.empty() ? nullptr : docName.c_str());
}

COLORREF CDirStatDoc::GetCushionColor(const std::wstring & ext)
{
    const auto& record = GetExtensionData()->find(ext);
    VERIFY(record != GetExtensionData()->end());
    return record->second.color;
}

COLORREF CDirStatDoc::GetZoomColor()
{
    return RGB(0, 0, 255);
}

const CExtensionData* CDirStatDoc::GetExtensionData()
{
    if (!m_ExtensionDataValid)
    {
        RebuildExtensionData();
    }
    return &m_ExtensionData;
}

ULONGLONG CDirStatDoc::GetRootSize() const
{
    ASSERT(m_RootItem != nullptr);
    ASSERT(IsRootDone());
    return m_RootItem->GetSizePhysical();
}

bool CDirStatDoc::IsDrive(const std::wstring& spec)
{
    return 3 == spec.size() && wds::chrColon == spec[1] && wds::chrBackslash == spec[2];
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

bool CDirStatDoc::IsZoomed() const
{
    return GetZoomItem() != GetRootItem();
}

void CDirStatDoc::SetHighlightExtension(const std::wstring & ext)
{
    m_HighlightExtension = ext;
    CMainFrame::Get()->SetSelectionMessageText();
}

std::wstring CDirStatDoc::GetHighlightExtension()
{
    return m_HighlightExtension;
}

// The very root has been deleted.
//
void CDirStatDoc::UnlinkRoot()
{
    CMainFrame::Get()->InvokeInMessageThread([this]
    {
        DeleteContents();
        UpdateAllViews(nullptr, HINT_NEWROOT);
    });
}

// Determines, whether an UDC works for a given item.
//
bool CDirStatDoc::UserDefinedCleanupWorksForItem(USERDEFINEDCLEANUP* udc, const CItem* item)
{
    return item != nullptr && (
        (item->IsType(IT_DRIVE) && udc->WorksForDrives) ||
        (item->IsType(IT_DIRECTORY) && udc->WorksForDirectories) ||
        (item->IsType(IT_FILE) && udc->WorksForFiles) ||
        (item->HasUncPath() && udc->WorksForUncPaths));
}

void CDirStatDoc::OpenItem(const CItem* item, const std::wstring & verb)
{
    ASSERT(item != nullptr);

    // determine path to feed into shell function
    SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree, nullptr);
    if (item->IsType(IT_MYCOMPUTER))
    {
        (void) SHGetSpecialFolderLocation(nullptr, CSIDL_DRIVES, &pidl);
    }
    else
    {
        pidl = ILCreateFromPath(item->GetPath().c_str());
    }

    // launch properties dialog
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

void CDirStatDoc::RecurseRefreshReparsePoints(CItem* item)
{
    std::vector<CItem*> toRefresh;

    if (item == nullptr) return;
    std::stack<CItem*> reparseStack({item});
    while (!reparseStack.empty())
    {
        const auto& qitem = reparseStack.top();
        reparseStack.pop();

        if (!item->IsType(IT_DIRECTORY | IT_DRIVE)) continue;
        for (const auto& child : qitem->GetChildren())
        {
            if (!child->IsType(IT_DIRECTORY | IT_DRIVE | ITF_ROOTITEM))
            {
                continue;
            }

            if (CReparsePoints::IsReparsePoint(child->GetAttributes()) &&
                CDirStatApp::Get()->IsFollowingAllowed(child->GetPathLong(), child->GetAttributes()))
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


    if (root->IsType(IT_MYCOMPUTER))
    {
        for (const auto& child : root->GetChildren())
        {
            ASSERT(child->IsType(IT_DRIVE));
            drives.push_back(child);
        }
    }
    else if (root->IsType(IT_DRIVE))
    {
        drives.push_back(root);
    }

    return drives;
}

void CDirStatDoc::RefreshRecyclers() const
{
    std::vector<CItem*> toRefresh;
    for (const auto & drive : GetDriveItems())
    {
        if (CItem* recycler = drive->FindRecyclerItem(); recycler != nullptr)
        {
            toRefresh.push_back(recycler);
        }
    }

    if (!toRefresh.empty()) GetDocument()->StartScanningEngine(toRefresh);
}

void CDirStatDoc::RebuildExtensionData()
{
    CWaitCursor wc;

    m_ExtensionData.clear();
    if (IsRootDone())
    {
        m_RootItem->CollectExtensionData(&m_ExtensionData);
    }
    
    std::vector<std::wstring> sortedExtensions;
    SortExtensionData(sortedExtensions);
    SetExtensionColors(sortedExtensions);

    m_ExtensionDataValid = true;
}

void CDirStatDoc::SortExtensionData(std::vector<std::wstring>& sortedExtensions)
{
    sortedExtensions.resize(m_ExtensionData.size());

    for (int i = 0; const auto& ext : m_ExtensionData | std::views::keys)
    {
        sortedExtensions[i++] = ext;
    }

    _pqsortExtensionData = &m_ExtensionData;
    qsort(sortedExtensions.data(), sortedExtensions.size(), sizeof(std::wstring), [](const LPCVOID item1, const LPCVOID item2)
    {
        const std::wstring &ext1 = *static_cast<const std::wstring*>(item1);
        const std::wstring &ext2 = *static_cast<const std::wstring*>(item2);
        const auto& r1 = _pqsortExtensionData->find(ext1);
        const auto& r2 = _pqsortExtensionData->find(ext2);
        ASSERT(r1 != _pqsortExtensionData->end());
        ASSERT(r2 != _pqsortExtensionData->end());
        return usignum(r2->second.bytes, r1->second.bytes);
    });
    _pqsortExtensionData = nullptr;
}

void CDirStatDoc::SetExtensionColors(const std::vector<std::wstring>& sortedExtensions)
{
    static std::vector<COLORREF> colors;

    if (colors.empty())
    {
        CTreeMap::GetDefaultPalette(colors);
    }

    for (std::size_t i = 0; i < sortedExtensions.size(); i++)
    {
        COLORREF c = colors[colors.size() - 1];
        if (i < colors.size())
        {
            c = colors[i];
        }
        m_ExtensionData[sortedExtensions[i]].color = c;
    }
}

CExtensionData* CDirStatDoc::_pqsortExtensionData;

// Deletes a file or directory via SHFileOperation.
// Return: false, if canceled
//
bool CDirStatDoc::DeletePhysicalItems(const std::vector<CItem*>& items, const bool toTrashBin)
{
    if (COptions::ShowDeleteWarning)
    {
        CDeleteWarningDlg warning(items);
        if (IDYES != warning.DoModal())
        {
            return false;
        }
        COptions::ShowDeleteWarning = !warning.m_DontShowAgain;
    }

    // Fetch the parent item of the current focus / selected item so we can reselect
    CTreeListItem* reselect = nullptr;
    if (const int mark = CFileTreeControl::Get()->GetSelectionMark(); FileTreeHasFocus() && mark != -1)
        reselect = CFileTreeControl::Get()->GetItem(mark)->GetParent();

    CModalShellApi msa;
    for (const auto& item : items)
    {
        msa.DeleteFile(item->GetPath(), toTrashBin);
    }

    RefreshItem(items);

    // Attempt to reselect the item
    if (reselect != nullptr)
        CFileTreeControl::Get()->SelectItem(reselect, true, true);

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
void CDirStatDoc::RefreshItem(const std::vector<CItem*>& item)
{
    GetDocument()->StartScanningEngine(item);
}

// UDC confirmation Dialog.
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
    if (IDYES != AfxMessageBox(msg.c_str(), MB_YESNO))
    {
        AfxThrowUserException();
    }
}

void CDirStatDoc::PerformUserDefinedCleanup(USERDEFINEDCLEANUP* udc, const CItem* item)
{
    CWaitCursor wc;

    const std::wstring path = item->GetPath();

    // Verify that path still exists
    if (item->IsType(IT_DIRECTORY | IT_DRIVE))
    {
        if (!FolderExists(path) && !DriveExists(path))
        {
            MdThrowStringException(Localization::Format(IDS_THEFILEsDOESNOTEXIST, path));
        }
    }
    else
    {
        ASSERT(item->IsType(IT_FILE));

        if (!::PathFileExists(path.c_str()))
        {
            MdThrowStringException(Localization::Format(IDS_THEFILEsDOESNOTEXIST, path));
        }
    }

    if (udc->RecurseIntoSubdirectories)
    {
        ASSERT(item->IsType(IT_DRIVE | IT_DIRECTORY));

        RecursiveUserDefinedCleanup(udc, path, path);
    }
    else
    {
        CallUserDefinedCleanup(item->IsType(IT_DIRECTORY | IT_DRIVE), udc->CommandLine.Obj(), path, path, udc->ShowConsoleWindow, udc->WaitForCompletion);
    }
}

void CDirStatDoc::RefreshAfterUserDefinedCleanup(const USERDEFINEDCLEANUP* udc, CItem* item)
{
    switch (static_cast<REFRESHPOLICY>(udc->RefreshPolicy.Obj()))
    {
    case RP_NO_REFRESH:
        break;

    case RP_REFRESH_THIS_ENTRY:
        {
            RefreshItem(item);
        }
        break;

    case RP_REFRESH_THIS_ENTRYS_PARENT:
        {
            RefreshItem(nullptr == item->GetParent() ? item : item->GetParent());
        }
        break;

    // case RP_ASSUME_ENTRY_HAS_BEEN_DELETED:
    // Feature not implemented.
    // break;

    default:
        ASSERT(FALSE);
    }
}

void CDirStatDoc::RecursiveUserDefinedCleanup(USERDEFINEDCLEANUP* udc, const std::wstring& rootPath, const std::wstring& currentPath)
{
    // (Depth first.)

    FileFindEnhanced finder;
    for (BOOL b = finder.FindFile(currentPath); b; b = finder.FindNextFile())
    {
        if (finder.IsDots() || !finder.IsDirectory())
        {
            continue;
        }
        if (!CDirStatApp::Get()->IsFollowingAllowed(finder.GetFilePathLong(), finder.GetAttributes()))
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

    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = showConsoleWindow ? SW_SHOWNORMAL : SW_HIDE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcess(app.c_str(), cmdline.data(), nullptr,
        nullptr, false, 0, nullptr,
        directory.c_str(), &si, &pi))
    {
        MdThrowStringException(Localization::Format(IDS_COULDNOTCREATEPROCESSssss,
            app, cmdline, directory, MdGetWinErrorText(static_cast<HRESULT>(::GetLastError()))));
        return;
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

std::vector<CItem*> CDirStatDoc::GetAllSelected()
{
    return DupeListHasFocus() ? CFileDupeControl::Get()->GetAllSelected<CItem>() :
        CFileTreeControl::Get()->GetAllSelected<CItem>();
}

void CDirStatDoc::OnUpdateCentralHandler(CCmdUI* pCmdUI)
{
    struct commandFilter
    {
        bool allowNone = false;       // allow display when nothing is selected
        bool allowMany = false;       // allow display when multiple items are selected
        bool allowEarly = false;      // allow display before processing is finished
        bool treeFocus = false;       // only display in tree view
        ITEMTYPE typesAllow = IT_ANY; // only display if these types are allowed
        bool (*extra)(CItem*) = [](CItem*) { return true; }; // extra checks
    };

    // special conditions
    static auto doc = this;
    static bool (*canZoomOut)(CItem*) = [](CItem*) { return doc->GetZoomItem() != doc->GetRootItem(); };
    static bool (*parentNotNull)(CItem*) = [](CItem* item) { return item != nullptr && item->GetParent() != nullptr; };
    static bool (*reslectAvail)(CItem*) = [](CItem*) { return doc->IsReselectChildAvailable(); };
    static bool (*notRoot)(CItem*) = [](CItem* item) { return item != nullptr && !item->IsRootItem(); };
    static bool (*isResumable)(CItem*) = [](CItem*) { return CMainFrame::Get()->IsScanSuspended(); };
    static bool (*isSuspendable)(CItem*) = [](CItem*) { return doc->HasRootItem() && !doc->IsRootDone() && !CMainFrame::Get()->IsScanSuspended(); };
    static bool (*isStoppable)(CItem*) = [](CItem*) { return doc->HasRootItem() && !doc->IsRootDone(); };

    static std::unordered_map<UINT, const commandFilter> filters
    {
        // ID                           none   many   early  focus  types
        { ID_REFRESH_ALL,             { true,  true,  false, false, IT_ANY} },
        { ID_REFRESH_SELECTED,        { false, true,  false, false, IT_MYCOMPUTER | IT_DRIVE | IT_DIRECTORY | IT_FILE } },
        { ID_SAVE_RESULTS,            { true,  true,  false, false, IT_ANY} },
        { ID_EDIT_COPY_CLIPBOARD,     { false, true,  true,  false, IT_DRIVE | IT_DIRECTORY | IT_FILE } },
        { ID_CLEANUP_EMPTY_BIN,       { true,  true,  false, false, IT_ANY} },
        { ID_TREEMAP_RESELECT_CHILD,  { true,  true,  true,  false, IT_ANY, reslectAvail } },
        { ID_TREEMAP_SELECT_PARENT,   { false, false, true,  false, IT_ANY, parentNotNull } },
        { ID_TREEMAP_ZOOMIN,          { false, false, false, false, IT_DRIVE | IT_DIRECTORY} },
        { ID_TREEMAP_ZOOMOUT,         { false, false, false, false, IT_DIRECTORY, canZoomOut } },
        { ID_CLEANUP_EXPLORER_SELECT, { false, true,  true,  false, IT_DIRECTORY | IT_FILE } },
        { ID_CLEANUP_OPEN_IN_CONSOLE, { false, true,  true,  false, IT_DRIVE | IT_DIRECTORY | IT_FILE } },
        { ID_COMPRESS_NONE,           { false, true,  false, false, IT_FILE } },
        { ID_COMPRESS_LZNT1,          { false, true,  false, false, IT_FILE } },
        { ID_COMPRESS_XPRESS4K,       { false, true,  false, false, IT_FILE } },
        { ID_COMPRESS_XPRESS8K,       { false, true,  false, false, IT_FILE } },
        { ID_COMPRESS_XPRESS16K,      { false, true,  false, false, IT_FILE } },
        { ID_COMPRESS_LZX,            { false, true,  false, false, IT_FILE } },
        { ID_SCAN_RESUME,             { true,  true,  true,  false, IT_ANY, isResumable } },
        { ID_SCAN_SUSPEND,            { true,  true,  true,  false, IT_ANY, isSuspendable } },
        { ID_SCAN_STOP,               { true,  true,  true,  false, IT_ANY, isStoppable } },
        { ID_CLEANUP_DELETE_BIN,      { false, true,  false,  true, IT_DIRECTORY | IT_FILE, notRoot } },
        { ID_CLEANUP_DELETE,          { false, true,  false,  true, IT_DIRECTORY | IT_FILE, notRoot } },
        { ID_CLEANUP_OPEN_SELECTED,   { false, true,  true,  false, IT_MYCOMPUTER | IT_DRIVE | IT_DIRECTORY | IT_FILE } },
        { ID_CLEANUP_PROPERTIES,      { false, true,  true,  false, IT_MYCOMPUTER | IT_DRIVE | IT_DIRECTORY | IT_FILE } }
    };

    if (!filters.contains(pCmdUI->m_nID))
    {
        ASSERT(FALSE);
        return;
    }

    const auto& filter = filters[pCmdUI->m_nID];
    const auto& items = GetAllSelected();

    bool allow = true;
    allow &= !filter.treeFocus || FileTreeHasFocus() || DupeListHasFocus();
    allow &= filter.allowNone || !items.empty();
    allow &= filter.allowMany || items.size() <= 1;
    allow &= filter.allowEarly || IsRootDone();
    if (items.empty()) allow &= filter.extra(nullptr);
    for (const auto& item : items)
    {
        allow &= filter.extra(item);
        allow &= item->IsType(filter.typesAllow);
    }

    pCmdUI->Enable(allow); 
}

#define ON_COMMAMD_UPDATE_WRAPPER(x,y) ON_COMMAND(x, y) ON_UPDATE_COMMAND_UI(x, OnUpdateCentralHandler)
BEGIN_MESSAGE_MAP(CDirStatDoc, CDocument) 
    ON_COMMAMD_UPDATE_WRAPPER(ID_REFRESH_SELECTED, OnRefreshSelected)
    ON_COMMAMD_UPDATE_WRAPPER(ID_REFRESH_ALL, OnRefreshAll)
    ON_COMMAND(ID_LOAD_RESULTS, OnLoadResults)
    ON_COMMAMD_UPDATE_WRAPPER(ID_SAVE_RESULTS, OnSaveResults)
    ON_COMMAMD_UPDATE_WRAPPER(ID_EDIT_COPY_CLIPBOARD, OnEditCopy)
    ON_COMMAMD_UPDATE_WRAPPER(ID_CLEANUP_EMPTY_BIN, OnCleanupEmptyRecycleBin)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWFREESPACE, OnUpdateViewShowFreeSpace)
    ON_COMMAND(ID_VIEW_SHOWFREESPACE, OnViewShowFreeSpace)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWUNKNOWN, OnUpdateViewShowUnknown)
    ON_COMMAND(ID_VIEW_SHOWUNKNOWN, OnViewShowUnknown)
    ON_COMMAMD_UPDATE_WRAPPER(ID_TREEMAP_ZOOMIN, OnTreeMapZoomIn)
    ON_COMMAMD_UPDATE_WRAPPER(ID_TREEMAP_ZOOMOUT, OnTreeMapZoomOut)
    ON_COMMAMD_UPDATE_WRAPPER(ID_CLEANUP_EXPLORER_SELECT, OnExplorerSelect)
    ON_COMMAMD_UPDATE_WRAPPER(ID_CLEANUP_OPEN_IN_CONSOLE, OnCommandPromptHere)
    ON_COMMAMD_UPDATE_WRAPPER(ID_CLEANUP_DELETE_BIN, OnCleanupDeleteToBin)
    ON_COMMAMD_UPDATE_WRAPPER(ID_CLEANUP_DELETE, OnCleanupDelete)
    ON_UPDATE_COMMAND_UI_RANGE(ID_USERDEFINEDCLEANUP0, ID_USERDEFINEDCLEANUP9, OnUpdateUserDefinedCleanup)
    ON_COMMAND_RANGE(ID_USERDEFINEDCLEANUP0, ID_USERDEFINEDCLEANUP9, OnUserDefinedCleanup)
    ON_COMMAMD_UPDATE_WRAPPER(ID_TREEMAP_SELECT_PARENT, OnTreeMapSelectParent)
    ON_COMMAMD_UPDATE_WRAPPER(ID_TREEMAP_RESELECT_CHILD, OnTreeMapReselectChild)
    ON_COMMAMD_UPDATE_WRAPPER(ID_CLEANUP_OPEN_SELECTED, OnCleanupOpenTarget)
    ON_COMMAMD_UPDATE_WRAPPER(ID_CLEANUP_PROPERTIES, OnCleanupProperties)
    ON_UPDATE_COMMAND_UI_RANGE(ID_COMPRESS_NONE, ID_COMPRESS_LZX, OnUpdateCentralHandler)
    ON_COMMAND_RANGE(ID_COMPRESS_NONE, ID_COMPRESS_LZX, OnCleanupCompress)
    ON_COMMAMD_UPDATE_WRAPPER(ID_SCAN_RESUME, OnScanResume)
    ON_COMMAMD_UPDATE_WRAPPER(ID_SCAN_SUSPEND, OnScanSuspend)
    ON_COMMAMD_UPDATE_WRAPPER(ID_SCAN_STOP, OnScanStop)
    ON_COMMAND_RANGE(CONTENT_MENU_MINCMD, CONTENT_MENU_MAXCMD, OnContextMenuExplore)
END_MESSAGE_MAP()

void CDirStatDoc::OnRefreshSelected()
{
    RefreshItem(GetAllSelected());
}

void CDirStatDoc::OnRefreshAll()
{
    RefreshItem(GetRootItem());
}

void CDirStatDoc::OnSaveResults()
{
    // Request the file path from the user
    std::wstring fileSelectString = std::format(L"{} (*.csv)|*.csv|{} (*.*)|*.*||",
        Localization::Lookup(IDS_CSV_FILES), Localization::Lookup(IDS_ALL_FILES));
    CFileDialog dlg(FALSE, L"csv", nullptr, OFN_EXPLORER | OFN_DONTADDTORECENT, fileSelectString.c_str());
    if (dlg.DoModal() != IDOK) return;

    CWaitCursor wc;
    SaveResults(dlg.GetPathName().GetString(), GetRootItem());
}

void CDirStatDoc::OnLoadResults()
{
    // Request the file path from the user
    std::wstring fileSelectString = std::format(L"{} (*.csv)|*.csv|{} (*.*)|*.*||",
        Localization::Lookup(IDS_CSV_FILES), Localization::Lookup(IDS_ALL_FILES));
    CFileDialog dlg(TRUE, L"csv", nullptr, OFN_EXPLORER | OFN_DONTADDTORECENT | OFN_PATHMUSTEXIST, fileSelectString.c_str());
    if (dlg.DoModal() != IDOK) return;

    CWaitCursor wc;
    CItem* newroot = LoadResults(dlg.GetPathName().GetString());
    GetDocument()->OnOpenDocument(newroot);
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
    CModalShellApi msa;

    SHEmptyRecycleBin(*AfxGetMainWnd(), nullptr, 0);

    RefreshRecyclers();
    UpdateAllViews(nullptr);
}

void CDirStatDoc::OnUpdateViewShowFreeSpace(CCmdUI* pCmdUI)
{
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

    // Toogle value
    m_ShowFreeSpace = !m_ShowFreeSpace;
    COptions::ShowFreeSpace = m_ShowFreeSpace;

    // Force recalculation and graph refresh
    StartScanningEngine({});
}

void CDirStatDoc::OnUpdateViewShowUnknown(CCmdUI* pCmdUI)
{
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

    // Toogle value
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
    try
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
            ShellExecuteThrow(*AfxGetMainWnd(), L"open", cmd, path, SW_SHOWNORMAL);
        }
    }
    catch (CException* pe)
    {
        pe->ReportError();
        pe->Delete();
    }
}

void CDirStatDoc::OnCleanupDeleteToBin()
{
    const auto & items = GetAllSelected();
    if (DeletePhysicalItems(items, true))
    {
        RefreshRecyclers();
        UpdateAllViews(nullptr);
    }
}

void CDirStatDoc::OnCleanupDelete()
{
    const auto & items = GetAllSelected();
    if (DeletePhysicalItems(items, false))
    {
        UpdateAllViews(nullptr);
    }
}

void CDirStatDoc::OnUpdateUserDefinedCleanup(CCmdUI* pCmdUI)
{
    const int i = pCmdUI->m_nID - ID_USERDEFINEDCLEANUP0;
    const auto & items = GetAllSelected();
    bool allowControl = (FileTreeHasFocus() || DupeListHasFocus()) && COptions::UserDefinedCleanups.at(i).Enabled && !items.empty();
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
            RefreshAfterUserDefinedCleanup(udc, item);
        }
        catch (CUserException* pe)
        {
            pe->Delete();
        }
        catch (CException* pe)
        {
            pe->ReportError();
            pe->Delete();
        }
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

void CDirStatDoc::OnCleanupCompress(UINT id)
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

    CWaitCursor wc;
    const auto& items = GetAllSelected();
    for (const auto& item : items)
    {
        CompressFile(item->GetPath(), compressionMap.at(id));
        item->UpdateStatsFromDisk();
        UpdateAllViews(nullptr);
    }
}

void CDirStatDoc::OnScanSuspend()
{
    // Wait for system to fully shutdown
    queue.SuspendExecution();

    // Mark as suspended
    if (CMainFrame::Get() != nullptr)
        CMainFrame::Get()->SuspendState(true);
}

void CDirStatDoc::OnScanResume()
{
    queue.ResumeExecution();

    if (CMainFrame::Get() != nullptr)
        CMainFrame::Get()->SuspendState(false);
}

void CDirStatDoc::OnScanStop()
{
    queue.CancelExecution();
    OnScanResume();
}

void CDirStatDoc::StopScanningEngine()
{
    OnScanStop();
}

void CDirStatDoc::OnContextMenuExplore(UINT nID)
{
    // get list of paths from items
    std::vector<std::wstring> paths;
    for (auto& item : CMainFrame::Get()->GetAllSelectedInFocus())
        paths.push_back(item->GetPath());

    // query current context menu
    if (paths.empty()) return;
    CComPtr<IContextMenu> contextMenu = GetContextMenu(CMainFrame::Get()->GetSafeHwnd(), paths);

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

    // Address currently zoomed / selected item conflicts
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
    std::thread([this,items] () mutable
    {
        // Wait for other threads to finish if this was scheduled in parallel
        static std::shared_mutex mutex;
        std::lock_guard lock(mutex);

        const auto selectedItems = GetAllSelected();
        using VisualInfo = struct { bool wasExpanded; bool isSelected; int oldScrollPosition; };
        std::unordered_map<CItem *,VisualInfo> visualInfo;
        for (auto item : std::vector(items))
        {
            // Clear items from duplicate list;
            CFileDupeControl::Get()->RemoveItem(item);

            // Record current visual arrangement to reapply afterward
            if (item->IsVisible())
            {
                visualInfo[item].isSelected = std::ranges::find(selectedItems, item) != selectedItems.end();
                visualInfo[item].wasExpanded = item->IsExpanded();
            }

            // Skip pruning if it is a new element
            if (!item->IsDone()) continue;
            item->UpwardRecalcLastChange(true);
            item->UpwardSubtractSizePhysical(item->GetSizePhysical());
            item->UpwardSubtractSizeLogical(item->GetSizeLogical());
            item->UpwardSubtractFiles(item->GetFilesCount());
            item->UpwardSubtractFolders(item->GetFoldersCount());
            item->RemoveAllChildren();
            item->UpwardSetUndone();

            // children removal will collapse item so re-expand it
            if (visualInfo.contains(item) && item->IsVisible())
                item->SetExpanded(visualInfo[item].wasExpanded);
  
            // Handle if item to be refreshed has been removed
            if (item->IsType(IT_FILE | IT_DIRECTORY | IT_DRIVE) &&
                !FileFindEnhanced::DoesFileExist(item->GetFolderPath(),
                    item->IsType(IT_FILE) ? item->GetName() : std::wstring()))
            {
                // Remove item from list so we do not rescan it
                std::erase(items, item);

                if (item->IsRootItem())
                {
                    // Handle deleted root item
                    CMainFrame::Get()->InvokeInMessageThread([]
                    {
                        GetDocument()->UnlinkRoot();
                        CMainFrame::Get()->MinimizeTreeMapView();
                        CMainFrame::Get()->MinimizeExtensionView();
                    });
                    return;
                }

                // Handle non-root item by removing from parent
                item->UpwardSubtractFiles(item->IsType(IT_FILE) ? 1 : 0);
                item->UpwardSubtractFolders(item->IsType(IT_FILE) ? 0 : 1);
                item->GetParent()->RemoveChild(item);
            }
        }

        // Add items to processing queue
        for (const auto item : std::vector(items))
        {
            // Skip any items we should not follow
            if (!item->IsType(ITF_ROOTITEM) && !CDirStatApp::Get()->IsFollowingAllowed(item->GetPath(), item->GetAttributes()))
            {
                continue;
            }

            item->UpwardAddReadJobs(1);
            item->UpwardSetUndone();
            queue.Push(item);
        }

        // Create subordinate threads if there is work to do
        if (queue.HasItems())
        {
            queue.StartThreads(COptions::ScanningThreads, [this]()
            {
                CItem::ScanItems(&queue);
            });

            // Wait for all threads to run out of work
            if (!queue.WaitForCompletionOrCancellation())
            {
                // Sorting and other finalization tasks
                CItem::ScanItemsFinalize(GetRootItem());

                // Exit here and stop progress if drained by an outside actor
                CMainFrame::Get()->InvokeInMessageThread([]
                {
                    CMainFrame::Get()->SetProgressComplete();
                    CMainFrame::Get()->MinimizeTreeMapView();
                    CMainFrame::Get()->MinimizeExtensionView();
                });
                return;
            }
        }

        // Restore unknown and freespace items
        for (const auto& item : items)
        {
            if (!item->IsType(IT_DRIVE)) continue;
            
            if (COptions::ShowFreeSpace)
            {
                item->CreateFreeSpaceItem();
            }
            if (COptions::ShowUnknown)
            {
                item->CreateUnknownItem();
            }
        }

        // Sorting and other finalization tasks
        CItem::ScanItemsFinalize(GetRootItem());

        // Invoke a UI thread to do updates
        CMainFrame::Get()->InvokeInMessageThread([&items,&visualInfo]
        {
            for (const auto& item : items)
            {
                // restore scroll position if previously set
                if (visualInfo.contains(item) && item->IsVisible())
                    item->SetScrollPosition(visualInfo[item].oldScrollPosition);
            }

            CMainFrame::Get()->LockWindowUpdate();
            GetDocument()->RebuildExtensionData();
            GetDocument()->UpdateAllViews(nullptr);
            CMainFrame::Get()->SetProgressComplete();
            CMainFrame::Get()->RestoreExtensionView();
            CMainFrame::Get()->RestoreTreeMapView();
            CMainFrame::Get()->GetTreeMapView()->SuspendRecalculationDrawing(false);
            CMainFrame::Get()-> UnlockWindowUpdate();
        });
    }).detach();
}
