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
class CWinDirStatPane;
enum LOGICAL_FOCUS : uint8_t;

//
// Data stored for each extension.
//
struct alignas(std::hardware_destructive_interference_size) SExtensionRecord
{
    std::atomic<ULONGLONG> files = 0;
    std::atomic<ULONGLONG> bytes = 0;
    COLORREF color = 0;

    // Use relaxed memory ordering for simple accumulation operations
    void AddFile(const ULONGLONG size) noexcept
    {
        files.fetch_add(1, std::memory_order_relaxed);
        bytes.fetch_add(size, std::memory_order_relaxed);
    }

    void RemoveFile(const ULONGLONG size) noexcept
    {
        files.fetch_sub(1, std::memory_order_relaxed);
        bytes.fetch_sub(size, std::memory_order_relaxed);
    }

    ULONGLONG GetFiles() const noexcept { return files.load(std::memory_order_relaxed); }
    ULONGLONG GetBytes() const noexcept { return bytes.load(std::memory_order_relaxed); }
};

//
// Maps an extension to an SExtensionRecord.
//
using CExtensionData = std::unordered_map<std::wstring, SExtensionRecord>;

//
// Model changes that panes can respond to.
//
enum MODEL_CHANGE : std::uint8_t
{
    MODEL_CHANGE_NONE,                // General update
    MODEL_CHANGE_NEW_ROOT,            // Root item has changed - clear everything.
    MODEL_CHANGE_SELECTION_ACTION,    // Inform central selection handler to update selection (uses item)
    MODEL_CHANGE_SELECTION_REFRESH,   // Inform all views to redraw based on current selections
    MODEL_CHANGE_SELECTION_STYLE,     // Only update selection in TreeMapView
    MODEL_CHANGE_EXTENSION_SELECTION, // Type list selected a new extension
    MODEL_CHANGE_ZOOM,                // Only zoom item has changed.
    MODEL_CHANGE_LIST_STYLE,          // Options: List style (grid/stripes) or treelist colors changed
    MODEL_CHANGE_TREEMAP_STYLE,       // Options: Treemap style (grid, colors etc.) changed
    MODEL_CHANGE_SIZE_MODE            // Options: Size mode (logical vs physical) changed
};

//
// CWinDirStatModel. The application model.
// Owner of the root item and various other data (see data members).
//
class CWinDirStatModel final : public MessageTarget<CWinDirStatModel, CCmdTarget>
{
    friend class CWinDirStatPane;

public:
    static CWinDirStatModel* Get() { return s_singleton; }
    CWinDirStatModel();
    ~CWinDirStatModel() override;

    void ClearScanState();
    bool ResetScan();
    bool StartScan(const std::wstring& pathSpec);
    bool OpenLoadedScan(CItem* loadedRoot);
    void SetScanPathSpec(const std::wstring& pathSpec);
    const std::wstring& GetScanPathSpec() const { return m_scanPathSpec; }
    const std::wstring& GetScanTitle() const { return m_scanTitle; }
    void SetScanTitle(const std::wstring& title) { m_scanTitle = title; }
    void SetScanTitlePrefix(const std::wstring& prefix) const;

    COLORREF GetCushionColor(const std::wstring& ext);
    COLORREF GetZoomColor() const { return RGB(0, 0, 255); }

    CExtensionData* GetExtensionData() { return &m_extensionData; }
    SExtensionRecord* GetExtensionDataRecord(const std::wstring& ext);
    bool IsExtensionRegistered(const std::wstring& ext) const { return m_registeredExtensions.contains(ext); }
    ULONGLONG GetRootSize() const;

    void RefreshReparsePointItems() const;

    bool HasRootItem() const { return m_rootItem != nullptr; }
    bool IsRootDone() const;
    bool IsScanRunning() const;
    bool IsScanSettled() const;
    void RunPendingHeapCleanup();
    CItem* GetRootItem() const { return m_rootItem; }
    CItem* GetZoomItem() const { return m_zoomItem; }
    bool IsZoomed() const { return GetZoomItem() != GetRootItem(); }

    void SetHighlightExtension(const std::wstring& ext, bool unregistered = false);
    std::wstring GetHighlightExtension() const { return m_highlightExtension; }
    bool IsHighlightUnregistered() const { return m_highlightUnregistered; }
    const std::unordered_set<std::wstring>& GetHighlightExtensions() const { return m_highlightExtensions; }

    void UnlinkRoot();
    bool UserDefinedCleanupWorksForItem(USERDEFINEDCLEANUP* udc, const CItem* item) const;
    void RunUserDefinedCleanup(size_t index);
    void StartScanningEngine(std::vector<CItem*> items);
    enum StopReason : uint8_t { Default, Stop, Abort };
    void StopScanningEngine(StopReason stopReason = Stop);
    void RefreshItem(const std::vector<CItem*>& item) const;
    void RefreshItem(CItem* item) const { RefreshItem(std::vector{ item }); }

    static void OpenItem(const CItem* item, const std::wstring& verb = {});

    void RecurseRefreshReparsePoints(CItem* items) const;
    void RebuildExtensionData();
    void RebuildRegisteredExtensions();
    void DeletePhysicalItems(const std::vector<CItem*>& items, bool toTrashBin, bool emptyOnly = false) const;
    void SetZoomItem(CItem* item);
    void PerformUserDefinedCleanup(USERDEFINEDCLEANUP* udc, const CItem* item);
    void RefreshAfterUserDefinedCleanup(const USERDEFINEDCLEANUP* udc, CItem* item, std::vector<CItem*> & refreshQueue) const;
    void RecursiveUserDefinedCleanup(USERDEFINEDCLEANUP* udc, const std::wstring& rootPath, const std::wstring& currentPath);
    static void CallUserDefinedCleanup(bool isDirectory, const std::wstring& format, const std::wstring& rootPath, const std::wstring& currentPath, bool showConsoleWindow, bool wait);
    static std::wstring BuildUserDefinedCleanupCommandLine(const std::wstring& format, const std::wstring& rootPath, const std::wstring& currentPath);
    void PushReselectChild(CItem* item);
    CItem* PopReselectChild();
    void ClearReselectChildStack() { m_reselectChildStack.clear(); }
    bool IsReselectChildAvailable() const { return !m_reselectChildStack.empty(); }
    static CompressionAlgorithm CompressionIdToAlg(UINT id);
    static bool FileTreeHasFocus();
    static bool DupeListHasFocus();
    static bool TopListHasFocus();
    static bool SearchListHasFocus();
    static bool WatcherListHasFocus();
    static bool PermsListHasFocus();
    std::span<CItem* const> GetSelectedItemsView();
    std::vector<CItem*> GetAllSelected();
    void InvalidateSelectionCache() { m_selectionCacheValid = false; }
    static CTreeListControl* GetFocusControl();
    void NotifyPanes(MODEL_CHANGE change = MODEL_CHANGE_NONE, CItem* item = nullptr);

private:
    static bool ConfirmOperation(std::wstring_view operationId, Setting<bool>& prompt,
        std::wstring_view detail = {});
    static bool ConfirmOperation(std::wstring_view operationId, Setting<bool>& prompt,
        std::span<CItem* const> affectedItems, std::wstring_view detail = {});
    void RemoveLocalProfiles(std::wstring_view whereClause) const;
    void NotifyPanesExcept(CWnd* sender, MODEL_CHANGE change = MODEL_CHANGE_NONE, CItem* item = nullptr);

    static CWinDirStatModel* s_singleton;

    std::wstring m_scanPathSpec;
    std::wstring m_scanTitle;

    CItem* m_rootItem = nullptr; // The very root item
    CItem* m_zoomItem = nullptr;   // Current "zoom root"
    std::wstring m_highlightExtension; // Currently highlighted extension
    bool m_highlightUnregistered = false; // Highlight all unregistered extensions instead of one
    std::unordered_set<std::wstring> m_highlightExtensions; // Precomputed unregistered set for the treemap highlight
    std::unordered_set<std::wstring> m_registeredExtensions; // Snapshot of HKEY_CLASSES_ROOT extensions, rebuilt per scan

    std::mutex m_extensionMutex;
    CExtensionData m_extensionData;    // Base for the extension view and cushion colors

    std::vector<CItem*> m_reselectChildStack; // Stack for the "Re-select Child"-Feature

    std::unordered_map<std::wstring, BlockingQueue<CItem*>> m_queues; // The scanning and thread queue
    std::atomic_bool m_heapMinPending = false;
    std::future<void> m_heapMinTask; // Heap cleanup that does not extend scan state
    std::jthread m_thread; // Wrapper thread so we do not occupy the UI thread

    // Cache selected items so command-update handlers can use a non-owning view
    // without repeated queries or copies
    LOGICAL_FOCUS m_cachedFocus{};
    std::vector<CItem*> m_cachedSelection;
    bool m_selectionCacheValid = false;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnRefreshSelected();
    void OnRefreshAll();
    void OnSaveResults() const;
    void OnSaveDuplicates();
    void OnSavePermissions();
    void OnLoadResults();
    void OnEditCopy();
    void OnCleanupEmptyRecycleBin() const;
    void OnUpdateCentralHandler(CCmdUI* pCmdUI);
    void OnUpdateCompressionHandler(CCmdUI* pCmdUI);
    void OnUpdateViewShowFreeSpace(CCmdUI* pCmdUI);
    void OnViewShowFreeSpace();
    void OnUpdateViewShowUnknown(CCmdUI* pCmdUI);
    void OnViewShowUnknown();
    void OnTreeMapZoomIn();
    void OnTreeMapZoomOut();
    void OnTreeMapZoomReset();
    void OnRemoveRoamingProfiles() const;
    void OnRemoveLocalProfiles();
    void OnDisableHibernateFile();
    void OnExecuteDiskCleanupUtility();
    void OnLaunchStorageSense();
    void OnExecuteProgramsFeatures();
    void OnExecuteDismAnalyze();
    void OnExecuteDismReset();
    void OnExecuteDism();
    void OnExplorerSelect();
    void OnCommandPromptHere();
    void OnPowerShellHere();
    void OnCleanupDeleteToBin();
    void OnCleanupDelete();
    void OnCleanupEmptyFolder();
    void OnSearch();
    void OnUpdateUserDefinedCleanup(CCmdUI* pCmdUI);
    void OnUserDefinedCleanup(UINT id);
    void OnTreeMapSelectParent();
    void OnTreeMapReselectChild();
    void OnCleanupOpenTarget();
    void OnCleanupProperties();
    void OnComputeHash();
    void OnCleanupCompress(UINT id);
    void OnCleanupOptimizeVhd();
    void OnCleanupSparsifyFile();
    void OnCleanupRemoveEmpty();
    void OnToolsSetDates();
    void OnScanSuspend();
    void OnScanResume();
    void OnScanStop();
    void OnContextMenuExplore(UINT nID);
    void OnRemoveShadowCopies() const;
    void OnCleanupMoveTo();
    void OnRemoveMarkOfTheWebTags();
    void OnUpdateCreateHardlink(CCmdUI* pCmdUI);
    void OnCreateHardlink();
    void OnFilterExcludeItem();
};

inline std::span<const RouteEntry> CWinDirStatModel::Routes()
{
    using ThisClass = CWinDirStatModel;
    static constexpr std::array entries
    {
        Route::Command<&ThisClass::OnRefreshSelected>(ID_REFRESH_SELECTED),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_REFRESH_SELECTED),
        Route::Command<&ThisClass::OnRefreshAll>(ID_REFRESH_ALL),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_REFRESH_ALL),
        Route::Command<&ThisClass::OnLoadResults>(ID_LOAD_RESULTS),
        Route::Command<&ThisClass::OnSaveResults>(ID_SAVE_RESULTS),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_SAVE_RESULTS),
        Route::Command<&ThisClass::OnSaveDuplicates>(ID_SAVE_DUPLICATES),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_SAVE_DUPLICATES),
        Route::Command<&ThisClass::OnSavePermissions>(ID_SAVE_PERMISSIONS),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_SAVE_PERMISSIONS),
        Route::Command<&ThisClass::OnEditCopy>(ID_EDIT_COPY_CLIPBOARD),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_EDIT_COPY_CLIPBOARD),
        Route::Command<&ThisClass::OnCleanupEmptyRecycleBin>(ID_CLEANUP_EMPTY_BIN),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_EMPTY_BIN),
        Route::Command<&ThisClass::OnCleanupMoveTo>(ID_CLEANUP_MOVE_TO),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_MOVE_TO),
        Route::Update<&ThisClass::OnUpdateViewShowFreeSpace>(ID_VIEW_SHOWFREESPACE),
        Route::Command<&ThisClass::OnViewShowFreeSpace>(ID_VIEW_SHOWFREESPACE),
        Route::Update<&ThisClass::OnUpdateViewShowUnknown>(ID_VIEW_SHOWUNKNOWN),
        Route::Command<&ThisClass::OnViewShowUnknown>(ID_VIEW_SHOWUNKNOWN),
        Route::Command<&ThisClass::OnTreeMapZoomIn>(ID_TREEMAP_ZOOMIN),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_TREEMAP_ZOOMIN),
        Route::Command<&ThisClass::OnTreeMapZoomOut>(ID_TREEMAP_ZOOMOUT),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_TREEMAP_ZOOMOUT),
        Route::Command<&ThisClass::OnTreeMapZoomReset>(ID_TREEMAP_ZOOMRESET),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_TREEMAP_ZOOMRESET),
        Route::Command<&ThisClass::OnExplorerSelect>(ID_CLEANUP_EXPLORER_SELECT),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_EXPLORER_SELECT),
        Route::Command<&ThisClass::OnCommandPromptHere>(ID_CLEANUP_OPEN_IN_CONSOLE),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_OPEN_IN_CONSOLE),
        Route::Command<&ThisClass::OnPowerShellHere>(ID_CLEANUP_OPEN_IN_PWSH),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_OPEN_IN_PWSH),
        Route::Command<&ThisClass::OnCleanupDeleteToBin>(ID_CLEANUP_DELETE_BIN),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_DELETE_BIN),
        Route::Command<&ThisClass::OnCleanupDelete>(ID_CLEANUP_DELETE),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_DELETE),
        Route::Command<&ThisClass::OnCleanupEmptyFolder>(ID_CLEANUP_EMPTY_FOLDER),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_EMPTY_FOLDER),
        Route::Command<&ThisClass::OnCleanupRemoveEmpty>(ID_CLEANUP_REMOVE_EMPTY),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_REMOVE_EMPTY),
        Route::Command<&ThisClass::OnRemoveShadowCopies>(ID_CLEANUP_REMOVE_SHADOW),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_REMOVE_SHADOW),
        Route::Command<&ThisClass::OnSearch>(ID_SEARCH),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_SEARCH),
        Route::Command<&ThisClass::OnExecuteDismAnalyze>(ID_CLEANUP_DISM_ANALYZE),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_DISM_ANALYZE),
        Route::Command<&ThisClass::OnExecuteDism>(ID_CLEANUP_DISM_NORMAL),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_DISM_NORMAL),
        Route::Command<&ThisClass::OnExecuteDismReset>(ID_CLEANUP_DISM_RESET),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_DISM_RESET),
        Route::Command<&ThisClass::OnDisableHibernateFile>(ID_CLEANUP_HIBERNATE),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_HIBERNATE),
        Route::Command<&ThisClass::OnRemoveRoamingProfiles>(ID_CLEANUP_REMOVE_ROAMING),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_REMOVE_ROAMING),
        Route::Command<&ThisClass::OnRemoveLocalProfiles>(ID_CLEANUP_REMOVE_LOCAL),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_REMOVE_LOCAL),
        Route::Command<&ThisClass::OnExecuteDiskCleanupUtility>(ID_CLEANUP_DISK_CLEANUP),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_DISK_CLEANUP),
        Route::Command<&ThisClass::OnLaunchStorageSense>(ID_CLEANUP_STORAGE_SENSE),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_STORAGE_SENSE),
        Route::Command<&ThisClass::OnExecuteProgramsFeatures>(ID_CLEANUP_REMOVE_PROGRAMS),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_REMOVE_PROGRAMS),
        Route::Command<&ThisClass::OnRemoveMarkOfTheWebTags>(ID_CLEANUP_REMOVE_MOTW),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_REMOVE_MOTW),
        Route::Update<&ThisClass::OnUpdateCreateHardlink>(ID_CLEANUP_CREATE_HARDLINK),
        Route::Command<&ThisClass::OnCreateHardlink>(ID_CLEANUP_CREATE_HARDLINK),
        Route::Update<&ThisClass::OnUpdateUserDefinedCleanup>(ID_USERDEFINEDCLEANUP0, ID_USERDEFINEDCLEANUP9),
        Route::Command<&ThisClass::OnUserDefinedCleanup>(ID_USERDEFINEDCLEANUP0, ID_USERDEFINEDCLEANUP9),
        Route::Command<&ThisClass::OnTreeMapSelectParent>(ID_TREEMAP_SELECT_PARENT),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_TREEMAP_SELECT_PARENT),
        Route::Command<&ThisClass::OnTreeMapReselectChild>(ID_TREEMAP_RESELECT_CHILD),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_TREEMAP_RESELECT_CHILD),
        Route::Command<&ThisClass::OnCleanupOpenTarget>(ID_CLEANUP_OPEN_SELECTED),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_OPEN_SELECTED),
        Route::Command<&ThisClass::OnCleanupProperties>(ID_CLEANUP_PROPERTIES),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_PROPERTIES),
        Route::Command<&ThisClass::OnComputeHash>(ID_COMPUTE_HASH),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_COMPUTE_HASH),
        Route::Update<&ThisClass::OnUpdateCompressionHandler>(ID_COMPRESS_NONE, ID_COMPRESS_LZX),
        Route::Command<&ThisClass::OnCleanupCompress>(ID_COMPRESS_NONE, ID_COMPRESS_LZX),
        Route::Command<&ThisClass::OnCleanupOptimizeVhd>(ID_CLEANUP_OPTIMIZE_VHD),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_OPTIMIZE_VHD),
        Route::Command<&ThisClass::OnCleanupSparsifyFile>(ID_CLEANUP_SPARSIFY_FILE),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_CLEANUP_SPARSIFY_FILE),
        Route::Command<&ThisClass::OnToolsSetDates>(ID_TOOLS_SET_DATES),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_TOOLS_SET_DATES),
        Route::Command<&ThisClass::OnScanResume>(ID_SCAN_RESUME),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_SCAN_RESUME),
        Route::Command<&ThisClass::OnScanSuspend>(ID_SCAN_SUSPEND),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_SCAN_SUSPEND),
        Route::Command<&ThisClass::OnScanStop>(ID_SCAN_STOP),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_SCAN_STOP),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_POPUP_CANCEL),
        Route::Command<&ThisClass::OnFilterExcludeItem>(ID_FILTER_EXCLUDE_ITEM),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_FILTER_EXCLUDE_ITEM),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_INDICATOR_RAM),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_INDICATOR_DISK),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_INDICATOR_IDLE),
        Route::Update<&ThisClass::OnUpdateCentralHandler>(ID_INDICATOR_SIZE),
        Route::Command<&ThisClass::OnContextMenuExplore>(CONTENT_MENU_MINCMD, CONTENT_MENU_MAXCMD),
    };
    return entries;
}
