// DirStatDoc.h - Declaration of the CDirStatDoc class
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

#include <vector>

#include "BlockingQueue.h"

class CItem;

//
// The treemap colors as calculated in CDirStatDoc::SetExtensionColors()
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
    HINT_LISTSTYLECHANGED,          // Options: List style (grid/stripes) or treelist colors changed
    HINT_TREEMAPSTYLECHANGED        // Options: Treemap style (grid, colors etc.) changed
};

//
// CDirStatDoc. The "Document" class.
// Owner of the root item and various other data (see data members).
//
class CDirStatDoc final : public CDocument
{
protected:
    CDirStatDoc(); // Created by MFC only
    DECLARE_DYNCREATE(CDirStatDoc)

public:
    ~CDirStatDoc() override;

    static CStringW EncodeSelection(RADIO radio, const CStringW& folder, const CStringArray& drives);
    static void DecodeSelection(const CStringW& s, CStringW& folder, CStringArray& drives);
    static WCHAR GetEncodingSeparator();

    void DeleteContents() override;
    BOOL OnNewDocument() override;
    BOOL OnOpenDocument(LPCWSTR lpszPathName) override;
    BOOL OnOpenDocument(CItem* newroot);
    void SetPathName(LPCWSTR lpszPathName, BOOL bAddToMRU) override;
    void SetTitlePrefix(const CStringW& prefix) const;

    COLORREF GetCushionColor(LPCWSTR ext);
    COLORREF GetZoomColor();

    const CExtensionData* GetExtensionData();
    ULONGLONG GetRootSize() const;

    static bool IsDrive(const CStringW& spec);
    void RefreshMountPointItems();
    void RefreshJunctionItems();

    bool IsRootDone() const;
    CItem* GetRootItem() const;
    CItem* GetZoomItem() const;
    bool IsZoomed() const;

    void SetHighlightExtension(LPCWSTR ext);
    CStringW GetHighlightExtension();

    void UnlinkRoot();
    bool UserDefinedCleanupWorksForItem(USERDEFINEDCLEANUP* udc, const CItem* item);
    void StartupCoordinator(std::vector<CItem*> items);
    void ShutdownCoordinator(bool wait = true);
    void RefreshItem(std::vector<CItem*> item);
    void RefreshItem(CItem* item) { RefreshItem(std::vector{ item }); }

    static void OpenItem(const CItem* item, LPCWSTR verb = L"open");

protected:
    void RecurseRefreshMountPointItems(CItem* item);
    void RecurseRefreshJunctionItems(CItem* item);
    std::vector<CItem*> GetDriveItems() const;
    void RefreshRecyclers() const;
    void RebuildExtensionData();
    void SortExtensionData(CStringArray& sortedExtensions);
    void SetExtensionColors(const CStringArray& sortedExtensions);
    static CExtensionData* _pqsortExtensionData;
    bool DeletePhysicalItem(CItem* item, bool toTrashBin);
    void SetZoomItem(CItem* item);
    static void AskForConfirmation(USERDEFINEDCLEANUP* udc, CItem* item);
    void PerformUserDefinedCleanup(USERDEFINEDCLEANUP* udc, CItem* item);
    void RefreshAfterUserDefinedCleanup(const USERDEFINEDCLEANUP* udc, CItem* item);
    void RecursiveUserDefinedCleanup(USERDEFINEDCLEANUP* udc, const CStringW& rootPath, const CStringW& currentPath);
    static void CallUserDefinedCleanup(bool isDirectory, const CStringW& format, const CStringW& rootPath, const CStringW& currentPath, bool showConsoleWindow, bool wait);
    static CStringW BuildUserDefinedCleanupCommandLine(LPCWSTR format, LPCWSTR rootPath, LPCWSTR currentPath);
    void PushReselectChild(CItem* item);
    CItem* PopReselectChild();
    void ClearReselectChildStack();
    bool IsReselectChildAvailable() const;
    static bool DirectoryListHasFocus();

    bool m_showFreeSpace; // Whether to show the <Free Space> item
    bool m_showUnknown;   // Whether to show the <Unknown> item

    bool m_showMyComputer; // True, if the user selected more than one drive for scanning.
    // In this case, we need a root pseudo item ("My Computer").

    CItem* m_rootItem; // The very root item

    CStringW m_highlightExtension; // Currently highlighted extension
    CItem* m_zoomItem;             // Current "zoom root"

    bool m_extensionDataValid;      // If this is false, m_extensionData must be rebuilt
    CExtensionData m_extensionData; // Base for the extension view and cushion colors

    CList<CItem*, CItem*> m_reselectChildStack; // Stack for the "Re-select Child"-Feature

    BlockingQueue<CItem*> queue;      // The scanning queue
    std::vector<std::thread> threads; // For tracking threads

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnRefreshSelected();
    afx_msg void OnRefreshAll();
    afx_msg void OnSaveResults();
    afx_msg void OnLoadResults();
    afx_msg void OnEditCopy();
    afx_msg void OnCleanupEmptyRecycleBin();
    afx_msg void OnUpdateCentralHandler(CCmdUI* pCmdUI);
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
};

//
// The document is needed in many places.
//
extern CDirStatDoc* GetDocument();
