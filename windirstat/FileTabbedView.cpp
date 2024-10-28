// FileTabbedView.cpp - Implementation of CExtensionListControl and CExtensionView
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "stdafx.h"
#include "WinDirStat.h"
#include "FileTabbedView.h"
#include "FileTreeView.h"
#include "Localization.h"

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

    m_FileTreeViewIndex = AddView(RUNTIME_CLASS(CFileTreeView), Localization::Lookup(IDS_ALL_FILES).c_str(), 100);
    m_FileTreeView = DYNAMIC_DOWNCAST(CFileTreeView, GetTabControl().GetTabWnd(m_FileTreeViewIndex));
    m_FileDupeViewIndex = AddView(RUNTIME_CLASS(CFileDupeView), Localization::Lookup(IDS_DUPLICATE_FILES).c_str(), 100);
    m_FileDupeView = DYNAMIC_DOWNCAST(CFileDupeView, GetTabControl().GetTabWnd(m_FileDupeViewIndex));
    GetTabControl().ModifyTabStyle(CMFCTabCtrl::STYLE_3D_ONENOTE);

    return 0;
}

BOOL CFileTabbedView::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

LRESULT CFileTabbedView::OnChangeActiveTab(WPARAM wp, LPARAM lp)
{
    if (wp == m_FileDupeViewIndex && !COptions::ScanForDuplicates)
    {
        return TRUE;
    }

    return CTabView::OnChangeActiveTab(wp, lp);
}
