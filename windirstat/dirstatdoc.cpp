// dirstatdoc.cpp - Implementation of CDirstatDoc
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2006, 2008 Oliver Schneider (assarbad.net)
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
// Author(s): - bseifert -> http://windirstat.info/contact/bernhard/
//            - assarbad -> http://windirstat.info/contact/oliver/
//
// $Id$

#include "stdafx.h"
#include "windirstat.h"
#include "item.h"
#include "mainframe.h"
#include "osspecific.h"
#include "deletewarningdlg.h"
#include "modalshellapi.h"
#include "dirstatdoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    const COLORREF _cushionColors[] = {
        RGB(0, 0, 255),
        RGB(255, 0, 0),
        RGB(0, 255, 0),
        RGB(0, 255, 255),
        RGB(255, 0, 255),
        RGB(255, 255, 0),
        RGB(150, 150, 255),
        RGB(255, 150, 150),
        RGB(150, 255, 150),
        RGB(150, 255, 255),
        RGB(255, 150, 255),
        RGB(255, 255, 150),
        RGB(255, 255, 255)
    };
}

CDirstatDoc *_theDocument;

CDirstatDoc *GetDocument()
{
    return _theDocument;
}


IMPLEMENT_DYNCREATE(CDirstatDoc, CDocument)


CDirstatDoc::CDirstatDoc()
{
    ASSERT(NULL == _theDocument);
    _theDocument = this;
    m_rootItem = NULL;
    m_workingItem = NULL;
    m_zoomItem = NULL;

    m_showFreeSpace = CPersistence::GetShowFreeSpace();
    m_showUnknown = CPersistence::GetShowUnknown();
    m_extensionDataValid = false;

    VTRACE(_T("sizeof(CItem) = %d"), sizeof(CItem));
}

CDirstatDoc::~CDirstatDoc()
{
    CPersistence::SetShowFreeSpace(m_showFreeSpace);
    CPersistence::SetShowUnknown(m_showUnknown);

    delete m_rootItem;
    _theDocument = NULL;
}

// Encodes a selection from the CSelectDrivesDlg into a string which can be routed as a pseudo
// document "path" through MFC and finally arrives in OnOpenDocument().
//
CString CDirstatDoc::EncodeSelection(RADIO radio, CString folder, const CStringArray& drives)
{
    CString ret;
    switch (radio)
    {
    case RADIO_ALLLOCALDRIVES:
    case RADIO_SOMEDRIVES:
        {
            for(int i = 0; i < drives.GetSize(); i++)
            {
                if(i > 0)
                {
                    ret += CString(GetEncodingSeparator());
                }
                ret += drives[i];
            }
        }
        break;

    case RADIO_AFOLDER:
        {
            ret.Format(_T("%s"), folder);
        }
        break;
    }
    return ret;
}

// The inverse of EncodeSelection
//
void CDirstatDoc::DecodeSelection(CString s, CString& folder, CStringArray& drives)
{
    folder.Empty();
    drives.RemoveAll();

    // s is either something like "C:\programme"
    // or something like "C:|D:|E:".

    CStringArray sa;
    int i = 0;

    while(i < s.GetLength())
    {
        CString token;
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
        for(int i = 0; i < sa.GetSize(); i++)
        {
            CString d = sa[i];
            ASSERT(2 == d.GetLength());
            ASSERT(wds::chrColon == d[1]);

            drives.Add(d + _T("\\"));
        }
    }
    else
    {
        CString f = sa[0];
        if(2 == f.GetLength() && wds::chrColon == f[1])
        {
            drives.Add(f + _T("\\"));
        }
        else
        {
            // Remove trailing backslash, if any and not drive-root.
            if((f.GetLength() > 0) && (wds::strBackslash == f.Right(1)) && (f.GetLength() != 3 || f[1] != wds::chrColon))
            {
                f = f.Left(f.GetLength() - 1);
            }

            folder = f;
        }
    }
}

TCHAR CDirstatDoc::GetEncodingSeparator()
{
    return wds::chrPipe; // This character must be one, which is not allowed in file names.
}

void CDirstatDoc::DeleteContents()
{
    delete m_rootItem;
    m_rootItem = NULL;
    SetWorkingItem(NULL);
    m_zoomItem = NULL;
    m_selectedItems.RemoveAll();
    GetWDSApp()->ReReadMountPoints();
}

BOOL CDirstatDoc::OnNewDocument()
{
    if(!CDocument::OnNewDocument())
    {
        return FALSE;
    }

    UpdateAllViews(NULL, HINT_NEWROOT);
    return TRUE;
}

BOOL CDirstatDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
    CDocument::OnNewDocument(); // --> DeleteContents()

    CString spec = lpszPathName;
    CString folder;
    CStringArray drives;
    DecodeSelection(spec, folder, drives);

    CStringArray rootFolders;
    if(drives.GetSize() > 0)
    {
        m_showMyComputer = (drives.GetSize() > 1);
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
        m_rootItem = new CItem((ITEMTYPE)(IT_MYCOMPUTER | ITF_ROOTITEM), LoadString(IDS_MYCOMPUTER));
        for(int i = 0; i < rootFolders.GetSize(); i++)
        {
            CItem *drive = new CItem(IT_DRIVE, rootFolders[i]);
            driveItems.Add(drive);
            m_rootItem->AddChild(drive);
        }
    }
    else
    {
        ITEMTYPE type = IsDrive(rootFolders[0]) ? IT_DRIVE : IT_DIRECTORY;
        m_rootItem = new CItem((ITEMTYPE)(type|ITF_ROOTITEM), rootFolders[0], false);
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

    UpdateAllViews(NULL, HINT_NEWROOT);
    return true;
}

// We don't want MFCs AfxFullPath()-Logic, because lpszPathName
// is not a path. So we have overridden this.
//
void CDirstatDoc::SetPathName(LPCTSTR lpszPathName, BOOL /*bAddToMRU*/)
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
void CDirstatDoc::SetTitlePrefix(CString prefix)
{
    CString docName = prefix + GetTitle();
    GetMainFrame()->UpdateFrameTitleForDocument(docName);
}

COLORREF CDirstatDoc::GetCushionColor(LPCTSTR ext)
{
    SExtensionRecord r;
    VERIFY(GetExtensionData()->Lookup(ext, r));
    return r.color;
}

COLORREF CDirstatDoc::GetZoomColor()
{
    return RGB(0,0,255);
}

bool CDirstatDoc::OptionShowFreeSpace()
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
    m_rootItem = NULL;

    m_zoomItem = NULL;
    m_selectedItems.RemoveAll();
}

// This method does some work for ticks ms.
// return: true if done or suspended.
//
bool CDirstatDoc::Work(DWORD ticks)
{
    if(NULL == m_rootItem)
    {
        return true;
    }

    if(GetMainFrame()->IsProgressSuspended())
    {
        return true;
    }

    if(!m_rootItem->IsDone())
    {
        m_rootItem->DoSomeWork(ticks);
        if(m_rootItem->IsDone())
        {
            m_extensionDataValid = false;

            GetMainFrame()->SetProgressPos100();
            GetMainFrame()->RestoreTypeView();
            GetMainFrame()->RestoreGraphView();

            UpdateAllViews(NULL);
        }
        else
        {
            ASSERT(m_workingItem != NULL);
            if(m_workingItem != NULL) // to be honest, "defensive programming" is stupid, but c'est la vie: it's safer.
            {
                GetMainFrame()->SetProgressPos(m_workingItem->GetProgressPos());
            }

            UpdateAllViews(NULL, HINT_SOMEWORKDONE);
        }

    }
    if(m_rootItem->IsDone())
    {
        SetWorkingItem(NULL);
        return true;
    }
    else
    {
        return false;
    }
}

bool CDirstatDoc::IsDrive(CString spec)
{
    return (3 == spec.GetLength() && wds::chrColon == spec[1] && wds::chrBackslash == spec[2]);
}

// Starts a refresh of all mount points in our tree.
// Called when the user changes the follow mount points option.
//
void CDirstatDoc::RefreshMountPointItems()
{
    CWaitCursor wc;

    CItem *root = GetRootItem();
    if(NULL == root)
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
    if(NULL == root)
    {
        return;
    }

    RecurseRefreshJunctionItems(root);
}

bool CDirstatDoc::IsRootDone()
{
    return m_rootItem != NULL && m_rootItem->IsDone();
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
    CItem *item = m_selectedItems[0];
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
    CItem *parent = GetSelectionParent();
    for (int i=0; i < m_selectedItems.GetCount(); i++)
        ASSERT(m_selectedItems[i]->GetParent() == parent);
}
#endif

void CDirstatDoc::SetSelection(const CItem * /*item*/, bool /*keepReselectChildStack*/)
{
}

CItem *CDirstatDoc::GetSelection(size_t i)
{
    return m_selectedItems.GetCount() ? m_selectedItems[i] : 0;
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

void CDirstatDoc::SetHighlightExtension(LPCTSTR ext)
{
    m_highlightExtension = ext;
    GetMainFrame()->SetSelectionMessageText();
}

CString CDirstatDoc::GetHighlightExtension()
{
    return m_highlightExtension;
}

// The very root has been deleted.
//
void CDirstatDoc::UnlinkRoot()
{
    DeleteContents();
    UpdateAllViews(NULL, HINT_NEWROOT);
}

// Determines, whether an UDC works for a given item.
//
bool CDirstatDoc::UserDefinedCleanupWorksForItem(const USERDEFINEDCLEANUP *udc, const CItem *item)
{
    bool works = false;

    if(item != NULL)
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
    if(m_workingItem != NULL)
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
        CString path;

        switch (item->GetType())
        {
        case IT_MYCOMPUTER:
            {
                SHELLEXECUTEINFO sei;
                ZeroMemory(&sei, sizeof(sei));
                sei.cbSize = sizeof(sei);
                sei.hwnd = *AfxGetMainWnd();
                sei.lpVerb = _T("open");
                //sei.fMask = SEE_MASK_INVOKEIDLIST;
                sei.nShow = SW_SHOWNORMAL;
                CCoTaskMem<LPITEMIDLIST> pidl;

                GetPidlOfMyComputer(&pidl);
                sei.lpIDList = pidl;
                sei.fMask |= SEE_MASK_IDLIST;

                ShellExecuteEx(&sei);
                // ShellExecuteEx seems to display its own Messagebox, if failed.

                return;
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
    if(IT_DIRECTORY == item->GetType() && item != GetRootItem() && GetWDSApp()->IsFolderJunction(item->GetPath()))
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

    if(NULL == root)
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
    while(pos != NULL)
    {
        CString ext;
        SExtensionRecord r;
        m_extensionData.GetNextAssoc(pos, ext, r);

        sortedExtensions[i++]= ext;
    }

    _pqsortExtensionData = &m_extensionData;
    qsort(sortedExtensions.GetData(), sortedExtensions.GetSize(), sizeof(CString), &_compareExtensions);
    _pqsortExtensionData = NULL;
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
    CString *ext1 = (CString *)item1;
    CString *ext2 = (CString *)item2;
    SExtensionRecord r1;
    SExtensionRecord r2;
    VERIFY(_pqsortExtensionData->Lookup(*ext1, r1));
    VERIFY(_pqsortExtensionData->Lookup(*ext2, r2));
    return signum(r2.bytes - r1.bytes);
}

void CDirstatDoc::SetWorkingItemAncestor(CItem *item)
{
    if(m_workingItem != NULL)
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
    if(GetMainFrame() != NULL)
    {
        if(item != NULL)
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
    UpdateAllViews(NULL, HINT_ZOOMCHANGED);
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
        UpdateAllViews(NULL, HINT_SELECTIONCHANGED);
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
            UpdateAllViews(NULL, HINT_SELECTIONCHANGED);
        }
        if(m_workingItem == item)
        {
            SetWorkingItem(parent);
        }
    }

    UpdateAllViews(NULL);
}

// UDC confirmation Dialog.
//
void CDirstatDoc::AskForConfirmation(const USERDEFINEDCLEANUP *udc, CItem *item)
{
    if(!udc->askForConfirmation)
    {
        return;
    }

    CString msg;
    msg.FormatMessage(udc->recurseIntoSubdirectories ? IDS_RUDC_CONFIRMATIONss : IDS_UDC_CONFIRMATIONss, udc->title, item->GetPath());

    if(IDYES != AfxMessageBox(msg, MB_YESNO))
    {
        AfxThrowUserException();
    }
}

void CDirstatDoc::PerformUserDefinedCleanup(const USERDEFINEDCLEANUP *udc, CItem *item)
{
    CWaitCursor wc;

    CString path = item->GetPath();

    bool isDirectory = IT_DRIVE == item->GetType() || IT_DIRECTORY == item->GetType() || IT_FILESFOLDER == item->GetType();

    // Verify that path still exists
    if(isDirectory)
    {
        if(!FolderExists(path) && !DriveExists(path))
        {
            MdThrowStringExceptionF(IDS_THEDIRECTORYsDOESNOTEXIST, path);
        }
    }
    else
    {
        ASSERT(IT_FILE == item->GetType());

        if(!FileExists(path))
        {
            MdThrowStringExceptionF(IDS_THEFILEsDOESNOTEXIST, path);
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
            RefreshItem(NULL == item->GetParent() ? item : item->GetParent());
        }
        break;

    // case RP_ASSUME_ENTRY_HAS_BEEN_DELETED:
    // Feature not implemented.
    // break;

    default:
        ASSERT(0);
    }
}

void CDirstatDoc::RecursiveUserDefinedCleanup(const USERDEFINEDCLEANUP *udc, const CString& rootPath, const CString& currentPath)
{
    // (Depth first.)

    CFileFind finder;
    BOOL b = finder.FindFile(currentPath + _T("\\*.*"));
    while(b)
    {
        b = finder.FindNextFile();
        if((finder.IsDots()) || (!finder.IsDirectory()))
        {
            continue;
        }
        if(GetWDSApp()->IsVolumeMountPoint(finder.GetFilePath()) && !GetOptions()->IsFollowMountPoints())
        {
            continue;
        }
        if(GetWDSApp()->IsFolderJunction(finder.GetFilePath()) && !GetOptions()->IsFollowJunctionPoints())
        {
            continue;
        }

        RecursiveUserDefinedCleanup(udc, rootPath, finder.GetFilePath());
    }

    CallUserDefinedCleanup(true, udc->commandLine, rootPath, currentPath, udc->showConsoleWindow, true);
}

void CDirstatDoc::CallUserDefinedCleanup(bool isDirectory, const CString& format, const CString& rootPath, const CString& currentPath, bool showConsoleWindow, bool wait)
{
    CString userCommandLine = BuildUserDefinedCleanupCommandLine(format, rootPath, currentPath);

    CString app = GetCOMSPEC();
    CString cmdline;
    cmdline.Format(_T("%s /C %s"), GetBaseNameFromPath(app), userCommandLine);
    CString directory = isDirectory ? currentPath : GetFolderNameFromPath(currentPath);

    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = showConsoleWindow ? SW_SHOWNORMAL : SW_HIDE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    BOOL b = CreateProcess(
        app,
        cmdline.GetBuffer(),
        NULL,
        NULL,
        false,
        0,
        NULL,
        directory,
        &si,
        &pi
    );
    cmdline.ReleaseBuffer();
    if(!b)
    {
        MdThrowStringExceptionF(IDS_COULDNOTCREATEPROCESSssss,
            app, cmdline, directory, MdGetWinErrorText(::GetLastError())
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


CString CDirstatDoc::BuildUserDefinedCleanupCommandLine(LPCTSTR format, LPCTSTR rootPath, LPCTSTR currentPath)
{
    CString rootName = GetBaseNameFromPath(rootPath);
    CString currentName = GetBaseNameFromPath(currentPath);

    CString s = format;

    // Because file names can contain "%", we first replace our placeholders with
    // strings which contain a forbidden character.
    s.Replace(_T("%p"), _T(">p"));
    s.Replace(_T("%n"), _T(">n"));
    s.Replace(_T("%sp"), _T(">sp"));
    s.Replace(_T("%sn"), _T(">sn"));

    // Now substitute
    s.Replace(_T(">p"), rootPath);
    s.Replace(_T(">n"), rootName);
    s.Replace(_T(">sp"), currentPath);
    s.Replace(_T(">sn"), currentName);

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
    return (LF_DIRECTORYLIST == GetMainFrame()->GetLogicalFocus());
}

BEGIN_MESSAGE_MAP(CDirstatDoc, CDocument)
    ON_COMMAND(ID_REFRESHSELECTED, OnRefreshselected)
    ON_UPDATE_COMMAND_UI(ID_REFRESHSELECTED, OnUpdateRefreshselected)
    ON_COMMAND(ID_REFRESHALL, OnRefreshall)
    ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
    ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
    ON_COMMAND(ID_CLEANUP_EMPTYRECYCLEBIN, OnCleanupEmptyrecyclebin)
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
        && GetSelection(0) != NULL
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
    pCmdUI->Enable(GetRootItem() != NULL);
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
        item != NULL &&
        item->GetType() != IT_MYCOMPUTER &&
        item->GetType() != IT_FILESFOLDER &&
        item->GetType() != IT_FREESPACE &&
        item->GetType() != IT_UNKNOWN
    );
}

void CDirstatDoc::OnEditCopy()
{
    CString paths;
    for (size_t i = 0; i < GetSelectionCount(); i++)
    {
        if (i > 0)
            paths += _T("\r\n");

        paths += GetSelection(i)->GetPath();
    }

    // FIXME: Need to fix the clipboard code!!!
    AfxMessageBox(paths);
}

void CDirstatDoc::OnCleanupEmptyrecyclebin()
{
    CModalShellApi msa;

    if(!msa.IsRecycleBinApiSupported())
    {
        return;
    }

    msa.EmptyRecycleBin();

    RefreshRecyclers();
    UpdateAllViews(NULL);
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
            CItem *free = drives[i]->FindFreeSpaceItem();
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

    UpdateAllViews(NULL);
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
            CItem *unknown = drives[i]->FindUnknownItem();
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

    UpdateAllViews(NULL);
}

void CDirstatDoc::OnUpdateTreemapZoomin(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(
        m_rootItem != NULL && m_rootItem->IsDone()
        // FIXME: Multi-select (2x)
        && GetSelection(0) != NULL && GetSelection(0) != GetZoomItem()
    );
}

void CDirstatDoc::OnTreemapZoomin()
{
    // FIXME: Multi-select
    CItem *p = GetSelection(0);
    CItem *z = NULL;
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
        m_rootItem != NULL && m_rootItem->IsDone()
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
        && GetSelection(0) != NULL
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
            sei.lpVerb = _T("explore");
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
            ShellExecuteThrow(*AfxGetMainWnd(), _T("explore"), item->GetFolderPath(), NULL, NULL, SW_SHOWNORMAL);
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
        && GetSelection(0) != NULL
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
        CItem *item = GetSelection(0);
        ASSERT(item != NULL);

        CString cmd = GetCOMSPEC();

        ShellExecuteThrow(*AfxGetMainWnd(), _T("open"), cmd, NULL, item->GetFolderPath(), SW_SHOWNORMAL);
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
    CItem *item = GetSelection(0);

    pCmdUI->Enable(
        DirectoryListHasFocus()
        && item != NULL
        && (IT_DIRECTORY == item->GetType() || IT_FILE == item->GetType())
        && !item->IsRootItem()
    );
}

void CDirstatDoc::OnCleanupDeletetotrashbin()
{
    // FIXME: Multi-select
    CItem *item = GetSelection(0);

    if(NULL == item || item->GetType() != IT_DIRECTORY && item->GetType() != IT_FILE || item->IsRootItem())
    {
        return;
    }

    if(DeletePhysicalItem(item, true))
    {
        RefreshRecyclers();
        UpdateAllViews(NULL);
    }
}

void CDirstatDoc::OnUpdateCleanupDelete(CCmdUI *pCmdUI)
{
    // FIXME: Multi-select
    CItem *item = GetSelection(0);

    pCmdUI->Enable(
        DirectoryListHasFocus()
        && item != NULL
        && (IT_DIRECTORY == item->GetType() || IT_FILE == item->GetType())
        && !item->IsRootItem()
    );
}

void CDirstatDoc::OnCleanupDelete()
{
    // FIXME: Multi-select
    CItem *item = GetSelection(0);

    if(NULL == item || item->GetType() != IT_DIRECTORY && item->GetType() != IT_FILE || item->IsRootItem())
    {
        return;
    }

    if(DeletePhysicalItem(item, false))
    {
        SetWorkingItem(GetRootItem());
        UpdateAllViews(NULL);
    }
}

void CDirstatDoc::OnUpdateUserdefinedcleanup(CCmdUI *pCmdUI)
{
    int i = pCmdUI->m_nID - ID_USERDEFINEDCLEANUP0;
    // FIXME: Multi-select
    CItem *item = GetSelection(0);

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
    pCmdUI->Enable(GetSelection(0) != NULL && GetSelection(0)->GetParent() != NULL);
}

void CDirstatDoc::OnTreemapSelectparent()
{
    // FIXME: Multi-select
    PushReselectChild(GetSelection(0));
    // FIXME: Multi-select
    CItem *p = GetSelection(0)->GetParent();
    SetSelection(p, true);
    UpdateAllViews(NULL, HINT_SHOWNEWSELECTION);
}

void CDirstatDoc::OnUpdateTreemapReselectchild(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(IsReselectChildAvailable());
}

void CDirstatDoc::OnTreemapReselectchild()
{
    CItem *item = PopReselectChild();
    SetSelection(item, true);
    UpdateAllViews(NULL, HINT_SHOWNEWSELECTION, reinterpret_cast<CObject*>(item));
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
//         sei.lpVerb = _T("properties");
//         sei.fMask = SEE_MASK_INVOKEIDLIST;
// 
//         CCoTaskMem<LPITEMIDLIST> pidl;
//         CString path;
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
