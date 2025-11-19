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

#include "stdafx.h"
#include "WinDirStat.h"
#include "FileTabbedView.h"
#include "FileSearchView.h"
#include "FileTopView.h"
#include "FileTreeView.h"
#include "Localization.h"
#include "MainFrame.h"
#include "MessageBoxDlg.h"
#include "DirStatDoc.h"
#include "ItemSearch.h"

IMPLEMENT_DYNCREATE(CFileTabbedView, CTabView)

BEGIN_MESSAGE_MAP(CFileTabbedView, CTabView)
    ON_WM_CREATE()
    ON_WM_ERASEBKGND()
    ON_REGISTERED_MESSAGE(AFX_WM_CHANGING_ACTIVE_TAB, OnChangeActiveTab)
END_MESSAGE_MAP()

int CFileTabbedView::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CTabView::OnCreate(lpCreateStruct) == -1)
        return -1;

    m_FileTreeViewIndex = AddView(RUNTIME_CLASS(CFileTreeView), Localization::Lookup(IDS_ALL_FILES).c_str(), CHAR_MAX);
    m_FileTreeView = DYNAMIC_DOWNCAST(CFileTreeView, GetTabControl().GetTabWnd(m_FileTreeViewIndex));
    m_FileTopViewIndex = AddView(RUNTIME_CLASS(CFileTopView), Localization::Lookup(IDS_LARGEST_FILES).c_str(), CHAR_MAX);
    m_FileTopView = DYNAMIC_DOWNCAST(CFileTopView, GetTabControl().GetTabWnd(m_FileTopViewIndex));
    m_FileDupeViewIndex = AddView(RUNTIME_CLASS(CFileDupeView), Localization::Lookup(IDS_DUPLICATE_FILES).c_str(), CHAR_MAX);
    m_FileDupeView = DYNAMIC_DOWNCAST(CFileDupeView, GetTabControl().GetTabWnd(m_FileDupeViewIndex));
    m_FileSearchViewIndex = AddView(RUNTIME_CLASS(CFileSearchView), Localization::Lookup(IDS_SEARCH_RESULTS).c_str(), CHAR_MAX);
    m_FileSearchView = DYNAMIC_DOWNCAST(CFileSearchView, GetTabControl().GetTabWnd(m_FileSearchViewIndex));

    return 0;
}

void CFileTabbedView::OnInitialUpdate()
{
    CTabView::OnInitialUpdate();

    CTabCtrlHelper::SetupTabControl(GetTabControl());

    SetSearchTabVisibility(false);
    SetDupeTabVisibility(COptions::ScanForDuplicates);
}

void CFileTabbedView::SetDupeTabVisibility(const bool show)
{
    GetTabControl().ShowTab(m_FileDupeViewIndex, show);
}

void CFileTabbedView::SetSearchTabVisibility(const bool show)
{
    GetTabControl().ShowTab(m_FileSearchViewIndex, show);
}

BOOL CFileTabbedView::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

LRESULT CFileTabbedView::OnChangeActiveTab(WPARAM wp, LPARAM lp)
{
    if (wp == static_cast<WPARAM>(m_FileDupeViewIndex))
    {
        // Duplicate view can take a while to populate so show wait cursor
        CWaitCursor wc;
        CFileDupeControl::Get()->SortItems();
    }

    return CTabView::OnChangeActiveTab(wp, lp);
}
