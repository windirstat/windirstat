// ownerdrawnlistcontrol.h	- Declaration of COwnerDrawnListControl and COwnerDrawnListItem
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

#pragma once

#include "sortinglistcontrol.h"

class COwnerDrawnListItem;
class COwnerDrawnListControl;


//
// COwnerDrawnListItem. An item in a COwnerDrawnListControl.
// Some columns (subitems) may be owner drawn (DrawSubitem() returns true),
// COwnerDrawnListControl draws the texts (GetText()) of all others.
// DrawLabel() draws a standard label (width image, text, seletion and focus rect)
//
class COwnerDrawnListItem: public CSortingListItem
{
public:
	COwnerDrawnListItem();
	virtual ~COwnerDrawnListItem();

	virtual CString GetText(int subitem) const =0;
	virtual bool DrawSubitem(int subitem, CDC *pdc, CRect rc, UINT state, int *width) const =0;
	virtual void DrawAdditionalState(CDC * /*pdc*/, const CRect& /*rcLabel*/) const {}

protected:
	void DrawLabel(COwnerDrawnListControl *list, CImageList *il, CDC *pdc, CRect& rc, UINT state, int *width, bool indent = true) const;
};


//
// COwnerDrawnListControl. Must be report view. Deals with COwnerDrawnListItems.
// Can have a grid or not (own implementation, don't set LVS_EX_GRIDLINES). Flicker-free.
//
class COwnerDrawnListControl: public CSortingListControl
{
	DECLARE_DYNAMIC(COwnerDrawnListControl)
public:
	COwnerDrawnListControl(LPCTSTR name, int rowHeight);
	virtual ~COwnerDrawnListControl();
	void OnColumnsInserted();

	int GetRowHeight();
	void ShowGrid(bool show);

	COwnerDrawnListItem *GetItem(int i);
	int FindListItem(const COwnerDrawnListItem *item);
	int GetTextXMargin();
	int GetGeneralLeftIndent();
	void AdjustColumnWidth(int col);
	CRect GetWholeSubitemRect(int item, int subitem);

protected:
	virtual void DrawItem(LPDRAWITEMSTRUCT pdis);
	int GetSubItemWidth(COwnerDrawnListItem *item, int subitem);
	bool IsColumnRightAligned(int col);

	int m_rowHeight;	// Height of an item
	bool m_showGrid;	// Whether to draw a grid
	int m_yFirstItem;	// Top of a first list item

	DECLARE_MESSAGE_MAP()
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnHdnDividerdblclick(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnHdnItemchanging(NMHDR *pNMHDR, LRESULT *pResult);
};

