// FileSearchView.h - Declaration of CFileSearchControl and CFileSearchView
//
// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
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

#include "FileSearchControl.h"

//
// CFileSearchView. The upper left view, which consists of the TreeList.
//
class CFileSearchView final : public CView
{
protected:
    CFileSearchView(); // Created by MFC only
    DECLARE_DYNCREATE(CFileSearchView)

    ~CFileSearchView() override = default;
    void SysColorChanged();

protected:
    void OnDraw(CDC* pDC) override;
    CDirStatDoc* GetDocument() const
    {
        return reinterpret_cast<CDirStatDoc*>(m_pDocument);
    }
    void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override;

    CFileSearchControl m_Control;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnLvnItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnUpdatePopupToggle(CCmdUI* pCmdUI);
    afx_msg void OnPopupToggle();
};
