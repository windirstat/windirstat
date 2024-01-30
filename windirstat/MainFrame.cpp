// MainFrame.cpp - Implementation of CMySplitterWnd, CPacmanControl and CMainFrame
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

#include "stdafx.h"
#include "WinDirStat.h"

#include "GraphView.h"
#include "DirStatView.h"
#include "TypeView.h"
#include "DirStatDoc.h"
#include "GlobalHelpers.h"
#include "TreeListControl.h"
#include "Item.h"

#include "PageCleanups.h"
#include "PageTreeList.h"
#include "PageTreeMap.h"
#include "PageGeneral.h"
#include "Property.h"

#include <common/MdExceptions.h>
#include <common/CommonHelpers.h>

#include "MainFrame.h"

#include <complex.h>
#include <functional>
#include <unordered_map>

#include "PageAdvanced.h"
#include "Property.h"

namespace
{
    // This must be synchronized with the IDR_MAINFRAME menu
    enum TOPLEVELMENU
    {
        TLM_FILE,
        TLM_EDIT,
        TLM_CLEANUP,
        TLM_TREEMAP,
        TLM_REPORT,
        TLM_VIEW,
        TLM_HELP
    };

    enum
    {
        // This is the position of the first "User defined cleanup" menu item in the "Cleanup" menu.
        // !!! MUST BE SYNCHRONIZED WITH THE MENU RESOURCE !!!
        MAINMENU_USERDEFINEDCLEANUP_POSITION = 11
    };

    // Clipboard-Opener
    class COpenClipboard final
    {
    public:
        COpenClipboard(CWnd* owner, bool empty = true)
        {
            m_open = owner->OpenClipboard();
            if (!m_open)
            {
                MdThrowStringException(IDS_CANNOTOPENCLIPBOARD);
            }
            if (empty)
            {
                if (!EmptyClipboard())
                {
                    MdThrowStringException(IDS_CANNOTEMTPYCLIPBOARD);
                }
            }
        }

        ~COpenClipboard()
        {
            if (m_open)
            {
                CloseClipboard();
            }
        }

    private:
        BOOL m_open;
    };
}

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(COptionsPropertySheet, CPropertySheet)

COptionsPropertySheet::COptionsPropertySheet()
    : CPropertySheet(IDS_WINDIRSTAT_SETTINGS)
      , m_restartApplication(false)
      , m_languageChanged(false)
      , m_alreadyAsked(false)
{
}

void COptionsPropertySheet::SetLanguageChanged(bool changed)
{
    m_languageChanged = changed;
}

BOOL COptionsPropertySheet::OnInitDialog()
{
    const BOOL bResult = CPropertySheet::OnInitDialog();
    SetActivePage(min(COptions::ConfigPage, GetPageCount() - 1));
    return bResult;
}

BOOL COptionsPropertySheet::OnCommand(WPARAM wParam, LPARAM lParam)
{
    COptions::ConfigPage = GetActiveIndex();

    const int cmd = LOWORD(wParam);
    if (IDOK == cmd || ID_APPLY_NOW == cmd)
    {
        if (m_languageChanged && (IDOK == cmd || !m_alreadyAsked))
        {
            const int r = AfxMessageBox(IDS_LANGUAGERESTARTNOW, MB_YESNOCANCEL);
            if (IDCANCEL == r)
            {
                return true; // "Message handled". Don't proceed.
            }
            else if (IDNO == r)
            {
                m_alreadyAsked = true; // Don't ask twice.
            }
            else
            {
                ASSERT(IDYES == r);
                m_restartApplication = true;

                if (ID_APPLY_NOW == cmd)
                {
                    // This _posts_ a message...
                    EndDialog(IDOK);
                    // ... so after returning from this function, the OnOK()-handlers
                    // of the pages will be called, before the sheet is closed.
                }
            }
        }
    }

    return CPropertySheet::OnCommand(wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////

CMySplitterWnd::CMySplitterWnd(double * splitterPos) :
    m_splitterPos(0),
    m_userSplitterPos(splitterPos)
{
    m_wasTrackedByUser = (*m_userSplitterPos > 0 && *m_userSplitterPos < 1);
}

BEGIN_MESSAGE_MAP(CMySplitterWnd, CSplitterWnd)
    ON_WM_SIZE()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

void CMySplitterWnd::StopTracking(BOOL bAccept)
{
    CSplitterWnd::StopTracking(bAccept);

    if (bAccept)
    {
        CRect rcClient;
        GetClientRect(rcClient);

        if (GetColumnCount() > 1)
        {
            int dummy;
            int cxLeft;
            GetColumnInfo(0, cxLeft, dummy);

            if (rcClient.Width() > 0)
            {
                m_splitterPos = static_cast<double>(cxLeft) / rcClient.Width();
            }
        }
        else
        {
            int dummy;
            int cyUpper;
            GetRowInfo(0, cyUpper, dummy);

            if (rcClient.Height() > 0)
            {
                m_splitterPos = static_cast<double>(cyUpper) / rcClient.Height();
            }
        }
        m_wasTrackedByUser = true;
        *m_userSplitterPos = m_splitterPos;
    }
}

void CMySplitterWnd::SetSplitterPos(double pos)
{
    m_splitterPos = pos;

    CRect rcClient;
    GetClientRect(rcClient);

    if (GetColumnCount() > 1)
    {
        if (m_pColInfo != nullptr)
        {
            const int cxLeft = static_cast<int>(pos * rcClient.Width());
            if (cxLeft >= 0)
            {
                SetColumnInfo(0, cxLeft, 0);
                RecalcLayout();
            }
        }
    }
    else
    {
        if (m_pRowInfo != nullptr)
        {
            const int cyUpper = static_cast<int>(pos * rcClient.Height());
            if (cyUpper >= 0)
            {
                SetRowInfo(0, cyUpper, 0);
                RecalcLayout();
            }
        }
    }
}

void CMySplitterWnd::RestoreSplitterPos(double posIfVirgin)
{
    if (m_wasTrackedByUser)
    {
        SetSplitterPos(*m_userSplitterPos);
    }
    else
    {
        SetSplitterPos(posIfVirgin);
    }
}

void CMySplitterWnd::OnSize(UINT nType, int cx, int cy)
{
    if (GetColumnCount() > 1)
    {
        const int cxLeft = static_cast<int>(cx * m_splitterPos);
        if (cxLeft > 0)
        {
            SetColumnInfo(0, cxLeft, 0);
        }
    }
    else
    {
        const int cyUpper = static_cast<int>(cy * m_splitterPos);
        if (cyUpper > 0)
        {
            SetRowInfo(0, cyUpper, 0);
        }
    }
    CSplitterWnd::OnSize(nType, cx, cy);
}

void CMySplitterWnd::OnDestroy()
{
    CSplitterWnd::OnDestroy();
}

/////////////////////////////////////////////////////////////////////////////

CPacmanControl::CPacmanControl()
{
    m_pacman.SetBackgroundColor(::GetSysColor(COLOR_BTNFACE));
    m_pacman.SetSpeed(0.00005f);
}

void CPacmanControl::Drive()
{
    if (::IsWindow(m_hWnd))
    {
        RedrawWindow();
    }
}

void CPacmanControl::Start()
{
    m_pacman.Start();
}

void CPacmanControl::Stop()
{
    m_pacman.Stop();
}

BEGIN_MESSAGE_MAP(CPacmanControl, CStatic)
    ON_WM_PAINT()
    ON_WM_CREATE()
END_MESSAGE_MAP()

int CPacmanControl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CStatic::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    m_pacman.Reset();
    m_pacman.Start();
    return 0;
}

void CPacmanControl::OnPaint()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(rc);
    m_pacman.Draw(&dc, rc);
}

/////////////////////////////////////////////////////////////////////////////

void CDeadFocusWnd::Create(CWnd* parent)
{
    const CRect rc(0, 0, 0, 0);
    VERIFY(CWnd::Create(AfxRegisterWndClass(0, 0, 0, 0), L"_deadfocus", WS_CHILD, rc, parent, 0));
}

CDeadFocusWnd::~CDeadFocusWnd()
{
    DestroyWindow();
}

BEGIN_MESSAGE_MAP(CDeadFocusWnd, CWnd)
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()

void CDeadFocusWnd::OnKeyDown(UINT nChar, UINT /* nRepCnt */, UINT /* nFlags */)
{
    if (nChar == VK_TAB)
    {
        GetMainFrame()->MoveFocus(LF_DIRECTORYLIST);
    }
}

/////////////////////////////////////////////////////////////////////////////
UINT CMainFrame::s_taskBarMessage = ::RegisterWindowMessage(L"TaskbarButtonCreated");

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWndEx)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWndEx)
    ON_COMMAND(ID_CONFIGURE, OnConfigure)
    ON_COMMAND(ID_TREEMAP_HELPABOUTTREEMAPS, OnTreemapHelpAboutTreeMaps)
    ON_COMMAND(ID_VIEW_SHOWFILETYPES, OnViewShowFileTypes)
    ON_COMMAND(ID_VIEW_SHOWTREEMAP, OnViewShowtreemap)
    ON_MESSAGE(WM_ENTERSIZEMOVE, OnEnterSizeMove)
    ON_MESSAGE(WM_EXITSIZEMOVE, OnExitSizeMove)
    ON_MESSAGE(WM_CALLBACKUI, OnCallbackRequest)
    ON_REGISTERED_MESSAGE(s_taskBarMessage, OnTaskButtonCreated)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWFILETYPES, OnUpdateViewShowFileTypes)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWTREEMAP, OnUpdateViewShowtreemap)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_MEMORYUSAGE, OnUpdateMemoryUsage)
    ON_WM_CLOSE()
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_INITMENUPOPUP()
    ON_WM_SIZE()
    ON_WM_SYSCOLORCHANGE()
    ON_WM_TIMER()
END_MESSAGE_MAP()

static UINT indicators[] =
{
    ID_SEPARATOR,
    ID_INDICATOR_MEMORYUSAGE,
    ID_INDICATOR_CAPS,
    ID_INDICATOR_NUM,
    ID_INDICATOR_SCRL,
};

static UINT indicatorsWithoutMemoryUsage[] =
{
    ID_SEPARATOR,
    ID_INDICATOR_CAPS,
    ID_INDICATOR_NUM,
    ID_INDICATOR_SCRL,
};

CMainFrame* CMainFrame::_theFrame;

CMainFrame* CMainFrame::GetTheFrame()
{
    return _theFrame;
}

CMainFrame::CMainFrame()
    : m_progressVisible(false)
      , m_scanSuspend(false)
      , m_progressRange(100)
      , m_progressPos(0)
      , m_workingItem(nullptr)
      , m_wndSubSplitter(COptions::SubSplitterPos.Ptr())
      , m_wndSplitter(COptions::MainSplitterPos.Ptr())
      , m_logicalFocus(LF_NONE)
      , m_TaskbarButtonState(TBPF_INDETERMINATE)
      , m_TaskbarButtonPreviousState(TBPF_INDETERMINATE)
{
    _theFrame = this;
}

CMainFrame::~CMainFrame()
{
    _theFrame = nullptr;
}

LRESULT CMainFrame::OnTaskButtonCreated(WPARAM, LPARAM)
{
    if (!m_TaskbarList)
    {
        const HRESULT hr = ::CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_ALL, IID_ITaskbarList3, reinterpret_cast<LPVOID*>(&m_TaskbarList));
        if (FAILED(hr))
        {
            VTRACE(L"CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL) failed %08X", hr);
        }
    }
    return 0;
}

void CMainFrame::CreateProgress(ULONGLONG range)
{
    // A range of 0 means that we have no range.
    // In this case we display pacman.
    
    if (COptions::FollowMountPoints || COptions::FollowJunctionPoints)
    {
        range = 0;
    }
    m_progressRange   = range;
    m_progressPos     = 0;
    m_progressVisible = true;
    if (range > 0)
    {
        CreateStatusProgress();
    }
    else
    {
        CreatePacmanProgress();
    }
}

void CMainFrame::SetProgressPos(ULONGLONG pos)
{
    if (m_progressRange > 0 && pos > m_progressRange)
    {
        pos = m_progressRange;
    }

    m_progressPos = pos;
    UpdateProgress();
}

void CMainFrame::SetProgressComplete() // called by CDirStatDoc
{
    if (m_TaskbarList)
    {
        m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = TBPF_NOPROGRESS);
    }
    UpdateProgress();
    DestroyProgress();

    GetDocument()->SetTitlePrefix(wds::strEmpty);
    SetMessageText(AFX_IDS_IDLEMESSAGE);
}

bool CMainFrame::IsScanSuspended() const
{
    return m_scanSuspend;
}

void CMainFrame::SuspendState(bool suspend)
{
    m_scanSuspend = suspend;
    if (m_TaskbarList)
    {
        if (m_TaskbarButtonState == TBPF_PAUSED)
        {
            m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = m_TaskbarButtonPreviousState);
        }
        else
        {
            m_TaskbarButtonPreviousState = m_TaskbarButtonState;
            m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = TBPF_PAUSED);
        }
    }
    CPacman::SetGlobalSuspendState(suspend);
    UpdateProgress();
}

void CMainFrame::UpdateProgress()
{
    // Update working item tracker if changed
    CItem* workingItem = GetDocument()->GetRootItem();
    if (workingItem != m_workingItem)
    {
        m_workingItem = workingItem;
        CreateProgress(m_workingItem->GetProgressRange());
    }

    // Exit early if we not ready for visual updates
    if (!m_progressVisible || m_workingItem == nullptr) return;

    // Update pacman graphic (does nothing if hidden)
    m_progressPos = m_workingItem->GetProgressPos();
    m_pacman.Drive();

    CStringW titlePrefix;
    CStringW suspended;

    // Display the suspend text in the bar if suspended
    if (IsScanSuspended())
    {
        VERIFY(suspended.LoadString(IDS_SUSPENDED_));
    }

    if (m_progressRange > 0 && m_progress.m_hWnd != nullptr)
    {
        const int pos = static_cast<int>((double)m_progressPos * 100 / m_progressRange);
        m_progress.SetPos(pos);
        titlePrefix.Format(L"%d%% %s", pos, suspended.GetString());
        if (m_TaskbarList && m_TaskbarButtonState != TBPF_PAUSED)
        {
            if (pos == 100)
            {
                m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = TBPF_INDETERMINATE); // often happens before we're finished
            }
            else
            {
                m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = TBPF_NORMAL); // often happens before we're finished
                m_TaskbarList->SetProgressValue(*this, m_progressPos, m_progressRange);
            }
        }
    }
    else
    {
        titlePrefix = LoadString(IDS_SCANNING_) + suspended;
    }

    GetDocument()->SetTitlePrefix(titlePrefix);
}

void CMainFrame::CreateStatusProgress()
{
    if (m_progress.m_hWnd == nullptr)
    {
        CRect rc;
        m_wndStatusBar.GetItemRect(0, rc);
        m_progress.Create(WS_CHILD | WS_VISIBLE, rc, &m_wndStatusBar, ID_WDS_CONTROL);
        m_progress.ModifyStyle(WS_BORDER, 0); // Doesn't help with XP-style control.
    }
    if (m_TaskbarList)
    {
        m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = TBPF_INDETERMINATE);
    }
}

void CMainFrame::CreatePacmanProgress()
{
    if (m_pacman.m_hWnd == nullptr)
    {
        CRect rc;
        m_wndStatusBar.GetItemRect(0, rc);
        m_pacman.Create(wds::strEmpty, WS_CHILD | WS_VISIBLE, rc, &m_wndStatusBar, ID_WDS_CONTROL);
        m_pacman.Start();
    }
}

void CMainFrame::DestroyProgress()
{
    if (::IsWindow(m_progress.m_hWnd))
    {
        m_progress.DestroyWindow();
        m_progress.m_hWnd = nullptr;
    }
    else if (::IsWindow(m_pacman.m_hWnd))
    {
        m_pacman.DestroyWindow();
        m_pacman.m_hWnd = nullptr;
    }
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWndEx::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    VERIFY(m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC));
    VERIFY(m_wndToolBar.LoadToolBar(IDR_MAINFRAME));

    const UINT* indic = indicators;
    UINT size = _countof(indicators);

    if (GetWDSApp()->GetCurrentProcessMemoryInfo() == wds::strEmpty)
    {
        indic = indicatorsWithoutMemoryUsage;
        size = _countof(indicatorsWithoutMemoryUsage);
    }

    VERIFY(m_wndStatusBar.Create(this));
    VERIFY(m_wndStatusBar.SetIndicators(indic, size));
    m_wndDeadFocus.Create(this);

    m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
    EnableDocking(CBRS_ALIGN_ANY);
    DockPane(&m_wndToolBar);

    // map from toolbar resources to specific icons
    const std::unordered_map<UINT, UINT> toolbar_map =
    {
        { ID_FILE_SELECT, IDB_FILE_SELECT },
        { ID_CLEANUP_OPEN_SELECTED, IDB_CLEANUP_OPEN_SELECTED },
        { ID_EDIT_COPY_CLIPBOARD, IDB_EDIT_COPY_CLIPBOARD },
        { ID_CLEANUP_EXPLORER_SELECT, IDB_CLEANUP_EXPLORER_SELECT },
        { ID_CLEANUP_OPEN_IN_CONSOLE, IDB_CLEANUP_OPEN_IN_CONSOLE },
        { ID_REFRESH_SELECTED, IDB_REFRESH_SELECTED },
        { ID_REFRESH_ALL, IDB_REFRESH_ALL },
        { ID_SCAN_RESUME, IDB_SCAN_RESUME },
        { ID_SCAN_SUSPEND, IDB_SCAN_SUSPEND },
        { ID_CLEANUP_DELETE_BIN, IDB_CLEANUP_DELETE_BIN },
        { ID_CLEANUP_DELETE, IDB_CLEANUP_DELETE },
        { ID_CLEANUP_PROPERTIES, IDB_CLEANUP_PROPERTIES },
        { ID_TREEMAP_ZOOMIN, IDB_TREEMAP_ZOOMIN },
        { ID_TREEMAP_ZOOMOUT, IDB_TREEMAP_ZOOMOUT },
        { ID_HELP_MANUAL, IDB_HELP_MANUAL } };

    // update toolbar images with high resolution versions
    CMFCToolBarImages* images = new CMFCToolBarImages();
    images->SetImageSize({ 16,16 }, TRUE);
    for (int i = 0; i < m_wndToolBar.GetCount();i++)
    {
        // lookup the button in the editor toolbox
        const auto button = m_wndToolBar.GetButton(i);
        if (button->m_nID == 0) continue;
        ASSERT(toolbar_map.contains(button->m_nID));

        // load high quality bitmap from resource
        CBitmap bitmap;
        bitmap.LoadBitmapW(toolbar_map.at(button->m_nID));
        const int image = images->AddImage(bitmap, TRUE);
        CMFCToolBar::SetUserImages(images);

        // copy button into new toolbar control
        CMFCToolBarButton new_button(button->m_nID, image, nullptr, TRUE, TRUE);
        new_button.m_nStyle = button->m_nStyle | TBBS_DISABLED;
        new_button.m_strText = button->m_strText;
        m_wndToolBar.ReplaceButton(button->m_nID, new_button);
    }

    // setup look at feel
    CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows7));
    CDockingManager::SetDockingMode(DT_SMART);

    return 0;
}

void CMainFrame::InitialShowWindow()
{
    WINDOWPLACEMENT wpsetting = COptions::MainWindowPlacement;
    if (wpsetting.length != 0)
    {
        SetWindowPlacement(&wpsetting);
    }

    SetTimer(ID_INDICATOR_MEMORYUSAGE, 25, nullptr);
}

void CMainFrame::InvokeInMessageThread(std::function<void()> callback)
{
    if (AfxGetApp()->m_nThreadID == GetCurrentThreadId()) callback();
    else GetMainFrame()->SendMessage(WM_CALLBACKUI, 0, reinterpret_cast<LPARAM>(&callback));
}

void CMainFrame::OnClose()
{
    CWaitCursor wc;

    // Suspend the scan and wait for scan to complete
    GetDocument()->ShutdownCoordinator();

    // It's too late, to do this in OnDestroy(). Because the toolbar, if undocked,
    // is already destroyed in OnDestroy(). So we must save the toolbar state here
    // in OnClose().
    COptions::ShowToolbar = (m_wndToolBar.GetStyle() & WS_VISIBLE) != 0;
    COptions::ShowStatusbar = (m_wndStatusBar.GetStyle() & WS_VISIBLE) != 0;

    CFrameWndEx::OnClose();
}

void CMainFrame::OnDestroy()
{
    // Save our window position
    WINDOWPLACEMENT wp = { .length = sizeof(wp) };
    GetWindowPlacement(&wp);
    COptions::MainWindowPlacement = wp;

    COptions::ShowFileTypes = GetTypeView()->IsShowTypes();
    COptions::ShowTreemap = GetGraphView()->IsShowTreemap();

    // Close all artifacts and our child windows
    CFrameWndEx::OnDestroy();

    // Persist values at very end after all children have closed
    PersistedSetting::WritePersistedProperties();
}

BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT /*lpcs*/, CCreateContext* pContext)
{
    VERIFY(m_wndSplitter.CreateStatic(this, 2, 1));
    VERIFY(m_wndSplitter.CreateView(1, 0, RUNTIME_CLASS(CGraphView), CSize(100, 100), pContext));
    VERIFY(m_wndSubSplitter.CreateStatic(&m_wndSplitter, 1, 2, WS_CHILD | WS_VISIBLE | WS_BORDER, m_wndSplitter.IdFromRowCol(0, 0)));
    VERIFY(m_wndSubSplitter.CreateView(0, 0, RUNTIME_CLASS(CDirStatView), CSize(700, 500), pContext));
    VERIFY(m_wndSubSplitter.CreateView(0, 1, RUNTIME_CLASS(CTypeView), CSize(100, 500), pContext));

    MinimizeGraphView();
    MinimizeTypeView();

    GetTypeView()->ShowTypes(COptions::ShowFileTypes);
    GetGraphView()->ShowTreemap(COptions::ShowTreemap);

    return TRUE;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    // seed initial title bar text
    static CStringW title = LoadString(AFX_IDS_APP_TITLE) + (IsAdmin() ? L" (Administrator)" : L"");
    cs.style &= ~FWS_ADDTOTITLE;
    cs.lpszName = title.GetString();

    if (!CFrameWndEx::PreCreateWindow(cs))
    {
        return FALSE;
    }

    return TRUE;
}

void CMainFrame::MinimizeTypeView()
{
    m_wndSubSplitter.SetSplitterPos(1.0);
}

void CMainFrame::RestoreTypeView()
{
    if (GetTypeView()->IsShowTypes())
    {
        m_wndSubSplitter.RestoreSplitterPos(0.72);
        GetTypeView()->RedrawWindow();
    }
}

void CMainFrame::MinimizeGraphView()
{
    m_wndSplitter.SetSplitterPos(1.0);
}

void CMainFrame::RestoreGraphView()
{
    if (GetGraphView()->IsShowTreemap())
    {
        m_wndSplitter.RestoreSplitterPos(0.4);
        GetGraphView()->DrawEmptyView();
        GetGraphView()->RedrawWindow();
    }
}

CDirStatView* CMainFrame::GetDirStatView() const
{
    CWnd* pWnd = m_wndSubSplitter.GetPane(0, 0);
    return DYNAMIC_DOWNCAST(CDirStatView, pWnd);
}

CGraphView* CMainFrame::GetGraphView() const
{
    CWnd* pWnd = m_wndSplitter.GetPane(1, 0);
    return DYNAMIC_DOWNCAST(CGraphView, pWnd);
}

CTypeView* CMainFrame::GetTypeView() const
{
    CWnd* pWnd = m_wndSubSplitter.GetPane(0, 1);
    return DYNAMIC_DOWNCAST(CTypeView, pWnd);
}

LRESULT CMainFrame::OnEnterSizeMove(WPARAM, LPARAM)
{
    GetGraphView()->SuspendRecalculationDrawing(true);
    return 0;
}

LRESULT CMainFrame::OnExitSizeMove(WPARAM, LPARAM)
{
    GetGraphView()->SuspendRecalculationDrawing(false);
    return 0;
}

void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
    // Update memory usage
    m_wndStatusBar.UpdateWindow();

    // Update tree control
    if (!GetDocument()->IsRootDone())
    {
        // Update the visual progress on the bottom of the screen
        UpdateProgress();

        // By sorting items, items will be redrawn which will
        // also force pacman to update with recent position
        CTreeListControl::GetTheTreeListControl()->SortItems();
    }

    CFrameWndEx::OnTimer(nIDEvent);
}

LRESULT CMainFrame::OnCallbackRequest(WPARAM, LPARAM lParam)
{
    const auto & callback = *static_cast<std::function<void()>*>(reinterpret_cast<LPVOID>(lParam));
    callback();
    return 0;
}

void CMainFrame::CopyToClipboard(LPCWSTR psz)
{
    try
    {
        COpenClipboard clipboard(this);
        const SIZE_T cchBufLen = _tcslen(psz) + 1;

        const HGLOBAL h = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE | GMEM_ZEROINIT, cchBufLen * sizeof(WCHAR));
        if (h == nullptr)
        {
            MdThrowStringException(L"GlobalAlloc failed.");
        }

        const LPVOID lp = ::GlobalLock(h);
        ASSERT(lp != NULL);

        if (!lp)
        {
            MdThrowStringException(L"GlobalLock failed.");
        }

        wcscpy_s(static_cast<LPWSTR>(lp), cchBufLen, psz);

        ::GlobalUnlock(h);

        if (nullptr == ::SetClipboardData(CF_UNICODETEXT, h))
        {
            MdThrowStringException(IDS_CANNOTSETCLIPBAORDDATA);
        }
    }
    catch (CException& pe)
    {
        pe.ReportError();
    }
}

void CMainFrame::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
    CFrameWndEx::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);

    if (!bSysMenu)
    {
        if (nIndex == TLM_CLEANUP)
        {
            UpdateCleanupMenu(pPopupMenu);
        }
    }
}

void CMainFrame::UpdateCleanupMenu(CMenu* menu)
{
    CStringW s = LoadString(IDS_EMPTYRECYCLEBIN);
    VERIFY(menu->ModifyMenu(ID_CLEANUP_EMPTY_BIN, MF_BYCOMMAND | MF_STRING, ID_CLEANUP_EMPTY_BIN, s));
    // TODO: can be cleaned, so that we don't disable and then enable the menu item
    menu->EnableMenuItem(ID_CLEANUP_EMPTY_BIN, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

    ULONGLONG items;
    ULONGLONG bytes;

    QueryRecycleBin(items, bytes);

    CStringW info;
    if (items == 1)
    {
        info.FormatMessage(IDS__ONEITEMss, FormatBytes(bytes).GetString(), COptions::HumanFormat && bytes != 0 ? wds::strEmpty : (wds::strBlankSpace + GetSpec_Bytes()).GetString());
    }
    else
    {
        info.FormatMessage(IDS__sITEMSss, FormatCount(items).GetString(), FormatBytes(bytes).GetString(), COptions::HumanFormat && bytes != 0 ? wds::strEmpty : (wds::strBlankSpace + GetSpec_Bytes()).GetString());
    }

    s += info;
    VERIFY(menu->ModifyMenu(ID_CLEANUP_EMPTY_BIN, MF_BYCOMMAND | MF_STRING, ID_CLEANUP_EMPTY_BIN, s));

    // ModifyMenu() re-enables the item. So we disable (or enable) it again.

    const UINT flags = items > 0 ? MF_ENABLED : (MF_DISABLED | MF_GRAYED);
    menu->EnableMenuItem(ID_CLEANUP_EMPTY_BIN, flags);

    const UINT toRemove = menu->GetMenuItemCount() - MAINMENU_USERDEFINEDCLEANUP_POSITION;
    for (UINT i = 0; i < toRemove; i++)
    {
        menu->RemoveMenu(MAINMENU_USERDEFINEDCLEANUP_POSITION, MF_BYPOSITION);
    }

    AppendUserDefinedCleanups(menu);
}

void CMainFrame::QueryRecycleBin(ULONGLONG& items, ULONGLONG& bytes)
{
    items = 0;
    bytes = 0;

    const DWORD drives = ::GetLogicalDrives();
    DWORD mask         = 0x00000001;
    for (int i = 0; i < wds::iNumDriveLetters; i++, mask <<= 1)
    {
        if ((drives & mask) == 0)
        {
            continue;
        }

        CStringW s;
        s.Format(L"%c:\\", i + wds::chrCapA);

        const UINT type = ::GetDriveType(s);
        if (type == DRIVE_UNKNOWN || type == DRIVE_NO_ROOT_DIR)
        {
            continue;
        }

        if (type == DRIVE_REMOTE)
        {
            continue;
        }

        SHQUERYRBINFO qbi;
        ZeroMemory(&qbi, sizeof(qbi));
        qbi.cbSize = sizeof(qbi);

        const HRESULT hr = ::SHQueryRecycleBin(s, &qbi);

        if (FAILED(hr))
        {
            continue;
        }

        items += qbi.i64NumItems;
        bytes += qbi.i64Size;
    }
}

void CMainFrame::AppendUserDefinedCleanups(CMenu* menu)
{
    bool bHasItem = false;
    for (size_t iCurrent = 0; iCurrent < COptions::UserDefinedCleanups.size(); iCurrent++)
    {
        auto& udc = COptions::UserDefinedCleanups[iCurrent];
        if (!udc.enabled) continue;

        CStringW string;
        string.FormatMessage(IDS_UDCsCTRLd, udc.title.Obj().c_str(), iCurrent);

        const auto& items = CTreeListControl::GetTheTreeListControl()->GetAllSelected<CItem>();
        bool udc_valid = GetLogicalFocus() == LF_DIRECTORYLIST && !items.empty();
        if (udc_valid) for (const auto& item : items)
        {
            udc_valid &= GetDocument()->UserDefinedCleanupWorksForItem(&udc, item);
        }

        bHasItem = true;
        const UINT flags = udc_valid ? MF_ENABLED : (MF_DISABLED | MF_GRAYED);
        menu->AppendMenu(flags | MF_STRING, ID_USERDEFINEDCLEANUP0 + iCurrent, string);
    }
   
    if (!bHasItem)
    {
        // This is just to show new users, that they can configure user defined cleanups.
        menu->AppendMenu(MF_GRAYED, 0, LoadString(IDS_USERDEFINEDCLEANUP0));
    }
}

void CMainFrame::SetLogicalFocus(LOGICAL_FOCUS lf)
{
    if (lf != m_logicalFocus)
    {
        m_logicalFocus = lf;
        SetSelectionMessageText();

        GetDocument()->UpdateAllViews(nullptr, HINT_SELECTIONSTYLECHANGED);
    }
}

LOGICAL_FOCUS CMainFrame::GetLogicalFocus() const
{
    return m_logicalFocus;
}

void CMainFrame::MoveFocus(LOGICAL_FOCUS lf)
{
    switch (lf)
    {
    case LF_NONE:
        {
            SetLogicalFocus(LF_NONE);
            m_wndDeadFocus.SetFocus();
        }
        break;
    case LF_DIRECTORYLIST:
        {
            GetDirStatView()->SetFocus();
        }
        break;
    case LF_EXTENSIONLIST:
        {
            GetTypeView()->SetFocus();
        }
        break;
    }
}

void CMainFrame::SetSelectionMessageText()
{
    switch (GetLogicalFocus())
    {
    case LF_NONE:
        {
            SetMessageText(AFX_IDS_IDLEMESSAGE);
        }
        break;
    case LF_DIRECTORYLIST:
        {
            // display file name in bottom left corner if only one item is selected
            auto item = CTreeListControl::GetTheTreeListControl()->GetFirstSelectedItem<CItem>();
            if (item != nullptr)
            {
                // decide when to do this
                SetMessageText(item->GetPath());
            }
            else
            {
                SetMessageText(AFX_IDS_IDLEMESSAGE);
            }
        }
        break;
    case LF_EXTENSIONLIST:
        {
            SetMessageText(wds::strStar + GetDocument()->GetHighlightExtension());
        }
        break;
    }
}

void CMainFrame::OnUpdateMemoryUsage(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(true);
    pCmdUI->SetText(GetWDSApp()->GetCurrentProcessMemoryInfo());
}

void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
    CFrameWndEx::OnSize(nType, cx, cy);

    if (!::IsWindow(m_wndStatusBar.m_hWnd))
    {
        return;
    }

    CRect rc;
    m_wndStatusBar.GetItemRect(0, rc);

    if (m_progress.m_hWnd != nullptr)
    {
        m_progress.MoveWindow(rc);
    }
    else if (m_pacman.m_hWnd != nullptr)
    {
        m_pacman.MoveWindow(rc);
    }
}

void CMainFrame::OnUpdateViewShowtreemap(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(GetGraphView()->IsShowTreemap());
}

void CMainFrame::OnViewShowtreemap()
{
    GetGraphView()->ShowTreemap(!GetGraphView()->IsShowTreemap());
    if (GetGraphView()->IsShowTreemap())
    {
        RestoreGraphView();
    }
    else
    {
        MinimizeGraphView();
    }
}

void CMainFrame::OnUpdateViewShowFileTypes(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(GetTypeView()->IsShowTypes());
}

void CMainFrame::OnViewShowFileTypes()
{
    GetTypeView()->ShowTypes(!GetTypeView()->IsShowTypes());
    if (GetTypeView()->IsShowTypes())
    {
        RestoreTypeView();
    }
    else
    {
        MinimizeTypeView();
    }
}

void CMainFrame::OnConfigure()
{
    COptionsPropertySheet sheet;

    CPageGeneral general;
    CPageTreelist treelist;
    CPageTreemap treemap;
    CPageCleanups cleanups;
    CPageAdvanced advanced;

    sheet.AddPage(&general);
    sheet.AddPage(&treelist);
    sheet.AddPage(&treemap);
    sheet.AddPage(&cleanups);
    sheet.AddPage(&advanced);

    sheet.DoModal();

    if (sheet.m_restartApplication)
    {
        GetWDSApp()->RestartApplication();
    }
}

void CMainFrame::OnTreemapHelpAboutTreeMaps()
{
    GetWDSApp()->DoContextHelp(IDH_Treemap);
}

void CMainFrame::OnSysColorChange()
{
    CFrameWndEx::OnSysColorChange();
    GetDirStatView()->SysColorChanged();
    GetTypeView()->SysColorChanged();
}

