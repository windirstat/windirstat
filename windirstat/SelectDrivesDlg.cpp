// SelectDrivesDlg.cpp	- Implementation of CDriveItem, CDrivesList and CSelectDrivesDlg
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
#include ".\selectdrivesdlg.h"

namespace
{
	enum
	{
		COL_NAME,
		COL_TOTAL,
		COL_FREE,
		COL_GRAPH,
		COL_PERCENTUSED,
		COLUMN_COUNT
	};

	const UINT WMU_OK = WM_USER + 100;
}



/////////////////////////////////////////////////////////////////////////////

CDriveItem::CDriveItem(CDrivesList *list, LPCTSTR pszPath)
{
	m_list= list;
	m_path= pszPath;

	m_name= FormatVolumeName(m_path);

	MyGetDiskFreeSpace(m_path, m_totalBytes, m_freeBytes);

	ASSERT(m_freeBytes <= m_totalBytes);
	m_used= 0;
	if (m_totalBytes > 0)
		m_used= (double)(m_totalBytes - m_freeBytes) / m_totalBytes;

	m_isRemote= (DRIVE_REMOTE == GetDriveType(m_path));
}

bool CDriveItem::IsRemote() const
{
	return m_isRemote;
}

int CDriveItem::Compare(const CSortingListItem *baseOther, int subitem) const
{
	const CDriveItem *other= (CDriveItem *)baseOther;

	int r= 0;

	switch (subitem)
	{
	case COL_NAME:		r= GetPath().CompareNoCase(other->GetPath()); break;
	case COL_TOTAL:		r= signum(m_totalBytes - other->m_totalBytes); break;
	case COL_FREE:		r= signum(m_freeBytes - other->m_freeBytes); break;
	case COL_GRAPH:
	case COL_PERCENTUSED:
						r= signum(m_used - other->m_used); break;
	default:
		ASSERT(0);
	}

	return r;
}


int CDriveItem::GetImage() const
{
	return GetMyImageList()->GetFileImage(m_path);
}

bool CDriveItem::DrawSubitem(int subitem, CDC *pdc, CRect rc, UINT state, int *width) const
{
	if (subitem == COL_NAME)
	{
		DrawLabel(m_list, GetMyImageList(), pdc, rc, state, width);
		return true;
	}
	else if (subitem == COL_GRAPH)
	{
		if (width != NULL)
		{		
			*width= 100;
			return true;
		}

		rc.DeflateRect(3, 3);
	
		DrawPercentage(pdc, rc, m_used, RGB(0,0,170));

		return true;
	}
	else
	{
		return false;
	}
}

CString CDriveItem::GetText(int subitem) const
{
	CString s= m_name;

	switch (subitem)
	{
	case COL_NAME:
		s= m_name;
		break;

	case COL_TOTAL:
		s= FormatBytes((LONGLONG)m_totalBytes);
		break;

	case COL_FREE:
		s= FormatBytes((LONGLONG)m_freeBytes);
		break;

	case COL_GRAPH:
		break;

	case COL_PERCENTUSED:
		s= FormatDouble(m_used * 100) + _T("%");
		break;

	default:
		ASSERT(0);
	}
	
	return s;
}

CString CDriveItem::GetPath() const
{
	return m_path;
}

CString CDriveItem::GetDrive() const
{
	return m_path.Left(2);
}


/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CDrivesList, COwnerDrawnListControl)

CDrivesList::CDrivesList()
: COwnerDrawnListControl(_T("drives"), 20)
{
}

CDriveItem *CDrivesList::GetItem(int i)
{
	return (CDriveItem *)GetItemData(i);
}

bool CDrivesList::HasImages()
{
	return true;
}

void CDrivesList::SelectItem(CDriveItem *item)
{
	int i= FindListItem(item);
	SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
}

bool CDrivesList::IsItemSelected(int i)
{
	return (LVIS_SELECTED == GetItemState(i, LVIS_SELECTED));
}

void CDrivesList::SelectAllLocalDrives()
{
	for (int i=0; i < GetItemCount(); i++)
	{
		CDriveItem *item= GetItem(i);
		UINT state= item->IsRemote() ? 0 : LVIS_SELECTED;
		SetItemState(i, LVIS_SELECTED, state);
	}
}

void CDrivesList::OnLButtonDown(UINT /*nFlags*/, CPoint /*point*/)
{
	// We simulate Ctrl-Key-Down here, so that the dialog
	// can be driven with one hand (mouse) only.
	const MSG *msg= GetCurrentMessage();
	DefWindowProc(msg->message, msg->wParam | MK_CONTROL, msg->lParam);
}

void CDrivesList::OnNMDblclk(NMHDR * /*pNMHDR*/, LRESULT *pResult)
{
	*pResult = 0;

	CPoint point= GetCurrentMessage()->pt;
	ScreenToClient(&point);
	int i= HitTest(point);
	if (i == -1)
		return;

	for (int k=0; k < GetItemCount(); k++)
		SetItemState(k, k == i ? LVIS_SELECTED : 0, LVIS_SELECTED);

	GetParent()->SendMessage(WMU_OK);
}

BEGIN_MESSAGE_MAP(CDrivesList, COwnerDrawnListControl)
	ON_WM_LBUTTONDOWN()
	ON_NOTIFY_REFLECT(LVN_DELETEITEM, OnLvnDeleteitem)
	ON_WM_MEASUREITEM_REFLECT()
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnNMDblclk)
END_MESSAGE_MAP()

void CDrivesList::OnLvnDeleteitem(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	delete GetItem(pNMLV->iItem);
	*pResult = 0;
}

void CDrivesList::MeasureItem(LPMEASUREITEMSTRUCT mis)
{
	mis->itemHeight= GetRowHeight();
}


/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(CSelectDrivesDlg, CDialog)

CSelectDrivesDlg::CSelectDrivesDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CSelectDrivesDlg::IDD, pParent)
	, m_layout(this, _T("sddlg"))
{
}

CSelectDrivesDlg::~CSelectDrivesDlg()
{
}

void CSelectDrivesDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_DRIVES, m_list);
	DDX_Radio(pDX, IDC_ALLDRIVES, m_radio);
	DDX_Text(pDX, IDC_FOLDERNAME, m_folderName);
	DDX_Control(pDX, IDOK, m_okButton);
}


BEGIN_MESSAGE_MAP(CSelectDrivesDlg, CDialog)
	ON_BN_CLICKED(IDC_BROWSEFOLDER, OnBnClickedBrowsefolder)
	ON_BN_CLICKED(IDC_AFOLDER, OnBnClickedAfolder)
	ON_BN_CLICKED(IDC_SOMEDRIVES, OnBnClickedSomedrives)
	ON_EN_CHANGE(IDC_FOLDERNAME, OnEnChangeFoldername)
	ON_WM_MEASUREITEM()
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_DRIVES, OnLvnItemchangedDrives)
	ON_BN_CLICKED(IDC_ALLLOCALDRIVES, OnBnClickedAlllocaldrives)
	ON_WM_SIZE()
	ON_WM_GETMINMAXINFO()
	ON_WM_DESTROY()
	ON_MESSAGE(WMU_OK, OnWmuOk)
END_MESSAGE_MAP()


BOOL CSelectDrivesDlg::OnInitDialog()
{
	CWaitCursor wc;

	CDialog::OnInitDialog();

	ModifyStyle(0, WS_CLIPCHILDREN);

	m_layout.AddControl(IDOK,				1, 0, 0, 0);
	m_layout.AddControl(IDCANCEL,			1, 0, 0, 0);
	m_layout.AddControl(IDC_DRIVES,			0, 0, 1, 1);
	m_layout.AddControl(IDC_AFOLDER,		0, 1, 0, 0);
	m_layout.AddControl(IDC_FOLDERNAME,		0, 1, 1, 0);
	m_layout.AddControl(IDC_BROWSEFOLDER,	1, 1, 0, 0);

	m_layout.OnInitDialog(true);

	m_list.ShowGrid(true);

	m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_HEADERDRAGDROP);
	// If we set an ImageList here, OnMeasureItem will have no effect ?!

	m_list.InsertColumn(COL_NAME,		LoadString(IDS_DRIVECOL_NAME),		LVCFMT_LEFT, 120, COL_NAME);
	m_list.InsertColumn(COL_TOTAL,		LoadString(IDS_DRIVECOL_TOTAL),		LVCFMT_RIGHT, 120, COL_TOTAL);
	m_list.InsertColumn(COL_FREE,		LoadString(IDS_DRIVECOL_FREE),		LVCFMT_RIGHT, 120, COL_FREE);
	m_list.InsertColumn(COL_GRAPH,		LoadString(IDS_DRIVECOL_GRAPH),		LVCFMT_RIGHT, 120, COL_GRAPH);
	m_list.InsertColumn(COL_PERCENTUSED,LoadString(IDS_DRIVECOL_PERCENTUSED),LVCFMT_RIGHT, 80, COL_PERCENTUSED);

	m_list.OnColumnsInserted();

	m_folderName= CPersistence::GetSelectDrivesFolder();
	CPersistence::GetSelectDrivesDrives(m_selectedDrives);

	ShowWindow(SW_SHOWNORMAL);
	UpdateWindow();
	BringWindowToTop();
	SetForegroundWindow();

	DWORD drives= GetLogicalDrives();
	int i;
	DWORD mask= 0x00000001;
	for (i=0; i < 32; i++, mask <<= 1)
	{
		if ((drives & mask) != 0)
		{
			CString s;
			s.Format(_T("%c:\\"), i + _T('A'));

			UINT type= GetDriveType(s);
			if (type == DRIVE_UNKNOWN || type == DRIVE_NO_ROOT_DIR)
			{
				continue;
			}

			if (!DriveExists(s))
			{
				continue;
			}

			CDriveItem *item= new CDriveItem(&m_list, s);
			m_list.InsertListItem(m_list.GetItemCount(), item);
			for (int col=0; col < COLUMN_COUNT; col++)
				m_list.AdjustColumnWidth(col);
			m_list.UpdateWindow();

			for (int k=0; k < m_selectedDrives.GetSize(); k++)
			{
				if (item->GetDrive() == m_selectedDrives[k])
				{
					m_list.SelectItem(item);
					break;
				}
			}
		}
	}

	m_list.SortItems();

	m_radio= CPersistence::GetSelectDrivesRadio();
	UpdateData(false);

	switch (m_radio)
	{
	case RADIO_ALLLOCALDRIVES:
	case RADIO_AFOLDER:
		m_okButton.SetFocus();
		break;
	case RADIO_SOMEDRIVES:
		m_list.SetFocus();
		break;
	}

	UpdateButtons();
	return false; // we have set the focus.
}

void CSelectDrivesDlg::OnBnClickedBrowsefolder()
{
	CString sDisplayName;
	BROWSEINFO bi;
	ZeroMemory(&bi, sizeof(bi));

	CString title= LoadString(IDS_SELECTFOLDER);
	bi.hwndOwner= m_hWnd;
	bi.pszDisplayName= sDisplayName.GetBuffer(_MAX_PATH);
	bi.lpszTitle= title;
	bi.ulFlags= BIF_RETURNONLYFSDIRS | BIF_EDITBOX | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;
	
	LPITEMIDLIST pidl= SHBrowseForFolder(&bi);
	sDisplayName.ReleaseBuffer();

	if (pidl != NULL)
	{
		CString sDir;

		LPSHELLFOLDER pshf;
		HRESULT hr= SHGetDesktopFolder(&pshf); 
		ASSERT(SUCCEEDED(hr));
		
		STRRET strret;
		strret.uType= STRRET_CSTR;
		hr= pshf->GetDisplayNameOf(pidl, SHGDN_FORPARSING, &strret);
		ASSERT(SUCCEEDED(hr));
		sDir= MyStrRetToString(pidl, &strret);

		CoTaskMemFree(pidl);
		pshf->Release();

		m_folderName= sDir;
		m_radio= RADIO_AFOLDER;
		UpdateData(false);
		UpdateButtons();
	}
}

void CSelectDrivesDlg::OnOK()
{
	UpdateData();

	m_drives.RemoveAll();
	m_selectedDrives.RemoveAll();
	if (m_radio == RADIO_AFOLDER)
	{
		m_folderName= MyGetFullPathName(m_folderName);
		UpdateData(false);
	}
	else
	{
		for (int i=0; i < m_list.GetItemCount(); i++)
		{
			CDriveItem *item= m_list.GetItem(i);

			if (m_radio == RADIO_ALLLOCALDRIVES && !item->IsRemote() 
			||  m_radio == RADIO_SOMEDRIVES && m_list.IsItemSelected(i))
			{
				m_drives.Add(item->GetDrive());
				if (m_list.IsItemSelected(i))
					m_selectedDrives.Add(item->GetDrive());
			}
		}
	}

	CPersistence::SetSelectDrivesRadio(m_radio);
	CPersistence::SetSelectDrivesFolder(m_folderName);
	CPersistence::SetSelectDrivesDrives(m_selectedDrives);

	CDialog::OnOK();
}

void CSelectDrivesDlg::UpdateButtons()
{
	UpdateData();
	bool enableOk= false;
	switch (m_radio)
	{
	case RADIO_ALLLOCALDRIVES:
		enableOk= true;
		break;
	case RADIO_SOMEDRIVES:
		enableOk= (m_list.GetSelectedCount() > 0);
		break;
	case RADIO_AFOLDER:
		if (!m_folderName.IsEmpty())
		{
			CString pattern= m_folderName;
			if (pattern.Right(1) != _T("\\"))
				pattern+= _T("\\");
			pattern+= _T("*.*");
			CFileFind finder;
			BOOL b= finder.FindFile(pattern);
            enableOk= b;
		}
		break;
	default:
		ASSERT(0);
	}
	m_okButton.EnableWindow(enableOk);
}

void CSelectDrivesDlg::OnBnClickedAfolder()
{
	UpdateButtons();
}

void CSelectDrivesDlg::OnBnClickedSomedrives()
{
	m_list.SetFocus();
	UpdateButtons();
}

void CSelectDrivesDlg::OnEnChangeFoldername()
{
	UpdateButtons();
}

void CSelectDrivesDlg::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT mis)
{
	if (nIDCtl == IDC_DRIVES)
		mis->itemHeight= 20;
	else
		CDialog::OnMeasureItem(nIDCtl, mis);
}

void CSelectDrivesDlg::OnLvnItemchangedDrives(NMHDR * /*pNMHDR*/, LRESULT *pResult)
{
	// unused: LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

	m_radio= RADIO_SOMEDRIVES;

	UpdateData(false);
	UpdateButtons();

	*pResult = 0;
}

void CSelectDrivesDlg::OnBnClickedAlllocaldrives()
{
	UpdateButtons();
}

void CSelectDrivesDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);
	m_layout.OnSize();
}

void CSelectDrivesDlg::OnGetMinMaxInfo(MINMAXINFO* mmi)
{
	m_layout.OnGetMinMaxInfo(mmi);
	CDialog::OnGetMinMaxInfo(mmi);
}

void CSelectDrivesDlg::OnDestroy()
{
	m_layout.OnDestroy();
	CDialog::OnDestroy();
}

LRESULT CSelectDrivesDlg::OnWmuOk(WPARAM, LPARAM)
{
	OnOK();
	return 0;
}
