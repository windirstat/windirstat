// TreeListControl.h - Declaration of CTreeListItem and CTreeListControl
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2006, 2008 Oliver Schneider (assarbad.net)
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
// Author(s): - bseifert -> bernhard@windirstat.info
//            - assarbad -> oliver@windirstat.info
//
// $Id$

#ifndef __WDS_TREELISTCONTROL_H__
#define __WDS_TREELISTCONTROL_H__
#pragma once

#include "ownerdrawnlistcontrol.h"
#include "pacman.h"

class CTreeListItem;
class CTreeListControl;


//
// CTreeListItem. An item in the CTreeListControl. (CItem is derived from CTreeListItem.)
// In order to save memory, once the item is actually inserted in the List,
// we allocate the VISIBLEINFO structure (m_vi).
// m_vi is freed as soon as the item is removed from the List.
// 
class CTreeListItem: public COwnerDrawnListItem
{
	// Data needed to display the item.
	struct VISIBLEINFO
	{
		int indent;			// 0 for the root item, 1 for its children, and so on.
		int image;			// -1 as long as not needed, >= 0: valid index in MyImageList.
		CRect rcPlusMinus;	// Coordinates of the little +/- rectangle, relative to the upper left corner of the item.
		CRect rcTitle;		// Coordinates of the label, relative to the upper left corner of the item.
		bool isExpanded;	// Whether item is expanded.

		// sortedChildren: This member contains our children (the same set of
		// children as in CItem::m_children) and is initialized as soon as
		// we are expanded. In contrast to CItem::m_children, this array is always
		// sorted depending on the current user-defined sort column and -order.
		CArray<CTreeListItem *, CTreeListItem *> sortedChildren;

		CPacman pacman;
	};

public:
	CTreeListItem();
	virtual ~CTreeListItem();

	virtual int CompareSibling(const CTreeListItem *tlib, int subitem) const =0;

	virtual bool DrawSubitem(int subitem, CDC *pdc, CRect rc, UINT state, int *width, int *focusLeft) const;
	virtual CString GetText(int subitem) const;
	virtual int GetImage() const;
	virtual int Compare(const CSortingListItem *other, int subitem) const;
	virtual CTreeListItem *GetTreeListChild(int i) const =0;
	virtual int GetChildrenCount() const =0;
	virtual int GetImageToCache() const =0;

	void DrawPacman(CDC *pdc, const CRect& rc, COLORREF bgColor) const;
	void UncacheImage();
	void SortChildren();
	CTreeListItem *GetSortedChild(int i);
	int FindSortedChild(const CTreeListItem *child);
	CTreeListItem *GetParent() const;
	void SetParent(CTreeListItem *parent);
	bool HasSiblings() const;
	bool HasChildren() const;
	bool IsExpanded() const;
	void SetExpanded(bool expanded =true);
	bool IsVisible() const;
	void SetVisible(bool visible =true);
	int GetIndent() const;
	CRect GetPlusMinusRect() const;
	void SetPlusMinusRect(const CRect& rc) const;
	CRect GetTitleRect() const;
	void SetTitleRect(const CRect& rc) const;

protected:
	static int __cdecl _compareProc(const void *p1, const void *p2);
	static CTreeListControl *GetTreeListControl();
	void StartPacman(bool start);
	bool DrivePacman(ULONGLONG readJobs);
	int GetScrollPosition();
	void SetScrollPosition(int top);

private:
	CTreeListItem *m_parent;
	mutable VISIBLEINFO *m_vi;
};


//
// CTreeListControl. A CListCtrl, which additionally behaves an looks like a tree control.
//
class CTreeListControl: public COwnerDrawnListControl
{
	DECLARE_DYNAMIC(CTreeListControl)

	// In order to save memory, and as we have only one CTreeListControl in the application,
	// this is global.
	static CTreeListControl *_theTreeListControl;
					
public:
	static CTreeListControl *GetTheTreeListControl();

	CTreeListControl(int rowHeight = -1);
	virtual ~CTreeListControl();
	void MySetImageList(CImageList *il);
	virtual BOOL CreateEx(DWORD dwExStyle, DWORD dwStyle, const RECT& rect,	CWnd* pParentWnd, UINT nID);
	virtual void SysColorChanged();
	void SetRootItem(CTreeListItem *root);
	void OnChildAdded(CTreeListItem *parent, CTreeListItem *child);
	void OnChildRemoved(CTreeListItem *parent, CTreeListItem *childdata);
	void OnRemovingAllChildren(CTreeListItem *parent);
	CTreeListItem *GetItem(int i);
	void DeselectAll();
	void SelectAndShowItem(const CTreeListItem *item, bool showWholePath);
	void DrawNode(CDC *pdc, CRect& rc, CRect& rcPlusMinus, const CTreeListItem *item, int *width);
	void SelectItem(const CTreeListItem *item);
	void Sort();
	void EnsureItemVisible(const CTreeListItem *item);
	void ExpandItem(CTreeListItem *item);
	int FindTreeItem(const CTreeListItem *item);
	int GetItemScrollPosition(CTreeListItem *item);
	void SetItemScrollPosition(CTreeListItem *item, int top);
	bool SelectedItemCanToggle();
	void ToggleSelectedItem();

	virtual bool HasImages();

protected:
	virtual void OnItemDoubleClick(int i);
	void InitializeNodeBitmaps();


	void InsertItem(int i, CTreeListItem *item);
	void DeleteItem(int i);
	void CollapseItem(int i);
	void ExpandItem(int i, bool scroll = true);
	void ToggleExpansion(int i);
	void SelectItem(int i);
	int GetSelectedItem();

	CBitmap m_bmNodes0;			// The bitmaps needed to draw the treecontrol-like branches
	CBitmap m_bmNodes1;			// The same bitmaps with stripe-background color
	CImageList *m_imageList;	// We don't use the system-supplied SetImageList(), but MySetImageList().
	int m_lButtonDownItem;		// Set in OnLButtonDown(). -1 if not item hit.
	bool m_lButtonDownOnPlusMinusRect;	// Set in OnLButtonDown(). True, if plus-minus-rect hit.

	DECLARE_MESSAGE_MAP()

	afx_msg void MeasureItem(LPMEASUREITEMSTRUCT mis);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
};

#endif // __WDS_TREELISTCONTROL_H__
