// dirstatdoc.h - Declaration of the CDirstatDoc class
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

#ifndef __WDS_DIRSTATDOC_H__
#define __WDS_DIRSTATDOC_H__
#pragma once

#include "selectdrivesdlg.h"
#include "../common/wds_constants.h"

class CItem;

//
// The treemap colors as calculated in CDirstatDoc::SetExtensionColors()
// all have the "brightness" BASE_BRIGHTNESS.
// I define brightness as a number from 0 to 3.0: (r+g+b)/255.
// RGB(127, 255, 0), for example, has a brightness of 2.5.
// 
#define BASE_BRIGHTNESS 1.8

//
// Data stored for each extension.
//
struct SExtensionRecord
{
	ULONGLONG files;
	ULONGLONG bytes;
	COLORREF color;
};

//
// Maps an extension (".bmp") to an SExtensionRecord.
//
typedef CMap<CString, LPCTSTR, SExtensionRecord, SExtensionRecord&> CExtensionData;

//
// Hints for UpdateAllViews()
//
enum
{
	HINT_NULL,				// General update
	HINT_NEWROOT,			// Root item has changed - clear everything.
	HINT_SELECTIONCHANGED,	// The selection has changed, EnsureVisible.
	HINT_SHOWNEWSELECTION,	// The selection has changed, Show Path
	HINT_SELECTIONSTYLECHANGED,	// Only update selection in Graphview
	HINT_EXTENSIONSELECTIONCHANGED,	// Type list selected a new extension
	HINT_ZOOMCHANGED,		// Only zoom item has changed.
	HINT_REDRAWWINDOW,		// Only graphically redraw views.
	HINT_SOMEWORKDONE,		// Directory list shall process mouse messages first, then re-sort.

	HINT_LISTSTYLECHANGED,	// Options: List style (grid/stripes) or treelist colors changed
	HINT_TREEMAPSTYLECHANGED	// Options: Treemap style (grid, colors etc.) changed
};

//
// CDirstatDoc. The "Document" class. 
// Owner of the root item and various other data (see data members).
//
class CDirstatDoc : public CDocument
{
protected:
	CDirstatDoc();	// Created by MFC only
	DECLARE_DYNCREATE(CDirstatDoc)

public:
	virtual ~CDirstatDoc();

	static CString EncodeSelection(RADIO radio, CString folder, const CStringArray& drives);
	static void DecodeSelection(CString s, CString& folder, CStringArray& drives);
	static TCHAR GetEncodingSeparator();

	virtual void DeleteContents();
	virtual BOOL OnNewDocument();
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
	virtual void SetPathName(LPCTSTR lpszPathName, BOOL bAddToMRU);
	virtual void Serialize(CArchive& ar);

	void SetTitlePrefix(CString prefix);

	COLORREF GetCushionColor(LPCTSTR ext);
	COLORREF GetZoomColor();

	bool OptionShowFreeSpace();
	bool OptionShowUnknown();

	const CExtensionData *GetExtensionData();
	ULONGLONG GetRootSize();

	void ForgetItemTree();
	bool Work(DWORD ticks); // return: true if done.
	bool IsDrive(CString spec);
	void RefreshMountPointItems();
	void RefreshJunctionItems();

	bool IsRootDone();
	CItem *GetRootItem();
	CItem *GetZoomItem();
	bool IsZoomed();
	
	void SetSelection(const CItem *item, bool keepReselectChildStack = false);
	CItem *GetSelection();
	
	void SetHighlightExtension(LPCTSTR ext);
	CString GetHighlightExtension();

	void UnlinkRoot();
	bool UserDefinedCleanupWorksForItem(const USERDEFINEDCLEANUP *udc, const CItem *item);
	ULONGLONG GetWorkingItemReadJobs();

	void OpenItem(const CItem *item);

protected:
	void RecurseRefreshMountPointItems(CItem *item);
	void RecurseRefreshJunctionItems(CItem *item);
	void GetDriveItems(CArray<CItem *, CItem *>& drives);
	void RefreshRecyclers();
	void RebuildExtensionData();
	void SortExtensionData(CStringArray& sortedExtensions);
	void SetExtensionColors(const CStringArray& sortedExtensions);
	static CExtensionData *_pqsortExtensionData;
	static int __cdecl _compareExtensions(const void *ext1, const void *ext2);
	void SetWorkingItemAncestor(CItem *item);
	void SetWorkingItem(CItem *item);
	bool DeletePhysicalItem(CItem *item, bool toTrashBin);
	void SetZoomItem(CItem *item);
	void RefreshItem(CItem *item);
	void AskForConfirmation(const USERDEFINEDCLEANUP *udc, CItem *item);
	void PerformUserDefinedCleanup(const USERDEFINEDCLEANUP *udc, CItem *item);
	void RefreshAfterUserDefinedCleanup(const USERDEFINEDCLEANUP *udc, CItem *item);
	void RecursiveUserDefinedCleanup(const USERDEFINEDCLEANUP *udc, const CString& rootPath, const CString& currentPath);
	void CallUserDefinedCleanup(bool isDirectory, const CString& format, const CString& rootPath, const CString& currentPath, bool showConsoleWindow, bool wait);
	CString BuildUserDefinedCleanupCommandLine(LPCTSTR format, LPCTSTR rootPath, LPCTSTR currentPath);
	void PushReselectChild(CItem *item);
	CItem *PopReselectChild();
	void ClearReselectChildStack();
	bool IsReselectChildAvailable();
	bool DirectoryListHasFocus();

	bool m_showFreeSpace;		// Whether to show the <Free Space> item
	bool m_showUnknown;			// Whether to show the <Unknown> item

	bool m_showMyComputer;		// True, if the user selected more than one drive for scanning.
								// In this case, we need a root pseudo item ("My Computer").

	CItem *m_rootItem;			// The very root item
	CItem *m_selectedItem;		// Currently selected item, or NULL
	CString m_highlightExtension;	// Currently highlighted extension
	CItem *m_zoomItem;			// Current "zoom root"
	CItem *m_workingItem;		// Current item we are working on. For progress indication

	bool m_extensionDataValid;			// If this is false, m_extensionData must be rebuilt
	CExtensionData m_extensionData;		// Base for the extension view and cushion colors

	CList<CItem *, CItem *> m_reselectChildStack; // Stack for the "Re-select Child"-Feature

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnUpdateRefreshselected(CCmdUI *pCmdUI);
	afx_msg void OnRefreshselected();
	afx_msg void OnUpdateRefreshall(CCmdUI *pCmdUI);
	afx_msg void OnRefreshall();
	afx_msg void OnUpdateEditCopy(CCmdUI *pCmdUI);
	afx_msg void OnEditCopy();
	afx_msg void OnCleanupEmptyrecyclebin();
	afx_msg void OnUpdateViewShowfreespace(CCmdUI *pCmdUI);
	afx_msg void OnViewShowfreespace();
	afx_msg void OnUpdateViewShowunknown(CCmdUI *pCmdUI);
	afx_msg void OnViewShowunknown();
	afx_msg void OnUpdateTreemapZoomin(CCmdUI *pCmdUI);
	afx_msg void OnTreemapZoomin();
	afx_msg void OnUpdateTreemapZoomout(CCmdUI *pCmdUI);
	afx_msg void OnTreemapZoomout();
	afx_msg void OnUpdateExplorerHere(CCmdUI *pCmdUI);
	afx_msg void OnExplorerHere();
	afx_msg void OnUpdateCommandPromptHere(CCmdUI *pCmdUI);
	afx_msg void OnCommandPromptHere();
	afx_msg void OnUpdateCleanupDeletetotrashbin(CCmdUI *pCmdUI);
	afx_msg void OnCleanupDeletetotrashbin();
	afx_msg void OnUpdateCleanupDelete(CCmdUI *pCmdUI);
	afx_msg void OnCleanupDelete();
	afx_msg void OnUpdateUserdefinedcleanup(CCmdUI *pCmdUI);
	afx_msg void OnUserdefinedcleanup(UINT id);
	afx_msg void OnUpdateTreemapSelectparent(CCmdUI *pCmdUI);
	afx_msg void OnTreemapSelectparent();
	afx_msg void OnUpdateTreemapReselectchild(CCmdUI *pCmdUI);
	afx_msg void OnTreemapReselectchild();
	afx_msg void OnUpdateCleanupOpen(CCmdUI *pCmdUI);
	afx_msg void OnCleanupOpen();
	afx_msg void OnUpdateCleanupProperties(CCmdUI *pCmdUI);
	afx_msg void OnCleanupProperties();

public:
	#ifdef _DEBUG
		virtual void AssertValid() const;
		virtual void Dump(CDumpContext& dc) const;
	#endif
};

//
// The document is needed in many places.
//
extern CDirstatDoc *GetDocument();

#endif // __WDS_DIRSTATDOC_H__
