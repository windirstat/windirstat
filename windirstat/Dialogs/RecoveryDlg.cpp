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
#include "RecoveryDlg.h"
#include "IconHandler.h"

bool RecoveryDlg::ResultsList::OnNotify(const WPARAM wParam, const LPARAM lParam, LRESULT* result)
{
    // Keep native header text readable with the dark palette.
    const auto header = reinterpret_cast<NMHDR*>(lParam);
    if (!header || header->code != NM_CUSTOMDRAW || header->hwndFrom != GetHeader().Handle() ||
        !DarkMode::IsDarkModeActive()) return CListCtrl::OnNotify(wParam, lParam, result);
    const auto draw = reinterpret_cast<NMCUSTOMDRAW*>(header);
    *result = draw->dwDrawStage == CDDS_PREPAINT ? CDRF_NOTIFYITEMDRAW : CDRF_DODEFAULT;
    if (draw->dwDrawStage == CDDS_ITEMPREPAINT) ::SetTextColor(draw->hdc, DarkMode::SystemColor(COLOR_BTNTEXT));
    return true;
}

void RecoveryDlg::ProgressAnimation::DrawItem(LPDRAWITEMSTRUCT item)
{
    auto target = CDC::Borrow(item->hDC);
    CBufferedDC dc(target, this);
    const auto background = DarkMode::SystemColor(DarkMode::IsDarkModeActive() ? COLOR_WINDOW : COLOR_BTNFACE);
    pacman.Draw(&dc, GetClientRect(), background, true, paused);
}

RecoveryDlg::~RecoveryDlg()
{
    // Keep worker-owned state alive until canceled I/O has finished.
    m_progress.cancel = true;
    m_progress.paused = false;
    if (m_worker.joinable())
    {
        CancelSynchronousIo(m_worker.native_handle());
        m_worker.join();
    }
}

bool RecoveryDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    Localization::UpdateDialogs(*this);
    DarkMode::AdjustControls(Handle());
    m_list.SubclassDlgItem(IDC_RECOVERY_LIST, this);
    m_animation.SubclassDlgItem(IDC_RECOVERY_PROGRESS, this);
    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    const std::array columns{ IDS_COL_NAME, IDS_RECOVERY_PATH, IDS_COL_SIZE_LOGICAL,
        IDS_COL_LAST_CHANGE, IDS_RECOVERY_CONDITION, IDS_RECOVERY_RESULT };
    const std::array widths{ 160, 220, 85, 120, 180, 210 };
    for (size_t i = 0; i < columns.size(); ++i)
        m_list.InsertColumn(static_cast<int>(i), Localization::Lookup(columns[i]), LVCFMT_LEFT, ScaleForDpi(widths[i]));
    m_list.SetBkColor(DarkMode::SystemColor(COLOR_WINDOW));
    m_list.SetTextBkColor(DarkMode::SystemColor(COLOR_WINDOW));
    m_list.SetTextColor(DarkMode::SystemColor(COLOR_WINDOWTEXT));
    for (const auto& drive : GetDriveList({ DRIVE_FIXED, DRIVE_REMOVABLE }, false, false))
    {
        const auto root = drive + L"\\";
        wchar_t filesystem[MAX_PATH] = {}, label[MAX_PATH] = {};
        if (!GetVolumeInformationW(root.c_str(), label, MAX_PATH, nullptr, nullptr, nullptr, filesystem, MAX_PATH) ||
            (_wcsicmp(filesystem, L"NTFS") != 0 && _wcsicmp(filesystem, L"exFAT") != 0)) continue;
        m_roots.push_back(root);
        const auto text = label[0] == L'\0' ? root : std::format(L"{} ({})", root, label);
        GetDlgItem(IDC_RECOVERY_SOURCE)->SendNativeMessage(CB_ADDSTRING, 0, text.c_str());
    }
    SetComboSelection(IDC_RECOVERY_SOURCE, m_roots.empty() ? -1 : 0);
    const int iconSize = ScaleForDpi(16);
    m_icons.emplace_back(&DestroyIcon, Icons::MakeIcon(iconSize, Icons::PaintFileSelect));
    m_icons.emplace_back(&DestroyIcon, Icons::MakeIcon(iconSize, Icons::Char(L'▶', RGB(50, 205, 50))));
    m_icons.emplace_back(&DestroyIcon, Icons::MakeIcon(iconSize, Icons::PaintPause));
    m_icons.emplace_back(&DestroyIcon, Icons::MakeIcon(iconSize, Icons::Char(L'■', RGB(220, 20, 60))));
    m_tooltips.Create(this);
    const std::array tipIds{ IDC_RECOVERY_BROWSE, IDC_RECOVERY_PLAY, IDC_RECOVERY_PAUSE, IDC_RECOVERY_STOP };
    const std::array tipKeys{ IDS_MENU_SELECT, IDS_RESUME, IDS_SUSPEND, IDS_STOP };
    for (size_t i = 0; i < tipIds.size(); ++i)
    {
        GetDlgItem(tipIds[i])->SendNativeMessage(BM_SETIMAGE, IMAGE_ICON, m_icons[i].Get());
        m_tooltipText[i] = Localization::Lookup(tipKeys[i]);
        std::erase(m_tooltipText[i], L'&');
        m_tooltips.AddTool(GetDlgItem(tipIds[i]), m_tooltipText[i]);
        SetText(tipIds[i], m_tooltipText[i]);
    }
    m_layout.AddControl(IDC_RECOVERY_SOURCE, 0, 0, 1, 0);
    m_layout.AddControl(IDC_RECOVERY_FILTER, 0, 0, 1, 0);
    for (const int id : { IDC_RECOVERY_REGEX, IDC_RECOVERY_PLAY, IDC_RECOVERY_PAUSE, IDC_RECOVERY_STOP })
        m_layout.AddControl(id, 1, 0, 0, 0);
    m_layout.AddControl(IDC_RECOVERY_LIST, 0, 0, 1, 1);
    m_layout.AddControl(IDC_RECOVERY_DEST_LABEL, 0, 1, 0, 0);
    m_layout.AddControl(IDC_RECOVERY_DEST, 0, 1, 1, 0);
    m_layout.AddControl(IDC_RECOVERY_BROWSE, 1, 1, 0, 0);
    m_layout.AddControl(IDC_RECOVERY_PROGRESS, 0, 1, 1, 0);
    for (const int id : { IDC_RECOVERY_RECOVER, IDCANCEL })
        m_layout.AddControl(id, 1, 1, 0, 0);
    m_layout.OnInitDialog(true);
    SetBusy(false);
    OnSourceChanged();
    return true;
}

void RecoveryDlg::UpdateActions()
{
    if (m_rebuilding) return;
    const bool ready = !m_busy && !m_filterPending && m_filterValid && m_volume != nullptr;
    const bool stopping = m_progress.cancel.load();
    const bool paused = m_progress.paused.load();
    GetDlgItem(IDC_RECOVERY_RECOVER)->EnableWindow(ready && m_list.GetSelectedCount() != 0 &&
        !GetText(IDC_RECOVERY_DEST).empty());
    GetDlgItem(IDC_RECOVERY_PLAY)->EnableWindow(!m_closing && !m_root.empty() &&
        (!m_busy || (paused && !stopping)));
    GetDlgItem(IDC_RECOVERY_PAUSE)->EnableWindow(m_busy && !paused && !stopping);
    GetDlgItem(IDC_RECOVERY_STOP)->EnableWindow(m_busy && !stopping);
    GetDlgItem(IDCANCEL)->EnableWindow(!m_closing);
}

void RecoveryDlg::SetBusy(const bool busy)
{
    m_busy = busy;
    m_progress.paused = false;
    if (busy)
    {
        m_done = false;
        m_error.clear();
        m_progress.cancel = false;

        // Discoveries append in scan order, so hide the sort indicator until completion.
        if (!m_recovering)
        {
            auto& header = m_list.GetHeader();
            HDITEM item{ .mask = HDI_FORMAT };
            if (header.GetItem(m_sortColumn, &item))
            {
                item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
                header.SetItem(m_sortColumn, &item);
            }
        }
        m_animation.paused = false;
        m_animation.pacman.Reset();
        m_animation.pacman.UpdatePosition();
        SetTimer(1, 50);
    }
    m_animation.ShowWindow(busy ? SW_SHOW : SW_HIDE);
    if (busy) m_animation.RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
    for (const int id : { IDC_RECOVERY_SOURCE, IDC_RECOVERY_FILTER, IDC_RECOVERY_REGEX,
        IDC_RECOVERY_DEST, IDC_RECOVERY_BROWSE }) GetDlgItem(id)->EnableWindow(!busy);
    UpdateActions();
}

void RecoveryDlg::StartWorker(std::function<void()> work)
{
    SetBusy(true);
    try
    {
        m_worker = std::jthread([this, work = std::move(work)]
        {
            try { work(); }
            catch (const NtfsRecovery::Failure& failure)
            {
                if (failure.error != ERROR_CANCELLED && failure.error != ERROR_OPERATION_ABORTED)
                    m_error = ErrorText(failure);
            }
            catch (const std::bad_alloc&) { m_error = ErrorText({ {}, ERROR_NOT_ENOUGH_MEMORY }); }
            catch (...) { m_error = ErrorText({ {}, ERROR_GEN_FAILURE }); }

            // Failed scans discard their metadata; stopping preserves validated discoveries.
            if (!m_recovering && !m_error.empty())
            {
                m_completedScan = {};
                m_volume.reset();
            }
            m_done = true;
        });
    }
    catch (...)
    {
        m_error = ErrorText({ {}, ERROR_GEN_FAILURE });
        m_done = true;
    }
}

std::wstring RecoveryDlg::ErrorText(const NtfsRecovery::Failure& failure)
{
    if (!failure.message.empty()) return Localization::Lookup(failure.message);
    auto text = TranslateError(failure.error == ERROR_OPERATION_ABORTED ? ERROR_CANCELLED : failure.error);
    const auto end = text.find_last_not_of(L" \t\r\n");
    text.resize(end == std::wstring::npos ? 0 : end + 1);
    return text;
}

void RecoveryDlg::OnSourceChanged()
{
    if (m_busy) return;
    m_rebuilding = true;
    ::KillTimer(Handle(), 2);
    m_filterPending = false;
    m_list.SetItemCountEx(0);
    m_visible.clear();
    m_outcomes.clear();
    m_scan = {};
    m_completedScan = {};
    m_pendingRecords.clear();
    m_volume.reset();
    m_error.clear();
    m_recovering = false;
    const int selection = GetComboSelection(IDC_RECOVERY_SOURCE);
    m_root = selection >= 0 && static_cast<size_t>(selection) < m_roots.size() ? m_roots[selection] : L"";

    const auto title = std::regex_replace(Localization::Lookup(IDS_MENU_RECOVERY),
        std::wregex(LR"(\(&.\)|&|\.{3}$|…$)"), L"");
    SetText(title + (m_root.empty() ? L"" : L" — " + m_root));
    if (m_root.empty()) DisplayError(ErrorText({ {}, ERROR_NOT_READY }));
    m_rebuilding = false;
    UpdateActions();
}

void RecoveryDlg::OnScan()
{
    // Resume an active operation; an idle Play starts a fresh scan of the selected volume.
    if (m_closing || (m_busy && m_progress.cancel.load())) return;
    if (m_busy)
    {
        m_progress.paused = false;
        UpdateActions();
        return;
    }
    OnSourceChanged();
    if (m_root.empty()) return;
    StartWorker([this]
    {
        m_volume = std::make_unique<NtfsRecovery>(m_root, &m_progress);
        m_volume->Scan(m_progress, m_completedScan, [this](const NtfsRecovery::Record& record)
        {
            // Publish display fields only; the worker retains recovery metadata until the scan finishes.
            NtfsRecovery::Record preview;
            preview.name = record.name;
            preview.path = record.path;
            preview.data.size = record.data.size;
            preview.modified = record.modified;
            preview.condition = record.condition;
            const std::lock_guard lock(m_resultMutex);
            m_pendingRecords.push_back(std::move(preview));
        });
    });
}

void RecoveryDlg::OnPause()
{
    if (!m_busy || m_progress.cancel.load()) return;
    m_progress.paused = true;
    UpdateActions();
}

void RecoveryDlg::OnStop()
{
    if (!m_busy) return;
    // Release paused work and interrupt synchronous reads so cancellation can finish.
    m_progress.cancel = true;
    m_progress.paused = false;
    if (m_worker.joinable()) CancelSynchronousIo(m_worker.native_handle());
    UpdateActions();
}

void RecoveryDlg::OnCancel()
{
    if (!m_busy) { CLayoutDialog::OnCancel(); return; }
    // Defer closing until the timer confirms the worker has stopped.
    m_closing = true;
    OnStop();
}

void RecoveryDlg::OnTimer(const UINT_PTR timer)
{
    if (timer == 2)
    {
        // Debounce typing so filtering and sorting run after edits settle.
        ::KillTimer(Handle(), 2);
        m_filterPending = false;
        if (!m_busy) UpdateList();
        return;
    }
    if (timer != 1 || !m_busy || m_timerActive) return;
    const ScopedValue updating(m_timerActive, true);

    // Read completion before draining queues so the final batch cannot be missed.
    const bool done = m_done.load();
    if (m_recovering)
    {

        // Move queued results out of the lock before updating visible rows.
        std::vector<std::pair<size_t, std::wstring>> outcomes;
        {
            const std::lock_guard lock(m_resultMutex);
            outcomes.swap(m_pendingOutcomes);
        }
        for (auto& [index, outcome] : outcomes) m_outcomes[index] = std::move(outcome);
        if (!outcomes.empty() && m_list.GetItemCount() != 0)
        {
            const int first = m_list.GetTopIndex();
            m_list.RedrawItems(first, std::min(first + m_list.GetCountPerPage(), m_list.GetItemCount() - 1));
        }
    }
    else
    {
        std::vector<NtfsRecovery::Record> records;
        {
            const std::lock_guard lock(m_resultMutex);
            records.swap(m_pendingRecords);
        }
        if (!records.empty())
        {
            const auto first = m_scan.records.size();
            m_scan.records.insert(m_scan.records.end(),
                std::make_move_iterator(records.begin()), std::make_move_iterator(records.end()));
            m_outcomes.resize(m_scan.records.size());
            UpdateList(first, true);
        }
    }
    if (!done)
    {
        m_animation.paused = m_progress.paused.load() || m_progress.cancel.load();
        if (!m_animation.paused) m_animation.pacman.UpdatePosition();
        m_animation.Invalidate(false);
        return;
    }
    // Join before reading final worker state or closing the dialog.
    if (m_worker.joinable()) m_worker.join();
    ::KillTimer(Handle(), 1);
    if (!m_recovering)
    {
        if (!m_error.empty())
        {
            m_list.SetItemCountEx(0);
            m_visible.clear();
        }

        // Replace previews with recovery metadata and resolved paths before the final sort.
        m_scan = std::move(m_completedScan);
        m_outcomes.resize(m_scan.records.size());
        UpdateList();
    }
    else if (m_sortColumn == 5) UpdateList();
    SetBusy(false);
    if (!m_error.empty()) DisplayError(m_error);
    if (m_closing) CLayoutDialog::OnCancel();
}

void RecoveryDlg::OnBrowse()
{
    if (const auto path = PickFolder(this)) SetText(IDC_RECOVERY_DEST, *path);
}

std::wstring RecoveryDlg::CellText(const size_t index, const int column) const
{
    const auto& record = m_scan.records[index];
    switch (column)
    {
    case 0: return record.name;
    case 1: return record.path;
    case 2: return FormatSizeSuffixes(record.data.size);
    case 3: return FormatFileTime(record.modified);
    case 4: return Localization::Lookup(record.condition == NtfsRecovery::Condition::Resident ?
        IDS_RECOVERY_RESIDENT : IDS_RECOVERY_UNALLOCATED);
    case 5: return m_outcomes[index];
    default: return {};
    }
}

void RecoveryDlg::UpdateList(const size_t first, const bool append)
{
    // Build the new view separately so invalid expressions preserve the current results.
    std::vector<size_t> visible;
    const auto filter = GetText(IDC_RECOVERY_FILTER);
    try
    {
        std::optional<std::wregex> pattern;
        if (IsChecked(IDC_RECOVERY_REGEX) && !filter.empty())
            pattern.emplace(filter, std::regex::icase | std::regex::optimize);
        for (size_t i = first; i < m_scan.records.size(); ++i)
        {
            const auto& record = m_scan.records[i];
            if (pattern ? !std::regex_search(record.path, *pattern) : (!filter.empty() &&
                StrStrIW(record.path.c_str(), filter.c_str()) == nullptr &&
                !PathMatchSpecW(record.name.c_str(), filter.c_str()))) continue;
            visible.push_back(i);
        }
    }
    catch (const std::regex_error&)
    {

        // Avoid repeating the same invalid-filter popup as more scan batches arrive.
        const bool notify = m_filterValid;
        m_filterValid = false;
        UpdateActions();
        if (notify) DisplayError(Localization::Lookup(IDS_PAGE_FILTERING_INVALID_FILTER) + L" " + filter);
        return;
    }
    m_filterValid = true;
    if (append)
    {
        // Append discoveries without moving existing rows, selection, or the scroll position.
        if (visible.empty()) return;
        const int previous = m_list.GetItemCount();
        m_visible.insert(m_visible.end(), visible.begin(), visible.end());
        m_list.SetItemCountEx(static_cast<int>(m_visible.size()), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
        const int last = std::min(m_list.GetTopIndex() + m_list.GetCountPerPage(), m_list.GetItemCount() - 1);
        if (previous <= last) m_list.RedrawItems(previous, last);
        return;
    }
    // Preserve selected file identities when filtering and sorting change row positions.
    const int top = m_list.GetTopIndex();
    const auto anchor = top > 0 && static_cast<size_t>(top) < m_visible.size() ? m_visible[top] : SIZE_MAX;
    CRect anchorRect;
    if (anchor != SIZE_MAX) m_list.GetItemRect(top, anchorRect);
    std::unordered_set<size_t> selected;
    for (int i = m_list.GetNextItem(-1, LVNI_SELECTED); i >= 0; i = m_list.GetNextItem(i, LVNI_SELECTED))
        selected.insert(m_visible[i]);
    const int focus = m_list.GetNextItem(-1, LVNI_FOCUSED);
    const auto focused = focus < 0 ? SIZE_MAX : m_visible[focus];
    const ScopedRedrawPause redraw(&m_list);
    m_rebuilding = true;
    m_list.SetItemCountEx(0);
    m_visible = std::move(visible);
    std::ranges::sort(m_visible, [this](const size_t left, const size_t right)
    {
        int compare = 0;
        const auto numeric = [&compare](const auto a, const auto b) { compare = a < b ? -1 : a > b ? 1 : 0; };
        const auto& a = m_scan.records[left];
        const auto& b = m_scan.records[right];
        switch (m_sortColumn)
        {
        case 0: compare = _wcsicmp(a.name.c_str(), b.name.c_str()); break;
        case 1: compare = _wcsicmp(a.path.c_str(), b.path.c_str()); break;
        case 2: numeric(a.data.size, b.data.size); break;
        case 3: compare = CompareFileTime(&a.modified, &b.modified); break;
        case 4: numeric(a.condition, b.condition); break;
        case 5: compare = _wcsicmp(m_outcomes[left].c_str(), m_outcomes[right].c_str()); break;
        }

        // Break ties by record identity to keep refresh ordering consistent.
        if (compare == 0) numeric(left, right);
        return m_descending ? compare > 0 : compare < 0;
    });
    m_list.SetItemCountEx(static_cast<int>(m_visible.size()));
    for (size_t i = 0; i < m_visible.size(); ++i)
    {
        const UINT state = (selected.contains(m_visible[i]) ? LVIS_SELECTED : 0) |
            (m_visible[i] == focused ? LVIS_FOCUSED : 0);
        if (state) m_list.SetItemState(static_cast<int>(i), state, LVIS_SELECTED | LVIS_FOCUSED);
    }

    // Keep the previous top record at the same vertical position after sorting.
    if (const auto position = std::ranges::find(m_visible, anchor); position != m_visible.end())
    {
        const int index = static_cast<int>(position - m_visible.begin());
        m_list.EnsureVisible(index, false);
        CRect rect;
        if (m_list.GetItemRect(index, rect)) m_list.Scroll(CSize(0, rect.top - anchorRect.top));
    }
    auto& header = m_list.GetHeader();
    for (int i = 0; i < header.GetItemCount(); ++i)
    {
        HDITEM item{ .mask = HDI_FORMAT };
        if (!header.GetItem(i, &item)) continue;
        item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (i == m_sortColumn) item.fmt |= m_descending ? HDF_SORTDOWN : HDF_SORTUP;
        header.SetItem(i, &item);
    }
    m_rebuilding = false;
    UpdateActions();
}

void RecoveryDlg::OnFilter()
{
    if (m_busy) return;
    m_filterPending = true;
    UpdateActions();
    SetTimer(2, 150);
}

void RecoveryDlg::OnColumnClick(NMHDR* header, LRESULT* result)
{
    *result = 0;
    const int column = reinterpret_cast<NMLISTVIEW*>(header)->iSubItem;
    if (m_busy || column < 0 || column > 5) return;
    m_descending = column == m_sortColumn && !m_descending;
    m_sortColumn = column;
    UpdateList();
}

void RecoveryDlg::OnSelectionChanged(NMHDR*, LRESULT* result)
{
    *result = 0;
    UpdateActions();
}

void RecoveryDlg::OnListKeyDown(NMHDR* header, LRESULT* result)
{
    *result = 0;
    if (m_busy || reinterpret_cast<NMLVKEYDOWN*>(header)->wVKey != 'A' || !(GetKeyState(VK_CONTROL) & 0x8000)) return;
    m_list.SetItemState(-1, LVIS_SELECTED, LVIS_SELECTED);
    UpdateActions();
}

void RecoveryDlg::OnDisplayInfo(NMHDR* header, LRESULT* result)
{
    *result = 0;
    auto& item = reinterpret_cast<NMLVDISPINFO*>(header)->item;
    if (!(item.mask & LVIF_TEXT) ||
        item.iItem < 0 || static_cast<size_t>(item.iItem) >= m_visible.size()) return;
    // Retain callback text until the list view finishes consuming it.
    m_cell = CellText(m_visible[item.iItem], item.iSubItem);
    item.pszText = m_cell.data();
}

void RecoveryDlg::OnRecover()
{
    if (m_busy || m_filterPending || !m_filterValid || !m_volume) return;
    // Snapshot record identities before the worker updates per-file results.
    std::vector<size_t> selected;
    for (int item = m_list.GetNextItem(-1, LVNI_SELECTED); item >= 0; item = m_list.GetNextItem(item, LVNI_SELECTED))
        selected.push_back(m_visible[item]);
    if (selected.empty()) return;
    std::wstring destination;
    try { destination = m_volume->ValidateDestination(GetText(IDC_RECOVERY_DEST)); }
    catch (const NtfsRecovery::Failure& failure)
    {
        DisplayError(ErrorText(failure));
        return;
    }
    // A same-volume pair shares one volume GUID; remember approval for this dialog session.
    const auto& source = m_volume->Name();
    if (_wcsnicmp(destination.c_str(), source.c_str(), source.size()) == 0 &&
        std::ranges::none_of(m_confirmedVolumes, [&](const auto& volume)
        { return _wcsicmp(volume.c_str(), source.c_str()) == 0; }))
    {
        if (CMessageBoxDlg::Show(Localization::Lookup(IDS_RECOVERY_SAME_VOLUME_CONFIRM),
            MB_YESNO | MB_ICONWARNING, this) != IDYES) return;
        m_confirmedVolumes.push_back(source);
    }
    for (const auto index : selected) m_outcomes[index].clear();
    m_list.Invalidate(false);
    m_recovering = true;
    StartWorker([this, selected = std::move(selected), destination]
    {
        // Publish per-file results and retain the first error for the completion popup.
        for (const auto index : selected)
        {
            std::wstring outcome;
            try
            {
                m_volume->Recover(m_scan.records[index], destination, m_progress);
                outcome = Localization::Lookup(IDS_RECOVERY_COPIED);
            }
            catch (const NtfsRecovery::Failure& failure)
            {
                if (failure.error == ERROR_CANCELLED || failure.error == ERROR_OPERATION_ABORTED) break;
                outcome = ErrorText(failure);
                if (m_error.empty()) m_error = outcome;
            }
            {
                const std::lock_guard lock(m_resultMutex);
                m_pendingOutcomes.emplace_back(index, std::move(outcome));
            }
        }
    });
}

HBRUSH RecoveryDlg::OnCtlColor(CDC* dc, CWnd* window, const UINT type)
{
    const HBRUSH brush = DarkMode::OnCtlColor(dc, type);
    return brush ? brush : CDialog::OnCtlColor(dc, window, type);
}
