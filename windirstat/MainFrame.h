// MainFrame.h - Declaration of CMySplitterWnd and CMainFrame
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
//

#pragma once

#include "PacMan.h"
#include "Item.h"
#include <common/Constants.h>

#include <functional>

class CMySplitterWnd;
class CMainFrame;

class CDirStatView;
class CGraphView;
class CTypeView;

//
// The "logical focus" can be
// - on the Directory List
// - on the Extension List
// Although these windows can lose the real focus, for instance
// when a dialog box is opened, the logical focus will not be lost.
//
enum LOGICAL_FOCUS
{
    LF_NONE,
    LF_DIRECTORYLIST,
    LF_EXTENSIONLIST
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

    bool m_restartApplication = false; // [out]

protected:
    BOOL OnCommand(WPARAM wParam, LPARAM lParam) override;

    bool m_languageChanged = false;
    bool m_alreadyAsked = false;
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
    double m_splitterPos{0};    // Current split ratio
    bool m_wasTrackedByUser;    // True as soon as user has modified the splitter position
    double * m_userSplitterPos; // Split ratio as set by the user

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
    CPacman m_pacman;

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
    static UINT s_taskBarMessage;
    static CMainFrame* _theFrame;
    CMainFrame(); // Created by MFC only
    DECLARE_DYNCREATE(CMainFrame)

    static CMainFrame* GetTheFrame();
    ~CMainFrame() override;
    void InitialShowWindow();
    void InvokeInMessageThread(std::function<void()> callback);

    void RestoreGraphView();
    void RestoreTypeView();
    void MinimizeGraphView();
    void MinimizeTypeView();
    void CopyToClipboard(LPCWSTR psz);

    CDirStatView* GetDirStatView() const;
    CGraphView* GetGraphView() const;
    CTypeView* GetTypeView() const;

    void CreateProgress(ULONGLONG range);
    void SetProgressPos(ULONGLONG pos);
    void SetProgressComplete();
    void SuspendState(bool suspend);
    bool IsScanSuspended() const;

    void UpdateProgress();
    void AppendUserDefinedCleanups(CMenu* menu) const;

    void SetLogicalFocus(LOGICAL_FOCUS lf);
    LOGICAL_FOCUS GetLogicalFocus() const;
    void MoveFocus(LOGICAL_FOCUS lf);

    void SetMessageText(const CStringW& text) { SetStatusPaneText(0, text); }
    void SetSelectionMessageText();

    static void QueryRecycleBin(ULONGLONG& items, ULONGLONG& bytes);

protected:
    BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) override;
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;

    void CreateStatusProgress();
    void CreatePacmanProgress();
    void DestroyProgress();

    void SetStatusPaneText(int pos, const CStringW& text);
    void UpdateCleanupMenu(CMenu* menu);

    bool m_progressVisible = false; // True while progress must be shown (either pacman or progress bar)
    bool m_scanSuspend = false;     // True if the scan has been suspended
    ULONGLONG m_progressRange = 0;  // Progress range. A range of 0 means Pacman should be used.
    ULONGLONG m_progressPos = 0;    // Progress position (<= progressRange, or an item count in case of m_progressRang == 0)
    CItem* m_workingItem = nullptr;

    CMySplitterWnd m_wndSubSplitter; // Contains the two upper views
    CMySplitterWnd m_wndSplitter;    // Contains (a) m_wndSubSplitter and (b) the graph view.

    CMFCStatusBar m_wndStatusBar; // Status bar
    CMFCToolBar m_wndToolBar;     // Tool bar
    CProgressCtrl m_progress;     // Progress control. Is Create()ed and Destroy()ed again every time.
    CPacmanControl m_pacman;      // Static control for Pacman.

    LOGICAL_FOCUS m_logicalFocus = LF_NONE; // Which view has the logical focus
    CDeadFocusWnd m_wndDeadFocus; // Zero-size window which holds the focus if logical focus is "NONE"

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
    afx_msg void OnUpdateViewShowtreemap(CCmdUI* pCmdUI);
    afx_msg void OnViewShowtreemap();
    afx_msg void OnUpdateViewShowFileTypes(CCmdUI* pCmdUI);
    afx_msg void OnViewShowFileTypes();
    afx_msg void OnConfigure();
    afx_msg void OnDestroy();
    afx_msg LRESULT OnTaskButtonCreated(WPARAM, LPARAM);

public:
    afx_msg void OnSysColorChange();
    BOOL LoadFrame(UINT nIDResource, DWORD dwDefaultStyle = WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, CWnd* pParentWnd = nullptr, CCreateContext* pContext = nullptr) override;
};
