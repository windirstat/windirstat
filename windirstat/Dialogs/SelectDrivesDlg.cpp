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

    UINT WMU_OK = ::RegisterWindowMessage(L"{662EB683-FBCC-4C87-8E69-664909A132C1}");
    UINT WMU_THREADFINISHED = ::RegisterWindowMessage(L"{F03D3293-86E0-4c87-B559-5FD103F5AF58}");

    // Return: false, if drive not accessible
    bool RetrieveDriveInformation(const std::wstring& path, std::wstring& name, ULONGLONG& total, ULONGLONG& freeBytes)
    {
        name = FormatVolumeNameOfRootPath(path);
        std::tie(total, freeBytes) = CDirStatApp::GetFreeDiskSpace(path);
        return total != 0;
    }

    std::wstring ResolveFullPath(const std::wstring& relativePath)
    {
        const SmartPointer<LPWSTR> path(free, _wfullpath(nullptr, relativePath.c_str(), 0));
        return path != nullptr ? static_cast<LPWSTR>(path) : relativePath;
    }
}

/////////////////////////////////////////////////////////////////////////////

CDriveItem::CDriveItem(CDrivesList* list, const std::wstring & pszPath)
    : m_driveList(list)
    , m_path(pszPath)
    , m_icon(GetIconHandler()->FetchShellIcon(m_path))
    , m_isRemote(DRIVE_REMOTE == ::GetDriveType(m_path.c_str()))
    , m_subst(IsSUBSTedDrive(m_path))
    , m_name(m_path) {}

CDriveItem::~CDriveItem()
{
    StopQuery();
}

void CDriveItem::StartQuery(const HWND dialog)
{
    ASSERT(dialog != nullptr);
    ASSERT(!m_queryThread.joinable()); // must not be called while a query is in progress

    m_dialog = dialog;

    // Capture 'this' and the path for the thread
    m_queryThread = std::jthread([this](const std::stop_token& stopToken)
    {
        std::wstring name;
        ULONGLONG total = 0;
        ULONGLONG free = 0;
        const bool success = RetrieveDriveInformation(m_path, name, total, free);

        if (stopToken.stop_requested())
        {
            return;
        }

        // Store results before posting; Windows message-queue delivery ensures
        // these writes are visible to the GUI thread when it handles the message.
        if (success)
        {
            m_name = name;
            m_totalBytes = total;
            m_freeBytes = free;
        }

        if (const HWND dialog = m_dialog.load(); dialog != nullptr)
        {
            ::PostMessage(dialog, WMU_THREADFINISHED, success ? 1 : 0, reinterpret_cast<LPARAM>(this));
        }
    });
}

void CDriveItem::StopQuery()
{
    m_dialog = nullptr;   // prevent any pending PostMessage from reaching the dialog
    m_queryThread = {};   // triggers request_stop + join via jthread destructor
}

void CDriveItem::SetDriveInformation(const bool success)
{
    m_querying = false;
    m_success  = success;

    if (m_success)
    {
        // guard against quotas where free may exceed total, or total is zero
        m_used = (m_totalBytes > 0 && m_totalBytes >= m_freeBytes)
            ? static_cast<double>(m_totalBytes - m_freeBytes) / m_totalBytes
            : 0.0;
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

int CDriveItem::Compare(const CWdsListItem* baseOther, const int subitem) const
{
    const CDriveItem* other = reinterpret_cast<const CDriveItem*>(baseOther);

    switch (subitem)
    {
        case COL_DRIVES_NAME: return signum(_wcsicmp(m_path.c_str(), other->m_path.c_str()));
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
        DrawLabel(m_driveList, pdc, rc, state, width, focusLeft);
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

        DrawSelection(m_driveList, pdc, rc, state);
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
        if (m_querying)
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
    return ::GetDrive(m_path);
}

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CDrivesList, CWdsListControl)

CDrivesList::CDrivesList()
    : CWdsListControl(COptions::DriveListColumnOrder.Ptr(), COptions::DriveListColumnWidths.Ptr())
{
}

CDriveItem* CDrivesList::GetItem(const int i) const
{
    return reinterpret_cast<CDriveItem*>(CWdsListControl::GetItem(i));
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
    if (i < 0) return;

    SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

    (void) GetParent()->SendMessage(WMU_OK);
}

BEGIN_MESSAGE_MAP(CDrivesList, CWdsListControl)
    ON_NOTIFY_REFLECT(LVN_DELETEITEM, OnLvnDeleteItem)
    ON_NOTIFY_REFLECT(NM_DBLCLK, OnDoubleClick)
END_MESSAGE_MAP()

void CDrivesList::OnLvnDeleteItem(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    delete GetItem(pNMLV->iItem);
    *pResult = FALSE;
}

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CSelectDrivesDlg, CLayoutDialogEx)

CSelectDrivesDlg::CSelectDrivesDlg(CWnd* pParent) :
    CLayoutDialogEx(IDD, COptions::DriveSelectWindowRect.Ptr(), pParent)
{
}

void CSelectDrivesDlg::DoDataExchange(CDataExchange* pDX)
{
    CLayoutDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TARGET_DRIVES_LIST, m_driveList);
    DDX_Radio(pDX, IDC_RADIO_TARGET_DRIVES_ALL, m_radio);
    DDX_Check(pDX, IDC_SCAN_DUPLICATES, m_scanDuplicates);
    DDX_Check(pDX, IDC_FAST_SCAN_CHECKBOX, m_useFastScan);
    DDX_Control(pDX, IDOK, m_okButton);
    DDX_Control(pDX, IDC_BROWSE_FOLDER, m_browseList);
    DDX_Control(pDX, IDC_BROWSE_BUTTON, m_browseButton);
    DDX_CBString(pDX, IDC_BROWSE_FOLDER, m_folderName);
}

BEGIN_MESSAGE_MAP(CSelectDrivesDlg, CLayoutDialogEx)
    ON_BN_CLICKED(IDC_BROWSE_BUTTON, &CSelectDrivesDlg::OnBnClickedBrowseButton)
    ON_BN_CLICKED(IDC_FAST_SCAN_CHECKBOX, OnBnClickedFastScanCheckbox)
    ON_BN_CLICKED(IDC_RADIO_TARGET_DRIVES_ALL, OnBnClickedUpdateButtons)
    ON_BN_CLICKED(IDC_RADIO_TARGET_DRIVES_SUBSET, &CSelectDrivesDlg::OnBnClickedRadioTargetDrivesSubset)
    ON_BN_CLICKED(IDC_RADIO_TARGET_FOLDER, &CSelectDrivesDlg::OnBnClickedRadioTargetFolder)
    ON_BN_CLICKED(IDC_SCAN_DUPLICATES, OnBnClickedUpdateButtons)
    ON_BN_DOUBLECLICKED(IDC_RADIO_TARGET_DRIVES_ALL, &CSelectDrivesDlg::OnBnDoubleclickedRadio)
    ON_BN_DOUBLECLICKED(IDC_RADIO_TARGET_DRIVES_SUBSET, &CSelectDrivesDlg::OnBnDoubleclickedRadio)
    ON_BN_DOUBLECLICKED(IDC_RADIO_TARGET_FOLDER, &CSelectDrivesDlg::OnBnDoubleclickedRadio)
    ON_CBN_EDITCHANGE(IDC_BROWSE_FOLDER, &CSelectDrivesDlg::OnEditchangeBrowseFolder)
    ON_CBN_SELCHANGE(IDC_BROWSE_FOLDER, &CSelectDrivesDlg::OnBnClickedUpdateButtons)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_TARGET_DRIVES_LIST, OnLvnItemChangedDrives)
    ON_NOTIFY(NM_SETFOCUS, IDC_TARGET_DRIVES_LIST, &CSelectDrivesDlg::OnNMSetfocusTargetDrivesList)
    ON_REGISTERED_MESSAGE(WMU_OK, OnWmuOk)
    ON_REGISTERED_MESSAGE(WMU_THREADFINISHED, OnWmDriveInfoThreadFinished)
    ON_WM_CTLCOLOR()
    ON_WM_SYSCOLORCHANGE()
END_MESSAGE_MAP()

BOOL CSelectDrivesDlg::OnInitDialog()
{
    CWaitCursor wc;

    CLayoutDialogEx::OnInitDialog();

    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(GetSafeHwnd());

    ModifyStyle(0, WS_CLIPCHILDREN);

    m_layout.AddControl(IDOK, 1, 1, 0, 0);
    m_layout.AddControl(IDCANCEL, 1, 1, 0, 0);
    m_layout.AddControl(IDC_TARGET_DRIVES_LIST, 0, 0, 1, 1);
    m_layout.AddControl(IDC_RADIO_TARGET_DRIVES_ALL, 0, 0, 1, 0);
    m_layout.AddControl(IDC_RADIO_TARGET_FOLDER, 0, 1, 0, 0);
    m_layout.AddControl(IDC_BROWSE_BUTTON, 1, 1, 0, 0);
    m_layout.AddControl(IDC_BROWSE_FOLDER, 0, 1, 1, 0);
    m_layout.AddControl(IDC_FAST_SCAN_CHECKBOX, 0, 1, 1, 0);
    m_layout.AddControl(IDC_SCAN_DUPLICATES, 0, 1, 1, 0);

    // Update checkbox text based on elevation status
    if (!IsElevationActive())
    {
        // Show unavailable message when user is not elevated
        CButton* pCheckbox = static_cast<CButton*>(GetDlgItem(IDC_FAST_SCAN_CHECKBOX));
        pCheckbox->SetWindowText(std::format(L"{} ({})",
            Localization::Lookup(IDS_FAST_SCAN_CHECKBOX),
            Localization::Lookup(IDS_ELEVATION_REQUIRED)).c_str());
    }

    m_layout.OnInitDialog(true);

    m_driveList.ShowGrid(COptions::ListGrid);
    m_driveList.ShowStripes(COptions::ListStripes);
    m_driveList.ShowFullRowSelection(COptions::ListFullRowSelection);
    m_driveList.SetExtendedStyle(m_driveList.GetExtendedStyle() | LVS_EX_HEADERDRAGDROP);

    m_driveList.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_NAME).c_str(), LVCFMT_LEFT, DpiRest(150), COL_DRIVES_NAME);
    m_driveList.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_TOTAL).c_str(), LVCFMT_RIGHT, DpiRest(65), COL_DRIVES_TOTAL);
    m_driveList.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_FREE).c_str(), LVCFMT_RIGHT, DpiRest(65), COL_DRIVES_FREE);
    m_driveList.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_USED_TOTAL).c_str(), LVCFMT_LEFT, DpiRest(100), COL_DRIVES_GRAPH);
    m_driveList.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_USED_TOTAL).c_str(), LVCFMT_RIGHT, DpiRest(75), COL_DRIVES_PERCENT_USED);

    m_driveList.OnColumnsInserted();

    // Add previously used folders to the combo box
    for (const auto& folder : COptions::SelectDrivesFolder.Obj())
    {
        m_browseList.AddString(folder.c_str());
    }

    // Select the first item and prime m_folderName from it
    if (m_browseList.GetCount() > 0)
    {
        m_browseList.SetCurSel(0);
        m_folderName = COptions::SelectDrivesFolder.Obj().front().c_str();
    }

    CBitmap bitmap;
    bitmap.Attach(Icons::Make<Icons::PaintFileSelect>(16, 16));
    m_browseButton.SetBitmap(bitmap);

    ShowWindow(SW_SHOWNORMAL);
    UpdateWindow();
    BringWindowToTop();
    SetForegroundWindow();

    // Read persisted settings
    m_scanDuplicates = COptions::ScanForDuplicates;
    m_useFastScan = COptions::UseFastScanEngine;
    m_radio = COptions::SelectDrivesRadio;
    m_selectedDrives = COptions::SelectDrivesDrives;

    const auto driveList = GetDriveList({ DRIVE_REMOVABLE, DRIVE_FIXED,
        DRIVE_REMOTE, DRIVE_CDROM, DRIVE_RAMDISK });
    m_suppressItemChanged = true;
    for (const auto & drive : driveList)
    {
        const auto item = new CDriveItem(&m_driveList, drive + L'\\');
        m_driveList.InsertListItem(m_driveList.GetItemCount(), { item });
        item->StartQuery(m_hWnd);

        if (std::ranges::find(m_selectedDrives, drive) != m_selectedDrives.end())
        {
            m_driveList.SelectItem(item);
        }
    }
    m_driveList.SortItems();
    PostMessage(WM_APP);

    // Create list of local drives to append "All Local Drives" option
    std::vector<std::wstring> localDrives;
    for (const int i : std::views::iota(0, m_driveList.GetItemCount()))
    {
        if (const CDriveItem* item = m_driveList.GetItem(i);
            !item->IsRemote() && !item->IsSUBSTed())
        {
            localDrives.emplace_back(item->GetDrive());
        }
    }

    // Append list of local drives to "All Local Drives" option
    SetDlgItemText(IDC_RADIO_TARGET_DRIVES_ALL, std::format(L"{} ({})",
        Localization::Lookup(IDS_DRIVES_ALL), JoinString(localDrives, L' ')).c_str());

    UpdateData(FALSE);

    if (m_radio == RADIO_TARGET_DRIVES_SUBSET)
        m_driveList.SetFocus();
    else
        m_okButton.SetFocus();

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
        if (!m_folderName.IsEmpty() && m_folderName.GetAt(m_folderName.GetLength() - 1) == L':') m_folderName.AppendChar(L'\\');
        m_folderName = ResolveFullPath(m_folderName.GetString()).c_str();

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

    for (const int i : std::views::iota(0, m_driveList.GetItemCount()))
    {
        const CDriveItem* item = m_driveList.GetItem(i);
        const bool selected = m_driveList.IsItemSelected(i);

        // m_selectedDrives persists the user's manual selection across sessions
        if (selected)
        {
            m_selectedDrives.emplace_back(item->GetDrive());
        }

        // m_drives is the set of paths actually handed to the scanner
        if ((m_radio == RADIO_TARGET_DRIVES_ALL && !item->IsRemote() && !item->IsSUBSTed()) ||
            (m_radio == RADIO_TARGET_DRIVES_SUBSET && selected))
        {
            m_drives.emplace_back(item->GetDrive());
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
    switch (m_radio)
    {
    case RADIO_TARGET_DRIVES_ALL:
        enableOk = true;
        break;
    case RADIO_TARGET_DRIVES_SUBSET:
        enableOk = m_driveList.GetSelectedCount() > 0;
        break;
    case RADIO_TARGET_FOLDER:
        if (!m_folderName.IsEmpty())
        {
            enableOk = (m_folderName.GetLength() >= 2 && m_folderName.Left(2) == L"\\\\") ||
                       FinderBasic::DoesFileExist(m_folderName.GetString());
        }
        break;
    default:
        ASSERT(FALSE);
    }
    m_okButton.EnableWindow(enableOk);
}

void CSelectDrivesDlg::OnBnClickedFastScanCheckbox()
{
    // Prompt to re-launch elevated if the user just enabled Fast Scan without elevation
    if (IsDlgButtonChecked(IDC_FAST_SCAN_CHECKBOX) != BST_UNCHECKED && !IsElevationActive() && IsElevationAvailable())
    {
        if (WdsMessageBox(*this, Localization::Lookup(IDS_ELEVATION_QUESTION),
            wds::strWinDirStat, MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            COptions::UseFastScanEngine = true;
            RunElevated(CDirStatDoc::Get()->GetPathName().GetString());
            return;
        }
    }
    UpdateButtons();
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

void CSelectDrivesDlg::OnBnDoubleclickedRadio()
{
    UpdateButtons();

    if (m_okButton.IsWindowEnabled())
    {
        SendMessage(WMU_OK);
    }
}

void CSelectDrivesDlg::OnLvnItemChangedDrives(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    if (m_suppressItemChanged) { *pResult = FALSE; return; }
    SetActiveRadio(IDC_RADIO_TARGET_DRIVES_SUBSET);
    UpdateButtons();

    *pResult = FALSE;
}

void CSelectDrivesDlg::OnBnClickedUpdateButtons()
{
    UpdateButtons();
}

LRESULT CSelectDrivesDlg::OnInitComplete(WPARAM, LPARAM)
{
    m_suppressItemChanged = false;
    return 0;
}

LRESULT CSelectDrivesDlg::OnWmuOk(WPARAM, LPARAM)
{
    OnOK();
    return 0;
}

LRESULT CSelectDrivesDlg::OnWmDriveInfoThreadFinished(const WPARAM wParam, const LPARAM lparam)
{
    const auto item = std::bit_cast<CDriveItem*>(lparam);
    const bool success = (wParam != 0);

    // Item may have already been deleted during dialog teardown; nothing to update
    LVFINDINFO fi{ .flags = LVFI_PARAM, .lParam = lparam };
    if (m_driveList.FindItem(&fi) == -1)
    {
        return 0;
    }

    // Update the item with the query result (data written by thread) and recompute m_used
    item->SetDriveInformation(success);

    m_suppressItemChanged = true;
    m_driveList.SortItems();
    m_suppressItemChanged = false;

    return 0;
}

void CSelectDrivesDlg::OnSysColorChange()
{
    CLayoutDialogEx::OnSysColorChange();
    m_driveList.SysColorChanged();
}

void CSelectDrivesDlg::OnNMSetfocusTargetDrivesList(NMHDR*, LRESULT* pResult)
{
    if (m_driveList.GetItemCount() > 0 && m_driveList.GetSelectedCount() == 0)
    {
        m_driveList.SelectItem(m_driveList.GetItem(0));
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

    // Intercept VK_DELETE to remove the highlighted history item from both UI and persistent options
    else if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_DELETE && m_browseList.GetDroppedState())
    {
        if (pMsg->hwnd == m_browseList.m_hWnd || ::GetParent(pMsg->hwnd) == m_browseList.m_hWnd)
        {
            const int n = m_browseList.GetCurSel();
            auto& h = COptions::SelectDrivesFolder.Obj();
            if (n != CB_ERR && n < static_cast<int>(h.size()))
            {
                h.erase(h.begin() + n);
                m_browseList.DeleteString(n);
                const int cnt = m_browseList.GetCount();

                if (cnt > 0)
                {
                    const int newSel = min(n, cnt - 1);
                    m_browseList.SetCurSel(newSel);
                    m_browseList.GetLBText(newSel, m_folderName);
                }
                else
                {
                    m_folderName = wds::strEmpty;
                }

                UpdateData(FALSE);
                UpdateButtons();
                return TRUE;
            }
        }
    }

    return CLayoutDialogEx::PreTranslateMessage(pMsg);
}

std::vector<std::wstring> CSelectDrivesDlg::GetSelectedItems() const
{
    if (m_radio == RADIO_TARGET_FOLDER)
    {
        return { m_folderName.GetString() };
    }
    return m_drives; // valid for both RADIO_TARGET_DRIVES_ALL and RADIO_TARGET_DRIVES_SUBSET
}

HBRUSH CSelectDrivesDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CLayoutDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CSelectDrivesDlg::OnBnClickedBrowseButton()
{
    // Setup folder picker dialog
    CFolderPickerDialog dlg(nullptr,
        OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_DONTADDTORECENT, this);
    dlg.m_ofn.lpstrTitle = const_cast<LPWSTR>(wds::strWinDirStat);

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
