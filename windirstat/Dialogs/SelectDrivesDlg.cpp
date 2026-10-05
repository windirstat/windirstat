// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "Filtering.h"
#include "SelectDrivesDlg.h"
#include "FinderBasic.h"
#include "FinderMtp.h"
#include "StorageSource.h"

class S3ConnectionDialog final : public CDialog
{
public:
    S3ConnectionDialog(CWnd* parent, const std::wstring& current) : CDialog(IDD_S3_CONNECTION, parent)
    {
        if (const auto parsed = StorageSource::Parse(current); parsed && parsed->protocol == RemoteProtocol::S3)
            target = *parsed;
    }

    RemoteTarget target;

    bool OnInitDialog() override
    {
        CDialog::OnInitDialog();
        SetIcon(LoadIconW(GetAppInstance(), MAKEINTRESOURCEW(IDR_MAINFRAME)), false);
        Localization::UpdateDialogs(*this);
        DarkMode::AdjustControls(Handle());
        SetText(IDC_S3_BUCKET, target.bucket);
        SetText(IDC_REMOTE_PREFIX, target.prefix);
        SetText(IDC_S3_REGION, target.region);
        SetText(IDC_REMOTE_ENDPOINT, target.endpoint);
        const auto credentials = StorageSource::GetCredentials(target);
        SetText(IDC_S3_ACCESS, credentials.user);
        SetText(IDC_REMOTE_SECRET, credentials.secret);
        SetText(IDC_S3_TOKEN, credentials.token);
        UpdateButtons();
        return true;
    }

    bool OnCommand(const WPARAM wParam, const LPARAM lParam) override
    {
        // Keep confirmation in sync with edits to the connection settings.
        if (lParam != 0 && HIWORD(wParam) == EN_CHANGE)
        {
            UpdateButtons();
            return true;
        }
        return CDialog::OnCommand(wParam, lParam);
    }

    void OnOK() override
    {
        // Revalidate before accepting the target.
        const auto parsed = ReadTarget();
        if (!parsed) return;
        target = *parsed;
        const RemoteCredentials credentials{
            GetText(IDC_S3_ACCESS), GetText(IDC_REMOTE_SECRET), GetText(IDC_S3_TOKEN) };
        const DWORD error = StorageSource::SetCredentials(target, credentials, IsChecked(IDC_REMOTE_REMEMBER));
        if (error != ERROR_SUCCESS) { DisplayError(TranslateError(error)); return; }
        CDialog::OnOK();
    }

private:
    std::optional<RemoteTarget> ReadTarget() const
    {
        // Require explicit connection settings and credentials without contacting the server.
        if (GetText(IDC_S3_ACCESS).empty() || GetText(IDC_REMOTE_SECRET).empty()) return {};
        RemoteTarget settings;
        settings.bucket = GetText(IDC_S3_BUCKET);
        settings.prefix = GetText(IDC_REMOTE_PREFIX);
        settings.region = GetText(IDC_S3_REGION);
        settings.endpoint = GetText(IDC_REMOTE_ENDPOINT);
        for (auto* value : { &settings.bucket, &settings.region, &settings.endpoint })
            TrimString(*value);
        return StorageSource::Parse(settings);
    }

    void UpdateButtons() { GetDlgItem(IDOK).EnableWindow(ReadTarget().has_value()); }
};

class RemoteConnectionDialog final : public CDialog
{
public:
    RemoteConnectionDialog(CWnd* parent, const std::wstring& current, const RemoteProtocol protocol) :
        CDialog(protocol == RemoteProtocol::Azure ? IDD_AZURE_CONNECTION : IDD_WEBDAV_CONNECTION, parent)
    {
        target.protocol = protocol;
        if (const auto parsed = StorageSource::Parse(current); parsed && parsed->protocol == protocol) target = *parsed;
    }

    RemoteTarget target;

    bool OnInitDialog() override
    {
        CDialog::OnInitDialog();
        SetIcon(LoadIconW(GetAppInstance(), MAKEINTRESOURCEW(IDR_MAINFRAME)), false);
        Localization::UpdateDialogs(*this);
        DarkMode::AdjustControls(Handle());
        const bool azure = target.protocol == RemoteProtocol::Azure;
        const auto credentials = StorageSource::GetCredentials(target);
        SetText(IDC_REMOTE_ENDPOINT, azure || target.endpoint.empty() ? target.endpoint : target.ToString());
        SetText(IDC_REMOTE_ACCOUNT, azure ? target.account : credentials.user);
        SetText(IDC_REMOTE_SECRET, credentials.secret);
        if (azure)
        {
            SetText(IDC_REMOTE_CONTAINER, target.bucket);
            SetText(IDC_REMOTE_PREFIX, target.prefix);
        }
        UpdateButtons();
        return true;
    }

    bool OnCommand(const WPARAM wParam, const LPARAM lParam) override
    {
        // Keep confirmation in sync with edits to the connection settings.
        if (lParam != 0 && HIWORD(wParam) == EN_CHANGE)
        {
            UpdateButtons();
            return true;
        }
        return CDialog::OnCommand(wParam, lParam);
    }

    void OnOK() override
    {
        // Validate the connection locally, keeping secrets outside history and saved scan paths.
        const bool azure = target.protocol == RemoteProtocol::Azure;
        const auto parsed = ReadTarget();
        if (!parsed) return;
        target = *parsed;
        if (!azure && !target.prefix.empty() && !target.prefix.ends_with(L'/')) target.prefix += L'/';
        RemoteCredentials credentials{ GetText(IDC_REMOTE_ACCOUNT), GetText(IDC_REMOTE_SECRET) };
        const DWORD error = StorageSource::SetCredentials(target, credentials, IsChecked(IDC_REMOTE_REMEMBER));
        if (error != ERROR_SUCCESS) { DisplayError(TranslateError(error)); return; }
        CDialog::OnOK();
    }

private:
    std::optional<RemoteTarget> ReadTarget() const
    {
        // Require explicit connection settings and credentials without contacting the server.
        if (GetText(IDC_REMOTE_ACCOUNT).empty() || GetText(IDC_REMOTE_SECRET).empty()) return {};
        const bool azure = target.protocol == RemoteProtocol::Azure;
        RemoteTarget settings = target;
        std::wstring endpoint = GetText(IDC_REMOTE_ENDPOINT);
        TrimString(endpoint);
        if (azure)
        {
            settings.endpoint = endpoint;
            settings.account = GetText(IDC_REMOTE_ACCOUNT);
            settings.bucket = GetText(IDC_REMOTE_CONTAINER);
            settings.prefix = GetText(IDC_REMOTE_PREFIX);
            TrimString(settings.account); TrimString(settings.bucket);
        }
        const auto parsed = azure ? StorageSource::Parse(settings) : StorageSource::Parse(endpoint);
        return parsed && parsed->protocol == target.protocol ? parsed : std::nullopt;
    }

    void UpdateButtons() { GetDlgItem(IDOK).EnableWindow(ReadTarget().has_value()); }
};

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

void CDriveItem::StartQuery()
{
    // Only the GUI thread accesses this registry; reuse queries still pending when the dialog is reopened.
    static std::map<std::wstring, std::shared_future<DriveInformation>> queries;
    std::erase_if(queries, [](const auto& entry) {
        return entry.second.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    });
    if (const auto it = queries.find(m_path); it != queries.end())
    {
        m_query = it->second;
        return;
    }

    try
    {
        std::packaged_task<DriveInformation()> query([path = m_path]
        {
            SetThreadErrorMode(GetThreadErrorMode() | SEM_FAILCRITICALERRORS, nullptr);
            DriveInformation info;
            info.success = RetrieveDriveInformation(path, info.name, info.total, info.free);
            return info;
        });
        m_query = query.get_future().share();
        queries.emplace(m_path, m_query);

        // The worker owns its inputs and result; neither dialog teardown nor future destruction waits for it.
        std::thread(std::move(query)).detach();
    }
    catch (...)
    {
        queries.erase(m_path);
        m_query = {};
    }
}

bool CDriveItem::UpdateDriveInformation()
{
    if (!m_query.valid() || m_query.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return false;

    try
    {
        const auto& info = m_query.get();
        m_success = info.success;
        if (m_success)
        {
            if (!info.name.empty()) m_name = info.name;
            m_totalBytes = info.total;
            m_freeBytes = info.free;

            // guard against quotas where free may exceed total, or total is zero
            m_used = (m_totalBytes > 0 && m_totalBytes >= m_freeBytes)
                ? static_cast<double>(m_totalBytes - m_freeBytes) / m_totalBytes
                : 0.0;
        }
    }
    catch (...)
    {
        m_success = false;
    }
    m_query = {};

    return true;
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
        if (IsQuerying())
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

    (void) GetParent().SendMessage(WM_WDS_SELECT_DRIVES_OK);
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
    m_addFolderButton.SubclassDlgItem(IDC_ADD_FOLDER, this);
    m_filterButton.SubclassDlgItem(IDC_FILTER_BUTTON, this);

    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(Handle());

    // Open the provider menu from the whole button and center its localized label between Filter and OK.
    const auto cloudButton = GetDlgItem(IDC_CLOUD_STORAGE);
    BUTTON_SPLITINFO splitInfo{ .mask = BCSIF_STYLE, .uSplitStyle = BCSS_NOSPLIT };
    Button_SetSplitInfo(cloudButton.Handle(), &splitInfo);
    const CRect cloudRect = GetChildWindowRect(cloudButton.Handle());
    SIZE cloudSize{};
    const int cloudWidth = Button_GetIdealSize(cloudButton.Handle(), &cloudSize) ? cloudSize.cx : cloudRect.Width();
    const int cloudLeft = (GetChildWindowRect(m_filterButton.Handle()).right +
        GetChildWindowRect(m_okButton.Handle()).left - cloudWidth) / 2;
    cloudButton.SetWindowPos(nullptr, cloudLeft, cloudRect.top, cloudWidth, cloudRect.Height(),
        SWP_NOZORDER | SWP_NOACTIVATE);

    ModifyStyle(0, WS_CLIPCHILDREN);

    m_layout.AddControl(IDOK, 1, 1, 0, 0);
    m_layout.AddControl(IDCANCEL, 1, 1, 0, 0);
    m_layout.AddControl(IDC_TARGET_DRIVES_LIST, 0, 0, 1, 1);
    m_layout.AddControl(IDC_RADIO_TARGET_DRIVES_ALL, 0, 0, 1, 0);
    m_layout.AddControl(IDC_RADIO_TARGET_DRIVES_SUBSET, 0, 0, 1, 0);
    m_layout.AddControl(IDC_RADIO_TARGET_FOLDER, 0, 1, 0, 0);
    m_layout.AddControl(IDC_BROWSE_BUTTON, 1, 1, 0, 0);
    m_layout.AddControl(IDC_ADD_FOLDER, 1, 1, 0, 0);
    m_layout.AddControl(IDC_BROWSE_FOLDER, 0, 1, 1, 0);
    m_layout.AddControl(IDC_FAST_SCAN_CHECKBOX, 0, 1, 1, 0);
    m_layout.AddControl(IDC_SCAN_DUPLICATES, 0, 1, 1, 0);
    m_layout.AddControl(IDC_FILTER_BUTTON, 0, 1, 0, 0);
    m_layout.AddControl(IDC_CLOUD_STORAGE, 0.5, 1, 0, 0);

    // Update checkbox text based on elevation status
    if (!IsElevationActive())
    {
        // Show unavailable message when user is not elevated
        if (auto checkbox = GetDlgItem(IDC_FAST_SCAN_CHECKBOX); checkbox != nullptr)
        {
            checkbox.SetText(std::format(L"{} ({})",
                Localization::Lookup(IDS_FAST_SCAN_CHECKBOX),
                Localization::Lookup(IDS_ELEVATION_REQUIRED)).c_str());
        }
    }

    m_layout.OnInitDialog(true);

    m_driveList.ShowGrid(COptions::ListGrid);
    m_driveList.ShowStripes(COptions::ListStripes);
    m_driveList.ShowFullRowSelection(COptions::ListFullRowSelection);
    m_driveList.SetExtendedStyle(m_driveList.GetExtendedStyle() | LVS_EX_HEADERDRAGDROP | LVS_EX_FULLROWSELECT);

    m_driveList.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_NAME), LVCFMT_LEFT, ScaleForDpi(150), COL_DRIVES_NAME);
    m_driveList.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_TOTAL), LVCFMT_RIGHT, ScaleForDpi(65), COL_DRIVES_TOTAL);
    m_driveList.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_FREE), LVCFMT_RIGHT, ScaleForDpi(65), COL_DRIVES_FREE);
    m_driveList.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_USED_TOTAL), LVCFMT_LEFT, ScaleForDpi(100), COL_DRIVES_GRAPH);
    m_driveList.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_USED_TOTAL), LVCFMT_RIGHT, ScaleForDpi(75), COL_DRIVES_PERCENT_USED);

    m_driveList.OnColumnsInserted();

    // Add previously used folders to the combo box
    for (const auto& folder : COptions::SelectDrivesFolder.Obj())
    {
        m_browseList.AddString(folder);
    }

    // Select the first item and prime m_folderName from it
    if (m_browseList.GetCount() > 0)
    {
        m_browseList.SetCurSel(0);
        m_folderName = COptions::SelectDrivesFolder.Obj().front();
    }

    m_browseIcon = Icons::MakeIcon(ScaleForDpi(16), Icons::PaintFileSelect);
    m_browseButton.SetIcon(m_browseIcon);
    m_addFolderIcon = Icons::MakeIcon(ScaleForDpi(16), Icons::PaintFolderAppend);
    m_addFolderButton.SetIcon(m_addFolderIcon);
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
            DRIVE_REMOTE, DRIVE_CDROM, DRIVE_RAMDISK }, false, false);
        const bool wasSuppressingItemChanged = m_suppressItemChanged;
        m_suppressItemChanged = true;
        for (const auto & drive : driveList)
        {
            const auto item = new CDriveItem(&m_driveList, drive + L'\\');
            item->StartQuery();
            m_driveList.InsertListItem(m_driveList.GetItemCount(), { item });

            if (std::ranges::contains(m_selectedDrives, drive))
            {
                m_driveList.SelectItem(item);
            }
        }

        // Add shell-backed portable devices alongside the filesystem drive list
        for (const auto& device : FinderMtp::GetDevices())
        {
            const auto item = new CDriveItem(&m_driveList, device.path, device.name);
            item->StartQuery();
            m_driveList.InsertListItem(m_driveList.GetItemCount(), { item });

            if (std::ranges::contains(m_selectedDrives, device.path))
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
    SetText(IDC_RADIO_TARGET_DRIVES_ALL, std::format(L"{} (\u200E{}\u200E)",
        Localization::Lookup(IDS_DRIVES_ALL), JoinString(localDrives, L' ')));

    if (COptions::SelectDrivesRadio == RADIO_TARGET_DRIVES_SUBSET)
        m_driveList.SetFocus();
    else
        m_okButton.SetFocus();

    UpdateButtons();
    SetTimer(QUERY_TIMER_ID, 100);
    return false; // we have set the focus.
}

void CSelectDrivesDlg::OnOK()
{
    if (!SaveSelection()) return;

    // Switch focus to file tree view
    const auto tabbedView = CMainFrame::Get()->GetFileTabbedView();
    tabbedView->SetActiveFileTreeView();

    CLayoutDialog::OnOK();
}

bool CSelectDrivesDlg::SaveSelection()
{
    m_radio = GetCheckedRadioButton(IDC_RADIO_TARGET_DRIVES_ALL, IDC_RADIO_TARGET_FOLDER) - IDC_RADIO_TARGET_DRIVES_ALL;
    m_folderName = GetText(IDC_BROWSE_FOLDER);

    m_drives.clear();
    m_selectedDrives.clear();
    if (m_radio == RADIO_TARGET_FOLDER)
    {
        // Normalize every pipe-separated path on its own and rebuild the spec from the results
        const auto folders = NormalizeScanPaths(m_folderName);
        if (folders.empty()) return false;
        m_folderName = JoinString(folders);

        // Record each folder on its own rather than the joined spec: the history is persisted
        // with JoinString/SplitString on the same separator, so a joined spec stored as one
        // entry would come back as several after a restart.
        for (const auto& folder : folders | std::views::reverse)
        {
            // Remove the folder from the most recently used list to avoid duplicates
            std::erase_if(COptions::SelectDrivesFolder.Obj(), [&folder](const std::wstring& s) {
                return StorageSource::IsPath(folder) ? s == folder : _wcsicmp(s.c_str(), folder.c_str()) == 0;
            });

            // Insert it at the beginning of the used list
            COptions::SelectDrivesFolder.Obj().insert(
                COptions::SelectDrivesFolder.Obj().begin(), folder);
        }

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
    return true;
}

void CSelectDrivesDlg::UpdateButtons(const std::wstring* const folderOverride)
{
    const int currentRadio = GetCheckedRadioButton(IDC_RADIO_TARGET_DRIVES_ALL, IDC_RADIO_TARGET_FOLDER) - IDC_RADIO_TARGET_DRIVES_ALL;
    const std::wstring currentFolder = folderOverride == nullptr ? GetText(IDC_BROWSE_FOLDER) : *folderOverride;

    bool enableOk = false, remoteOnly = false;
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
            // Validate remote targets locally and check filesystem paths without blocking on network shares.
            auto folders = SplitString(currentFolder);
            std::erase(folders, std::wstring{});
            remoteOnly = !folders.empty() && std::ranges::all_of(folders, StorageSource::IsPath);
            enableOk = !folders.empty() && std::ranges::all_of(folders, [](const std::wstring& part)
            {
                return StorageSource::IsPath(part) ? StorageSource::Parse(part).has_value() :
                    part.starts_with(L"\\\\") || FinderBasic::DoesFileExist(part);
            });
        }
        break;
    default:
        assert(false);
    }
    m_okButton.EnableWindow(enableOk);
    GetDlgItem(IDC_FAST_SCAN_CHECKBOX).ShowWindow(remoteOnly ? SW_HIDE : SW_SHOW);
    GetDlgItem(IDC_SCAN_DUPLICATES).ShowWindow(remoteOnly ? SW_HIDE : SW_SHOW);
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
    if (IsChecked(IDC_FAST_SCAN_CHECKBOX) && IsElevationAvailable())
    {
        if (ShowMessageBox(*this, Localization::Lookup(IDS_ELEVATION_QUESTION),
            wds::strWinDirStat, MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            const auto previousSettings = std::tuple{ COptions::UseFastScanEngine.Obj(), COptions::SelectDrivesRadio.Obj(),
                COptions::SelectDrivesDrives.Obj(), COptions::SelectDrivesFolder.Obj(), COptions::ScanForDuplicates.Obj() };
            COptions::UseFastScanEngine = true;
            (void)SaveSelection();
            RunElevated({});
            std::tie(COptions::UseFastScanEngine.Obj(), COptions::SelectDrivesRadio.Obj(), COptions::SelectDrivesDrives.Obj(),
                COptions::SelectDrivesFolder.Obj(), COptions::ScanForDuplicates.Obj()) = previousSettings;
            PersistedSetting::WritePersistedProperties();
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

void CSelectDrivesDlg::OnTimer(const UINT_PTR nIDEvent)
{
    if (nIDEvent != QUERY_TIMER_ID)
    {
        CLayoutDialog::OnTimer(nIDEvent);
        return;
    }

    bool updated = false;
    bool querying = false;
    for (const int i : std::views::iota(0, m_driveList.GetItemCount()))
    {
        CDriveItem* item = m_driveList.GetItem(i);
        updated |= item->UpdateDriveInformation();
        querying |= item->IsQuerying();
    }
    if (!querying) ::KillTimer(m_hWnd, QUERY_TIMER_ID);
    if (!updated) return;

    const ScopedValue suppressItemChanged(m_suppressItemChanged, true);
    m_driveList.SortItems();
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
    else if (RemoveSelectedHistoryEntry(pMsg, m_browseList, COptions::SelectDrivesFolder.Obj()))
    {
        m_folderName = m_browseList.GetText();
        UpdateButtons();
        return true;
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

void CSelectDrivesDlg::BrowseFolders(const bool append)
{
    // Show dialog and validate results
    const auto folders = PickFolders(this);
    if (folders.empty()) return;

    const std::wstring selected = JoinString(folders);
    const std::wstring current = append ? GetText(IDC_BROWSE_FOLDER) : std::wstring{};
    SetText(IDC_BROWSE_FOLDER, JoinString(NormalizeScanPaths(
        current.empty() ? selected : current + wds::chrPipe + selected)));

    SetActiveRadio(IDC_RADIO_TARGET_FOLDER);
    UpdateButtons();
}

void CSelectDrivesDlg::OnBnClickedFilterButton()
{
    const bool restart = CSettingsSheet::ShowSettings(1, false); // 1 = Filtering tab
    if (restart)
    {
        CDirStatApp::Get()->RestartApplication();
        return;
    }
    UpdateFilterButton();
}

void CSelectDrivesDlg::OnCloudStorage()
{
    // Choose a provider before editing its connection settings.
    CMenu menu = CMenu::CreatePopup();
    menu.Append(MF_STRING, 1, L"&S3...");
    menu.Append(MF_STRING, 2, L"&Azure Blob...");
    menu.Append(MF_STRING, 3, L"&WebDAV...");
    const auto bounds = GetDlgItem(IDC_CLOUD_STORAGE).GetWindowRect();
    const UINT choice = menu.ShowPopup(TPM_RETURNCMD | TPM_LEFTALIGN, bounds.left, bounds.bottom, this);
    if (choice == 0) return;

    // Apply an accepted connection to the scan target.
    RemoteTarget target;
    if (choice == 1)
    {
        S3ConnectionDialog dialog(this, GetText(IDC_BROWSE_FOLDER));
        if (dialog.ShowModal() != IDOK) return;
        target = std::move(dialog.target);
    }
    else
    {
        RemoteConnectionDialog dialog(this, GetText(IDC_BROWSE_FOLDER),
            choice == 2 ? RemoteProtocol::Azure : RemoteProtocol::WebDav);
        if (dialog.ShowModal() != IDOK) return;
        target = std::move(dialog.target);
    }
    SetText(IDC_BROWSE_FOLDER, target.ToString());
    SetActiveRadio(IDC_RADIO_TARGET_FOLDER);
    UpdateButtons();
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

    const std::wstring selectedFolder = m_browseList.GetItemText(selection);
    UpdateButtons(&selectedFolder);
}

void CSelectDrivesDlg::SetActiveRadio(const int radio)
{
    SetCheckedRadioButton(IDC_RADIO_TARGET_DRIVES_ALL, IDC_RADIO_TARGET_FOLDER, radio);

    // Keep the active mode's input first in the keyboard order.
    const int firstInput = radio == IDC_RADIO_TARGET_FOLDER ? IDC_BROWSE_FOLDER : IDC_TARGET_DRIVES_LIST;
    const int secondInput = radio == IDC_RADIO_TARGET_FOLDER ? IDC_TARGET_DRIVES_LIST : IDC_BROWSE_FOLDER;
    GetDlgItem(secondInput).SetWindowPos(GetDlgItem(firstInput), 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}
