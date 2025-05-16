// MainFrame.h - Declaration of CMySplitterWnd and CMainFrame
//
// WinDirStat - Directory Statistics
// Copyright © WinDirStat Team
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

#include "PacMan.h"
#include "Item.h"
#include "FileTabbedView.h"

#include <functional>

class CMySplitterWnd;
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
enum LOGICAL_FOCUS : std::uint8_t
{
    LF_NONE = 0,
    LF_FILETREE = 1 << 0,
    LF_DUPELIST = 1 << 1,
    LF_TOPLIST = 1 << 2,
    LF_EXTLIST = 1 << 3,
};

//
// COptionsPropertySheet. The options dialog.
//
class COptionsPropertySheet final : public CPropertySheet
{
    DECLARE_DYNAMIC(COptionsPropertySheet)

    COptionsPropertySheet();
    void SetLanguageChanged(bool changed);
    BOOL OnInitDialog() override;

    bool m_RestartApplication = false; // [out]

protected:
    BOOL OnCommand(WPARAM wParam, LPARAM lParam) override;

    bool m_LanguageChanged = false;
    bool m_AlreadyAsked = false;
};

//
// CMySplitterWnd. A CSplitterWnd with 2 columns or rows, which
// knows about the current split ratio and retains it even when resized.
//
class CMySplitterWnd final : public CSplitterWnd
{
public:
    CMySplitterWnd(double * splitterPos);
    void StopTracking(BOOL bAccept) override;
    void SetSplitterPos(double pos);
    void RestoreSplitterPos(double posIfVirgin);

protected:
    double m_SplitterPos{0};    // Current split ratio
    bool m_WasTrackedByUser;    // True as soon as user has modified the splitter position
    double * m_UserSplitterPos; // Split ratio as set by the user

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSize(UINT nType, int cx, int cy);

public:
    afx_msg void OnDestroy();
};

//
// CPacmanControl. Pacman on the status bar.
//
class CPacmanControl final : public CStatic
{
public:
    CPacmanControl();
    void Drive();
    void Start();
    void Stop();

protected:
    CPacman m_Pacman;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};

//
// CDeadFocusWnd. The focus in Windirstat can be on
// - the directory list
// - the extension list,
// - or none of them. In this case the focus lies on
//   an invisible (zero-size) child of CMainFrame.
// On VK_TAB CDeadFocusWnd moves the focus to the
// directory list then.
//
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
    CFileTabbedView* m_FileTabbedView = nullptr;
    CExtensionView* m_ExtensionView = nullptr;
    CTreeMapView* m_TreeMapView = nullptr;
    CFileTreeView* GetFileTreeView() const { return m_FileTabbedView->GetFileTreeView(); }
    CFileTopView* GetFileTopView() const { return m_FileTabbedView->GetFileTopView(); }
    CFileDupeView* GetFileDupeView() const { return m_FileTabbedView->GetFileDupeView(); }
    CFileTabbedView* GetFileTabbedView() const { return m_FileTabbedView; }
    CTreeMapView* GetTreeMapView() const { return m_TreeMapView; }
    CExtensionView* GetExtensionView() const { return m_ExtensionView; }

    void CreateProgress(ULONGLONG range);
    void SetProgressPos(ULONGLONG pos);
    void SetProgressComplete();
    void SuspendState(bool suspend);
    bool IsScanSuspended() const;

    void UpdateProgress();
    void UpdateDynamicMenuItems(CMenu* menu) const;
    std::vector<CItem*> GetAllSelectedInFocus() const;
    std::pair<CMenu*, int> LocateNamedMenu(const CMenu* menu, const std::wstring& subMenuText) const;

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

    void SetStatusPaneText(int pos, const std::wstring& text, int minWidth = 0);
    void UpdateCleanupMenu(CMenu* menu) const;

    UINT_PTR m_Timer = 0;           // Timer for updating the display
    bool m_ProgressVisible = false; // True while progress must be shown (either pacman or progress bar)
    bool m_ScanSuspend = false;     // True if the scan has been suspended
    ULONGLONG m_ProgressRange = 0;  // Progress range. A range of 0 means Pacman should be used.
    ULONGLONG m_ProgressPos = 0;    // Progress position (<= progressRange, or an item count in case of m_ProgressRang == 0)
    CItem* m_WorkingItem = nullptr;

    CMySplitterWnd m_SubSplitter; // Contains the two upper views
    CMySplitterWnd m_Splitter;    // Contains (a) m_WndSubSplitter and (b) the graph view.

    CMFCStatusBar m_WndStatusBar; // Status bar
    CMFCToolBar m_WndToolBar;     // Tool bar
    CProgressCtrl m_Progress;     // Progress control. Is Create()ed and Destroy()ed again every time.
    CPacmanControl m_Pacman;      // Static control for Pacman.
    CMFCToolBarImages m_Images;   // Tool bar images
    LOGICAL_FOCUS m_LogicalFocus = LF_NONE; // Which view has the logical focus
    CDeadFocusWnd m_WndDeadFocus; // Zero-size window which holds the focus if logical focus is "NONE"

    CComPtr<ITaskbarList3> m_TaskbarList;
    TBPFLAG m_TaskbarButtonState = TBPF_INDETERMINATE;
    TBPFLAG m_TaskbarButtonPreviousState = TBPF_INDETERMINATE;

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
    afx_msg void OnViewShowtreemap();
    afx_msg void OnUpdateViewShowFileTypes(CCmdUI* pCmdUI);
    afx_msg void OnViewShowFileTypes();
    afx_msg void OnConfigure();
    afx_msg void OnDestroy();
    afx_msg LRESULT OnTaskButtonCreated(WPARAM, LPARAM);
    afx_msg void OnSysColorChange();

public:
    static CMainFrame* Get() { return s_Singleton; }
    BOOL LoadFrame(UINT nIDResource, DWORD dwDefaultStyle = WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, CWnd* pParentWnd = NULL, CCreateContext* pContext = NULL) override;
};
