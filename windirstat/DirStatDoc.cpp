// dirstatdoc.cpp - Implementation of CDirStatDoc
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
#include "Item.h"
#include "MainFrame.h"
#include "GlobalHelpers.h"
#include "deletewarningdlg.h"
#include "ModalShellApi.h"
#include <common/MdExceptions.h>
#include <common/SmartPointer.h>
#include <common/CommonHelpers.h>
#include "DirStatDoc.h"

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <unordered_set>

#include "graphview.h"

CDirStatDoc* _theDocument;

CDirStatDoc* GetDocument()
{
    return _theDocument;
}

IMPLEMENT_DYNCREATE(CDirStatDoc, CDocument)

CDirStatDoc::CDirStatDoc()
    : m_showFreeSpace(CPersistence::GetShowFreeSpace())
      , m_showUnknown(CPersistence::GetShowUnknown())
      , m_showMyComputer(false)
      , m_rootItem(nullptr)
      , m_zoomItem(nullptr)
      , m_workingItem(nullptr)
      , m_extensionDataValid(false)
{
    ASSERT(NULL == _theDocument);
    _theDocument = this;

    VTRACE(L"sizeof(CItem) = %d", sizeof(CItem));
}

CDirStatDoc::~CDirStatDoc()
{
    CPersistence::SetShowFreeSpace(m_showFreeSpace);
    CPersistence::SetShowUnknown(m_showUnknown);

    delete m_rootItem;
    _theDocument = nullptr;
}

// Encodes a selection from the CSelectDrivesDlg into a string which can be routed as a pseudo
// document "path" through MFC and finally arrives in OnOpenDocument().
//
CStringW CDirStatDoc::EncodeSelection(RADIO radio, const CStringW& folder, const CStringArray& drives)
{
    CStringW ret;
    switch (radio)
    {
    case RADIO_ALLLOCALDRIVES:
    case RADIO_SOMEDRIVES:
        {
            for (int i = 0; i < drives.GetSize(); i++)
            {
                if (i > 0)
                {
                    ret += CStringW(GetEncodingSeparator());
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
void CDirStatDoc::DecodeSelection(const CStringW& s, CStringW& folder, CStringArray& drives)
{
    folder.Empty();
    drives.RemoveAll();

    // s is either something like "C:\programme"
    // or something like "C:|D:|E:".

    CStringArray sa;
    int i = 0;

    while (i < s.GetLength())
    {
        CStringW token;
        while (i < s.GetLength() && s[i] != GetEncodingSeparator())
        {
            token += s[i++];
        }

        token.TrimLeft();
        token.TrimRight();
        ASSERT(!token.IsEmpty());
        sa.Add(token);

        if (i < s.GetLength())
        {
            i++;
        }
    }

    ASSERT(sa.GetSize() > 0);

    if (sa.GetSize() > 1)
    {
        for (int j = 0; j < sa.GetSize(); j++)
        {
            CStringW d = sa[j];
            ASSERT(2 == d.GetLength());
            ASSERT(wds::chrColon == d[1]);

            drives.Add(d + L"\\");
        }
    }
    else
    {
        CStringW f = sa[0];
        if (2 == f.GetLength() && wds::chrColon == f[1])
        {
            drives.Add(f + L"\\");
        }
        else
        {
            // Remove trailing backslash, if any and not drive-root.
            if (f.GetLength() > 0 && wds::strBackslash == f.Right(1) && (f.GetLength() != 3 || f[1] != wds::chrColon))
            {
                f = f.Left(f.GetLength() - 1);
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
    delete m_rootItem;
    m_rootItem = nullptr;
    SetWorkingItem(nullptr);
    m_zoomItem = nullptr;
    GetWDSApp()->ReReadMountPoints();
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
    CDocument::OnNewDocument(); // --> DeleteContents()

    const CStringW spec = lpszPathName;
    CStringW folder;
    CStringArray drives;
    DecodeSelection(spec, folder, drives);

    CStringArray rootFolders;
    if (drives.GetSize() > 0)
    {
        m_showMyComputer = drives.GetSize() > 1;
        for (int i = 0; i < drives.GetSize(); i++)
        {
            rootFolders.Add(drives[i]);
        }
    }
    else
    {
        ASSERT(!folder.IsEmpty());
        m_showMyComputer = false;
        rootFolders.Add(folder);
    }

    CArray<CItem*, CItem*> driveItems;

    if (m_showMyComputer)
    {
        m_rootItem = new CItem(static_cast<ITEMTYPE>(IT_MYCOMPUTER | ITF_ROOTITEM), LoadString(IDS_MYCOMPUTER));
        for (int i = 0; i < rootFolders.GetSize(); i++)
        {
            const auto drive = new CItem(IT_DRIVE, rootFolders[i]);
            driveItems.Add(drive);
            m_rootItem->AddChild(drive);
        }
    }
    else
    {
        const ITEMTYPE type = IsDrive(rootFolders[0]) ? IT_DRIVE : IT_DIRECTORY;
        m_rootItem          = new CItem(static_cast<ITEMTYPE>(type | ITF_ROOTITEM), rootFolders[0], false);
        if (m_rootItem->IsType(IT_DRIVE))
        {
            driveItems.Add(m_rootItem);
        }
        m_rootItem->UpdateLastChange();
    }
    m_zoomItem = m_rootItem;

    for (int i = 0; i < driveItems.GetSize(); i++)
    {
        if (OptionShowFreeSpace())
        {
            driveItems[i]->CreateFreeSpaceItem();
        }
        if (OptionShowUnknown())
        {
            driveItems[i]->CreateUnknownItem();
        }
    }

    SetWorkingItem(m_rootItem);

    GetMainFrame()->MinimizeGraphView();
    GetMainFrame()->MinimizeTypeView();

    UpdateAllViews(nullptr, HINT_NEWROOT);
    return true;
}

// We don't want MFCs AfxFullPath()-Logic, because lpszPathName
// is not a path. So we have overridden this.
//
void CDirStatDoc::SetPathName(LPCWSTR lpszPathName, BOOL /*bAddToMRU*/)
{
    // MRU would be fine but is not implemented yet.

    m_strPathName = lpszPathName;
    ASSERT(!m_strPathName.IsEmpty()); // must be set to something
    m_bEmbedded = FALSE;
    SetTitle(lpszPathName);

    ASSERT_VALID(this);
}

void CDirStatDoc::Serialize(CArchive& /*ar*/)
{
}

// Prefix the window title (with percentage or "Scanning")
//
void CDirStatDoc::SetTitlePrefix(const CStringW& prefix) const
{
    static CStringW suffix = IsAdmin() ? L" (Administrator)" : L"";
    const CStringW docName = prefix + GetTitle() + suffix;
    GetMainFrame()->UpdateFrameTitleForDocument(docName);
}

COLORREF CDirStatDoc::GetCushionColor(LPCWSTR ext)
{
    SExtensionRecord r;
    VERIFY(GetExtensionData()->Lookup(ext, r));
    return r.color;
}

COLORREF CDirStatDoc::GetZoomColor()
{
    return RGB(0, 0, 255);
}

bool CDirStatDoc::OptionShowFreeSpace() const
{
    return m_showFreeSpace;
}

bool CDirStatDoc::OptionShowUnknown() const
{
    return m_showUnknown;
}

const CExtensionData* CDirStatDoc::GetExtensionData()
{
    if (!m_extensionDataValid)
    {
        RebuildExtensionData();
    }
    return &m_extensionData;
}

ULONGLONG CDirStatDoc::GetRootSize() const
{
    ASSERT(m_rootItem != NULL);
    ASSERT(IsRootDone());
    return m_rootItem->GetSize();
}

// This method does some work for ticks ms.
// return: true if done or suspended.
//
bool CDirStatDoc::Work(CWorkLimiter* limiter)
{
    if (nullptr == m_rootItem)
    {
        return true;
    }

    if (GetMainFrame()->IsScanSuspended())
    {
        return true;
    }

    if (!m_rootItem->IsDone())
    {
        m_rootItem->DoSomeWork(limiter);
        if (m_rootItem->IsDone())
        {
            m_extensionDataValid = false;

            GetMainFrame()->SetProgressPos100();
            GetMainFrame()->RestoreTypeView();
            GetMainFrame()->RestoreGraphView();

            UpdateAllViews(nullptr);
        }
        else
        {
            ASSERT(m_workingItem != NULL);
            if (m_workingItem != nullptr) // to be honest, "defensive programming" is stupid, but c'est la vie: it's safer.
            {
                GetMainFrame()->SetProgressPos(m_workingItem->GetProgressPos());
            }

            UpdateAllViews(nullptr, HINT_SOMEWORKDONE);
        }
    }
    if (m_rootItem->IsDone())
    {
        SetWorkingItem(nullptr);
        return true;
    }
    else
    {
        return false;
    }
}

bool CDirStatDoc::IsDrive(const CStringW& spec)
{
    return 3 == spec.GetLength() && wds::chrColon == spec[1] && wds::chrBackslash == spec[2];
}

// Starts a refresh of all mount points in our tree.
// Called when the user changes the follow mount points option.
//
void CDirStatDoc::RefreshMountPointItems()
{
    CWaitCursor wc;

    CItem* root = GetRootItem();
    if (nullptr == root)
    {
        return;
    }

    RecurseRefreshMountPointItems(root);
}

// Starts a refresh of all junction points in our tree.
// Called when the user changes the ignore junction points option.
//
void CDirStatDoc::RefreshJunctionItems()
{
    CWaitCursor wc;

    CItem* root = GetRootItem();
    if (nullptr == root)
    {
        return;
    }

    RecurseRefreshJunctionItems(root);
}

bool CDirStatDoc::IsRootDone() const
{
    return m_rootItem != nullptr && m_rootItem->IsDone();
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

void CDirStatDoc::SetHighlightExtension(LPCWSTR ext)
{
    m_highlightExtension = ext;
    GetMainFrame()->SetSelectionMessageText();
}

CStringW CDirStatDoc::GetHighlightExtension()
{
    return m_highlightExtension;
}

// The very root has been deleted.
//
void CDirStatDoc::UnlinkRoot()
{
    DeleteContents();
    UpdateAllViews(nullptr, HINT_NEWROOT);
}

// Determines, whether an UDC works for a given item.
//
bool CDirStatDoc::UserDefinedCleanupWorksForItem(const USERDEFINEDCLEANUP* udc, const CItem* item)
{
    bool works = false;

    if (item != nullptr)
    {
        if (!udc->worksForUncPaths && item->HasUncPath())
        {
            return false;
        }

        switch (item->GetType())
        {
        case IT_DRIVE:
            {
                works = udc->worksForDrives;
            }
            break;

        case IT_DIRECTORY:
            {
                works = udc->worksForDirectories;
            }
            break;

        case IT_FILE:
            {
                works = udc->worksForFiles;
            }
            break;
        }
    }

    return works;
}

ULONGLONG CDirStatDoc::GetWorkingItemReadJobs()
{
    if (m_workingItem != nullptr)
    {
        return m_workingItem->GetReadJobs();
    }
    else
    {
        return 0;
    }
}

void CDirStatDoc::OpenItem(const CItem* item, LPCWSTR verb)
{
    ASSERT(item != NULL);

    // determine path to feed into shell function
    SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree, nullptr);
    if (item->IsType(IT_MYCOMPUTER))
    {
        (void) SHGetSpecialFolderLocation(nullptr, CSIDL_DRIVES, &pidl);
    }
    else
    {
        pidl = ILCreateFromPath(item->GetPath());
    }

    // launch properties dialog
    SHELLEXECUTEINFO sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.hwnd = *AfxGetMainWnd();
    sei.lpVerb = verb;
    sei.fMask = SEE_MASK_INVOKEIDLIST | SEE_MASK_IDLIST;
    sei.lpIDList = pidl;
    sei.nShow = sei.nShow = SW_SHOWNORMAL;
    ShellExecuteEx(&sei);
}

void CDirStatDoc::RecurseRefreshMountPointItems(CItem* item)
{
    if (item->IsType(IT_DIRECTORY) && item != GetRootItem() && GetWDSApp()->IsVolumeMountPoint(item->GetPath()))
    {
        RefreshItem(item);
    }
    for (int i = 0; i < item->GetChildrenCount(); i++)
    {
        RecurseRefreshMountPointItems(item->GetChild(i));
    }
}

void CDirStatDoc::RecurseRefreshJunctionItems(CItem* item)
{
    if (item->IsType(IT_DIRECTORY) && item != GetRootItem() && GetWDSApp()->IsFolderJunction(item->GetAttributes()))
    {
        RefreshItem(item);
    }
    for (int i = 0; i < item->GetChildrenCount(); i++)
    {
        RecurseRefreshJunctionItems(item->GetChild(i));
    }
}

// Gets all items of type IT_DRIVE.
//
void CDirStatDoc::GetDriveItems(CArray<CItem*, CItem*>& drives)
{
    drives.RemoveAll();

    CItem* root = GetRootItem();

    if (nullptr == root)
    {
        return;
    }

    if (root->IsType(IT_MYCOMPUTER))
    {
        for (int i = 0; i < root->GetChildrenCount(); i++)
        {
            CItem* drive = root->GetChild(i);
            ASSERT(drive->IsType(IT_DRIVE));
            drives.Add(drive);
        }
    }
    else if (root->IsType(IT_DRIVE))
    {
        drives.Add(root);
    }
}

void CDirStatDoc::RefreshRecyclers()
{
    CArray<CItem*, CItem*> drives;
    GetDriveItems(drives);

    for (int i = 0; i < drives.GetSize(); i++)
    {
        drives[i]->RefreshRecycler();
    }

    SetWorkingItem(GetRootItem());
}

void CDirStatDoc::RebuildExtensionData()
{
    CWaitCursor wc;

    m_extensionData.RemoveAll();
    // 2048 is a rough estimate for amount of different extensions
    m_extensionData.InitHashTable(2048);
    m_rootItem->RecurseCollectExtensionData(&m_extensionData);

    CStringArray sortedExtensions;
    SortExtensionData(sortedExtensions);
    SetExtensionColors(sortedExtensions);

    m_extensionDataValid = true;
}

void CDirStatDoc::SortExtensionData(CStringArray& sortedExtensions)
{
    sortedExtensions.SetSize(m_extensionData.GetCount());

    POSITION pos = m_extensionData.GetStartPosition();
    for (int i = 0; pos != nullptr; i++)
    {
        CStringW ext;
        SExtensionRecord r;
        m_extensionData.GetNextAssoc(pos, ext, r);

        sortedExtensions[i] = ext;
    }

    _pqsortExtensionData = &m_extensionData;
    qsort(sortedExtensions.GetData(), sortedExtensions.GetSize(), sizeof(CStringW), &_compareExtensions);
    _pqsortExtensionData = nullptr;
}

void CDirStatDoc::SetExtensionColors(const CStringArray& sortedExtensions)
{
    static CArray<COLORREF, COLORREF&> colors;

    if (colors.IsEmpty())
    {
        CTreemap::GetDefaultPalette(colors);
    }

    for (int i = 0; i < sortedExtensions.GetSize(); i++)
    {
        COLORREF c = colors[colors.GetSize() - 1];
        if (i < colors.GetSize())
        {
            c = colors[i];
        }
        m_extensionData[sortedExtensions[i]].color = c;
    }
}

CExtensionData* CDirStatDoc::_pqsortExtensionData;

int __cdecl CDirStatDoc::_compareExtensions(const void* item1, const void* item2)
{
    const CStringW* ext1 = (CStringW*)item1;
    const CStringW* ext2 = (CStringW*)item2;
    SExtensionRecord r1;
    SExtensionRecord r2;
    VERIFY(_pqsortExtensionData->Lookup(*ext1, r1));
    VERIFY(_pqsortExtensionData->Lookup(*ext2, r2));
    return usignum(r2.bytes, r1.bytes);
}

void CDirStatDoc::SetWorkingItemAncestor(CItem* item)
{
    if (m_workingItem != nullptr)
    {
        SetWorkingItem(CItem::FindCommonAncestor(m_workingItem, item));
    }
    else
    {
        SetWorkingItem(item);
    }
}

void CDirStatDoc::SetWorkingItem(CItem* item)
{
    if (GetMainFrame() != nullptr)
    {
        if (item != nullptr)
        {
            GetMainFrame()->ShowProgress(item->GetProgressRange());
        }
        else
        {
            GetMainFrame()->HideProgress();
        }
    }
    m_workingItem = item;
}

// Deletes a file or directory via SHFileOperation.
// Return: false, if canceled
//
bool CDirStatDoc::DeletePhysicalItem(CItem* item, bool toTrashBin)
{
    if (CPersistence::GetShowDeleteWarning())
    {
        CDeleteWarningDlg warning;
        warning.m_fileName = item->GetPath();
        if (IDYES != warning.DoModal())
        {
            return false;
        }
        CPersistence::SetShowDeleteWarning(!warning.m_dontShowAgain);
    }

    ASSERT(item->GetParent() != NULL);

    CModalShellApi msa;
    msa.DeleteFile(item->GetPath(), toTrashBin);

    RefreshItem(item);
    return true;
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
void CDirStatDoc::RefreshItem(CItem* item)
{
    ASSERT(item != NULL);

    CWaitCursor wc;

    ClearReselectChildStack();

    if (item->IsAncestorOf(GetZoomItem()))
    {
        SetZoomItem(item);
    }

    SetWorkingItemAncestor(item);

    CItem* parent = item->GetParent();

    if (!item->StartRefresh())
    {
        if (GetZoomItem() == item)
        {
            SetZoomItem(parent);
        }
        if (CTreeListControl::GetTheTreeListControl()->IsItemSelected(parent))
        {
            UpdateAllViews(nullptr, HINT_SELECTIONREFRESH);
        }
        if (m_workingItem == item)
        {
            SetWorkingItem(parent);
        }
    }

    UpdateAllViews(nullptr);
}

// UDC confirmation Dialog.
//
void CDirStatDoc::AskForConfirmation(const USERDEFINEDCLEANUP* udc, CItem* item)
{
    if (!udc->askForConfirmation)
    {
        return;
    }

    CStringW msg;
    msg.FormatMessage(udc->recurseIntoSubdirectories ? IDS_RUDC_CONFIRMATIONss : IDS_UDC_CONFIRMATIONss, udc->title.GetString(), item->GetPath().GetString());

    if (IDYES != AfxMessageBox(msg, MB_YESNO))
    {
        AfxThrowUserException();
    }
}

void CDirStatDoc::PerformUserDefinedCleanup(const USERDEFINEDCLEANUP* udc, CItem* item)
{
    CWaitCursor wc;

    const CStringW path = item->GetPath();

    // Verify that path still exists
    if (item->IsType(IT_DIRECTORY | IT_DRIVE))
    {
        if (!FolderExists(path) && !DriveExists(path))
        {
            MdThrowStringExceptionF(IDS_THEDIRECTORYsDOESNOTEXIST, path.GetString());
        }
    }
    else
    {
        ASSERT(item->IsType(IT_FILE));

        if (!::PathFileExists(path))
        {
            MdThrowStringExceptionF(IDS_THEFILEsDOESNOTEXIST, path.GetString());
        }
    }

    if (udc->recurseIntoSubdirectories)
    {
        ASSERT(item->IsType(IT_DRIVE | IT_DIRECTORY));

        RecursiveUserDefinedCleanup(udc, path, path);
    }
    else
    {
        CallUserDefinedCleanup(item->IsType(IT_DIRECTORY | IT_DRIVE), udc->commandLine, path, path, udc->showConsoleWindow, udc->waitForCompletion);
    }
}

void CDirStatDoc::RefreshAfterUserDefinedCleanup(const USERDEFINEDCLEANUP* udc, CItem* item)
{
    switch (udc->refreshPolicy)
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
        ASSERT(0);
    }
}

void CDirStatDoc::RecursiveUserDefinedCleanup(const USERDEFINEDCLEANUP* udc, const CStringW& rootPath, const CStringW& currentPath)
{
    // (Depth first.)

    FileFindEnhanced finder;
    for (BOOL b = finder.FindFile(currentPath + L"\\*.*"); b; b = finder.FindNextFile())
    {
        if (finder.IsDots() || !finder.IsDirectory())
        {
            continue;
        }
        if (GetWDSApp()->IsVolumeMountPoint(finder.GetFilePath()) && !GetOptions()->IsFollowMountPoints())
        {
            continue;
        }
        if (GetWDSApp()->IsFolderJunction(finder.GetAttributes()) && !GetOptions()->IsFollowJunctionPoints())
        {
            continue;
        }

        RecursiveUserDefinedCleanup(udc, rootPath, finder.GetFilePath());
    }

    CallUserDefinedCleanup(true, udc->commandLine, rootPath, currentPath, udc->showConsoleWindow, true);
}

void CDirStatDoc::CallUserDefinedCleanup(bool isDirectory, const CStringW& format, const CStringW& rootPath, const CStringW& currentPath, bool showConsoleWindow, bool wait)
{
    const CStringW userCommandLine = BuildUserDefinedCleanupCommandLine(format, rootPath, currentPath);

    const CStringW app = GetCOMSPEC();
    CStringW cmdline;
    cmdline.Format(L"%s /C %s", GetBaseNameFromPath(app).GetString(), userCommandLine.GetString());
    const CStringW directory = isDirectory ? currentPath : GetFolderNameFromPath(currentPath);

    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = showConsoleWindow ? SW_SHOWNORMAL : SW_HIDE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    const BOOL b = CreateProcess(
        app,
        cmdline.GetBuffer(),
        nullptr,
        nullptr,
        false,
        0,
        nullptr,
        directory,
        &si,
        &pi
    );
    cmdline.ReleaseBuffer();
    if (!b)
    {
        MdThrowStringExceptionF(IDS_COULDNOTCREATEPROCESSssss,
                                app.GetString(), cmdline.GetString(), directory.GetString(), MdGetWinErrorText(::GetLastError()).GetString()
        );
        return;
    }

    CloseHandle(pi.hThread);

    if (wait)
    {
        WaitForHandleWithRepainting(pi.hProcess);
    }

    CloseHandle(pi.hProcess);
}

CStringW CDirStatDoc::BuildUserDefinedCleanupCommandLine(LPCWSTR format, LPCWSTR rootPath, LPCWSTR currentPath)
{
    const CStringW rootName    = GetBaseNameFromPath(rootPath);
    const CStringW currentName = GetBaseNameFromPath(currentPath);

    CStringW s = format;

    // Because file names can contain "%", we first replace our placeholders with
    // strings which contain a forbidden character.
    s.Replace(L"%p", L">p");
    s.Replace(L"%n", L">n");
    s.Replace(L"%sp", L">sp");
    s.Replace(L"%sn", L">sn");

    // Now substitute
    s.Replace(L">p", rootPath);
    s.Replace(L">n", rootName);
    s.Replace(L">sp", currentPath);
    s.Replace(L">sn", currentName);

    return s;
}

void CDirStatDoc::PushReselectChild(CItem* item)
{
    m_reselectChildStack.AddHead(item);
}

CItem* CDirStatDoc::PopReselectChild()
{
    return m_reselectChildStack.RemoveHead();
}

void CDirStatDoc::ClearReselectChildStack()
{
    m_reselectChildStack.RemoveAll();
}

bool CDirStatDoc::IsReselectChildAvailable() const
{
    return !m_reselectChildStack.IsEmpty();
}

bool CDirStatDoc::DirectoryListHasFocus()
{
    return LF_DIRECTORYLIST == GetMainFrame()->GetLogicalFocus();
}

void CDirStatDoc::OnUpdateCentralHandler(CCmdUI* pCmdUI)
{
    struct command_filter
    {
        bool allow_none = false;       // allow display when nothing is selected
        bool allow_many = false;       // allow display when multiple items are selected
        bool allow_early = false;      // allow display before processing is finished
        bool tree_focus = false;       // only display in tree view
        ITEMTYPE types_allow = IT_ANY; // only display if these types are allowed
        bool (*extra)(CItem*) = [](CItem*) { return true; }; // extra checks
    };

    // special conditions
    static auto doc = this;
    static bool (*can_zoom_out)(CItem*) = [](CItem*) { return doc->GetZoomItem() != doc->GetRootItem(); };
    static bool (*parent_not_null)(CItem*) = [](CItem* item) { return item != nullptr && item->GetParent() != nullptr; };
    static bool (*reslect_avail)(CItem*) = [](CItem*) { return doc->IsReselectChildAvailable(); };
    static bool (*not_root)(CItem*) = [](CItem* item) { return item != nullptr && !item->IsRootItem(); };
    static bool (*is_suspended)(CItem*) = [](CItem*) { return GetMainFrame()->IsScanSuspended(); };
    static bool (*is_not_suspended)(CItem*) = [](CItem*) { return !doc->IsRootDone() && !GetMainFrame()->IsScanSuspended(); };

    static std::map<UINT, const command_filter> filters
    {
        // ID                           none   many   early  focus  types
        { ID_REFRESH_ALL,             { true,  true,  false, false, IT_MYCOMPUTER | IT_DRIVE | IT_DIRECTORY} },
        { ID_REFRESH_SELECTED,        { false, true,  false, true,  IT_MYCOMPUTER | IT_DRIVE | IT_DIRECTORY} },
        { ID_EDIT_COPY_CLIPBOARD,     { false, true,  true,  false, IT_DRIVE | IT_DIRECTORY | IT_FILE } },
        { ID_CLEANUP_EMPTY_BIN,       { true,  true,  false, false, IT_ANY} },
        { ID_TREEMAP_RESELECT_CHILD,  { true,  true,  true,  false, IT_ANY, reslect_avail } },
        { ID_TREEMAP_SELECT_PARENT,   { false, false, true,  false, IT_ANY, parent_not_null } },
        { ID_TREEMAP_ZOOMIN,          { false, false, false, false, IT_DRIVE | IT_DIRECTORY} },
        { ID_TREEMAP_ZOOMOUT,         { false, false, false, false, IT_DIRECTORY, can_zoom_out } },
        { ID_CLEANUP_EXPLORER_SELECT, { false, true,  true,  false, IT_DIRECTORY | IT_FILE } },
        { ID_CLEANUP_OPEN_IN_CONSOLE, { false, true,  true,  false, IT_DRIVE | IT_DIRECTORY | IT_FILE } },
        { ID_SCAN_RESUME,             { true,  true,  true,  false, IT_ANY, is_suspended } },
        { ID_SCAN_SUSPEND,            { true,  true,  true,  false, IT_ANY, is_not_suspended } },
        { ID_CLEANUP_DELETE_BIN,      { false, true,  true,  true,  IT_DIRECTORY | IT_FILE, not_root } },
        { ID_CLEANUP_DELETE,          { false, true,  true,  true,  IT_DIRECTORY | IT_FILE, not_root } },
        { ID_CLEANUP_OPEN_SELECTED,     { false, true,  true,  false, IT_MYCOMPUTER | IT_DRIVE | IT_DIRECTORY | IT_FILE } },
        { ID_CLEANUP_PROPERTIES,      { false, true,  true,  false, IT_MYCOMPUTER | IT_DRIVE | IT_DIRECTORY | IT_FILE } }
    };

    if (!filters.contains(pCmdUI->m_nID))
    {
        ASSERT(0);
        return;
    }

    const auto& filter = filters[pCmdUI->m_nID];
    const auto& items = CTreeListControl::GetTheTreeListControl()->GetAllSelected<CItem>();

    bool allow = true;
    allow &= !filter.tree_focus || DirectoryListHasFocus();
    allow &= filter.allow_none || !items.empty();
    allow &= filter.allow_many || items.size() <= 1;
    allow &= filter.allow_early || IsRootDone();
    if (items.empty()) allow &= filter.extra(nullptr);
    for (const auto& item : items)
    {
        allow &= filter.extra(item);
        allow &= item->IsType(filter.types_allow);
    }

    pCmdUI->Enable(allow); 
}

#define ON_COMMAMD_UPDATE_WRAPPER(x,y) ON_COMMAND(x, y) ON_UPDATE_COMMAND_UI(x, OnUpdateCentralHandler)
BEGIN_MESSAGE_MAP(CDirStatDoc, CDocument) 
    ON_COMMAMD_UPDATE_WRAPPER(ID_REFRESH_SELECTED, OnRefreshSelected)
    ON_COMMAMD_UPDATE_WRAPPER(ID_REFRESH_ALL, OnRefreshAll)
    ON_COMMAMD_UPDATE_WRAPPER(ID_EDIT_COPY, OnEditCopy)
    ON_COMMAMD_UPDATE_WRAPPER(ID_CLEANUP_EMPTY_BIN, OnCleanupEmptyRecycleBin)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWFREESPACE, OnUpdateViewShowFreeSpace)
    ON_COMMAND(ID_VIEW_SHOWFREESPACE, OnViewShowFreeSpace)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWUNKNOWN, OnUpdateViewShowUnknown)
    ON_COMMAND(ID_VIEW_SHOWUNKNOWN, OnViewShowUnknown)
    ON_COMMAMD_UPDATE_WRAPPER(ID_TREEMAP_ZOOMIN, OnTreemapZoomIn)
    ON_COMMAMD_UPDATE_WRAPPER(ID_TREEMAP_ZOOMOUT, OnTreemapZoomOut)
    ON_COMMAMD_UPDATE_WRAPPER(ID_CLEANUP_EXPLORER_SELECT, OnExplorerSelect)
    ON_COMMAMD_UPDATE_WRAPPER(ID_CLEANUP_OPEN_IN_CONSOLE, OnCommandPromptHere)
    ON_COMMAMD_UPDATE_WRAPPER(ID_CLEANUP_DELETE_BIN, OnCleanupDeleteToBin)
    ON_COMMAMD_UPDATE_WRAPPER(ID_CLEANUP_DELETE, OnCleanupDelete)
    ON_UPDATE_COMMAND_UI_RANGE(ID_USERDEFINEDCLEANUP0, ID_USERDEFINEDCLEANUP9, OnUpdateUserDefinedCleanup)
    ON_COMMAND_RANGE(ID_USERDEFINEDCLEANUP0, ID_USERDEFINEDCLEANUP9, OnUserDefinedCleanup)
    ON_COMMAMD_UPDATE_WRAPPER(ID_TREEMAP_SELECT_PARENT, OnTreemapSelectParent)
    ON_COMMAMD_UPDATE_WRAPPER(ID_TREEMAP_RESELECT_CHILD, OnTreemapReselectChild)
    ON_COMMAMD_UPDATE_WRAPPER(ID_CLEANUP_OPEN_SELECTED, OnCleanupOpenTarget)
    ON_COMMAMD_UPDATE_WRAPPER(ID_CLEANUP_PROPERTIES, OnCleanupProperties)
    ON_COMMAMD_UPDATE_WRAPPER(ID_SCAN_RESUME, OnScanResume)
    ON_COMMAMD_UPDATE_WRAPPER(ID_SCAN_SUSPEND, OnScanSuspend)
END_MESSAGE_MAP()

void CDirStatDoc::OnRefreshSelected()
{
    const auto & items = CTreeListControl::GetTheTreeListControl()->GetAllSelected<CItem>();
    for (const auto & item : items)
    {
        RefreshItem(item);
    }
}

void CDirStatDoc::OnRefreshAll()
{
    RefreshItem(GetRootItem());
}

void CDirStatDoc::OnEditCopy()
{
    // create concatenated paths
    CStringW paths;
    const auto & items = CTreeListControl::GetTheTreeListControl()->GetAllSelected<CItem>();
    for (const auto & item : items)
    {
        if (paths.GetLength() > 0) paths += L"\r\n";
        paths += item->GetPath();
    }

    CMainFrame::GetTheFrame()->CopyToClipboard(paths.GetBuffer());
}

void CDirStatDoc::OnCleanupEmptyRecycleBin()
{
    CModalShellApi msa;

    SHEmptyRecycleBin(*AfxGetMainWnd(), NULL, 0);

    RefreshRecyclers();
    UpdateAllViews(NULL);
}

void CDirStatDoc::OnUpdateViewShowFreeSpace(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_showFreeSpace);
}

void CDirStatDoc::OnViewShowFreeSpace()
{
    CArray<CItem*, CItem*> drives;
    GetDriveItems(drives);

    if (m_showFreeSpace)
    {
        for (int i = 0; i < drives.GetSize(); i++)
        {
            const CItem* free = drives[i]->FindFreeSpaceItem();
            ASSERT(free != NULL);

            if (GetZoomItem() == free)
            {
                m_zoomItem = free->GetParent();
            }

            drives[i]->RemoveFreeSpaceItem();
        }
        m_showFreeSpace = false;
    }
    else
    {
        for (int i = 0; i < drives.GetSize(); i++)
        {
            drives[i]->CreateFreeSpaceItem();
        }
        m_showFreeSpace = true;
    }

    if (drives.GetSize() > 0)
    {
        SetWorkingItem(GetRootItem());
    }

    UpdateAllViews(nullptr);
}

void CDirStatDoc::OnUpdateViewShowUnknown(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_showUnknown);
}

void CDirStatDoc::OnViewShowUnknown()
{
    CArray<CItem*, CItem*> drives;
    GetDriveItems(drives);

    if (m_showUnknown)
    {
        for (int i = 0; i < drives.GetSize(); i++)
        {
            const CItem* unknown = drives[i]->FindUnknownItem();
            ASSERT(unknown != NULL);

            if (GetZoomItem() == unknown)
            {
                m_zoomItem = unknown->GetParent();
            }

            drives[i]->RemoveUnknownItem();
        }
        m_showUnknown = false;
    }
    else
    {
        for (int i = 0; i < drives.GetSize(); i++)
        {
            drives[i]->CreateUnknownItem();
        }
        m_showUnknown = true;
    }

    if (drives.GetSize() > 0)
    {
        SetWorkingItem(GetRootItem());
    }

    UpdateAllViews(nullptr);
}

void CDirStatDoc::OnTreemapZoomIn()
{
    const auto & item = CTreeListControl::GetTheTreeListControl()->GetFirstSelectedItem<CItem>(true);
    if (item != nullptr)
    {
        SetZoomItem(item);
    }
}

void CDirStatDoc::OnTreemapZoomOut()
{
    if (GetZoomItem() != nullptr)
    {
        SetZoomItem(GetZoomItem()->GetParent());
    }
}

void CDirStatDoc::OnExplorerSelect()
{
    // accumulate a unique set of paths
    const auto& items = CTreeListControl::GetTheTreeListControl()->GetAllSelected<CItem>();
    std::unordered_set<std::wstring>paths;
    for (const auto& item : items)
    {
        // use function to determine parent to address non-drive rooted paths
        std::filesystem::path target(item->GetPath().GetString());
        paths.insert(target.parent_path());
    }
    
    for (const auto& path : paths)
    {
        // create path pidl
        SmartPointer<LPITEMIDLIST> parent(CoTaskMemFree);
        parent = ILCreateFromPath(path.c_str());

        // structures to hold and track pidls for children
        std::vector<SmartPointer<LPITEMIDLIST>> pidl_cleanup;
        std::vector<LPITEMIDLIST> pidl;

        // create list of children from paths
        for (auto & item : items)
        {
            // not processing this path yet
            std::filesystem::path target(item->GetPath().GetString());
            if (target.parent_path() == path)
            {
                pidl.push_back(ILCreateFromPath(item->GetPath()));
                pidl_cleanup.emplace_back(CoTaskMemFree, pidl.back());
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
        const auto& items = CTreeListControl::GetTheTreeListControl()->GetAllSelected<CItem>();
        std::unordered_set<std::wstring>paths;
        for (const auto& item : items)
        {
            paths.insert(item->GetFolderPath().GetString());
        }

        // launch a command prompt for each path
        const CStringW cmd = GetCOMSPEC();
        for (const auto& path : paths)
        {
            ShellExecuteThrow(*AfxGetMainWnd(), L"open", cmd, nullptr, path.c_str(), SW_SHOWNORMAL);
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
    const auto & items = CTreeListControl::GetTheTreeListControl()->GetAllSelected<CItem>();
    for (const auto & item : items)
    {
        if (DeletePhysicalItem(item, true))
        {
            RefreshRecyclers();
            UpdateAllViews(nullptr);
        }
    }
}

void CDirStatDoc::OnCleanupDelete()
{
    const auto & items = CTreeListControl::GetTheTreeListControl()->GetAllSelected<CItem>();
    for (const auto & item : items)
    {
        if (DeletePhysicalItem(item, false))
        {
            SetWorkingItem(GetRootItem());
            UpdateAllViews(nullptr);
        }
    }
}

void CDirStatDoc::OnUpdateUserDefinedCleanup(CCmdUI* pCmdUI)
{
    const int i = pCmdUI->m_nID - ID_USERDEFINEDCLEANUP0;
    const auto & items = CTreeListControl::GetTheTreeListControl()->GetAllSelected<CItem>();
    bool allow_control = DirectoryListHasFocus() && GetOptions()->IsUserDefinedCleanupEnabled(i) && items.size() > 1;
    for (const auto & item : items)
    {
        allow_control &= UserDefinedCleanupWorksForItem(GetOptions()->GetUserDefinedCleanup(i), item);
    }

    pCmdUI->Enable(allow_control);
}

void CDirStatDoc::OnUserDefinedCleanup(UINT id)
{
    const USERDEFINEDCLEANUP* udc = GetOptions()->GetUserDefinedCleanup(id - ID_USERDEFINEDCLEANUP0);
    const auto & items = CTreeListControl::GetTheTreeListControl()->GetAllSelected<CItem>();
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

void CDirStatDoc::OnTreemapSelectParent()
{
    const auto & item = CTreeListControl::GetTheTreeListControl()->GetFirstSelectedItem<CItem>(true);
    PushReselectChild(item);
    CTreeListControl::GetTheTreeListControl()->SelectItem(item->GetParent(), true, true);
    UpdateAllViews(nullptr, HINT_SELECTIONREFRESH);
}

void CDirStatDoc::OnTreemapReselectChild()
{
    CItem* item = PopReselectChild();
    CTreeListControl::GetTheTreeListControl()->SelectItem(item, true, true);
    UpdateAllViews(nullptr, HINT_SELECTIONREFRESH);
}

void CDirStatDoc::OnCleanupOpenTarget()
{
    const auto & items = CTreeListControl::GetTheTreeListControl()->GetAllSelected<CItem>();
    for (const auto & item : items)
    {
        OpenItem(item);
    }
}

void CDirStatDoc::OnCleanupProperties()
{
    const auto & items = CTreeListControl::GetTheTreeListControl()->GetAllSelected<CItem>();
    for (const auto & item : items)
    {
        OpenItem(item, L"properties");
    }
}

void CDirStatDoc::OnScanSuspend()
{
    GetMainFrame()->SuspendScan(true);
}

void CDirStatDoc::OnScanResume()
{
    GetMainFrame()->SuspendScan(false);
}
