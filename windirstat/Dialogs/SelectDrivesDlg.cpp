// SelectDrivesDlg.cpp - Implementation of CDriveItem, CDrivesList and CSelectDrivesDlg
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
#include <common/SmartPointer.h>
#include <common/CommonHelpers.h>
#include "Options.h"
#include "GlobalHelpers.h"
#include "SelectDrivesDlg.h"

namespace
{
    enum
    {
        COL_NAME,
        COL_TOTAL,
        COL_FREE,
        COL_GRAPH,
        COL_PERCENTUSED,
        COLUMN_COUNT
    };

    constexpr UINT WMU_OK = WM_USER + 100;

    UINT WMU_THREADFINISHED = ::RegisterWindowMessage(L"{F03D3293-86E0-4c87-B559-5FD103F5AF58}");

    // Return: false, if drive not accessible
    bool RetrieveDriveInformation(LPCWSTR path, CStringW& name, ULONGLONG& total, ULONGLONG& free)
    {
        CStringW volumeName;

        if (!GetVolumeName(path, volumeName))
        {
            return false;
        }

        name = FormatVolumeName(path, volumeName);

        if (!CDirStatApp::getDiskFreeSpace(path, total, free))
        {
            return false;
        }

        // This condition *can* become true if quotas exist!
        //ASSERT(free <= total);

        return true;
    }
}

/////////////////////////////////////////////////////////////////////////////

CDriveItem::CDriveItem(CDrivesList* list, LPCWSTR pszPath)
    : m_list(list)
      , m_path(pszPath)
      , m_isRemote(DRIVE_REMOTE == ::GetDriveType(m_path))
      , m_querying(true)
      , m_success(false)
      , m_name(m_path)
      , m_totalBytes(0)
      , m_freeBytes(0)
      , m_used(0)
{
    /*
    For local drives we could do this synchronously:
        if(!m_isRemote)
        {
            m_querying = false;

            CStringW name = m_name;
            ULONGLONG total = 0;
            ULONGLONG free = 0;

            bool success = RetrieveDriveInformation(m_path, name, total, free);
            SetDriveInformation(success, name, total, free);
        }
    */
}

void CDriveItem::StartQuery(HWND dialog, UINT serial)
{
    ASSERT(dialog != NULL);

    ASSERT(m_querying); // The synchronous query in the constructor is commented out.

    if (m_querying)
    {
        new CDriveInformationThread(m_path, (LPARAM)this, dialog, serial);
        // (will delete itself when finished.)
    }
}

void CDriveItem::SetDriveInformation(bool success, LPCWSTR name, ULONGLONG total, ULONGLONG free)
{
    m_querying = false;
    m_success  = success;

    if (m_success)
    {
        m_name       = name;
        m_totalBytes = total;
        m_freeBytes  = free;
        m_used       = 0.0;

        // guard against cases where free bytes might be limited (.e.g, quotas)
        if (m_totalBytes > 0 && m_totalBytes >= m_freeBytes)
        {
            m_used = static_cast<double>(m_totalBytes - m_freeBytes) / m_totalBytes;
        }
    }
}

bool CDriveItem::IsRemote() const
{
    return m_isRemote;
}

bool CDriveItem::IsSUBSTed() const
{
    return IsSUBSTedDrive(m_path);
}

int CDriveItem::Compare(const CSortingListItem* baseOther, int subitem) const
{
    const CDriveItem* other = (CDriveItem*)baseOther;

    int r = 0;

    switch (subitem)
    {
    case COL_NAME:
        {
            r = signum(GetPath().CompareNoCase(other->GetPath()));
        }
        break;
    case COL_TOTAL:
        {
            r = usignum(m_totalBytes, other->m_totalBytes);
        }
        break;
    case COL_FREE:
        {
            r = usignum(m_freeBytes, other->m_freeBytes);
        }
        break;
    case COL_GRAPH:
    case COL_PERCENTUSED:
        {
            r = signum(m_used - other->m_used);
        }
        break;
    default:
        {
            ASSERT(0);
        }
    }

    return r;
}

int CDriveItem::GetImage() const
{
    return GetMyImageList()->getFileImage(m_path);
}

bool CDriveItem::DrawSubitem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) const
{
    if (subitem == COL_NAME)
    {
        DrawLabel(m_list, GetMyImageList(), pdc, rc, state, width, focusLeft);
        return true;
    }

    if (subitem == COL_GRAPH)
    {
        if (!m_success)
        {
            return false;
        }

        if (width != nullptr)
        {
            *width = 100;
            return true;
        }

        DrawSelection(m_list, pdc, rc, state);

        rc.DeflateRect(3, 5);

        DrawPercentage(pdc, rc, m_used, RGB(0, 0, 170));

        return true;
    }

    return false;
}

CStringW CDriveItem::GetText(int subitem) const
{
    CStringW s;

    switch (subitem)
    {
    case COL_NAME:
        {
            s = m_name;
        }
        break;

    case COL_TOTAL:
        if (m_success)
        {
            s = FormatBytes(m_totalBytes);
        }
        break;

    case COL_FREE:
        if (m_success)
        {
            s = FormatBytes(m_freeBytes);
        }
        break;

    case COL_GRAPH:
        if (m_querying)
        {
            VERIFY(s.LoadString(IDS_QUERYING));
        }
        else if (!m_success)
        {
            VERIFY(s.LoadString(IDS_NOTACCESSIBLE));
        }
        break;

    case COL_PERCENTUSED:
        if (m_success)
        {
            s = FormatDouble(m_used * 100) + L"%";
        }
        break;

    default:
        ASSERT(0);
    }

    return s;
}

CStringW CDriveItem::GetPath() const
{
    return m_path;
}

CStringW CDriveItem::GetDrive() const
{
    return m_path.Left(2);
}

/////////////////////////////////////////////////////////////////////////////


std::unordered_set<CDriveInformationThread*> CDriveInformationThread::_runningThreads;
std::shared_mutex CDriveInformationThread::_mutexRunningThreads;

void CDriveInformationThread::AddRunningThread()
{
    std::lock_guard lock(_mutexRunningThreads);
    _runningThreads.insert(this);
}

void CDriveInformationThread::RemoveRunningThread()
{
    std::lock_guard lock(_mutexRunningThreads);
    _runningThreads.erase(this);
}

// This static method is called by the dialog when the dialog gets closed.
// We set the m_dialog members of all running threads to null, so that
// they don't send messages around to a no-more-existing window.
//
void CDriveInformationThread::InvalidateDialogHandle()
{
    std::lock_guard lock(_mutexRunningThreads);
    for (const auto & thread : _runningThreads)
    {
        std::lock_guard lockd(thread->m_mutex);
        thread->m_dialog = nullptr;
    }
}

// The constructor starts the thread.
//
CDriveInformationThread::CDriveInformationThread(LPCWSTR path, LPARAM driveItem, HWND dialog, UINT serial)
    : m_path(path)
      , m_driveItem(driveItem)
      , m_dialog(dialog)
      , m_serial(serial)
      , m_totalBytes(0)
      , m_freeBytes(0)
      , m_success(false)
{
    ASSERT(m_bAutoDelete);

    AddRunningThread();

    VERIFY(CreateThread());
}

BOOL CDriveInformationThread::InitInstance()
{
    m_success = RetrieveDriveInformation(m_path, m_name, m_totalBytes, m_freeBytes);

    HWND dialog = nullptr;

    {
        std::lock_guard lock(m_mutex);
        dialog = m_dialog;
    }

    if (dialog != nullptr)
    {
        // Theoretically the dialog may have been closed at this point.
        // SendMessage() to a non-existing window simply fails immediately.
        // If in the meantime the system recycled the window handle,
        // (it may even belong to another process now?!),
        // we are safe, because WMU_THREADFINISHED is a unique registered message.
        // (Well if the other process crashes because of our message, there is nothing we can do about it.)
        // If the window handle is recycled by a new Select drives dialog,
        // its new serial will prevent it from reacting.
        ::SendMessage(dialog, WMU_THREADFINISHED, m_serial, (LPARAM)this);
    }

    RemoveRunningThread();

    ASSERT(m_bAutoDelete); // Object will delete itself.
    return false;          // no Run(), please!
}

// This method is only called by the gui thread, while we hang
// in SendMessage(dialog, WMU_THREADFINISHED, 0, this).
// So we need no synchronization.
//
LPARAM CDriveInformationThread::GetDriveInformation(bool& success, CStringW& name, ULONGLONG& total, ULONGLONG& free) const
{
    name    = m_name;
    total   = m_totalBytes;
    free    = m_freeBytes;
    success = m_success;

    return m_driveItem;
}

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CDrivesList, COwnerDrawnListControl)

// TODO: Persist Settings?
CDrivesList::CDrivesList()
    : COwnerDrawnListControl(20, COptions::DriveListColumnOrder.Ptr(), COptions::DriveListColumnWidths.Ptr())
{
}

CDriveItem* CDrivesList::GetItem(int i) const
{
    return (CDriveItem*)GetItemData(i);
}

bool CDrivesList::HasImages()
{
    return true;
}

void CDrivesList::SelectItem(CDriveItem* item)
{
    const int i = FindListItem(item);
    SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
}

bool CDrivesList::IsItemSelected(int i)
{
    return LVIS_SELECTED == GetItemState(i, LVIS_SELECTED);
}

void CDrivesList::OnLButtonDown(UINT /*nFlags*/, CPoint /*point*/)
{
    if (GetFocus() == this || GetSelectedCount() == 0)
    {
        // We simulate Ctrl-Key-Down here, so that the dialog
        // can be driven with one hand (mouse) only.
        const MSG* msg = GetCurrentMessage();
        DefWindowProc(msg->message, msg->wParam | MK_CONTROL, msg->lParam);
    }
    else
    {
        SetFocus();

        // Send a LVN_ITEMCHANGED to the parent, so that it can
        // update the radio button.
        NMLISTVIEW lv;
        ZeroMemory(&lv, sizeof(lv));
        lv.hdr.hwndFrom = m_hWnd;
        lv.hdr.idFrom   = GetDlgCtrlID();
#pragma warning(suppress: 26454)
        lv.hdr.code = LVN_ITEMCHANGED;
        GetParent()->SendMessage(WM_NOTIFY, GetDlgCtrlID(), (LPARAM)&lv);

        // no further action
    }
}

void CDrivesList::OnNMDblclk(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    *pResult = 0;

    CPoint point = GetCurrentMessage()->pt;
    ScreenToClient(&point);
    const int i = HitTest(point);
    if (i == -1)
    {
        return;
    }

    for (int k = 0; k < GetItemCount(); k++)
    {
        SetItemState(k, k == i ? LVIS_SELECTED : 0, LVIS_SELECTED);
    }

    GetParent()->SendMessage(WMU_OK);
}

BEGIN_MESSAGE_MAP(CDrivesList, COwnerDrawnListControl)
    ON_WM_LBUTTONDOWN()
#pragma warning(suppress: 26454)
    ON_NOTIFY_REFLECT(LVN_DELETEITEM, OnLvnDeleteitem)
    ON_WM_MEASUREITEM_REFLECT()
#pragma warning(suppress: 26454)
    ON_NOTIFY_REFLECT(NM_DBLCLK, OnNMDblclk)
END_MESSAGE_MAP()

void CDrivesList::OnLvnDeleteitem(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    delete GetItem(pNMLV->iItem);
    *pResult = 0;
}

void CDrivesList::MeasureItem(LPMEASUREITEMSTRUCT mis)
{
    mis->itemHeight = GetRowHeight();
}

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CSelectDrivesDlg, CDialog)

UINT CSelectDrivesDlg::_serial;

CSelectDrivesDlg::CSelectDrivesDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CSelectDrivesDlg::IDD, pParent)
      , m_radio(0), m_layout(this, COptions::DriveWindowRect.Ptr())
{
    _serial++;
}

void CSelectDrivesDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_DRIVES, m_list);
    DDX_Radio(pDX, IDC_ALLDRIVES, m_radio);
    DDX_Text(pDX, IDC_FOLDERNAME, m_folderName);
    DDX_Control(pDX, IDOK, m_okButton);
}

BEGIN_MESSAGE_MAP(CSelectDrivesDlg, CDialog)
    ON_BN_CLICKED(IDC_BROWSEFOLDER, OnBnClickedBrowsefolder)
    ON_BN_CLICKED(IDC_AFOLDER, OnBnClickedAfolder)
    ON_BN_CLICKED(IDC_SOMEDRIVES, OnBnClickedSomedrives)
    ON_EN_CHANGE(IDC_FOLDERNAME, OnEnChangeFoldername)
    ON_WM_MEASUREITEM()
#pragma warning(suppress: 26454)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_DRIVES, OnLvnItemchangedDrives)
    ON_BN_CLICKED(IDC_ALLLOCALDRIVES, OnBnClickedAlllocaldrives)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_DESTROY()
    ON_MESSAGE(WMU_OK, OnWmuOk)
    ON_REGISTERED_MESSAGE(WMU_THREADFINISHED, OnWmuThreadFinished)
    ON_WM_SYSCOLORCHANGE()
END_MESSAGE_MAP()

BOOL CSelectDrivesDlg::OnInitDialog()
{
    CWaitCursor wc;

    CDialog::OnInitDialog();

    if (WMU_THREADFINISHED == 0)
    {
        VTRACE(L"RegisterMessage() failed. Using WM_USER + 123");
        WMU_THREADFINISHED = WM_USER + 123;
    }

    ModifyStyle(0, WS_CLIPCHILDREN);

    m_layout.AddControl(IDOK, 1, 0, 0, 0);
    m_layout.AddControl(IDCANCEL, 1, 0, 0, 0);
    m_layout.AddControl(IDC_DRIVES, 0, 0, 1, 1);
    m_layout.AddControl(IDC_AFOLDER, 0, 1, 0, 0);
    m_layout.AddControl(IDC_FOLDERNAME, 0, 1, 1, 0);
    m_layout.AddControl(IDC_BROWSEFOLDER, 1, 1, 0, 0);

    m_layout.OnInitDialog(true);

    m_list.ShowGrid(COptions::ListGrid);
    m_list.ShowStripes(COptions::ListStripes);
    m_list.ShowFullRowSelection(COptions::ListFullRowSelection);

    m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_HEADERDRAGDROP);
    // If we set an ImageList here, OnMeasureItem will have no effect ?!

    m_list.InsertColumn(COL_NAME, LoadString(IDS_DRIVECOL_NAME), LVCFMT_LEFT, 120, COL_NAME);
    m_list.InsertColumn(COL_TOTAL, LoadString(IDS_DRIVECOL_TOTAL), LVCFMT_RIGHT, 55, COL_TOTAL);
    m_list.InsertColumn(COL_FREE, LoadString(IDS_DRIVECOL_FREE), LVCFMT_RIGHT, 55, COL_FREE);
    m_list.InsertColumn(COL_GRAPH, LoadString(IDS_DRIVECOL_GRAPH), LVCFMT_LEFT, 100, COL_GRAPH);
    m_list.InsertColumn(COL_PERCENTUSED,LoadString(IDS_DRIVECOL_PERCENTUSED),LVCFMT_RIGHT, 55, COL_PERCENTUSED);

    m_list.OnColumnsInserted();

    m_folderName = COptions::SelectDrivesFolder.Obj().c_str();
    m_selectedDrives = COptions::SelectDrivesDrives;

    ShowWindow(SW_SHOWNORMAL);
    UpdateWindow();
    BringWindowToTop();
    SetForegroundWindow();

    const DWORD drives = ::GetLogicalDrives();
    DWORD mask         = 0x00000001;
    for (int i = 0; i < wds::iNumDriveLetters; i++, mask <<= 1)
    {
        if ((drives & mask) == 0)
        {
            continue;
        }

        CStringW s;
        s.Format(L"%c:\\", i + wds::chrCapA);

        const UINT type = ::GetDriveType(s);
        if (type == DRIVE_UNKNOWN || type == DRIVE_NO_ROOT_DIR)
        {
            continue;
        }

        // The check of remote drives will be done in the background by the CDriveInformationThread.
        if (type != DRIVE_REMOTE && !DriveExists(s))
        {
            continue;
        }

        auto item = new CDriveItem(&m_list, s);
        m_list.InsertListItem(m_list.GetItemCount(), item);
        item->StartQuery(m_hWnd, _serial);

        for (const auto & drive : m_selectedDrives)
        {
            if (std::wstring(item->GetDrive()) == drive)
            {
                m_list.SelectItem(item);
                break;
            }
        }
    }

    m_list.SortItems();

    m_radio = COptions::SelectDrivesRadio;
    UpdateData(false);

    switch (m_radio)
    {
    case RADIO_ALLLOCALDRIVES:
    case RADIO_AFOLDER:
        {
            m_okButton.SetFocus();
        }
        break;
    case RADIO_SOMEDRIVES:
        {
            m_list.SetFocus();
        }
        break;
    }

    UpdateButtons();
    return false; // we have set the focus.
}

void CSelectDrivesDlg::OnBnClickedBrowsefolder()
{
    // Buffer, because SHBrowseForFolder() wants a buffer
    CStringW sDisplayName, sSelectedFolder = m_folderName;
    BROWSEINFO bi;
    ZeroMemory(&bi, sizeof(bi));

    // Load a meaningful title for the browse dialog
    const CStringW title = LoadString(IDS_SELECTFOLDER);
    bi.hwndOwner         = m_hWnd;
    // Use the CStringW as buffer (minimum is MAX_PATH as length)
    bi.pszDisplayName = sDisplayName.GetBuffer(_MAX_PATH);
    bi.lpszTitle      = title;
    // Set a callback function to pre-select a folder
    bi.lpfn   = static_cast<BFFCALLBACK>(BrowseCallbackProc);
    bi.lParam = LPARAM(sSelectedFolder.GetBuffer());
    // Set the required flags
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_EDITBOX | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;

    SmartPointer<LPITEMIDLIST> pidl(CoTaskMemFree, SHBrowseForFolder(&bi));
    // Release the actual buffer
    sDisplayName.ReleaseBuffer();
    sSelectedFolder.ReleaseBuffer();

    if (pidl != nullptr)
    {
        CComPtr<IShellFolder> pshf;
        HRESULT hr = ::SHGetDesktopFolder(&pshf);
        ASSERT(SUCCEEDED(hr));

        STRRET strret;
        strret.uType = STRRET_CSTR;
        hr           = pshf->GetDisplayNameOf(pidl, SHGDN_FORPARSING, &strret);
        ASSERT(SUCCEEDED(hr));
        CStringW sDir = MyStrRetToString(pidl, &strret);

        m_folderName = sDir;
        m_radio      = RADIO_AFOLDER;
        UpdateData(false);
        UpdateButtons();
    }
}

void CSelectDrivesDlg::OnOK()
{
    UpdateData();

    m_drives.RemoveAll();
    m_selectedDrives.clear();
    if (m_radio == RADIO_AFOLDER)
    {
        m_folderName = getFullPathName_(m_folderName);
        UpdateData(false);
    }

    for (int i = 0; i < m_list.GetItemCount(); i++)
    {
        const CDriveItem* item = m_list.GetItem(i);

        if (m_radio == RADIO_ALLLOCALDRIVES && !item->IsRemote() && !item->IsSUBSTed()
            || m_radio == RADIO_SOMEDRIVES && m_list.IsItemSelected(i))
        {
            m_drives.Add(item->GetDrive());
        }
        if (m_list.IsItemSelected(i))
        {
            m_selectedDrives.emplace_back(item->GetDrive().GetString());
        }
    }

    COptions::SelectDrivesRadio = m_radio;
    COptions::SelectDrivesFolder = std::wstring(m_folderName);

    CDialog::OnOK();
}

void CSelectDrivesDlg::UpdateButtons()
{
    UpdateData();
    bool enableOk = false;
    switch (m_radio)
    {
    case RADIO_ALLLOCALDRIVES:
        {
            enableOk = true;
        }
        break;
    case RADIO_SOMEDRIVES:
        {
            enableOk = m_list.GetSelectedCount() > 0;
        }
        break;
    case RADIO_AFOLDER:
        if (!m_folderName.IsEmpty())
        {
            if (m_folderName.GetLength() >= 2 && m_folderName.Left(2) == L"\\\\")
            {
                enableOk = true;
            }
            else
            {
                CStringW pattern = m_folderName;
                if (pattern.Right(1) != L"\\")
                {
                    pattern += L"\\";
                }
                pattern += L"*.*";
                enableOk = FALSE != CFileFind().FindFile(pattern);
            }
        }
        break;
    default:
        {
            ASSERT(0);
        }
    }
    m_okButton.EnableWindow(enableOk);
}

void CSelectDrivesDlg::OnBnClickedAfolder()
{
    UpdateButtons();
}

void CSelectDrivesDlg::OnBnClickedSomedrives()
{
    m_list.SetFocus();
    UpdateButtons();
}

void CSelectDrivesDlg::OnEnChangeFoldername()
{
    UpdateButtons();
}

void CSelectDrivesDlg::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT mis)
{
    if (nIDCtl == IDC_DRIVES)
    {
        mis->itemHeight = 20;
    }
    else
    {
        CDialog::OnMeasureItem(nIDCtl, mis);
    }
}

void CSelectDrivesDlg::OnLvnItemchangedDrives(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    m_radio = RADIO_SOMEDRIVES;

    UpdateData(false);
    UpdateButtons();

    *pResult = 0;
}

void CSelectDrivesDlg::OnBnClickedAlllocaldrives()
{
    UpdateButtons();
}

void CSelectDrivesDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);
    m_layout.OnSize();
}

void CSelectDrivesDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    m_layout.OnGetMinMaxInfo(lpMMI);
    CDialog::OnGetMinMaxInfo(lpMMI);
}

void CSelectDrivesDlg::OnDestroy()
{
    CDriveInformationThread::InvalidateDialogHandle();

    m_layout.OnDestroy();
    CDialog::OnDestroy();
}

LRESULT CSelectDrivesDlg::OnWmuOk(WPARAM, LPARAM)
{
    OnOK();
    return 0;
}

// This message is _sent_ by a CDriveInformationThread.
//
LRESULT CSelectDrivesDlg::OnWmuThreadFinished(WPARAM serial, LPARAM lparam)
{
    if (serial != _serial)
    {
        VTRACE(L"OnWmuThreadFinished: invalid serial (window handle recycled?)");
        return 0;
    }

    auto thread = (CDriveInformationThread*)lparam;

    bool success;
    CStringW name;
    ULONGLONG total;
    ULONGLONG free;

    const LPARAM driveItem = thread->GetDriveInformation(success, name, total, free);

    // For paranoia's sake we check, whether driveItem is in our list.
    // (and we so find its index.)
    LVFINDINFO fi;
    ZeroMemory(&fi, sizeof(fi));
    fi.flags  = LVFI_PARAM;
    fi.lParam = driveItem;

    const int i = m_list.FindItem(&fi);
    if (i == -1)
    {
        VTRACE(L"OnWmuThreadFinished: item not found!");
        return 0;
    }

    auto item = (CDriveItem*)driveItem;

    item->SetDriveInformation(success, name, total, free);

    m_list.RedrawItems(i, i);

    m_list.SortItems();

    return 0;
}

void CSelectDrivesDlg::OnSysColorChange()
{
    CDialog::OnSysColorChange();
    m_list.SysColorChanged();
}

// Callback function for the dialog shown by SHBrowseForFolder()
int CALLBACK CSelectDrivesDlg::BrowseCallbackProc(HWND hWnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    UNREFERENCED_PARAMETER(lParam);

    if (uMsg == BFFM_INITIALIZED)
    {
        ::SendMessage(hWnd, BFFM_SETSELECTION, TRUE, lpData);
    }

    return 0;
}

CStringW CSelectDrivesDlg::getFullPathName_(LPCWSTR relativePath)
{
    LPWSTR dummy;
    CStringW buffer;

    DWORD len = _MAX_PATH;

    DWORD dw = ::GetFullPathName(relativePath, len, buffer.GetBuffer(len), &dummy);
    buffer.ReleaseBuffer();

    while (dw >= len)
    {
        len += _MAX_PATH;
        dw = ::GetFullPathName(relativePath, len, buffer.GetBuffer(len), &dummy);
        buffer.ReleaseBuffer();
    }

    if (0 == dw)
    {
        VTRACE(L"GetFullPathName(%s) failed: GetLastError returns %u", relativePath, ::GetLastError());
        return relativePath;
    }

    return buffer;
}
