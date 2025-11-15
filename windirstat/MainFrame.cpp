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

#include "stdafx.h"
#include "WinDirStat.h"
#include "TreeMapView.h"
#include "FileTabbedView.h"
#include "FileTreeView.h"
#include "ExtensionView.h"
#include "DirStatDoc.h"
#include "GlobalHelpers.h"
#include "Item.h"
#include "Localization.h"
#include "Property.h"
#include "PageAdvanced.h"
#include "PageFiltering.h"
#include "PageCleanups.h"
#include "PageFileTree.h"
#include "PageTreeMap.h"
#include "PageGeneral.h"
#include "MainFrame.h"
#include "SelectObject.h"
#include "FileTopControl.h"
#include "FileSearchControl.h"
#include "SmartPointer.h"
#include "DarkMode.h"
#include "MessageBoxDlg.h"

namespace
{
    // Clipboard-Opener
    class COpenClipboard final
    {
    public:
        COpenClipboard(CWnd* owner)
        {
            m_Open = owner->OpenClipboard();
            if (!m_Open || !EmptyClipboard())
            {
                DisplayError(TranslateError());
            }
        }

        ~COpenClipboard()
        {
            if (m_Open)
            {
                CloseClipboard();
            }
        }

    private:
        BOOL m_Open = FALSE;
    };
}

/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(COptionsPropertySheet, CMFCPropertySheet)

COptionsPropertySheet::COptionsPropertySheet()
    : CMFCPropertySheet(Localization::Lookup(IDS_WINDIRSTAT_SETTINGS).c_str())
{
    m_look = PropSheetLook_OneNoteTabs;
}

void COptionsPropertySheet::SetRestartRequired(const bool changed)
{
    m_RestartRequest = changed;
}

BEGIN_MESSAGE_MAP(COptionsPropertySheet, CMFCPropertySheet)
    ON_WM_CTLCOLOR()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL COptionsPropertySheet::OnEraseBkgnd(CDC* pDC)
{
    if (!DarkMode::IsDarkModeActive())
    {
        return CMFCPropertySheet::OnEraseBkgnd(pDC);
    }

    // Paint the background with dark mode color
    CRect rect;
    GetClientRect(&rect);
    pDC->FillSolidRect(&rect, DarkMode::WdsSysColor(CTLCOLOR_DLG));
    
    return TRUE;
}

HBRUSH COptionsPropertySheet::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CMFCPropertySheet::OnCtlColor(pDC, pWnd, nCtlColor);
}

BOOL COptionsPropertySheet::OnInitDialog()
{
    const BOOL bResult = CMFCPropertySheet::OnInitDialog();
    CTabCtrlHelper::SetupTabControl(GetTab());

    Localization::UpdateDialogs(*this);
    Localization::UpdateTabControl(GetTab());
    DarkMode::AdjustControls(GetSafeHwnd());

    SetActivePage(min(COptions::ConfigPage, GetPageCount() - 1));
    return bResult;
}

BOOL COptionsPropertySheet::OnCommand(const WPARAM wParam, const LPARAM lParam)
{
    COptions::ConfigPage = GetActiveIndex();

    const int cmd = LOWORD(wParam);
    if (IDOK == cmd || ID_APPLY_NOW == cmd)
    {
        if (m_RestartRequest && (IDOK == cmd || !m_AlreadyAsked))
        {
            const int r = WdsMessageBox(*this, Localization::Lookup(IDS_RESTART_REQUEST),
                Localization::Lookup(IDS_APP_TITLE), MB_YESNOCANCEL);
            if (IDCANCEL == r)
            {
                return true; // "Message handled". Don't proceed.
            }
            else if (IDNO == r)
            {
                m_AlreadyAsked = true; // Don't ask twice.
            }
            else
            {
                ASSERT(IDYES == r);
                m_RestartApplication = true;

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

    return CMFCPropertySheet::OnCommand(wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////

CMySplitterWnd::CMySplitterWnd(double* splitterPos) :
    m_UserSplitterPos(splitterPos)
{
    m_WasTrackedByUser = (*splitterPos > 0 && *splitterPos < 1);
}

BEGIN_MESSAGE_MAP(CMySplitterWnd, CSplitterWndEx)
    ON_WM_SIZE()
END_MESSAGE_MAP()

void CMySplitterWnd::StopTracking(const BOOL bAccept)
{
    CSplitterWndEx::StopTracking(bAccept);

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
                m_SplitterPos = static_cast<double>(cxLeft) / rcClient.Width();
            }
        }
        else
        {
            int dummy;
            int cyUpper;
            GetRowInfo(0, cyUpper, dummy);

            if (rcClient.Height() > 0)
            {
                m_SplitterPos = static_cast<double>(cyUpper) / rcClient.Height();
            }
        }
        m_WasTrackedByUser = true;
        *m_UserSplitterPos = m_SplitterPos;
    }
}

void CMySplitterWnd::SetSplitterPos(const double pos)
{
    m_SplitterPos = pos;

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

void CMySplitterWnd::RestoreSplitterPos(const double posIfVirgin)
{
    if (m_WasTrackedByUser)
    {
        SetSplitterPos(*m_UserSplitterPos);
    }
    else
    {
        SetSplitterPos(posIfVirgin);
    }
}

void CMySplitterWnd::OnSize(const UINT nType, const int cx, const int cy)
{
    if (GetColumnCount() > 1)
    {
        const int cxLeft = static_cast<int>(cx * m_SplitterPos);
        if (cxLeft > 0)
        {
            SetColumnInfo(0, cxLeft, 0);
        }
    }
    else
    {
        const int cyUpper = static_cast<int>(cy * m_SplitterPos);
        if (cyUpper > 0)
        {
            SetRowInfo(0, cyUpper, 0);
        }
    }
    CSplitterWndEx::OnSize(nType, cx, cy);
}

/////////////////////////////////////////////////////////////////////////////

void CPacmanControl::Drive()
{
    if (IsWindow(m_hWnd))
    {
        m_Pacman.UpdatePosition();
        RedrawWindow();
    }
}

void CPacmanControl::Start()
{
    m_Pacman.Start();
}

void CPacmanControl::Stop()
{
    m_Pacman.Stop();
}

BEGIN_MESSAGE_MAP(CPacmanControl, CWnd)
    ON_WM_PAINT()
    ON_WM_CREATE()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

int CPacmanControl::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    if (CWnd::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    m_Pacman.Reset();
    m_Pacman.Start();
    return 0;
}

BOOL CPacmanControl::OnEraseBkgnd(CDC* pDC)
{
    UNREFERENCED_PARAMETER(pDC);
    return TRUE;
}

void CPacmanControl::OnPaint()
{
    // Setup double buffering
    CPaintDC dc(this);
    CDC memDC;
    memDC.CreateCompatibleDC(&dc);

    CRect rect;
    GetClientRect(&rect);

    CBitmap bm;
    bm.CreateCompatibleBitmap(&dc, rect.Width(), rect.Height());
    CSelectObject sobm(&memDC, &bm);

    // Draw the animation
    m_Pacman.Draw(&memDC, rect);

    // Draw the borders
    CMFCVisualManager::GetInstance()->OnDrawStatusBarPaneBorder(
        &memDC, &CMainFrame::Get()->m_WndStatusBar, rect, 0, CMainFrame::Get()->GetStyle());

    // Copy memory DC to screen DC
    dc.BitBlt(0, 0, rect.Width(), rect.Height(), &memDC, 0, 0, SRCCOPY);
}

/////////////////////////////////////////////////////////////////////////////

void CDeadFocusWnd::Create(CWnd* parent)
{
    const CRect rc(0, 0, 0, 0);
    VERIFY(CWnd::Create(AfxRegisterWndClass(0, nullptr, nullptr, nullptr), L"_deadfocus", WS_CHILD, rc, parent, 0));
}

CDeadFocusWnd::~CDeadFocusWnd()
{
    CWnd::DestroyWindow();
}

BEGIN_MESSAGE_MAP(CDeadFocusWnd, CWnd)
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()

void CDeadFocusWnd::OnKeyDown(const UINT nChar, UINT /*nRepCnt*/, UINT /*nFlags*/ )
{
    if (nChar == VK_TAB)
    {
        CMainFrame::Get()->MoveFocus(LF_FILETREE);
    }
}

/////////////////////////////////////////////////////////////////////////////
UINT CMainFrame::s_TaskBarMessage = ::RegisterWindowMessage(L"TaskbarButtonCreated");

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWndEx)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWndEx)
    ON_COMMAND(ID_CONFIGURE, OnConfigure)
    ON_COMMAND(ID_VIEW_SHOWFILETYPES, OnViewShowFileTypes)
    ON_COMMAND(ID_VIEW_SHOWTREEMAP, OnViewShowTreeMap)
    ON_COMMAND(ID_TREEMAP_LOGICAL_SIZE, OnViewTreeMapUseLogical)
    ON_MESSAGE(WM_ENTERSIZEMOVE, OnEnterSizeMove)
    ON_MESSAGE(WM_EXITSIZEMOVE, OnExitSizeMove)
    ON_MESSAGE(WM_CALLBACKUI, OnCallbackRequest)
    ON_MESSAGE(DarkMode::WM_UAHDRAWMENU, OnUahDrawMenu)
    ON_MESSAGE(DarkMode::WM_UAHDRAWMENUITEM, OnUahDrawMenu)
    ON_REGISTERED_MESSAGE(s_TaskBarMessage, OnTaskButtonCreated)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWFILETYPES, OnUpdateViewShowFileTypes)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWTREEMAP, OnUpdateViewShowTreeMap)
    ON_UPDATE_COMMAND_UI(ID_TREEMAP_LOGICAL_SIZE, OnUpdateTreeMapUseLogical)
    ON_WM_CLOSE()
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_INITMENUPOPUP()
    ON_WM_SIZE()
    ON_WM_SYSCOLORCHANGE()
    ON_WM_TIMER()
    ON_WM_NCPAINT()
    ON_WM_NCACTIVATE()
    ON_COMMAND(ID_VIEW_ALL_FILES, &CMainFrame::OnViewAllFiles)
    ON_COMMAND(ID_VIEW_LARGEST_FILES, &CMainFrame::OnViewLargestFiles)
    ON_COMMAND(ID_VIEW_DUPLICATE_FILES, &CMainFrame::OnViewDuplicateFiles)
    ON_COMMAND(ID_VIEW_SEARCH_RESULTS, &CMainFrame::OnViewSearchResults)
END_MESSAGE_MAP()

constexpr auto ID_STATUSPANE_IDLE_INDEX = 0;
constexpr auto ID_STATUSPANE_DISK_INDEX = 1;
constexpr auto ID_STATUSPANE_MEM_INDEX = 2;
constexpr auto ID_STATUSPANE_CAPS_INDEX = 3;
constexpr auto ID_STATUSPANE_NUM_INDEX = 4;
constexpr auto ID_STATUSPANE_SCRL_INDEX = 5;

constexpr UINT indicators[]
{
    ID_INDICATOR_IDLE,
    ID_INDICATOR_DISK,
    ID_INDICATOR_MEM,
    ID_INDICATOR_CAPS,
    ID_INDICATOR_NUM,
    ID_INDICATOR_SCRL,
};

CMainFrame* CMainFrame::s_Singleton;

CMainFrame::CMainFrame() :
        m_SubSplitter(COptions::SubSplitterPos.Ptr())
      , m_Splitter(COptions::MainSplitterPos.Ptr())
{
    s_Singleton = this;
}

CMainFrame::~CMainFrame()
{
    s_Singleton = nullptr;
}

LRESULT CMainFrame::OnTaskButtonCreated(WPARAM, LPARAM)
{
    if (!m_TaskbarList)
    {
        const HRESULT hr = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_TaskbarList));
        if (FAILED(hr))
        {
            VTRACE(L"CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_ALL) failed {:#08X}", static_cast<DWORD>(hr));
        }
    }
    return 0;
}

void CMainFrame::CreateProgress(ULONGLONG range)
{
    // Directory structure may contain other volume or internal loops
    // so set range to indicate there is no range so display pacman
    if (!COptions::ExcludeVolumeMountPoints ||
        !COptions::ExcludeJunctions ||
        !COptions::ExcludeSymbolicLinksDirectory)
    {
        range = 0;
    }

    m_ProgressRange = range;
    m_ProgressPos = 0;
    m_ProgressVisible = true;
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
    if (m_ProgressRange > 0 && pos > m_ProgressRange)
    {
        pos = m_ProgressRange;
    }

    m_ProgressPos = pos;
    UpdateProgress();
}

void CMainFrame::SetProgressComplete()
{
    // Disable any potential suspend state
    SuspendState(false);

    if (m_TaskbarList)
    {
        m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = TBPF_NOPROGRESS);
    }

    DestroyProgress();
    CDirStatDoc::GetDocument()->SetTitlePrefix(wds::strEmpty);
    CFileTreeControl::Get()->SortItems();
    CFileDupeControl::Get()->SortItems();
    CFileTopControl::Get()->SortItems();
}

bool CMainFrame::IsScanSuspended() const
{
    return m_ScanSuspend;
}

void CMainFrame::SuspendState(const bool suspend)
{
    m_ScanSuspend = suspend;
    if (m_TaskbarList)
    {
        if (m_TaskbarButtonState == TBPF_PAUSED)
        {
            m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = m_TaskbarButtonPreviousState);
        }
        else
        {
            m_TaskbarButtonPreviousState = m_TaskbarButtonState;
            m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState |= TBPF_PAUSED);
        }
    }
    CPacman::SetGlobalSuspendState(suspend);
    UpdateProgress();
}

void CMainFrame::UpdateProgress()
{
    // Update working item tracker if changed
    const auto currentRoot = CDirStatDoc::GetDocument()->GetRootItem();
    if (currentRoot != m_WorkingItem &&
        currentRoot != nullptr && !currentRoot->IsDone())
    {
        m_WorkingItem = currentRoot;
        CreateProgress(m_WorkingItem->GetProgressRange());
    }

    // Exit early if we not ready for visual updates
    if (!m_ProgressVisible || m_WorkingItem == nullptr || currentRoot == nullptr) return;

    // Update pacman graphic (does nothing if hidden)
    m_ProgressPos = m_WorkingItem->GetProgressPos();
    m_Pacman.Drive();

    std::wstring titlePrefix;
    std::wstring suspended;

    // Display the suspend text in the bar if suspended
    if (IsScanSuspended())
    {
        static const std::wstring suspendString = Localization::Lookup(IDS_SUSPENDED);
        suspended = suspendString;
    }

    if (m_ProgressRange > 0 && m_Progress.m_hWnd != nullptr)
    {
        // Limit progress at 100% as hard-linked files will count twice
        const int pos = min(static_cast<int>((m_ProgressPos * 100ull) / m_ProgressRange), 100);
        m_Progress.SetPos(pos);

        titlePrefix = std::to_wstring(pos) + L"% " + suspended;
        if (m_TaskbarList && m_TaskbarButtonState != TBPF_PAUSED)
        {
            if (pos == 100)
            {
                m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = TBPF_INDETERMINATE); // often happens before we're finished
            }
            else
            {
                m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = TBPF_NORMAL); // often happens before we're finished
                m_TaskbarList->SetProgressValue(*this, m_ProgressPos, m_ProgressRange);
            }
        }
    }
    else
    {
        static const std::wstring scanningString = Localization::Lookup(IDS_SCANNING);
        titlePrefix = scanningString + L" " + suspended;
    }

    TrimString(titlePrefix);
    CDirStatDoc::GetDocument()->SetTitlePrefix(titlePrefix);
}

void CMainFrame::CreateStatusProgress()
{
    if (m_Progress.m_hWnd == nullptr)
    {
        CRect rc;
        m_WndStatusBar.GetItemRect(ID_STATUSPANE_IDLE_INDEX, rc);
        m_Progress.Create(WS_CHILD | WS_VISIBLE, rc, &m_WndStatusBar, ID_WDS_CONTROL);
        m_Progress.ModifyStyle(WS_BORDER,0);

        if (DarkMode::IsDarkModeActive())
        {
            // Disable theming for progress bar to avoid light background in dark mode
            SetWindowTheme(m_Progress.GetSafeHwnd(), L"", L"");
            m_Progress.SetBkColor(DarkMode::WdsSysColor(COLOR_WINDOWFRAME));
        }
    }
    if (m_TaskbarList)
    {
        m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = TBPF_INDETERMINATE);
    }
}

void CMainFrame::CreatePacmanProgress()
{
    if (m_Pacman.m_hWnd == nullptr)
    {
        // Get rectangle and remove top/bottom border dimension
        CRect rc;
        m_WndStatusBar.GetItemRect(0, rc);
        m_Pacman.Create(nullptr, nullptr, WS_CHILD | WS_VISIBLE, rc, &m_WndStatusBar, ID_WDS_CONTROL);
        m_Pacman.Start();
    }
}

void CMainFrame::DestroyProgress()
{
    if (IsWindow(m_Progress.m_hWnd))
    {
        m_Progress.DestroyWindow();
        m_Progress.m_hWnd = nullptr;
    }
    else if (IsWindow(m_Pacman.m_hWnd))
    {
        m_Pacman.Stop();
        m_Pacman.DestroyWindow();
        m_Pacman.m_hWnd = nullptr;
    }

    m_WorkingItem = nullptr;
    m_ProgressVisible = false;
}

void CMainFrame::SetStatusPaneText(const int pos, const std::wstring & text, const int minWidth)
{
    // do not process the update if text is the same
    static std::unordered_map<int, std::wstring> last;
    if (last.contains(pos) && last[pos] == text) return;
    last[pos] = text;

    // set status path width and then set text
    const CClientDC dc(this);
    const auto cx = dc.GetTextExtent(text.c_str(), static_cast<int>(text.size())).cx;
    m_WndStatusBar.SetPaneWidth(pos, max(cx, minWidth));
    m_WndStatusBar.SetPaneText(pos, text.c_str());
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWndEx::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }
    
    m_WndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS | CBRS_SIZE_DYNAMIC);
    m_WndToolBar.LoadToolBar(IDR_MAINFRAME);
    m_WndToolBar.SetBorders(CRect());
    m_WndToolBar.SetPaneStyle(m_WndToolBar.GetPaneStyle() & ~CBRS_GRIPPER);

    // Setup status pane and force initial field population
    VERIFY(m_WndStatusBar.Create(this));
    m_WndStatusBar.SetIndicators(indicators, std::size(indicators));
    m_WndStatusBar.SetPaneStyle(ID_STATUSPANE_IDLE_INDEX, SBPS_STRETCH);
    SetStatusPaneText(ID_STATUSPANE_CAPS_INDEX, Localization::Lookup(IDS_INDICATOR_CAPS));
    SetStatusPaneText(ID_STATUSPANE_NUM_INDEX, Localization::Lookup(IDS_INDICATOR_NUM));
    SetStatusPaneText(ID_STATUSPANE_SCRL_INDEX, Localization::Lookup(IDS_INDICATOR_SCRL));
    UpdatePaneText();

    // Setup status pane for dark mode
    if (DarkMode::IsDarkModeActive())
    {
        for (int i = 0; i < m_WndStatusBar.GetCount(); i++)
        {
            m_WndStatusBar.SetPaneBackgroundColor(i, DarkMode::WdsSysColor(COLOR_WINDOW));
        }
    }

    // Show or hide status bar if requested
    if (!COptions::ShowStatusBar) m_WndStatusBar.ShowWindow(SW_HIDE);
    if (!COptions::ShowToolBar) m_WndToolBar.ShowWindow(SW_HIDE);

    m_WndDeadFocus.Create(this);
    DockPane(&m_WndToolBar);

    // map from toolbar resources to specific icons
    const std::unordered_map<UINT, std::pair<UINT, std::wstring_view>> toolbarMap =
    {
        { ID_FILE_SELECT, {IDB_FILE_SELECT, IDS_FILE_SELECT}},
        { ID_CLEANUP_OPEN_SELECTED, {IDB_CLEANUP_OPEN_SELECTED, IDS_CLEANUP_OPEN_SELECTED}},
        { ID_EDIT_COPY_CLIPBOARD, {IDB_EDIT_COPY_CLIPBOARD, IDS_EDIT_COPY_CLIPBOARD}},
        { ID_CLEANUP_EXPLORER_SELECT, {IDB_CLEANUP_EXPLORER_SELECT, IDS_CLEANUP_EXPLORER_SELECT}},
        { ID_CLEANUP_OPEN_IN_CONSOLE, {IDB_CLEANUP_OPEN_IN_CONSOLE, IDS_CLEANUP_OPEN_IN_CONSOLE}},
        { ID_REFRESH_SELECTED, {IDB_REFRESH_SELECTED, IDS_REFRESH_SELECTED}},
        { ID_REFRESH_ALL, {IDB_REFRESH_ALL, IDS_REFRESH_ALL}},
        { ID_FILTER, {IDB_FILTER, IDS_PAGE_FILTERING_TITLE}},
        { ID_SEARCH, {IDB_SEARCH, IDS_SEARCH_TITLE}},
        { ID_SCAN_SUSPEND, {IDB_SCAN_SUSPEND, IDS_SUSPEND}},
        { ID_SCAN_RESUME, {IDB_SCAN_RESUME, IDS_RESUME}},
        { ID_SCAN_STOP, {IDB_SCAN_STOP, IDS_STOP}},
        { ID_CLEANUP_DELETE_BIN, {IDB_CLEANUP_DELETE_BIN, IDS_CLEANUP_DELETE_BIN}},
        { ID_CLEANUP_DELETE, {IDB_CLEANUP_DELETE, IDS_CLEANUP_DELETE}},
        { ID_CLEANUP_PROPERTIES, {IDB_CLEANUP_PROPERTIES, IDS_CLEANUP_PROPERTIES}},
        { ID_TREEMAP_ZOOMIN, {IDB_TREEMAP_ZOOMIN, IDS_TREEMAP_ZOOMIN}},
        { ID_TREEMAP_ZOOMOUT, {IDB_TREEMAP_ZOOMOUT, IDS_TREEMAP_ZOOMOUT}},
        { ID_HELP_MANUAL, {IDB_HELP_MANUAL, IDS_HELP_MANUAL}} };

    // update toolbar images with high resolution versions
    m_Images.SetImageSize({ 16,16 }, TRUE);
    for (int i = 0; i < m_WndToolBar.GetCount(); i++)
    {
        // lookup the button in the editor toolbox
        const auto button = m_WndToolBar.GetButton(i);
        if (button->m_nID == 0) continue;
        ASSERT(toolbarMap.contains(button->m_nID));

        // load high quality bitmap from resource
        CBitmap bitmap;
        bitmap.LoadBitmapW(toolbarMap.at(button->m_nID).first);
        DarkMode::LightenBitmap(&bitmap);
        const int image = m_Images.AddImage(bitmap, TRUE);
        CMFCToolBar::SetUserImages(&m_Images);

        // copy button into new toolbar control
        CMFCToolBarButton newButton(button->m_nID, image, nullptr, TRUE, TRUE);
        newButton.m_nStyle = button->m_nStyle | TBBS_DISABLED;
        newButton.m_strText = Localization::Lookup(toolbarMap.at(button->m_nID).second).c_str();
        m_WndToolBar.ReplaceButton(button->m_nID, newButton);
    }

    // setup look and feel with dark mode support
    CMFCVisualManager::SetDefaultManager(DarkMode::IsDarkModeActive() ?
        RUNTIME_CLASS(CDarkModeVisualManager) : RUNTIME_CLASS(CMFCVisualManagerWindows7));

    // apply dark mode to main frame window
    DarkMode::AdjustControls(GetSafeHwnd());

    return 0;
}

void CMainFrame::InitialShowWindow()
{
    const WINDOWPLACEMENT wpsetting = COptions::MainWindowPlacement;
    if (wpsetting.length != 0)
    {
        SetWindowPlacement(&wpsetting);
    }

    SetTimer(ID_WDS_CONTROL, 25, nullptr);
}

void CMainFrame::InvokeInMessageThread(std::function<void()> callback) const
{
    if (CDirStatApp::Get()->m_nThreadID == GetCurrentThreadId()) callback();
    else Get()->SendMessage(WM_CALLBACKUI, 0, reinterpret_cast<LPARAM>(&callback));
}

void CMainFrame::OnClose()
{
    CWaitCursor wc;

    // Stop the timer so we are not updating elements during shutdown
    KillTimer(ID_WDS_CONTROL);

    // Suspend the scan and wait for scan to complete
    CDirStatDoc::GetDocument()->StopScanningEngine(CDirStatDoc::Abort);

    // Stop icon queue
    GetIconHandler()->StopAsyncShellInfoQueue();

    // It's too late, to do this in OnDestroy(). Because the toolbar, if undocked,
    // is already destroyed in OnDestroy(). So we must save the toolbar state here
    // in OnClose().
    COptions::ShowToolBar = (m_WndToolBar.GetStyle() & WS_VISIBLE) != 0;
    COptions::ShowStatusBar = (m_WndStatusBar.GetStyle() & WS_VISIBLE) != 0;

    CFrameWndEx::OnClose();
}

void CMainFrame::OnDestroy()
{
    // Save our window position
    WINDOWPLACEMENT wp = { .length = sizeof(wp) };
    GetWindowPlacement(&wp);
    COptions::MainWindowPlacement = wp;

    COptions::ShowFileTypes = GetExtensionView()->IsShowTypes();
    COptions::ShowTreeMap = GetTreeMapView()->IsShowTreeMap();

    // Close all artifacts and our child windows
    CFrameWndEx::OnDestroy();

    // Persist values at very end after all children have closed
    PersistedSetting::WritePersistedProperties();
}

BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT /*lpcs*/, CCreateContext* pContext)
{
    m_Splitter.CreateStatic(this, 2, 1);
    m_Splitter.CreateView(1, 0, RUNTIME_CLASS(CTreeMapView), CSize(100, 100), pContext);
    m_SubSplitter.CreateStatic(&m_Splitter, 1, 2,WS_CHILD  | WS_VISIBLE | WS_BORDER, m_Splitter.IdFromRowCol(0, 0));
    m_SubSplitter.CreateView(0, 0, RUNTIME_CLASS(CFileTabbedView), CSize(700, 500), pContext);
    m_SubSplitter.CreateView(0, 1, RUNTIME_CLASS(CExtensionView), CSize(100, 500), pContext);

    m_TreeMapView = DYNAMIC_DOWNCAST(CTreeMapView, m_Splitter.GetPane(1, 0));
    m_FileTabbedView = DYNAMIC_DOWNCAST(CFileTabbedView, m_SubSplitter.GetPane(0, 0));
    m_ExtensionView = DYNAMIC_DOWNCAST(CExtensionView, m_SubSplitter.GetPane(0, 1));

    MinimizeTreeMapView();
    MinimizeExtensionView();

    GetExtensionView()->ShowTypes(COptions::ShowFileTypes);
    GetTreeMapView()->ShowTreeMap(COptions::ShowTreeMap);

    return TRUE;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    // seed initial Title bar text
    static std::wstring title = Localization::Lookup(IDS_APP_TITLE) + (IsElevationActive() ? L" (Administrator)" : L"");
    cs.style &= ~FWS_ADDTOTITLE;
    cs.lpszName = title.c_str();

    if (!CFrameWndEx::PreCreateWindow(cs))
    {
        return FALSE;
    }

    return TRUE;
}

void CMainFrame::MinimizeExtensionView()
{
    m_SubSplitter.SetSplitterPos(1.0);
}

void CMainFrame::RestoreExtensionView()
{
    if (GetExtensionView()->IsShowTypes())
    {
        m_SubSplitter.RestoreSplitterPos(0.72);
        GetExtensionView()->RedrawWindow();
    }
}

void CMainFrame::MinimizeTreeMapView()
{
    m_Splitter.SetSplitterPos(1.0);
}

void CMainFrame::RestoreTreeMapView()
{
    if (GetTreeMapView()->IsShowTreeMap())
    {
        m_Splitter.RestoreSplitterPos(0.5);
        GetTreeMapView()->DrawEmptyView();
        GetTreeMapView()->RedrawWindow();
    }
}

LRESULT CMainFrame::OnEnterSizeMove(WPARAM, LPARAM)
{
    GetTreeMapView()->SuspendRecalculationDrawing(true);
    return 0;
}

LRESULT CMainFrame::OnExitSizeMove(WPARAM, LPARAM)
{
    GetTreeMapView()->SuspendRecalculationDrawing(false);
    return 0;
}

void CMainFrame::OnTimer(const UINT_PTR nIDEvent)
{
    // Calculate UI updates that do not need to processed frequently
    static unsigned int updateCounter = 0;
    const bool doInfrequentUpdate = updateCounter++ % 15 == 0;
    if (doInfrequentUpdate && !IsIconic())
    {
        // Update memory usage
        UpdatePaneText();
    }

    // UI updates that do need to processed frequently
    if (!CDirStatDoc::GetDocument()->IsRootDone() && !IsScanSuspended())
    {
        // Update the visual progress on the bottom of the screen
        UpdateProgress();

        // By sorting items, items will be redrawn which will
        // also force pacman to update with recent position
        CFileTreeControl::Get()->SortItems();

        // Conditionally sort duplicates
        if (COptions::ScanForDuplicates && doInfrequentUpdate && GetFileTabbedView()->IsFileDupeViewTabActive())
        {
            CFileDupeControl::Get()->SortItems();
        }

        // Conditionally sort duplicates
        if (doInfrequentUpdate && GetFileTabbedView()->IsFileTopViewTabActive())
        {
            CFileTopControl::Get()->SortItems();
        }
    }

    CFrameWndEx::OnTimer(nIDEvent);
}

LRESULT CMainFrame::OnCallbackRequest(WPARAM, const LPARAM lParam)
{
    const auto & callback = *static_cast<std::function<void()>*>(reinterpret_cast<LPVOID>(lParam));
    callback();
    return 0;
}

void CMainFrame::CopyToClipboard(const std::wstring & psz)
{
    COpenClipboard clipboard(this);
    const SIZE_T cchBufLen = psz.size() + 1;

    SmartPointer<HGLOBAL> h(GlobalFree, GlobalAlloc(GMEM_MOVEABLE, cchBufLen * sizeof(WCHAR)));
    if (h == nullptr)
    {
        DisplayError(TranslateError());
        return;
    }

    // Allocate memory - scoped to forced release after copy
    {
        SmartPointer<LPVOID> lp(GlobalUnlock, GlobalLock(h));
        if (lp == nullptr)
        {
            DisplayError(TranslateError());
            return;
        }
        wcscpy_s(static_cast<LPWSTR>(*lp), cchBufLen, psz.c_str());
    }
    
    if (SetClipboardData(CF_UNICODETEXT, h) == nullptr)
    {
        DisplayError(TranslateError());
    }

    // System now owns pointer so do not allow cleanup
    h.Release();
}

void CMainFrame::OnInitMenuPopup(CMenu* pPopupMenu, const UINT nIndex, const BOOL bSysMenu)
{
    CFrameWndEx::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);

    // update cleanup menu if this is the cleanup submenu
    if (pPopupMenu->GetMenuState(ID_CLEANUP_EMPTY_BIN, MF_BYCOMMAND) != static_cast<UINT>(-1))
    {
        UpdateCleanupMenu(pPopupMenu);
    }
}

void CMainFrame::UpdateCleanupMenu(CMenu* menu) const
{
    ULONGLONG items;
    ULONGLONG bytes;
    QueryRecycleBin(items, bytes);

    std::wstring info;
    if (items == 1)
    {
        info = Localization::Format(IDS_ONEITEMs, FormatBytes(bytes));
    }
    else
    {
        info = Localization::Format(IDS_sITEMSs, FormatCount(items), FormatBytes(bytes));
    }

    const std::wstring s = Localization::Lookup(IDS_EMPTY_RECYCLEBIN) + info;
    const UINT state = menu->GetMenuState(ID_CLEANUP_EMPTY_BIN, MF_BYCOMMAND);
    VERIFY(menu->ModifyMenu(ID_CLEANUP_EMPTY_BIN, MF_BYCOMMAND | MF_STRING, ID_CLEANUP_EMPTY_BIN, s.c_str()));
    menu->EnableMenuItem(ID_CLEANUP_EMPTY_BIN, state);

    UpdateDynamicMenuItems(menu);
}

void CMainFrame::QueryRecycleBin(ULONGLONG& items, ULONGLONG& bytes)
{
    items = 0;
    bytes = 0;

    const DWORD drives = GetLogicalDrives();
    DWORD mask = 0x00000001;
    for (std::size_t i = 0; i < wds::strAlpha.size(); i++, mask <<= 1)
    {
        if ((drives & mask) == 0)
        {
            continue;
        }

        std::wstring s = std::wstring(1, wds::strAlpha.at(i)) + L":\
";
        const UINT type = ::GetDriveType(s.c_str());
        if (type == DRIVE_UNKNOWN ||
            type == DRIVE_NO_ROOT_DIR ||
            type == DRIVE_REMOTE)
        {
            continue;
        }

        SHQUERYRBINFO qbi;
        ZeroMemory(&qbi, sizeof(qbi));
        qbi.cbSize = sizeof(qbi);

        if (FAILED(::SHQueryRecycleBin(s.c_str(), &qbi)))
        {
            continue;
        }

        items += qbi.i64NumItems;
        bytes += qbi.i64Size;
    }
}

std::vector<CItem*> CMainFrame::GetAllSelectedInFocus() const
{
    if (GetLogicalFocus() == LF_DUPELIST) return CFileDupeControl::Get()->GetAllSelected<CItem>();
    if (GetLogicalFocus() == LF_TOPLIST) return CFileTopControl::Get()->GetAllSelected<CItem>();
    if (GetLogicalFocus() == LF_SEARCHLIST) return CFileSearchControl::Get()->GetAllSelected<CItem>();
    return CFileTreeControl::Get()->GetAllSelected<CItem>();
}

std::pair<CMenu*,int> CMainFrame::LocateNamedMenu(const CMenu* menu, const std::wstring & subMenuText) const
{
    // locate submenu
    CMenu* subMenu = nullptr;
    int subMenuPos = -1;
    for (int i = 0; i < menu->GetMenuItemCount(); i++)
    {
        CStringW menuString;
        if (menu->GetMenuStringW(i, menuString, MF_BYPOSITION) > 0 &&
            _wcsicmp(menuString, subMenuText.c_str()) == 0)
        {
            subMenu = menu->GetSubMenu(i);
            subMenuPos = i;
            break;
        }
    }

    // cleanup old items
    if (subMenu != nullptr) while (subMenu->GetMenuItemCount() > 0)
        subMenu->DeleteMenu(0, MF_BYPOSITION);
    return { subMenu, subMenuPos };
}

void CMainFrame::UpdateDynamicMenuItems(CMenu* menu) const
{
    const auto& items = GetAllSelectedInFocus();

    // get list of paths from items
    std::vector<std::wstring> paths;
    for (auto& item : items) paths.push_back(item->GetPath());

    // locate submenu and merge explorer items
    auto [explorerMenu, explorerMenuPos] = LocateNamedMenu(menu, Localization::Lookup(IDS_MENU_EXPLORER_MENU));
    if (explorerMenu != nullptr && !paths.empty())
    {
        CComPtr contextMenu = GetContextMenu(Get()->GetSafeHwnd(), paths);
        if (contextMenu != nullptr) contextMenu->QueryContextMenu(explorerMenu->GetSafeHmenu(), 0,
            CONTENT_MENU_MINCMD, CONTENT_MENU_MAXCMD, CMF_NORMAL);

        // conditionally disable menu if empty
        if (explorerMenuPos >= 0) menu->EnableMenuItem(explorerMenuPos, MF_BYPOSITION |
            (explorerMenu->GetMenuItemCount() > 0 ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)));
    }

    auto[customMenu, customMenuPos] = LocateNamedMenu(menu, Localization::Lookup(IDS_USER_DEFINED_CLEANUP));
    for (size_t iCurrent = 0; customMenu != nullptr && iCurrent < COptions::UserDefinedCleanups.size(); iCurrent++)
    {
        auto& udc = COptions::UserDefinedCleanups[iCurrent];
        if (!udc.Enabled) continue;

        std::wstring string = std::vformat(Localization::Lookup(IDS_UDCsCTRLd),
            std::make_wformat_args(udc.Title.Obj(), iCurrent));

        bool udcValid = GetLogicalFocus() == LF_FILETREE && !items.empty();
        if (udcValid) for (const auto& item : items)
        {
            udcValid &= CDirStatDoc::GetDocument()->UserDefinedCleanupWorksForItem(&udc, item);
        }

        const UINT flags = udcValid ? MF_ENABLED : (MF_DISABLED | MF_GRAYED);
        customMenu->AppendMenu(flags | MF_STRING, ID_USERDEFINEDCLEANUP0 + iCurrent, string.c_str());
    }

    // conditionally disable menu if empty
    if (customMenu && customMenuPos >= 0) menu->EnableMenuItem(customMenuPos, MF_BYPOSITION |
        (customMenu->GetMenuItemCount() > 0 ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)));
}

void CMainFrame::SetLogicalFocus(const LOGICAL_FOCUS lf)
{
    if (lf != m_LogicalFocus)
    {
        m_LogicalFocus = lf;
        UpdatePaneText();

        CDirStatDoc::GetDocument()->UpdateAllViews(nullptr, HINT_SELECTIONSTYLECHANGED);
    }
}

LOGICAL_FOCUS CMainFrame::GetLogicalFocus() const
{
    return m_LogicalFocus;
}

void CMainFrame::MoveFocus(const LOGICAL_FOCUS logicalFocus)
{
    switch (logicalFocus)
    {
        case LF_EXTLIST: GetExtensionView()->SetFocus(); break;
        case LF_DUPELIST: GetFileDupeView()->SetFocus(); break;
        case LF_TOPLIST: GetFileTopView()->SetFocus(); break;
        case LF_SEARCHLIST: GetFileSearchView()->SetFocus(); break;
        case LF_FILETREE: GetFileTreeView()->SetFocus(); break;
        case LF_NONE:
        {
            SetLogicalFocus(LF_NONE);
            m_WndDeadFocus.SetFocus();
        }
    }
}

void CMainFrame::UpdatePaneText()
{
    const auto focus = GetLogicalFocus();
    std::wstring fileSelectionText = Localization::Lookup(IDS_IDLEMESSAGE);
    ULONGLONG size = MAXULONG64;

    // Allow override test
    if (const std::wstring & hover = m_TreeMapView->GetTreeMapHoverPath(); !hover.empty())
    {
        fileSelectionText = hover;
    }

    // Only get the data the document is not actively updating
    else if (CDirStatDoc::GetDocument()->IsRootDone())
    {
        if (focus == LF_EXTLIST)
        {
            fileSelectionText = wds::chrStar + CDirStatDoc::GetDocument()->GetHighlightExtension();
        }
        else if (focus == LF_FILETREE || focus == LF_DUPELIST || focus == LF_TOPLIST || focus == LF_SEARCHLIST)
        {
            const auto& items = GetAllSelectedInFocus();
            if (items.size() == 1) fileSelectionText = items.front()->GetPath();
            for (size = 0; const auto& item : items)
            {
                size += item->GetSizePhysical();
            }
        }
    }

    // Update message selection area
    SetStatusPaneText(ID_STATUSPANE_IDLE_INDEX, fileSelectionText);

    // Update disk usage area
    SetStatusPaneText(
        ID_STATUSPANE_DISK_INDEX,
        (size != MAXULONG64) ? (L"      ∑  " + FormatBytes(size)) : L"",
        100
    );

    // Update memory usage
    SetStatusPaneText(ID_STATUSPANE_MEM_INDEX, CDirStatApp::GetCurrentProcessMemoryInfo(), 100);
}

void CMainFrame::OnUpdateEnableControl(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(true);
}

void CMainFrame::OnSize(const UINT nType, const int cx, const int cy)
{
    CFrameWndEx::OnSize(nType, cx, cy);

    if (!IsWindow(m_WndStatusBar.m_hWnd))
    {
        return;
    }

    CRect rc;
    m_WndStatusBar.GetItemRect(ID_STATUSPANE_IDLE_INDEX, rc);

    if (m_Progress.m_hWnd != nullptr)
    {
        m_Progress.MoveWindow(rc);
    }
    else if (m_Pacman.m_hWnd != nullptr)
    {
        m_Pacman.MoveWindow(rc);
    }
}

void CMainFrame::OnUpdateViewShowTreeMap(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(GetTreeMapView()->IsShowTreeMap());
}

void CMainFrame::OnUpdateTreeMapUseLogical(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(COptions::TreeMapUseLogical);
}

void CMainFrame::OnViewShowTreeMap()
{
    GetTreeMapView()->ShowTreeMap(!GetTreeMapView()->IsShowTreeMap());
    if (GetTreeMapView()->IsShowTreeMap())
    {
        RestoreTreeMapView();
    }
    else
    {
        MinimizeTreeMapView();
    }
}

void CMainFrame::OnViewTreeMapUseLogical()
{
    COptions::TreeMapUseLogical = !COptions::TreeMapUseLogical;
    if (GetTreeMapView()->IsShowTreeMap())
    {
        CDirStatDoc::GetDocument()->RefreshItem(CDirStatDoc::GetDocument()->GetRootItem());
    }
}

void CMainFrame::OnUpdateViewShowFileTypes(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(GetExtensionView()->IsShowTypes());
}

void CMainFrame::OnViewShowFileTypes()
{
    GetExtensionView()->ShowTypes(!GetExtensionView()->IsShowTypes());
    if (GetExtensionView()->IsShowTypes())
    {
        RestoreExtensionView();
    }
    else
    {
        MinimizeExtensionView();
    }
}

void CMainFrame::OnConfigure()
{
    COptionsPropertySheet sheet;

    CPageGeneral general;
    CPageFiltering filtering;
    CPageFileTree treelist;
    CPageTreeMap treemap;
    CPageCleanups cleanups;
    CPageAdvanced advanced;

    sheet.AddPage(&general);
    sheet.AddPage(&filtering);
    sheet.AddPage(&treelist);
    sheet.AddPage(&treemap);
    sheet.AddPage(&cleanups);
    sheet.AddPage(&advanced);

    sheet.DoModal();

    // Save settings in case the application exits abnormally
    PersistedSetting::WritePersistedProperties();
    
    if (sheet.m_RestartApplication)
    {
        CDirStatApp::Get()->RestartApplication();
    }
}

void CMainFrame::OnSysColorChange()
{
    CFrameWndEx::OnSysColorChange();
    GetFileTreeView()->SysColorChanged();
    GetExtensionView()->SysColorChanged();

    // Redraw splitter windows
    m_Splitter.Invalidate();
    m_SubSplitter.Invalidate();

    // Redraw menus for dark mode
    DarkMode::SetAppDarkMode();
    DrawMenuBar();
}

LRESULT CMainFrame::OnUahDrawMenu(WPARAM wParam, LPARAM lParam)
{
    return DarkMode::HandleMenuMessage(GetCurrentMessage()->message, wParam, lParam, *this);
}

void CMainFrame::OnNcPaint()
{
    // Update the bottom of the menu bar that is not properly painted
    CFrameWndEx::OnNcPaint();
    DarkMode::DrawMenuClientArea(*this);
}

BOOL CMainFrame::OnNcActivate(BOOL bActive)
{
    // Update the bottom of the menu bar that is not properly painted
    const auto ret = CFrameWndEx::OnNcActivate(bActive);
    DarkMode::DrawMenuClientArea(*this);
    return ret;
}

BOOL CMainFrame::LoadFrame(const UINT nIDResource, const DWORD dwDefaultStyle, CWnd* pParentWnd, CCreateContext* pContext)
{
    if (!CFrameWndEx::LoadFrame(nIDResource, dwDefaultStyle, pParentWnd, pContext))
    {
        return FALSE;
    }

    Localization::UpdateMenu(*GetMenu());
    Localization::UpdateDialogs(*this);
    SetTitle(Localization::Lookup(IDS_APP_TITLE).c_str());

    return TRUE;
}
