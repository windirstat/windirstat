// FileTreeView.cpp - Implementation of CFileTreeView
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
#include "Item.h"
#include "MainFrame.h"
#include <common/CommonHelpers.h>
#include "FileTreeView.h"
#include "OsSpecific.h"
#include "GlobalHelpers.h"
#include "Localization.h"

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNCREATE(CFileTreeView, CView)

CFileTreeView::CFileTreeView()
{
    m_control.SetSorting(COL_SIZE_PHYSICAL, false);
}

void CFileTreeView::SysColorChanged()
{
    m_control.SysColorChanged();
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
    ON_WM_DESTROY()
    ON_WM_SETFOCUS()
    ON_WM_SETTINGCHANGE()
    ON_NOTIFY(LVN_ITEMCHANGED, ID_WDS_CONTROL, OnLvnItemchanged)
    ON_UPDATE_COMMAND_UI(ID_POPUP_TOGGLE, OnUpdatePopupToggle)
    ON_COMMAND(ID_POPUP_TOGGLE, OnPopupToggle)
END_MESSAGE_MAP()
#pragma warning(pop)

void CFileTreeView::OnSize(UINT nType, int cx, int cy)
{
    CView::OnSize(nType, cx, cy);
    if (::IsWindow(m_control.m_hWnd))
    {
        CRect rc(0, 0, cx, cy);
        m_control.MoveWindow(rc);
    }
}

void CFileTreeView::CreateColumns(bool all)
{
    if (all)
    {
        m_control.InsertColumn(SHORT_MAX, Localization::Lookup(IDS_COL_NAME), LVCFMT_LEFT, 200, COL_NAME);
        m_control.InsertColumn(SHORT_MAX, Localization::Lookup(IDS_COL_SUBTREEPERCENTAGE), LVCFMT_RIGHT, CItem::GetSubtreePercentageWidth(), COL_SUBTREEPERCENTAGE);
        m_control.InsertColumn(SHORT_MAX, Localization::Lookup(IDS_COL_PERCENTAGE), LVCFMT_RIGHT, 55, COL_PERCENTAGE);
    }

    // reset sort and remove optional
    m_control.SetSorting(COL_PERCENTAGE, m_control.GetAscendingDefault(COL_PERCENTAGE));
    m_control.SortItems();
    while (m_control.DeleteColumn(COL_SIZE_PHYSICAL)) {}

    // add optional columns based on settings
    if (COptions::ShowColumnSizePhysical)
        m_control.InsertColumn(SHORT_MAX, Localization::Lookup(IDS_COL_SIZE_PHYSICAL), LVCFMT_RIGHT, 80, COL_SIZE_PHYSICAL);
    if (COptions::ShowColumnSizeLogical)
        m_control.InsertColumn(SHORT_MAX, Localization::Lookup(IDS_COL_SIZE_LOGICAL), LVCFMT_RIGHT, 80, COL_SIZE_LOGICAL);
    if (COptions::ShowColumnItems)
        m_control.InsertColumn(SHORT_MAX, Localization::Lookup(IDS_COL_ITEMS), LVCFMT_RIGHT, 60, COL_ITEMS);
    if (COptions::ShowColumnFiles)
        m_control.InsertColumn(SHORT_MAX, Localization::Lookup(IDS_COL_FILES), LVCFMT_RIGHT, 60, COL_FILES);
    if (COptions::ShowColumnFolders)
        m_control.InsertColumn(SHORT_MAX, Localization::Lookup(IDS_COL_FOLDERS), LVCFMT_RIGHT, 60, COL_FOLDERS);
    if (COptions::ShowColumnLastChange)
        m_control.InsertColumn(SHORT_MAX, Localization::Lookup(IDS_COL_LASTCHANGE), LVCFMT_LEFT, 120, COL_LASTCHANGE);
    if (COptions::ShowColumnAttributes)
        m_control.InsertColumn(SHORT_MAX, Localization::Lookup(IDS_COL_ATTRIBUTES), LVCFMT_LEFT, 50, COL_ATTRIBUTES);
    if (COptions::ShowColumnOwner)
        m_control.InsertColumn(SHORT_MAX, Localization::Lookup(IDS_COL_OWNER), LVCFMT_LEFT, 120, COL_OWNER);

    m_control.OnColumnsInserted();
    
}

int CFileTreeView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CView::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    constexpr RECT rect = {0, 0, 0, 0};
    VERIFY(m_control.CreateEx(LVS_EX_HEADERDRAGDROP, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, rect, this, ID_WDS_CONTROL));

    m_control.ShowGrid(COptions::ListGrid);
    m_control.ShowStripes(COptions::ListStripes);
    m_control.ShowFullRowSelection(COptions::ListFullRowSelection);

    CreateColumns(true);

    m_control.MySetImageList(GetIconImageList());

    return 0;
}

BOOL CFileTreeView::OnEraseBkgnd(CDC* /*pDC*/)
{
    return true;
}

void CFileTreeView::OnDestroy()
{
    m_control.MySetImageList(nullptr);
    CView::OnDestroy();
}

void CFileTreeView::OnSetFocus(CWnd* /*pOldWnd*/)
{
    m_control.SetFocus();
}

void CFileTreeView::OnSettingChange(UINT uFlags, LPCWSTR lpszSection)
{
    if (uFlags & SPI_SETNONCLIENTMETRICS)
    {
        FileIconInit(TRUE);
    }
    CView::OnSettingChange(uFlags, lpszSection);
}
void CFileTreeView::OnLvnItemchanged(NMHDR* pNMHDR, LRESULT* pResult)
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

void CFileTreeView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
    ASSERT(AfxGetThread() != nullptr);

    switch (lHint)
    {
    case HINT_NEWROOT:
        {
            m_control.SetRootItem(GetDocument()->GetRootItem());
            m_control.Sort();
            m_control.Invalidate();
        }
        break;

    case HINT_SELECTIONACTION:
        {
            m_control.EmulateInteractiveSelection(reinterpret_cast<const CItem*>(pHint));
        }
        break;

    case HINT_ZOOMCHANGED:
        {
            CView::OnUpdate(pSender, lHint, pHint);
        }
        break;

    case HINT_LISTSTYLECHANGED:
        {
            m_control.ShowGrid(COptions::ListGrid);
            m_control.ShowStripes(COptions::ListStripes);
            m_control.ShowFullRowSelection(COptions::ListFullRowSelection);
        }
        break;

    case HINT_NULL:
        {
            m_control.Sort();
            CView::OnUpdate(pSender, lHint, pHint);
        }
        break;

    default:
        break;
    }
}

void CFileTreeView::OnUpdatePopupToggle(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_control.SelectedItemCanToggle());
}

void CFileTreeView::OnPopupToggle()
{
    m_control.ToggleSelectedItem();
}
