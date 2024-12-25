// FileTreeView.h - Declaration of CFileTreeControl and CFileTreeView
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

#include "FileTreeControl.h"
#include "DirStatDoc.h"

//
// CFileTreeView. The upper left view, which consists of the TreeList.
//
class CFileTreeView final : public CView
{
protected:
    CFileTreeView(); // Created by MFC only
    DECLARE_DYNCREATE(CFileTreeView)

    ~CFileTreeView() override = default;
    void SysColorChanged();
    void CreateColumns(bool all = false);

protected:
    void OnDraw(CDC* pDC) override;
    CDirStatDoc* GetDocument() const
    {
        return reinterpret_cast<CDirStatDoc*>(m_pDocument);
    }
    void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override;

    CFileTreeControl m_Control;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnSettingChange(UINT uFlags, LPCWSTR lpszSection);
    afx_msg void OnLvnItemchanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnUpdatePopupToggle(CCmdUI* pCmdUI);
    afx_msg void OnPopupToggle();
};
