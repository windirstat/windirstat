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
#include "FileSearchView.h"
#include "FileTopView.h"
#include "FileTreeView.h"
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

    m_fileTreeViewIndex = AddView(RUNTIME_CLASS(CFileTreeView), IDS_ALL_FILES.data(), CHAR_MAX);
    m_fileTreeView = DYNAMIC_DOWNCAST(CFileTreeView, GetTabControl().GetTabWnd(m_fileTreeViewIndex));
    m_fileTopViewIndex = AddView(RUNTIME_CLASS(CFileTopView), IDS_LARGEST_FILES.data(), CHAR_MAX);
    m_fileTopView = DYNAMIC_DOWNCAST(CFileTopView, GetTabControl().GetTabWnd(m_fileTopViewIndex));
    m_fileDupeViewIndex = AddView(RUNTIME_CLASS(CFileDupeView), IDS_DUPLICATE_FILES.data(), CHAR_MAX);
    m_fileDupeView = DYNAMIC_DOWNCAST(CFileDupeView, GetTabControl().GetTabWnd(m_fileDupeViewIndex));
    m_fileSearchViewIndex = AddView(RUNTIME_CLASS(CFileSearchView), IDS_SEARCH_RESULTS.data(), CHAR_MAX);
    m_fileSearchView = DYNAMIC_DOWNCAST(CFileSearchView, GetTabControl().GetTabWnd(m_fileSearchViewIndex));

    return 0;
}

void CFileTabbedView::OnInitialUpdate()
{
    CTabView::OnInitialUpdate();

    CTabCtrlHelper::SetupTabControl(GetTabControl());
    Localization::UpdateTabControl(GetTabControl());

    SetSearchTabVisibility(false);
    SetDupeTabVisibility(COptions::ScanForDuplicates &&
        CDirStatDoc::Get()->GetRootItem() != nullptr);
}

void CFileTabbedView::SetDupeTabVisibility(const bool show)
{
    GetTabControl().ShowTab(m_fileDupeViewIndex, show);
}

void CFileTabbedView::SetSearchTabVisibility(const bool show)
{
    GetTabControl().ShowTab(m_fileSearchViewIndex, show);
}

BOOL CFileTabbedView::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

LRESULT CFileTabbedView::OnChangeActiveTab(WPARAM wp, LPARAM lp)
{
    if (wp == static_cast<WPARAM>(m_fileDupeViewIndex))
    {
        // Duplicate view can take a while to populate so show wait cursor
        CWaitCursor wc;
        CFileDupeControl::Get()->SortItems();
    }

    return CTabView::OnChangeActiveTab(wp, lp);
}

bool CFileTabbedView::CycleTab(const bool forward)
{
    std::vector<int> visibleTabs;
    for (const int tabIndex : { m_fileTreeViewIndex, m_fileTopViewIndex, m_fileDupeViewIndex, m_fileSearchViewIndex })
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

    return CTabView::PreTranslateMessage(pMsg);
}
