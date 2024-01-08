// DirStatDoc.h - Declaration of the CDirstatDoc class
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2024 WinDirStat Team (windirstat.net)
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

#pragma once

#include "selectdrivesdlg.h"
#include <common/Constants.h>
#include "Options.h"

class CItem;
class CWorkLimiter;

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
typedef CMap<CStringW, LPCWSTR, SExtensionRecord, SExtensionRecord&> CExtensionData;

//
// Hints for UpdateAllViews()
//
enum
{
    HINT_NULL,                      // General update
    HINT_NEWROOT,                   // Root item has changed - clear everything.
    HINT_SELECTIONACTION,           // Inform central selection handler to update selection (uses pHint)
    HINT_SELECTIONREFRESH,          // Inform all views to redraw based on current selections
    HINT_SELECTIONSTYLECHANGED,     // Only update selection in Graphview
    HINT_EXTENSIONSELECTIONCHANGED, // Type list selected a new extension
    HINT_ZOOMCHANGED,               // Only zoom item has changed.
    HINT_REDRAWWINDOW,              // Update menu controls
    HINT_SOMEWORKDONE,              // Directory list shall process mouse messages first, then re-sort.
    HINT_LISTSTYLECHANGED,          // Options: List style (grid/stripes) or treelist colors changed
    HINT_TREEMAPSTYLECHANGED        // Options: Treemap style (grid, colors etc.) changed
};

//
// CDirstatDoc. The "Document" class.
// Owner of the root item and various other data (see data members).
//
class CDirstatDoc final : public CDocument
{
protected:
    CDirstatDoc(); // Created by MFC only
    DECLARE_DYNCREATE(CDirstatDoc)

public:
    ~CDirstatDoc() override;

    static CStringW EncodeSelection(RADIO radio, const CStringW& folder, const CStringArray& drives);
    static void DecodeSelection(const CStringW& s, CStringW& folder, CStringArray& drives);
    static WCHAR GetEncodingSeparator();

    void DeleteContents() override;
    BOOL OnNewDocument() override;
    BOOL OnOpenDocument(LPCWSTR lpszPathName) override;
    void SetPathName(LPCWSTR lpszPathName, BOOL bAddToMRU) override;
    void Serialize(CArchive& ar) override;

    void SetTitlePrefix(const CStringW& prefix) const;

    COLORREF GetCushionColor(LPCWSTR ext);
    COLORREF GetZoomColor();

    bool OptionShowFreeSpace() const;
    bool OptionShowUnknown() const;

    const CExtensionData* GetExtensionData();
    ULONGLONG GetRootSize();

    bool Work(CWorkLimiter* limiter); // return: true if done.
    bool IsDrive(const CStringW& spec);
    void RefreshMountPointItems();
    void RefreshJunctionItems();

    bool IsRootDone();
    CItem* GetRootItem();
    CItem* GetZoomItem();
    bool IsZoomed();

    size_t GetSelectionCount();
    CItem* GetSelection(size_t i);
    void OnUpdateCentralHandler(CCmdUI* pCmdUI);

    void SetHighlightExtension(LPCWSTR ext);
    CStringW GetHighlightExtension();

    void UnlinkRoot();
    bool UserDefinedCleanupWorksForItem(const USERDEFINEDCLEANUP* udc, const CItem* item);
    ULONGLONG GetWorkingItemReadJobs();

    static void OpenItem(const CItem* item, LPCWSTR verb = L"open");

protected:
    void RecurseRefreshMountPointItems(CItem* item);
    void RecurseRefreshJunctionItems(CItem* item);
    void GetDriveItems(CArray<CItem*, CItem*>& drives);
    void RefreshRecyclers();
    void RebuildExtensionData();
    void SortExtensionData(CStringArray& sortedExtensions);
    void SetExtensionColors(const CStringArray& sortedExtensions);
    static CExtensionData* _pqsortExtensionData;
    static int __cdecl _compareExtensions(const void* ext1, const void* ext2);
    void SetWorkingItemAncestor(CItem* item);
    void SetWorkingItem(CItem* item);
    bool DeletePhysicalItem(CItem* item, bool toTrashBin);
    void SetZoomItem(CItem* item);
    void RefreshItem(CItem* item);
    void AskForConfirmation(const USERDEFINEDCLEANUP* udc, CItem* item);
    void PerformUserDefinedCleanup(const USERDEFINEDCLEANUP* udc, CItem* item);
    void RefreshAfterUserDefinedCleanup(const USERDEFINEDCLEANUP* udc, CItem* item);
    void RecursiveUserDefinedCleanup(const USERDEFINEDCLEANUP* udc, const CStringW& rootPath, const CStringW& currentPath);
    void CallUserDefinedCleanup(bool isDirectory, const CStringW& format, const CStringW& rootPath, const CStringW& currentPath, bool showConsoleWindow, bool wait);
    CStringW BuildUserDefinedCleanupCommandLine(LPCWSTR format, LPCWSTR rootPath, LPCWSTR currentPath);
    void PushReselectChild(CItem* item);
    CItem* PopReselectChild();
    void ClearReselectChildStack();
    bool IsReselectChildAvailable();
    static bool DirectoryListHasFocus();

    bool m_showFreeSpace; // Whether to show the <Free Space> item
    bool m_showUnknown;   // Whether to show the <Unknown> item

    bool m_showMyComputer; // True, if the user selected more than one drive for scanning.
    // In this case, we need a root pseudo item ("My Computer").

    CItem* m_rootItem;                      // The very root item

    CStringW m_highlightExtension; // Currently highlighted extension
    CItem* m_zoomItem;             // Current "zoom root"
    CItem* m_workingItem;          // Current item we are working on. For progress indication

    bool m_extensionDataValid;      // If this is false, m_extensionData must be rebuilt
    CExtensionData m_extensionData; // Base for the extension view and cushion colors

    CList<CItem*, CItem*> m_reselectChildStack; // Stack for the "Re-select Child"-Feature

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnRefreshSelected();
    afx_msg void OnRefreshAll();
    afx_msg void OnEditCopy();
    afx_msg void OnCleanupEmptyRecycleBin();
    afx_msg void OnUpdateViewShowFreeSpace(CCmdUI* pCmdUI);
    afx_msg void OnViewShowFreeSpace();
    afx_msg void OnUpdateViewShowUnknown(CCmdUI* pCmdUI);
    afx_msg void OnViewShowUnknown();
    afx_msg void OnTreemapZoomIn();
    afx_msg void OnTreemapZoomOut();
    afx_msg void OnExplorerSelect();
    afx_msg void OnCommandPromptHere();
    afx_msg void OnCleanupDeleteToBin();
    afx_msg void OnCleanupDelete();
    afx_msg void OnUpdateUserDefinedCleanup(CCmdUI* pCmdUI);
    afx_msg void OnUserDefinedCleanup(UINT id);
    afx_msg void OnTreemapSelectParent();
    afx_msg void OnTreemapReselectChild();
    afx_msg void OnCleanupOpenTarget();
    afx_msg void OnCleanupProperties();
    afx_msg void OnScanSuspend();
    afx_msg void OnScanResume();

public:
#ifdef _DEBUG
    void AssertValid() const override;
    void Dump(CDumpContext& dc) const override;
#endif
};

//
// The document is needed in many places.
//
extern CDirstatDoc* GetDocument();
