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

#include "stdafx.h"
#include "FileTreeView.h"
#include "FileDupeView.h"
#include "FileTopView.h"
#include "FileSearchView.h"

class CFileTabbedView final : public CTabView
{
public:
    bool IsFileTreeViewTabActive() { return GetTabControl().GetActiveTab() == m_FileTreeViewIndex; }
    bool IsFileDupeViewTabActive() { return GetTabControl().GetActiveTab() == m_FileDupeViewIndex; }
    bool IsFileTopViewTabActive() { return GetTabControl().GetActiveTab() == m_FileTopViewIndex; }
    bool IsFileSearchViewTabActive() { return GetTabControl().GetActiveTab() == m_FileTopViewIndex; }
    CFileTopView* GetFileTopView() const { return m_FileTopView; }
    CFileTreeView* GetFileTreeView() const { return m_FileTreeView; }
    CFileDupeView* GetFileDupeView() const { return m_FileDupeView; }
    CFileSearchView* GetFileSearchView() const { return m_FileSearchView; }
    void SetActiveFileTreeView() { SetActiveView(m_FileTreeViewIndex); }
    void SetActiveTopView() { SetActiveView(m_FileTopViewIndex); }
    void SetActiveDupeView() { SetActiveView(m_FileDupeViewIndex); }
    void SetActiveSearchView() { SetActiveView(m_FileSearchViewIndex); }
    void SetDupeTabVisibility(bool show = true);
    void SetSearchTabVisibility(bool show = true);

protected:
    CFileTabbedView() = default;
    ~CFileTabbedView() override = default;
    DECLARE_DYNCREATE(CFileTabbedView)
    void OnInitialUpdate() override;

    // Used for storing and retrieving the various tab views
    int m_FileTreeViewIndex = -1;
    CFileTreeView* m_FileTreeView = nullptr;
    int m_FileDupeViewIndex = -1;
    CFileDupeView* m_FileDupeView = nullptr;
    int m_FileTopViewIndex = -1;
    CFileTopView* m_FileTopView = nullptr;
    int m_FileSearchViewIndex = -1;
    CFileSearchView* m_FileSearchView = nullptr;

    DECLARE_MESSAGE_MAP()
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg LRESULT OnChangeActiveTab(WPARAM wp, LPARAM lp);
};
