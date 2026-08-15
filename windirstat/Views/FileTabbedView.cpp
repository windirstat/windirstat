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
#include "FileTabbedView.h"
#include "FileTreeView.h"
#include "StorageAnalyticsView.h"

template<typename Pane>
Pane* CFileTabbedView::AddPane(int& index, const std::wstring_view& tabLabel)
{
    static_assert(std::is_base_of_v<CWinDirStatPane, Pane>);
    auto* pane = new Pane;
    index = GetTabControl().TabCount();
    if (!pane->Create(nullptr, nullptr, WS_CHILD | WS_VISIBLE, CRect(), &m_tabControl,
        static_cast<UINT>(WDS_PANE_ID_BASE + index)))
    {
        assert(false);
        return nullptr;
    }

    GetTabControl().AddTab(pane, tabLabel);
    return pane;
}

int CFileTabbedView::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CWinDirStatPane::OnCreate(lpCreateStruct) == -1)
        return -1;

    m_tabControl.Create(CRect(), this, ID_WDS_CONTROL);

    m_fileTreeView = AddPane<CFileTreeView>(m_fileTreeViewIndex, IDS_ALL_FILES);
    m_fileTopView = AddPane<CFileTopView>(m_fileTopViewIndex, IDS_LARGEST_FILES);
    m_fileDupeView = AddPane<CFileDupeView>(m_fileDupeViewIndex, IDS_DUPLICATE_FILES);
    m_fileSearchView = AddPane<CFileSearchView>(m_fileSearchViewIndex, IDS_SEARCH_RESULTS);
    m_fileWatcherView = AddPane<CFileWatcherView>(m_fileWatcherViewIndex, IDS_WATCHER);
    m_filePermsView = AddPane<CFilePermsView>(m_filePermsViewIndex, IDS_PERMISSIONS);
    m_storageAnalyticsView = AddPane<CStorageAnalyticsView>(m_storageAnalyticsViewIndex, IDS_STORAGE_ANALYTICS);
    if (m_fileTreeView == nullptr || m_fileTopView == nullptr || m_fileDupeView == nullptr
        || m_fileSearchView == nullptr || m_fileWatcherView == nullptr || m_filePermsView == nullptr
        || m_storageAnalyticsView == nullptr)
    {
        return -1;
    }

    OnInitialUpdate();
    return 0;
}

void CFileTabbedView::FocusActiveTabContent()
{
    if (CWnd* tabWnd = m_tabControl.TabWindow(m_tabControl.ActiveTab()))
    {
        tabWnd->SetFocus();
    }
}

void CFileTabbedView::OnSetFocus(CWnd* /*pOldWnd*/)
{
    FocusActiveTabContent();
}

void CFileTabbedView::OnInitialUpdate()
{
    CTabCtrlHelper::SetupTabControl(GetTabControl());
    Localization::UpdateTabControl(GetTabControl());
    ResetOptionalTabVisibility();
}

void CFileTabbedView::ResetOptionalTabVisibility()
{
    SetSearchTabVisibility(false);
    SetWatcherTabVisibility(false);
    SetPermsTabVisibility(false);
    SetStorageAnalyticsTabVisibility(false);
    SetDupeTabVisibility(COptions::ScanForDuplicates &&
        CWinDirStatModel::Get()->GetRootItem() != nullptr);
}

void CFileTabbedView::OnSize(UINT /*nType*/, const int cx, const int cy)
{
    if (IsWindow(m_tabControl.m_hWnd))
    {
        m_tabControl.MoveWindow(0, 0, cx, cy);
    }
}

void CFileTabbedView::SetDupeTabVisibility(const bool show)
{
    GetTabControl().SetTabVisible(m_fileDupeViewIndex, show);
}

void CFileTabbedView::SetSearchTabVisibility(const bool show)
{
    GetTabControl().SetTabVisible(m_fileSearchViewIndex, show);
}

void CFileTabbedView::SetWatcherTabVisibility(const bool show)
{
    GetTabControl().SetTabVisible(m_fileWatcherViewIndex, show);
    if (show)
    {
        CFileWatcherControl::Get()->StartMonitoring();
    }
    else
    {
        CFileWatcherControl::Get()->StopMonitoring();
        CFileWatcherControl::Get()->DeleteAllItems();
    }
}

void CFileTabbedView::SetPermsTabVisibility(const bool show)
{
    if (!show)
    {
        GetTabControl().SetTabVisible(m_filePermsViewIndex, false);
        return;
    }

    // Scan first; only reveal the tab if the scan completed (a cancelled scan stays hidden)
    GetTabControl().SetTabVisible(m_filePermsViewIndex, CFilePermsControl::Get()->StartScan());
}

void CFileTabbedView::SetStorageAnalyticsTabVisibility(const bool show)
{
    GetTabControl().SetTabVisible(m_storageAnalyticsViewIndex, show);
}

LRESULT CFileTabbedView::OnChangeActiveTab(const WPARAM wp, const LPARAM lp)
{
    UNREFERENCED_PARAMETER(lp);

    if (wp == static_cast<WPARAM>(m_fileDupeViewIndex))
    {
        // Duplicate view can take a while to populate so show wait cursor
        CWaitCursor wc;
        CFileDupeControl::Get()->SortItems();
    }

    // Show the contextual watcher toolbar buttons only while its tab is active
    if (CMainFrame::Get() != nullptr)
    {
        CMainFrame::Get()->SetWatcherToolBarButtons(wp == static_cast<WPARAM>(m_fileWatcherViewIndex));
    }

    // Route keyboard focus to the newly-active tab's content when focus is
    // already inside this container (tab clicked while app is focused, or
    // programmatic switch from within this pane).
    if (const CWnd* focused = GetFocus(); focused != nullptr &&
        (focused->Handle() == m_hWnd || IsChild(focused)))
    {
        FocusActiveTabContent();
    }

    return 0;
}

void CFileTabbedView::OnUpdate(CWnd* sender, const MODEL_CHANGE change, CItem* item)
{
    if (change == MODEL_CHANGE_NEW_ROOT)
    {
        ResetOptionalTabVisibility();
    }

    for (auto* pane : { static_cast<CWinDirStatPane*>(m_fileTreeView),
        static_cast<CWinDirStatPane*>(m_fileTopView),
        static_cast<CWinDirStatPane*>(m_fileDupeView),
        static_cast<CWinDirStatPane*>(m_fileSearchView),
        static_cast<CWinDirStatPane*>(m_fileWatcherView),
        static_cast<CWinDirStatPane*>(m_filePermsView),
        static_cast<CWinDirStatPane*>(m_storageAnalyticsView) })
    {
        if (pane != nullptr && pane != sender)
        {
            pane->OnUpdate(sender, change, item);
        }
    }
}

bool CFileTabbedView::CycleTab(const bool forward)
{
    std::vector<int> visibleTabs;
    for (const int tabIndex : { m_fileTreeViewIndex, m_fileTopViewIndex, m_fileDupeViewIndex, m_fileSearchViewIndex, m_fileWatcherViewIndex, m_filePermsViewIndex, m_storageAnalyticsViewIndex })
    {
        if (GetTabControl().IsTabVisible(tabIndex)) visibleTabs.push_back(tabIndex);
    }

    const int activeTab = GetTabControl().ActiveTab();
    const auto it = std::ranges::find(visibleTabs, activeTab);
    if (it == visibleTabs.end()) return false;

    const size_t currentPos = std::distance(visibleTabs.begin(), it);
    const size_t nextPos = currentPos + (forward ? 1 : -1);

    if (nextPos >= visibleTabs.size()) return false;

    SetActiveView(visibleTabs[nextPos]);
    FocusActiveTabContent();
    return true;
}

bool CFileTabbedView::PreprocessMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB)
    {
        if (!CycleTab(!IsKeyDown(VK_SHIFT)))
        {
            CMainFrame::Get()->MoveFocus(LF_EXTLIST);
        }
        return true;
    }

    return CWinDirStatPane::PreprocessMessage(pMsg);
}
