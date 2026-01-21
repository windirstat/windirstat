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

class CWdsSplitterWnd;
class CMainFrame;

class CFileTreeView;
class CTreeMapView;
class CExtensionView;

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
    LF_EXTLIST,
};

//
// COptionsPropertySheet.
//
class COptionsPropertySheet final : public CMFCPropertySheet
{
    DECLARE_DYNAMIC(COptionsPropertySheet)

    COptionsPropertySheet();
    void SetRestartRequired(bool changed);
    BOOL OnInitDialog() override;

    bool m_restartApplication = false; // [out]

protected:
    BOOL OnCommand(WPARAM wParam, LPARAM lParam) override;
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

    bool m_restartRequest = false;
    bool m_alreadyAsked = false;

    DECLARE_MESSAGE_MAP()
};

//
// CWdsSplitterWnd. A CSplitterWnd with 2 columns or rows, which
// knows about the current split ratio and retains it even when resized.
//
class CWdsSplitterWnd final : public CSplitterWndEx
{
public:
    CWdsSplitterWnd(double * splitterPos);
    void StopTracking(BOOL bAccept) override;
    void SetSplitterPos(double pos);
    void RestoreSplitterPos(double posIfVirgin);

protected:
    double m_splitterPos{0};    // Current split ratio
    bool m_wasTrackedByUser;    // True as soon as user has modified the splitter position
    double * m_userSplitterPos; // Split ratio as set by the user

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSize(UINT nType, int cx, int cy);
};

//
// CPacmanControl. Pacman on the status bar.
//
class CPacmanControl final : public CWnd
{
public:
    CPacmanControl() = default;
    void Drive();
    void Start();
    void Stop();

protected:
    CPacman m_pacman;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
};

//
// CDeadFocusWnd. The focus in WinDirStat can be on
// - the directory list
// - the extension list,
// - or none of them. In that case the focus resides on
//   an invisible (zero-size) child of CMainFrame.
// Pressing VK_TAB while this window has focus moves focus to the
// directory list.
class CDeadFocusWnd final : public CWnd
{
public:
    CDeadFocusWnd() = default;
    void Create(CWnd* parent);
    ~CDeadFocusWnd() override;

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
};

//
// CMainFrame. The main application window.
//
class CMainFrame final : public CFrameWndEx
{
protected:
    static constexpr DWORD WM_CALLBACKUI = WM_USER + 1;
    static UINT s_TaskBarMessage;
    static CMainFrame* s_Singleton;

    CMainFrame();
    ~CMainFrame() override;
    DECLARE_DYNCREATE(CMainFrame)

    void InitialShowWindow();
    void InvokeInMessageThread(std::function<void()> callback) const;

    void RestoreTreeMapView();
    void RestoreExtensionView();
    void MinimizeTreeMapView();
    void MinimizeExtensionView();
    void CopyToClipboard(const std::wstring& psz);

    // Used for storing and retrieving the various views
    CFileTabbedView* m_fileTabbedView = nullptr;
    CExtensionView* m_extensionView = nullptr;
    CTreeMapView* m_treeMapView = nullptr;
    CFileTreeView* GetFileTreeView() const { return m_fileTabbedView->GetFileTreeView(); }
    CFileTopView* GetFileTopView() const { return m_fileTabbedView->GetFileTopView(); }
    CFileDupeView* GetFileDupeView() const { return m_fileTabbedView->GetFileDupeView(); }
    CFileSearchView* GetFileSearchView() const { return m_fileTabbedView->GetFileSearchView(); }
    CFileTabbedView* GetFileTabbedView() const { return m_fileTabbedView; }
    CTreeMapView* GetTreeMapView() const { return m_treeMapView; }
    CExtensionView* GetExtensionView() const { return m_extensionView; }

    void CreateProgress(ULONGLONG range);
    void SetProgressPos(ULONGLONG pos);
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

    BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) override;
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;

    void CreateStatusProgress();
    void CreatePacmanProgress();
    void DestroyProgress();

    void SetStatusPaneText(const CDC& cdc, int pos, const std::wstring& text, int minWidth = 0);
    void UpdateCleanupMenu(CMenu* menu, bool triggerAsync = true);

    UINT_PTR m_timer = 0;           // Timer for updating the display
    bool m_progressVisible = false; // True while progress must be shown (either pacman or progress bar)
    bool m_scanSuspend = false;     // True if the scan has been suspended
    bool m_shuttingDown = false;    // Marks the process is shutting down so we can exit timers
    ULONGLONG m_progressRange = 0;  // Progress range. A range of 0 means Pacman should be used.
    ULONGLONG m_progressPos = 0;    // Progress position (<= progressRange, or an item count in case of m_progressRang == 0)
    CItem* m_workingItem = nullptr;

    CWdsSplitterWnd m_subSplitter; // Contains the two upper views
    CWdsSplitterWnd m_splitter;    // Contains (a) m_wndSubSplitter and (b) the graph view.

    CMFCStatusBar m_wndStatusBar; // Status bar
    CMFCToolBar m_wndToolBar;     // Toolbar
    CProgressCtrl m_progress;     // Progress control. Is Create()ed and Destroy()ed again every time.
    CPacmanControl m_pacman;      // Static control for Pacman.
    CMFCToolBarImages m_images;   // Toolbar images
    LOGICAL_FOCUS m_logicalFocus = LF_NONE; // Which view has the logical focus
    CDeadFocusWnd m_wndDeadFocus; // Zero-size window which holds the focus if logical focus is "NONE"

    CComPtr<ITaskbarList3> m_taskbarList;
    TBPFLAG m_taskbarButtonState = TBPF_INDETERMINATE;
    TBPFLAG m_taskbarButtonPreviousState = TBPF_INDETERMINATE;

    // Cached values for cleanup menu queries (updated asynchronously)
    ULONGLONG m_recycleBinItems = 0;
    ULONGLONG m_recycleBinBytes = 0;
    ULONGLONG m_shadowCopyCount = 0;
    ULONGLONG m_shadowCopyBytes = 0;

    DECLARE_MESSAGE_MAP()
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg LRESULT OnEnterSizeMove(WPARAM, LPARAM);
    afx_msg LRESULT OnExitSizeMove(WPARAM, LPARAM);
    afx_msg LRESULT OnCallbackRequest(WPARAM, LPARAM lParam);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnClose();
    afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
    afx_msg void OnUpdateEnableControl(CCmdUI* pCmdUI);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnUpdateViewShowTreeMap(CCmdUI* pCmdUI);
    afx_msg void OnUpdateTreeMapUseLogical(CCmdUI* pCmdUI);
    afx_msg void OnViewShowTreeMap();
    afx_msg void OnViewTreeMapUseLogical();
    afx_msg void OnUpdateViewShowFileTypes(CCmdUI* pCmdUI);
    afx_msg void OnViewShowFileTypes();
    afx_msg void OnViewAllFiles() { GetFileTabbedView()->SetActiveFileTreeView(); }
    afx_msg void OnViewLargestFiles() { GetFileTabbedView()->SetActiveTopView(); }
    afx_msg void OnViewDuplicateFiles() { GetFileTabbedView()->SetActiveDupeView(); }
    afx_msg void OnViewSearchResults() { GetFileTabbedView()->SetActiveSearchView(); }
    afx_msg void OnConfigure();
    afx_msg void OnDestroy();
    afx_msg LRESULT OnTaskButtonCreated(WPARAM, LPARAM);
    afx_msg void OnSysColorChange();
    afx_msg LRESULT OnUahDrawMenu(WPARAM wParam, LPARAM lParam);
    afx_msg void OnNcPaint();
    afx_msg BOOL OnNcActivate(BOOL bActive);
public:
    static CMainFrame* Get() { return s_Singleton; }
    BOOL LoadFrame(UINT nIDResource, DWORD dwDefaultStyle = WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, CWnd* pParentWnd = NULL, CCreateContext* pContext = NULL) override;
};
