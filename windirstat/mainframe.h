// mainframe.h - Declaration of CMySplitterWnd and CMainFrame
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
// Author(s): - bseifert -> http://windirstat.info/contact/bernhard/
//            - assarbad -> http://windirstat.info/contact/oliver/
//

#ifndef __WDS_MAINFRAME_H__
#define __WDS_MAINFRAME_H__
#pragma once

#include "pacman.h"
#include <common/wds_constants.h>

class CMySplitterWnd;
class CMainFrame;

class CDirstatView;
class CGraphView;
class CTypeView;

//
// The "logical focus" can be
// - on the Directory List
// - on the Extension List
// Although these windows can loose the real focus, for instance
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
class COptionsPropertySheet: public CPropertySheet
{
    DECLARE_DYNAMIC(COptionsPropertySheet)

public:
    COptionsPropertySheet();
    void SetLanguageChanged(bool changed);
    virtual BOOL OnInitDialog();

    bool m_restartApplication;  // [out]

protected:
    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

    bool m_languageChanged;
    bool m_alreadyAsked;
};


//
// CMySplitterWnd. A CSplitterWnd with 2 columns or rows, which
// knows about the current split ratio and retains it even when resized.
//
class CMySplitterWnd: public CSplitterWnd
{
public:
    CMySplitterWnd(LPCTSTR name);
    virtual void StopTracking(BOOL bAccept);
    double GetSplitterPos();
    void SetSplitterPos(double pos);
    void RestoreSplitterPos(double posIfVirgin);

protected:
    CString m_persistenceName;  // Name of object for CPersistence
    double m_splitterPos;       // Current split ratio
    bool m_wasTrackedByUser;    // True as soon as user has modified the splitter position
    double m_userSplitterPos;   // Split ratio as set by the user

    DECLARE_MESSAGE_MAP()
    afx_msg void OnSize(UINT nType, int cx, int cy);
public:
    afx_msg void OnDestroy();
};

//
// CPacmanControl. Pacman on the status bar.
//
class CPacmanControl: public CStatic
{
public:
    CPacmanControl();
    void Drive(ULONGLONG readJobs);
    void Start(bool start);

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
class CDeadFocusWnd: public CWnd
{
public:
    CDeadFocusWnd();
    void Create(CWnd *parent);
    ~CDeadFocusWnd();

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
};


//
// CMainFrame. The main application window.
//
class CMainFrame: public CFrameWnd
{
protected:
    static UINT CMainFrame::s_taskBarMessage;
    static CMainFrame *_theFrame;
    CMainFrame();   // Created by MFC only
    DECLARE_DYNCREATE(CMainFrame)

public:
    static CMainFrame *GetTheFrame();
    virtual ~CMainFrame();
    void InitialShowWindow();

    void RestoreGraphView();
    void RestoreTypeView();
    void MinimizeGraphView();
    void MinimizeTypeView();
    void CopyToClipboard(LPCTSTR psz);

    CDirstatView *GetDirstatView();
    CGraphView *GetGraphView();
    CTypeView *GetTypeView();

    void ShowProgress(ULONGLONG range);
    void HideProgress();
    void SetProgressPos(ULONGLONG pos);
    void SetProgressPos100();
    bool IsProgressSuspended();
    void DrivePacman();

    void UpdateProgress();
    void AppendUserDefinedCleanups(CMenu *menu);

    void SetLogicalFocus(LOGICAL_FOCUS lf);
    LOGICAL_FOCUS GetLogicalFocus();
    void MoveFocus(LOGICAL_FOCUS lf);

    void SetSelectionMessageText();

    static void queryRecycleBin(ULONGLONG& items, ULONGLONG& bytes);

protected:
    virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    void MakeSaneShowCmd(UINT& u);

    void CreateStatusProgress();
    void CreatePacmanProgress();
    void CreateSuspendButton(CRect& rc);
    void DestroyProgress();

    void UpdateCleanupMenu(CMenu *menu);

    bool m_progressVisible;     // True while progress must be shown (either pacman or progress bar)
    ULONGLONG m_progressRange;  // Progress range. A range of 0 means that we have no range available. In this case we should display pacman.
    ULONGLONG m_progressPos;    // Progress position (<= progressRange, or an item count in case of m_progressRang == 0)

    CMySplitterWnd m_wndSubSplitter;    // Contains the two upper views
    CMySplitterWnd m_wndSplitter;       // Contains (a) m_wndSubSplitter and (b) the graph view.

    CStatusBar  m_wndStatusBar;     // Status bar
    CToolBar    m_wndToolBar;       // Tool bar
    CProgressCtrl   m_progress;     // Progress control. Is Create()ed and Destroy()ed again every time.
    CPacmanControl  m_pacman;       // Static control for Pacman.
    CButton         m_suspendButton;// Progress-Suspend-Button

    LOGICAL_FOCUS   m_logicalFocus; // Which view has the logical focus
    CDeadFocusWnd   m_wndDeadFocus; // Zero-size window which holds the focus if logical focus is "NONE"
    CComPtr<ITaskbarList3> m_TaskbarList;
    TBPFLAG m_TaskbarButtonState, m_TaskbarButtonPreviousState;

    DECLARE_MESSAGE_MAP()
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg LRESULT OnEnterSizeMove(WPARAM, LPARAM);
    afx_msg LRESULT OnExitSizeMove(WPARAM, LPARAM);
    afx_msg void OnClose();
    afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
    afx_msg void OnUpdateMemoryUsage(CCmdUI *pCmdUI);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnUpdateViewShowtreemap(CCmdUI *pCmdUI);
    afx_msg void OnViewShowtreemap();
    afx_msg void OnUpdateViewShowfiletypes(CCmdUI *pCmdUI);
    afx_msg void OnViewShowfiletypes();
    afx_msg void OnConfigure();
    afx_msg void OnDestroy();
    afx_msg void OnBnClickedSuspend();
    afx_msg void OnTreemapHelpabouttreemaps();
    afx_msg LRESULT OnTaskButtonCreated(WPARAM, LPARAM);

public:
    #ifdef _DEBUG
        virtual void AssertValid() const;
        virtual void Dump(CDumpContext& dc) const;
    #endif
        afx_msg void OnSysColorChange();
};

#endif // __WDS_MAINFRAME_H__
