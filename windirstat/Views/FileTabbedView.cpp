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

IMPLEMENT_DYNCREATE(CFileTabbedView, CWinDirStatPane)

BEGIN_MESSAGE_MAP(CFileTabbedView, CWinDirStatPane)
    ON_WM_CREATE()
    ON_WM_SETFOCUS()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_REGISTERED_MESSAGE(AFX_WM_CHANGING_ACTIVE_TAB, OnChangeActiveTab)
END_MESSAGE_MAP()

int CFileTabbedView::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CWinDirStatPane::OnCreate(lpCreateStruct) == -1)
        return -1;

    m_tabControl.Create(CMFCTabCtrl::STYLE_3D_VS2005, CRect(), this, ID_WDS_CONTROL);

    m_fileTreeViewIndex = AddPane(RUNTIME_CLASS(CFileTreeView), IDS_ALL_FILES);
    m_fileTreeView = DYNAMIC_DOWNCAST(CFileTreeView, GetTabControl().GetTabWnd(m_fileTreeViewIndex));
    m_fileTopViewIndex = AddPane(RUNTIME_CLASS(CFileTopView), IDS_LARGEST_FILES);
    m_fileTopView = DYNAMIC_DOWNCAST(CFileTopView, GetTabControl().GetTabWnd(m_fileTopViewIndex));
    m_fileDupeViewIndex = AddPane(RUNTIME_CLASS(CFileDupeView), IDS_DUPLICATE_FILES);
    m_fileDupeView = DYNAMIC_DOWNCAST(CFileDupeView, GetTabControl().GetTabWnd(m_fileDupeViewIndex));
    m_fileSearchViewIndex = AddPane(RUNTIME_CLASS(CFileSearchView), IDS_SEARCH_RESULTS);
    m_fileSearchView = DYNAMIC_DOWNCAST(CFileSearchView, GetTabControl().GetTabWnd(m_fileSearchViewIndex));
    m_fileWatcherViewIndex = AddPane(RUNTIME_CLASS(CFileWatcherView), IDS_WATCHER);
    m_fileWatcherView = DYNAMIC_DOWNCAST(CFileWatcherView, GetTabControl().GetTabWnd(m_fileWatcherViewIndex));
    m_filePermsViewIndex = AddPane(RUNTIME_CLASS(CFilePermsView), IDS_PERMISSIONS);
    m_filePermsView = DYNAMIC_DOWNCAST(CFilePermsView, GetTabControl().GetTabWnd(m_filePermsViewIndex));
    m_storageAnalyticsViewIndex = AddPane(RUNTIME_CLASS(CStorageAnalyticsView), IDS_STORAGE_ANALYTICS);
    m_storageAnalyticsView = DYNAMIC_DOWNCAST(CStorageAnalyticsView, GetTabControl().GetTabWnd(m_storageAnalyticsViewIndex));

    OnInitialUpdate();
    return 0;
}

void CFileTabbedView::FocusActiveTabContent()
{
    if (CWnd* tabWnd = m_tabControl.GetTabWnd(m_tabControl.GetActiveTab()))
    {
        tabWnd->SetFocus();
    }
}

void CFileTabbedView::OnSetFocus(CWnd* /*pOldWnd*/)
{
    FocusActiveTabContent();
}

int CFileTabbedView::AddPane(CRuntimeClass* paneClass, const std::wstring_view& tabLabel)
{
    ASSERT(paneClass != nullptr);

    auto* pane = DYNAMIC_DOWNCAST(CWinDirStatPane, paneClass->CreateObject());
    if (pane == nullptr)
    {
        ASSERT(FALSE);
        return -1;
    }

    const int index = GetTabControl().GetTabsNum();
    if (!pane->Create(nullptr, nullptr, WS_CHILD | WS_VISIBLE, CRect(), &m_tabControl,
        static_cast<UINT>(AFX_IDW_PANE_FIRST + index), nullptr))
    {
        delete pane;
        ASSERT(FALSE);
        return -1;
    }

    GetTabControl().AddTab(pane, std::wstring(tabLabel).c_str(), static_cast<UINT>(-1));
    return index;
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

void CFileTabbedView::OnSize(const UINT nType, const int cx, const int cy)
{
    CWinDirStatPane::OnSize(nType, cx, cy);
    if (IsWindow(m_tabControl.m_hWnd))
    {
        m_tabControl.MoveWindow(0, 0, cx, cy);
    }
}

void CFileTabbedView::SetDupeTabVisibility(const bool show)
{
    GetTabControl().ShowTab(m_fileDupeViewIndex, show);
}

void CFileTabbedView::SetSearchTabVisibility(const bool show)
{
    GetTabControl().ShowTab(m_fileSearchViewIndex, show);
}

void CFileTabbedView::SetWatcherTabVisibility(const bool show)
{
    GetTabControl().ShowTab(m_fileWatcherViewIndex, show);
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
        GetTabControl().ShowTab(m_filePermsViewIndex, false);
        CFilePermsControl::Get()->StopScan();
        return;
    }

    // Scan first; only reveal the tab if the scan completed (a cancelled scan stays hidden)
    GetTabControl().ShowTab(m_filePermsViewIndex, CFilePermsControl::Get()->StartScan());
}

void CFileTabbedView::SetStorageAnalyticsTabVisibility(const bool show)
{
    GetTabControl().ShowTab(m_storageAnalyticsViewIndex, show);
}

BOOL CFileTabbedView::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

LRESULT CFileTabbedView::OnChangeActiveTab(WPARAM wp, LPARAM lp)
{
    UNREFERENCED_PARAMETER(lp);

    if (wp == static_cast<WPARAM>(m_fileDupeViewIndex))
    {
        // Duplicate view can take a while to populate so show wait cursor
        CWaitCursor wc;
        CFileDupeControl::Get()->SortItems();
    }

    // Show the contextual watcher toolbar buttons only while its tab is active;
    // this message is sent before the switch, so compare against the new index
    if (CMainFrame::Get() != nullptr)
    {
        CMainFrame::Get()->SetWatcherToolBarButtons(wp == static_cast<WPARAM>(m_fileWatcherViewIndex));
    }

    // Route keyboard focus to the newly-active tab's content when focus is
    // already inside this container (tab clicked while app is focused, or
    // programmatic switch from within this pane).
    if (const CWnd* focused = GetFocus(); focused != nullptr &&
        (focused->GetSafeHwnd() == m_hWnd || IsChild(focused)))
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

    const int activeTab = GetTabControl().GetActiveTab();
    const auto it = std::ranges::find(visibleTabs, activeTab);
    if (it == visibleTabs.end()) return false;

    const size_t currentPos = std::distance(visibleTabs.begin(), it);
    const size_t nextPos = currentPos + (forward ? 1 : -1);

    if (nextPos >= visibleTabs.size()) return false;

    SetActiveView(visibleTabs[nextPos]);
    FocusActiveTabContent();
    return true;
}

BOOL CFileTabbedView::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_TAB)
    {
        if (!CycleTab(!IsShiftKeyDown()))
        {
            CMainFrame::Get()->MoveFocus(LF_EXTLIST);
        }
        return TRUE;
    }

    return CWinDirStatPane::PreTranslateMessage(pMsg);
}
