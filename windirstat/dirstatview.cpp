// dirstatview.cpp : Implementation of CDirstatView
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2004 Bernhard Seifert
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

CMyTreeListControl::CMyTreeListControl(CDirstatView *dirstatView)
: CTreeListControl(20)
, m_dirstatView(dirstatView)
{
}

bool CMyTreeListControl::GetAscendingDefault(int column)
{
	return (column == COL_NAME || column == COL_LASTCHANGE);
}

BEGIN_MESSAGE_MAP(CMyTreeListControl, CTreeListControl)
	ON_WM_CONTEXTMENU()
	ON_WM_SETFOCUS()
	ON_WM_KEYDOWN()
END_MESSAGE_MAP()


void CMyTreeListControl::OnContextMenu(CWnd* /*pWnd*/, CPoint /*ptscreen*/)
{
	int i= GetSelectedItem();
	if (i == -1)
		return;

	CTreeListItem *item= GetItem(i);

	CRect rc= GetWholeSubitemRect(i, 0);
	CRect rcTitle= item->GetTitleRect() + rc.TopLeft();

	CPoint ptmenu(rcTitle.right, rcTitle.top + rcTitle.Height() / 2);
	ClientToScreen(&ptmenu);

	CMenu menu;
	menu.LoadMenu(IDR_POPUPLIST);
	CMenu *sub= menu.GetSubMenu(0);

	PrepareDefaultMenu(sub, (CItem *)item);
	GetMainFrame()->AppendUserDefinedCleanups(sub);

	sub->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, ptmenu.x, ptmenu.y, AfxGetMainWnd());
}

void CMyTreeListControl::OnItemDoubleClick(int i)
{
	const CItem *item= (const CItem *)GetItem(i);
	if (item->GetType() == IT_FILE)
	{
		GetDocument()->OpenItem(item);
	}
	else
	{
		CTreeListControl::OnItemDoubleClick(i);
	}
}

void CMyTreeListControl::PrepareDefaultMenu(CMenu *menu, const CItem *item)
{
	if (IsLeaf(item->GetType()))
	{
		menu->DeleteMenu(0, MF_BYPOSITION);	// Remove "Expand/Collapse" item
		menu->DeleteMenu(0, MF_BYPOSITION);	// Remove separator
		menu->SetDefaultItem(ID_CLEANUP_OPEN, false);
	}
	else
	{
		CString command = LoadString(item->IsExpanded() && item->HasChildren() ? IDS_COLLAPSE : IDS_EXPAND);
		VERIFY(menu->ModifyMenu(ID_POPUP_TOGGLE, MF_BYCOMMAND|MF_STRING, ID_POPUP_TOGGLE, command));
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

IMPLEMENT_DYNCREATE(CDirstatView, CView)

CDirstatView::CDirstatView()
: m_treeListControl(this)
{
	m_treeListControl.SetSorting(COL_SUBTREETOTAL, false);
}

CDirstatView::~CDirstatView()
{
}

CString CDirstatView::GenerateReport()
{
	CString report= GetOptions()->GetReportPrefix() + _T("\r\n");

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

	report+= _T("\r\n\r\n");
	report+= GetOptions()->GetReportSuffix();

	return report;
}

// Just a shortcut for CMainFrame to obtain
// the small font for the suspend button.
CFont *CDirstatView::GetSmallFont() 
{ 
	return m_treeListControl.GetFont(); 
}

void CDirstatView::SysColorChanged()
{
	m_treeListControl.SysColorChanged();
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
	ON_UPDATE_COMMAND_UI(ID_POPUP_TOGGLE, OnUpdatePopupToggle)
	ON_COMMAND(ID_POPUP_TOGGLE, OnPopupToggle)
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

	m_treeListControl.ShowGrid(GetOptions()->IsListGrid());
	m_treeListControl.ShowStripes(GetOptions()->IsListStripes());

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

	case HINT_LISTSTYLECHANGED:
		m_treeListControl.ShowGrid(GetOptions()->IsListGrid());
		m_treeListControl.ShowStripes(GetOptions()->IsListStripes());
		break;

	case HINT_SOMEWORKDONE:
		{
			MSG msg;
			while (PeekMessage(&msg, m_treeListControl, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE))
			{
				if (msg.message == WM_QUIT)
				{
					PostQuitMessage(msg.wParam);
					break;
				}
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		// fall thru
	case 0:
		m_treeListControl.Sort();
		
		// I decided (from 1.0.1 to 1.0.2) that this is not so good:
		// m_treeListControl.EnsureItemVisible(GetDocument()->GetSelection());

		CView::OnUpdate(pSender, lHint, pHint);
		break;

	default:
		break;
	}
}

void CDirstatView::OnUpdatePopupToggle(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(m_treeListControl.SelectedItemCanToggle());
}

void CDirstatView::OnPopupToggle()
{
	m_treeListControl.ToggleSelectedItem();
}



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



