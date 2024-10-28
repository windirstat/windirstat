// FileTabbedView.h - Declaration of CExtensionListControl and CExtensionView
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

#pragma once

#include "stdafx.h"
#include "FileTreeView.h"
#include "FileDupeView.h"

class CFileTabbedView : public CTabView
{
protected:
    CFileTabbedView() = default;
    ~CFileTabbedView() override = default;
    DECLARE_DYNCREATE(CFileTabbedView)

    // Used for storing and retrieving the various tab views
    int m_FileTreeViewIndex = -1;
    CFileTreeView* m_FileTreeView = nullptr;
    CFileTreeView* GetFileTreeView() const { return m_FileTreeView; }
    int m_FileDupeViewIndex = -1;
    CFileDupeView* m_FileDupeView = nullptr;
    CFileDupeView* GetFileDupeView() const { return m_FileDupeView; }

    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg LRESULT OnChangeActiveTab(WPARAM wp, LPARAM lp);
    DECLARE_MESSAGE_MAP()
};
