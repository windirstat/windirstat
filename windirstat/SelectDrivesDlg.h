// SelectDrivesDlg.h	- Declaration of CDriveItem, CDrivesList and CSelectDrivesDlg
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

#include "ownerdrawnlistcontrol.h"
#include "layout.h"

//
// The dialog has these three radio buttons.
//
enum RADIO
{
	RADIO_ALLLOCALDRIVES,
	RADIO_SOMEDRIVES,
	RADIO_AFOLDER
};


class CDrivesList;

//
// CDriveItem. An item in the CDrivesList Control.
//
class CDriveItem: public COwnerDrawnListItem
{
public:
	CDriveItem(CDrivesList *list, LPCTSTR pszPath);
	virtual int Compare(const CSortingListItem *other, int subitem) const;

	CString GetPath() const;
	CString GetDrive() const;
	bool IsRemote() const;
	virtual bool DrawSubitem(int subitem, CDC *pdc, CRect rc, UINT state, int *width) const;
	virtual CString GetText(int subitem) const;
	int GetImage() const;

private:
	CDrivesList *m_list;	// Backpointer
	CString m_path;			// e.g. "C:\"
	CString m_name;			// e.g. "BOOT (C:)"
	LONGLONG m_totalBytes;	// Capacity
	LONGLONG m_freeBytes;	// Free space
	bool m_isRemote;		// Whether the drive type is DRIVE_REMOTE (network drive)
	double m_used;			// used space / total space
};


//
// CDrivesList. 
//
class CDrivesList: public COwnerDrawnListControl
{
	DECLARE_DYNAMIC(CDrivesList)
public:
	CDrivesList();
	CDriveItem *GetItem(int i);
	void SelectItem(CDriveItem *item);
	bool IsItemSelected(int i);
	void SelectAllLocalDrives();

	virtual bool HasImages();

	DECLARE_MESSAGE_MAP()
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLvnDeleteitem(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	afx_msg void OnNMDblclk(NMHDR *pNMHDR, LRESULT *pResult);
};


//
// CSelectDrivesDlg. The initial dialog, where the user can select 
// one or more drives or a folder for scanning.
//
class CSelectDrivesDlg : public CDialog
{
	DECLARE_DYNAMIC(CSelectDrivesDlg)
	enum { IDD = IDD_SELECTDRIVES };

public:
	CSelectDrivesDlg(CWnd* pParent = NULL);
	virtual ~CSelectDrivesDlg();

	// Dialog Data
	int m_radio;			// out.
	CString m_folderName;	// out. Valid if m_radio = RADIO_AFOLDER
	CStringArray m_drives;	// out. Valid if m_radio != RADIO_AFOLDER

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK();

	void UpdateButtons();

	CDrivesList m_list;
	CButton m_okButton;
	CStringArray m_selectedDrives;
	CLayout m_layout;

	DECLARE_MESSAGE_MAP()
	afx_msg void OnBnClickedBrowsefolder();
	afx_msg void OnLbnSelchangeDrives();
	afx_msg void OnBnClickedAlllocaldrives();
	afx_msg void OnBnClickedAfolder();
	afx_msg void OnBnClickedSomedrives();
	afx_msg void OnEnChangeFoldername();
	afx_msg void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	afx_msg void OnLvnItemchangedDrives(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	afx_msg void OnDestroy();
	afx_msg LRESULT OnWmuOk(WPARAM, LPARAM);
};
