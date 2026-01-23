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

#include "pch.h"
#include "TreeMapView.h"
#include "FileTabbedView.h"
#include "FileTreeView.h"
#include "DrawTextCache.h"
#include "ExtensionView.h"
#include "Property.h"
#include "PageAdvanced.h"
#include "PageFiltering.h"
#include "PageCleanups.h"
#include "PageFileTree.h"
#include "PageTreeMap.h"
#include "PageGeneral.h"
#include "PagePrompts.h"
#include "FileTopControl.h"

namespace
{
    // Clipboard-Opener
    class COpenClipboard final
    {
    public:
        COpenClipboard(CWnd* owner)
        {
            m_open = owner->OpenClipboard();
            if (!m_open || !EmptyClipboard())
            {
                DisplayError(TranslateError());
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
        BOOL m_open = FALSE;
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
    m_restartRequest = changed;
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
        if (m_restartRequest && (IDOK == cmd || !m_alreadyAsked))
        {
            const int r = WdsMessageBox(*this, Localization::Lookup(IDS_RESTART_REQUEST),
                Localization::LookupNeutral(AFX_IDS_APP_TITLE), MB_YESNOCANCEL);
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

    return CMFCPropertySheet::OnCommand(wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////

CWdsSplitterWnd::CWdsSplitterWnd(double* splitterPos) :
    m_userSplitterPos(splitterPos)
{
    m_wasTrackedByUser = (*splitterPos > 0 && *splitterPos < 1);
}

BEGIN_MESSAGE_MAP(CWdsSplitterWnd, CSplitterWndEx)
    ON_WM_SIZE()
END_MESSAGE_MAP()

void CWdsSplitterWnd::StopTracking(const BOOL bAccept)
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

void CWdsSplitterWnd::SetSplitterPos(const double pos)
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

void CWdsSplitterWnd::RestoreSplitterPos(const double posIfVirgin)
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

void CWdsSplitterWnd::OnSize(const UINT nType, const int cx, const int cy)
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
    CSplitterWndEx::OnSize(nType, cx, cy);
}

/////////////////////////////////////////////////////////////////////////////

void CPacmanControl::Drive()
{
    if (IsWindow(m_hWnd))
    {
        m_pacman.UpdatePosition();
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

    m_pacman.Reset();
    m_pacman.Start();
    return 0;
}

BOOL CPacmanControl::OnEraseBkgnd(CDC* pDC)
{
    UNREFERENCED_PARAMETER(pDC);
    return TRUE;
}

void CPacmanControl::OnPaint()
{
    CPaintDC dc(this);
    CMemDC memDC(dc, this);
    CDC* pDC = &memDC.GetDC();

    CRect rect;
    GetClientRect(&rect);

    // Draw the animation
    m_pacman.Draw(pDC, rect);

    // Draw the borders
    CMFCVisualManager::GetInstance()->OnDrawStatusBarPaneBorder(
        pDC, &CMainFrame::Get()->m_wndStatusBar, rect, 0, CMainFrame::Get()->GetStyle());
}

/////////////////////////////////////////////////////////////////////////////

void CDeadFocusWnd::Create(CWnd* parent)
{
    const CRect rc(0, 0, 0, 0);
    CWnd::Create(AfxRegisterWndClass(0, nullptr, nullptr, nullptr), L"_deadfocus", WS_CHILD, rc, parent, 0);
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
constexpr auto ID_STATUSPANE_SIZE_INDEX = 1;
constexpr auto ID_STATUSPANE_RAM_INDEX = 2;

constexpr UINT indicators[]
{
    ID_INDICATOR_IDLE,
    ID_INDICATOR_SIZE,
    ID_INDICATOR_RAM
};

CMainFrame* CMainFrame::s_Singleton;

CMainFrame::CMainFrame() :
        m_subSplitter(COptions::SubSplitterPos.Ptr())
      , m_splitter(COptions::MainSplitterPos.Ptr())
{
    s_Singleton = this;
}

CMainFrame::~CMainFrame()
{
    s_Singleton = nullptr;
}

LRESULT CMainFrame::OnTaskButtonCreated(WPARAM, LPARAM)
{
    if (!m_taskbarList)
    {
        const HRESULT hr = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_taskbarList));
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

    m_progressRange = range;
    m_progressPos = 0;
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

void CMainFrame::SetProgressComplete()
{
    // Disable any potential suspend state
    SuspendState(false);

    if (m_taskbarList)
    {
        m_taskbarList->SetProgressState(*this, m_taskbarButtonState = TBPF_NOPROGRESS);
    }

    DestroyProgress();
    CDirStatDoc::Get()->SetTitlePrefix(wds::strEmpty);
    CFileTreeControl::Get()->SortItems();
    CFileDupeControl::Get()->SortItems();
    CFileTopControl::Get()->SortItems();
}

bool CMainFrame::IsScanSuspended() const
{
    return m_scanSuspend;
}

void CMainFrame::SuspendState(const bool suspend)
{
    m_scanSuspend = suspend;
    if (m_taskbarList)
    {
        if (m_taskbarButtonState == TBPF_PAUSED)
        {
            m_taskbarList->SetProgressState(*this, m_taskbarButtonState = m_taskbarButtonPreviousState);
        }
        else
        {
            m_taskbarButtonPreviousState = m_taskbarButtonState;
            m_taskbarList->SetProgressState(*this, m_taskbarButtonState |= TBPF_PAUSED);
        }
    }
    CPacman::SetGlobalSuspendState(suspend);
    UpdateProgress();
}

void CMainFrame::UpdateProgress()
{
    // Update working item tracker if changed
    const auto currentRoot = CDirStatDoc::Get()->GetRootItem();
    if (currentRoot != m_workingItem &&
        currentRoot != nullptr && !currentRoot->IsDone())
    {
        m_workingItem = currentRoot;
        CreateProgress(m_workingItem->GetProgressRange());
    }

    // Exit early if we not ready for visual updates
    if (!m_progressVisible || m_workingItem == nullptr || currentRoot == nullptr) return;

    // Update pacman graphic (does nothing if hidden)
    m_progressPos = m_workingItem->GetProgressPos();
    m_pacman.Drive();

    std::wstring titlePrefix;
    std::wstring suspended;

    // Display the suspend text in the bar if suspended
    if (IsScanSuspended())
    {
        static const std::wstring suspendString = Localization::Lookup(IDS_SUSPENDED);
        suspended = suspendString;
    }

    if (m_progressRange > 0 && m_progress.m_hWnd != nullptr)
    {
        // Limit progress at 100% as hard-linked files will count twice
        const int pos = min(static_cast<int>((m_progressPos * 100ull) / m_progressRange), 100);
        m_progress.SetPos(pos);

        titlePrefix = std::to_wstring(pos) + L"% " + suspended;
        if (m_taskbarList && m_taskbarButtonState != TBPF_PAUSED)
        {
            if (pos == 100)
            {
                m_taskbarList->SetProgressState(*this, m_taskbarButtonState = TBPF_INDETERMINATE); // often happens before we're finished
            }
            else
            {
                m_taskbarList->SetProgressState(*this, m_taskbarButtonState = TBPF_NORMAL); // often happens before we're finished
                m_taskbarList->SetProgressValue(*this, m_progressPos, m_progressRange);
            }
        }
    }
    else
    {
        static const std::wstring scanningString = Localization::Lookup(IDS_SCANNING);
        titlePrefix = scanningString + L" " + suspended;
    }

    TrimString(titlePrefix);
    CDirStatDoc::Get()->SetTitlePrefix(titlePrefix);
}

void CMainFrame::CreateStatusProgress()
{
    if (m_progress.m_hWnd == nullptr)
    {
        CRect rc;
        m_wndStatusBar.GetItemRect(ID_STATUSPANE_IDLE_INDEX, rc);
        m_progress.Create(WS_CHILD | WS_VISIBLE, rc, &m_wndStatusBar, ID_WDS_CONTROL);
        m_progress.ModifyStyle(WS_BORDER, 0);

        if (DarkMode::IsDarkModeActive())
        {
            // Disable theming for progress bar to avoid light background in dark mode
            SetWindowTheme(m_progress.GetSafeHwnd(), L"", L"");
            m_progress.SetBkColor(DarkMode::WdsSysColor(COLOR_WINDOWFRAME));
            m_progress.ModifyStyleEx(WS_EX_STATICEDGE, 0);
        }
    }
    if (m_taskbarList)
    {
        m_taskbarList->SetProgressState(*this, m_taskbarButtonState = TBPF_INDETERMINATE);
    }
}

void CMainFrame::CreatePacmanProgress()
{
    if (m_pacman.m_hWnd == nullptr)
    {
        // Get rectangle and remove top/bottom border dimension
        CRect rc;
        m_wndStatusBar.GetItemRect(0, rc);
        m_pacman.Create(nullptr, nullptr, WS_CHILD | WS_VISIBLE, rc, &m_wndStatusBar, ID_WDS_CONTROL);
        m_pacman.Start();
    }
}

void CMainFrame::DestroyProgress()
{
    if (IsWindow(m_progress.m_hWnd))
    {
        m_progress.DestroyWindow();
        m_progress.m_hWnd = nullptr;
    }
    else if (IsWindow(m_pacman.m_hWnd))
    {
        m_pacman.Stop();
        m_pacman.DestroyWindow();
        m_pacman.m_hWnd = nullptr;
    }

    m_workingItem = nullptr;
    m_progressVisible = false;
}

void CMainFrame::SetStatusPaneText(const CDC& cdc, const int pos,
    const std::wstring & text, const int minWidth)
{
    // do not process the update if text is the same
    static std::unordered_map<int, std::wstring> last;
    if (const auto it = last.find(pos); it != last.end() && it->second == text) return;
    last.insert_or_assign(pos, text);

    // set status path width and then set text
    const auto cx = cdc.GetTextExtent(text.c_str(), static_cast<int>(text.size())).cx;
    m_wndStatusBar.SetPaneWidth(pos, max(cx, DpiRest(minWidth)));
    m_wndStatusBar.SetPaneText(pos, text.c_str());
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWndEx::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }
    
    m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_TOOLTIPS | CBRS_SIZE_DYNAMIC);
    m_wndToolBar.LoadToolBar(IDR_MAINFRAME);
    m_wndToolBar.SetBorders(CRect());
    m_wndToolBar.SetPaneStyle(m_wndToolBar.GetPaneStyle() & ~CBRS_GRIPPER);

    // Setup status pane and force initial field population
    m_wndStatusBar.Create(this);
    m_wndStatusBar.SetIndicators(indicators, std::size(indicators));
    m_wndStatusBar.SetPaneStyle(ID_STATUSPANE_IDLE_INDEX, SBPS_STRETCH);
    UpdatePaneText();

    // Setup status pane for dark mode
    if (DarkMode::IsDarkModeActive())
    {
        for (const int i : std::views::iota(0, m_wndStatusBar.GetCount()))
        {
            m_wndStatusBar.SetPaneBackgroundColor(i, DarkMode::WdsSysColor(COLOR_WINDOW));
        }
    }

    // Show or hide status bar if requested
    if (!COptions::ShowStatusBar) m_wndStatusBar.ShowWindow(SW_HIDE);
    if (!COptions::ShowToolBar) m_wndToolBar.ShowWindow(SW_HIDE);

    m_wndDeadFocus.Create(this);
    DockPane(&m_wndToolBar);

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
    m_images.SetImageSize({ 16,16 }, TRUE);
    for (const int i : std::views::iota(0, m_wndToolBar.GetCount()))
    {
        // lookup the button in the editor toolbox
        const auto button = m_wndToolBar.GetButton(i);
        if (button->m_nID == 0) continue;
        ASSERT(toolbarMap.contains(button->m_nID));

        // load high quality bitmap from resource
        CBitmap bitmap;
        bitmap.Attach(static_cast<HBITMAP>(LoadImage(AfxGetResourceHandle(),
            MAKEINTRESOURCE(toolbarMap.at(button->m_nID).first),
            IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION)));
        DarkMode::LightenBitmap(&bitmap);
        const int image = m_images.AddImage(bitmap, TRUE);
        CMFCToolBar::SetUserImages(&m_images);

        // copy button into new toolbar control
        CMFCToolBarButton newButton(button->m_nID, image, nullptr, TRUE, TRUE);
        newButton.m_nStyle = button->m_nStyle | TBBS_DISABLED;
        newButton.m_strText = Localization::Lookup(toolbarMap.at(button->m_nID).second).c_str();
        m_wndToolBar.ReplaceButton(button->m_nID, newButton);
    }

    // setup look and feel with dark mode support
    CMFCVisualManager::SetDefaultManager(DarkMode::IsDarkModeActive() ?
        RUNTIME_CLASS(CDarkModeVisualManager) : RUNTIME_CLASS(CMFCVisualManagerWindows));

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

    // Mark process as shutting down
    m_shuttingDown = true;

    // Suspend the scan and wait for scan to complete
    CDirStatDoc::Get()->StopScanningEngine(CDirStatDoc::Abort);

    // Stop icon queue
    GetIconHandler()->StopAsyncShellInfoQueue();

    // It's too late, to do this in OnDestroy(). Because the toolbar, if undocked,
    // is already destroyed in OnDestroy(). So we must save the toolbar state here
    // in OnClose().
    COptions::ShowToolBar = (m_wndToolBar.GetStyle() & WS_VISIBLE) != 0;
    COptions::ShowStatusBar = (m_wndStatusBar.GetStyle() & WS_VISIBLE) != 0;

    CFrameWndEx::OnClose();
}

void CMainFrame::OnDestroy()
{
    // Mark process as shutting down
    m_shuttingDown = true;

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
    m_splitter.CreateStatic(this, 2, 1);
    m_splitter.CreateView(1, 0, RUNTIME_CLASS(CTreeMapView), CSize(100, 100), pContext);
    m_subSplitter.CreateStatic(&m_splitter, 1, 2,WS_CHILD  | WS_VISIBLE | WS_BORDER, m_splitter.IdFromRowCol(0, 0));
    m_subSplitter.CreateView(0, 0, RUNTIME_CLASS(CFileTabbedView), CSize(700, 500), pContext);
    m_subSplitter.CreateView(0, 1, RUNTIME_CLASS(CExtensionView), CSize(100, 500), pContext);

    m_treeMapView = DYNAMIC_DOWNCAST(CTreeMapView, m_splitter.GetPane(1, 0));
    m_fileTabbedView = DYNAMIC_DOWNCAST(CFileTabbedView, m_subSplitter.GetPane(0, 0));
    m_extensionView = DYNAMIC_DOWNCAST(CExtensionView, m_subSplitter.GetPane(0, 1));

    MinimizeTreeMapView();
    MinimizeExtensionView();

    GetExtensionView()->ShowTypes(COptions::ShowFileTypes);
    GetTreeMapView()->ShowTreeMap(COptions::ShowTreeMap);

    return TRUE;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    // seed initial Title bar text
    static std::wstring title = Localization::LookupNeutral(AFX_IDS_APP_TITLE) + (IsElevationActive() ? L" (Administrator)" : L"");
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
    m_subSplitter.SetSplitterPos(1.0);
}

void CMainFrame::RestoreExtensionView()
{
    if (GetExtensionView()->IsShowTypes())
    {
        m_subSplitter.RestoreSplitterPos(0.72);
        GetExtensionView()->RedrawWindow();
    }
}

void CMainFrame::MinimizeTreeMapView()
{
    m_splitter.SetSplitterPos(1.0);
}

void CMainFrame::RestoreTreeMapView()
{
    if (GetTreeMapView()->IsShowTreeMap())
    {
        m_splitter.RestoreSplitterPos(0.5);
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
    // Exit early if shutting down
    if (nIDEvent != ID_WDS_CONTROL || m_shuttingDown) return;

    // Calculate UI updates that do not need to processed frequently
    static unsigned int updateCounter = 0;
    const bool doInfrequentUpdate = updateCounter++ % 15 == 0;
    if (doInfrequentUpdate && !IsIconic())
    {
        // Update memory usage
        UpdatePaneText();
    }

    // UI updates that do need to processed frequently
    if (!CDirStatDoc::Get()->IsRootDone() && !IsScanSuspended())
    {
        // Update the visual progress at the bottom of the screen
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
    const auto & callback = *static_cast<std::function<void()>*>(std::bit_cast<LPVOID>(lParam));
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

void CMainFrame::UpdateCleanupMenu(CMenu* menu, const bool triggerAsync)
{
    // Define menu items structure with cached values
    struct { ULONGLONG* count; ULONGLONG* bytes; UINT menuId; LPCWSTR prefix; } menuItems[] = {
        { &m_recycleBinItems, &m_recycleBinBytes, ID_CLEANUP_EMPTY_BIN, IDS_EMPTY_RECYCLEBIN.data() },
        { &m_shadowCopyCount, &m_shadowCopyBytes, ID_CLEANUP_REMOVE_SHADOW, IDS_MENU_REMOVE_SHADOW.data() }
    };

    // Update menu items using cached values (initially shows zeros or last cached values)
    for (const auto& [count, bytes, menuId, prefix] : menuItems)
    {
        const std::wstring label = Localization::Lookup(prefix) + ((*count == 1) ?
            Localization::Format(IDS_ONEITEMs, FormatBytes(*bytes)) :
            Localization::Format(IDS_sITEMSs, FormatCount(*count), FormatBytes(*bytes)));

        const UINT state = menu->GetMenuState(menuId, MF_BYCOMMAND);
        menu->ModifyMenu(menuId, MF_BYCOMMAND | MF_STRING, menuId, label.c_str());
        menu->EnableMenuItem(menuId, state);
    }

    UpdateDynamicMenuItems(menu);

    // Launch a detached thread to perform the queries
    if (triggerAsync) std::thread([this]()
    {
        // Query recycle bin and shadow copies
        QueryRecycleBin(m_recycleBinItems, m_recycleBinBytes);
        QueryShadowCopies(m_shadowCopyCount, m_shadowCopyBytes);

        // Use InvokeInMessageThread to update the menu on the UI thread
        InvokeInMessageThread([this]()
        {
            // Check if the menu is still valid and visible
            const auto [menuObj, menuPos] = LocateNamedMenu(GetMenu(), Localization::Lookup(IDS_MENU_CLEANUP), false);
            if (menuObj == nullptr || menuObj->GetMenuItemCount() <= 0) return;

            // Update menu items with the newly retrieved values
            UpdateCleanupMenu(menuObj, false);
        });
    }).detach();
}

void CMainFrame::QueryRecycleBin(ULONGLONG& items, ULONGLONG& bytes)
{
    items = 0;
    bytes = 0;

    const DWORD drives = GetLogicalDrives();
    for (const auto i : std::views::iota(0, static_cast<int>(wds::strAlpha.size())))
    {
        if (((0x00000001 << i) & drives) == 0)
        {
            continue;
        }

        std::wstring s = std::wstring(1, wds::strAlpha.at(i)) + L":\\";
        if (const UINT type = ::GetDriveType(s.c_str());
            type == DRIVE_UNKNOWN ||
            type == DRIVE_NO_ROOT_DIR ||
            type == DRIVE_REMOTE)
        {
            continue;
        }

        SHQUERYRBINFO qbi{ .cbSize = sizeof(qbi) };
        if (FAILED(::SHQueryRecycleBin(s.c_str(), &qbi)))
        {
            continue;
        }

        items += qbi.i64NumItems;
        bytes += qbi.i64Size;
    }
}

std::pair<CMenu*,int> CMainFrame::LocateNamedMenu(const CMenu* menu, const std::wstring & subMenuText, const bool removeItems) const
{
    // locate submenu
    CMenu* subMenu = nullptr;
    int subMenuPos = -1;
    for (const int i : std::views::iota(0, menu->GetMenuItemCount()))
    {
        CStringW menuString;
        if (menu->GetMenuString(i, menuString, MF_BYPOSITION) > 0 &&
            _wcsicmp(menuString, subMenuText.c_str()) == 0)
        {
            subMenu = menu->GetSubMenu(i);
            subMenuPos = i;
            break;
        }
    }

    // cleanup old items
    if (removeItems && subMenu != nullptr) while (subMenu->GetMenuItemCount() > 0)
        subMenu->DeleteMenu(0, MF_BYPOSITION);
    return { subMenu, subMenuPos };
}

void CMainFrame::UpdateDynamicMenuItems(CMenu* menu) const
{
    const auto& items = CDirStatDoc::Get()->GetAllSelected();

    // get list of paths from items
    std::vector<std::wstring> paths;
    for (auto& item : items) paths.push_back(item->GetPath());

    // locate submenu and merge explorer items
    auto [explorerMenu, explorerMenuPos] = LocateNamedMenu(menu, Localization::Lookup(IDS_MENU_EXPLORER_MENU));
    if (explorerMenu != nullptr && !paths.empty())
    {
        const CComPtr contextMenu = GetContextMenu(Get()->GetSafeHwnd(), paths);
        if (contextMenu != nullptr) contextMenu->QueryContextMenu(explorerMenu->GetSafeHmenu(), 0,
            CONTENT_MENU_MINCMD, CONTENT_MENU_MAXCMD, CMF_NORMAL);

        // conditionally disable menu if empty
        if (explorerMenuPos >= 0) menu->EnableMenuItem(explorerMenuPos, MF_BYPOSITION |
            (explorerMenu->GetMenuItemCount() > 0 ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)));
    }

    // locate compress menu
    auto [compressMenu, compressMenuPos] = CMainFrame::LocateNamedMenu(menu, Localization::Lookup(IDS_MENU_COMPRESS_MENU), false);
    if (compressMenu && compressMenuPos >= 0)
    {
        // Check if any submenu items are enabled
        const int menuItemCount = compressMenu->GetMenuItemCount();
        const bool anyEnabled = std::ranges::any_of(std::views::iota(0, menuItemCount), [&](int i)
        {
            CCmdUI state;
            state.m_nIndex = i;
            state.m_nIndexMax = menuItemCount;
            state.m_nID = compressMenu->GetMenuItemID(i);
            state.m_pMenu = compressMenu;
            state.DoUpdate(const_cast<CMainFrame*>(this), FALSE);
            return (compressMenu->GetMenuState(i, MF_BYPOSITION) & (MF_DISABLED | MF_GRAYED)) == 0;
        });

        menu->EnableMenuItem(compressMenuPos, MF_BYPOSITION | (anyEnabled ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)));
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
            udcValid &= CDirStatDoc::Get()->UserDefinedCleanupWorksForItem(&udc, item);
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
    if (lf != m_logicalFocus)
    {
        m_logicalFocus = lf;
        UpdatePaneText();

        CDirStatDoc::Get()->UpdateAllViews(nullptr, HINT_SELECTIONSTYLECHANGED);
    }
}

LOGICAL_FOCUS CMainFrame::GetLogicalFocus() const
{
    return m_logicalFocus;
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
            m_wndDeadFocus.SetFocus();
        }
    }
}

void CMainFrame::UpdatePaneText()
{
    const auto focus = GetLogicalFocus();
    std::wstring fileSelectionText = Localization::Lookup(IDS_IDLEMESSAGE);
    ULONGLONG size = MAXULONG64;

    // Allow override on hover
    if (const auto [hoverPath, hoverSize] =
        m_treeMapView->GetTreeMapHoverInfo(); !hoverPath.empty())
    {
        fileSelectionText = hoverPath;
        size = hoverSize;
    }

    // Only get the data the document is not actively updating
    else if (CDirStatDoc::Get()->IsRootDone())
    {
        if (focus != LF_EXTLIST)
        {
            const auto& items = CDirStatDoc::Get()->GetAllSelected();
            if (items.size() == 1)
            {
                // If single item selected, show full path
                const auto path = items.front()->GetPath();
                if (!path.empty()) fileSelectionText = path;
            }
            for (size = 0; const auto& item : items)
            {
                size += item->GetSizePhysical();
            }

        }
        else if (fileSelectionText.empty())
        {
            fileSelectionText = wds::chrStar + CDirStatDoc::Get()->GetHighlightExtension();
        }
    }

    // Update select logical size
    const CClientDC dc(this);
    const auto sizeSummary = std::format(L"{}: {}", Localization::Lookup(IDS_COL_SIZE_LOGICAL), FormatBytes(size));
    SetStatusPaneText(dc, ID_STATUSPANE_IDLE_INDEX, fileSelectionText);
    SetStatusPaneText(dc, ID_STATUSPANE_SIZE_INDEX, (size != MAXULONG64) ? sizeSummary : L"", 175);
    SetStatusPaneText(dc, ID_STATUSPANE_RAM_INDEX, CDirStatApp::GetCurrentProcessMemoryInfo(), 175);
}

void CMainFrame::OnUpdateEnableControl(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(true);
}

void CMainFrame::OnSize(const UINT nType, const int cx, const int cy)
{
    CFrameWndEx::OnSize(nType, cx, cy);

    if (!IsWindow(m_wndStatusBar.m_hWnd))
    {
        return;
    }

    CRect rc;
    m_wndStatusBar.GetItemRect(ID_STATUSPANE_IDLE_INDEX, rc);

    if (m_progress.m_hWnd != nullptr)
    {
        m_progress.MoveWindow(rc);
    }
    else if (m_pacman.m_hWnd != nullptr)
    {
        m_pacman.MoveWindow(rc);
    }
}

/////////////////////////////////////////////////////////////////////////////

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
        CDirStatDoc::Get()->RefreshItem(CDirStatDoc::Get()->GetRootItem());
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
    CPagePrompts prompts;
    CPageAdvanced advanced;

    sheet.AddPage(&general);
    sheet.AddPage(&filtering);
    sheet.AddPage(&treelist);
    sheet.AddPage(&treemap);
    sheet.AddPage(&cleanups);
    sheet.AddPage(&prompts);
    sheet.AddPage(&advanced);

    sheet.DoModal();

    // Save settings in case the application exits abnormally
    PersistedSetting::WritePersistedProperties();
    
    if (sheet.m_restartApplication)
    {
        CDirStatApp::Get()->RestartApplication();
    }
}

void CMainFrame::OnSysColorChange()
{
    CFrameWndEx::OnSysColorChange();
    GetFileTreeView()->SysColorChanged();
    GetExtensionView()->SysColorChanged();
    DrawTextCache::Get()->ClearCache();

    // Redraw menus for dark mode
    DarkMode::SetAppDarkMode();
    RedrawWindow();
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
    SetTitle(Localization::LookupNeutral(AFX_IDS_APP_TITLE).c_str());

    return TRUE;
}
