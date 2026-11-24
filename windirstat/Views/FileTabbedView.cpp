// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#include "pch.h"
#include "FileTabbedView.h"
#include "FileTreeView.h"
#include "StorageAnalyticsView.h"

CFileTabbedView::~CFileTabbedView()
{
    DestroyWindow();
}

int CFileTabbedView::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CWinDirStatPane::OnCreate(lpCreateStruct) == -1 ||
        !m_tabControl.Create(CRect(), this, ID_WDS_CONTROL)) return -1;

    // Keep tab IDs stable even when optional tabs are hidden.
    const auto addPane = [this](FileTab tab, std::unique_ptr<CWinDirStatPane> pane, std::wstring_view label)
    {
        const int index = std::to_underlying(tab);
        assert(index == m_tabControl.GetTabCount());
        if (!pane->Create(nullptr, nullptr, WS_CHILD | WS_VISIBLE, CRect(), &m_tabControl,
            static_cast<UINT>(WDS_PANE_ID_BASE + index))) return false;
        m_panes.push_back(std::move(pane));
        m_tabControl.AddTab(m_panes.back().get(), label);
        return true;
    };
    if (!addPane(FileTab::Tree, std::make_unique<CFileTreeView>(), IDS_ALL_FILES) ||
        !addPane(FileTab::Largest, std::make_unique<CFileTopView>(), IDS_LARGEST_FILES) ||
        !addPane(FileTab::Duplicates, std::make_unique<CFileDupeView>(), IDS_DUPLICATE_FILES) ||
        !addPane(FileTab::Search, std::make_unique<CFileSearchView>(), IDS_SEARCH_RESULTS) ||
        !addPane(FileTab::Watcher, std::make_unique<CFileWatcherView>(), IDS_WATCHER) ||
        !addPane(FileTab::Permissions, std::make_unique<CFilePermsView>(), IDS_PERMISSIONS) ||
        !addPane(FileTab::StorageAnalytics, std::make_unique<CStorageAnalyticsView>(), IDS_STORAGE_ANALYTICS))
        return -1;

    Localization::UpdateTabControl(m_tabControl);
    ResetOptionalTabVisibility();
    return 0;
}

void CFileTabbedView::FocusActiveTabContent()
{
    if (CWnd* tabWnd = m_tabControl.GetTabWindow(m_tabControl.GetActiveTab()))
    {
        tabWnd->SetFocus();
    }
}

void CFileTabbedView::OnSetFocus(WindowRef /*pOldWnd*/)
{
    FocusActiveTabContent();
}

void CFileTabbedView::ResetOptionalTabVisibility()
{
    SetTabVisible(FileTab::Search, false);
    SetTabVisible(FileTab::Watcher, false);
    SetTabVisible(FileTab::Permissions, false);
    SetTabVisible(FileTab::StorageAnalytics, false);
    SetTabVisible(FileTab::Duplicates, COptions::ScanForDuplicates &&
        CWinDirStatModel::Get()->GetRootItem() != nullptr);
}

void CFileTabbedView::OnSize(UINT /*nType*/, const int cx, const int cy)
{
    if (IsWindow(m_tabControl.m_hWnd))
    {
        m_tabControl.MoveWindow(0, 0, cx, cy);
    }
}

void CFileTabbedView::SetTabVisible(const FileTab tab, bool show)
{
    // Reveal permissions only after the scan completes.
    if (tab == FileTab::Permissions && show) show = CFilePermsControl::Get()->StartScan();
    m_tabControl.SetTabVisible(std::to_underlying(tab), show);
    if (tab != FileTab::Watcher) return;
    if (show) CFileWatcherControl::Get()->StartMonitoring();
    else
    {
        CFileWatcherControl::Get()->StopMonitoring();
        CFileWatcherControl::Get()->ClearResults();
    }
}

LRESULT CFileTabbedView::OnChangeActiveTab(const WPARAM wp, const LPARAM lp)
{
    UNREFERENCED_PARAMETER(lp);

    if (wp == static_cast<WPARAM>(FileTab::Duplicates))
    {
        // Duplicate view can take a while to populate so show wait cursor
        CWaitCursor wc;
        CFileDupeControl::Get()->SortItems();
    }

    // Show search and watcher buttons for the active tab
    if (CMainFrame::Get() != nullptr)
    {
        CMainFrame::Get()->SetSearchToolBarButtons(wp == static_cast<WPARAM>(FileTab::Search));
        CMainFrame::Get()->SetWatcherToolBarButtons(wp == static_cast<WPARAM>(FileTab::Watcher));
    }

    // Route keyboard focus to the newly active tab's content when focus is
    // already inside this container (tab clicked while app is focused, or
    // programmatic switch from within this pane).
    if (const auto focused = GetFocus(); focused != nullptr &&
        (focused.Handle() == m_hWnd || IsChild(focused)))
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

    for (const auto& pane : m_panes)
    {
        if (pane.get() != sender) pane->OnUpdate(sender, change, item);
    }
}

bool CFileTabbedView::CycleTab(const bool forward)
{
    const int active = m_tabControl.GetActiveTab();
    if (!m_tabControl.IsTabVisible(active)) return false;
    const int step = forward ? 1 : -1;
    for (int i = active + step; i >= 0 && i < m_tabControl.GetTabCount(); i += step)
    {
        if (!m_tabControl.IsTabVisible(i)) continue;
        m_tabControl.SelectTab(i);
        FocusActiveTabContent();
        return true;
    }
    return false;
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
