// DirStatView.cpp - Implementation of CDirStatView
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
#include "DirStatView.h"
#include "OsSpecific.h"
#include "GlobalHelpers.h"

CMyTreeListControl::CMyTreeListControl() : CTreeListControl(20)
{
}

bool CMyTreeListControl::GetAscendingDefault(int column)
{
    return column == COL_NAME || column == COL_LASTCHANGE;
}

BEGIN_MESSAGE_MAP(CMyTreeListControl, CTreeListControl)
    ON_WM_CONTEXTMENU()
    ON_WM_SETFOCUS()
    ON_WM_KEYDOWN()
    ON_NOTIFY_EX(HDN_ENDDRAG, 0, OnHeaderEndDrag)
END_MESSAGE_MAP()

BOOL CMyTreeListControl::OnHeaderEndDrag(UINT, NMHDR* pNMHDR, LRESULT* pResult)
{
    // Do not allow first column to be re-ordered
    const LPNMHEADERW hdr = reinterpret_cast<LPNMHEADERW>(pNMHDR);
    const BOOL block = (hdr->iItem == COL_NAME || hdr->pitem->iOrder == COL_NAME);
    *pResult = block;
    return block;
}

void CMyTreeListControl::OnContextMenu(CWnd* /*pWnd*/, CPoint pt)
{
    const int i = GetSelectionMark();
    if (i == -1)
    {
        return;
    }

    CTreeListItem* item = GetItem(i);
    CRect rc = GetWholeSubitemRect(i, 0);
    const CRect rcTitle = item->GetTitleRect() + rc.TopLeft();

    CMenu menu;
    menu.LoadMenu(IDR_POPUPLIST);
    CMenu* sub = menu.GetSubMenu(0);

    PrepareDefaultMenu(sub, static_cast<CItem*>(item));
    GetMainFrame()->AppendUserDefinedCleanups(sub);
    
    // Show popup menu and act accordingly.
    //
    // The menu shall not overlap the label but appear
    // horizontally at the cursor position,
    // vertically under (or above) the label.
    // TrackPopupMenuEx() behaves in the desired way, if
    // we exclude the label rectangle extended to full screen width.
    // 
    // Thanks to Sven for this compromise between the old WinDirStat
    // behavior (show the menu to the right of the label) and the
    // Explorer behavior (show the menu at the cursor position).

    TPMPARAMS tp;
    tp.cbSize    = sizeof(tp);
    tp.rcExclude = rcTitle;
    ClientToScreen(&tp.rcExclude);

    CRect desktop;
    GetDesktopWindow()->GetWindowRect(desktop);

    tp.rcExclude.left  = desktop.left;
    tp.rcExclude.right = desktop.right;

    constexpr int overlap = 2; // a little vertical overlapping
    tp.rcExclude.top += overlap;
    tp.rcExclude.bottom -= overlap;

    sub->TrackPopupMenuEx(TPM_LEFTALIGN | TPM_LEFTBUTTON, pt.x, pt.y, AfxGetMainWnd(), &tp);
}

void CMyTreeListControl::OnItemDoubleClick(int i)
{
    const auto item = reinterpret_cast<const CItem*>(GetItem(i));
    if (item->IsType(IT_FILE))
    {
        CDirStatDoc::OpenItem(item);
    }
    else
    {
        CTreeListControl::OnItemDoubleClick(i);
    }
}

void CMyTreeListControl::PrepareDefaultMenu(CMenu* menu, const CItem* item)
{
    if (item->TmiIsLeaf())
    {
        menu->DeleteMenu(0, MF_BYPOSITION); // Remove "Expand/Collapse" item
        menu->DeleteMenu(0, MF_BYPOSITION); // Remove separator
        menu->SetDefaultItem(ID_CLEANUP_OPEN_SELECTED, false);
    }
    else
    {
        const CStringW command = LoadString(item->IsExpanded() && item->HasChildren() ? IDS_COLLAPSE : IDS_EXPAND);
        VERIFY(menu->ModifyMenu(ID_POPUP_TOGGLE, MF_BYCOMMAND | MF_STRING, ID_POPUP_TOGGLE, command));
        menu->SetDefaultItem(ID_POPUP_TOGGLE, false);
    }
}

void CMyTreeListControl::OnSetFocus(CWnd* pOldWnd)
{
    CTreeListControl::OnSetFocus(pOldWnd);
    GetMainFrame()->SetLogicalFocus(LF_DIRECTORYLIST);
}

void CMyTreeListControl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_TAB)
    {
        GetMainFrame()->MoveFocus(LF_EXTENSIONLIST);
    }
    else if (nChar == VK_ESCAPE)
    {
        GetMainFrame()->MoveFocus(LF_NONE);
    }
    CTreeListControl::OnKeyDown(nChar, nRepCnt, nFlags);
}

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNCREATE(CDirStatView, CView)

CDirStatView::CDirStatView()
{
    m_treeListControl.SetSorting(COL_SUBTREETOTAL, false);
}

CStringW CDirStatView::GenerateReport()
{
    CStringW report = CStringW(COptions::ReportPrefix.Obj().c_str()) + L"\r\n";

    const auto& items = CTreeListControl::GetTheTreeListControl()->GetAllSelected<CItem>();
    for (const auto& root : items)
    {
        ASSERT(root->IsVisible());

        const int r = m_treeListControl.FindTreeItem(root);

        for (
            int i = r;
            i < m_treeListControl.GetItemCount()
            && (i == r || m_treeListControl.GetItem(i)->GetIndent() > root->GetIndent());
            i++
            )
        {
            const CItem* item = static_cast<CItem*>(m_treeListControl.GetItem(i));

            if (item->IsType(IT_MYCOMPUTER))
            {
                continue;
            }

            report.AppendFormat(L"%s %s\r\n", PadWidthBlanks(FormatLongLongHuman(item->GetSize()), 11).GetString(), item->GetReportPath().GetString());
        }
    }

    report += L"\r\n\r\n";
    report += COptions::ReportSuffix.Obj().c_str();

    return report;
}

void CDirStatView::SysColorChanged()
{
    m_treeListControl.SysColorChanged();
}

void CDirStatView::OnDraw(CDC* pDC)
{
    UNREFERENCED_PARAMETER(pDC);
}

BEGIN_MESSAGE_MAP(CDirStatView, CView)
    ON_WM_INITMENUPOPUP()
    ON_WM_SIZE()
    ON_WM_CREATE()
    ON_WM_ERASEBKGND()
    ON_WM_DESTROY()
    ON_WM_SETFOCUS()
    ON_WM_SETTINGCHANGE()
#pragma warning(suppress: 26454)
    ON_NOTIFY(LVN_ITEMCHANGED, ID_WDS_CONTROL, OnLvnItemchanged)
    ON_UPDATE_COMMAND_UI(ID_POPUP_TOGGLE, OnUpdatePopupToggle)
    ON_COMMAND(ID_POPUP_TOGGLE, OnPopupToggle)
END_MESSAGE_MAP()

void CDirStatView::OnSize(UINT nType, int cx, int cy)
{
    CView::OnSize(nType, cx, cy);
    if (::IsWindow(m_treeListControl.m_hWnd))
    {
        CRect rc(0, 0, cx, cy);
        m_treeListControl.MoveWindow(rc);
    }
}

void CDirStatView::CreateColumns(bool all)
{
    if (all)
    {
        m_treeListControl.InsertColumn(COL_NAME, LoadString(IDS_TREECOL_NAME), LVCFMT_LEFT, 200, COL_NAME);
        m_treeListControl.InsertColumn(COL_SUBTREEPERCENTAGE, LoadString(IDS_TREECOL_SUBTREEPERCENTAGE), LVCFMT_RIGHT, CItem::GetSubtreePercentageWidth(), COL_SUBTREEPERCENTAGE);
        m_treeListControl.InsertColumn(COL_PERCENTAGE, LoadString(IDS_TREECOL_PERCENTAGE), LVCFMT_RIGHT, 55, COL_PERCENTAGE);
        m_treeListControl.InsertColumn(COL_SUBTREETOTAL, LoadString(IDS_TREECOL_SIZE), LVCFMT_RIGHT, 90, COL_SUBTREETOTAL);
    }

    // reset sort and remove optional
    m_treeListControl.SetSorting(COL_SUBTREETOTAL, m_treeListControl.GetAscendingDefault(COL_SUBTREETOTAL));
    m_treeListControl.SortItems();
    while (m_treeListControl.DeleteColumn(COL_ITEMS));

    // add optional columns based on settings
    if (COptions::ShowColumnItems)
        m_treeListControl.InsertColumn(COL_ITEMS, LoadString(IDS_TREECOL_ITEMS), LVCFMT_RIGHT, 55, COL_ITEMS);
    if (COptions::ShowColumnFiles)
        m_treeListControl.InsertColumn(COL_FILES, LoadString(IDS_TREECOL_FILES), LVCFMT_RIGHT, 55, COL_FILES);
    if (COptions::ShowColumnSubdirs)
        m_treeListControl.InsertColumn(COL_SUBDIRS, LoadString(IDS_TREECOL_SUBDIRS), LVCFMT_RIGHT, 55, COL_SUBDIRS);
    if (COptions::ShowColumnLastChange)
        m_treeListControl.InsertColumn(COL_LASTCHANGE, LoadString(IDS_TREECOL_LASTCHANGE), LVCFMT_LEFT, 120, COL_LASTCHANGE);
    if (COptions::ShowColumnAttributes)
        m_treeListControl.InsertColumn(COL_ATTRIBUTES, LoadString(IDS_TREECOL_ATTRIBUTES), LVCFMT_LEFT, 50, COL_ATTRIBUTES);
    if (COptions::ShowColumnOwner)
        m_treeListControl.InsertColumn(COL_OWNER, LoadString(IDS_TREECOL_OWNER), LVCFMT_LEFT, 120, COL_OWNER);

    m_treeListControl.OnColumnsInserted();
    
}

int CDirStatView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CView::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    constexpr RECT rect = {0, 0, 0, 0};
    VERIFY(m_treeListControl.CreateEx(LVS_EX_HEADERDRAGDROP, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS, rect, this, ID_WDS_CONTROL));

    m_treeListControl.ShowGrid(COptions::ListGrid);
    m_treeListControl.ShowStripes(COptions::ListStripes);
    m_treeListControl.ShowFullRowSelection(COptions::ListFullRowSelection);

    CreateColumns(true);

    m_treeListControl.MySetImageList(GetMyImageList());

    return 0;
}

BOOL CDirStatView::OnEraseBkgnd(CDC* /*pDC*/)
{
    return true;
}

void CDirStatView::OnDestroy()
{
    m_treeListControl.MySetImageList(nullptr);
    CView::OnDestroy();
}

void CDirStatView::OnSetFocus(CWnd* /*pOldWnd*/)
{
    m_treeListControl.SetFocus();
}

void CDirStatView::OnSettingChange(UINT uFlags, LPCWSTR lpszSection)
{
    if (uFlags & SPI_SETNONCLIENTMETRICS)
    {
        FileIconInit(TRUE);
    }
    CView::OnSettingChange(uFlags, lpszSection);
}
void CDirStatView::OnLvnItemchanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    auto pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

    // only process state changes
    if ((pNMLV->uChanged & LVIF_STATE) == 0)
    {
        return;
    }
  
    // Signal to listeners that selection has changed
    GetDocument()->UpdateAllViews(this, HINT_SELECTIONREFRESH);
     
    *pResult = FALSE;
}

void CDirStatView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
    ASSERT(AfxGetThread() != nullptr);

    switch (lHint)
    {
    case HINT_NEWROOT:
        {
            m_treeListControl.SetRootItem(GetDocument()->GetRootItem());
            m_treeListControl.Sort();
            m_treeListControl.Invalidate();
        }
        break;

    case HINT_SELECTIONACTION:
        {
            m_treeListControl.EmulateInteractiveSelection(reinterpret_cast<const CItem*>(pHint));
        }
        break;

    case HINT_ZOOMCHANGED:
        {
            CView::OnUpdate(pSender, lHint, pHint);
        }
        break;

    case HINT_LISTSTYLECHANGED:
        {
            m_treeListControl.ShowGrid(COptions::ListGrid);
            m_treeListControl.ShowStripes(COptions::ListStripes);
            m_treeListControl.ShowFullRowSelection(COptions::ListFullRowSelection);
        }
        break;

    case HINT_NULL:
        {
            m_treeListControl.Sort();
            CView::OnUpdate(pSender, lHint, pHint);
        }
        break;

    default:
        break;
    }
}

void CDirStatView::OnUpdatePopupToggle(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_treeListControl.SelectedItemCanToggle());
}

void CDirStatView::OnPopupToggle()
{
    m_treeListControl.ToggleSelectedItem();
}
