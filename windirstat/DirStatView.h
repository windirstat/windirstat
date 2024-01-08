// DirStatView.h - Declaration of CMyTreeListControl and CDirStatView
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
//

#pragma once

#include "TreeListControl.h"

class CDirStatView;
class CDirStatDoc;
class CItem;

//
// CMyTreeListControl. I had to derive from CTreeListControl because
// CTreeListControl doesn't know about the column constants (COL_***).
//
class CMyTreeListControl final : public CTreeListControl
{
public:
    CMyTreeListControl(CDirStatView* dirstatView);
    bool GetAscendingDefault(int column) override;

protected:
    void OnItemDoubleClick(int i) override;

    void PrepareDefaultMenu(CMenu* menu, const CItem* item);

    DECLARE_MESSAGE_MAP()
    afx_msg void OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
};

//
// CDirStatView. The upper left view, which consists of the TreeList.
//
class CDirStatView final : public CView
{
protected:
    CDirStatView(); // Created by MFC only
    DECLARE_DYNCREATE(CDirStatView)

public:
    ~CDirStatView() override = default;
    CStringW GenerateReport();
    void SysColorChanged();

protected:
    void OnDraw(CDC* pDC) override;
    CDirStatDoc* GetDocument() const
    {
        return reinterpret_cast<CDirStatDoc*>(m_pDocument);
    }
    void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override;

    CMyTreeListControl m_treeListControl; // The tree list

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnDestroy();
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnSettingChange(UINT uFlags, LPCWSTR lpszSection);
    afx_msg void OnLvnItemchanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnUpdatePopupToggle(CCmdUI* pCmdUI);
    afx_msg void OnPopupToggle();
};
