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
#include "Filtering.h"
#include "SelectDrivesDlg.h"
#include "FinderBasic.h"
#include "FinderMtp.h"

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

    // Return: false, if drive not accessible
    bool RetrieveDriveInformation(const std::wstring& path, std::wstring& name, ULONGLONG& total, ULONGLONG& freeBytes)
    {
        // Query MTP devices through their Shell storage metadata
        if (FinderMtp::IsPath(path)) return FinderMtp::GetDriveInfo(path, name, total, freeBytes);

        name = FormatVolumeNameOfRootPath(path);
        std::tie(total, freeBytes) = CDirStatApp::GetFreeDiskSpace(path);
        return total != 0;
    }

    std::wstring ResolveFullPath(const std::wstring& relativePath)
    {
        const SmartPointer path(free, _wfullpath(nullptr, relativePath.c_str(), 0));
        return path != nullptr ? static_cast<LPWSTR>(path) : relativePath;
    }
}

/////////////////////////////////////////////////////////////////////////////

CDriveItem::CDriveItem(CDrivesList* list, const std::wstring& pszPath, std::wstring name)
    : m_driveList(list)
    , m_path(pszPath)
    , m_mtp(FinderMtp::IsPath(m_path))
    , m_icon(GetIconHandler()->FetchShellIcon(m_path))
    , m_isRemote(!m_mtp && DRIVE_REMOTE == ::GetDriveType(m_path.c_str()))
    , m_subst(!m_mtp && IsSUBSTedDrive(m_path))
    , m_name(m_mtp && !name.empty() ? std::move(name) : m_path) {}

CDriveItem::~CDriveItem()
{
    StopQuery();
}

void CDriveItem::StartQuery(const HWND dialog)
{
    assert(dialog != nullptr);
    assert(!m_queryThread.joinable()); // must not be called while a query is in progress

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
            if (!name.empty()) m_name = std::move(name);
            m_totalBytes = total;
            m_freeBytes = free;
        }

        if (const HWND dialog = m_dialog.load(); dialog != nullptr)
        {
            ::PostMessage(dialog, WM_WDS_DRIVE_INFO_FINISHED, success ? 1 : 0, reinterpret_cast<LPARAM>(this));
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
        default: assert(false);
    }

    return 0;
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
        rc.Deflate(3, 5);
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
        assert(false);
    }

    return s;
}

std::wstring CDriveItem::GetDrive() const
{
    return m_mtp ? m_path : ::GetDrive(m_path);
}

/////////////////////////////////////////////////////////////////////////////

CDrivesList::CDrivesList()
    : MessageTarget(COptions::DriveListColumnOrder.Ptr(), COptions::DriveListColumnWidths.Ptr(), COptions::DriveListColumnVisibility.Ptr())
{
}

void CDrivesList::SortItems()
{
    const ScopedValue sorting(m_sortInProgress, true);
    CWdsListControl::SortItems();
}

void CDrivesList::SelectItem(const CDriveItem* item)
{
    const int i = FindListItem(item);
    SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
}

void CDrivesList::OnDoubleClick(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    *pResult = false;

    const CPoint point = ToClient(CurrentMessage().pt);
    const int i = HitTest(point);
    if (i < 0) return;

    SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

    (void) GetParent()->SendMessage(WM_WDS_SELECT_DRIVES_OK);
}

void CDrivesList::OnLvnDeleteItem(NMHDR* pNMHDR, LRESULT* pResult) const
{
    const auto pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    delete GetItem(pNMLV->iItem);
    *pResult = false;
}

/////////////////////////////////////////////////////////////////////////////

CSelectDrivesDlg::CSelectDrivesDlg(CWnd* pParent) :
    MessageTarget(IDD, COptions::DriveSelectWindowRect.Ptr(), pParent)
{
}

bool CSelectDrivesDlg::OnInitDialog()
{
    CWaitCursor wc;

    CLayoutDialog::OnInitDialog();

    m_driveList.SubclassDlgItem(IDC_TARGET_DRIVES_LIST, this);
    m_okButton.SubclassDlgItem(IDOK, this);
    m_browseList.SubclassDlgItem(IDC_BROWSE_FOLDER, this);
    m_browseButton.SubclassDlgItem(IDC_BROWSE_BUTTON, this);
    m_filterButton.SubclassDlgItem(IDC_FILTER_BUTTON, this);

    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(Handle());

    ModifyStyle(0, WS_CLIPCHILDREN);

    m_layout.AddControl(IDOK, 1, 1, 0, 0);
    m_layout.AddControl(IDCANCEL, 1, 1, 0, 0);
    m_layout.AddControl(IDC_TARGET_DRIVES_LIST, 0, 0, 1, 1);
    m_layout.AddControl(IDC_RADIO_TARGET_DRIVES_ALL, 0, 0, 1, 0);
    m_layout.AddControl(IDC_RADIO_TARGET_DRIVES_SUBSET, 0, 0, 1, 0);
    m_layout.AddControl(IDC_RADIO_TARGET_FOLDER, 0, 1, 0, 0);
    m_layout.AddControl(IDC_BROWSE_BUTTON, 1, 1, 0, 0);
    m_layout.AddControl(IDC_BROWSE_FOLDER, 0, 1, 1, 0);
    m_layout.AddControl(IDC_FAST_SCAN_CHECKBOX, 0, 1, 1, 0);
    m_layout.AddControl(IDC_SCAN_DUPLICATES, 0, 1, 1, 0);
    m_layout.AddControl(IDC_FILTER_BUTTON, 0, 1, 0, 0);

    // Update checkbox text based on elevation status
    if (!IsElevationActive())
    {
        // Show unavailable message when user is not elevated
        if (CWnd* checkbox = GetDlgItem(IDC_FAST_SCAN_CHECKBOX); checkbox != nullptr)
        {
            checkbox->SetText(std::format(L"{} ({})",
                Localization::Lookup(IDS_FAST_SCAN_CHECKBOX),
                Localization::Lookup(IDS_ELEVATION_REQUIRED)).c_str());
        }
    }

    m_layout.OnInitDialog(true);

    m_driveList.ShowGrid(COptions::ListGrid);
    m_driveList.ShowStripes(COptions::ListStripes);
    m_driveList.ShowFullRowSelection(COptions::ListFullRowSelection);
    m_driveList.SetExtendedStyle(m_driveList.GetExtendedStyle() | LVS_EX_HEADERDRAGDROP | LVS_EX_FULLROWSELECT);

    m_driveList.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_NAME).c_str(), LVCFMT_LEFT, ScaleForDpi(150), COL_DRIVES_NAME);
    m_driveList.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_TOTAL).c_str(), LVCFMT_RIGHT, ScaleForDpi(65), COL_DRIVES_TOTAL);
    m_driveList.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_FREE).c_str(), LVCFMT_RIGHT, ScaleForDpi(65), COL_DRIVES_FREE);
    m_driveList.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_USED_TOTAL).c_str(), LVCFMT_LEFT, ScaleForDpi(100), COL_DRIVES_GRAPH);
    m_driveList.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_USED_TOTAL).c_str(), LVCFMT_RIGHT, ScaleForDpi(75), COL_DRIVES_PERCENT_USED);

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
        m_folderName = COptions::SelectDrivesFolder.Obj().front();
    }

    m_browseIcon = Icons::MakeIcon(ScaleForDpi(16), Icons::PaintFileSelect);
    m_browseButton.SetIcon(m_browseIcon);
    UpdateFilterButton();

    ShowWindow(SW_SHOWNORMAL);
    UpdateWindow();
    BringWindowToTop();
    SetForegroundWindow();

    // Read persisted settings
    SetChecked(IDC_SCAN_DUPLICATES, COptions::ScanForDuplicates);
    SetChecked(IDC_FAST_SCAN_CHECKBOX, COptions::UseFastScanEngine);
    SetActiveRadio(IDC_RADIO_TARGET_DRIVES_ALL + COptions::SelectDrivesRadio);
    m_selectedDrives = COptions::SelectDrivesDrives;

    {
        const auto driveList = GetDriveList({ DRIVE_REMOVABLE, DRIVE_FIXED,
            DRIVE_REMOTE, DRIVE_CDROM, DRIVE_RAMDISK });
        const bool wasSuppressingItemChanged = m_suppressItemChanged;
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

        // Add shell-backed portable devices alongside the filesystem drive list
        for (const auto& device : FinderMtp::GetDevices())
        {
            const auto item = new CDriveItem(&m_driveList, device.path, device.name);
            m_driveList.InsertListItem(m_driveList.GetItemCount(), { item });
            item->StartQuery(m_hWnd);

            if (std::ranges::find(m_selectedDrives, device.path) != m_selectedDrives.end())
            {
                m_driveList.SelectItem(item);
            }
        }
        m_driveList.SortItems();
        m_suppressItemChanged = wasSuppressingItemChanged;
    }

    // Create list of local drives to append "All Local Drives" option
    std::vector<std::wstring> localDrives;
    for (const int i : std::views::iota(0, m_driveList.GetItemCount()))
    {
        if (const CDriveItem* item = m_driveList.GetItem(i);
            !item->IsMtp() && !item->IsRemote() && !item->IsSUBSTed())
        {
            localDrives.emplace_back(item->GetDrive());
        }
    }

    // Append list of local drives to "All Local Drives" option
    SetText(IDC_RADIO_TARGET_DRIVES_ALL, std::format(L"{} ({})",
        Localization::Lookup(IDS_DRIVES_ALL), JoinString(localDrives, L' ')));

    if (COptions::SelectDrivesRadio == RADIO_TARGET_DRIVES_SUBSET)
        m_driveList.SetFocus();
    else
        m_okButton.SetFocus();

    UpdateButtons();
    return false; // we have set the focus.
}

void CSelectDrivesDlg::OnOK()
{
    m_radio = CheckedRadioButton(IDC_RADIO_TARGET_DRIVES_ALL, IDC_RADIO_TARGET_FOLDER) - IDC_RADIO_TARGET_DRIVES_ALL;
    m_folderName = GetText(IDC_BROWSE_FOLDER);

    m_drives.clear();
    m_selectedDrives.clear();
    if (m_radio == RADIO_TARGET_FOLDER)
    {
        if (!m_folderName.empty() && m_folderName.back() == L':') m_folderName.push_back(L'\\');
        m_folderName = ResolveFullPath(m_folderName);

        // Remove the folder from the most recently used list to avoid duplicates
        const std::wstring& folderName = m_folderName;
        std::erase_if(COptions::SelectDrivesFolder.Obj(), [&folderName](const std::wstring& s) {
            return _wcsicmp(s.c_str(), folderName.c_str()) == 0;
        });

        // Insert it at the beginning of the used list
        COptions::SelectDrivesFolder.Obj().insert(
          COptions::SelectDrivesFolder.Obj().begin(), folderName);

        // Limit the folder history to the configured count
        COptions::SelectDrivesFolder.Obj().resize(std::min(static_cast<size_t>(COptions::FolderHistoryCount),
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
        // Keep "All Local Drives" limited to direct filesystem volumes
        if ((m_radio == RADIO_TARGET_DRIVES_ALL && !item->IsMtp() && !item->IsRemote() && !item->IsSUBSTed()) ||
            (m_radio == RADIO_TARGET_DRIVES_SUBSET && selected))
        {
            m_drives.emplace_back(item->GetDrive());
        }
    }

    COptions::SelectDrivesRadio = m_radio;
    COptions::SelectDrivesDrives = m_selectedDrives;
    COptions::ScanForDuplicates = IsChecked(IDC_SCAN_DUPLICATES);
    COptions::UseFastScanEngine = IsChecked(IDC_FAST_SCAN_CHECKBOX);

    // Switch focus to file tree view
    const auto tabbedView = CMainFrame::Get()->GetFileTabbedView();
    tabbedView->SetActiveFileTreeView();

    CLayoutDialog::OnOK();
}

void CSelectDrivesDlg::UpdateButtons(const std::wstring* const folderOverride)
{
    const int currentRadio = CheckedRadioButton(IDC_RADIO_TARGET_DRIVES_ALL, IDC_RADIO_TARGET_FOLDER) - IDC_RADIO_TARGET_DRIVES_ALL;
    const std::wstring currentFolder = folderOverride == nullptr ? GetText(IDC_BROWSE_FOLDER) : *folderOverride;

    bool enableOk = false;
    switch (currentRadio)
    {
    case RADIO_TARGET_DRIVES_ALL:
        enableOk = true;
        break;
    case RADIO_TARGET_DRIVES_SUBSET:
        enableOk = m_driveList.GetSelectedCount() > 0;
        break;
    case RADIO_TARGET_FOLDER:
        if (!currentFolder.empty())
        {
            enableOk = (currentFolder.size() >= 2 && currentFolder.starts_with(L"\\\\")) ||
                       FinderBasic::DoesFileExist(currentFolder);
        }
        break;
    default:
        assert(false);
    }
    m_okButton.EnableWindow(enableOk);
}

void CSelectDrivesDlg::UpdateFilterButton()
{
    const bool active = CFiltering::IsFilterActive();
    m_filterIcon = Icons::MakeIcon(ScaleForDpi(20), [active](auto& g) { Icons::PaintFilter(g, active); });
    m_filterButton.SetIcon(m_filterIcon);
}

void CSelectDrivesDlg::OnBnClickedFastScanCheckbox()
{
    // Prompt to re-launch elevated if the user just enabled Fast Scan without elevation
    if (ButtonCheckState(IDC_FAST_SCAN_CHECKBOX) != BST_UNCHECKED && !IsElevationActive() && IsElevationAvailable())
    {
        if (ShowMessageBox(*this, Localization::Lookup(IDS_ELEVATION_QUESTION),
            wds::strWinDirStat, MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            COptions::UseFastScanEngine = true;
            RunElevated(CWinDirStatModel::Get()->GetScanPathSpec());
            return;
        }
    }
    UpdateButtons();
}

void CSelectDrivesDlg::OnBnClickedRadioTargetDrivesAll()
{
    SetActiveRadio(IDC_RADIO_TARGET_DRIVES_ALL);
    UpdateButtons();
}

void CSelectDrivesDlg::OnBnClickedRadioTargetDrivesSubset()
{
    SetActiveRadio(IDC_RADIO_TARGET_DRIVES_SUBSET);
    UpdateButtons();
}

void CSelectDrivesDlg::OnBnClickedRadioTargetFolder()
{
    SetActiveRadio(IDC_RADIO_TARGET_FOLDER);
    UpdateButtons();
}

void CSelectDrivesDlg::OnBnDoubleclickedRadio()
{
    UpdateButtons();

    if (m_okButton.IsWindowEnabled())
    {
        SendMessage(WM_WDS_SELECT_DRIVES_OK);
    }
}

void CSelectDrivesDlg::OnLvnItemChangedDrives(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    if (m_suppressItemChanged || m_driveList.IsSorting())
    {
        *pResult = false;
        return;
    }
    SetActiveRadio(IDC_RADIO_TARGET_DRIVES_SUBSET);
    UpdateButtons();

    *pResult = false;
}

void CSelectDrivesDlg::OnBnClickedUpdateButtons()
{
    UpdateButtons();
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

    const bool wasSuppressingItemChanged = m_suppressItemChanged;
    m_suppressItemChanged = true;
    m_driveList.SortItems();
    m_suppressItemChanged = wasSuppressingItemChanged;

    return 0;
}

void CSelectDrivesDlg::OnSysColorChange()
{
    CLayoutDialog::OnSysColorChange();
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

bool CSelectDrivesDlg::PreprocessMessage(MSG* pMsg)
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
                    const int newSel = std::min(n, cnt - 1);
                    m_browseList.SetCurSel(newSel);
                    m_folderName = m_browseList.ItemText(newSel);
                }
                else
                {
                    m_folderName = wds::strEmpty;
                    SetText(IDC_BROWSE_FOLDER, m_folderName);
                }

                UpdateButtons();
                return true;
            }
        }
    }

    return CLayoutDialog::PreprocessMessage(pMsg);
}

std::vector<std::wstring> CSelectDrivesDlg::GetSelectedItems() const
{
    if (m_radio == RADIO_TARGET_FOLDER)
    {
        return { m_folderName };
    }
    return m_drives; // valid for both RADIO_TARGET_DRIVES_ALL and RADIO_TARGET_DRIVES_SUBSET
}

HBRUSH CSelectDrivesDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, const UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CLayoutDialog::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CSelectDrivesDlg::OnBnClickedBrowseButton()
{
    // Show dialog and validate results
    const auto selectedFolder = PickFolder(this);
    if (!selectedFolder) return;
    const std::wstring& path = *selectedFolder;

    if (!FinderBasic::DoesFileExist(path)) return;
    SetText(IDC_BROWSE_FOLDER, path);

    SetActiveRadio(IDC_RADIO_TARGET_FOLDER);
    UpdateButtons();
}

void CSelectDrivesDlg::OnBnClickedFilterButton()
{
    CSettingsSheet::ShowSettings(1, false); // 1 = Filtering tab
    UpdateFilterButton();
}

void CSelectDrivesDlg::OnEditchangeBrowseFolder()
{
    // Force assessing folder to make the okay button light up
    SetActiveRadio(IDC_RADIO_TARGET_FOLDER);
    UpdateButtons();
}

void CSelectDrivesDlg::OnSelchangeBrowseFolder()
{
    SetActiveRadio(IDC_RADIO_TARGET_FOLDER);

    const int selection = m_browseList.GetCurSel();
    if (selection == CB_ERR)
    {
        UpdateButtons();
        return;
    }

    const std::wstring selectedFolder = m_browseList.ItemText(selection);
    UpdateButtons(&selectedFolder);
}

void CSelectDrivesDlg::SetActiveRadio(const int radio)
{
    SetCheckedRadioButton(IDC_RADIO_TARGET_DRIVES_ALL, IDC_RADIO_TARGET_FOLDER, radio);

    // Keep the active mode's input first in the keyboard order.
    const int firstInput = radio == IDC_RADIO_TARGET_FOLDER ? IDC_BROWSE_FOLDER : IDC_TARGET_DRIVES_LIST;
    const int secondInput = radio == IDC_RADIO_TARGET_FOLDER ? IDC_TARGET_DRIVES_LIST : IDC_BROWSE_FOLDER;
    GetDlgItem(secondInput)->SetWindowPos(GetDlgItem(firstInput), 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}
