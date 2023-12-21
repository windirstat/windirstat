// dirstatdoc.cpp - Implementation of CDirstatDoc
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
#include "windirstat.h"
#include "item.h"
#include "mainframe.h"
#include "globalhelpers.h"
#include "deletewarningdlg.h"
#include "modalshellapi.h"
#include <common/mdexceptions.h>
#include <common/cotaskmem.h>
#include <common/commonhelpers.h>
#include "dirstatdoc.h"

CDirstatDoc *_theDocument;

CDirstatDoc *GetDocument()
{
    return _theDocument;
}

IMPLEMENT_DYNCREATE(CDirstatDoc, CDocument)

CDirstatDoc::CDirstatDoc()
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

CDirstatDoc::~CDirstatDoc()
{
    CPersistence::SetShowFreeSpace(m_showFreeSpace);
    CPersistence::SetShowUnknown(m_showUnknown);

    delete m_rootItem;
    _theDocument = nullptr;
}

// Encodes a selection from the CSelectDrivesDlg into a string which can be routed as a pseudo
// document "path" through MFC and finally arrives in OnOpenDocument().
//
CStringW CDirstatDoc::EncodeSelection(RADIO radio, const CStringW& folder, const CStringArray& drives)
{
    CStringW ret;
    switch (radio)
    {
    case RADIO_ALLLOCALDRIVES:
    case RADIO_SOMEDRIVES:
        {
            for(int i = 0; i < drives.GetSize(); i++)
            {
                if(i > 0)
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
void CDirstatDoc::DecodeSelection(const CStringW& s, CStringW& folder, CStringArray& drives)
{
    folder.Empty();
    drives.RemoveAll();

    // s is either something like "C:\programme"
    // or something like "C:|D:|E:".

    CStringArray sa;
    int i = 0;

    while(i < s.GetLength())
    {
        CStringW token;
        while(i < s.GetLength() && s[i] != GetEncodingSeparator())
        {
            token += s[i++];
        }

        token.TrimLeft();
        token.TrimRight();
        ASSERT(!token.IsEmpty());
        sa.Add(token);

        if(i < s.GetLength())
        {
            i++;
        }
    }

    ASSERT(sa.GetSize() > 0);

    if(sa.GetSize() > 1)
    {
        for(int j = 0; j < sa.GetSize(); j++)
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
        if(2 == f.GetLength() && wds::chrColon == f[1])
        {
            drives.Add(f + L"\\");
        }
        else
        {
            // Remove trailing backslash, if any and not drive-root.
            if(f.GetLength() > 0 && wds::strBackslash == f.Right(1) && (f.GetLength() != 3 || f[1] != wds::chrColon))
            {
                f = f.Left(f.GetLength() - 1);
            }

            folder = f;
        }
    }
}

WCHAR CDirstatDoc::GetEncodingSeparator()
{
    return wds::chrPipe; // This character must be one, which is not allowed in file names.
}

void CDirstatDoc::DeleteContents()
{
    delete m_rootItem;
    m_rootItem = nullptr;
    SetWorkingItem(nullptr);
    m_zoomItem = nullptr;
    m_selectedItems.RemoveAll();
    GetWDSApp()->ReReadMountPoints();
}

BOOL CDirstatDoc::OnNewDocument()
{
    if(!CDocument::OnNewDocument())
    {
        return FALSE;
    }

    UpdateAllViews(nullptr, HINT_NEWROOT);
    return TRUE;
}

BOOL CDirstatDoc::OnOpenDocument(LPCWSTR lpszPathName)
{
    CDocument::OnNewDocument(); // --> DeleteContents()

    const CStringW spec = lpszPathName;
    CStringW folder;
    CStringArray drives;
    DecodeSelection(spec, folder, drives);

    CStringArray rootFolders;
    if(drives.GetSize() > 0)
    {
        m_showMyComputer = drives.GetSize() > 1;
        for(int i = 0; i < drives.GetSize(); i++)
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

    CArray<CItem *, CItem *> driveItems;

    if(m_showMyComputer)
    {
        m_rootItem = new CItem(static_cast<ITEMTYPE>(IT_MYCOMPUTER | ITF_ROOTITEM), LoadString(IDS_MYCOMPUTER));
        for(int i = 0; i < rootFolders.GetSize(); i++)
        {
            CItem *drive = new CItem(IT_DRIVE, rootFolders[i]);
            driveItems.Add(drive);
            m_rootItem->AddChild(drive);
        }
    }
    else
    {
	    const ITEMTYPE type = IsDrive(rootFolders[0]) ? IT_DRIVE : IT_DIRECTORY;
        m_rootItem = new CItem(static_cast<ITEMTYPE>(type | ITF_ROOTITEM), rootFolders[0], false);
        if(IT_DRIVE == m_rootItem->GetType())
        {
            driveItems.Add(m_rootItem);
        }
        m_rootItem->UpdateLastChange();
    }
    m_zoomItem = m_rootItem;

    for(int i = 0; i < driveItems.GetSize(); i++)
    {
        if(OptionShowFreeSpace())
        {
            driveItems[i]->CreateFreeSpaceItem();
        }
        if(OptionShowUnknown())
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
void CDirstatDoc::SetPathName(LPCWSTR lpszPathName, BOOL /*bAddToMRU*/)
{
    // MRU would be fine but is not implemented yet.

    m_strPathName = lpszPathName;
    ASSERT(!m_strPathName.IsEmpty());       // must be set to something
    m_bEmbedded = FALSE;
    SetTitle(lpszPathName);

    ASSERT_VALID(this);
}

void CDirstatDoc::Serialize(CArchive& /*ar*/)
{
}

// Prefix the window title (with percentage or "Scanning")
//
void CDirstatDoc::SetTitlePrefix(const CStringW& prefix) const
{
	const CStringW docName = prefix + GetTitle();
    GetMainFrame()->UpdateFrameTitleForDocument(docName);
}

COLORREF CDirstatDoc::GetCushionColor(LPCWSTR ext)
{
    SExtensionRecord r;
    VERIFY(GetExtensionData()->Lookup(ext, r));
    return r.color;
}

COLORREF CDirstatDoc::GetZoomColor()
{
    return RGB(0,0,255);
}

bool CDirstatDoc::OptionShowFreeSpace() const
{
    return m_showFreeSpace;
}

bool CDirstatDoc::OptionShowUnknown()
{
    return m_showUnknown;
}

const CExtensionData *CDirstatDoc::GetExtensionData()
{
    if(!m_extensionDataValid)
    {
        RebuildExtensionData();
    }
    return &m_extensionData;
}

ULONGLONG CDirstatDoc::GetRootSize()
{
    ASSERT(m_rootItem != NULL);
    ASSERT(IsRootDone());
    return m_rootItem->GetSize();
}

void CDirstatDoc::ForgetItemTree()
{
    // The program is closing.
    // As "delete m_rootItem" can last a long time (many minutes), if
    // we have been paged out, we simply forget our item tree here and
    // hope that the system will free all our memory anyway.
    m_rootItem = nullptr;

    m_zoomItem = nullptr;
    m_selectedItems.RemoveAll();
}

// This method does some work for ticks ms.
// return: true if done or suspended.
//
bool CDirstatDoc::Work(CWorkLimiter* limiter)
{
    if(nullptr == m_rootItem)
    {
        return true;
    }

    if(GetMainFrame()->IsProgressSuspended())
    {
        return true;
    }

    if(!m_rootItem->IsDone())
    {
        m_rootItem->DoSomeWork(limiter);
        if(m_rootItem->IsDone())
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
            if(m_workingItem != nullptr) // to be honest, "defensive programming" is stupid, but c'est la vie: it's safer.
            {
                GetMainFrame()->SetProgressPos(m_workingItem->GetProgressPos());
            }

            UpdateAllViews(nullptr, HINT_SOMEWORKDONE);
        }

    }
    if(m_rootItem->IsDone())
    {
        SetWorkingItem(nullptr);
        return true;
    }
    else
    {
        return false;
    }
}

bool CDirstatDoc::IsDrive(const CStringW& spec)
{
    return 3 == spec.GetLength() && wds::chrColon == spec[1] && wds::chrBackslash == spec[2];
}

// Starts a refresh of all mount points in our tree.
// Called when the user changes the follow mount points option.
//
void CDirstatDoc::RefreshMountPointItems()
{
    CWaitCursor wc;

    CItem *root = GetRootItem();
    if(nullptr == root)
    {
        return;
    }

    RecurseRefreshMountPointItems(root);
}

// Starts a refresh of all junction points in our tree.
// Called when the user changes the ignore junction points option.
//
void CDirstatDoc::RefreshJunctionItems()
{
    CWaitCursor wc;

    CItem *root = GetRootItem();
    if(nullptr == root)
    {
        return;
    }

    RecurseRefreshJunctionItems(root);
}

bool CDirstatDoc::IsRootDone()
{
    return m_rootItem != nullptr && m_rootItem->IsDone();
}

CItem *CDirstatDoc::GetRootItem()
{
    return m_rootItem;
}

CItem *CDirstatDoc::GetZoomItem()
{
    return m_zoomItem;
}

bool CDirstatDoc::IsZoomed()
{
    return GetZoomItem() != GetRootItem();
}

void CDirstatDoc::RemoveAllSelections()
{
    m_selectedItems.RemoveAll();
}

CItem *CDirstatDoc::GetSelectionParent()
{
    ASSERT(m_selectedItems.GetCount() > 0);
    const CItem *item = m_selectedItems[0];
    return item->GetParent();
}

bool CDirstatDoc::CanAddSelection(const CItem *item)
{
    if (m_selectedItems.GetCount() == 0)
        return true;

    return item->GetParent() == GetSelectionParent();
}

void CDirstatDoc::AddSelection(const CItem *item)
{
    ASSERT(CanAddSelection(item));
    m_selectedItems.Add(const_cast<CItem *>(item));
}

void CDirstatDoc::RemoveSelection(const CItem *item)
{
    for (int i = 0; i < m_selectedItems.GetCount(); i++)
    {
        if (m_selectedItems[i] == item)
        {
            m_selectedItems.RemoveAt(i);
            return;
        }
    }
    ASSERT(!m_selectedItems.GetCount()); // Must never reach this point
}

#ifdef _DEBUG
void CDirstatDoc::AssertSelectionValid()
{
    if (m_selectedItems.GetCount() == 0)
        return;
    const CItem *parent = GetSelectionParent();
    for (int i=0; i < m_selectedItems.GetCount(); i++)
        ASSERT(m_selectedItems[i]->GetParent() == parent);
}
#endif

void CDirstatDoc::SetSelection(const CItem * /*item*/, bool /*keepReselectChildStack*/)
{
}

CItem *CDirstatDoc::GetSelection(size_t i)
{
    return m_selectedItems.GetCount() ? m_selectedItems[i] : nullptr;
}

size_t CDirstatDoc::GetSelectionCount()
{
    return m_selectedItems.GetCount();
}

bool CDirstatDoc::IsSelected(const CItem *item)
{
    for (int i = 0; i < m_selectedItems.GetCount(); i++)
    {
        if (m_selectedItems[i] == item)
        {
            return true;
        }
    }
    return false;
}

void CDirstatDoc::SetHighlightExtension(LPCWSTR ext)
{
    m_highlightExtension = ext;
    GetMainFrame()->SetSelectionMessageText();
}

CStringW CDirstatDoc::GetHighlightExtension()
{
    return m_highlightExtension;
}

// The very root has been deleted.
//
void CDirstatDoc::UnlinkRoot()
{
    DeleteContents();
    UpdateAllViews(nullptr, HINT_NEWROOT);
}

// Determines, whether an UDC works for a given item.
//
bool CDirstatDoc::UserDefinedCleanupWorksForItem(const USERDEFINEDCLEANUP *udc, const CItem *item)
{
    bool works = false;

    if(item != nullptr)
    {
        if(!udc->worksForUncPaths && item->HasUncPath())
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

        case IT_FILESFOLDER:
            {
                works = udc->worksForFilesFolder;
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

ULONGLONG CDirstatDoc::GetWorkingItemReadJobs()
{
    if(m_workingItem != nullptr)
    {
        return m_workingItem->GetReadJobs();
    }
    else
    {
        return 0;
    }
}

void CDirstatDoc::OpenItem(const CItem *item)
{
    ASSERT(item != NULL);

    CWaitCursor wc;

    try
    {
        CStringW path;

        switch (item->GetType())
        {
        case IT_MYCOMPUTER:
            {
                SHELLEXECUTEINFO sei;
                ZeroMemory(&sei, sizeof(sei));
                sei.cbSize = sizeof(sei);
                sei.hwnd = *AfxGetMainWnd();
                sei.lpVerb = L"open";
                //sei.fMask = SEE_MASK_INVOKEIDLIST;
                sei.nShow = SW_SHOWNORMAL;
                CCoTaskMem<LPITEMIDLIST> pidl;

                GetPidlOfMyComputer(&pidl);
                sei.lpIDList = pidl;
                sei.fMask |= SEE_MASK_IDLIST;

                ShellExecuteEx(&sei);
                // ShellExecuteEx seems to display its own Messagebox, if failed.
            }
            break;

        case IT_DRIVE:
        case IT_DIRECTORY:
            {
                path = item->GetFolderPath();
            }
            break;

        case IT_FILE:
            {
                path = item->GetPath();
            }
            break;

        default:
            {
                ASSERT(0);
            }
        }

        ShellExecuteWithAssocDialog(*AfxGetMainWnd(), path);
    }
    catch (CException *pe)
    {
        pe->ReportError();
        pe->Delete();
    }
}

void CDirstatDoc::RecurseRefreshMountPointItems(CItem *item)
{
    if(IT_DIRECTORY == item->GetType() && item != GetRootItem() && GetWDSApp()->IsVolumeMountPoint(item->GetPath()))
    {
        RefreshItem(item);
    }
    for(int i = 0; i < item->GetChildrenCount(); i++)
    {
        RecurseRefreshMountPointItems(item->GetChild(i));
    }
}

void CDirstatDoc::RecurseRefreshJunctionItems(CItem *item)
{
    
    if(IT_DIRECTORY == item->GetType() && item != GetRootItem() && GetWDSApp()->IsFolderJunction(item->GetAttributes()))
    {
        RefreshItem(item);
    }
    for(int i = 0; i < item->GetChildrenCount(); i++)
    {
        RecurseRefreshJunctionItems(item->GetChild(i));
    }
}

// Gets all items of type IT_DRIVE.
//
void CDirstatDoc::GetDriveItems(CArray<CItem *, CItem *>& drives)
{
    drives.RemoveAll();

    CItem *root = GetRootItem();

    if(nullptr == root)
    {
        return;
    }

    if(IT_MYCOMPUTER == root->GetType())
    {
        for(int i = 0; i < root->GetChildrenCount(); i++)
        {
            CItem *drive = root->GetChild(i);
            ASSERT(IT_DRIVE == drive->GetType());
            drives.Add(drive);
        }
    }
    else if(IT_DRIVE == root->GetType())
    {
        drives.Add(root);
    }
}

void CDirstatDoc::RefreshRecyclers()
{
    CArray<CItem *, CItem *> drives;
    GetDriveItems(drives);

    for(int i = 0; i < drives.GetSize(); i++)
    {
        drives[i]->RefreshRecycler();
    }

    SetWorkingItem(GetRootItem());
}

void CDirstatDoc::RebuildExtensionData()
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

void CDirstatDoc::SortExtensionData(CStringArray& sortedExtensions)
{
    sortedExtensions.SetSize(m_extensionData.GetCount());

    int i = 0;
    POSITION pos = m_extensionData.GetStartPosition();
    while(pos != nullptr)
    {
        CStringW ext;
        SExtensionRecord r;
        m_extensionData.GetNextAssoc(pos, ext, r);

        sortedExtensions[i++]= ext;
    }

    _pqsortExtensionData = &m_extensionData;
    qsort(sortedExtensions.GetData(), sortedExtensions.GetSize(), sizeof(CStringW), &_compareExtensions);
    _pqsortExtensionData = nullptr;
}

void CDirstatDoc::SetExtensionColors(const CStringArray& sortedExtensions)
{
    static CArray<COLORREF, COLORREF&> colors;

    if(0 == colors.GetSize())
    {
        CTreemap::GetDefaultPalette(colors);
    }

    for(int i = 0; i < sortedExtensions.GetSize(); i++)
    {
        COLORREF c = colors[colors.GetSize() - 1];
        if(i < colors.GetSize())
        {
            c = colors[i];
        }
        m_extensionData[sortedExtensions[i]].color = c;
    }
}

CExtensionData *CDirstatDoc::_pqsortExtensionData;

int __cdecl CDirstatDoc::_compareExtensions(const void *item1, const void *item2)
{
	const CStringW *ext1 = (CStringW *)item1;
	const CStringW *ext2 = (CStringW *)item2;
    SExtensionRecord r1;
    SExtensionRecord r2;
    VERIFY(_pqsortExtensionData->Lookup(*ext1, r1));
    VERIFY(_pqsortExtensionData->Lookup(*ext2, r2));
    return usignum(r2.bytes, r1.bytes);
}

void CDirstatDoc::SetWorkingItemAncestor(CItem *item)
{
    if(m_workingItem != nullptr)
    {
        SetWorkingItem(CItem::FindCommonAncestor(m_workingItem, item));
    }
    else
    {
        SetWorkingItem(item);
    }
}

void CDirstatDoc::SetWorkingItem(CItem *item)
{
    if(GetMainFrame() != nullptr)
    {
        if(item != nullptr)
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
bool CDirstatDoc::DeletePhysicalItem(CItem *item, bool toTrashBin)
{
    if(CPersistence::GetShowDeleteWarning())
    {
        CDeleteWarningDlg warning;
        warning.m_fileName = item->GetPath();
        if(IDYES != warning.DoModal())
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

void CDirstatDoc::SetZoomItem(CItem *item)
{
    m_zoomItem = item;
    UpdateAllViews(nullptr, HINT_ZOOMCHANGED);
}

// Starts a refresh of an item.
// If the physical item has been deleted,
// updates selection, zoom and working item accordingly.
//
void CDirstatDoc::RefreshItem(CItem *item)
{
    ASSERT(item != NULL);

    CWaitCursor wc;

    ClearReselectChildStack();

    if(item->IsAncestorOf(GetZoomItem()))
    {
        SetZoomItem(item);
    }

    // FIXME: Multi-select
    if(item->IsAncestorOf(GetSelection(0)))
    {
        SetSelection(item);
        UpdateAllViews(nullptr, HINT_SELECTIONCHANGED);
    }

    SetWorkingItemAncestor(item);

    CItem *parent = item->GetParent();

    if(!item->StartRefresh())
    {
        if(GetZoomItem() == item)
        {
            SetZoomItem(parent);
        }
        // FIXME: Multi-select
        if(GetSelection(0) == item)
        {
            SetSelection(parent);
            UpdateAllViews(nullptr, HINT_SELECTIONCHANGED);
        }
        if(m_workingItem == item)
        {
            SetWorkingItem(parent);
        }
    }

    UpdateAllViews(nullptr);
}

// UDC confirmation Dialog.
//
void CDirstatDoc::AskForConfirmation(const USERDEFINEDCLEANUP *udc, CItem *item)
{
    if(!udc->askForConfirmation)
    {
        return;
    }

    CStringW msg;
    msg.FormatMessage(udc->recurseIntoSubdirectories ? IDS_RUDC_CONFIRMATIONss : IDS_UDC_CONFIRMATIONss, udc->title.GetString(), item->GetPath().GetString());

    if(IDYES != AfxMessageBox(msg, MB_YESNO))
    {
        AfxThrowUserException();
    }
}

void CDirstatDoc::PerformUserDefinedCleanup(const USERDEFINEDCLEANUP *udc, CItem *item)
{
    CWaitCursor wc;

    const CStringW path = item->GetPath();

    const bool isDirectory = IT_DRIVE == item->GetType() || IT_DIRECTORY == item->GetType() || IT_FILESFOLDER == item->GetType();

    // Verify that path still exists
    if(isDirectory)
    {
        if(!FolderExists(path) && !DriveExists(path))
        {
            MdThrowStringExceptionF(IDS_THEDIRECTORYsDOESNOTEXIST, path.GetString());
        }
    }
    else
    {
        ASSERT(IT_FILE == item->GetType());

        if(!::PathFileExists(path))
        {
            MdThrowStringExceptionF(IDS_THEFILEsDOESNOTEXIST, path.GetString());
        }
    }

    if(udc->recurseIntoSubdirectories && item->GetType() != IT_FILESFOLDER)
    {
        ASSERT(IT_DRIVE == item->GetType() || IT_DIRECTORY == item->GetType());

        RecursiveUserDefinedCleanup(udc, path, path);
    }
    else
    {
        CallUserDefinedCleanup(isDirectory, udc->commandLine, path, path, udc->showConsoleWindow, udc->waitForCompletion);
    }
}

void CDirstatDoc::RefreshAfterUserDefinedCleanup(const USERDEFINEDCLEANUP *udc, CItem *item)
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

void CDirstatDoc::RecursiveUserDefinedCleanup(const USERDEFINEDCLEANUP *udc, const CStringW& rootPath, const CStringW& currentPath)
{
    // (Depth first.)

    CFileFindWDS finder;
    BOOL b = finder.FindFile(currentPath + L"\\*.*");
    while(b)
    {
        b = finder.FindNextFile();
        if(finder.IsDots() || !finder.IsDirectory())
        {
            continue;
        }
        if(GetWDSApp()->IsVolumeMountPoint(finder.GetFilePath()) && !GetOptions()->IsFollowMountPoints())
        {
            continue;
        }
        if(GetWDSApp()->IsFolderJunction(finder.GetAttributes()) && !GetOptions()->IsFollowJunctionPoints())
        {
            continue;
        }

        RecursiveUserDefinedCleanup(udc, rootPath, finder.GetFilePath());
    }

    CallUserDefinedCleanup(true, udc->commandLine, rootPath, currentPath, udc->showConsoleWindow, true);
}

void CDirstatDoc::CallUserDefinedCleanup(bool isDirectory, const CStringW& format, const CStringW& rootPath, const CStringW& currentPath, bool showConsoleWindow, bool wait)
{
	const CStringW userCommandLine = BuildUserDefinedCleanupCommandLine(format, rootPath, currentPath);

	const CStringW app = GetCOMSPEC();
    CStringW cmdline;
    cmdline.Format(L"%s /C %s", GetBaseNameFromPath(app).GetString(), userCommandLine.GetString());
	const CStringW directory = isDirectory ? currentPath : GetFolderNameFromPath(currentPath);

    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
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
    if(!b)
    {
        MdThrowStringExceptionF(IDS_COULDNOTCREATEPROCESSssss,
            app.GetString(), cmdline.GetString(), directory.GetString(), MdGetWinErrorText(::GetLastError()).GetString()
        );
        return;
    }

    CloseHandle(pi.hThread);

    if(wait)
    {
        WaitForHandleWithRepainting(pi.hProcess);
    }

    CloseHandle(pi.hProcess);
}


CStringW CDirstatDoc::BuildUserDefinedCleanupCommandLine(LPCWSTR format, LPCWSTR rootPath, LPCWSTR currentPath)
{
	const CStringW rootName = GetBaseNameFromPath(rootPath);
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


void CDirstatDoc::PushReselectChild(CItem *item)
{
    m_reselectChildStack.AddHead(item);
}

CItem *CDirstatDoc::PopReselectChild()
{
    return m_reselectChildStack.RemoveHead();
}

void CDirstatDoc::ClearReselectChildStack()
{
    m_reselectChildStack.RemoveAll();
}

bool CDirstatDoc::IsReselectChildAvailable()
{
    return !m_reselectChildStack.IsEmpty();
}

bool CDirstatDoc::DirectoryListHasFocus()
{
    return LF_DIRECTORYLIST == GetMainFrame()->GetLogicalFocus();
}

BEGIN_MESSAGE_MAP(CDirstatDoc, CDocument)
    ON_COMMAND(ID_REFRESHSELECTED, OnRefreshselected)
    ON_UPDATE_COMMAND_UI(ID_REFRESHSELECTED, OnUpdateRefreshselected)
    ON_COMMAND(ID_REFRESHALL, OnRefreshall)
    ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
    ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWFREESPACE, OnUpdateViewShowfreespace)
    ON_COMMAND(ID_VIEW_SHOWFREESPACE, OnViewShowfreespace)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWUNKNOWN, OnUpdateViewShowunknown)
    ON_COMMAND(ID_VIEW_SHOWUNKNOWN, OnViewShowunknown)
    ON_UPDATE_COMMAND_UI(ID_TREEMAP_SELECTPARENT, OnUpdateTreemapSelectparent)
    ON_COMMAND(ID_TREEMAP_SELECTPARENT, OnTreemapSelectparent)
    ON_UPDATE_COMMAND_UI(ID_TREEMAP_ZOOMIN, OnUpdateTreemapZoomin)
    ON_COMMAND(ID_TREEMAP_ZOOMIN, OnTreemapZoomin)
    ON_UPDATE_COMMAND_UI(ID_TREEMAP_ZOOMOUT, OnUpdateTreemapZoomout)
    ON_COMMAND(ID_TREEMAP_ZOOMOUT, OnTreemapZoomout)
    ON_UPDATE_COMMAND_UI(ID_CLEANUP_OPENINEXPLORER, OnUpdateExplorerHere)
    ON_COMMAND(ID_CLEANUP_OPENINEXPLORER, OnExplorerHere)
    ON_UPDATE_COMMAND_UI(ID_CLEANUP_OPENINCONSOLE, OnUpdateCommandPromptHere)
    ON_COMMAND(ID_CLEANUP_OPENINCONSOLE, OnCommandPromptHere)
    ON_UPDATE_COMMAND_UI(ID_CLEANUP_DELETETOTRASHBIN, OnUpdateCleanupDeletetotrashbin)
    ON_COMMAND(ID_CLEANUP_DELETETOTRASHBIN, OnCleanupDeletetotrashbin)
    ON_UPDATE_COMMAND_UI(ID_CLEANUP_DELETE, OnUpdateCleanupDelete)
    ON_COMMAND(ID_CLEANUP_DELETE, OnCleanupDelete)
    ON_UPDATE_COMMAND_UI_RANGE(ID_USERDEFINEDCLEANUP0, ID_USERDEFINEDCLEANUP9, OnUpdateUserdefinedcleanup)
    ON_COMMAND_RANGE(ID_USERDEFINEDCLEANUP0, ID_USERDEFINEDCLEANUP9, OnUserdefinedcleanup)
    ON_UPDATE_COMMAND_UI(ID_REFRESHALL, OnUpdateRefreshall)
    ON_UPDATE_COMMAND_UI(ID_TREEMAP_RESELECTCHILD, OnUpdateTreemapReselectchild)
    ON_COMMAND(ID_TREEMAP_RESELECTCHILD, OnTreemapReselectchild)
    ON_UPDATE_COMMAND_UI(ID_CLEANUP_OPEN, OnUpdateCleanupOpen)
    ON_COMMAND(ID_CLEANUP_OPEN, OnCleanupOpen)
    ON_UPDATE_COMMAND_UI(ID_CLEANUP_PROPERTIES, OnUpdateCleanupProperties)
    ON_COMMAND(ID_CLEANUP_PROPERTIES, OnCleanupProperties)
END_MESSAGE_MAP()


void CDirstatDoc::OnUpdateRefreshselected(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(
        DirectoryListHasFocus()
        // FIXME: Multi-select
        && GetSelection(0) != nullptr
        // FIXME: Multi-select
        && GetSelection(0)->GetType() != IT_FREESPACE
        // FIXME: Multi-select
        && GetSelection(0)->GetType() != IT_UNKNOWN
    );
}

void CDirstatDoc::OnRefreshselected()
{
    // FIXME: Multi-select
    RefreshItem(GetSelection(0));
}

void CDirstatDoc::OnUpdateRefreshall(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(GetRootItem() != nullptr);
}

void CDirstatDoc::OnRefreshall()
{
    RefreshItem(GetRootItem());
}

void CDirstatDoc::OnUpdateEditCopy(CCmdUI *pCmdUI)
{
    // FIXME: Multi-select
    const CItem *item = GetSelection(0);
    pCmdUI->Enable(
        DirectoryListHasFocus() &&
        item != nullptr &&
        item->GetType() != IT_MYCOMPUTER &&
        item->GetType() != IT_FILESFOLDER &&
        item->GetType() != IT_FREESPACE &&
        item->GetType() != IT_UNKNOWN
    );
}

void CDirstatDoc::OnEditCopy()
{
    CStringW paths;
    for (size_t i = 0; i < GetSelectionCount(); i++)
    {
        if (i > 0)
            paths += L"\r\n";

        paths += GetSelection(i)->GetPath();
    }

    // FIXME: Need to fix the clipboard code!!!
    AfxMessageBox(paths);
}

void CDirstatDoc::OnUpdateViewShowfreespace(CCmdUI *pCmdUI)
{
    pCmdUI->SetCheck(m_showFreeSpace);
}

void CDirstatDoc::OnViewShowfreespace()
{
    CArray<CItem *, CItem *> drives;
    GetDriveItems(drives);

    if(m_showFreeSpace)
    {
        for(int i = 0; i < drives.GetSize(); i++)
        {
	        const CItem *free = drives[i]->FindFreeSpaceItem();
            ASSERT(free != NULL);

            // FIXME: Multi-select
            if(GetSelection(0) == free)
            {
                SetSelection(free->GetParent());
            }

            if(GetZoomItem() == free)
            {
                m_zoomItem = free->GetParent();
            }

            drives[i]->RemoveFreeSpaceItem();
        }
        m_showFreeSpace = false;
    }
    else
    {
        for(int i = 0; i < drives.GetSize(); i++)
        {
            drives[i]->CreateFreeSpaceItem();
        }
        m_showFreeSpace = true;
    }

    if(drives.GetSize() > 0)
    {
        SetWorkingItem(GetRootItem());
    }

    UpdateAllViews(nullptr);
}

void CDirstatDoc::OnUpdateViewShowunknown(CCmdUI *pCmdUI)
{
    pCmdUI->SetCheck(m_showUnknown);
}

void CDirstatDoc::OnViewShowunknown()
{
    CArray<CItem *, CItem *> drives;
    GetDriveItems(drives);

    if(m_showUnknown)
    {
        for(int i = 0; i < drives.GetSize(); i++)
        {
	        const CItem *unknown = drives[i]->FindUnknownItem();
            ASSERT(unknown != NULL);

            // FIXME: Multi-select
            if(GetSelection(0) == unknown)
            {
                SetSelection(unknown->GetParent());
            }

            if(GetZoomItem() == unknown)
            {
                m_zoomItem = unknown->GetParent();
            }

            drives[i]->RemoveUnknownItem();
        }
        m_showUnknown = false;
    }
    else
    {
        for(int i = 0; i < drives.GetSize(); i++)
        {
            drives[i]->CreateUnknownItem();
        }
        m_showUnknown = true;
    }

    if(drives.GetSize() > 0)
    {
        SetWorkingItem(GetRootItem());
    }

    UpdateAllViews(nullptr);
}

void CDirstatDoc::OnUpdateTreemapZoomin(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(
        m_rootItem != nullptr && m_rootItem->IsDone()
        // FIXME: Multi-select (2x)
        && GetSelection(0) != nullptr && GetSelection(0) != GetZoomItem()
    );
}

void CDirstatDoc::OnTreemapZoomin()
{
    // FIXME: Multi-select
    CItem *p = GetSelection(0);
    CItem *z = nullptr;
    while(p != GetZoomItem())
    {
        z = p;
        p = p->GetParent();
    }
    ASSERT(z != NULL);
    SetZoomItem(z);
}

void CDirstatDoc::OnUpdateTreemapZoomout(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(
        m_rootItem != nullptr && m_rootItem->IsDone()
        && GetZoomItem() != m_rootItem
    );
}

void CDirstatDoc::OnTreemapZoomout()
{
    SetZoomItem(GetZoomItem()->GetParent());
}


void CDirstatDoc::OnUpdateExplorerHere(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(
        DirectoryListHasFocus()
        // FIXME: Multi-select
        && GetSelection(0) != nullptr
        // FIXME: Multi-select
        && GetSelection(0)->GetType() != IT_FREESPACE
        // FIXME: Multi-select
        && GetSelection(0)->GetType() != IT_UNKNOWN
    );
}

void CDirstatDoc::OnExplorerHere()
{
    try
    {
        // FIXME: Multi-select
        const CItem *item = GetSelection(0);
        ASSERT(item != NULL);

        if(IT_MYCOMPUTER == item->GetType())
        {
            SHELLEXECUTEINFO sei;
            ZeroMemory(&sei, sizeof(sei));
            sei.cbSize = sizeof(sei);
            sei.hwnd = *AfxGetMainWnd();
            sei.lpVerb = L"explore";
            sei.nShow = SW_SHOWNORMAL;

            CCoTaskMem<LPITEMIDLIST> pidl;
            GetPidlOfMyComputer(&pidl);

            sei.lpIDList = pidl;
            sei.fMask |= SEE_MASK_IDLIST;

            ShellExecuteEx(&sei);
            // ShellExecuteEx seems to display its own MessageBox on error.
        }
        else
        {
            ShellExecuteThrow(*AfxGetMainWnd(), L"explore", item->GetFolderPath(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }
    catch (CException *pe)
    {
        pe->ReportError();
        pe->Delete();
    }
}

void CDirstatDoc::OnUpdateCommandPromptHere(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(
        DirectoryListHasFocus()
        // FIXME: Multi-select
        && GetSelection(0) != nullptr
        // FIXME: Multi-select
        && GetSelection(0)->GetType() != IT_MYCOMPUTER
        // FIXME: Multi-select
        && GetSelection(0)->GetType() != IT_FREESPACE
        // FIXME: Multi-select
        && GetSelection(0)->GetType() != IT_UNKNOWN
        // FIXME: Multi-select
        && ! GetSelection(0)->HasUncPath()
    );
}

void CDirstatDoc::OnCommandPromptHere()
{
    try
    {
        // FIXME: Multi-select
        const CItem *item = GetSelection(0);
        ASSERT(item != NULL);

        const CStringW cmd = GetCOMSPEC();

        ShellExecuteThrow(*AfxGetMainWnd(), L"open", cmd, nullptr, item->GetFolderPath(), SW_SHOWNORMAL);
    }
    catch (CException *pe)
    {
        pe->ReportError();
        pe->Delete();
    }
}

void CDirstatDoc::OnUpdateCleanupDeletetotrashbin(CCmdUI *pCmdUI)
{
    // FIXME: Multi-select
    const CItem *item = GetSelection(0);

    pCmdUI->Enable(
        DirectoryListHasFocus()
        && item != nullptr
        && (IT_DIRECTORY == item->GetType() || IT_FILE == item->GetType())
        && !item->IsRootItem()
    );
}

void CDirstatDoc::OnCleanupDeletetotrashbin()
{
    // FIXME: Multi-select
    CItem *item = GetSelection(0);

    if(nullptr == item || item->GetType() != IT_DIRECTORY && item->GetType() != IT_FILE || item->IsRootItem())
    {
        return;
    }

    if(DeletePhysicalItem(item, true))
    {
        RefreshRecyclers();
        UpdateAllViews(nullptr);
    }
}

void CDirstatDoc::OnUpdateCleanupDelete(CCmdUI *pCmdUI)
{
    // FIXME: Multi-select
    const CItem *item = GetSelection(0);

    pCmdUI->Enable(
        DirectoryListHasFocus()
        && item != nullptr
        && (IT_DIRECTORY == item->GetType() || IT_FILE == item->GetType())
        && !item->IsRootItem()
    );
}

void CDirstatDoc::OnCleanupDelete()
{
    // FIXME: Multi-select
    CItem *item = GetSelection(0);

    if(nullptr == item || item->GetType() != IT_DIRECTORY && item->GetType() != IT_FILE || item->IsRootItem())
    {
        return;
    }

    if(DeletePhysicalItem(item, false))
    {
        SetWorkingItem(GetRootItem());
        UpdateAllViews(nullptr);
    }
}

void CDirstatDoc::OnUpdateUserdefinedcleanup(CCmdUI *pCmdUI)
{
	const int i = pCmdUI->m_nID - ID_USERDEFINEDCLEANUP0;
    // FIXME: Multi-select
	const CItem *item = GetSelection(0);

    pCmdUI->Enable(
        DirectoryListHasFocus()
        && GetOptions()->IsUserDefinedCleanupEnabled(i)
        && UserDefinedCleanupWorksForItem(GetOptions()->GetUserDefinedCleanup(i), item)
    );
}

void CDirstatDoc::OnUserdefinedcleanup(UINT id)
{
    const USERDEFINEDCLEANUP *udc = GetOptions()->GetUserDefinedCleanup(id - ID_USERDEFINEDCLEANUP0);
    // FIXME: Multi-select
    CItem *item = GetSelection(0);

    ASSERT(UserDefinedCleanupWorksForItem(udc, item));
    if(!UserDefinedCleanupWorksForItem(udc, item))
    {
        return;
    }

    ASSERT(item != NULL);

    try
    {
        AskForConfirmation(udc, item);
        PerformUserDefinedCleanup(udc, item);
        RefreshAfterUserDefinedCleanup(udc, item);
    }
    catch (CUserException *pe)
    {
        pe->Delete();
    }
    catch (CException *pe)
    {
        pe->ReportError();
        pe->Delete();
    }
}

void CDirstatDoc::OnUpdateTreemapSelectparent(CCmdUI *pCmdUI)
{
    // FIXME: Multi-select
    pCmdUI->Enable(GetSelection(0) != nullptr && GetSelection(0)->GetParent() != nullptr);
}

void CDirstatDoc::OnTreemapSelectparent()
{
    // FIXME: Multi-select
    PushReselectChild(GetSelection(0));
    // FIXME: Multi-select
    const CItem *p = GetSelection(0)->GetParent();
    SetSelection(p, true);
    UpdateAllViews(nullptr, HINT_SHOWNEWSELECTION);
}

void CDirstatDoc::OnUpdateTreemapReselectchild(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(IsReselectChildAvailable());
}

void CDirstatDoc::OnTreemapReselectchild()
{
    CItem *item = PopReselectChild();
    SetSelection(item, true);
    UpdateAllViews(nullptr, HINT_SHOWNEWSELECTION, reinterpret_cast<CObject*>(item));
}

void CDirstatDoc::OnUpdateCleanupOpen(CCmdUI * /*pCmdUI*/)
{
// FIXME: Multi-select
//     pCmdUI->Enable(
//         DirectoryListHasFocus()
//         && GetSelection() != NULL
//         && GetSelection()->GetType() != IT_FILESFOLDER
//         && GetSelection()->GetType() != IT_FREESPACE
//         && GetSelection()->GetType() != IT_UNKNOWN
//     );
}

void CDirstatDoc::OnCleanupOpen()
{
// FIXME: Multi-select
//     const CItem *item = GetSelection();
//     ASSERT(item != NULL);
// 
//     OpenItem(item);
}

void CDirstatDoc::OnUpdateCleanupProperties(CCmdUI * /*pCmdUI*/)
{
// FIXME: Multi-select
//     pCmdUI->Enable(
//         DirectoryListHasFocus()
//         && GetSelection() != NULL
//         && GetSelection()->GetType() != IT_FREESPACE
//         && GetSelection()->GetType() != IT_UNKNOWN
//         && GetSelection()->GetType() != IT_FILESFOLDER
//     );
}

void CDirstatDoc::OnCleanupProperties()
{
// FIXME: Multi-select
//     try
//     {
//         SHELLEXECUTEINFO sei;
//         ZeroMemory(&sei, sizeof(sei));
//         sei.cbSize = sizeof(sei);
//         sei.hwnd = *AfxGetMainWnd();
//         sei.lpVerb = L"properties";
//         sei.fMask = SEE_MASK_INVOKEIDLIST;
// 
//         CCoTaskMem<LPITEMIDLIST> pidl;
//         CStringW path;
// 
//         const CItem *item = GetSelection();
//         ASSERT(item != NULL);
// 
//         switch (item->GetType())
//         {
//         case IT_MYCOMPUTER:
//             {
//                 GetPidlOfMyComputer(&pidl);
//                 sei.lpIDList = pidl;
//                 sei.fMask |= SEE_MASK_IDLIST;
//             }
//             break;
// 
//         case IT_DRIVE:
//         case IT_DIRECTORY:
//             {
//                 path = item->GetFolderPath();
//                 sei.lpFile = path; // Must not be a temporary variable
//             }
//             break;
// 
//         case IT_FILE:
//             {
//                 path = item->GetPath();
//                 sei.lpFile = path; // Must not be temporary variable
//             }
//             break;
// 
//         default:
//             {
//                 ASSERT(0);
//             }
//         }
// 
//         ShellExecuteEx(&sei);
//         // BUGBUG: ShellExecuteEx seems to display its own MessageBox on error.
//     }
//     catch (CException *pe)
//     {
//         pe->ReportError();
//         pe->Delete();
//     }
}

// CDirstatDoc Diagnostics
#ifdef _DEBUG
void CDirstatDoc::AssertValid() const
{
    CDocument::AssertValid();
}

void CDirstatDoc::Dump(CDumpContext& dc) const
{
    CDocument::Dump(dc);
}
#endif //_DEBUG
