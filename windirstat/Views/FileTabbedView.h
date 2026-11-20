// WinDirStat - Windows Directory Statistics
// Copyright © WinDirStat Team
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Distributed WITHOUT ANY WARRANTY; see LICENSE.md for details.

#pragma once

#include "pch.h"
#include "WinDirStatPane.h"
#include "FileTreeView.h"

enum class FileTab { Tree, Largest, Duplicates, Search, Watcher, Permissions, StorageAnalytics };

class CFileTabbedView final : public MessageTarget<CFileTabbedView, CWinDirStatPane>
{
public:
    ~CFileTabbedView() override;
    bool IsActive(FileTab tab) const { return m_tabControl.GetActiveTab() == std::to_underlying(tab); }
    bool IsTabVisible(FileTab tab) const { return m_tabControl.IsTabVisible(std::to_underlying(tab)); }
    CWnd* GetPane(FileTab tab) const { return m_tabControl.GetTabWindow(std::to_underlying(tab)); }
    void SetActiveTab(FileTab tab) { m_tabControl.SelectTab(std::to_underlying(tab)); }
    void SetTabVisible(FileTab tab, bool show);
    void FocusActiveTabContent();
    void OnUpdate(CWnd* sender, MODEL_CHANGE change, CItem* item) override;
    static std::span<const RouteEntry> Routes();

private:
    CTabControl m_tabControl;
    std::vector<std::unique_ptr<CWinDirStatPane>> m_panes;
    void ResetOptionalTabVisibility();
    bool CycleTab(bool forward);

    int OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnSetFocus(WindowRef pOldWnd);
    void OnSize(UINT nType, int cx, int cy);
    bool OnEraseBkgnd(CDC*) { return true; }
    LRESULT OnChangeActiveTab(WPARAM wp, LPARAM lp);
    bool PreprocessMessage(MSG* pMsg) override;
};

inline std::span<const RouteEntry> CFileTabbedView::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnCreate>(WM_CREATE),
        Route::Window<&OnSetFocus>(WM_SETFOCUS),
        Route::Window<&OnSize>(WM_SIZE),
        Route::Window<&OnEraseBkgnd>(WM_ERASEBKGND),
        Route::Window<&OnChangeActiveTab>(WM_WDS_TAB_CHANGED),
    };
    return entries;
}
