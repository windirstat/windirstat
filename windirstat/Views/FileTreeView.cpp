﻿// FileTreeView.cpp - Implementation of CFileTreeView
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

#include "stdafx.h"
#include "WinDirStat.h"
#include "DirStatDoc.h"
#include "Item.h"
#include "MainFrame.h"
#include "FileTreeView.h"
#include "GlobalHelpers.h"
#include "Localization.h"

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNCREATE(CFileTreeView, CView)

CFileTreeView::CFileTreeView() = default;

void CFileTreeView::SysColorChanged()
{
    m_Control.SysColorChanged();
}

void CFileTreeView::OnDraw(CDC* pDC)
{
    UNREFERENCED_PARAMETER(pDC);
}

#pragma warning(push)
#pragma warning(disable:26454)
BEGIN_MESSAGE_MAP(CFileTreeView, CView)
    ON_WM_INITMENUPOPUP()
    ON_WM_SIZE()
    ON_WM_CREATE()
    ON_WM_ERASEBKGND()
    ON_WM_SETFOCUS()
    ON_NOTIFY(LVN_ITEMCHANGED, ID_WDS_CONTROL, OnLvnItemChanged)
    ON_UPDATE_COMMAND_UI(ID_POPUP_TOGGLE, OnUpdatePopupToggle)
    ON_COMMAND(ID_POPUP_TOGGLE, OnPopupToggle)
END_MESSAGE_MAP()
#pragma warning(pop)

void CFileTreeView::OnSize(const UINT nType, const int cx, const int cy)
{
    CView::OnSize(nType, cx, cy);
    if (IsWindow(m_Control.m_hWnd))
    {
        CRect rc(0, 0, cx, cy);
        m_Control.MoveWindow(rc);
    }
}

void CFileTreeView::CreateColumns(const bool all)
{
    if (all)
    {
        // Columns should be in enumeration order so initial sort will work
        m_Control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_NAME).c_str(), LVCFMT_LEFT, 250, COL_NAME);
        m_Control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_SUBTREEPERCENTAGE).c_str(), LVCFMT_RIGHT, CItem::GetSubtreePercentageWidth() + 30, COL_SUBTREEPERCENTAGE);
        m_Control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_PERCENTAGE).c_str(), LVCFMT_RIGHT, 90, COL_PERCENTAGE);
    }

    // reset sort and remove optional columns
    m_Control.SetSorting(COL_PERCENTAGE, m_Control.GetAscendingDefault(COL_PERCENTAGE));
    m_Control.SortItems();
    while (m_Control.DeleteColumn(COL_OPTIONAL_START)) {}

    // add optional columns based on settings
    if (COptions::ShowColumnSizePhysical)
        m_Control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_SIZE_PHYSICAL).c_str(), LVCFMT_RIGHT, 90, COL_SIZE_PHYSICAL);
    if (COptions::ShowColumnSizeLogical)
        m_Control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_SIZE_LOGICAL).c_str(), LVCFMT_RIGHT, 90, COL_SIZE_LOGICAL);
    if (COptions::ShowColumnItems)
        m_Control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_ITEMS).c_str(), LVCFMT_RIGHT, 90, COL_ITEMS);
    if (COptions::ShowColumnFiles)
        m_Control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_FILES).c_str(), LVCFMT_RIGHT, 90, COL_FILES);
    if (COptions::ShowColumnFolders)
        m_Control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_FOLDERS).c_str(), LVCFMT_RIGHT, 90, COL_FOLDERS);
    if (COptions::ShowColumnLastChange)
        m_Control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_LASTCHANGE).c_str(), LVCFMT_LEFT, 120, COL_LASTCHANGE);
    if (COptions::ShowColumnAttributes)
        m_Control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_ATTRIBUTES).c_str(), LVCFMT_LEFT, 90, COL_ATTRIBUTES);
    if (COptions::ShowColumnOwner)
        m_Control.InsertColumn(CHAR_MAX, Localization::Lookup(IDS_COL_OWNER).c_str(), LVCFMT_LEFT, 200, COL_OWNER);

    m_Control.OnColumnsInserted();
    
}

int CFileTreeView::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CView::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    constexpr RECT rect = {0, 0, 0, 0};
    VERIFY(m_Control.CreateExtended(LVS_EX_HEADERDRAGDROP, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, rect, this, ID_WDS_CONTROL));

    m_Control.ShowGrid(COptions::ListGrid);
    m_Control.ShowStripes(COptions::ListStripes);
    m_Control.ShowFullRowSelection(COptions::ListFullRowSelection);

    CreateColumns(true);

    return 0;
}

BOOL CFileTreeView::OnEraseBkgnd(CDC* /*pDC*/)
{
    return true;
}

void CFileTreeView::OnSetFocus(CWnd* /*pOldWnd*/)
{
    m_Control.SetFocus();
}

void CFileTreeView::OnLvnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
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

void CFileTreeView::OnUpdate(CView* pSender, const LPARAM lHint, CObject* pHint)
{
    ASSERT(AfxGetThread() != nullptr);

    switch (lHint)
    {
    case HINT_NEWROOT:
        {
            m_Control.SetRootItem(GetDocument()->GetRootItem());
            m_Control.Invalidate();
        }
        break;

    case HINT_SELECTIONACTION:
        {
            m_Control.EmulateInteractiveSelection(reinterpret_cast<const CItem*>(pHint));
        }
        break;

    case HINT_ZOOMCHANGED:
        {
            CView::OnUpdate(pSender, lHint, pHint);
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

void CFileTreeView::OnUpdatePopupToggle(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_Control.SelectedItemCanToggle());
}

void CFileTreeView::OnPopupToggle()
{
    m_Control.ToggleSelectedItem();
}
