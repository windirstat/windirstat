// PageColors.cpp : implementation file
//

#include "stdafx.h"
#include "bdirstat.h"
#include "dirstatdoc.h"
#include "PageColors.h"
#include ".\pagecolors.h"


/////////////////////////////////////////////////////////////////////////////

CExtensionListControl::CListItem::CListItem(LPCSTR extension)
{
	m_extension= extension;
	m_extension.MakeLower();
	m_image= -1;
}

CString CExtensionListControl::CListItem::GetText(int subitem)
{
	switch (subitem)
	{
	case COL_EXTENSION:
		return GetExtension();

	case COL_DESCRIPTION:
		return GetDescription();

	default:
		ASSERT(0);
		return "";
	}
}

CString CExtensionListControl::CListItem::GetExtension()
{
	return m_extension;
}

int CExtensionListControl::CListItem::GetImage()
{
	if (m_image == -1)
	{
		m_image= GetExtensionImageIndexAndDescription(m_extension, m_description);
	}
	return m_image;
}


CString CExtensionListControl::CListItem::GetDescription()
{
	if (m_description.IsEmpty())
	{
		m_image= GetExtensionImageIndexAndDescription(m_extension, m_description);
	}
	return m_description;
}


/////////////////////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CExtensionListControl, CListCtrl)
	ON_WM_DESTROY()
	ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnLvnGetdispinfo)
	ON_NOTIFY_REFLECT(LVN_DELETEITEM, OnLvnDeleteitem)
END_MESSAGE_MAP()

// As we will not receive WM_CREATE, we must do initialization
// in this extra method. The counterpart is OnDestroy().
void CExtensionListControl::Initialize()
{
	SetImageList(CImageList::FromHandle(GetSystemImageList()), LVSIL_SMALL);
	InsertColumn(COL_EXTENSION, "Extension", LVCFMT_LEFT, 70, COL_EXTENSION);
	InsertColumn(COL_DESCRIPTION, "Description", LVCFMT_LEFT, 210, COL_DESCRIPTION);
}

void CExtensionListControl::OnDestroy()
{
	SetImageList(NULL, LVSIL_SMALL);
	
	CListCtrl::OnDestroy();
}

void CExtensionListControl::AddExtensions(const CExtensionSet& extensions, bool select)
{
	POSITION pos= extensions.GetStartPosition();
	while (pos != NULL)
	{
		CString ext;
		extensions.GetNextAssoc(pos, ext);

		CListItem *item= new CListItem(ext);

		LVITEM lvi;
		ZeroMemory(&lvi, sizeof(lvi));
		lvi.mask= LVIF_IMAGE | LVIF_PARAM | LVIF_TEXT | LVIF_STATE;
		lvi.iItem= GetItemCount();
		lvi.state= select ? LVIS_SELECTED : 0;
		lvi.stateMask= LVIS_SELECTED;
		lvi.pszText= LPSTR_TEXTCALLBACK;
		lvi.iImage= I_IMAGECALLBACK;
		lvi.lParam= (LPARAM)item;
		
		InsertItem(&lvi);
	}
}

void CExtensionListControl::GetSelectedIndices(CArray<int, int>& selected)
{
	selected.RemoveAll();
	POSITION pos= GetFirstSelectedItemPosition();
	while (pos != NULL)
	{
		int i= GetNextSelectedItem(pos);
		selected.Add(i);
	}
}

CExtensionListControl::CListItem *CExtensionListControl::GetListItem(int i)
{
	return (CListItem *)GetItemData(i);
}

void CExtensionListControl::OnLvnGetdispinfo(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVDISPINFO *di= reinterpret_cast<NMLVDISPINFO*>(pNMHDR);

	CListItem *item= GetListItem(di->item.iItem);

	if ((di->item.mask & LVIF_IMAGE) != 0)
		di->item.iImage= item->GetImage();
	
	if ((di->item.mask & LVIF_TEXT) != 0)
		lstrcpyn(di->item.pszText, item->GetText(di->item.iSubItem), di->item.cchTextMax);

	*pResult = 0;
}

void CExtensionListControl::OnLvnDeleteitem(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW lv= reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	delete (CListItem *)(lv->lParam);
	*pResult = 0;
}

/////////////////////////////////////////////////////////////////////////////

void CGroupListControl::Initialize()
{
	// TODO: Imagelist
	InsertColumn(0, "Color Group", LVCFMT_LEFT, 280, 0);
}


/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CPageColors, CPropertyPage)

CPageColors::CPageColors()
	: CPropertyPage(CPageColors::IDD)
{
}

CPageColors::~CPageColors()
{
}

void CPageColors::ReadOptions(SOptions *options)
{
	// Copy colorgroup
	m_colorGroup.RemoveAll();
	for (int i=0; i < options->colorGroup.GetSize(); i++)
		m_colorGroup.Add(options->colorGroup[i]);

	// Copy extension map
	m_extension.RemoveAll();
	POSITION pos= options->extension.GetStartPosition();
	while (pos != NULL)
	{
		CString ext;
		int group;
		options->extension.GetNextAssoc(pos, ext, group);
		m_extension.SetAt(ext, group);
	}

	// Include unassigned extensions
	pos= options->extensionColorMap.GetStartPosition();
	while (pos != NULL)
	{
		CString ext;
		COLORREF dummy;
		options->extensionColorMap.GetNextAssoc(pos, ext, dummy);

		int group;
		if (!m_extension.Lookup(ext, group))
			m_extension.SetAt(ext, 0);
	}
}

void CPageColors::WriteOptions(SOptions *options)
{
	// Copy colorgroup
	options->colorGroup.RemoveAll();
	for (int i=0; i < m_colorGroup.GetSize(); i++)
		options->colorGroup.Add(m_colorGroup[i]);

	// Copy extension map
	options->extension.RemoveAll();
	POSITION pos= m_extension.GetStartPosition();
	while (pos != NULL)
	{
		CString ext;
		int group;
		m_extension.GetNextAssoc(pos, ext, group);
		options->extension.SetAt(ext, group);
	}

	// Recreate extensionColorMap
	options->extensionColorMap.RemoveAll();
	pos= m_extension.GetStartPosition();
	while (pos != NULL)
	{
		CString ext;
		int group;
		m_extension.GetNextAssoc(pos, ext, group);

		options->extensionColorMap.SetAt(ext, m_colorGroup[group].color);
	}
}

void CPageColors::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EXTENSIONS, m_extensionList);
	DDX_Control(pDX, IDC_MEMBERS, m_memberList);
	DDX_Control(pDX, IDC_GROUPS, m_groupList);
}


BEGIN_MESSAGE_MAP(CPageColors, CPropertyPage)
	ON_NOTIFY(LVN_BEGINDRAG, IDC_EXTENSIONS, OnLvnBegindrag)
	ON_NOTIFY(LVN_BEGINDRAG, IDC_GROUPS, OnLvnBegindrag)
	ON_NOTIFY(LVN_BEGINDRAG, IDC_MEMBERS, OnLvnBegindrag)
END_MESSAGE_MAP()


// CPageColors message handlers

BOOL CPageColors::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	m_extensionList.Initialize();
	m_groupList.Initialize();
	m_memberList.Initialize();

	CExtensionSet es;
	POSITION pos= m_extension.GetStartPosition();
	while (pos != NULL)
	{
		CString ext;
		int group;
		m_extension.GetNextAssoc(pos, ext, group);
		if (group == 0)
			es.SetKey(ext);
	}

	m_extensionList.AddExtensions(es, false);
	return TRUE;  // return TRUE unless you set the focus to a control
}


// See MFC sample "LISTHDR".
void CPageColors::OnLvnBegindrag(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW lv= reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	*pResult = 0;

	if (lv->hdr.idFrom == IDC_GROUPS)
		return;
	
	CExtensionListControl *source= NULL;
	switch (lv->hdr.idFrom)
	{
	case IDC_EXTENSIONS:	source= &m_extensionList;	break;
	case IDC_MEMBERS:		source= &m_memberList;		break;
	default:				ASSERT(0);					break;
	}

	CRect rcSource;
	source->GetWindowRect(rcSource);
	ScreenToClient(rcSource);

	const itemDrag= lv->iItem;
	CPoint ptAction= lv->ptAction;
	ptAction+= rcSource.TopLeft();

	CPoint ptItem;
	source->GetItemPosition(itemDrag, &ptItem); // view coordinates
	ptItem+= rcSource.TopLeft();

	CPoint ptOrigin;
	source->GetOrigin(&ptOrigin);

	const CPoint ptHotSpot= ptAction - ptItem + ptOrigin;	// relative to drag image

	CArray<int, int> ia;
	source->GetSelectedIndices(ia);
	if (ia.GetSize() == 0)
		return;

	for (int i=0; i < ia.GetSize(); i++)
		if (ia[i] == itemDrag)
			break;
	ASSERT(i < ia.GetSize());
	int tmp= ia[0]; ia[0]= ia[i]; ia[i]= tmp;

	CImageList il;
	il.Create(IDB_DRAGIMAGES, 24, 0, RGB(255,0,255));
	int image= ia.GetSize() > 1 ? 1 : 0;

	CSize szDelta= ptAction - ptItem;
	il.DragShowNolock(true);  // lock updates and show drag image
	il.SetDragCursorImage(image, CPoint(0, 0)); // define the hot spot for the new cursor image
	ptAction-= szDelta;
	il.BeginDrag(image, CPoint(0, 0));
	il.DragEnter(this, ptAction);

	il.DragMove(ptAction);


	//ShowCursor(false);
	SetCapture();

	for (;;)
	{
        MSG msg;
		GetMessage(&msg, *this, 0, 0);
		
		if (msg.message == WM_MOUSEMOVE)
		{
			CPoint point= msg.pt;
			ScreenToClient(&point);		
			il.DragMove(point - szDelta);  // move the image 
		}
		else if (msg.message == WM_LBUTTONUP)
		{
			break;
		}
	}

	il.DragLeave(this);
	il.EndDrag();

	ReleaseCapture();
	//ShowCursor(true);
}


