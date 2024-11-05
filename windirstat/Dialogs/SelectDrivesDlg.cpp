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
#include "SmartPointer.h"
#include "CommonHelpers.h"
#include "Options.h"
#include "GlobalHelpers.h"
#include "SelectDrivesDlg.h"

#include "FileFind.h"
#include "Localization.h"

namespace
{
    enum : std::uint8_t
    {
        COL_NAME,
        COL_TOTAL,
        COL_FREE,
        COL_GRAPH,
        COL_PERCENTUSED
    };

    constexpr UINT WMU_OK = WM_USER + 100;

    UINT WMU_THREADFINISHED = ::RegisterWindowMessage(L"{F03D3293-86E0-4c87-B559-5FD103F5AF58}");

    // Return: false, if drive not accessible
    bool RetrieveDriveInformation(const std::wstring & path, std::wstring& name, ULONGLONG& total, ULONGLONG& free)
    {
        std::wstring volumeName;

        if (!GetVolumeName(path, volumeName))
        {
            return false;
        }

        name = FormatVolumeName(path, volumeName);

        std::tie(total, free) = CDirStatApp::GetFreeDiskSpace(path);
        if (total == 0)
        {
            return false;
        }

        return true;
    }
}

/////////////////////////////////////////////////////////////////////////////

CDriveItem::CDriveItem(CDrivesList* list, const std::wstring & pszPath)
    : m_List(list)
    , m_Path(pszPath)
    , m_Image(GetIconImageList()->GetFileImage(m_Path))
    , m_IsRemote(DRIVE_REMOTE == ::GetDriveType(m_Path.c_str()))
    , m_Name(m_Path) {}

void CDriveItem::StartQuery(HWND dialog, const UINT serial) const
{
    ASSERT(dialog != nullptr);

    ASSERT(m_Querying); // The synchronous query in the constructor is commented out.

    if (m_Querying)
    {
        new CDriveInformationThread(m_Path, reinterpret_cast<LPARAM>(this), dialog, serial);
        // (will delete itself when finished.)
    }
}

void CDriveItem::SetDriveInformation(const bool success, const std::wstring & name, const ULONGLONG total, const ULONGLONG free)
{
    m_Querying = false;
    m_Success  = success;

    if (m_Success)
    {
        m_Name       = name;
        m_TotalBytes = total;
        m_FreeBytes  = free;
        m_Used       = 0.0;

        // guard against cases where free bytes might be limited (.e.g, quotas)
        if (m_TotalBytes > 0 && m_TotalBytes >= m_FreeBytes)
        {
            m_Used = static_cast<double>(m_TotalBytes - m_FreeBytes) / m_TotalBytes;
        }
    }
}

bool CDriveItem::IsRemote() const
{
    return m_IsRemote;
}

bool CDriveItem::IsSUBSTed() const
{
    return IsSUBSTedDrive(m_Path);
}

int CDriveItem::Compare(const CSortingListItem* baseOther, const int subitem) const
{
    const CDriveItem* other = reinterpret_cast<const CDriveItem*>(baseOther);

    switch (subitem)
    {
        case COL_NAME: return signum(_wcsicmp(GetPath().c_str(),other->GetPath().c_str()));
        case COL_TOTAL: return usignum(m_TotalBytes, other->m_TotalBytes);
        case COL_FREE: return usignum(m_FreeBytes, other->m_FreeBytes);
        case COL_GRAPH:
        case COL_PERCENTUSED: return signum(m_Used - other->m_Used);
        default: ASSERT(FALSE);
    }

    return 0;
}

int CDriveItem::GetImage() const
{
    return m_Image;
}

bool CDriveItem::DrawSubitem(const int subitem, CDC* pdc, CRect rc, const UINT state, int* width, int* focusLeft) const
{
    if (subitem == COL_NAME)
    {
        DrawLabel(m_List, GetIconImageList(), pdc, rc, state, width, focusLeft);
        return true;
    }

    if (subitem == COL_GRAPH)
    {
        if (!m_Success)
        {
            return false;
        }

        if (width != nullptr)
        {
            *width = 100;
            return true;
        }

        DrawSelection(m_List, pdc, rc, state);

        rc.DeflateRect(3, 5);

        DrawPercentage(pdc, rc, m_Used, RGB(0, 0, 170));

        return true;
    }

    return false;
}

std::wstring CDriveItem::GetText(const int subitem) const
{
    std::wstring s;

    switch (subitem)
    {
    case COL_NAME:
        {
            s = m_Name;
        }
        break;

    case COL_TOTAL:
        if (m_Success)
        {
            s = FormatBytes(m_TotalBytes);
        }
        break;

    case COL_FREE:
        if (m_Success)
        {
            s = FormatBytes(m_FreeBytes);
        }
        break;

    case COL_GRAPH:
        if (m_Querying)
        {
            s = Localization::Lookup(IDS_QUERYING);
        }
        else if (!m_Success)
        {
            s = Localization::Lookup(IDS_NOTACCESSIBLE);
        }
        break;

    case COL_PERCENTUSED:
        if (m_Success)
        {
            s = FormatDouble(m_Used * 100) + L"%";
        }
        break;

    default:
        ASSERT(FALSE);
    }

    return s;
}

std::wstring CDriveItem::GetPath() const
{
    return m_Path;
}

std::wstring CDriveItem::GetDrive() const
{
    return m_Path.substr(0, 2);
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
// We set the m_Dialog members of all running threads to null, so that
// they don't send messages around to a no-more-existing window.
//
void CDriveInformationThread::InvalidateDialogHandle()
{
    std::lock_guard lock(_mutexRunningThreads);
    for (const auto & thread : _runningThreads)
    {
        std::lock_guard lockd(thread->m_Mutex);
        thread->m_Dialog = nullptr;
    }
}

// The constructor starts the thread.
//
CDriveInformationThread::CDriveInformationThread(const std::wstring & path, const LPARAM driveItem, HWND dialog, const UINT serial)
    : m_Path(path)
      , m_DriveItem(driveItem)
      , m_Dialog(dialog)
      , m_Serial(serial)
{
    ASSERT(m_bAutoDelete);

    AddRunningThread();

    VERIFY(CreateThread());
}

BOOL CDriveInformationThread::InitInstance()
{
    m_Success = RetrieveDriveInformation(m_Path, m_Name, m_TotalBytes, m_FreeBytes);

    HWND dialog = nullptr;

    {
        std::lock_guard lock(m_Mutex);
        dialog = m_Dialog;
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
        ::SendMessage(dialog, WMU_THREADFINISHED, m_Serial, reinterpret_cast<LPARAM>(this));
    }

    RemoveRunningThread();

    ASSERT(m_bAutoDelete); // Object will delete itself.
    return false;          // no Run(), please!
}

// This method is only called by the gui thread, while we hang
// in SendMessage(dialog, WMU_THREADFINISHED, 0, this).
// So we need no synchronization.
//
LPARAM CDriveInformationThread::GetDriveInformation(bool& success, std::wstring& name, ULONGLONG& total, ULONGLONG& free) const
{
    name    = m_Name;
    total   = m_TotalBytes;
    free    = m_FreeBytes;
    success = m_Success;

    return m_DriveItem;
}

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CDrivesList, COwnerDrawnListControl)

CDrivesList::CDrivesList()
    : COwnerDrawnListControl(20, COptions::DriveListColumnOrder.Ptr(), COptions::DriveListColumnWidths.Ptr())
{
}

CDriveItem* CDrivesList::GetItem(const int i) const
{
    return reinterpret_cast<CDriveItem*>(GetItemData(i));
}

bool CDrivesList::HasImages()
{
    return true;
}

void CDrivesList::SelectItem(const CDriveItem* item)
{
    const int i = FindListItem(item);
    SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
}

bool CDrivesList::IsItemSelected(const int i) const
{
    return LVIS_SELECTED == GetItemState(i, LVIS_SELECTED);
}

void CDrivesList::OnLButtonDown(UINT /*nFlags*/ , CPoint/*point*/)
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
        lv.hdr.code = static_cast<UINT>(LVN_ITEMCHANGED);
        GetParent()->SendMessage(WM_NOTIFY, GetDlgCtrlID(), reinterpret_cast<LPARAM>(&lv));
    }
}

void CDrivesList::OnNMDblclk(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    *pResult = FALSE;

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

    (void) GetParent()->SendMessage(WMU_OK);
}

#pragma warning(push)
#pragma warning(disable:26454)
BEGIN_MESSAGE_MAP(CDrivesList, COwnerDrawnListControl)
    ON_WM_LBUTTONDOWN()
    ON_NOTIFY_REFLECT(LVN_DELETEITEM, OnLvnDeleteitem)
    ON_WM_MEASUREITEM_REFLECT()
    ON_NOTIFY_REFLECT(NM_DBLCLK, OnNMDblclk)
END_MESSAGE_MAP()
#pragma warning(pop)

void CDrivesList::OnLvnDeleteitem(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    delete GetItem(pNMLV->iItem);
    *pResult = FALSE;
}

void CDrivesList::MeasureItem(LPMEASUREITEMSTRUCT mis)
{
    mis->itemHeight = GetRowHeight();
}

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CSelectDrivesDlg, CDialogEx)

UINT CSelectDrivesDlg::_serial;

CSelectDrivesDlg::CSelectDrivesDlg(CWnd* pParent) : CDialogEx(IDD, pParent)
      , m_Layout(this, COptions::DriveSelectWindowRect.Ptr())
{
    _serial++;
}

void CSelectDrivesDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TARGET_DRIVES_LIST, m_List);
    DDX_Radio(pDX, IDC_RADIO_TARGET_DRIVES_ALL, m_Radio);
    DDX_Check(pDX, IDC_SCAN_DUPLICATES, m_ScanDuplicates);
    DDX_Control(pDX, IDOK, m_OkButton);
    DDX_Control(pDX, IDC_BROWSE_FOLDER, m_Browse);
}

#pragma warning(push)
#pragma warning(disable:26454)
BEGIN_MESSAGE_MAP(CSelectDrivesDlg, CDialogEx)
    ON_BN_CLICKED(IDC_RADIO_TARGET_DRIVES_ALL, OnBnClickedUpdateButtons)
    ON_BN_CLICKED(IDC_RADIO_TARGET_DRIVES_SUBSET, &CSelectDrivesDlg::OnBnClickedRadioTargetDrivesSubset)
    ON_BN_CLICKED(IDC_RADIO_TARGET_FOLDER, &CSelectDrivesDlg::OnBnClickedRadioTargetFolder)
    ON_BN_CLICKED(IDC_SCAN_DUPLICATES, OnBnClickedUpdateButtons)
    ON_EN_CHANGE(IDC_BROWSE_FOLDER, OnEnChangeFolderName)
    ON_MESSAGE(WMU_OK, OnWmuOk)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_TARGET_DRIVES_LIST, OnLvnItemchangedDrives)
    ON_NOTIFY(NM_SETFOCUS, IDC_TARGET_DRIVES_LIST, &CSelectDrivesDlg::OnNMSetfocusTargetDrivesList)
    ON_REGISTERED_MESSAGE(WMU_THREADFINISHED, OnWmDriveInfoThreadFinished)
    ON_WM_DESTROY()
    ON_WM_GETMINMAXINFO()
    ON_WM_MEASUREITEM()
    ON_WM_SIZE()
    ON_WM_SYSCOLORCHANGE()
END_MESSAGE_MAP()
#pragma warning(pop)

BOOL CSelectDrivesDlg::OnInitDialog()
{
    CWaitCursor wc;

    CDialogEx::OnInitDialog();

    Localization::UpdateDialogs(*this);

    if (WMU_THREADFINISHED == 0)
    {
        VTRACE(L"Failed. Using WM_USER + 123");
        WMU_THREADFINISHED = WM_USER + 123;
    }

    ModifyStyle(0, WS_CLIPCHILDREN);

    m_Layout.AddControl(IDOK, 1, 0, 0, 0);
    m_Layout.AddControl(IDCANCEL, 1, 0, 0, 0);
    m_Layout.AddControl(IDC_TARGET_DRIVES_LIST, 0, 0, 1, 1);
    m_Layout.AddControl(IDC_RADIO_TARGET_FOLDER, 0, 1, 0, 0);
    m_Layout.AddControl(IDC_BROWSE_FOLDER, 0, 1, 1, 0);
    m_Layout.AddControl(IDC_SCAN_DUPLICATES, 0, 1, 1, 0);

    m_Layout.OnInitDialog(true);

    m_List.ModifyStyle(0, LVS_SHOWSELALWAYS);
    m_List.ShowGrid(COptions::ListGrid);
    m_List.ShowStripes(COptions::ListStripes);
    m_List.ShowFullRowSelection(COptions::ListFullRowSelection);
    m_List.SetExtendedStyle(m_List.GetExtendedStyle() | LVS_EX_HEADERDRAGDROP);

    m_List.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_NAME).c_str(), LVCFMT_LEFT, 120, COL_NAME);
    m_List.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_TOTAL).c_str(), LVCFMT_RIGHT, 55, COL_TOTAL);
    m_List.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_FREE).c_str(), LVCFMT_RIGHT, 55, COL_FREE);
    m_List.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_GRAPH).c_str(), LVCFMT_LEFT, 100, COL_GRAPH);
    m_List.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_PERCENTUSED).c_str(),LVCFMT_RIGHT, 55, COL_PERCENTUSED);

    m_List.OnColumnsInserted();

    m_FolderName = COptions::SelectDrivesFolder.Obj().c_str();
    m_SelectedDrives = COptions::SelectDrivesDrives;
    m_ScanDuplicates = COptions::ScanForDuplicates;

    CBitmap bitmap;
    bitmap.LoadBitmapW(IDB_FILE_SELECT);
    m_Browse.SetBrowseButtonImage(bitmap, TRUE);
    m_Browse.SetWindowTextW(m_FolderName);

    ShowWindow(SW_SHOWNORMAL);
    UpdateWindow();
    BringWindowToTop();
    SetForegroundWindow();

    const DWORD drives = GetLogicalDrives();
    DWORD mask = 0x00000001;
    for (std::size_t i = 0; i < wds::strAlpha.size(); i++, mask <<= 1)
    {
        if ((drives & mask) == 0)
        {
            continue;
        }
        
        std::wstring s = std::wstring(1, wds::strAlpha.at(i)) + L":\\";
        const UINT type = ::GetDriveType(s.c_str());
        if (type == DRIVE_UNKNOWN || type == DRIVE_NO_ROOT_DIR)
        {
            continue;
        }

        // The check of remote drives will be done in the background by the CDriveInformationThread.
        if (type != DRIVE_REMOTE && !DriveExists(s))
        {
            continue;
        }

        const auto item = new CDriveItem(&m_List, s);
        m_List.InsertListItem(m_List.GetItemCount(), item);
        item->StartQuery(m_hWnd, _serial);

        for (const auto & drive : m_SelectedDrives)
        {
            if (std::wstring(item->GetDrive()) == drive)
            {
                m_List.SelectItem(item);
                break;
            }
        }
    }

    m_List.SortItems();

    m_Radio = COptions::SelectDrivesRadio;
    UpdateData(FALSE);

    if (m_Radio == RADIO_TARGET_DRIVES_ALL ||
        m_Radio == RADIO_TARGET_FOLDER)
    {
        m_OkButton.SetFocus();
    }
    else if (m_Radio == RADIO_TARGET_DRIVES_SUBSET)
    {
        m_List.SetFocus();
    }

    UpdateButtons();
    return false; // we have set the focus.
}

void CSelectDrivesDlg::OnOK()
{
    UpdateData();

    m_Drives.clear();
    m_SelectedDrives.clear();
    if (m_Radio == RADIO_TARGET_FOLDER)
    {
        if (m_FolderName.GetAt(m_FolderName.GetLength() - 1) == L':') m_FolderName.AppendChar(L'\\');
        m_FolderName = GetFullPathName(m_FolderName.GetString()).c_str();
        UpdateData(FALSE);
    }

    for (int i = 0; i < m_List.GetItemCount(); i++)
    {
        const CDriveItem* item = m_List.GetItem(i);

        if (m_Radio == RADIO_TARGET_DRIVES_ALL && !item->IsRemote() && !item->IsSUBSTed()
            || m_Radio == RADIO_TARGET_DRIVES_SUBSET && m_List.IsItemSelected(i))
        {
            m_Drives.emplace_back(item->GetDrive());
        }
        if (m_List.IsItemSelected(i))
        {
            m_SelectedDrives.emplace_back(item->GetDrive());
        }
    }

    COptions::SelectDrivesRadio = m_Radio;
    COptions::SelectDrivesFolder = std::wstring(m_FolderName);
    COptions::ScanForDuplicates = (FALSE != m_ScanDuplicates);

    CDialogEx::OnOK();
}

void CSelectDrivesDlg::UpdateButtons()
{
    UpdateData();
    bool enableOk = false;
    switch (m_Radio)
    {
    case RADIO_TARGET_DRIVES_ALL:
        {
            enableOk = true;
        }
        break;
    case RADIO_TARGET_DRIVES_SUBSET:
        {
            enableOk = m_List.GetSelectedCount() > 0;
        }
        break;
    case RADIO_TARGET_FOLDER:
        if (!m_FolderName.IsEmpty())
        {
            if (m_FolderName.GetLength() >= 2 && m_FolderName.Left(2) == L"\\\\")
            {
                enableOk = true;
            }
            else
            {
                enableOk = FileFindEnhanced::DoesFileExist(m_FolderName.GetString());
            }
        }
        break;
    default:
        {
            ASSERT(FALSE);
        }
    }
    m_OkButton.EnableWindow(enableOk);
}

void CSelectDrivesDlg::OnBnClickedRadioTargetDrivesSubset()
{
    // dynamically adjust next tab order
    GetDlgItem(IDC_BROWSE_FOLDER)->SetWindowPos(
        GetDlgItem(IDC_TARGET_DRIVES_LIST), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    UpdateButtons();
}

void CSelectDrivesDlg::OnBnClickedRadioTargetFolder()
{
    // dynamically adjust next tab order
    GetDlgItem(IDC_TARGET_DRIVES_LIST)->SetWindowPos(
        GetDlgItem(IDC_BROWSE_FOLDER), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    UpdateButtons();
}

void CSelectDrivesDlg::OnEnChangeFolderName()
{
    m_Radio = RADIO_TARGET_FOLDER;
    UpdateData(FALSE);

    m_Browse.GetWindowText(m_FolderName);
    UpdateButtons();
}

void CSelectDrivesDlg::OnLvnItemchangedDrives(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    m_Radio = RADIO_TARGET_DRIVES_SUBSET;

    UpdateData(FALSE);
    UpdateButtons();

    *pResult = FALSE;
}

void CSelectDrivesDlg::OnBnClickedUpdateButtons()
{
    UpdateButtons();
}

void CSelectDrivesDlg::OnSize(const UINT nType, const int cx, const int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    m_Layout.OnSize();
}

void CSelectDrivesDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    m_Layout.OnGetMinMaxInfo(lpMMI);
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}

void CSelectDrivesDlg::OnDestroy()
{
    CDriveInformationThread::InvalidateDialogHandle();

    m_Layout.OnDestroy();
    CDialogEx::OnDestroy();
}

LRESULT CSelectDrivesDlg::OnWmuOk(WPARAM, LPARAM)
{
    OnOK();
    return 0;
}

LRESULT CSelectDrivesDlg::OnWmDriveInfoThreadFinished(const WPARAM serial, const LPARAM lparam)
{
    if (serial != _serial)
    {
        VTRACE(L"Invalid serial");
        return 0;
    }

    bool success;
    std::wstring name;
    ULONGLONG total;
    ULONGLONG free;

    const auto thread = reinterpret_cast<CDriveInformationThread*>(lparam);
    const LPARAM driveItem = thread->GetDriveInformation(success, name, total, free);

    LVFINDINFO fi;
    ZeroMemory(&fi, sizeof(fi));
    fi.flags  = LVFI_PARAM;
    fi.lParam = driveItem;

    if (m_List.FindItem(&fi) == -1)
    {
        VTRACE(L"Item not found!");
        return 0;
    }

    const auto item = reinterpret_cast<CDriveItem*>(driveItem);
    item->SetDriveInformation(success, name, total, free);

    m_List.SortItems();

    return 0;
}

void CSelectDrivesDlg::OnSysColorChange()
{
    CDialogEx::OnSysColorChange();
    m_List.SysColorChanged();
}

std::wstring CSelectDrivesDlg::GetFullPathName(const std::wstring & relativePath)
{
    SmartPointer<LPWSTR> path(free, _wfullpath(nullptr, relativePath.c_str(), 0));
    return path != nullptr ? static_cast<LPWSTR>(path) : relativePath;
}

void CSelectDrivesDlg::OnNMSetfocusTargetDrivesList(NMHDR*, LRESULT* pResult)
{
    if (m_List.GetItemCount() > 0 && m_List.GetSelectedCount() == 0)
    {
        m_List.SetFocus();
        m_List.SelectItem(m_List.GetItem(0));
    }

    *pResult = 0;
}

BOOL CSelectDrivesDlg::PreTranslateMessage(MSG* pMsg)
{
    // Change radio button if a user clicks in the dialog box without changing it
    if ((pMsg->wParam == VK_LBUTTON) && (m_Browse.m_hWnd == pMsg->hwnd))
    {
        m_Radio = RADIO_TARGET_FOLDER;
        UpdateData(FALSE);
        UpdateButtons();
    }

    return CDialogEx::PreTranslateMessage(pMsg);
}
