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
#include "PacMan.h"
#include "FileTabbedView.h"
#include "Dialogs/ProgressDlg.h"
#include "LayoutPopup.h"

class CWdsSplitterWnd;
class CMainFrame;

class CFileTreeView;
class CExtensionView;
class CVisualizationPane;

//
// The "logical focus" can be
// - on the Directory List
// - on the Extension List
// Although these windows can lose the real focus, for instance
// when a dialog box is opened, the logical focus will not be lost.
//
enum LOGICAL_FOCUS : uint8_t
{
    LF_NONE = 0,
    LF_FILETREE,
    LF_DUPELIST,
    LF_TOPLIST,
    LF_SEARCHLIST,
    LF_WATCHERLIST,
    LF_PERMSLIST,
    LF_EXTLIST,
    LF_STORAGEANALYTICS,
};

//
// CSettingsSheet.
//
class CSettingsSheet final : public MessageTarget<CSettingsSheet, CPropertySheet>
{
public:
    CSettingsSheet();
    void SetRestartRequired(const bool changed) { m_restartRequest = changed; }
    bool OnInitDialog() override;
    static bool ShowSettings(int initialPage = -1, bool refreshOnFilteringChange = true);

    bool m_restartApplication = false; // [out]
    int m_initialPage = -1;

protected:
    bool OnCommand(WPARAM wParam, LPARAM lParam) override;
    HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    bool OnEraseBkgnd(CDC* pDC) const;

    bool m_restartRequest = false;
    bool m_alreadyAsked = false;

public:
    static std::span<const RouteEntry> Routes();

};

//
// CWdsSplitterWnd. A CSplitterWnd with 2 columns or rows, which
// knows about the current split ratio and retains it even when resized.
//
class CWdsSplitterWnd final : public MessageTarget<CWdsSplitterWnd, CSplitterWnd>
{
public:
    CWdsSplitterWnd(double * splitterPos);
    void StopTracking(bool bAccept) override;
    void SetSplitterPos(double pos);
    void RestoreSplitterPos(double posIfVirgin);
    void ResetUserPosition() { m_wasTrackedByUser = false; }
    void SetStorage(double* ptr) { m_userSplitterPos = ptr; m_wasTrackedByUser = (*ptr > 0.0 && *ptr < 1.0); }
    void ClearPaneTracking();
    void TrackPane(int pane, std::function<void(bool)> onToggle, std::function<void()> onMinimize);

protected:
    bool PreCreateWindow(CREATESTRUCT& cs) override;

    struct PaneTracking
    {
        std::function<void(bool)> onToggle;
        std::function<void()> onMinimize;
    };

    double m_splitterPos{0};    // Current split ratio
    bool m_wasTrackedByUser;    // True as soon as user has modified the splitter position
    double * m_userSplitterPos; // Split ratio as set by the user
    PaneTracking m_paneTracking[2];

    void PostNcDestroy() override;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnSize(UINT nType, int cx, int cy);
};

//
// CPacmanControl. Pacman on the status bar.
//
class CPacmanControl final : public MessageTarget<CPacmanControl, CWnd>
{
public:
    CPacmanControl() = default;
    void Drive();
    void Start();
    void Stop();

protected:
    CPacman m_pacman;

public:
    static std::span<const RouteEntry> Routes();

protected:
    void OnPaint();
    int OnCreate(LPCREATESTRUCT lpCreateStruct);
    bool OnEraseBkgnd(CDC* pDC);
};

//
// CMainFrame. The main application window.
//
class CMainFrame final : public MessageTarget<CMainFrame, CFrameWnd>
{
protected:
    static constexpr DWORD WM_CALLBACKUI = WM_APP + 1;
    static UINT s_TaskBarMessage;
    inline static CMainFrame* s_Singleton = nullptr;

public:
    CMainFrame();
    ~CMainFrame() override;

    void InitialShowWindow();
    void InvokeInMessageThread(std::function<void()> callback) const;

    void RestoreVisualizationPane(bool force = false);
    void MinimizeVisualizationPane();
    void MinimizeExtensionView();
    void ExpandFileTabbedView();
    void RestoreSplitterPositions();
    void ApplyPaneVisibility(bool restoreDuringScan = false);

    // Used for storing and retrieving the main panes
    CFileTabbedView* m_fileTabbedView = nullptr;
    CExtensionView* m_extensionView = nullptr;
    CVisualizationPane* m_visualizationPane = nullptr;
    CFileTreeView* GetFileTreeView() const { return m_fileTabbedView->GetFileTreeView(); }
    CFileTopView* GetFileTopView() const { return m_fileTabbedView->GetFileTopView(); }
    CFileDupeView* GetFileDupeView() const { return m_fileTabbedView->GetFileDupeView(); }
    CFileSearchView* GetFileSearchView() const { return m_fileTabbedView->GetFileSearchView(); }
    CFileWatcherView* GetFileWatcherView() const { return m_fileTabbedView->GetFileWatcherView(); }
    CFilePermsView* GetFilePermsView() const { return m_fileTabbedView->GetFilePermsView(); }
    CFileTabbedView* GetFileTabbedView() const { return m_fileTabbedView; }
    CExtensionView* GetExtensionView() const { return m_extensionView; }
    GraphPane GetGraphPaneType() const;
    void SelectGraphPane(GraphPane pane);
    void ShowVisualization(bool show) const;
    bool IsVisualizationShown() const;
    CWinDirStatPane* GetVisualizationPane() const;
    CWinDirStatPane* GetActiveVisualization() const;

    void CreateProgress(ULONGLONG range);
    void UpdateProgressRange(ULONGLONG range);
    void SetProgressComplete();
    void SuspendState(bool suspend);
    bool IsScanSuspended() const { return m_scanSuspend; }

    void UpdateProgress();
    void UpdateDynamicMenuItems(CMenu* menu, CMenu* menuHeader = nullptr) const;
    std::pair<CMenu*, int> LocateNamedMenu(const CMenu* menu, const std::wstring& subMenuText, bool removeItems = true) const;

    void SetLogicalFocus(LOGICAL_FOCUS lf);
    LOGICAL_FOCUS GetLogicalFocus() const { return m_logicalFocus; }
    void MoveFocus(LOGICAL_FOCUS logicalFocus);
    void UpdatePaneText();

    static void QueryRecycleBin(ULONGLONG& items, ULONGLONG& bytes);

    bool OnCreateClient() override;
    bool PreCreateWindow(CREATESTRUCT& cs) override;

    void CreateStatusProgress();
    void CreatePacmanProgress();
    void LayoutProgress();
    void DestroyProgress();

    void SetStatusPaneText(const CDC& cdc, CStatusBar::PaneId pane, const std::wstring& text, int minWidth = 0);
    void UpdateCleanupMenu(CMenu* menu, bool triggerAsync = true);

    UINT_PTR m_timer = 0;           // Timer for updating the display
    bool m_progressVisible = false; // True while progress must be shown (either pacman or progress bar)
    bool m_scanSuspend = false;     // True if the scan has been suspended
    bool m_shuttingDown = false;    // Marks the process is shutting down so we can exit timers
    ULONGLONG m_progressRange = 0;  // Progress range. A range of 0 means Pacman should be used.
    ULONGLONG m_progressPos = 0;    // Progress position (<= progressRange, or an item count in case of m_progressRang == 0)
    CItem* m_workingItem = nullptr;

    CWdsSplitterWnd m_subSplitter{ COptions::SubSplitterPos.Ptr() }; // Contains the two upper views
    CWdsSplitterWnd m_splitter{ COptions::MainSplitterPos.Ptr() };    // Contains (a) m_wndSubSplitter and (b) the graph view.
    CLayoutPopup m_layoutPopup;                                        // Floating layout-picker popup

    CStatusBar m_wndStatusBar; // Status bar
    CToolBar m_wndToolBar;     // Toolbar
    CSize m_defaultButtonSize;  // Toolbar button size at creation, before DPI and size scaling
    int m_watcherAutoScrollOnImage = -1;
    int m_watcherAutoScrollOffImage = -1;
    CWdsProgressCtrl m_progress;  // Progress control. Is Create()ed and Destroy()ed again every time.
    CPacmanControl m_pacman;      // Static control for Pacman
    LOGICAL_FOCUS m_logicalFocus = LF_NONE; // Which view has the logical focus

    CComPtr<ITaskbarList3> m_taskbarList;
    TBPFLAG m_taskbarButtonState = TBPF_INDETERMINATE;
    TBPFLAG m_taskbarButtonPreviousState = TBPF_INDETERMINATE;

    // Cached values for cleanup menu queries (updated asynchronously)
    ULONGLONG m_recycleBinItems = 0;
    ULONGLONG m_recycleBinBytes = 0;
    ULONGLONG m_shadowCopyCount = 0;
    ULONGLONG m_shadowCopyBytes = 0;

static std::span<const RouteEntry> Routes();

protected:
    CCmdTarget* GetCommandTarget() const override { return CWinDirStatModel::Get(); }
    int OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnSetFocus(CWnd* pOldWnd);
    void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    LRESULT OnEnterSizeMove(WPARAM, LPARAM) const;
    LRESULT OnExitSizeMove(WPARAM, LPARAM) const;
    LRESULT OnCallbackRequest(WPARAM, LPARAM lParam);
    void OnTimer(UINT_PTR nIDEvent);
    void OnClose();
    void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, bool bSysMenu);
    LRESULT OnMenuCommand(WPARAM position, LPARAM menuHandle);
    void OnSize(UINT nType, int cx, int cy);
    void OnUpdateViewShowVisualization(CCmdUI* pCmdUI) const;
    void OnUpdateTreeMapUseLogical(CCmdUI* pCmdUI);
    void OnUpdateTreeMapUsePhysical(CCmdUI* pCmdUI);
    void OnUpdateViewAbsolutePercentages(CCmdUI* pCmdUI);
    void OnUpdateViewShowFileTypes(CCmdUI* pCmdUI) const;
    void OnUpdateViewGroupUnregisteredTypes(CCmdUI* pCmdUI) const;
    void OnUpdateViewShowWatcher(CCmdUI* pCmdUI) const;
    void OnViewShowVisualization();
    void OnViewTreeMapStyle(UINT commandId);
    void OnUpdateViewTreeMapStyle(CCmdUI* pCmdUI) const;
    void OnViewFlameGraph();
    void OnUpdateViewFlameGraph(CCmdUI* pCmdUI) const;
    void OnViewSunburst();
    void OnUpdateViewSunburst(CCmdUI* pCmdUI) const;
    void OnViewTreeMapUseLogical();
    void OnViewTreeMapUsePhysical();
    void OnViewAbsolutePercentages() const;
    void OnViewShowFileTypes();
    void OnViewGroupUnregisteredTypes() const;
    void OnViewShowExtensionsOnTreeMap() const;
    void OnUpdateViewShowExtensionsOnTreeMap(CCmdUI* pCmdUI) const;
    void OnViewShowFolderFramesOnTreeMap() const;
    void OnUpdateViewShowFolderFramesOnTreeMap(CCmdUI* pCmdUI) const;
    void OnViewAllFiles() const { GetFileTabbedView()->SetActiveFileTreeView(); }
    void OnViewLargestFiles() const { GetFileTabbedView()->SetActiveTopView(); }
    void OnViewDuplicateFiles() const { GetFileTabbedView()->SetActiveDupeView(); }
    void OnViewSearchResults() const { GetFileTabbedView()->SetActiveSearchView(); }
    void OnViewToolBarSize(UINT commandId);
    void OnUpdateViewToolBarSize(CCmdUI* pCmdUI) const;
    void OnViewFontSize(UINT commandId);
    void OnUpdateViewFontSize(CCmdUI* pCmdUI) const;
    void OnAdvancedShadowCopy(UINT nID);
    void OnAdvancedDefrag(UINT nID);
    void OnAdvancedChkdsk(UINT nID);
    void OnToolsWatcher() const;
    void OnWatcherStart();
    void OnUpdateWatcherStart(CCmdUI* pCmdUI);
    void OnWatcherPause();
    void OnUpdateWatcherPause(CCmdUI* pCmdUI);
    void OnWatcherAutoScroll();
    void OnUpdateWatcherAutoScroll(CCmdUI* pCmdUI);
    void OnWatcherClear();
    void OnUpdateWatcherClear(CCmdUI* pCmdUI);
    void OnToolsPermissions() const;
    void OnUpdateToolsPermissions(CCmdUI* pCmdUI) const;
    void OnToolsStorageAnalytics() const;
    void OnUpdateToolsStorageAnalytics(CCmdUI* pCmdUI) const;
    void UpdateToolsMenu(CMenu* menu) const;
    void OnViewWindowLayout();
    void OnConfigure();
    void OnDestroy();
    LRESULT OnTaskButtonCreated(WPARAM, LPARAM);
    UINT OnPowerBroadcast(UINT, LPARAM);
    void OnSysColorChange();
    void OnSettingChange(UINT, LPCTSTR);
    LRESULT OnUahDrawMenu(WPARAM wParam, LPARAM lParam) const;
    void OnNcPaint();
    bool OnNcActivate(bool bActive);
    bool OnEraseBkgnd(CDC*) { return true; }
public:
    static CMainFrame* Get() { return s_Singleton; }
    void UpdateFrameTitleForScan(LPCWSTR scanName);
    void UpdateAllPanes(CWnd* sender, MODEL_CHANGE change, CItem* item) const;
    void RebuildToolBar(bool rebuildButtons = true);
    void SetWatcherToolBarButtons(bool visible, bool updateLayout = true);
    void RebuildLayout(bool resetPositions = false);
    bool CreateFromResource(UINT nIDResource) override;

private:
    void ApplyFontSize(int percent, bool rebuildToolBar = false);
    void ApplyWindowsTextScale();
    void BuildSplitterLayout(int topo, int perm, HWND hFTV, HWND hExtV, HWND hVisualization);
    void ConfigureSplitterCallbacks(int topo, int perm);
};

inline std::span<const RouteEntry> CSettingsSheet::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnCtlColor>(WM_CTLCOLOR),
        Route::Window<&OnEraseBkgnd>(WM_ERASEBKGND),
    };
    return entries;
}

inline std::span<const RouteEntry> CWdsSplitterWnd::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnSize>(WM_SIZE),
    };
    return entries;
}

inline std::span<const RouteEntry> CPacmanControl::Routes()
{
    static constexpr std::array entries
    {
        Route::Window<&OnPaint>(WM_PAINT),
        Route::Window<&OnCreate>(WM_CREATE),
        Route::Window<&OnEraseBkgnd>(WM_ERASEBKGND),
    };
    return entries;
}

inline std::span<const RouteEntry> CMainFrame::Routes()
{
    static constexpr std::array entries
    {
        Route::Command<&OnConfigure>(ID_CONFIGURE),
        Route::Command<&OnViewShowFileTypes>(ID_VIEW_SHOWFILETYPES),
        Route::Command<&OnViewGroupUnregisteredTypes>(ID_VIEW_GROUP_TYPES),
        Route::Command<&OnViewShowVisualization>(ID_VIEW_SHOWVISUALIZATION),
        Route::Command<&OnViewTreeMapStyle>(ID_VIEW_TREEMAP_ROWS, ID_VIEW_TREEMAP_MOORE),
        Route::Command<&OnViewFlameGraph>(ID_VIEW_FLAMEGRAPH),
        Route::Command<&OnViewSunburst>(ID_VIEW_SUNBURST),
        Route::Command<&OnViewTreeMapUseLogical>(ID_TREEMAP_LOGICAL_SIZE),
        Route::Command<&OnViewTreeMapUsePhysical>(ID_TREEMAP_PHYSICAL_SIZE),
        Route::Command<&OnViewAbsolutePercentages>(ID_VIEW_ABSOLUTE_PERCENTAGES),
        Route::Window<&OnEnterSizeMove>(WM_ENTERSIZEMOVE),
        Route::Window<&OnExitSizeMove>(WM_EXITSIZEMOVE),
        Route::Window<&OnCallbackRequest>(WM_CALLBACKUI),
        Route::Window<&OnUahDrawMenu>(DarkMode::WM_UAHDRAWMENU),
        Route::Window<&OnUahDrawMenu>(DarkMode::WM_UAHDRAWMENUITEM),
        Route::Registered<&OnTaskButtonCreated>(s_TaskBarMessage),
        Route::Update<&OnUpdateViewShowVisualization>(ID_VIEW_SHOWVISUALIZATION),
        Route::Update<&OnUpdateViewTreeMapStyle>(ID_VIEW_TREEMAP_ROWS, ID_VIEW_TREEMAP_MOORE),
        Route::Update<&OnUpdateViewFlameGraph>(ID_VIEW_FLAMEGRAPH),
        Route::Update<&OnUpdateViewSunburst>(ID_VIEW_SUNBURST),
        Route::Update<&OnUpdateViewShowFileTypes>(ID_VIEW_SHOWFILETYPES),
        Route::Update<&OnUpdateViewGroupUnregisteredTypes>(ID_VIEW_GROUP_TYPES),
        Route::Update<&OnUpdateTreeMapUseLogical>(ID_TREEMAP_LOGICAL_SIZE),
        Route::Update<&OnUpdateTreeMapUsePhysical>(ID_TREEMAP_PHYSICAL_SIZE),
        Route::Update<&OnUpdateViewAbsolutePercentages>(ID_VIEW_ABSOLUTE_PERCENTAGES),
        Route::Command<&OnViewShowExtensionsOnTreeMap>(ID_TREEMAP_SHOW_EXTENSIONS),
        Route::Update<&OnUpdateViewShowExtensionsOnTreeMap>(ID_TREEMAP_SHOW_EXTENSIONS),
        Route::Command<&OnViewShowFolderFramesOnTreeMap>(ID_TREEMAP_SHOW_FOLDER_FRAMES),
        Route::Update<&OnUpdateViewShowFolderFramesOnTreeMap>(ID_TREEMAP_SHOW_FOLDER_FRAMES),
        Route::Update<&OnUpdateViewShowWatcher>(ID_TOOLS_WATCHER),
        Route::Window<&OnClose>(WM_CLOSE),
        Route::Window<&OnCreate>(WM_CREATE),
        Route::Window<&OnDestroy>(WM_DESTROY),
        Route::Window<&OnInitMenuPopup>(WM_INITMENUPOPUP),
        Route::Window<&OnMenuCommand>(WM_MENUCOMMAND),
        Route::Window<&OnSize>(WM_SIZE),
        Route::Window<&OnSysColorChange>(WM_SYSCOLORCHANGE),
        Route::Window<&OnSettingChange>(WM_SETTINGCHANGE),
        Route::Window<&OnPowerBroadcast>(WM_POWERBROADCAST),
        Route::Window<&OnTimer>(WM_TIMER),
        Route::Window<&OnNcPaint>(WM_NCPAINT),
        Route::Window<&OnNcActivate>(WM_NCACTIVATE),
        Route::Window<&OnEraseBkgnd>(WM_ERASEBKGND),
        Route::Window<&OnSetFocus>(WM_SETFOCUS),
        Route::Window<&OnKeyDown>(WM_KEYDOWN),
        Route::Command<&OnViewAllFiles>(ID_VIEW_ALL_FILES),
        Route::Command<&OnViewLargestFiles>(ID_VIEW_LARGEST_FILES),
        Route::Command<&OnViewDuplicateFiles>(ID_VIEW_DUPLICATE_FILES),
        Route::Command<&OnViewSearchResults>(ID_VIEW_SEARCH_RESULTS),
        Route::Command<&OnViewToolBarSize>(ID_VIEW_TOOLBAR_SIZE_100, ID_VIEW_TOOLBAR_SIZE_USE_WINDOWS),
        Route::Update<&OnUpdateViewToolBarSize>(ID_VIEW_TOOLBAR_SIZE_100, ID_VIEW_TOOLBAR_SIZE_USE_WINDOWS),
        Route::Command<&OnViewFontSize>(ID_VIEW_FONT_SIZE_100, ID_VIEW_FONT_SIZE_USE_WINDOWS),
        Route::Update<&OnUpdateViewFontSize>(ID_VIEW_FONT_SIZE_100, ID_VIEW_FONT_SIZE_USE_WINDOWS),
        Route::Command<&OnAdvancedShadowCopy>(ID_TOOLS_SHADOW_COPY_BASE, ID_TOOLS_SHADOW_COPY_BASE + wds::alphaSize),
        Route::Command<&OnAdvancedDefrag>(ID_TOOLS_DEFRAG_BASE, ID_TOOLS_DEFRAG_BASE + wds::alphaSize),
        Route::Command<&OnAdvancedChkdsk>(ID_TOOLS_CHKDSK_BASE, ID_TOOLS_CHKDSK_BASE + wds::alphaSize),
        Route::Command<&OnToolsWatcher>(ID_TOOLS_WATCHER),
        Route::Command<&OnWatcherStart>(ID_WATCHER_START),
        Route::Update<&OnUpdateWatcherStart>(ID_WATCHER_START),
        Route::Command<&OnWatcherPause>(ID_WATCHER_PAUSE),
        Route::Update<&OnUpdateWatcherPause>(ID_WATCHER_PAUSE),
        Route::Command<&OnWatcherAutoScroll>(ID_WATCHER_AUTOSCROLL),
        Route::Update<&OnUpdateWatcherAutoScroll>(ID_WATCHER_AUTOSCROLL),
        Route::Command<&OnWatcherClear>(ID_WATCHER_CLEAR),
        Route::Update<&OnUpdateWatcherClear>(ID_WATCHER_CLEAR),
        Route::Command<&OnToolsPermissions>(ID_TOOLS_PERMISSIONS),
        Route::Update<&OnUpdateToolsPermissions>(ID_TOOLS_PERMISSIONS),
        Route::Command<&OnToolsStorageAnalytics>(ID_TOOLS_STORAGE_ANALYTICS),
        Route::Update<&OnUpdateToolsStorageAnalytics>(ID_TOOLS_STORAGE_ANALYTICS),
        Route::Command<&OnViewWindowLayout>(ID_VIEW_WINDOW_LAYOUT),
    };
    return entries;
}
