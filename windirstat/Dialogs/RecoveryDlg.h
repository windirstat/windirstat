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

#pragma once

#include "pch.h"
#include "Layout.h"
#include "RecoveryNtfs.h"
#include "PacMan.h"

class RecoveryDlg final : public MessageTarget<RecoveryDlg, CLayoutDialog>
{
public:
    explicit RecoveryDlg(CWnd* parent) : MessageTarget(IDD_RECOVERY, nullptr, parent) {}
    ~RecoveryDlg() override;
    static std::span<const RouteEntry> Routes();

protected:
    bool OnInitDialog() override;
    void OnCancel() override;
    void OnOK() override {}

private:
    class ResultsList final : public CListCtrl
    {
        bool OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* result) override;
    };

    class ProgressAnimation final : public CStatic
    {
    public:
        CPacman pacman;
        bool paused = false;
        void DrawItem(LPDRAWITEMSTRUCT item) override;
    };

    void OnSourceChanged();
    void OnScan();
    void OnPause();
    void OnStop();
    void OnRecover();
    void OnBrowse();
    void OnFilter();
    void OnSelectionChanged(NMHDR* header, LRESULT* result);
    void OnListKeyDown(NMHDR* header, LRESULT* result);
    void UpdateActions();
    void OnTimer(UINT_PTR timer);
    void OnDisplayInfo(NMHDR* header, LRESULT* result);
    void OnColumnClick(NMHDR* header, LRESULT* result);
    HBRUSH OnCtlColor(CDC* dc, CWnd* window, UINT type);
    void SetBusy(bool busy);
    void StartWorker(std::function<void()> work);
    void UpdateList(size_t first = 0, bool append = false);
    std::wstring CellText(size_t index, int column) const;
    static std::wstring ErrorText(const NtfsRecovery::Failure& failure);

    ResultsList m_list;
    ProgressAnimation m_animation;
    CToolTipCtrl m_tooltips;
    std::array<std::wstring, 4> m_tooltipText;
    std::vector<SmartPointer<HICON, decltype(&DestroyIcon)>> m_icons;
    std::vector<std::wstring> m_roots;
    std::vector<std::wstring> m_confirmedVolumes;
    std::wstring m_root;
    std::unique_ptr<NtfsRecovery> m_volume;
    NtfsRecovery::ScanResult m_scan;
    NtfsRecovery::ScanResult m_completedScan;
    std::vector<std::wstring> m_outcomes;
    std::mutex m_resultMutex;
    std::vector<NtfsRecovery::Record> m_pendingRecords;
    std::vector<std::pair<size_t, std::wstring>> m_pendingOutcomes;
    std::vector<size_t> m_visible;
    NtfsRecovery::Progress m_progress;
    std::wstring m_error;
    std::wstring m_cell;
    bool m_busy = false;
    bool m_timerActive = false;
    bool m_recovering = false;
    bool m_rebuilding = false;
    bool m_filterPending = false;
    bool m_filterValid = true;
    bool m_closing = false;
    int m_sortColumn = 0;
    bool m_descending = false;
    std::atomic<bool> m_done = false;
    std::jthread m_worker;
};

inline std::span<const RouteEntry> RecoveryDlg::Routes()
{
    static constexpr std::array entries
    {
        Route::Control<&OnSourceChanged>(CBN_SELCHANGE, IDC_RECOVERY_SOURCE),
        Route::Control<&OnScan>(BN_CLICKED, IDC_RECOVERY_PLAY),
        Route::Control<&OnPause>(BN_CLICKED, IDC_RECOVERY_PAUSE),
        Route::Control<&OnStop>(BN_CLICKED, IDC_RECOVERY_STOP),
        Route::Control<&OnRecover>(BN_CLICKED, IDC_RECOVERY_RECOVER),
        Route::Control<&OnBrowse>(BN_CLICKED, IDC_RECOVERY_BROWSE),
        Route::Control<&OnFilter>(EN_CHANGE, IDC_RECOVERY_FILTER),
        Route::Control<&OnFilter>(BN_CLICKED, IDC_RECOVERY_REGEX),
        Route::Control<&UpdateActions>(EN_CHANGE, IDC_RECOVERY_DEST),
        Route::Control<&OnCancel>(BN_CLICKED, IDCANCEL),
        Route::Notify<&OnDisplayInfo>(LVN_GETDISPINFO, IDC_RECOVERY_LIST),
        Route::Notify<&OnColumnClick>(LVN_COLUMNCLICK, IDC_RECOVERY_LIST),
        Route::Notify<&OnSelectionChanged>(LVN_ITEMCHANGED, IDC_RECOVERY_LIST),
        Route::Notify<&OnSelectionChanged>(LVN_ODSTATECHANGED, IDC_RECOVERY_LIST),
        Route::Notify<&OnListKeyDown>(LVN_KEYDOWN, IDC_RECOVERY_LIST),
        Route::Window<&OnTimer>(WM_TIMER),
        Route::Window<&OnCtlColor>(WM_CTLCOLOR),
    };
    return entries;
}
