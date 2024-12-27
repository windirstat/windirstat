// FileTopView.cpp - Implementation of CFileTopView
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
#include "DirStatDoc.h"
#include "MainFrame.h"
#include "FileTopView.h"
#include "GlobalHelpers.h"
#include "Localization.h"

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNCREATE(CFileTopView, CView)

CFileTopView::CFileTopView() = default;

void CFileTopView::SysColorChanged()
{
    m_Control.SysColorChanged();
}

void CFileTopView::OnDraw(CDC* pDC)
{
    UNREFERENCED_PARAMETER(pDC);
}

#pragma warning(push)
#pragma warning(disable:26454)
BEGIN_MESSAGE_MAP(CFileTopView, CView)
    ON_WM_INITMENUPOPUP()
    ON_WM_SIZE()
    ON_WM_CREATE()
    ON_WM_ERASEBKGND()
    ON_WM_SETFOCUS()
    ON_NOTIFY(LVN_ITEMCHANGED, ID_WDS_CONTROL, OnLvnItemchanged)
    ON_UPDATE_COMMAND_UI(ID_POPUP_TOGGLE, OnUpdatePopupToggle)
    ON_COMMAND(ID_POPUP_TOGGLE, OnPopupToggle)
END_MESSAGE_MAP()
#pragma warning(pop)

void CFileTopView::OnSize(const UINT nType, const int cx, const int cy)
{
    CView::OnSize(nType, cx, cy);
    if (IsWindow(m_Control.m_hWnd))
    {
        CRect rc(0, 0, cx, cy);
        m_Control.MoveWindow(rc);
    }
}

int CFileTopView::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CView::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    constexpr RECT rect = {0, 0, 0, 0};
    VERIFY(m_Control.CreateExtended(0, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, rect, this, ID_WDS_CONTROL));

    m_Control.ShowGrid(COptions::ListGrid);
    m_Control.ShowStripes(COptions::ListStripes);
    m_Control.ShowFullRowSelection(COptions::ListFullRowSelection);

    // Columns should be in enumeration order so initial sort will work
    m_Control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_NAME).c_str(), LVCFMT_LEFT, 500, COL_ITEMTOP_NAME);
    m_Control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_SIZE_PHYSICAL).c_str(), LVCFMT_RIGHT, 90, COL_ITEMTOP_SIZE_PHYSICAL);
    m_Control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_SIZE_LOGICAL).c_str(), LVCFMT_RIGHT, 90, COL_ITEMTOP_SIZE_LOGICAL);
    m_Control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_LASTCHANGE).c_str(), LVCFMT_RIGHT, 100, COL_ITEMTOP_LASTCHANGE);
    m_Control.SetSorting(COL_ITEMTOP_SIZE_PHYSICAL, false);

    m_Control.OnColumnsInserted();

    return 0;
}

BOOL CFileTopView::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

void CFileTopView::OnSetFocus(CWnd* /*pOldWnd*/)
{
    m_Control.SetFocus();
}

void CFileTopView::OnLvnItemchanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    const auto pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

    // only process state changes
    if ((pNMLV->uChanged & LVIF_STATE) == 0)
    {
        return;
    }
  
    // Signal to listeners that selection has changed
    GetDocument()->UpdateAllViews(this, HINT_SELECTIONREFRESH);
     
    *pResult = FALSE;
}

void CFileTopView::OnUpdate(CView* pSender, const LPARAM lHint, CObject* pHint)
{
    ASSERT(AfxGetThread() != nullptr);

    switch (lHint)
    {
        case HINT_NEWROOT:
        {
            m_Control.SetRootItem(GetDocument()->GetRootItemTop());
            m_Control.Invalidate();
        }
        break;

        case HINT_LISTSTYLECHANGED:
        {
            m_Control.ShowGrid(COptions::ListGrid);
            m_Control.ShowStripes(COptions::ListStripes);
            m_Control.ShowFullRowSelection(COptions::ListFullRowSelection);
        }
        break;

        case HINT_NULL:
        {
            CView::OnUpdate(pSender, lHint, pHint);
        }
        break;

        default:
        break;
    }
}

void CFileTopView::OnUpdatePopupToggle(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_Control.SelectedItemCanToggle());
}

void CFileTopView::OnPopupToggle()
{
    m_Control.ToggleSelectedItem();
}
