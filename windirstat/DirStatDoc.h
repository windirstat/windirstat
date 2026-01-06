// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

#include "pch.h"
#include "TreeListControl.h"

class CItem;
class CItemDupe;
class CItemTop;
class CItemSearch;
enum LOGICAL_FOCUS : uint8_t;

//
// Data stored for each extension.
//
struct SExtensionRecord
{
    std::atomic<ULONGLONG> files = 0;
    std::atomic<ULONGLONG> bytes = 0;
    COLORREF color = 0;
};

//
// Maps an extension to an SExtensionRecord.
//
using CExtensionData = std::unordered_map<std::wstring, SExtensionRecord>;

//
// Hints for UpdateAllViews()
//
using VIEW_HINT = enum VIEW_HINT : std::uint8_t
{
    HINT_NULL,                      // General update
    HINT_NEWROOT,                   // Root item has changed - clear everything.
    HINT_SELECTIONACTION,           // Inform central selection handler to update selection (uses pHint)
    HINT_SELECTIONREFRESH,          // Inform all views to redraw based on current selections
    HINT_SELECTIONSTYLECHANGED,     // Only update selection in TreeMapView
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
public:
    static CDirStatDoc* Get() { return s_singleton; }

protected:
    CDirStatDoc(); // Created by MFC only
    DECLARE_DYNCREATE(CDirStatDoc)

    ~CDirStatDoc() override;

    static std::wstring EncodeSelection(const std::vector<std::wstring>& folders);
    static std::vector<std::wstring> DecodeSelection(const std::wstring& encodedPath);

    void DeleteContents() override;
    BOOL OnNewDocument() override;
    BOOL OnOpenDocument(LPCWSTR lpszPathName) override;
    BOOL OnOpenDocument(CItem* newroot);
    void SetPathName(LPCWSTR lpszPathName, BOOL bAddToMRU) override;
    void SetTitlePrefix(const std::wstring& prefix) const;

    COLORREF GetCushionColor(const std::wstring& ext);
    COLORREF GetZoomColor() const;

    CExtensionData* GetExtensionData();
    SExtensionRecord* GetExtensionDataRecord(const std::wstring& ext);
    ULONGLONG GetRootSize() const;

    void RefreshReparsePointItems();

    bool HasRootItem() const;
    bool IsRootDone() const;
    bool IsScanRunning() const;
    CItem* GetRootItem() const;
    CItem* GetZoomItem() const;
    bool IsZoomed() const;

    void SetHighlightExtension(const std::wstring& ext);
    std::wstring GetHighlightExtension() const;

    void UnlinkRoot();
    bool UserDefinedCleanupWorksForItem(USERDEFINEDCLEANUP* udc, const CItem* item) const;
    void StartScanningEngine(std::vector<CItem*> items);
    enum StopReason : uint8_t { Default, Stop, Abort };
    void StopScanningEngine(StopReason stopReason = Stop);
    void RefreshItem(const std::vector<CItem*>& item) const;
    void RefreshItem(CItem* item) const { RefreshItem(std::vector{ item }); }

    static void OpenItem(const CItem* item, const std::wstring& verb = {});

    void RecurseRefreshReparsePoints(CItem* items) const;
    void RebuildExtensionData();
    void DeletePhysicalItems(const std::vector<CItem*>& items, bool toTrashBin, bool emptyOnly = false) const;
    void SetZoomItem(CItem* item);
    static void AskForConfirmation(USERDEFINEDCLEANUP* udc, const CItem* item);
    void PerformUserDefinedCleanup(USERDEFINEDCLEANUP* udc, const CItem* item);
    void RefreshAfterUserDefinedCleanup(const USERDEFINEDCLEANUP* udc, CItem* item, std::vector<CItem*> & refreshQueue) const;
    void RecursiveUserDefinedCleanup(USERDEFINEDCLEANUP* udc, const std::wstring& rootPath, const std::wstring& currentPath);
    static void CallUserDefinedCleanup(bool isDirectory, const std::wstring& format, const std::wstring& rootPath, const std::wstring& currentPath, bool showConsoleWindow, bool wait);
    static std::wstring BuildUserDefinedCleanupCommandLine(const std::wstring& format, const std::wstring& rootPath, const std::wstring& currentPath);
    void PushReselectChild(CItem* item);
    CItem* PopReselectChild();
    void ClearReselectChildStack();
    bool IsReselectChildAvailable() const;
    static CompressionAlgorithm CompressionIdToAlg(UINT id);
    static bool FileTreeHasFocus();
    static bool DupeListHasFocus();
    static bool TopListHasFocus();
    static bool SearchListHasFocus();
    std::vector<CItem*> GetAllSelected();
    void InvalidateSelectionCache();
    static CTreeListControl* GetFocusControl();
    void UpdateAllViews(CView* pSender, VIEW_HINT hint = HINT_NULL, CItem* pHint = nullptr);

    static CDirStatDoc* s_singleton;

    bool m_showFreeSpace; // Whether to show the <Free Space> item
    bool m_showUnknown;   // Whether to show the <Unknown> item

    CItem* m_rootItem = nullptr; // The very root item
    std::wstring m_highlightExtension; // Currently highlighted extension
    CItem* m_zoomItem = nullptr;   // Current "zoom root"

    std::mutex m_extensionMutex;
    CExtensionData m_extensionData;    // Base for the extension view and cushion colors

    CList<CItem*, CItem*> m_reselectChildStack; // Stack for the "Re-select Child"-Feature

    std::unordered_map<std::wstring, BlockingQueue<CItem*>> m_queues; // The scanning and thread queue
    std::optional<std::jthread> m_thread; // Wrapper thread so we do not occupy the UI thread

    // Cache for GetAllSelected to avoid expensive queries
    LOGICAL_FOCUS m_cachedFocus{}; 
    std::vector<CItem*> m_cachedSelection;
    bool m_selectionCacheValid = false;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnRefreshSelected();
    afx_msg void OnRefreshAll();
    afx_msg void OnSaveResults();
    afx_msg void OnSaveDuplicates();
    afx_msg void OnLoadResults();
    afx_msg void OnEditCopy();
    afx_msg void OnCleanupEmptyRecycleBin();
    afx_msg void OnUpdateCentralHandler(CCmdUI* pCmdUI);
    afx_msg void OnUpdateCompressionHandler(CCmdUI* pCmdUI);
    afx_msg void OnUpdateViewShowFreeSpace(CCmdUI* pCmdUI);
    afx_msg void OnViewShowFreeSpace();
    afx_msg void OnUpdateViewShowUnknown(CCmdUI* pCmdUI);
    afx_msg void OnViewShowUnknown();
    afx_msg void OnTreeMapZoomIn();
    afx_msg void OnTreeMapZoomOut();
    afx_msg void OnRemoveRoamingProfiles();
    afx_msg void OnRemoveLocalProfiles();
    afx_msg void OnDisableHibernateFile();
    afx_msg void OnExecuteDiskCleanupUtility();
    afx_msg void OnExecuteProgramsFeatures();
    afx_msg void OnExecuteDismAnalyze();
    afx_msg void OnExecuteDismReset();
    afx_msg void OnExecuteDism();
    afx_msg void OnExplorerSelect();
    afx_msg void OnCommandPromptHere();
    afx_msg void OnPowerShellHere();
    afx_msg void OnCleanupDeleteToBin();
    afx_msg void OnCleanupDelete();
    afx_msg void OnCleanupEmptyFolder();
    afx_msg void OnSearch();
    afx_msg void OnUpdateUserDefinedCleanup(CCmdUI* pCmdUI);
    afx_msg void OnUserDefinedCleanup(UINT id);
    afx_msg void OnTreeMapSelectParent();
    afx_msg void OnTreeMapReselectChild();
    afx_msg void OnCleanupOpenTarget();
    afx_msg void OnCleanupProperties();
    afx_msg void OnComputeHash();
    afx_msg void OnCleanupCompress(UINT id);
    afx_msg void OnCleanupOptimizeVhd();
    afx_msg void OnScanSuspend();
    afx_msg void OnScanResume();
    afx_msg void OnScanStop();
    afx_msg void OnContextMenuExplore(UINT nID);
    afx_msg void OnRemoveShadowCopies();
    afx_msg void OnCleanupMoveTo();
};
