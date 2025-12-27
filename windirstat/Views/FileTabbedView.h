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
#include "FileTreeView.h"
#include "FileDupeView.h"
#include "FileTopView.h"
#include "FileSearchView.h"

class CFileTabbedView final : public CTabView
{
public:
    bool IsFileTreeViewTabActive() { return GetTabControl().GetActiveTab() == m_fileTreeViewIndex; }
    bool IsFileDupeViewTabActive() { return GetTabControl().GetActiveTab() == m_fileDupeViewIndex; }
    bool IsFileTopViewTabActive() { return GetTabControl().GetActiveTab() == m_fileTopViewIndex; }
    bool IsFileSearchViewTabActive() { return GetTabControl().GetActiveTab() == m_fileTopViewIndex; }
    CFileTopView* GetFileTopView() const { return m_fileTopView; }
    CFileTreeView* GetFileTreeView() const { return m_fileTreeView; }
    CFileDupeView* GetFileDupeView() const { return m_fileDupeView; }
    CFileSearchView* GetFileSearchView() const { return m_fileSearchView; }
    void SetActiveFileTreeView() { SetActiveView(m_fileTreeViewIndex); }
    void SetActiveTopView() { SetActiveView(m_fileTopViewIndex); }
    void SetActiveDupeView() { SetActiveView(m_fileDupeViewIndex); }
    void SetActiveSearchView() { SetActiveView(m_fileSearchViewIndex); }
    void SetDupeTabVisibility(bool show = true);
    void SetSearchTabVisibility(bool show = true);
    bool IsDupeTabVisible() { return GetTabControl().IsTabVisible(m_fileDupeViewIndex); }
    bool IsSearchTabVisible() { return GetTabControl().IsTabVisible(m_fileSearchViewIndex); }

protected:
    CFileTabbedView() = default;
    ~CFileTabbedView() override = default;
    DECLARE_DYNCREATE(CFileTabbedView)
    void OnInitialUpdate() override;

    // Used for storing and retrieving the various tab views
    int m_fileTreeViewIndex = -1;
    CFileTreeView* m_fileTreeView = nullptr;
    int m_fileDupeViewIndex = -1;
    CFileDupeView* m_fileDupeView = nullptr;
    int m_fileTopViewIndex = -1;
    CFileTopView* m_fileTopView = nullptr;
    int m_fileSearchViewIndex = -1;
    CFileSearchView* m_fileSearchView = nullptr;

    DECLARE_MESSAGE_MAP()
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg LRESULT OnChangeActiveTab(WPARAM wp, LPARAM lp);
};
