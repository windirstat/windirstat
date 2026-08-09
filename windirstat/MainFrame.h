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
    void SetRestartRequired(bool changed);
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
    bool IsScanSuspended() const;

    void UpdateProgress();
    void UpdateDynamicMenuItems(CMenu* menu) const;
    std::pair<CMenu*, int> LocateNamedMenu(const CMenu* menu, const std::wstring& subMenuText, bool removeItems = true) const;

    void SetLogicalFocus(LOGICAL_FOCUS lf);
    LOGICAL_FOCUS GetLogicalFocus() const;
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
    CSize m_defaultButtonSize;  // Toolbar button size at creation (pre-SetMetrics, DPI-scaled)
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
    void OnUpdateEnableControl(CCmdUI* pCmdUI);
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
    void OnViewLargeToolBar();
    void OnUpdateViewLargeToolBar(CCmdUI* pCmdUI) const;
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
    LRESULT OnUahDrawMenu(WPARAM wParam, LPARAM lParam) const;
    void OnNcPaint();
    bool OnNcActivate(bool bActive);
    bool OnEraseBkgnd(CDC* pDC);
public:
    static CMainFrame* Get() { return s_Singleton; }
    void UpdateFrameTitleForScan(LPCWSTR scanName);
    void UpdateAllPanes(CWnd* sender, MODEL_CHANGE change, CItem* item) const;
    void RebuildToolBar();
    void SetWatcherToolBarButtons(bool visible);
    void RebuildLayout(bool resetPositions = false);
    bool CreateFromResource(UINT nIDResource) override;

private:
    void BuildSplitterLayout(int topo, int perm, HWND hFTV, HWND hExtV, HWND hVisualization);
    void ConfigureSplitterCallbacks(int topo, int perm);
};

inline std::span<const RouteEntry> CSettingsSheet::Routes()
{
    using ThisClass = CSettingsSheet;
    static constexpr std::array entries
    {
        Route::Window<&ThisClass::OnCtlColor>(WM_CTLCOLOR),
        Route::Window<&ThisClass::OnEraseBkgnd>(WM_ERASEBKGND),
    };
    return entries;
}

inline std::span<const RouteEntry> CWdsSplitterWnd::Routes()
{
    using ThisClass = CWdsSplitterWnd;
    static constexpr std::array entries
    {
        Route::Window<&ThisClass::OnSize>(WM_SIZE),
    };
    return entries;
}

inline std::span<const RouteEntry> CPacmanControl::Routes()
{
    using ThisClass = CPacmanControl;
    static constexpr std::array entries
    {
        Route::Window<&ThisClass::OnPaint>(WM_PAINT),
        Route::Window<&ThisClass::OnCreate>(WM_CREATE),
        Route::Window<&ThisClass::OnEraseBkgnd>(WM_ERASEBKGND),
    };
    return entries;
}

inline std::span<const RouteEntry> CMainFrame::Routes()
{
    using ThisClass = CMainFrame;
    static constexpr std::array entries
    {
        Route::Command<&ThisClass::OnConfigure>(ID_CONFIGURE),
        Route::Command<&ThisClass::OnViewShowFileTypes>(ID_VIEW_SHOWFILETYPES),
        Route::Command<&ThisClass::OnViewGroupUnregisteredTypes>(ID_VIEW_GROUP_TYPES),
        Route::Command<&ThisClass::OnViewShowVisualization>(ID_VIEW_SHOWVISUALIZATION),
        Route::Command<&ThisClass::OnViewTreeMapStyle>(ID_VIEW_TREEMAP_ROWS, ID_VIEW_TREEMAP_MOORE),
        Route::Command<&ThisClass::OnViewFlameGraph>(ID_VIEW_FLAMEGRAPH),
        Route::Command<&ThisClass::OnViewSunburst>(ID_VIEW_SUNBURST),
        Route::Command<&ThisClass::OnViewTreeMapUseLogical>(ID_TREEMAP_LOGICAL_SIZE),
        Route::Command<&ThisClass::OnViewTreeMapUsePhysical>(ID_TREEMAP_PHYSICAL_SIZE),
        Route::Command<&ThisClass::OnViewAbsolutePercentages>(ID_VIEW_ABSOLUTE_PERCENTAGES),
        Route::Window<&ThisClass::OnEnterSizeMove>(WM_ENTERSIZEMOVE),
        Route::Window<&ThisClass::OnExitSizeMove>(WM_EXITSIZEMOVE),
        Route::Window<&ThisClass::OnCallbackRequest>(WM_CALLBACKUI),
        Route::Window<&ThisClass::OnUahDrawMenu>(DarkMode::WM_UAHDRAWMENU),
        Route::Window<&ThisClass::OnUahDrawMenu>(DarkMode::WM_UAHDRAWMENUITEM),
        Route::Registered<&ThisClass::OnTaskButtonCreated>(s_TaskBarMessage),
        Route::Update<&ThisClass::OnUpdateViewShowVisualization>(ID_VIEW_SHOWVISUALIZATION),
        Route::Update<&ThisClass::OnUpdateViewTreeMapStyle>(ID_VIEW_TREEMAP_ROWS, ID_VIEW_TREEMAP_MOORE),
        Route::Update<&ThisClass::OnUpdateViewFlameGraph>(ID_VIEW_FLAMEGRAPH),
        Route::Update<&ThisClass::OnUpdateViewSunburst>(ID_VIEW_SUNBURST),
        Route::Update<&ThisClass::OnUpdateViewShowFileTypes>(ID_VIEW_SHOWFILETYPES),
        Route::Update<&ThisClass::OnUpdateViewGroupUnregisteredTypes>(ID_VIEW_GROUP_TYPES),
        Route::Update<&ThisClass::OnUpdateTreeMapUseLogical>(ID_TREEMAP_LOGICAL_SIZE),
        Route::Update<&ThisClass::OnUpdateTreeMapUsePhysical>(ID_TREEMAP_PHYSICAL_SIZE),
        Route::Update<&ThisClass::OnUpdateViewAbsolutePercentages>(ID_VIEW_ABSOLUTE_PERCENTAGES),
        Route::Command<&ThisClass::OnViewShowExtensionsOnTreeMap>(ID_TREEMAP_SHOW_EXTENSIONS),
        Route::Update<&ThisClass::OnUpdateViewShowExtensionsOnTreeMap>(ID_TREEMAP_SHOW_EXTENSIONS),
        Route::Command<&ThisClass::OnViewShowFolderFramesOnTreeMap>(ID_TREEMAP_SHOW_FOLDER_FRAMES),
        Route::Update<&ThisClass::OnUpdateViewShowFolderFramesOnTreeMap>(ID_TREEMAP_SHOW_FOLDER_FRAMES),
        Route::Update<&ThisClass::OnUpdateViewShowWatcher>(ID_TOOLS_WATCHER),
        Route::Window<&ThisClass::OnClose>(WM_CLOSE),
        Route::Window<&ThisClass::OnCreate>(WM_CREATE),
        Route::Window<&ThisClass::OnDestroy>(WM_DESTROY),
        Route::Window<&ThisClass::OnInitMenuPopup>(WM_INITMENUPOPUP),
        Route::Window<&ThisClass::OnSize>(WM_SIZE),
        Route::Window<&ThisClass::OnSysColorChange>(WM_SYSCOLORCHANGE),
        Route::Window<&ThisClass::OnPowerBroadcast>(WM_POWERBROADCAST),
        Route::Window<&ThisClass::OnTimer>(WM_TIMER),
        Route::Window<&ThisClass::OnNcPaint>(WM_NCPAINT),
        Route::Window<&ThisClass::OnNcActivate>(WM_NCACTIVATE),
        Route::Window<&ThisClass::OnEraseBkgnd>(WM_ERASEBKGND),
        Route::Window<&ThisClass::OnSetFocus>(WM_SETFOCUS),
        Route::Window<&ThisClass::OnKeyDown>(WM_KEYDOWN),
        Route::Command<&CMainFrame::OnViewAllFiles>(ID_VIEW_ALL_FILES),
        Route::Command<&CMainFrame::OnViewLargestFiles>(ID_VIEW_LARGEST_FILES),
        Route::Command<&CMainFrame::OnViewDuplicateFiles>(ID_VIEW_DUPLICATE_FILES),
        Route::Command<&CMainFrame::OnViewSearchResults>(ID_VIEW_SEARCH_RESULTS),
        Route::Command<&CMainFrame::OnViewLargeToolBar>(ID_VIEW_LARGE_TOOLBAR),
        Route::Update<&CMainFrame::OnUpdateViewLargeToolBar>(ID_VIEW_LARGE_TOOLBAR),
        Route::Command<&ThisClass::OnAdvancedShadowCopy>(ID_TOOLS_SHADOW_COPY_BASE, ID_TOOLS_SHADOW_COPY_BASE + wds::alphaSize),
        Route::Command<&ThisClass::OnAdvancedDefrag>(ID_TOOLS_DEFRAG_BASE, ID_TOOLS_DEFRAG_BASE + wds::alphaSize),
        Route::Command<&ThisClass::OnAdvancedChkdsk>(ID_TOOLS_CHKDSK_BASE, ID_TOOLS_CHKDSK_BASE + wds::alphaSize),
        Route::Command<&CMainFrame::OnToolsWatcher>(ID_TOOLS_WATCHER),
        Route::Command<&CMainFrame::OnWatcherStart>(ID_WATCHER_START),
        Route::Update<&CMainFrame::OnUpdateWatcherStart>(ID_WATCHER_START),
        Route::Command<&CMainFrame::OnWatcherPause>(ID_WATCHER_PAUSE),
        Route::Update<&CMainFrame::OnUpdateWatcherPause>(ID_WATCHER_PAUSE),
        Route::Command<&CMainFrame::OnWatcherAutoScroll>(ID_WATCHER_AUTOSCROLL),
        Route::Update<&CMainFrame::OnUpdateWatcherAutoScroll>(ID_WATCHER_AUTOSCROLL),
        Route::Command<&CMainFrame::OnWatcherClear>(ID_WATCHER_CLEAR),
        Route::Update<&CMainFrame::OnUpdateWatcherClear>(ID_WATCHER_CLEAR),
        Route::Command<&CMainFrame::OnToolsPermissions>(ID_TOOLS_PERMISSIONS),
        Route::Update<&ThisClass::OnUpdateToolsPermissions>(ID_TOOLS_PERMISSIONS),
        Route::Command<&CMainFrame::OnToolsStorageAnalytics>(ID_TOOLS_STORAGE_ANALYTICS),
        Route::Update<&CMainFrame::OnUpdateToolsStorageAnalytics>(ID_TOOLS_STORAGE_ANALYTICS),
        Route::Command<&CMainFrame::OnViewWindowLayout>(ID_VIEW_WINDOW_LAYOUT),
    };
    return entries;
}
