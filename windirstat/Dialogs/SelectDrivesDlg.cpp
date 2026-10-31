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

static void RememberCloudTarget(const std::wstring& path)
{
    auto& history = COptions::SelectDrivesCloud.Obj();
    std::erase(history, path);
    history.insert(history.begin(), path);

    // Each cloud provider retains ten targets independently of the local folder limit.
    std::array<unsigned, 3> counts{};
    std::erase_if(COptions::SelectDrivesCloud.Obj(), [&counts](const std::wstring& path) {
        const auto target = StorageSource::Parse(path);
        return !target || ++counts[static_cast<size_t>(target->protocol)] > 10;
    });
}

class CStorageSelectionPage;

class CCloudConnectionDlg final : public MessageTarget<CCloudConnectionDlg, CLayoutDialog>
{
public:
    CCloudConnectionDlg(std::wstring path, CWnd* parent);
    ~CCloudConnectionDlg() override;

    const std::wstring& GetSelectedPath() const { return m_path; }
    bool OnInitDialog() override;
    void OnOK() override;
    void UpdateButtons();
    void FitEditor();
    static std::span<const RouteEntry> Routes();

private:
    void OnSourceChanged();
    void OnSize(UINT type, int width, int height);
    void OnGetMinMaxInfo(MINMAXINFO* info);
    CStorageSelectionPage* ActivePage() const;
    std::vector<std::unique_ptr<CStorageSelectionPage>> m_storagePages;
    std::wstring m_path;
    CRect m_pageRect;
    CSize m_minimumSize, m_frameSize;
    int m_footerSpace = 0;
};

class CStorageSelectionPage final : public MessageTarget<CStorageSelectionPage, CPropertyPage>
{
public:
    CStorageSelectionPage(CCloudConnectionDlg* owner, const RemoteProtocol protocol) :
        MessageTarget(protocol == RemoteProtocol::S3 ? IDD_S3_CONNECTION :
            protocol == RemoteProtocol::Azure ? IDD_AZURE_CONNECTION : IDD_WEBDAV_CONNECTION),
        m_owner(*owner), m_target{ .protocol = protocol }
    {
    }

    bool OnInitDialog() override
    {
        CPropertyPage::OnInitDialog();
        Localization::UpdateDialogs(*this);
        DarkMode::AdjustControls(Handle());
        m_headingFont.Attach(CFont(GetDlgItem(IDC_REMOTE_CREDENTIALS).GetFont(), FW_BOLD).Detach());
        for (const int id : { IDC_REMOTE_TARGET, IDC_REMOTE_CREDENTIALS }) GetDlgItem(id).SetFont(m_headingFont);
        m_size = GetClientRect().Size();
        for (HWND child = ::GetWindow(Handle(), GW_CHILD); child; child = ::GetWindow(child, GW_HWNDNEXT))
            m_controls.emplace_back(child, GetChildWindowRect(child));
        m_credentialsTop = GetChildWindowRect(GetDlgItem(IDC_REMOTE_CREDENTIALS).Handle()).top;
        const CRect summary = GetChildWindowRect(GetDlgItem(IDC_REMOTE_STATUS).Handle());
        m_collapsedHeight = summary.bottom;
        if (m_target.protocol == RemoteProtocol::S3)
        {
            m_tokenOffset = GetChildWindowRect(GetDlgItem(IDC_REMOTE_REMEMBER).Handle()).top -
                GetChildWindowRect(GetDlgItem(IDC_S3_TOKEN).Handle()).top;
            auto label = GetText(IDC_S3_TOKEN_TOGGLE);
            if (label.ends_with(L':')) label.pop_back();
            SetText(IDC_S3_TOKEN_TOGGLE, label);
        }
        LoadTarget({ .protocol = m_target.protocol });
        return true;
    }

    void LoadTarget(const RemoteTarget& target)
    {
        // Replace all fields so a new target cannot inherit another connection's credentials.
        m_loading = true;
        m_target = target;
        const auto credentials = StorageSource::GetCredentials(target);
        const bool s3 = target.protocol == RemoteProtocol::S3;
        const bool webdav = target.protocol == RemoteProtocol::WebDav;
        SetText(IDC_REMOTE_ENDPOINT, webdav && !target.endpoint.empty() ? target.ToString() : target.endpoint);
        SetText(IDC_REMOTE_SECRET, credentials.secret);
        SetChecked(IDC_REMOTE_REMEMBER, false);
        if (s3)
        {
            SetText(IDC_S3_BUCKET, target.bucket);
            SetText(IDC_S3_REGION, target.region);
            SetText(IDC_S3_ACCESS, credentials.user);
            SetText(IDC_S3_TOKEN, credentials.token);
            SetChecked(IDC_S3_TOKEN_TOGGLE, !credentials.token.empty());
        }
        else SetText(IDC_REMOTE_ACCOUNT, webdav ? credentials.user : target.account);
        if (!webdav) SetText(IDC_REMOTE_PREFIX, target.prefix);
        if (target.protocol == RemoteProtocol::Azure) SetText(IDC_REMOTE_CONTAINER, target.bucket);
        m_expanded = credentials.user.empty() || credentials.secret.empty();
        m_loading = false;
        ArrangeControls();
    }

    std::optional<RemoteTarget> ReadTarget() const
    {
        // Validate explicit settings and credentials without contacting the server.
        const bool s3 = m_target.protocol == RemoteProtocol::S3;
        if (GetText(s3 ? IDC_S3_ACCESS : IDC_REMOTE_ACCOUNT).empty() || GetText(IDC_REMOTE_SECRET).empty()) return {};
        return ReadConnection();
    }

    bool SaveSelection(std::wstring& path)
    {
        auto target = ReadTarget();
        if (!target) return false;
        target->EndCollection();
        const bool s3 = target->protocol == RemoteProtocol::S3;
        const RemoteCredentials credentials{ GetText(s3 ? IDC_S3_ACCESS : IDC_REMOTE_ACCOUNT),
            GetText(IDC_REMOTE_SECRET), s3 && IsChecked(IDC_S3_TOKEN_TOGGLE) ? GetText(IDC_S3_TOKEN) : std::wstring{} };
        const DWORD error = StorageSource::SetCredentials(*target, credentials, IsChecked(IDC_REMOTE_REMEMBER));
        if (error != ERROR_SUCCESS) { DisplayError(TranslateError(error)); return false; }
        path = target->ToString();
        return true;
    }

    int ContentHeight() const
    {
        return !m_expanded ? m_collapsedHeight : m_size.cy -
            (m_target.protocol == RemoteProtocol::S3 && !IsChecked(IDC_S3_TOKEN_TOGGLE) ? m_tokenOffset : 0);
    }

    void FocusInput()
    {
        const int user = m_target.protocol == RemoteProtocol::S3 ? IDC_S3_ACCESS : IDC_REMOTE_ACCOUNT;
        const int field = ReadConnection() && m_expanded ?
            (GetText(user).empty() ? user : IDC_REMOTE_SECRET) : IDC_REMOTE_ENDPOINT;
        GetDlgItem(field).SetFocus();
    }

    void OnOK() override { m_owner.OnOK(); }
    void OnCancel() override { m_owner.OnCancel(); }

    bool OnCommand(const WPARAM wParam, const LPARAM lParam) override
    {
        if (m_loading) return CPropertyPage::OnCommand(wParam, lParam);
        if (lParam != 0 && HIWORD(wParam) == EN_CHANGE)
        {
            // Hidden credentials belong only to the displayed connection, including the WebDAV folder.
            const auto target = ReadConnection();
            if (!m_expanded && (!target || !target->SameConnection(m_target) ||
                target->protocol == RemoteProtocol::WebDav && target->prefix != m_target.prefix))
            {
                m_expanded = true;
                SetText(IDC_REMOTE_SECRET, L"");
                if (m_target.protocol == RemoteProtocol::S3)
                {
                    SetText(IDC_S3_ACCESS, L"");
                    SetText(IDC_S3_TOKEN, L"");
                    SetChecked(IDC_S3_TOKEN_TOGGLE, false);
                }
                else if (m_target.protocol == RemoteProtocol::WebDav) SetText(IDC_REMOTE_ACCOUNT, L"");
                m_owner.FitEditor();
            }
            m_owner.UpdateButtons();
            return true;
        }
        if (lParam != 0 && HIWORD(wParam) == BN_CLICKED &&
            (LOWORD(wParam) == IDC_REMOTE_CHANGE || LOWORD(wParam) == IDC_S3_TOKEN_TOGGLE))
        {
            m_expanded = true;
            m_owner.FitEditor();
            if (LOWORD(wParam) == IDC_REMOTE_CHANGE) FocusInput();
            return true;
        }
        return CPropertyPage::OnCommand(wParam, lParam);
    }

    static std::span<const RouteEntry> Routes()
    {
        static constexpr std::array entries{ Route::Window<&OnSize>(WM_SIZE) };
        return entries;
    }

private:
    std::optional<RemoteTarget> ReadConnection() const
    {
        RemoteTarget target = m_target;
        target.endpoint = GetText(IDC_REMOTE_ENDPOINT);
        TrimString(target.endpoint);
        if (target.protocol == RemoteProtocol::WebDav)
        {
            const auto parsed = StorageSource::Parse(target.endpoint);
            return parsed && parsed->protocol == target.protocol ? parsed : std::nullopt;
        }
        const bool s3 = target.protocol == RemoteProtocol::S3;
        target.bucket = GetText(s3 ? IDC_S3_BUCKET : IDC_REMOTE_CONTAINER);
        target.prefix = GetText(IDC_REMOTE_PREFIX);
        if (s3) target.region = GetText(IDC_S3_REGION);
        else target.account = GetText(IDC_REMOTE_ACCOUNT);
        for (auto* value : { &target.bucket, &target.region, &target.account }) TrimString(*value);
        return StorageSource::Parse(target);
    }

    void ArrangeControls()
    {
        // Keep labels next to their fields and stretch paired columns evenly.
        const int delta = GetClientRect().Width() - m_size.cx;
        const bool token = m_target.protocol != RemoteProtocol::S3 || IsChecked(IDC_S3_TOKEN_TOGGLE);
        for (const auto& [window, original] : m_controls)
        {
            const int id = ::GetDlgCtrlID(window);
            CRect rect = original;
            if (id == IDC_REMOTE_CHANGE) rect.Offset(delta, 0);
            else
            {
                const bool right = original.left > m_size.cx / 2;
                const bool half = right || original.right < m_size.cx;
                rect.Offset(right ? delta / 2 : 0, 0);
                rect.right += half ? delta / 2 : delta;
            }
            rect.right = std::min(rect.right, GetClientRect().right);
            if (!token && id == IDC_REMOTE_REMEMBER) rect.Offset(0, -m_tokenOffset);
            ::MoveWindow(window, rect.left, rect.top, rect.Width(), rect.Height(), true);
            const bool visible = id == IDC_REMOTE_CHANGE || id == IDC_REMOTE_STATUS ? !m_expanded :
                (original.top <= m_credentialsTop || m_expanded) && (id != IDC_S3_TOKEN || token);
            ::ShowWindow(window, visible ? SW_SHOW : SW_HIDE);
        }
    }

    void OnSize(UINT, int, int) { if (m_size.cx != 0) ArrangeControls(); }

    CCloudConnectionDlg& m_owner;
    RemoteTarget m_target;
    CFont m_headingFont;
    std::vector<std::pair<HWND, CRect>> m_controls;
    CSize m_size;
    int m_credentialsTop = 0;
    int m_collapsedHeight = 0;
    int m_tokenOffset = 0;
    bool m_expanded = true;
    bool m_loading = false;
};

CCloudConnectionDlg::CCloudConnectionDlg(std::wstring path, CWnd* parent) :
    MessageTarget(IDD_CLOUD_CONNECTION, nullptr, parent), m_path(std::move(path))
{
}

CCloudConnectionDlg::~CCloudConnectionDlg() = default;

bool CCloudConnectionDlg::OnInitDialog()
{
    CLayoutDialog::OnInitDialog();
    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(Handle());
    ModifyStyle(0, WS_CLIPCHILDREN);
    m_pageRect = GetChildWindowRect(GetDlgItem(IDC_STORAGE_PAGE).Handle());
    GetDlgItem(IDC_STORAGE_PAGE).DestroyWindow();

    // Independent pages retain unfinished input while choosing a provider.
    for (const auto protocol : { RemoteProtocol::S3, RemoteProtocol::Azure, RemoteProtocol::WebDav })
    {
        auto page = std::make_unique<CStorageSelectionPage>(this, protocol);
        if (!page->Create(this)) { CloseModal(-1); return false; }
        GetDlgItem(IDC_REMOTE_PROVIDER).SendMessage(CB_ADDSTRING, 0, page->GetText().c_str());
        page->ShowWindow(SW_HIDE);
        page->EnableWindow(false);
        page->SetWindowPos(GetDlgItem(IDC_REMOTE_PROVIDER), 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        m_storagePages.push_back(std::move(page));
    }
    int provider = COptions::SelectCloudProvider;
    if (const auto target = StorageSource::Parse(m_path))
    {
        provider = static_cast<int>(target->protocol);
        m_storagePages[provider]->LoadTarget(*target);
    }
    SetComboSelection(IDC_REMOTE_PROVIDER, provider);
    m_minimumSize = GetWindowRect().Size();
    m_frameSize = m_minimumSize - GetClientRect().Size();

    // Keep the native footer margin above and below the confirmation buttons.
    const CRect confirm = GetChildWindowRect(GetDlgItem(IDOK).Handle());
    m_footerSpace = confirm.Height() + 2 * (GetClientRect().Height() - confirm.bottom);
    for (const int id : { IDOK, IDCANCEL }) m_layout.AddControl(id, 1, 1, 0, 0);
    m_layout.OnInitDialog(false);
    OnSourceChanged();
    CenterWindow();
    if (m_path.empty()) GetDlgItem(IDC_REMOTE_PROVIDER).SetFocus();
    else ActivePage()->FocusInput();
    return false;
}

CStorageSelectionPage* CCloudConnectionDlg::ActivePage() const
{
    const int index = GetComboSelection(IDC_REMOTE_PROVIDER);
    return index >= 0 && static_cast<size_t>(index) < m_storagePages.size() ? m_storagePages[index].get() : nullptr;
}

void CCloudConnectionDlg::OnOK()
{
    if (ActivePage() && ActivePage()->SaveSelection(m_path)) CLayoutDialog::OnOK();
}

void CCloudConnectionDlg::OnSourceChanged()
{
    for (const auto& page : m_storagePages)
    {
        const bool active = page.get() == ActivePage();
        page->EnableWindow(active);
        page->ShowWindow(active ? SW_SHOW : SW_HIDE);
    }
    FitEditor();
    UpdateButtons();
}

void CCloudConnectionDlg::FitEditor()
{
    if (!ActivePage()) return;
    const int height = m_pageRect.top + ActivePage()->ContentHeight() + m_footerSpace + m_frameSize.cy;
    SetWindowPos(nullptr, 0, 0, GetWindowRect().Width(), height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    OnSize(0, 0, 0);
}

void CCloudConnectionDlg::UpdateButtons()
{
    GetDlgItem(IDOK).EnableWindow(ActivePage() && ActivePage()->ReadTarget().has_value());
}

void CCloudConnectionDlg::OnSize(const UINT type, const int width, const int height)
{
    if (m_minimumSize.cx == 0) return;
    CLayoutDialog::OnSize(type, width, height);
    if (!ActivePage()) return;
    CRect rect = m_pageRect;
    rect.right = GetClientRect().right - rect.left;
    rect.bottom = rect.top + ActivePage()->ContentHeight();
    ActivePage()->MoveWindow(rect);
}

void CCloudConnectionDlg::OnGetMinMaxInfo(MINMAXINFO* info)
{
    CLayoutDialog::OnGetMinMaxInfo(info);
    if (m_minimumSize.cx == 0 || !ActivePage()) return;
    info->ptMinTrackSize.x = m_minimumSize.cx;
    info->ptMinTrackSize.y = m_pageRect.top + ActivePage()->ContentHeight() + m_footerSpace + m_frameSize.cy;
}

std::span<const RouteEntry> CCloudConnectionDlg::Routes()
{
    static constexpr std::array entries{
        Route::Control<&OnSourceChanged>(CBN_SELCHANGE, IDC_REMOTE_PROVIDER),
        Route::Window<&OnSize>(WM_SIZE),
        Route::Window<&OnGetMinMaxInfo>(WM_GETMINMAXINFO)
    };
    return entries;
}

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

    // Restore local scan preferences.
    SetChecked(IDC_SCAN_DUPLICATES, COptions::ScanForDuplicates);
    SetChecked(IDC_FAST_SCAN_CHECKBOX, COptions::UseFastScanEngine);
    SetActiveRadio(IDC_RADIO_TARGET_DRIVES_ALL + COptions::SelectDrivesRadio);
    m_selectedDrives = COptions::SelectDrivesDrives;

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

    // Restore local folder history independently of cloud selections.
    for (const auto& folder : COptions::SelectDrivesFolder.Obj())
    {
        if (StorageSource::IsPath(folder)) continue;
        m_browseList.AddString(folder);
        if (m_folderName.empty()) m_folderName = folder;
    }
    m_browseList.SetText(m_folderName);

    // Keep restored window sizes large enough for the current text scale.
    const CRect minimumRect = GetWindowRect();
    m_layout.OnInitDialog(true);
    const CRect restoredRect = GetWindowRect();
    if (restoredRect.Width() < minimumRect.Width() || restoredRect.Height() < minimumRect.Height())
    {
        SetWindowPos(nullptr, 0, 0, std::max(restoredRect.Width(), minimumRect.Width()),
            std::max(restoredRect.Height(), minimumRect.Height()), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        CenterWindow();
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
    m_radio = GetCheckedRadioButton(IDC_RADIO_TARGET_DRIVES_ALL,
        IDC_RADIO_TARGET_FOLDER) - IDC_RADIO_TARGET_DRIVES_ALL;
    m_folderName = GetText(IDC_BROWSE_FOLDER);

    m_drives.clear();
    m_selectedDrives.clear();
    if (m_radio == RADIO_TARGET_FOLDER)
    {
        // Normalize every pipe-separated path on its own and rebuild the spec from the results
        const auto folders = NormalizeScanPaths(m_folderName);
        if (folders.empty()) return false;
        m_folderName = JoinString(folders);

        // Store each target independently, preserving exact cloud keys and local path comparisons.
        for (const auto& folder : folders | std::views::reverse)
        {
            if (StorageSource::IsPath(folder)) { RememberCloudTarget(folder); continue; }
            auto& history = COptions::SelectDrivesFolder.Obj();
            std::erase_if(history, [&folder](const std::wstring& previous) {
                return _wcsicmp(previous.c_str(), folder.c_str()) == 0;
            });
            history.insert(history.begin(), folder);
        }

        std::erase_if(COptions::SelectDrivesFolder.Obj(), StorageSource::IsPath);
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
            const auto previousSettings = std::tuple{ COptions::UseFastScanEngine.Obj(),
                COptions::SelectDrivesRadio.Obj(), COptions::SelectDrivesDrives.Obj(), COptions::SelectDrivesFolder.Obj(),
                COptions::ScanForDuplicates.Obj(), COptions::SelectDrivesCloud.Obj() };
            COptions::UseFastScanEngine = true;
            (void)SaveSelection();
            RunElevated({});
            std::tie(COptions::UseFastScanEngine.Obj(), COptions::SelectDrivesRadio.Obj(),
                COptions::SelectDrivesDrives.Obj(), COptions::SelectDrivesFolder.Obj(), COptions::ScanForDuplicates.Obj(),
                COptions::SelectDrivesCloud.Obj()) = previousSettings;
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
        CallDefaultHandler();
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
    CallDefaultHandler();
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

CSelectCloudDlg::CSelectCloudDlg(CWnd* parent) : MessageTarget(IDD_SELECTCLOUD, nullptr, parent)
{
}

MessageResult CSelectCloudDlg::RecentList::OnNotify(const WPARAM wParam, const LPARAM lParam)
{
    // Native headers need the dialog's text color in dark mode.
    const auto header = reinterpret_cast<NMHDR*>(lParam);
    if (!header || header->code != NM_CUSTOMDRAW || header->hwndFrom != GetHeader().Handle() ||
        !DarkMode::IsDarkModeActive()) return CListCtrl::OnNotify(wParam, lParam);
    const auto draw = reinterpret_cast<NMCUSTOMDRAW*>(header);
    if (draw->dwDrawStage == CDDS_ITEMPREPAINT) ::SetTextColor(draw->hdc, DarkMode::SystemColor(COLOR_BTNTEXT));
    return draw->dwDrawStage == CDDS_PREPAINT ? CDRF_NOTIFYITEMDRAW : CDRF_DODEFAULT;
}

bool CSelectCloudDlg::OnInitDialog()
{
    CLayoutDialog::OnInitDialog();
    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(Handle());
    ModifyStyle(0, WS_CLIPCHILDREN);
    m_recent.SubclassDlgItem(IDC_REMOTE_RECENT, this);
    m_recent.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    m_recent.SetBkColor(DarkMode::SystemColor(COLOR_WINDOW));
    m_recent.SetTextBkColor(DarkMode::SystemColor(COLOR_WINDOW));
    m_recent.SetTextColor(DarkMode::SystemColor(COLOR_WINDOWTEXT));
    m_headingFont.Attach(CFont(GetDlgItem(IDC_REMOTE_RECENT_LABEL).GetFont(), FW_BOLD).Detach());
    GetDlgItem(IDC_REMOTE_RECENT_LABEL).SetFont(m_headingFont);
    const std::array columns{ IDS_REMOTE_TARGET, IDS_REMOTE_PROVIDER, IDS_S3_ENDPOINT };
    for (const auto [index, key] : std::views::enumerate(columns))
    {
        auto label = Localization::Lookup(key);
        std::erase(label, L'&');
        if (label.ends_with(L':')) label.pop_back();
        m_recent.InsertColumn(static_cast<int>(index), label);
    }

    // Size the picker actions to their translated captions.
    CClientDC dc(this);
    const GdiObjectSelection font(&dc, GetDlgItem(IDC_REMOTE_NEW).GetFont());
    const CRect confirm = GetChildWindowRect(GetDlgItem(IDOK).Handle());
    const int buttonGap = GetChildWindowRect(GetDlgItem(IDCANCEL).Handle()).left - confirm.right;
    const int margin = GetChildWindowRect(GetDlgItem(IDC_REMOTE_RECENT).Handle()).left;
    int left = margin;
    for (const int id : { IDC_REMOTE_NEW, IDC_REMOTE_EDIT, IDC_REMOTE_REMOVE })
    {
        auto label = GetText(id);
        std::erase(label, L'&');
        CRect rect = GetChildWindowRect(GetDlgItem(id).Handle());
        rect.left = left;
        rect.right = left + std::max<int>(confirm.Width(), dc.GetTextExtent(label).cx + 2 * margin);
        GetDlgItem(id).MoveWindow(rect);
        left = rect.right + buttonGap;
    }
    const CSize minimumSize = GetWindowRect().Size();
    m_layout.AddControl(IDC_REMOTE_RECENT, 0, 0, 1, 1);
    m_layout.AddControl(IDC_REMOTE_STATUS, 0, 1, 1, 0);
    for (const int id : { IDC_REMOTE_NEW, IDC_REMOTE_EDIT, IDC_REMOTE_REMOVE })
        m_layout.AddControl(id, 0, 1, 0, 0);
    for (const int id : { IDOK, IDCANCEL }) m_layout.AddControl(id, 1, 1, 0, 0);
    m_layout.OnInitDialog(true);
    m_initialized = true;
    const CRect saved = COptions::CloudSelectWindowRect.Obj();
    if (saved.Width() > 0 && saved.Height() > 0)
        SetWindowPos(nullptr, 0, 0, std::max<int>(saved.Width(), minimumSize.cx),
            std::max<int>(saved.Height(), minimumSize.cy), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    OnSize(0, 0, 0);
    ReloadRecentTargets();
    CenterWindow();
    return false;
}

void CSelectCloudDlg::ReloadRecentTargets()
{
    // One chronological list covers all providers, retaining the exact target identities.
    m_recent.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    m_recentTargets.clear();
    for (const auto& path : COptions::SelectDrivesCloud.Obj())
        if (StorageSource::Parse(path)) m_recentTargets.push_back(path);
    m_recent.SetItemCountEx(static_cast<int>(m_recentTargets.size()));
    if (!m_recentTargets.empty())
    {
        m_recent.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        m_recent.EnsureVisible(0, false);
    }
    m_recent.Invalidate();
    UpdateButtons();
    GetDlgItem(m_recentTargets.empty() ? IDC_REMOTE_NEW : IDC_REMOTE_RECENT).SetFocus();
}

void CSelectCloudDlg::OnOK()
{
    const int index = m_recent.GetFirstSelectedIndex();
    if (index < 0 || static_cast<size_t>(index) >= m_recentTargets.size()) return;
    const auto target = *StorageSource::Parse(m_recentTargets[index]);
    const auto credentials = StorageSource::GetCredentials(target);
    if (credentials.user.empty() || credentials.secret.empty())
    {
        if (!ShowEditor(index)) return;
        m_path = m_recentTargets.front();
    }
    else m_path = target.ToString();

    // Scanning a connection promotes it without changing local scan preferences.
    RememberCloudTarget(m_path);
    COptions::SelectCloudProvider = static_cast<int>(StorageSource::Parse(m_path)->protocol);
    CMainFrame::Get()->GetFileTabbedView()->SetActiveFileTreeView();
    CLayoutDialog::OnOK();
}

bool CSelectCloudDlg::ShowEditor(const int index)
{
    if (index >= 0 && static_cast<size_t>(index) >= m_recentTargets.size()) return false;
    CCloudConnectionDlg dialog(index < 0 ? std::wstring{} : m_recentTargets[index], this);
    if (dialog.ShowModal() != IDOK) return false;

    // Confirmed connections join the list; canceling an editor leaves history untouched.
    RememberCloudTarget(dialog.GetSelectedPath());
    COptions::SelectCloudProvider = static_cast<int>(StorageSource::Parse(dialog.GetSelectedPath())->protocol);
    ReloadRecentTargets();
    return true;
}

void CSelectCloudDlg::OnNew() { ShowEditor(-1); }
void CSelectCloudDlg::OnEdit()
{
    const int index = m_recent.GetFirstSelectedIndex();
    if (index >= 0) ShowEditor(index);
}

void CSelectCloudDlg::OnRemove()
{
    const int index = m_recent.GetFirstSelectedIndex();
    if (index < 0 || static_cast<size_t>(index) >= m_recentTargets.size()) return;
    std::erase(COptions::SelectDrivesCloud.Obj(), m_recentTargets[index]);
    m_recent.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);

    // Revoke the saved secret once no remaining connection shares it.
    const auto removed = *StorageSource::Parse(m_recentTargets[index]);
    m_recentTargets.erase(m_recentTargets.begin() + index);
    StorageSource::ForgetCredentials(removed, m_recentTargets);
    m_recent.SetItemCountEx(static_cast<int>(m_recentTargets.size()));
    if (!m_recentTargets.empty())
    {
        const int next = std::min(index, static_cast<int>(m_recentTargets.size()) - 1);
        m_recent.SetItemState(next, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        m_recent.EnsureVisible(next, false);
    }
    m_recent.Invalidate();
    UpdateButtons();
    GetDlgItem(m_recentTargets.empty() ? IDC_REMOTE_NEW : IDC_REMOTE_RECENT).SetFocus();
}

void CSelectCloudDlg::UpdateButtons()
{
    const int index = m_recent.GetFirstSelectedIndex();
    const bool selected = index >= 0 && static_cast<size_t>(index) < m_recentTargets.size();
    for (const int id : { IDOK, IDC_REMOTE_EDIT, IDC_REMOTE_REMOVE }) GetDlgItem(id).EnableWindow(selected);
    std::wstring status;
    if (selected)
    {
        const auto credentials = StorageSource::GetCredentials(*StorageSource::Parse(m_recentTargets[index]));
        status = Localization::Lookup(credentials.user.empty() || credentials.secret.empty() ?
            IDS_REMOTE_CREDENTIALS_REQUIRED : IDS_REMOTE_CREDENTIALS_AVAILABLE);
    }
    else if (m_recentTargets.empty()) status = Localization::Lookup(IDS_REMOTE_EMPTY);
    SetText(IDC_REMOTE_STATUS, status);
}

void CSelectCloudDlg::OnSelectionChanged(NMHDR*, LRESULT* result) { *result = 0; UpdateButtons(); }
void CSelectCloudDlg::OnDoubleClick(NMHDR* header, LRESULT* result)
{
    *result = 0;
    if (reinterpret_cast<NMITEMACTIVATE*>(header)->iItem >= 0) OnOK();
}
void CSelectCloudDlg::OnListKeyDown(NMHDR* header, LRESULT* result)
{
    *result = 0;
    if (reinterpret_cast<NMLVKEYDOWN*>(header)->wVKey == VK_DELETE) OnRemove();
}

void CSelectCloudDlg::OnDisplayInfo(NMHDR* header, LRESULT* result)
{
    *result = 0;
    auto& item = reinterpret_cast<NMLVDISPINFO*>(header)->item;
    if (!(item.mask & LVIF_TEXT) || item.iItem < 0 || static_cast<size_t>(item.iItem) >= m_recentTargets.size()) return;
    const auto target = *StorageSource::Parse(m_recentTargets[item.iItem]);
    if (item.iSubItem == 0)
        m_cell = target.protocol == RemoteProtocol::WebDav ? L"/" + target.prefix :
            (target.account.empty() ? L"" : target.account + L"/") + target.bucket + L"/" + target.prefix;
    else if (item.iSubItem == 1)
        m_cell = target.protocol == RemoteProtocol::S3 ? L"S3" :
            target.protocol == RemoteProtocol::Azure ? L"Azure Blob" : L"WebDAV";
    else m_cell = target.endpoint + (target.region.empty() ? L"" : L" (" + target.region + L")");
    item.pszText = m_cell.data();
}

void CSelectCloudDlg::OnSize(const UINT type, const int width, const int height)
{
    if (!m_initialized) return;
    CLayoutDialog::OnSize(type, width, height);
    const int available = m_recent.GetClientRect().Width() - ScaleForDpi(4);
    m_recent.SetColumnWidth(0, available * 42 / 100);
    m_recent.SetColumnWidth(1, available * 20 / 100);
    m_recent.SetColumnWidth(2, available - available * 42 / 100 - available * 20 / 100);
}

void CSelectCloudDlg::SavePersistentAttributes() const
{
    COptions::CloudSelectWindowRect.Obj() = GetWindowRect();
}

void CSelectCloudDlg::OnDestroy()
{
    SavePersistentAttributes();
    CLayoutDialog::OnDestroy();
}
