// ownerdrawnlistcontrol.h - Declaration of COwnerDrawnListControl and COwnerDrawnListItem
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2006 Oliver Schneider (assarbad.net)
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
//
// $Header$

#pragma once

#include "sortinglistcontrol.h"

class COwnerDrawnListItem;
class COwnerDrawnListControl;


//
// COwnerDrawnListItem. An item in a COwnerDrawnListControl.
// Some columns (subitems) may be owner drawn (DrawSubitem() returns true),
// COwnerDrawnListControl draws the texts (GetText()) of all others.
// DrawLabel() draws a standard label (width image, text, selection and focus rect)
//
class COwnerDrawnListItem: public CSortingListItem
{
public:
	COwnerDrawnListItem();
	virtual ~COwnerDrawnListItem();

	// This text is drawn, if DrawSubitem returns false
	virtual CString GetText(int subitem) const = 0;
	// This color is used for the  current item
	virtual COLORREF GetItemTextColor() const { return GetSysColor(COLOR_WINDOWTEXT); }
	
	// Returnvalue is true, if the item draws itself.
	// width != NULL -> only determine width, do not draw.
	// If focus rectangle shall not begin leftmost, set *focusLeft
	// to the left edge of the desired focus rectangle.
	virtual bool DrawSubitem(int subitem, CDC *pdc, CRect rc, UINT state, int *width, int *focusLeft) const =0;

	virtual void DrawAdditionalState(CDC * /*pdc*/, const CRect& /*rcLabel*/) const {}

	void DrawSelection(COwnerDrawnListControl *list, CDC *pdc, CRect rc, UINT state) const;
protected:
	void DrawLabel(COwnerDrawnListControl *list, CImageList *il, CDC *pdc, CRect& rc, UINT state, int *width, int *focusLeft, bool indent = true) const;
	void DrawPercentage(CDC *pdc, CRect rc, double fraction, COLORREF color) const;
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
	virtual void SysColorChanged();

	int GetRowHeight();
	void ShowGrid(bool show);
	void ShowStripes(bool show);
	void ShowFullRowSelection(bool show);
	bool IsFullRowSelection();

	COLORREF GetWindowColor();
	COLORREF GetStripeColor();
	COLORREF GetNonFocusHighlightColor();
	COLORREF GetNonFocusHighlightTextColor();
	COLORREF GetHighlightColor();
	COLORREF GetHighlightTextColor();

	bool IsItemStripeColor(int i);
	bool IsItemStripeColor(const COwnerDrawnListItem *item);
	COLORREF GetItemBackgroundColor(int i);
	COLORREF GetItemBackgroundColor(const COwnerDrawnListItem *item);
	COLORREF GetItemSelectionBackgroundColor(int i);
	COLORREF GetItemSelectionBackgroundColor(const COwnerDrawnListItem *item);
	COLORREF GetItemSelectionTextColor(int i);

	COwnerDrawnListItem *GetItem(int i);
	int FindListItem(const COwnerDrawnListItem *item);
	int GetTextXMargin();
	int GetGeneralLeftIndent();
	void AdjustColumnWidth(int col);
	CRect GetWholeSubitemRect(int item, int subitem);

	bool HasFocus();
	bool IsShowSelectionAlways();

protected:
	void InitializeColors();
	virtual void DrawItem(LPDRAWITEMSTRUCT pdis);
	int GetSubItemWidth(COwnerDrawnListItem *item, int subitem);
	bool IsColumnRightAligned(int col);

	int m_rowHeight;	// Height of an item
	bool m_showGrid;	// Whether to draw a grid
	bool m_showStripes;	// Whether to show stripes
	bool m_showFullRowSelection; // Whether to draw full row selection
	int m_yFirstItem;	// Top of a first list item
	COLORREF m_windowColor;	// The default background color if !m_showStripes
	COLORREF m_stripeColor;	// The stripe color, used for every other item if m_showStripes

	DECLARE_MESSAGE_MAP()
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnHdnDividerdblclick(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnHdnItemchanging(NMHDR *pNMHDR, LRESULT *pResult);
};


// $Log$
// Revision 1.11  2006/07/04 22:49:20  assarbad
// - Replaced CVS keyword "Date" by "Header" in the file headers
//
// Revision 1.10  2006/07/04 20:45:23  assarbad
// - See changelog for the changes of todays previous check-ins as well as this one!
//
// Revision 1.9  2004/11/12 22:14:16  bseifert
// Eliminated CLR_NONE. Minor corrections.
//
// Revision 1.8  2004/11/12 00:47:42  assarbad
// - Fixed the code for coloring of compressed/encrypted items. Now the coloring spans the full row!
//
// Revision 1.7  2004/11/08 00:46:26  assarbad
// - Added feature to distinguish compressed and encrypted files/folders by color as in the Windows 2000/XP explorer.
//   Same rules apply. (Green = encrypted / Blue = compressed)
//
// Revision 1.6  2004/11/07 23:28:14  assarbad
// - Partial implementation for coloring of compressed/encrypted files
//
// Revision 1.5  2004/11/05 16:53:07  assarbad
// Added Date and History tag where appropriate.
//
