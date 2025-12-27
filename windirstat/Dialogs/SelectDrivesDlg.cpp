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
#include "SelectDrivesDlg.h"
#include "FinderBasic.h"
#include "MessageBoxDlg.h"

namespace
{
    enum : std::uint8_t
    {
        COL_DRIVES_NAME,
        COL_DRIVES_TOTAL,
        COL_DRIVES_FREE,
        COL_DRIVES_GRAPH,
        COL_DRIVES_PERCENT_USED
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
    : m_list(list)
    , m_path(pszPath)
    , m_icon(GetIconHandler()->FetchShellIcon(m_path))
    , m_isRemote(DRIVE_REMOTE == ::GetDriveType(m_path.c_str()))
    , m_subst(IsSUBSTedDrive(m_path))
    , m_name(m_path) {}

void CDriveItem::StartQuery(HWND dialog, const UINT serial) const
{
    ASSERT(dialog != nullptr);

    ASSERT(m_querying); // The synchronous query in the constructor is commented out.

    if (m_querying)
    {
        new CDriveInformationThread(m_path, reinterpret_cast<LPARAM>(this), dialog, serial);
        // (will delete itself when finished.)
    }
}

void CDriveItem::SetDriveInformation(const bool success, const std::wstring & name, const ULONGLONG total, const ULONGLONG free)
{
    m_querying = false;
    m_success  = success;

    if (m_success)
    {
        m_name       = name;
        m_totalBytes = total;
        m_freeBytes  = free;
        m_used       = 0.0;

        // guard against cases where free bytes might be limited (e.g., quotas)
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
    return m_subst;
}

int CDriveItem::Compare(const CSortingListItem* baseOther, const int subitem) const
{
    const CDriveItem* other = reinterpret_cast<const CDriveItem*>(baseOther);

    switch (subitem)
    {
        case COL_DRIVES_NAME: return signum(_wcsicmp(GetPath().c_str(),other->GetPath().c_str()));
        case COL_DRIVES_TOTAL: return usignum(m_totalBytes, other->m_totalBytes);
        case COL_DRIVES_FREE: return usignum(m_freeBytes, other->m_freeBytes);
        case COL_DRIVES_GRAPH:
        case COL_DRIVES_PERCENT_USED: return signum(m_used - other->m_used);
        default: ASSERT(FALSE);
    }

    return 0;
}

HICON CDriveItem::GetIcon()
{
    return m_icon;
}

bool CDriveItem::DrawSubItem(const int subitem, CDC* pdc, CRect rc, const UINT state, int* width, int* focusLeft)
{
    if (subitem == COL_DRIVES_NAME)
    {
        DrawLabel(m_list, pdc, rc, state, width, focusLeft);
        return true;
    }

    if (subitem == COL_DRIVES_GRAPH)
    {
        if (!m_success || IsSUBSTed())
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

        DrawPercentage(pdc, rc, m_used, RGB(80, 80, 170));

        return true;
    }

    return false;
}

std::wstring CDriveItem::GetText(const int subitem) const
{
    std::wstring s;

    switch (subitem)
    {
    case COL_DRIVES_NAME:
        {
            s = m_name;
        }
        break;

    case COL_DRIVES_TOTAL:
        if (m_success && !IsSUBSTed())
        {
            s = FormatBytes(m_totalBytes);
        }
        break;

    case COL_DRIVES_FREE:
        if (m_success)
        {
            s = FormatBytes(m_freeBytes);
        }
        break;

    case COL_DRIVES_GRAPH:
        if (m_querying && !IsSUBSTed())
        {
            s = Localization::Lookup(IDS_QUERYING);
        }
        else if (!m_success)
        {
            s = Localization::Lookup(IDS_NOTACCESSIBLE);
        }
        break;

    case COL_DRIVES_PERCENT_USED:
        if (m_success && !IsSUBSTed())
        {
            s = FormatDouble(m_used * 100) + L"%";
        }
        break;

    default:
        ASSERT(FALSE);
    }

    return s;
}

std::wstring CDriveItem::GetPath() const
{
    return m_path;
}

std::wstring CDriveItem::GetDrive() const
{
    return m_path.substr(0, 2);
}

/////////////////////////////////////////////////////////////////////////////

std::unordered_set<CDriveInformationThread*> CDriveInformationThread::s_runningThreads;
std::mutex CDriveInformationThread::s_mutexRunningThreads;

void CDriveInformationThread::AddRunningThread()
{
    std::scoped_lock lock(s_mutexRunningThreads);
    s_runningThreads.insert(this);
}

void CDriveInformationThread::RemoveRunningThread()
{
    std::scoped_lock lock(s_mutexRunningThreads);
    s_runningThreads.erase(this);
}

// This static method is called by the dialog when the dialog gets closed.
// We set the m_dialog members of all running threads to null, so that
// they don't send messages around to a no-more-existing window.
void CDriveInformationThread::InvalidateDialogHandle()
{
    std::scoped_lock lock(s_mutexRunningThreads);
    for (const auto & thread : s_runningThreads)
    {
        thread->m_dialog = nullptr;
    }
}

CDriveInformationThread::CDriveInformationThread(const std::wstring & path, const LPARAM driveItem, HWND dialog, const UINT serial)
    : m_path(path)
      , m_driveItem(driveItem)
      , m_serial(serial)
      , m_dialog(dialog)
{
    ASSERT(m_bAutoDelete);

    // The constructor starts the thread
    AddRunningThread();

    VERIFY(CreateThread());
}

BOOL CDriveInformationThread::InitInstance()
{
    m_success = RetrieveDriveInformation(m_path, m_name, m_totalBytes, m_freeBytes);

    if (HWND dialog = m_dialog; dialog != nullptr)
    {
        ::SendMessage(dialog, WMU_THREADFINISHED, m_serial, reinterpret_cast<LPARAM>(this));
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
    name    = m_name;
    total   = m_totalBytes;
    free    = m_freeBytes;
    success = m_success;

    return m_driveItem;
}

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CDrivesList, COwnerDrawnListControl)

CDrivesList::CDrivesList()
    : COwnerDrawnListControl(COptions::DriveListColumnOrder.Ptr(), COptions::DriveListColumnWidths.Ptr())
{
}

CDriveItem* CDrivesList::GetItem(const int i) const
{
    return reinterpret_cast<CDriveItem*>(GetItemData(i));
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

void CDrivesList::OnDoubleClick(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    *pResult = FALSE;

    CPoint point = GetCurrentMessage()->pt;
    ScreenToClient(&point);
    const int i = HitTest(point);
    
    SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

    (void) GetParent()->SendMessage(WMU_OK);
}

BEGIN_MESSAGE_MAP(CDrivesList, COwnerDrawnListControl)
    ON_NOTIFY_REFLECT(LVN_DELETEITEM, OnLvnDeleteItem)
    ON_WM_MEASUREITEM_REFLECT()
    ON_NOTIFY_REFLECT(NM_DBLCLK, OnDoubleClick)
END_MESSAGE_MAP()

void CDrivesList::OnLvnDeleteItem(NMHDR* pNMHDR, LRESULT* pResult)
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

IMPLEMENT_DYNAMIC(CSelectDrivesDlg, CLayoutDialogEx)

UINT CSelectDrivesDlg::s_serial;

CSelectDrivesDlg::CSelectDrivesDlg(CWnd* pParent) :
    CLayoutDialogEx(IDD, COptions::DriveSelectWindowRect.Ptr(), pParent)
{
    s_serial++;
}

void CSelectDrivesDlg::DoDataExchange(CDataExchange* pDX)
{
    CLayoutDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TARGET_DRIVES_LIST, m_list);
    DDX_Radio(pDX, IDC_RADIO_TARGET_DRIVES_ALL, m_radio);
    DDX_Check(pDX, IDC_SCAN_DUPLICATES, m_scanDuplicates);
    DDX_Check(pDX, IDC_FAST_SCAN_CHECKBOX, m_useFastScan);
    DDX_Control(pDX, IDOK, m_okButton);
    DDX_Control(pDX, IDC_BROWSE_FOLDER, m_browseList);
    DDX_Control(pDX, IDC_BROWSE_BUTTON, m_browseButton);
    DDX_CBString(pDX, IDC_BROWSE_FOLDER, m_folderName);
}

BEGIN_MESSAGE_MAP(CSelectDrivesDlg, CLayoutDialogEx)
    ON_BN_CLICKED(IDC_RADIO_TARGET_DRIVES_ALL, OnBnClickedUpdateButtons)
    ON_BN_CLICKED(IDC_RADIO_TARGET_DRIVES_SUBSET, &CSelectDrivesDlg::OnBnClickedRadioTargetDrivesSubset)
    ON_BN_CLICKED(IDC_RADIO_TARGET_FOLDER, &CSelectDrivesDlg::OnBnClickedRadioTargetFolder)
    ON_BN_CLICKED(IDC_SCAN_DUPLICATES, OnBnClickedUpdateButtons)
    ON_BN_CLICKED(IDC_FAST_SCAN_CHECKBOX, OnBnClickedUpdateButtons)
    ON_MESSAGE(WMU_OK, OnWmuOk)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_TARGET_DRIVES_LIST, OnLvnItemChangedDrives)
    ON_NOTIFY(NM_SETFOCUS, IDC_TARGET_DRIVES_LIST, &CSelectDrivesDlg::OnNMSetfocusTargetDrivesList)
    ON_REGISTERED_MESSAGE(WMU_THREADFINISHED, OnWmDriveInfoThreadFinished)
    ON_WM_DESTROY()
    ON_WM_MEASUREITEM()
    ON_WM_SYSCOLORCHANGE()
    ON_WM_CTLCOLOR()
    ON_BN_CLICKED(IDC_BROWSE_BUTTON, &CSelectDrivesDlg::OnBnClickedBrowseButton)
    ON_CBN_EDITCHANGE(IDC_BROWSE_FOLDER, &CSelectDrivesDlg::OnEditchangeBrowseFolder)
    ON_CBN_SELCHANGE(IDC_BROWSE_FOLDER, &CSelectDrivesDlg::OnCbnSelchangeBrowseFolder)
END_MESSAGE_MAP()

BOOL CSelectDrivesDlg::OnInitDialog()
{
    CWaitCursor wc;

    CLayoutDialogEx::OnInitDialog();

    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(GetSafeHwnd());

    ModifyStyle(0, WS_CLIPCHILDREN);

    m_layout.AddControl(IDOK, 1, 0, 0, 0);
    m_layout.AddControl(IDCANCEL, 1, 0, 0, 0);
    m_layout.AddControl(IDC_TARGET_DRIVES_LIST, 0, 0, 1, 1);
    m_layout.AddControl(IDC_RADIO_TARGET_FOLDER, 0, 1, 0, 0);
    m_layout.AddControl(IDC_BROWSE_BUTTON, 1, 1, 0, 0);
    m_layout.AddControl(IDC_BROWSE_FOLDER, 0, 1, 1, 0);
    m_layout.AddControl(IDC_FAST_SCAN_CHECKBOX, 0, 1, 1, 0);
    m_layout.AddControl(IDC_SCAN_DUPLICATES, 0, 1, 1, 0);

    // Disable the Fast Scan checkbox if elevation is not available
    GetDlgItem(IDC_FAST_SCAN_CHECKBOX)->EnableWindow(IsElevationActive() || IsElevationAvailable());

    m_layout.OnInitDialog(true);

    m_list.ModifyStyle(0, LVS_SHOWSELALWAYS);
    m_list.ShowGrid(COptions::ListGrid);
    m_list.ShowStripes(COptions::ListStripes);
    m_list.ShowFullRowSelection(COptions::ListFullRowSelection);
    m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_HEADERDRAGDROP);

    m_list.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_NAME).c_str(), LVCFMT_LEFT, 150, COL_DRIVES_NAME);
    m_list.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_TOTAL).c_str(), LVCFMT_RIGHT, 65, COL_DRIVES_TOTAL);
    m_list.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_FREE).c_str(), LVCFMT_RIGHT, 65, COL_DRIVES_FREE);
    m_list.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_GRAPH).c_str(), LVCFMT_LEFT, 100, COL_DRIVES_GRAPH);
    m_list.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_PERCENT_USED).c_str(),LVCFMT_RIGHT, 65, COL_DRIVES_PERCENT_USED);

    m_list.OnColumnsInserted();

    m_selectedDrives = COptions::SelectDrivesDrives;
    m_scanDuplicates = COptions::ScanForDuplicates;
    m_useFastScan = COptions::UseFastScanEngine;

    // Add previously used folders to the combo box
    for (const auto & folder : COptions::SelectDrivesFolder.Obj())
    {
        m_browseList.AddString(folder.c_str());
    }

    // Select the first folder value from the list
    if (m_browseList.GetCount() > 0)
    {
        m_browseList.SetCurSel(0);
        m_folderName = COptions::SelectDrivesFolder.Obj().front().c_str();
    }

    UpdateData(FALSE);

    CBitmap bitmap;
    bitmap.LoadBitmapW(IDB_FILE_SELECT);
    DarkMode::LightenBitmap(&bitmap);
    m_browseButton.SetBitmap(bitmap);

    ShowWindow(SW_SHOWNORMAL);
    UpdateWindow();
    BringWindowToTop();
    SetForegroundWindow();

    const DWORD drives = GetLogicalDrives();
    for (const size_t i : std::views::iota(0u, wds::strAlpha.size()))
    {
        const DWORD mask = 0x00000001 << i;
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

        const auto item = new CDriveItem(&m_list, s);
        m_list.InsertListItem(m_list.GetItemCount(), item);
        item->StartQuery(m_hWnd, s_serial);

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
    UpdateData(FALSE);

    if (m_radio == RADIO_TARGET_DRIVES_ALL ||
        m_radio == RADIO_TARGET_FOLDER)
    {
        m_okButton.SetFocus();
    }
    else if (m_radio == RADIO_TARGET_DRIVES_SUBSET)
    {
        m_list.SetFocus();
    }

    UpdateButtons();
    return false; // we have set the focus.
}

void CSelectDrivesDlg::OnOK()
{
    UpdateData();

    m_drives.clear();
    m_selectedDrives.clear();
    if (m_radio == RADIO_TARGET_FOLDER)
    {
        if (m_folderName.GetAt(m_folderName.GetLength() - 1) == L':') m_folderName.AppendChar(L'\\');
        m_folderName = GetFullPathName(m_folderName.GetString()).c_str();
        UpdateData(FALSE);

        // Remove the folder from the most recently used list to avoid duplicates
        std::wstring folderName = m_folderName.GetString();
        std::erase_if(COptions::SelectDrivesFolder.Obj(), [&folderName](const std::wstring& s) {
            return _wcsicmp(s.c_str(), folderName.c_str()) == 0;
        });

        // Insert it at the beginning of the used list
        COptions::SelectDrivesFolder.Obj().insert(
          COptions::SelectDrivesFolder.Obj().begin(), folderName);

        // Limit the folder history to the configured count
        COptions::SelectDrivesFolder.Obj().resize(min(static_cast<size_t>(COptions::FolderHistoryCount),
            COptions::SelectDrivesFolder.Obj().size()));
    }

    for (const int i : std::views::iota(0, m_list.GetItemCount()))
    {
        const CDriveItem* item = m_list.GetItem(i);

        if (m_radio == RADIO_TARGET_DRIVES_ALL && !item->IsRemote() && !item->IsSUBSTed() ||
            m_radio == RADIO_TARGET_DRIVES_SUBSET && m_list.IsItemSelected(i))
        {
            m_drives.emplace_back(item->GetDrive());
        }
        if (m_list.IsItemSelected(i))
        {
            m_selectedDrives.emplace_back(item->GetDrive());
        }
    }

    COptions::SelectDrivesRadio = m_radio;
    COptions::SelectDrivesDrives = m_selectedDrives;
    COptions::ScanForDuplicates = (FALSE != m_scanDuplicates);
    COptions::UseFastScanEngine = (FALSE != m_useFastScan);

    // Switch focus to file tree view
    const auto tabbedView = CMainFrame::Get()->GetFileTabbedView();
    tabbedView->SetActiveFileTreeView();

    CLayoutDialogEx::OnOK();
}

void CSelectDrivesDlg::UpdateButtons()
{
    UpdateData();
    bool enableOk = false;

    // Prompt user to elevate if the Fast Scan option is checked in non-elevated session
    if (m_useFastScan && IsElevationAvailable())
    {
        if (WdsMessageBox(*this, Localization::Lookup(IDS_ELEVATION_QUESTION),
            Localization::LookupNeutral(AFX_IDS_APP_TITLE), MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            COptions::UseFastScanEngine = true;
            RunElevated(CDirStatDoc::Get()->GetPathName().GetString());
            return;
        }
        else
        {
            // If the user declines, uncheck the fast scan box to revert the state.
            m_useFastScan = false;
            UpdateData(FALSE);
        }
    }

    switch (m_radio)
    {
    case RADIO_TARGET_DRIVES_ALL:
        {
            enableOk = true;
        }
        break;
    case RADIO_TARGET_DRIVES_SUBSET:
        {
            enableOk = m_list.GetSelectedCount() > 0;
        }
        break;
    case RADIO_TARGET_FOLDER:
        if (!m_folderName.IsEmpty())
        {
            if (m_folderName.GetLength() >= 2 && m_folderName.Left(2) == L"\\\\")
            {
                enableOk = true;
            }
            else
            {
                enableOk = FinderBasic::DoesFileExist(m_folderName.GetString());
            }
        }
        break;
    default:
        {
            ASSERT(FALSE);
        }
    }
    m_okButton.EnableWindow(enableOk);
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

void CSelectDrivesDlg::OnLvnItemChangedDrives(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    SetActiveRadio(IDC_RADIO_TARGET_DRIVES_SUBSET);
    UpdateButtons();

    *pResult = FALSE;
}

void CSelectDrivesDlg::OnBnClickedUpdateButtons()
{
    UpdateButtons();
}

void CSelectDrivesDlg::OnDestroy()
{
    CDriveInformationThread::InvalidateDialogHandle();
    CLayoutDialogEx::OnDestroy();
}

LRESULT CSelectDrivesDlg::OnWmuOk(WPARAM, LPARAM)
{
    OnOK();
    return 0;
}

LRESULT CSelectDrivesDlg::OnWmDriveInfoThreadFinished(const WPARAM serial, const LPARAM lparam)
{
    if (serial != s_serial)
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

    if (m_list.FindItem(&fi) == -1)
    {
        VTRACE(L"Item not found!");
        return 0;
    }

    const auto item = reinterpret_cast<CDriveItem*>(driveItem);
    item->SetDriveInformation(success, name, total, free);

    m_list.SortItems();

    return 0;
}

void CSelectDrivesDlg::OnSysColorChange()
{
    CLayoutDialogEx::OnSysColorChange();
    m_list.SysColorChanged();
}

std::wstring CSelectDrivesDlg::GetFullPathName(const std::wstring & relativePath)
{
    SmartPointer<LPWSTR> path(free, _wfullpath(nullptr, relativePath.c_str(), 0));
    return path != nullptr ? static_cast<LPWSTR>(path) : relativePath;
}

void CSelectDrivesDlg::OnNMSetfocusTargetDrivesList(NMHDR*, LRESULT* pResult)
{
    if (m_list.GetItemCount() > 0 && m_list.GetSelectedCount() == 0)
    {
        m_list.SetFocus();
        m_list.SelectItem(m_list.GetItem(0));
    }

    *pResult = 0;
}

BOOL CSelectDrivesDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_LBUTTONDOWN &&
        (m_browseList.m_hWnd == pMsg->hwnd || m_browseList.m_hWnd == ::GetParent(pMsg->hwnd)))
    {
        SetActiveRadio(IDC_RADIO_TARGET_FOLDER);
        UpdateButtons();
    }

    return CLayoutDialogEx::PreTranslateMessage(pMsg);
}

std::vector<std::wstring> CSelectDrivesDlg::GetSelectedItems() const
{
    return (m_radio == RADIO_TARGET_DRIVES_ALL) ? m_drives :
        (m_radio == RADIO_TARGET_DRIVES_SUBSET) ? m_selectedDrives :
        std::vector<std::wstring>{ m_folderName.GetString() };
}

HBRUSH CSelectDrivesDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CLayoutDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CSelectDrivesDlg::OnBnClickedBrowseButton()
{
    // Prompt user and then update folder in combo box
    UpdateData();

    // Setup folder picker dialog
    CFolderPickerDialog dlg(nullptr,
        OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_DONTADDTORECENT, this);
    const auto title = Localization::LookupNeutral(AFX_IDS_APP_TITLE);
    dlg.m_ofn.lpstrTitle = title.c_str();

    // Show dialog and validate results
    if (dlg.DoModal() != IDOK) return;
    const std::wstring path = dlg.GetFolderPath().GetString();

    if (!FinderBasic::DoesFileExist(path)) return;
    m_folderName = path.c_str();
    UpdateData(FALSE);

    SetActiveRadio(IDC_RADIO_TARGET_FOLDER);
    UpdateButtons();
}

void CSelectDrivesDlg::OnEditchangeBrowseFolder()
{
    // Force assessing folder to make the okay button light up
    SetActiveRadio(IDC_RADIO_TARGET_FOLDER);
    UpdateButtons();
}

void CSelectDrivesDlg::SetActiveRadio(const int radio)
{
    CheckRadioButton(IDC_RADIO_TARGET_DRIVES_ALL, IDC_RADIO_TARGET_FOLDER, radio);
}

void CSelectDrivesDlg::OnCbnSelchangeBrowseFolder()
{
    // Get the current selection text and assess if valid for okay button
    m_browseList.GetLBText(m_browseList.GetCurSel(), m_folderName);
    UpdateData(FALSE);
    UpdateButtons();
}
