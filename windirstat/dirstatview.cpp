// dirstatview.cpp : Implementation of CDirstatView
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003 Bernhard Seifert
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
// Author: bseifert@users.sourceforge.net, bseifert@daccord.net

#include "stdafx.h"
#include "windirstat.h"
#include "dirstatdoc.h"
#include "item.h"
#include "mainframe.h"
#include ".\dirstatview.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	const UINT _nIdTreeListControl = 4711;
}

CMyTreeListControl::CMyTreeListControl()
: CTreeListControl(20)
{
}

bool CMyTreeListControl::GetAscendingDefault(int column)
{
	return (column == COL_NAME || column == COL_LASTCHANGE);
}

BEGIN_MESSAGE_MAP(CMyTreeListControl, CTreeListControl)
	ON_WM_CONTEXTMENU()
END_MESSAGE_MAP()


void CMyTreeListControl::OnContextMenu(CWnd* /*pWnd*/, CPoint ptscreen)
{
	CPoint point= ptscreen;
	ScreenToClient(&point);

	LVHITTESTINFO hti;
	ZeroMemory(&hti, sizeof(hti));
	hti.pt= point;

	int i= HitTest(&hti);
	if (i == -1)
		return;

	if (hti.iSubItem != 0)
		return;

	CTreeListItem *item= GetItem(i);

	CRect rc= GetWholeSubitemRect(i, 0);
	CRect rcTitle= item->GetTitleRect() + rc.TopLeft();
	if (!rcTitle.PtInRect(point))
		return;

	COwnerDrawnListControl::OnLButtonDown(0, point);

	CPoint ptmenu(rcTitle.right, rcTitle.top + rcTitle.Height() / 2);
	ClientToScreen(&ptmenu);

	CMenu menu;
	menu.LoadMenu(IDR_POPUPLIST);
	CMenu *sub= menu.GetSubMenu(0);

	GetMainFrame()->AppendUserDefinedCleanups(sub);

	sub->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, ptmenu.x, ptmenu.y, AfxGetMainWnd());
}


IMPLEMENT_DYNCREATE(CDirstatView, CView)

CDirstatView::CDirstatView()
{
	m_treeListControl.SetSorting(COL_SUBTREETOTAL, false);
}

CDirstatView::~CDirstatView()
{
}

CString CDirstatView::GenerateReport()
{
	CString report= LoadString(IDS_PLEASECHECKYOURDISKUSAGE);

	CItem *root= GetDocument()->GetSelection();
	ASSERT(root != NULL);
	ASSERT(root->IsVisible());

	int r= m_treeListControl.FindTreeItem(root);
	
	for (
		int i=r; 
			i < m_treeListControl.GetItemCount() 
			&& (i == r || m_treeListControl.GetItem(i)->GetIndent() > root->GetIndent());
		i++
	)
	{
		CItem *item= (CItem *)m_treeListControl.GetItem(i);
		
		if (item->GetType() == IT_MYCOMPUTER)
			continue;

		report.AppendFormat(_T("%s %s\r\n"), PadWidthBlanks(FormatLongLongHuman(item->GetSize()), 11), item->GetReportPath());
	}

	report+= LoadString(IDS_DISKUSAGEREPORTGENERATEDBYWINDIRSTAT);
	report.AppendFormat(_T("http://%s/\r\n"), GetWinDirStatHomepage());

	return report;
}

// Just a shortcut for CMainFrame to obtain
// the small font for the suspend button.
CFont *CDirstatView::GetSmallFont() 
{ 
	return m_treeListControl.GetFont(); 
}
BOOL CDirstatView::PreCreateWindow(CREATESTRUCT& cs)
{
	return CView::PreCreateWindow(cs);
}

void CDirstatView::OnInitialUpdate()
{
	CView::OnInitialUpdate();
}
void CDirstatView::OnDraw(CDC* pDC)
{
	CView::OnDraw(pDC);
}

#ifdef _DEBUG
CDirstatDoc* CDirstatView::GetDocument() const // Non debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CDirstatDoc)));
	return (CDirstatDoc*)m_pDocument;
}
#endif

BEGIN_MESSAGE_MAP(CDirstatView, CView)
	ON_WM_SIZE()
	ON_WM_CREATE()
	ON_WM_ERASEBKGND()
	ON_WM_DESTROY()
	ON_WM_SETFOCUS()
	ON_NOTIFY(LVN_ITEMCHANGED, _nIdTreeListControl, OnLvnItemchanged)
END_MESSAGE_MAP()

void CDirstatView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	if (IsWindow(m_treeListControl.m_hWnd))
	{
		CRect rc(0, 0, cx, cy);
		m_treeListControl.MoveWindow(rc);
	}
}

int CDirstatView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;

	RECT rect= { 0, 0, 0, 0 };
	VERIFY(m_treeListControl.CreateEx(0, WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SHOWSELALWAYS, rect, this, _nIdTreeListControl));
	m_treeListControl.AddExtendedStyle(LVS_EX_HEADERDRAGDROP);
	if (GetOptions()->IsTreelistGrid())
		m_treeListControl.ShowGrid(true);

	m_treeListControl.InsertColumn(COL_NAME, LoadString(IDS_TREECOL_NAME), LVCFMT_LEFT,	200, COL_NAME);
	m_treeListControl.InsertColumn(COL_SUBTREEPERCENTAGE, LoadString(IDS_TREECOL_SUBTREEPERCENTAGE), LVCFMT_RIGHT, CItem::GetSubtreePercentageWidth(), COL_SUBTREEPERCENTAGE);
	m_treeListControl.InsertColumn(COL_PERCENTAGE, LoadString(IDS_TREECOL_PERCENTAGE), LVCFMT_RIGHT, 55, COL_PERCENTAGE);
	m_treeListControl.InsertColumn(COL_SUBTREETOTAL, LoadString(IDS_TREECOL_SIZE), LVCFMT_RIGHT, 90, COL_SUBTREETOTAL);
	m_treeListControl.InsertColumn(COL_ITEMS, LoadString(IDS_TREECOL_ITEMS), LVCFMT_RIGHT, 55, COL_ITEMS);
	m_treeListControl.InsertColumn(COL_FILES, LoadString(IDS_TREECOL_FILES), LVCFMT_RIGHT, 55, COL_FILES);
	m_treeListControl.InsertColumn(COL_SUBDIRS, LoadString(IDS_TREECOL_SUBDIRS), LVCFMT_RIGHT, 55, COL_SUBDIRS);
	m_treeListControl.InsertColumn(COL_LASTCHANGE, LoadString(IDS_TREECOL_LASTCHANGE), LVCFMT_LEFT, 120, COL_LASTCHANGE);

	m_treeListControl.OnColumnsInserted();

	m_treeListControl.MySetImageList(GetMyImageList());

	return 0;
}

BOOL CDirstatView::OnEraseBkgnd(CDC* /*pDC*/)
{
	return true;
}

void CDirstatView::OnDestroy()
{
	m_treeListControl.MySetImageList(NULL);
	CView::OnDestroy();
}

void CDirstatView::OnSetFocus(CWnd* /*pOldWnd*/)
{
	m_treeListControl.SetFocus();
}

void CDirstatView::OnLvnItemchanged(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

	if ((pNMLV->uChanged & LVIF_STATE) != 0)
	{
		if (pNMLV->iItem == -1)
		{
			ASSERT(false); // mal gucken
		}
		else
		{
			// This is not true (don't know why): ASSERT(m_treeListControl.GetItemState(pNMLV->iItem, LVIS_SELECTED) == pNMLV->uNewState);
			bool selected= ((m_treeListControl.GetItemState(pNMLV->iItem, LVIS_SELECTED) & LVIS_SELECTED) != 0);
			CItem *item= (CItem *)m_treeListControl.GetItem(pNMLV->iItem);
			ASSERT(item != NULL);
			if (selected)
			{
				GetDocument()->SetSelection(item);
				GetDocument()->UpdateAllViews(this, HINT_SELECTIONCHANGED);
			}
		}
	}

	*pResult = 0;
}

void CDirstatView::OnUpdate(CView *pSender, LPARAM lHint, CObject *pHint)
{
	switch (lHint)
	{
	case HINT_NEWROOT:
		m_treeListControl.SetRootItem(GetDocument()->GetRootItem());
		m_treeListControl.Sort();
		m_treeListControl.RedrawItems(0, m_treeListControl.GetItemCount() - 1);
		break;

	case HINT_SELECTIONCHANGED:
		m_treeListControl.SelectAndShowItem(GetDocument()->GetSelection(), false);
		break;

	case HINT_SHOWNEWSELECTION:
		m_treeListControl.SelectAndShowItem(GetDocument()->GetSelection(), true);
		break;

	case HINT_REDRAWWINDOW:
		m_treeListControl.RedrawWindow();
		break;

	case HINT_ZOOMCHANGED:
		CView::OnUpdate(pSender, lHint, pHint);
		break;

	case HINT_TREELISTSTYLECHANGED:
		m_treeListControl.ShowGrid(GetOptions()->IsTreelistGrid());
		break;

	case 0:
		m_treeListControl.Sort();
		m_treeListControl.EnsureItemVisible(GetDocument()->GetSelection());
		m_treeListControl.RedrawItems(0, m_treeListControl.GetItemCount() - 1);
		break;

	default:
		break;
	}
}

// CDirstatView Diagnostics

#ifdef _DEBUG
void CDirstatView::AssertValid() const
{
	CView::AssertValid();
}

void CDirstatView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

#endif //_DEBUG



