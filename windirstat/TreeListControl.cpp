// TreeListControl.cpp	- Implementation of CTreeListItem and CTreeListControl
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
#include ".\treelistcontrol.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
	// Sequence within IDB_NODES
	enum 
	{
		NODE_PLUS_SIBLING,
		NODE_PLUS_END,
		NODE_MINUS_SIBLING,
		NODE_MINUS_END,
		NODE_SIBLING,
		NODE_END,
		NODE_LINE
	};
	const UINT NODE_WIDTH = 15;		// Width of a node within IDB_NODES 
	const UINT NODE_HEIGHT = 24;	// Height of IDB_NODES
	const UINT INDENT_WIDTH = 18;

	const UINT HOTNODE_CX = 9;		// Size and position of the +/- buttons
	const UINT HOTNODE_CY = 9;
	const UINT HOTNODE_X = 0;

}

CTreeListItem::CTreeListItem() 
{ 
	m_parent= NULL; 
	m_vi= NULL;
}

CTreeListItem::~CTreeListItem() 
{ 
	delete m_vi;
}

bool CTreeListItem::DrawSubitem(int subitem, CDC *pdc, CRect rc, UINT state, int *width, int *focusLeft) const
{
	if (subitem != 0)
		return false; 

	CRect rcNode= rc;
	CRect rcPlusMinus;
	GetTreeListControl()->DrawNode(pdc, rcNode, rcPlusMinus, this, width);

	CRect rcLabel= rc;
	rcLabel.left= rcNode.right;
	DrawLabel(GetTreeListControl(), GetMyImageList(), pdc, rcLabel, state, width, focusLeft, false);

	if (width != NULL)
	{
		*width= rcLabel.right - rc.left;
	}
	else
	{
		SetPlusMinusRect(rcPlusMinus - rc.TopLeft());
		SetTitleRect(rcLabel - rc.TopLeft());
	}

	return true;
}

CString CTreeListItem::GetText(int /*subitem*/) const
{ 
	return _T("test"); 
}

int CTreeListItem::GetImage() const
{
	ASSERT(IsVisible());
	if (m_vi->image == -1)
		m_vi->image= GetImageToCache();
	return m_vi->image;
}

void CTreeListItem::DrawPacman(CDC *pdc, const CRect& rc, COLORREF bgColor) const
{
	ASSERT(IsVisible());
	m_vi->pacman.SetBackgroundColor(bgColor);
	m_vi->pacman.Draw(pdc, rc);
}

void CTreeListItem::StartPacman(bool start)
{
	if (IsVisible())
		m_vi->pacman.Start(start);
}

bool CTreeListItem::DrivePacman(LONGLONG readJobs)
{
	if (!IsVisible())
		return false;

	return m_vi->pacman.Drive(readJobs);
}

int CTreeListItem::GetScrollPosition()
{
	return GetTreeListControl()->GetItemScrollPosition(this);
}

void CTreeListItem::SetScrollPosition(int top)
{
	GetTreeListControl()->SetItemScrollPosition(this, top);
}

void CTreeListItem::UncacheImage()
{
	if (IsVisible())
		m_vi->image= -1;
}

void CTreeListItem::SortChildren()
{
	ASSERT(IsVisible());
	m_vi->sortedChildren.SetSize(GetChildrenCount());
	for (int i=0; i < GetChildrenCount(); i++)
		m_vi->sortedChildren[i]= GetTreeListChild(i);

	qsort(m_vi->sortedChildren.GetData(), m_vi->sortedChildren.GetSize(), sizeof(CTreeListItem *), &_compareProc);
}

int __cdecl CTreeListItem::_compareProc(const void *p1, const void *p2)
{
	CTreeListItem *item1= *(CTreeListItem **)p1;
	CTreeListItem *item2= *(CTreeListItem **)p2;
	return item1->CompareS(item2, GetTreeListControl()->GetSorting());
}

CTreeListItem *CTreeListItem::GetSortedChild(int i)
{
	return m_vi->sortedChildren[i];
}

int CTreeListItem::Compare(const CSortingListItem *baseOther, int subitem) const
{
	CTreeListItem *other= (CTreeListItem *)baseOther;

	if (other == this)
		return 0;

	if (m_parent == other->m_parent)
		return CompareSibling(other, subitem);

	if (m_parent == NULL)
		return -2;
	
	if (other->m_parent == NULL)
		return 2;

	if (GetIndent() < other->GetIndent())
        return Compare(other->m_parent, subitem);
	else if (GetIndent() > other->GetIndent())
		return m_parent->Compare(other, subitem);
	else
		return m_parent->Compare(other->m_parent, subitem);
}

int CTreeListItem::FindSortedChild(const CTreeListItem *child) 
{ 
	for (int i=0; i < GetChildrenCount(); i++) 
		if (child == GetSortedChild(i)) 
			return i; 
	ASSERT(0); 
	return 0; 
}
CTreeListItem *CTreeListItem::GetParent() const
{ 
	return m_parent; 
}
void CTreeListItem::SetParent(CTreeListItem *parent) 
{ 
	m_parent= parent; 
}
bool CTreeListItem::HasSiblings() const
{ 
	if (m_parent == NULL) 
		return false; 
	int i= m_parent->FindSortedChild(this); 
	return i < m_parent->GetChildrenCount() - 1; 
}
bool CTreeListItem::HasChildren() const
{ 
	return GetChildrenCount() > 0;
}
bool CTreeListItem::IsExpanded() const
{
	ASSERT(IsVisible());
	return m_vi->isExpanded; 
}
void CTreeListItem::SetExpanded(bool expanded) 
{ 
	ASSERT(IsVisible());
	m_vi->isExpanded= expanded; 
}
bool CTreeListItem::IsVisible() const
{
	return (m_vi != NULL);
}
void CTreeListItem::SetVisible(bool visible) 
{ 
	if (visible)
	{
		ASSERT(!IsVisible());
		m_vi= new VISIBLEINFO;
		if (GetParent() == NULL)
			m_vi->indent= 0;
		else
			m_vi->indent= GetParent()->GetIndent() + 1;
		m_vi->image= -1;
		m_vi->isExpanded= false;
	}
	else
	{
		ASSERT(IsVisible());
		delete m_vi;
		m_vi= NULL;
	}
}
int CTreeListItem::GetIndent() const
{
	ASSERT(IsVisible());
	return m_vi->indent;
}
CRect CTreeListItem::GetPlusMinusRect() const
{ 
	ASSERT(IsVisible());
	return m_vi->rcPlusMinus; 
}
void CTreeListItem::SetPlusMinusRect(const CRect& rc) const
{ 
	ASSERT(IsVisible());
	m_vi->rcPlusMinus= rc; 
}
CRect CTreeListItem::GetTitleRect() const
{ 
	ASSERT(IsVisible());
	return m_vi->rcTitle; 
}
void CTreeListItem::SetTitleRect(const CRect& rc) const
{
	ASSERT(IsVisible());
	m_vi->rcTitle= rc; 
}

CTreeListControl *CTreeListItem::GetTreeListControl()
{
	// As we only have 1 TreeListControl and want to economize memory
	// we simple made the TreeListControl global.
	return CTreeListControl::GetTheTreeListControl();
}


/////////////////////////////////////////////////////////////////////////////
// CTreeListControl

CTreeListControl *CTreeListControl::_theTreeListControl;

CTreeListControl *CTreeListControl::GetTheTreeListControl()
{
	ASSERT(_theTreeListControl != NULL);
	return _theTreeListControl;
}


IMPLEMENT_DYNAMIC(CTreeListControl, COwnerDrawnListControl)

CTreeListControl::CTreeListControl(int rowHeight)
: COwnerDrawnListControl(_T("treelist"), rowHeight)
{
	ASSERT(_theTreeListControl == NULL);
	_theTreeListControl= this;

	ASSERT(rowHeight <= NODE_HEIGHT);	// größer können wir nicht
	ASSERT(rowHeight % 2 == 0);			// muss gerade sein
}

CTreeListControl::~CTreeListControl()
{
}

bool CTreeListControl::HasImages()
{
	return true;
}
void CTreeListControl::MySetImageList(CImageList *il)
{
	m_imageList= il;
}

void CTreeListControl::SelectItem(int i)
{
	SetItemState(i, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED);
	EnsureVisible(i, false);
}

int CTreeListControl::GetSelectedItem()
{
	POSITION pos= GetFirstSelectedItemPosition();
	if (pos == NULL)
		return -1;
	return GetNextSelectedItem(pos);
}

void CTreeListControl::SelectItem(const CTreeListItem *item)
{
	int i= FindTreeItem(item);
	if (i != -1)
		SelectItem(i);
}

BOOL CTreeListControl::CreateEx(DWORD dwExStyle, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	InitializeNodeBitmaps();

	dwStyle|= LVS_OWNERDRAWFIXED | LVS_SINGLESEL;
	VERIFY(COwnerDrawnListControl::CreateEx(dwExStyle, dwStyle, rect, pParentWnd, nID));
	return true;
}

void CTreeListControl::SysColorChanged()
{
	COwnerDrawnListControl::SysColorChanged();
	InitializeNodeBitmaps();
}

CTreeListItem *CTreeListControl::GetItem(int i)
{
	CTreeListItem *item= (CTreeListItem *)GetItemData(i);
	return item;
}

void CTreeListControl::SetRootItem(CTreeListItem *root)
{
	DeleteAllItems();

	if (root != NULL)
	{
		InsertItem(0, root);
		ExpandItem(0);
	}
}

void CTreeListControl::DeselectAll()
{
	for (int i=0; i < GetItemCount(); i++)
		SetItemState(i, 0, LVIS_SELECTED);
}

void CTreeListControl::SelectAndShowItem(const CTreeListItem *item, bool showWholePath)
{
	if (item == NULL)
		return;

	CArray<const CTreeListItem *, const CTreeListItem *> path;
	const CTreeListItem *p= item;
	while (p != NULL)
	{
		path.Add(p);
		p= p->GetParent();
	}

	int parent= 0;
	for (int i=path.GetSize() - 1; i >= 0; i--)
	{
		int index= FindTreeItem(path[i]);
		if (index == -1)
		{
			ASSERT(i < path.GetSize() - 1);
			ExpandItem(parent, false);
			index= FindTreeItem(path[i]);
		}
		else
		{
			for (int k=parent+1; k < index; k++)
			{
				CollapseItem(k);
				index= FindTreeItem(path[i]);
			}
		}
		parent= index;
	}

	i= FindTreeItem(path[0]); 
	
	int w= GetSubItemWidth(GetItem(i), 0) + 5;
	if (GetColumnWidth(0) < w)
		SetColumnWidth(0, w);

	if (showWholePath)
		EnsureVisible(0, false);

	SelectItem(i);
}

void CTreeListControl::OnItemDoubleClick(int i)
{
	ToggleExpansion(i);
}

void CTreeListControl::InitializeNodeBitmaps()
{
	m_bmNodes0.DeleteObject();
	m_bmNodes1.DeleteObject();

	COLORMAP cm[1] = {	{ RGB(255,0,255), 0 } };
	
	cm[0].to= GetWindowColor();
	VERIFY(m_bmNodes0.LoadMappedBitmap(IDB_NODES, 0, cm, 1));
	cm[0].to= GetStripeColor();
	VERIFY(m_bmNodes1.LoadMappedBitmap(IDB_NODES, 0, cm, 1));
}

void CTreeListControl::InsertItem(int i, CTreeListItem *item)
{
	COwnerDrawnListControl::InsertListItem(i, item);
	item->SetVisible(true);
}

void CTreeListControl::DeleteItem(int i)
{
	GetItem(i)->SetExpanded(false);
	GetItem(i)->SetVisible(false);
	COwnerDrawnListControl::DeleteItem(i);
}

int CTreeListControl::FindTreeItem(const CTreeListItem *item)
{
	return COwnerDrawnListControl::FindListItem(item);
}

BEGIN_MESSAGE_MAP(CTreeListControl, COwnerDrawnListControl)
	ON_WM_MEASUREITEM_REFLECT()
	ON_WM_LBUTTONDOWN()
	ON_WM_KEYDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_DESTROY()
END_MESSAGE_MAP()


void CTreeListControl::DrawNode(CDC *pdc, CRect& rc, CRect& rcPlusMinus, const CTreeListItem *item, int *width)
{
	CRect rcRest= rc;
	rcRest.left+= GetGeneralLeftIndent();
	if (item->GetIndent() > 0)
	{
		rcRest.left+= 3;

		CDC dcmem;
		dcmem.CreateCompatibleDC(pdc);
		CSelectObject sonodes(&dcmem, (IsItemStripeColor(item) ? &m_bmNodes1 : &m_bmNodes0));

		int ysrc= NODE_HEIGHT / 2 - GetRowHeight() / 2;

		if (width == NULL)
		{
			const CTreeListItem *ancestor= item;
			for (int indent=item->GetIndent() - 2; indent >= 0; indent--)
			{
				ancestor= ancestor->GetParent();
				if (ancestor->HasSiblings())
				{
					pdc->BitBlt(rcRest.left + indent * INDENT_WIDTH, rcRest.top, NODE_WIDTH, NODE_HEIGHT, &dcmem, NODE_WIDTH * NODE_LINE, ysrc, SRCCOPY);
				}
			}
		}

		rcRest.left+= (item->GetIndent() - 1) * INDENT_WIDTH;

		if (width == NULL)
		{
			int node;
			if (item->HasChildren())
				if (item->HasSiblings())
					if (item->IsExpanded())
						node= NODE_MINUS_SIBLING;
					else
						node= NODE_PLUS_SIBLING;
				else
					if (item->IsExpanded())
						node= NODE_MINUS_END;
					else
						node= NODE_PLUS_END;
			else
				if (item->HasSiblings())
					node= NODE_SIBLING;
				else
					node= NODE_END;

			pdc->BitBlt(rcRest.left, rcRest.top, NODE_WIDTH, NODE_HEIGHT, &dcmem, NODE_WIDTH * node, ysrc, SRCCOPY);

			rcPlusMinus.left	= rcRest.left + HOTNODE_X;
			rcPlusMinus.right	= rcPlusMinus.left + HOTNODE_CX;
			rcPlusMinus.top		= rcRest.top + rcRest.Height() / 2 - HOTNODE_CY / 2 - 1;
			rcPlusMinus.bottom	= rcPlusMinus.top + HOTNODE_CY;
		}
		rcRest.left+= NODE_WIDTH;
	}

	rc.right= rcRest.left;

	if (width != NULL)
		*width= rc.Width();
}

void CTreeListControl::OnLButtonDown(UINT nFlags, CPoint point)
{
	m_lButtonDownItem= -1;

	LVHITTESTINFO hti;
	ZeroMemory(&hti, sizeof(hti));
	hti.pt= point;

	int i= HitTest(&hti);
	if (i == -1)
		return;

	if (hti.iSubItem != 0)
	{
		COwnerDrawnListControl::OnLButtonDown(nFlags, point);
		return;
	}

	CRect rc= GetWholeSubitemRect(i, 0);
	CPoint pt= point - rc.TopLeft();

	CTreeListItem *item= GetItem(i);

	m_lButtonDownItem= i;

	if (item->GetPlusMinusRect().PtInRect(pt))
	{
		m_lButtonDownOnPlusMinusRect= true;
		ToggleExpansion(i);
	}
	else
	{
		m_lButtonDownOnPlusMinusRect= false;
		COwnerDrawnListControl::OnLButtonDown(nFlags, point);
	}
}

void CTreeListControl::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	COwnerDrawnListControl::OnLButtonDblClk(nFlags, point);

	if (m_lButtonDownItem == -1)
		return;

	if (m_lButtonDownOnPlusMinusRect)
	{
		ToggleExpansion(m_lButtonDownItem);
	}
	else
	{
		OnItemDoubleClick(m_lButtonDownItem);
	}
}

void CTreeListControl::ToggleExpansion(int i)
{
	if (GetItem(i)->IsExpanded())
		CollapseItem(i);
	else
		ExpandItem(i);
}

void CTreeListControl::CollapseItem(int i)
{
	CTreeListItem *item= GetItem(i);
	if (!item->IsExpanded())
		return;

	CWaitCursor wc;
	LockWindowUpdate();
	bool selectNode= false;
	int todelete= 0;
	for (int k=i+1; k < GetItemCount(); k++)
	{
		CTreeListItem *child= GetItem(k);
		if (child->GetIndent() <= item->GetIndent())
			break;
		if (GetItemState(k, LVIS_SELECTED) == LVIS_SELECTED)
			selectNode= true;
		todelete++;
	}
	for (int m=0; m < todelete; m++)
		DeleteItem(i + 1);
	item->SetExpanded(false);
	if (selectNode)
		SelectItem(i);
	UnlockWindowUpdate();
	RedrawItems(i, i);
}

int CTreeListControl::GetItemScrollPosition(CTreeListItem *item)
{
	CRect rc;
	VERIFY(GetItemRect(FindTreeItem(item), rc, LVIR_BOUNDS));
	return rc.top;
}

void CTreeListControl::SetItemScrollPosition(CTreeListItem *item, int top)
{
	int old= GetItemScrollPosition(item);
	Scroll(CSize(0, top - old));
}

bool CTreeListControl::SelectedItemCanToggle()
{
	int i= GetSelectedItem();
	if (i == -1)
		return false;

	const CTreeListItem *item= GetItem(i);

	return item->HasChildren();
}

void CTreeListControl::ToggleSelectedItem()
{
	int i= GetSelectedItem();
	ASSERT(i != -1);
	ToggleExpansion(i);
}

void CTreeListControl::ExpandItem(CTreeListItem *item)
{
	ExpandItem(FindTreeItem(item), false);
}

void CTreeListControl::ExpandItem(int i, bool scroll)
{
	CTreeListItem *item= GetItem(i);
	if (item->IsExpanded())
		return;

	CWaitCursor wc; // TODO: smart WaitCursor. In CollapseItem(), too.
	LockWindowUpdate();

	item->SortChildren();

	int maxwidth= GetSubItemWidth(item, 0);
	for (int c=0; c < item->GetChildrenCount(); c++)
	{
		CTreeListItem *child= item->GetSortedChild(c);
		InsertItem(i + 1 + c, child);
		if (scroll)
		{
			int w= GetSubItemWidth(child, 0);
			if (w > maxwidth)
				maxwidth= w;
		}
	}

	if (scroll && GetColumnWidth(0) < maxwidth)
		SetColumnWidth(0, maxwidth);

	item->SetExpanded(true);
	UnlockWindowUpdate();
	RedrawItems(i, i);

	if (scroll)
	{
#if 0
		EnsureVisible(i, false);
#elif 1
		// Scroll up so far, that i is still visible
		// and the first child becomes visible, if possible.
		if (item->GetChildrenCount() > 0)
			EnsureVisible(i + 1, false);
		EnsureVisible(i, false);
#elif 0
		// Scroll up so far, that i is still visible
		// and the last child becomes visible, if possible.

		CRect rcClient;
		GetClientRect(rcClient);

		CRect rcLastChild;
		VERIFY(GetItemRect(i + item->GetChildrenCount(), rcLastChild, LVIR_BOUNDS));
		
		int cy= rcLastChild.bottom - rcClient.bottom;
		if (cy < 0)
			return;

		CRect rcHeader;
		GetHeaderCtrl()->GetWindowRect(rcHeader);
		ScreenToClient(rcHeader);

		CRect rcParent;
		VERIFY(GetItemRect(i, rcParent, LVIR_BOUNDS));

		int cymax= rcParent.top - rcHeader.bottom;
		if (cymax < 0)
			return;

		if (cy > cymax)
			cy= cymax;

		Scroll(CSize(0, cy));
#endif
	}

}

void CTreeListControl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	int i= GetNextItem(-1, LVNI_FOCUSED);
	if (i != -1)
	{
		CTreeListItem *item= GetItem(i);
		switch (nChar)
		{
		case VK_LEFT:
			if (item->IsExpanded())
			{
				CollapseItem(i);
			}
			else if (item->GetParent() != NULL)
			{
				SelectItem(item->GetParent());
			}
			return;

		case VK_RIGHT:
			if (!item->IsExpanded())
			{
				ExpandItem(i);
			}
			else if (item->GetChildrenCount() > 0)
			{
				SelectItem(item->GetSortedChild(0));
			}
			return;
		}
	}
	COwnerDrawnListControl::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CTreeListControl::OnChildAdded(CTreeListItem *parent, CTreeListItem *child)
{
	if (!parent->IsVisible())
		return;

	int p= FindTreeItem(parent);
	ASSERT(p != -1);

	if (parent->IsExpanded())
	{
		InsertItem(p + 1, child);
		RedrawItems(p, p);
		Sort();
	}
	else
	{
		RedrawItems(p, p);
	}
}

void CTreeListControl::OnChildRemoved(CTreeListItem *parent, CTreeListItem *child)
{
	if (!parent->IsVisible())
		return;

	int p= FindTreeItem(parent);
	ASSERT(p != -1);

	if (parent->IsExpanded())
	{
		for (int i=0; i < child->GetChildrenCount(); i++)
		{
			OnChildRemoved(child, child->GetTreeListChild(i));
		}

		int c= FindTreeItem(child);
		ASSERT(c != -1);
		COwnerDrawnListControl::DeleteItem(c);
		parent->SortChildren();
	}

	RedrawItems(p, p);
}

void CTreeListControl::OnRemovingAllChildren(CTreeListItem *parent)
{
	if (!parent->IsVisible())
		return;
	
	int p= FindTreeItem(parent);
	ASSERT(p != -1);

	CollapseItem(p);
}

void CTreeListControl::Sort()
{
	for (int i=0; i < GetItemCount(); i++)
	{
		if (GetItem(i)->IsExpanded())
			GetItem(i)->SortChildren();
	}
	COwnerDrawnListControl::SortItems();
}

void CTreeListControl::EnsureItemVisible(const CTreeListItem *item)
{
	if (item == NULL)
		return;
	int i= FindTreeItem(item);
	if (i == -1)
		return;
	EnsureVisible(i, false);
}

void CTreeListControl::MeasureItem(LPMEASUREITEMSTRUCT mis)
{
	mis->itemHeight= GetRowHeight();
}

