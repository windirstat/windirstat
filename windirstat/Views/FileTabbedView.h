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
#include "WinDirStatPane.h"
#include "FileTreeView.h"

class CStorageAnalyticsView;

class CFileTabbedView final : public MessageTarget<CFileTabbedView, CWinDirStatPane>
{
public:
    bool IsFileTreeViewTabActive() { return GetTabControl().ActiveTab() == m_fileTreeViewIndex; }
    bool IsFileDupeViewTabActive() { return GetTabControl().ActiveTab() == m_fileDupeViewIndex; }
    bool IsFileTopViewTabActive() { return GetTabControl().ActiveTab() == m_fileTopViewIndex; }
    bool IsFileSearchViewTabActive() { return GetTabControl().ActiveTab() == m_fileSearchViewIndex; }
    bool IsFileWatcherViewTabActive() { return GetTabControl().ActiveTab() == m_fileWatcherViewIndex; }
    bool IsFilePermsViewTabActive() { return GetTabControl().ActiveTab() == m_filePermsViewIndex; }
    bool IsStorageAnalyticsViewTabActive() { return GetTabControl().ActiveTab() == m_storageAnalyticsViewIndex; }
    CFileTopView* GetFileTopView() const { return m_fileTopView; }
    CFileTreeView* GetFileTreeView() const { return m_fileTreeView; }
    CFileDupeView* GetFileDupeView() const { return m_fileDupeView; }
    CFileSearchView* GetFileSearchView() const { return m_fileSearchView; }
    CFileWatcherView* GetFileWatcherView() const { return m_fileWatcherView; }
    CFilePermsView* GetFilePermsView() const { return m_filePermsView; }
    CStorageAnalyticsView* GetStorageAnalyticsView() const { return m_storageAnalyticsView; }
    void SetActiveFileTreeView() { SetActiveView(m_fileTreeViewIndex); }
    void SetActiveTopView() { SetActiveView(m_fileTopViewIndex); }
    void SetActiveDupeView() { SetActiveView(m_fileDupeViewIndex); }
    void SetActiveSearchView() { SetActiveView(m_fileSearchViewIndex); }
    void SetActiveWatcherView() { SetActiveView(m_fileWatcherViewIndex); }
    void SetActivePermsView() { SetActiveView(m_filePermsViewIndex); }
    void SetActiveStorageAnalyticsView() { SetActiveView(m_storageAnalyticsViewIndex); }
    void SetDupeTabVisibility(bool show = true);
    void SetSearchTabVisibility(bool show = true);
    void SetWatcherTabVisibility(bool show = true);
    void SetPermsTabVisibility(bool show = true);
    void SetStorageAnalyticsTabVisibility(bool show = true);
    bool IsDupeTabVisible() { return GetTabControl().IsTabVisible(m_fileDupeViewIndex); }
    bool IsSearchTabVisible() { return GetTabControl().IsTabVisible(m_fileSearchViewIndex); }
    bool IsWatcherTabVisible() { return GetTabControl().IsTabVisible(m_fileWatcherViewIndex); }
    bool IsPermsTabVisible() { return GetTabControl().IsTabVisible(m_filePermsViewIndex); }
    bool IsStorageAnalyticsTabVisible() { return GetTabControl().IsTabVisible(m_storageAnalyticsViewIndex); }
    bool CycleTab(bool forward);
    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;

CFileTabbedView() = default;
    ~CFileTabbedView() override = default;
    void OnInitialUpdate();
    CTabControl& GetTabControl() { return m_tabControl; }
    const CTabControl& GetTabControl() const { return m_tabControl; }
    void SetActiveView(const int index) { m_tabControl.SelectTab(index); }
    template<typename Pane>
    Pane* AddPane(int& index, const std::wstring_view& tabLabel);
    void ResetOptionalTabVisibility();

    // Used for storing and retrieving the various tab views
    CTabControl m_tabControl;
    int m_fileTreeViewIndex = -1;
    CFileTreeView* m_fileTreeView = nullptr;
    int m_fileDupeViewIndex = -1;
    CFileDupeView* m_fileDupeView = nullptr;
    int m_fileTopViewIndex = -1;
    CFileTopView* m_fileTopView = nullptr;
    int m_fileSearchViewIndex = -1;
    CFileSearchView* m_fileSearchView = nullptr;
    int m_fileWatcherViewIndex = -1;
    CFileWatcherView* m_fileWatcherView = nullptr;
    int m_filePermsViewIndex = -1;
    CFilePermsView* m_filePermsView = nullptr;
    int m_storageAnalyticsViewIndex = -1;
    CStorageAnalyticsView* m_storageAnalyticsView = nullptr;

    void FocusActiveTabContent();

static std::span<const RouteEntry> Routes();

protected:
    int OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnSetFocus(CWnd* pOldWnd);
    void OnSize(UINT nType, int cx, int cy);
    bool OnEraseBkgnd(CDC*) { return true; }
    LRESULT OnChangeActiveTab(WPARAM wp, LPARAM lp);
    bool PreprocessMessage(MSG* pMsg) override;
};

inline std::span<const RouteEntry> CFileTabbedView::Routes()
{
    using ThisClass = CFileTabbedView;
    static constexpr std::array entries
    {
        Route::Window<&ThisClass::OnCreate>(WM_CREATE),
        Route::Window<&ThisClass::OnSetFocus>(WM_SETFOCUS),
        Route::Window<&ThisClass::OnSize>(WM_SIZE),
        Route::Window<&ThisClass::OnEraseBkgnd>(WM_ERASEBKGND),
        Route::Window<&ThisClass::OnChangeActiveTab>(WM_WDS_TAB_CHANGED),
    };
    return entries;
}
