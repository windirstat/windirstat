// typeview.cpp - Implementation of CExtensionListControl and CTypeView
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
// Last modified: $Date$

#include "stdafx.h"
#include "windirstat.h"
#include "item.h"
#include "mainframe.h"
#include "dirstatdoc.h"
#include "typeview.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


/////////////////////////////////////////////////////////////////////////////

CExtensionListControl::CListItem::CListItem(CExtensionListControl *list, LPCTSTR extension, SExtensionRecord r)
{
	m_list = list;
	m_extension = extension;
	m_record = r;
	m_image = -1;
}

bool CExtensionListControl::CListItem::DrawSubitem(int subitem, CDC *pdc, CRect rc, UINT state, int *width, int *focusLeft) const
{
	if(subitem == COL_EXTENSION)
	{
		DrawLabel(m_list, GetMyImageList(), pdc, rc, state, width, focusLeft);
	}
	else if(subitem == COL_COLOR)
	{
		DrawColor(pdc, rc, state, width);
	}
	else
	{
		return false;
	}

	return true;
}

void CExtensionListControl::CListItem::DrawColor(CDC *pdc, CRect rc, UINT state, int *width) const
{
	if(width != NULL)
	{
		*width = 40;
		return;
	}

	DrawSelection(m_list, pdc, rc, state);

	rc.DeflateRect(2, 3);

	if(rc.right <= rc.left || rc.bottom <= rc.top)
	{
		return;
	}

	CTreemap treemap;
	treemap.DrawColorPreview(pdc, rc, m_record.color, GetOptions()->GetTreemapOptions());
}

CString CExtensionListControl::CListItem::GetText(int subitem) const
{
	switch (subitem)
	{
	case COL_EXTENSION:
		{
			return GetExtension();
		}

	case COL_COLOR:
		{
			return TEXT("(color)");
		}

	case COL_BYTES:
		{
			return FormatBytes(m_record.bytes);
		}

	case COL_FILES:
		{
			return FormatCount(m_record.files);
		}

	case COL_DESCRIPTION:
		{
			return GetDescription();
		}

	case COL_BYTESPERCENT:
		{
			return GetBytesPercent();
		}

	default:
		{
			ASSERT(0);
			return strEmpty;
		}
	}
}

CString CExtensionListControl::CListItem::GetExtension() const
{
	return m_extension;
}

int CExtensionListControl::CListItem::GetImage() const
{
	if(m_image == -1)
	{
		m_image = GetMyImageList()->GetExtImageAndDescription(m_extension, m_description);
	}
	return m_image;
}

CString CExtensionListControl::CListItem::GetDescription() const
{
	if(m_description.IsEmpty())
	{
		m_image = GetMyImageList()->GetExtImageAndDescription(m_extension, m_description);
	}
	return m_description;
}

CString CExtensionListControl::CListItem::GetBytesPercent() const
{
	CString s;
	s.Format(TEXT("%s%%"), FormatDouble(GetBytesFraction() * 100));
	return s;
}

double CExtensionListControl::CListItem::GetBytesFraction() const
{
	if(m_list->GetRootSize() == 0)
	{
		return 0;
	}

	return (double)	m_record.bytes / m_list->GetRootSize();
}

int CExtensionListControl::CListItem::Compare(const CSortingListItem *baseOther, int subitem) const
{
	int r = 0;

	const CListItem *other = (const CListItem *)baseOther;

	switch (subitem)
	{
	case COL_EXTENSION:
		{
			r = signum(GetExtension().CompareNoCase(other->GetExtension()));
		}
		break;

	case COL_COLOR:
	case COL_BYTES:
		{
			r = signum(m_record.bytes - other->m_record.bytes);
		}
		break;

	case COL_FILES:
		{
			r = signum(m_record.files - other->m_record.files);
		}
		break;

	case COL_DESCRIPTION:
		{
			r = signum(GetDescription().CompareNoCase(other->GetDescription()));
		}
		break;

	case COL_BYTESPERCENT:
		{
			r = signum(GetBytesFraction() - other->GetBytesFraction());
		}
		break;

	default:
		{
			ASSERT(0);
		}
	}

	return r;
}

/////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CExtensionListControl, COwnerDrawnListControl)
	ON_WM_MEASUREITEM_REFLECT()
	ON_WM_DESTROY()
	ON_NOTIFY_REFLECT(LVN_DELETEITEM, OnLvnDeleteitem)
	ON_WM_SETFOCUS()
	ON_NOTIFY_REFLECT(LVN_ITEMCHANGED, OnLvnItemchanged)
	ON_WM_KEYDOWN()
END_MESSAGE_MAP()

CExtensionListControl::CExtensionListControl(CTypeView *typeView)
	: COwnerDrawnListControl(TEXT("types"), 19)
	, m_typeView(typeView)
{
	m_rootSize = 0;
}

bool CExtensionListControl::GetAscendingDefault(int column)
{
	switch (column)
	{
	case COL_EXTENSION:
	case COL_DESCRIPTION:
		{
			return true;
		}
	case COL_COLOR:
	case COL_BYTES:
	case COL_FILES:
	case COL_BYTESPERCENT:
		{
			return false;
		}
	default:
		{
			ASSERT(0);
			return true;
		}
	}
}

// As we will not receive WM_CREATE, we must do initialization
// in this extra method. The counterpart is OnDestroy().
void CExtensionListControl::Initialize()
{
	SetSorting(COL_BYTES, false);

	InsertColumn(COL_EXTENSION,		LoadString(IDS_EXTCOL_EXTENSION),	LVCFMT_LEFT, 60, COL_EXTENSION);
	InsertColumn(COL_COLOR,			LoadString(IDS_EXTCOL_COLOR),		LVCFMT_LEFT, 40, COL_COLOR);
	InsertColumn(COL_BYTES,			LoadString(IDS_EXTCOL_BYTES),		LVCFMT_RIGHT, 60, COL_BYTES);
	InsertColumn(COL_BYTESPERCENT,	TEXT("% ") + LoadString(IDS_EXTCOL_BYTES), LVCFMT_RIGHT, 50, COL_BYTESPERCENT);
	InsertColumn(COL_FILES,			LoadString(IDS_EXTCOL_FILES),		LVCFMT_RIGHT, 50, COL_FILES);
	InsertColumn(COL_DESCRIPTION,	LoadString(IDS_EXTCOL_DESCRIPTION), LVCFMT_LEFT, 170, COL_DESCRIPTION);

	OnColumnsInserted();

	// We don't use the list control's image list, but attaching an image list
	// to the control ensures a proper line height.
	SetImageList(GetMyImageList(), LVSIL_SMALL);
}

void CExtensionListControl::OnDestroy()
{
	SetImageList(NULL, LVSIL_SMALL);
	COwnerDrawnListControl::OnDestroy();
}

void CExtensionListControl::SetExtensionData(const CExtensionData *ed)
{
	DeleteAllItems();

	int i = 0;
	POSITION pos = ed->GetStartPosition();
	while(pos != NULL)
	{
		CString ext;
		SExtensionRecord r;
		ed->GetNextAssoc(pos, ext, r);

		CListItem *item = new CListItem(this, ext, r);
		InsertListItem(i++, item);
	}

	SortItems();
}

void CExtensionListControl::SetRootSize(ULONGLONG totalBytes)
{
	m_rootSize = totalBytes;
}

ULONGLONG CExtensionListControl::GetRootSize()
{
	return m_rootSize;
}

void CExtensionListControl::SelectExtension(LPCTSTR ext)
{
	int i = 0;
	for(i = 0; i < GetItemCount(); i++)
	{
		if(GetListItem(i)->GetExtension().CompareNoCase(ext) == 0)
		{
			break;
		}
	}
	if(i < GetItemCount())
	{
		SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		EnsureVisible(i, false);
	}
}

CString CExtensionListControl::GetSelectedExtension()
{
	POSITION pos = GetFirstSelectedItemPosition();
	if(pos == NULL)
	{
		return strEmpty;
	}
	else
	{
		int i = GetNextSelectedItem(pos);
		CListItem *item = GetListItem(i);
		return item->GetExtension();
	}
}

CExtensionListControl::CListItem *CExtensionListControl::GetListItem(int i)
{
	return (CListItem *)GetItemData(i);
}

void CExtensionListControl::OnLvnDeleteitem(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW lv = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	delete (CListItem *)(lv->lParam);
	*pResult = 0;
}

void CExtensionListControl::MeasureItem(LPMEASUREITEMSTRUCT mis)
{
	mis->itemHeight = GetRowHeight();
}

void CExtensionListControl::OnSetFocus(CWnd* pOldWnd)
{
	COwnerDrawnListControl::OnSetFocus(pOldWnd);
	GetMainFrame()->SetLogicalFocus(LF_EXTENSIONLIST);
}

void CExtensionListControl::OnLvnItemchanged(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	if((pNMLV->uNewState & LVIS_SELECTED) != 0)
	{
		m_typeView->SetHighlightExtension(GetSelectedExtension());
	}
	*pResult = 0;
}

void CExtensionListControl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if(nChar == VK_TAB)
	{
		GetMainFrame()->MoveFocus(LF_DIRECTORYLIST);
	}
	else if(nChar == VK_ESCAPE)
	{
		GetMainFrame()->MoveFocus(LF_NONE);
	}
	COwnerDrawnListControl::OnKeyDown(nChar, nRepCnt, nFlags);
}

/////////////////////////////////////////////////////////////////////////////

static UINT _nIdExtensionListControl = 4711;


IMPLEMENT_DYNCREATE(CTypeView, CView)

BEGIN_MESSAGE_MAP(CTypeView, CView)
	ON_WM_CREATE()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
END_MESSAGE_MAP()


CTypeView::CTypeView()
	: m_extensionListControl(this)
{
	m_showTypes = true;
}

CTypeView::~CTypeView()
{
}

void CTypeView::SysColorChanged()
{
	m_extensionListControl.SysColorChanged();
}

bool CTypeView::IsShowTypes()
{
	return m_showTypes;
}

void CTypeView::ShowTypes(bool show)
{
	m_showTypes = show;
	OnUpdate(NULL, 0, NULL);
}

void CTypeView::SetHighlightExtension(LPCTSTR ext)
{
	GetDocument()->SetHighlightExtension(ext);
	if(GetFocus() == &m_extensionListControl)
	{
		GetDocument()->UpdateAllViews(this, HINT_EXTENSIONSELECTIONCHANGED);
	}
}

BOOL CTypeView::PreCreateWindow(CREATESTRUCT& cs)
{
	return CView::PreCreateWindow(cs);
}

int CTypeView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if(CView::OnCreate(lpCreateStruct) == -1)
	{
		return -1;
	}

	RECT rect = { 0, 0, 0, 0 };
	VERIFY(m_extensionListControl.Create(LVS_SINGLESEL | LVS_OWNERDRAWFIXED | LVS_SHOWSELALWAYS | WS_CHILD | WS_VISIBLE | LVS_REPORT, rect, this, _nIdExtensionListControl));
	m_extensionListControl.SetExtendedStyle(m_extensionListControl.GetExtendedStyle() | LVS_EX_HEADERDRAGDROP);

	m_extensionListControl.ShowGrid(GetOptions()->IsListGrid());
	m_extensionListControl.ShowStripes(GetOptions()->IsListStripes());
	m_extensionListControl.ShowFullRowSelection(GetOptions()->IsListFullRowSelection());

	m_extensionListControl.Initialize();
	return 0;
}

void CTypeView::OnInitialUpdate()
{
	CView::OnInitialUpdate();
}

void CTypeView::OnUpdate(CView * /*pSender*/, LPARAM lHint, CObject *)
{
	switch (lHint)
	{
	case HINT_NEWROOT:
	case 0:
		if(IsShowTypes() && GetDocument()->IsRootDone())
		{
			m_extensionListControl.SetRootSize(GetDocument()->GetRootSize());
			m_extensionListControl.SetExtensionData(GetDocument()->GetExtensionData());

			// If there is no vertical scroll bar, the header control doen't repaint
			// correctly. Don't know why. But this helps:
			m_extensionListControl.GetHeaderCtrl()->InvalidateRect(NULL);
		}
		else
		{
			m_extensionListControl.DeleteAllItems();
		}
		
		// fall through

	case HINT_SELECTIONCHANGED:
	case HINT_SHOWNEWSELECTION:
		if(IsShowTypes())
		{
			SetSelection();
		}
		break;

	case HINT_ZOOMCHANGED:
		break;

	case HINT_REDRAWWINDOW:
		{
			m_extensionListControl.RedrawWindow();
		}
		break;

	case HINT_TREEMAPSTYLECHANGED:
		{
			InvalidateRect(NULL);
			m_extensionListControl.InvalidateRect(NULL);
			m_extensionListControl.GetHeaderCtrl()->InvalidateRect(NULL);
		}
		break;

	case HINT_LISTSTYLECHANGED:
		{
			m_extensionListControl.ShowGrid(GetOptions()->IsListGrid());
			m_extensionListControl.ShowStripes(GetOptions()->IsListStripes());
			m_extensionListControl.ShowFullRowSelection(GetOptions()->IsListFullRowSelection());
		}
		break;
	default:
		break;
	}
}

void CTypeView::SetSelection()
{
	CItem *item = GetDocument()->GetSelection();
	if(item == NULL || item->GetType() != IT_FILE)
	{
		m_extensionListControl.EnsureVisible(0, false);
	}
	else
	{
		m_extensionListControl.SelectExtension(item->GetExtension());
	}
}

#ifdef _DEBUG
void CTypeView::AssertValid() const
{
	CView::AssertValid();
}

void CTypeView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CDirstatDoc* CTypeView::GetDocument() const // Nicht-Debugversion ist inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CDirstatDoc)));
	return (CDirstatDoc*)m_pDocument;
}
#endif //_DEBUG


void CTypeView::OnDraw(CDC* pDC)
{
	CView::OnDraw(pDC);
}

BOOL CTypeView::OnEraseBkgnd(CDC* pDC)
{
	return CView::OnEraseBkgnd(pDC);
}


void CTypeView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	if(IsWindow(m_extensionListControl.m_hWnd))
	{
		CRect rc(0, 0, cx, cy);
		m_extensionListControl.MoveWindow(rc);
	}
}

void CTypeView::OnSetFocus(CWnd* /*pOldWnd*/)
{
	m_extensionListControl.SetFocus();
}


// $Log$
// Revision 1.14  2006/07/04 20:45:23  assarbad
// - See changelog for the changes of todays previous check-ins as well as this one!
//
// Revision 1.13  2005/04/10 16:49:30  assarbad
// - Some smaller fixes including moving the resource string version into the rc2 files
//
// Revision 1.12  2004/12/31 16:01:42  bseifert
// Bugfixes. See changelog 2004-12-31.
//
// Revision 1.11  2004/11/12 22:14:16  bseifert
// Eliminated CLR_NONE. Minor corrections.
//
// Revision 1.10  2004/11/12 00:47:42  assarbad
// - Fixed the code for coloring of compressed/encrypted items. Now the coloring spans the full row!
//
// Revision 1.9  2004/11/08 00:46:26  assarbad
// - Added feature to distinguish compressed and encrypted files/folders by color as in the Windows 2000/XP explorer.
//   Same rules apply. (Green = encrypted / Blue = compressed)
//
// Revision 1.8  2004/11/05 16:53:08  assarbad
// Added Date and History tag where appropriate.
//
